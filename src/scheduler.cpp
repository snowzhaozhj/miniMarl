#include "osfiber.hpp"  // 必须位于最前面

#include "marl/scheduler.hpp"

#include "marl/debug.hpp"
#include "marl/sanitizer.hpp"
#include "marl/thread.hpp"
#include "marl/trace.hpp"

#define ENABLE_TRACE_EVENTS 0
#define ENABLE_DEBUG_LOGGING 0

#if ENABLE_TRACE_EVENTS
#define TRACE(...) MARL_SCOPED_EVENT(__VA_ARGS__)
#else
#define TRACE(...)
#endif

#if ENABLE_DEBUG_LOGGING
#define DBG_LOG(msg, ...) \
  printf("%.3x " msg "\n", (int)threadID() & 0xfff, __VA_ARGS__)
#else
#define DBG_LOG(msg, ...)
#endif

#define ASSERT_FIBER_STATE(FIBER, STATE) \
  MARL_ASSERT(FIBER->state_ == STATE,     \
              "fiber %d was in state %s, but expected %s", \
              (int)FIBER->id_, Fiber::toString(FIBER->state_), Fiber::toString(STATE))

namespace {

#if ENABLE_DEBUG_LOGGING
// threadID() returns a uint64_t representing the currently executing thread.
// threadID() is only intended to be used for debugging purposes.
inline uint64_t threadID() {
  auto id = std::this_thread::get_id();
  return std::hash<std::thread::id>()(id);
}
#endif

inline void nop() {
  __asm__ __volatile__("nop");
}

inline marl::Scheduler::Config setConfigDefaults(
    const marl::Scheduler::Config &config_in) {
  marl::Scheduler::Config config{config_in};
  if (config.worker_thread.count > 0 && !config.worker_thread.affinity_policy) {
    config.worker_thread.affinity_policy = marl::Thread::Affinity::Policy::anyOf(
        marl::Thread::Affinity::all(config.allocator), config.allocator);
  }
  return config;
}

} // anonymous namespace

