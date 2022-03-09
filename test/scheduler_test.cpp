#include <memory>

#include "marl/scheduler.hpp"

#include "marl_test.hpp"

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
