#ifndef MINIMARL_INCLUDE_MARL_SCHEDULER_HPP_
#define MINIMARL_INCLUDE_MARL_SCHEDULER_HPP_

#include "containers.hpp"
#include "debug.hpp"
#include "deprecated.hpp"
#include "export.hpp"
#include "memory.hpp"
#include "mutex.hpp"
#include "task.hpp"
#include "thread.hpp"

#include <atomic>
#include <thread>

namespace marl {

class OSFiber;

/// Scheduler异步地处理任务
/// 通过bind()方法，一个Scheduler可以绑定到一个或者多个线程上
/// 一旦绑定到一个线程上，该线程就可以调用marl::schedule()来向任务队列添加任务
/// Scheduler默认以单线程模式构造
/// 可以通过setWorkerThreadCount()方法来设置工作线程数
class Scheduler {
  class Worker;

 public:
  using TimePoint = std::chrono::system_clock::time_point;
  using Predicate = std::function<bool()>;
  using ThreadInitializer = std::function<void(int worker_id)>;

  /// 保存了Scheduler相关的配置，
  struct Config {
    static constexpr size_t DefautlFiberStackSize = 1024 * 1024;

    /// 每个工作线程的配置
    struct WorkerThread {
      /// 总共的工作线程数
      int count = 0;
      /// 线程创建后调用的初始化函数，将会在处理任务前运行
      ThreadInitializer initializer;
      /// 工作线程的线程亲和性策略
      std::shared_ptr<Thread::Affinity::Policy> affinity_policy;
    };

    WorkerThread worker_thread;
    /// Scheduler和内部分配使用的内存分配器
    Allocator *allocator = Allocator::Default;
    /// 每个fiber栈的大小
    size_t fiber_stack_size = DefautlFiberStackSize;

    /// 返回一个配置，该配置为每个可用的CPU配置一个工作线程
    MARL_EXPORT
    static Config allCores();

    MARL_NO_EXPORT inline Config &setAllocator(Allocator *alloc) {
      allocator = alloc;
      return *this;
    }
    MARL_NO_EXPORT inline Config &setFiberStackSize(size_t size) {
      fiber_stack_size = size;
      return *this;
    }
    MARL_NO_EXPORT inline Config &setWorkerThreadCount(int count) {
      worker_thread.count = count;
      return *this;
    }
    MARL_NO_EXPORT inline Config &setWorkerThreadInitializer(const ThreadInitializer &initializer) {
      worker_thread.initializer = initializer;
      return *this;
    }
    MARL_NO_EXPORT inline Config &setWorkerThreadAffinityPolicy(
        const std::shared_ptr<Thread::Affinity::Policy> &policy) {
      worker_thread.affinity_policy = policy;
      return *this;
    }
  };

  MARL_EXPORT
  Scheduler(const Config &config);

  /// 阻塞，直到所有的线程都和Scheduler解绑
  MARL_EXPORT
  ~Scheduler();

  /// 返回一个和当前线程绑定的Scheduler
  MARL_EXPORT
  static Scheduler *get();

  /// 将当前线程和这个Scheduler绑定
  /// @note 在调用前，不能存在其他Scheduler和当前线程绑定
  MARL_EXPORT
  void bind();

  /// 将当前线程和当前Scheduler解绑
  /// @note 在调用前，必须存在一个Scheduler和当前线程绑定
  MARL_EXPORT
  static void unbind();

  /// 将任务放入队列中
  MARL_EXPORT
  void enqueue(Task &&task);

  /// 返回当前Scheduler使用的配置
  MARL_EXPORT
  const Config &config() const;

  /// Fiber向Scheduler暴露接口，以进行协作多任务处理，Fiber会由Scheduler自动创建\n
  /// 可以通过Fiber::current()来获取当前正在运行的fiber\n
  /// 当执行流被阻塞时，可以调用yield()的方法来挂起当前fiber，开始执行其他的正在等待的任务\n
  /// 当执行流不再阻塞之后，可以重新调用schedule()方法来在之前执行的线程上重新调度该fiber\n
  class Fiber {
   public:
    /// 返回当前正在运行的fiber，如果没有绑定的Scheduler的话，将会返回nullptr
    MARL_EXPORT
    static Fiber *current();