namespace marl {

//// Scheduler ////

thread_local Scheduler *Scheduler::bound = nullptr;

Scheduler *Scheduler::get() {
  return bound;
}

void Scheduler::bind() {
#if !MEMORY_SANITIZER_ENABLED
  // 动态库中的thread_local变量会在装载的时候初始化，
  // 但是如果loader没有被检测到的话，这个行为无法被MemorySanitizer观察到
  // 进而导致报错
  MARL_ASSERT(bound == nullptr, "Scheduler already bound");
#endif
  bound = this;
  {
    marl::lock lock(single_threaded_workers_.mutex);
    auto worker = cfg_.allocator->make_unique<Worker>(
        this, Worker::Mode::SingleThreaded, -1);
    worker->start();
    auto tid = std::this_thread::get_id();
    single_threaded_workers_.by_tid.emplace(tid, std::move(worker));
  }
}

void Scheduler::unbind() {
  MARL_ASSERT(bound != nullptr, "No scheduler bound");
  auto worker = Worker::getCurrent();
  worker->stop();
  {
    marl::lock lock(bound->single_threaded_workers_.mutex);
    auto tid = std::this_thread::get_id();
    auto it = bound->single_threaded_workers_.by_tid.find(tid);
    MARL_ASSERT(it != bound->single_threaded_workers_.by_tid.end(),
                "singleThreadedWorker not found");
    MARL_ASSERT(it->second.get() == worker, "worker is not bound?");
    bound->single_threaded_workers_.by_tid.erase(it);
    if (bound->single_threaded_workers_.by_tid.empty()) {
      bound->single_threaded_workers_.unbind.notify_one();
    }
  }
  bound = nullptr;
}

Scheduler::Scheduler(const Config &config)
    : cfg_(setConfigDefaults(config)),
      worker_threads_(),
      single_threaded_workers_(config.allocator) {
  for (auto &spinning_worker : spinning_workers_) {
    spinning_worker = -1;
  }
  for (int i = 0; i < cfg_.worker_thread.count; ++i) {
    worker_threads_[i] = cfg_.allocator->create<Worker>(this, Worker::Mode::MultiThreaded, i);
  }
  for (int i = 0; i < cfg_.worker_thread.count; ++i) {
    worker_threads_[i]->start();
  }
}

Scheduler::~Scheduler() {
  {
    // 等待所有single threaded workers被解绑
    marl::lock lock(single_threaded_workers_.mutex);
    lock.wait(single_threaded_workers_.unbind,
              [this]() REQUIRES(single_threaded_workers_.mutex) {
                return single_threaded_workers_.by_tid.empty();
              });
  }
  // 释放所有的工作线程
  // 这个过程会等待所有任务完成
  for (int i = cfg_.worker_thread.count - 1; i >= 0; --i) {
    worker_threads_[i]->stop();
  }
  for (int i = cfg_.worker_thread.count - 1; i >= 0; --i) {
    cfg_.allocator->destroy(worker_threads_[i]);
  }
}

void Scheduler::enqueue(Task &&task) {
  if (task.is(Task::Flags::SameThread)) {
    Worker::getCurrent()->enqueue(std::move(task));
    return;
  }
  if (cfg_.worker_thread.count > 0) {
    while (true) {
      // 优先分配给正在spinning的工作线程
      auto i = --next_spinning_worker_index_ % spinning_workers_.size();
      auto idx = spinning_workers_[i].exchange(-1);
      if (idx < 0) {
        // 如果没有spinning的工作线程，则在工作线程中循环选取
        idx = next_enqueue_index_++ % cfg_.worker_thread.count;
      }
      auto worker = worker_threads_[idx];
      if (worker->tryLock()) {
        worker->enqueueAndUnlock(std::move(task));
        return;
      }
    }
  } else {  // cfg_.worker_thread.count <= 0
    if (auto worker = Worker::getCurrent()) {
      worker->enqueue(std::move(task));
    } else {
      MARL_FATAL(
          "singleThreadedWorker not found. Did you forget to call "
          "marl::Scheduler::bind()?");
    }
  }
}

const Scheduler::Config &Scheduler::config() const {
  return cfg_;
}

bool Scheduler::stealWork(Worker *thief, uint64_t from, Task &out) {
  if (cfg_.worker_thread.count > 0) {
    auto thread = worker_threads_[from % cfg_.worker_thread.count];
    if (thread != thief) {
      if (thread->steal(out)) {
        return true;
      }
    }
  }
  return false;
}

void Scheduler::onBeginSpinning(int worker_id) {
  auto idx = next_spinning_worker_index_++ % spinning_workers_.size();
  spinning_workers_[idx] = worker_id;
}

//// Scheduler::Config ////

Scheduler::Config Scheduler::Config::allCores() {
  return Config().setWorkerThreadCount(Thread::numLogicalCPUs());
}

//// Schduler::Fiber ////

Scheduler::Fiber::Fiber(Allocator::unique_ptr<OSFiber> &&impl, uint32_t id)
    : id_(id), impl_(std::move(impl)), worker_(Worker::getCurrent()) {
  MARL_ASSERT(worker_ != nullptr, "No Scheduler::Worker bound");
}

CLANG_NO_SANITIZE_MEMORY
Scheduler::Fiber *Scheduler::Fiber::current() {
  auto worker = Worker::getCurrent();
  return worker != nullptr ? worker->getCurrentFiber() : nullptr;
}

void Scheduler::Fiber::notify() {
  worker_->enqueue(this);
}

void Scheduler::Fiber::wait(marl::lock &lock, const Predicate &pred) {
  MARL_ASSERT(worker_ == Worker::getCurrent(),
              "Scheduler::Fiber::wait() must only be called on the currently "
              "executing fiber");
  worker_->wait(lock, nullptr, pred);
}

void Scheduler::Fiber::switchTo(Fiber *to) {
  MARL_ASSERT(worker_ == Worker::getCurrent(),
              "Scheduler::Fiber::wait() must only be called on the currently "
              "executing fiber");
  if (to != this) {
    impl_->switchTo(to->impl_.get());
  }
}

Allocator::unique_ptr<Scheduler::Fiber> Scheduler::Fiber::create(
    Allocator *allocator,
    uint32_t id,
    size_t stack_size,
    const std::function<void()> &func) {
  return allocator->make_unique<Fiber>(OSFiber::createFiber(allocator, stack_size, func), id);
}

Allocator::unique_ptr<Scheduler::Fiber> Scheduler::Fiber::createFromCurrentThread(Allocator *allocator, uint32_t id) {
  return allocator->make_unique<Fiber>(OSFiber::createFiberFromCurrentThread(allocator), id);
}

const char *Scheduler::Fiber::toString(State state) {
  switch (state) {
    case State::Idle:
      return "Idle";
    case State::Yielded:
      return "Yielded";
    case State::Queued:
      return "Queued";
    case State::Running:
      return "Running";
    case State::Waiting:
      return "Waiting";
  }
  MARL_ASSERT(false, "bad fiber state");
  return "<unknown>";
}

//// Scheduler::WaitingFibers ////

Scheduler::WaitingFibers::WaitingFibers(Allocator *allocator)
    : timeouts(allocator), fibers(allocator) {}

Scheduler::WaitingFibers::operator bool() const {
  return !fibers.empty();
}

Scheduler::Fiber *Scheduler::WaitingFibers::take(const TimePoint &timeout) {
  if (!*this) {
    return nullptr;
  }
  auto it = timeouts.begin();
  if (timeout < it->timepoint) {
    return nullptr;
  }
  auto fiber = it->fiber;
  timeouts.erase(it);
  auto deleted = (fibers.erase(fiber) != 0);
  (void) deleted;
  MARL_ASSERT(deleted, "WaitingFibers::take() maps out of sync");
  return fiber;
}

Scheduler::TimePoint Scheduler::WaitingFibers::next() const {
  MARL_ASSERT(*this, "WaitingFibers::next() called when there' no waiting fibers");
  return timeouts.begin()->timepoint;
}

void Scheduler::WaitingFibers::add(const TimePoint &timeout, Fiber *fiber) {
  timeouts.emplace(Timeout{timeout, fiber});
  bool added = fibers.emplace(fiber, timeout).second;
  (void) added;
  MARL_ASSERT(added, "WaitingFibers::add() fiber already waiting");
}

void Scheduler::WaitingFibers::erase(Fiber *fiber) {
  auto it = fibers.find(fiber);
  if (it != fibers.end()) {
    auto timeout = it->second;
    auto erased = (timeouts.erase(Timeout{timeout, fiber}) != 0);
    (void) erased;
    MARL_ASSERT(erased, "WaitingFibers::erase() maps out of sync");
    fibers.erase(it);
  }
}

bool Scheduler::WaitingFibers::contains(Fiber *fiber) const {
  return fibers.count(fiber) != 0;
}

bool Scheduler::WaitingFibers::Timeout::operator<(const Timeout &o) const {
  if (timepoint != o.timepoint) {
    return timepoint < o.timepoint;
  }
  return fiber < o.fiber;
}

//// Scheduler::Worker ////

thread_local Scheduler::Worker *Scheduler::Worker::current = nullptr;

Scheduler::Worker::Worker(Scheduler *scheduler, Mode mode, uint32_t id)
    : id_(id),
      mode_(mode),
      scheduler_(scheduler),
      work_(scheduler->cfg_.allocator),
      idle_fibers_(scheduler->cfg_.allocator) {
}

void Scheduler::Worker::start() {
  switch (mode_) {
    case Mode::MultiThreaded: {
      auto allocator = scheduler_->cfg_.allocator;
      auto &affinity_policy = scheduler_->cfg_.worker_thread.affinity_policy;
      auto affinity = affinity_policy->get(id_, allocator);
      thread_ = Thread(std::move(affinity), [=] {
        Thread::setName("Thread<%.2d>", int(id_));
        if (const auto &init_func = scheduler_->cfg_.worker_thread.initializer) {
          init_func(id_);
        }
        Scheduler::bound = scheduler_;
        Worker::current = this;
        main_fiber_ = Fiber::createFromCurrentThread(scheduler_->cfg_.allocator, 0);
        current_fiber_ = main_fiber_.get();
        {
          marl::lock lock(work_.mutex);
          run();
        }
        main_fiber_.reset();
        Worker::current = nullptr;
      });
      break;
    } // case Mode::MultiThreaded
    case Mode::SingleThreaded: {
      Worker::current = this;
      main_fiber_ = Fiber::createFromCurrentThread(scheduler_->cfg_.allocator, 0);
      current_fiber_ = main_fiber_.get();
      break;
    } // case Mode::SingleThreaded
    default:
      MARL_ASSERT(false, "Unknown mode: %d", int(mode_));
  }
}

void Scheduler::Worker::stop() {
  switch (mode_) {
    case Mode::MultiThreaded: {
      enqueue(Task([this] { shutdown = true; }));
      thread_.join();
      break;
    }
    case Mode::SingleThreaded: {
      marl::lock lock(work_.mutex);
      shutdown = true;
      runUntilShutdown();
      Worker::current = nullptr;
      break;
    }
    default:
      MARL_ASSERT(false, "Unknown mode: %d", int(mode_));
  }
}

bool Scheduler::Worker::wait(const TimePoint *timeout) {
  DBG_LOG("%d: WAIT(%d)", (int) id, (int) currentFiber->id);
  {
    marl::lock lock(work_.mutex);
    suspend(timeout);
  }
  return timeout == nullptr || std::chrono::system_clock::now() < *timeout;
}

bool Scheduler::Worker::wait(marl::lock &wait_lock, const TimePoint *timeout, const Predicate &pred) {
  DBG_LOG("%d: WAIT(%d)", (int) id, (int) currentFiber->id);
  while (!pred()) {
    // 锁住work的mutex，以调用suspend()
    work_.mutex.lock();

    // 在持有work_.mutex的条件下释放wait锁
    // 确保在释放wait_lock和切换fiber之间不会有fiber入队（通过Fiber::notify()）
    wait_lock.unlock_no_tsa();

    // 挂起fiber
    suspend(timeout);

    // fiber已经恢复，无需再持有锁
    work_.mutex.unlock();

    if (timeout != nullptr && std::chrono::system_clock::now() >= *timeout) {
      return false;
    }
    // 进入下一轮循环
  }
  return true;
}

void Scheduler::Worker::suspend(const TimePoint *timeout) {
  if (timeout != nullptr) {
    changeFiberState(current_fiber_, Fiber::State::Running,
                     Fiber::State::Waiting);
    work_.waiting.add(*timeout, current_fiber_);
  } else {
    changeFiberState(current_fiber_, Fiber::State::Running,
                     Fiber::State::Yielded);
  }

  // 等到当前Worker有其他任务可做
  waitForWork();

  ++work_.num_blocked_fibers;

  if (!work_.fibers.empty()) {
    // 存在被解锁的fiber，进行恢复
    --work_.num;
    auto to = containers::take(work_.fibers);
    ASSERT_FIBER_STATE(to, Fiber::State::Queued);
    switchToFiber(to);
  } else if (!idle_fibers_.empty()) {
    // 存在可复用的旧fiber，进行恢复
    auto to = containers::take(idle_fibers_);
    ASSERT_FIBER_STATE(to, Fiber::State::Idle);
    switchToFiber(to);
  } else {
    // 有任务要处理，但没有fiber可恢复，则创建一个新的fiber
    switchToFiber(createWorkerFiber());
  }
  --work_.num_blocked_fibers;
  setFiberState(current_fiber_, Fiber::State::Running);
}

bool Scheduler::Worker::tryLock() {
  return work_.mutex.try_lock();
}

void Scheduler::Worker::enqueue(Fiber *fiber) {
  bool notify = false;
  {
    marl::lock lock(work_.mutex);
    DBG_LOG("%d: ENQUEUE(%d %s)", (int) id, (int) fiber->id,
            Fiber::toString(fiber->state));
    switch (fiber->state_) {
      case Fiber::State::Running:
      case Fiber::State::Queued:
        return; // 什么都不需要做
      case Fiber::State::Waiting:
        work_.waiting.erase(fiber);
        break;
      case Fiber::State::Idle:
      case Fiber::State::Yielded:
        break;
    }
    notify = work_.notify_added;
    work_.fibers.push_back(fiber);
    MARL_ASSERT(!work_.waiting.contains(fiber),
                "fiber is unexpectedly in the waiting list");
    setFiberState(fiber, Fiber::State::Queued);
  }
  if (notify) {
    work_.added.notify_one();
  }
}

void Scheduler::Worker::enqueue(Task &&task) {
  work_.mutex.lock();
  enqueueAndUnlock(std::move(task));
}

void Scheduler::Worker::enqueueAndUnlock(Task &&task) {
  auto notify = work_.notify_added;
  work_.tasks.push_back(std::move(task));
  ++work_.num;
  work_.mutex.unlock();
  if (notify) {
    work_.added.notify_one();
  }
}

bool Scheduler::Worker::steal(Task &out) {
  if (work_.num.load() == 0) {
    return false;
  }
  if (!work_.mutex.try_lock()) {
    return false;
  }
  if (work_.tasks.empty() || work_.tasks.front().is(Task::Flags::SameThread)) {
    work_.mutex.unlock();
    return false;
  }
  --work_.num;
  out = containers::take(work_.tasks);
  work_.mutex.unlock();
  return true;
}

void Scheduler::Worker::run() {
  if (mode_ == Mode::MultiThreaded) {
    MARL_NAME_THREAD("Thread<%.2d> Fiber<%.2d>", int(id), Fiber::current()->id);
    work_.wait([this]() REQUIRES(work_.mutex) {
      return work_.num > 0 || work_.waiting || shutdown;
    });
  }
  ASSERT_FIBER_STATE(current_fiber_, Fiber::State::Running);
  runUntilShutdown();
  switchToFiber(main_fiber_.get());
}

void Scheduler::Worker::runUntilShutdown() {
  while (!shutdown || work_.num > 0 || work_.num_blocked_fibers) {
    waitForWork();
    runUntilIdle();
  }
}

void Scheduler::Worker::waitForWork() {
  MARL_ASSERT(work_.num == work_.fibers.size() + work_.tasks.size(),
              "work.num out of sync");
  if (work_.num > 0) {
    return;
  }
  if (mode_ == Mode::MultiThreaded) {
    scheduler_->onBeginSpinning(id_);
    work_.mutex.unlock();
    spinForWork();
    work_.mutex.lock();
  }

  work_.wait([this]() REQUIRES(work_.mutex) {
    return work_.num > 0 || (shutdown && work_.num_blocked_fibers == 0);
  });
  if (work_.waiting) {
    enqueueFiberTimeouts();
  }
}

void Scheduler::Worker::enqueueFiberTimeouts() {
  auto now = std::chrono::system_clock::now();
  while (auto fiber = work_.waiting.take(now)) {
    changeFiberState(fiber, Fiber::State::Waiting, Fiber::State::Queued);
    work_.fibers.push_back(fiber);
    ++work_.num;
  }
}

void Scheduler::Worker::changeFiberState(Fiber *fiber, Fiber::State from, Fiber::State to) const {
  (void) from;
  DBG_LOG("%d: CHANGE_FIBER_STATE(%d %s -> %s)", (int) id, (int) fiber->id,
          Fiber::toString(from), Fiber::toString(to));
  ASSERT_FIBER_STATE(fiber, from);
  fiber->state_ = to;
}

void Scheduler::Worker::setFiberState(Fiber *fiber, Fiber::State to) const {
  DBG_LOG("%d: SET_FIBER_STATE(%d %s -> %s)", (int) id, (int) fiber->id,
          Fiber::toString(fiber->state), Fiber::toString(to));
  fiber->state_ = to;
}

void Scheduler::Worker::spinForWork() {
  TRACE("SPIN");
  Task stolen;

  constexpr auto duration = std::chrono::milliseconds(1);
  auto start = std::chrono::high_resolution_clock::now();
  while (std::chrono::high_resolution_clock::now() - start < duration) {
    for (int i = 0; i < 256; ++i) { // 256为按经验挑选的魔数
      // @formatter:off
      nop(); nop(); nop(); nop(); nop(); nop(); nop(); nop();
      nop(); nop(); nop(); nop(); nop(); nop(); nop(); nop();
      nop(); nop(); nop(); nop(); nop(); nop(); nop(); nop();
      nop(); nop(); nop(); nop(); nop(); nop(); nop(); nop();
      // @formatter:on
      if (work_.num > 0) {
        return;
      }
    } // end of for loop
    if (scheduler_->stealWork(this, rng(), stolen)) {
      marl::lock lock(work_.mutex);
      work_.tasks.emplace_back(std::move(stolen));
      ++work_.num;
      return;
    }
    std::this_thread::yield();
  } // end of while loop
}

void Scheduler::Worker::runUntilIdle() {
  ASSERT_FIBER_STATE(current_fiber_, Fiber::State::Running);
  MARL_ASSERT(work_.num == work_.fibers.size() + work_.tasks.size(),
              "work.num out of sync");
  while (!work_.fibers.empty() || !work_.tasks.empty()) {
    // 我们不能同时获取和存储多个fiber
    while (!work_.fibers.empty()) {
      --work_.num;
      auto fiber = containers::take(work_.fibers);
      MARL_ASSERT(idle_fibers_.count(fiber) == 0, "dequeued fiber is idle");
      MARL_ASSERT(fiber != current_fiber_, "dequeued fiber is currently running");
      ASSERT_FIBER_STATE(fiber, Fiber::State::Queued);

      changeFiberState(current_fiber_, Fiber::State::Running, Fiber::State::Idle);
      auto added = idle_fibers_.emplace(current_fiber_).second;
      (void) added;
      MARL_ASSERT(added, "fiber already idle");

      switchToFiber(fiber);
      changeFiberState(current_fiber_, Fiber::State::Idle, Fiber::State::Running);
    } // while (!work_.fibers.empty())

    if (!work_.tasks.empty()) {
      --work_.num;
      auto task = containers::take(work_.tasks);
      work_.mutex.unlock();

      task();

      // std::function的析构函数比较复杂，尽量在不加锁的时候析构
      task = Task();

      work_.mutex.lock();
    }
  } // while (!work_.fibers.empty() || !work_.tasks.empty())
}

Scheduler::Fiber *Scheduler::Worker::createWorkerFiber() {
  auto fiber_id = static_cast<uint32_t>(worker_fibers_.size() + 1);
  DBG_LOG("%d: CREATE(%d)", (int) id, (int) fiberId);
  auto fiber = Fiber::create(scheduler_->cfg_.allocator,
                             fiber_id,
                             scheduler_->cfg_.fiber_stack_size,
                             [&]() REQUIRES(work_.mutex) { run(); });
  auto ptr = fiber.get();
  worker_fibers_.push_back(std::move(fiber));
  return ptr;
}

void Scheduler::Worker::switchToFiber(Fiber *to) {
  DBG_LOG("%d: SWITCH(%d -> %d)", (int) id, (int) currentFiber->id, (int) to->id);
  MARL_ASSERT(to == main_fiber_.get() || idle_fibers_.count(to) == 0,
              "switching to idle fiber");
  auto from = current_fiber_;
  current_fiber_ = to;
  from->switchTo(to);
}

//// Scheduler::Worker::Work
Scheduler::Worker::Work::Work(Allocator *allocator)
    : tasks(allocator), fibers(allocator), waiting(allocator) {}

template<typename F>
void Scheduler::Worker::Work::wait(F &&f) {
  notify_added = true;
  if (waiting) {
    mutex.wait_until_locked(added, waiting.next(), f);
  } else {
    mutex.wait_locked(added, f);
  }
  notify_added = false;
}

//// Scheduler::Worker::Work ////

Scheduler::SingleThreadedWorkers::SingleThreadedWorkers(Allocator *allocator)
    : by_tid(allocator) {}

} // namespace marl
