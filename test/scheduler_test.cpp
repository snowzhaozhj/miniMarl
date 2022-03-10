#include "marl/scheduler.hpp"

#include "marl_test.hpp"

#include "marl/wait_group.hpp"
#include "marl/defer.hpp"

#include <memory>

using namespace std::chrono_literals;

class SchedulerTestWithoutBound : public WithoutBoundScheduler {};

class SchedulerTestWithBound : public WithBoundScheduler {};

INSTANTIATE_WithBoundSchedulerTest(SchedulerTestWithBound);

TEST_F(SchedulerTestWithoutBound, ConstructAndDestruct) {
  auto scheduler = std::make_unique<marl::Scheduler>(
      marl::Scheduler::Config());
}

TEST_F(SchedulerTestWithoutBound, BindAndUnbind) {
  auto scheduler = std::make_unique<marl::Scheduler>(
      marl::Scheduler::Config());
  scheduler->bind();
  auto got = marl::Scheduler::get();
  EXPECT_EQ(scheduler.get(), got);
  scheduler->unbind();
  got = marl::Scheduler::get();
  EXPECT_EQ(got, nullptr);
}

TEST_F(SchedulerTestWithoutBound, CheckConfig) {
  marl::Scheduler::Config cfg;
  cfg.setAllocator(allocator_)
      .setWorkerThreadCount(10)
      .setFiberStackSize(9999);
  auto scheduler = std::make_unique<marl::Scheduler>(cfg);

  EXPECT_EQ(scheduler->config().allocator, allocator_);
  EXPECT_EQ(scheduler->config().worker_thread.count, 10);
  EXPECT_EQ(scheduler->config().fiber_stack_size, cfg.fiber_stack_size);
}

TEST_F(SchedulerTestWithoutBound, TasksOnlyScheduledOnWorkerThreads) {
  marl::Scheduler::Config cfg;
  cfg.setWorkerThreadCount(8);

  auto scheduler = std::make_unique<marl::Scheduler>(cfg);
  scheduler->bind();
  defer(scheduler->unbind());

  std::mutex mutex;
  marl::containers::unordered_set<std::thread::id> threads(allocator_);
  marl::WaitGroup wg;
  for (int i = 0; i < 10000; ++i) {
    wg.add(1);
    marl::schedule([&mutex, &threads, wg] {
      defer(wg.done());
      std::lock_guard<std::mutex> ul(mutex);
      threads.emplace(std::this_thread::get_id());
    });
  }
  wg.wait();

  EXPECT_LE(threads.size(), 8);
  EXPECT_EQ(threads.count(std::this_thread::get_id()), 0);
}

TEST_F(SchedulerTestWithoutBound, NoBindDeath) {
  marl::Scheduler::Config cfg;
  auto scheduler = std::make_unique<marl::Scheduler>(cfg);
  EXPECT_DEATH(scheduler->enqueue(marl::Task([] {})),
               "Did you forget to call marl::Scheduler::bind");
}

TEST_P(SchedulerTestWithBound, DestructWithPendingTasks) {
  std::atomic<int> counter{0};
  for (int i = 0; i < 1000; ++i) {
    marl::schedule([&] { ++counter; });
  }
  auto scheduler = marl::Scheduler::get();
  marl::Scheduler::unbind();
  delete scheduler;

  ASSERT_EQ(counter.load(), 1000);

  (new marl::Scheduler(marl::Scheduler::Config()))->bind();
}

TEST_P(SchedulerTestWithBound, DestructWithPendingFibers) {
  std::atomic<int> counter{0};
  marl::WaitGroup wg(1);

  for (int i = 0; i < 1000; ++i) {
    marl::schedule([&] {
      wg.wait();
      ++counter;
    });
  }

  // 分配一个任务来解锁所有在等待的fiber
  marl::schedule([=] {
    wg.done();
  });

  auto scheduler = marl::Scheduler::get();
  marl::Scheduler::unbind();
  delete scheduler;

  EXPECT_EQ(counter.load(), 1000);

  (new marl::Scheduler(marl::Scheduler::Config()))->bind();
}

TEST_P(SchedulerTestWithBound, ScheduleWithArgs) {
  std::string got;
  marl::WaitGroup wg(1);
  marl::schedule([wg, &got](const std::string &s, int i, bool b) {
    got = "s: '" + s + "', i: " + std::to_string(i) +
        ", b: " + (b ? "true" : "false");
    wg.done();
  }, "a string", 42, true);
  wg.wait();
  EXPECT_EQ(got, "s: 'a string', i: 42, b: true");
}

TEST_P(SchedulerTestWithBound, FibersResumeOnSameThread) {
  marl::WaitGroup fence(1);
  marl::WaitGroup wg(1000);

  for (int i = 0; i < 1000; i++) {
    marl::schedule([=] {
      auto threadID = std::this_thread::get_id();
      fence.wait();
      EXPECT_EQ(threadID, std::this_thread::get_id());
      wg.done();
    });
  }

  std::this_thread::sleep_for(10ms);
  fence.done();
  wg.wait();
}

TEST_P(SchedulerTestWithBound, FibersResumeOnSameStdThread) {
  auto scheduler = marl::Scheduler::get();
  constexpr auto num_threads = sizeof(void *) > 4 ? 1000 : 100;
  marl::WaitGroup fence(1);
  marl::WaitGroup wg(num_threads);
  marl::containers::vector<std::thread, 32 > threads;
  for (int i = 0; i < num_threads; ++i) {
    threads.push_back(std::thread([=] {
      scheduler->bind();
      defer(scheduler->unbind());

      auto threadID = std::this_thread::get_id();
      fence.wait();
      EXPECT_EQ(threadID, std::this_thread::get_id());
      wg.done();
    }));
  }

  std::this_thread::sleep_for(10ms);
  fence.done();
  wg.wait();

  for (auto &thread : threads) {
    thread.join();
  }
}