    /// 挂起当前fiber，直到该fiber被notify()唤醒，并且predicate返回true\n
    /// 如果fiber被notify唤醒时，predicate为false，则fiber会被重新挂起\n
    /// 该fiber被挂起时，scheduler线程可能会继续执行其他任务\n
    /// 在调用wait()前，lock必须已经上锁，lock将在fiber被挂起的前一刻被释放\n
    /// 在fiber被恢复后，lock会被重新上锁\n
    /// 在wait()返回前，lock会被上锁\n
    /// pred始终是在持有锁的条件下被调用的\n
    /// wait()只能在当前正在执行的fiber中被调用\n
    MARL_EXPORT
    void wait(marl::lock &lock, const Predicate &pred);

    /// 挂起当前fiber，直到该fiber被notify()唤醒，并且predicate返回true，或者已经超过timeout\n
    /// 如果fiber被notify唤醒时，predicate为false，则fiber会被重新挂起\n
    /// 该fiber被挂起时，scheduler线程可能会继续执行其他任务\n
    /// 在调用wait()前，lock必须已经上锁，lock将在fiber被挂起的前一刻被释放\n
    /// 在fiber被恢复后，lock会被重新上锁\n
    /// 在wait()返回前，lock会被上锁\n
    /// pred始终是在持有锁的条件下被调用的\n
    /// wait()只能在当前正在执行的fiber中被调用\n
    template<typename Clock, typename Duration>
    MARL_NO_EXPORT inline bool wait(
        marl::lock &lock,
        const std::chrono::time_point<Clock, Duration> &timeout,
        const Predicate &pred) {
      using ToDuration = typename TimePoint::duration;
      using ToClock = typename TimePoint::clock;
      auto tp = std::chrono::time_point_cast<ToDuration, ToClock>(timeout);
      return worker_->wait(lock, &tp, pred);
    }

    /// 挂起当前fiber，直到该fiber被notify()唤醒\n
    /// 该fiber被挂起时，scheduler线程可能会继续执行其他任务\n
    /// wait()只能在当前正在执行的fiber中被调用
    /// @note 不同于该函数的其他重载类型，当前版本在notify()的信号产生在fiber挂起前时，可能会导致死锁
    /// @note 因此不建议使用这个重载版本，除非你确保notify()和wait()是在同一个线程上被调用的
    /// @note 谨慎使用
    MARL_NO_EXPORT inline void wait() {
      worker_->wait(nullptr);
    }

    /// 挂起当前fiber，直到该fiber被notify()唤醒或者已经超时\n
    /// 该fiber被挂起时，scheduler线程可能会继续执行其他任务\n
    /// wait()只能在当前正在执行的fiber中被调用
    /// @note 不同于该函数的其他重载类型，当前版本在notify()的信号产生在fiber挂起前时，可能会导致死锁
    /// @note 因此不建议使用这个重载版本，除非你确保notify()和wait()是在同一个线程上被调用的
    /// @note 谨慎使用
    template<typename Clock, typename Duration>
    MARL_NO_EXPORT inline bool wait(
        const std::chrono::time_point<Clock, Duration> &timeout) {
      using ToDuration = typename TimePoint::duration;
      using ToClock = typename TimePoint::clock;
      auto tp = std::chrono::time_point_cast<ToDuration, ToClock>(timeout);
      return worker_->wait(&tp);
    }

    /// 重新调度被挂起的fiber，一般在pred条件大概率为true时被调用
    MARL_EXPORT
    void notify();

    /// 线程中唯一标识fiber的id
    uint32_t const id_;

   private:
    friend class Allocator;
    friend class Scheduler;

    enum class State {
      Idle,     ///< fiber未被使用，位于Worker::idle_fibers
      Yielded,  ///< fiber阻塞在wait()中，并且没有设置超时时间
      Waiting,  ///< fiber阻塞在wait()中，设置了超时时间，位于Worker::Work::waiting队列中
      Queued,   ///< fiber在排队等待执行，位于Worker::Work::fibers队列中
      Running,  ///< fiber正在执行中
    };

    Fiber(Allocator::unique_ptr<OSFiber> &&, uint32_t id);

    /// 将执行流切换到另一个fiber
    void switchTo(Fiber *to);

    /// 根据指定的参数创建一个新的fiber
    static Allocator::unique_ptr<Fiber> create(
        Allocator *allocator,
        uint32_t id,
        size_t stack_size,
        const std::function<void()> &func);

    /// 在当前线程上创建一个新的fiber
    static Allocator::unique_ptr<Fiber> createFromCurrentThread(
        Allocator *allocator,
        uint32_t id);

    /// 将fiber的状态以字符串的形式返回，用于debug
    static const char *toString(State state);

    Allocator::unique_ptr<OSFiber> const impl_;
    Worker *const worker_;
    State state_{State::Running}; ///< 由Worker的work.mutex保护
  };

 private:
  Scheduler(const Scheduler &) = delete;
  Scheduler(Scheduler &&) = delete;
  Scheduler &operator=(const Scheduler &) = delete;
  Scheduler &operator=(Scheduler &&) = delete;

  /// 最大的工作线程数
  static constexpr size_t MaxWorkerThreads = 256;

  /// 存储所有正在等待超时的fiber
  struct WaitingFibers {
    inline WaitingFibers(Allocator *allocator);

    /// 如果存在正在等待超时的fiber，则返回true，否则返回false
    inline operator bool() const;

    /// 返回下一个已经超时的fiber，如果没有超时的fiber的话返回nullptr
    inline Fiber *take(const TimePoint &timeout);

    /// 返回下一个fiber的超时时间
    /// @note 只有当bool()为true时，才能调用该函数
    inline TimePoint next() const;

    /// 添加一个fiber和它的超时时间
    inline void add(const TimePoint &timeout, Fiber *fiber);

    /// 从等待队列中删除fiber
    inline void erase(Fiber *fiber);

    /// 当fiber处于等待状态中时返回true，否则返回false
    inline bool contains(Fiber *fiber) const;

   private:
    struct Timeout {
      TimePoint timepoint;
      Fiber *fiber;
      inline bool operator<(const Timeout &other) const;
    };
    containers::set<Timeout, std::less<Timeout>> timeouts;
    containers::unordered_map<Fiber *, TimePoint> fibers;
  };

  // TODO: 实现一个可以循环访问元素的队列，以减少堆分配的次数
  using TaskQueue = containers::deque<Task>;
  using FiberQueue = containers::deque<Fiber *>;
  using FiberSet = containers::unordered_set<Fiber *>;

  /// Worker在一个线程上执行任务\n
  /// 当任务开始后，可能会yield到同一Worker上的其他任务
  /// 任务总是被同一个Worker恢复
  class Worker {
   public:
    enum class Mode {
      MultiThreaded,  ///< Worker将会在后台启动一个线程来处理任务
      SingleThreaded, ///< Worker将在它yield时处理任务
    };

    Worker(Scheduler *scheduler, Mode mode, uint32_t id);

    /// 开始worker的执行流
    void start() EXCLUDES(work_.mutex);

    /// 停止worker的执行流，会阻塞直到所有正在等待的任务完成
    void stop() EXCLUDES(work_.mutex);

    /// 挂起当前任务，直到pred返回true或者到达了timeout，timeout是可选的
    MARL_EXPORT
    bool wait(marl::lock &wait_lock, const TimePoint *timeout, const Predicate &pred) EXCLUDES(work_.mutex);

    /// 挂起当前任务，直到到达timeout
    MARL_EXPORT
    bool wait(const TimePoint *timeout) EXCLUDES(work_.mutex);

    /// 挂起当前fiber，直到被enqueue(Fiber*)唤醒，或者到达了timeout
    void suspend(const TimePoint *timeout) REQUIRES(work_.mutex);

    /// 将一个fiber放入队列中，恢复一个挂起的fiber
    void enqueue(Fiber *fiber) EXCLUDES(work_.mutex);

    /// 将一个新的，未开始的任务放入队列中
    void enqueue(Task &&task) EXCLUDES(work_.mutex);

    /// 尝试锁住worker，以进行任务入队操作
    /// 如果加锁成功则返回true，并且需要调用enqueueAndUnlock()来解锁
    bool tryLock() EXCLUDES(work_.mutex) TRY_ACQUIRE(true, work_.mutex);

    /// 将任务放入队列，并且解锁worker
    /// 只能在tryLock返回true之后调用
    void enqueueAndUnlock(Task &&task) REQUIRES(work_.mutex) RELEASE(work_.mutex);

    /// 一直运行直到处理完所有的任务或者shutdown为true
    void runUntilShutdown() REQUIRES(work_.mutex);

    /// 尝试从当前Worker中窃取出一个任务给另一个Worker
    bool steal(Task &out) EXCLUDES(work_.mutex);

    /// 返回绑定到当前线程的Worker
    static inline Worker *getCurrent() { return Worker::current; }

    /// 返回当前正在执行的fiber
    inline Fiber *getCurrentFiber() const { return current_fiber_; }

    /// Worker的唯一标识符
    const uint32_t id_;

   private:
    /// 一直处理任务，直到stop()被调用
    void run() REQUIRES(work_.mutex);

    /// 创建一个运行run()的fiber
    Fiber *createWorkerFiber() REQUIRES(work_.mutex);

    /// 将执行流切换到给定的fiber
    void switchToFiber(Fiber *to) REQUIRES(work_.mutex);

    /// 执行所有正在等待的任务，然后返回
    void runUntilIdle() REQUIRES(work_.mutex);

    /// 阻塞，直到有新的任务，可能是由spinForWork唤醒
    void waitForWork() REQUIRES(work_.mutex);

    /// 尝试从另一个Worker中窃取任务，并且保持线程一段时间处于活跃状态，以避免频繁的使线程休眠和唤醒
    void spinForWork();

    /// 将所有完成等待的fiber加入队列中
    void enqueueFiberTimeouts() REQUIRES(work_.mutex);

    inline void changeFiberState(Fiber *fiber,
                                 Fiber::State from,
                                 Fiber::State to) const REQUIRES(work_.mutex);

    inline void setFiberState(Fiber *fiber, Fiber::State to) const REQUIRES(work_.mutex);

    struct Work {
      inline Work(Allocator *allocator);

      std::atomic<uint64_t> num{0}; // tasks.size() + fibers.size()
      GUARDED_BY(mutex) uint64_t num_blocked_fibers{0};
      GUARDED_BY(mutex) TaskQueue tasks;
      GUARDED_BY(mutex) FiberQueue fibers;
      GUARDED_BY(mutex) WaitingFibers waiting;
      GUARDED_BY(mutex) bool notify_added{true};
      std::condition_variable added;
      marl::mutex mutex;

      template<typename F>
      inline void wait(F &&f) REQUIRES(mutex);
    };

    class FaskRnd {
     public:
      inline uint64_t operator()() {
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        return x;
      }
     private:
      uint64_t x = std::chrono::system_clock::now().time_since_epoch().count();
    };

    /// 绑定到当前线程的Worker
    static thread_local Worker *current;

    Mode const mode_;
    Scheduler *const scheduler_;
    Allocator::unique_ptr<Fiber> main_fiber_;
    Fiber *current_fiber_{nullptr};
    Thread thread_;
    Work work_;
    FiberSet idle_fibers_;
    containers::vector<Allocator::unique_ptr<Fiber>, 16>
        worker_fibers_;
    FaskRnd rng;
    bool shutdown{false};
  };

  /// 尝试窃取一个任务，如果窃取成功并且将任务放到out中，则返回true，否则返回false
  bool stealWork(Worker *thief, uint64_t from, Task &out);

  /// 调用Work::spinForWork时会调用当前函数，Scheduler会提高该worker分配任务的优先级，来避免其进入休眠
  void onBeginSpinning(int worker_id);

  /// 和当前线程绑定的scheduler
  static thread_local Scheduler *bound;

  /// 用来构造Scheduler的不可修改配置
  const Config cfg_;

  std::array<std::atomic<int>, 8> spinning_workers_;
  std::atomic<unsigned int> next_spinning_worker_index_{0x8000000};

  std::atomic<unsigned int> next_enqueue_index_{0};
  std::array<Worker *, MaxWorkerThreads> worker_threads_;

  struct SingleThreadedWorkers {
    inline SingleThreadedWorkers(Allocator *allocator);

    using WorkerByTid = containers::unordered_map<std::thread::id,
                                                  Allocator::unique_ptr<Worker>>;
    marl::mutex mutex;
    GUARDED_BY(mutex) std::condition_variable unbind;
    GUARDED_BY(mutex) WorkerByTid by_tid;
  };
  SingleThreadedWorkers single_threaded_workers_;
};

/// 将任务分配给当前绑定的scheduler以异步执行
inline void schedule(Task &&t) {
  MARL_ASSERT_HAS_BOUND_SCHEDULER("marl::schedule");
  auto scheduler = Scheduler::get();
  scheduler->enqueue(std::move(t));
}

/// 将函数f分配给当前绑定的scheduler以异步执行，并且以传递的参数调用该函数
template<typename Function, typename ...Args>
inline void schedule(Function &&f, Args &&...args) {
  MARL_ASSERT_HAS_BOUND_SCHEDULER("marl::schedule");
  auto scheduler = Scheduler::get();
  scheduler->enqueue(Task(std::bind(std::forward<Function>(f),
                                    std::forward<Args>(args)...)));
}

template<typename Function>
inline void schedule(Function &&f) {
  MARL_ASSERT_HAS_BOUND_SCHEDULER("marl::schedule");
  auto scheduler = Scheduler::get();
  scheduler->enqueue(Task(std::forward<Function>(f)));
}

} // namespace marl

#endif //MINIMARL_INCLUDE_MARL_SCHEDULER_HPP_
