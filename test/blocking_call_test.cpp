#include "marl/blocking_call.hpp"

#include "marl_test.hpp"

#include "marl/defer.hpp"

class BlockingCallTestWithBound : public WithBoundScheduler {};

INSTANTIATE_WithBoundSchedulerTest(BlockingCallTestWithBound);

TEST_P(BlockingCallTestWithBound, VoidReturn) {
  auto mutex = std::make_shared<std::mutex>();
  mutex->lock();

  marl::WaitGroup wg(100);
  for (int i = 0; i < 100; ++i) {
    marl::schedule([=] {
      defer(wg.done());
      marl::blocking_call([=] {
        mutex->lock();
        defer(mutex->unlock());
      });
    });
  }

  mutex->unlock();
  wg.wait();
}

TEST_P(BlockingCallTestWithBound, IntReturn) {
  auto mutex = std::make_shared<std::mutex>();
  mutex->lock();

  marl::WaitGroup wg(100);
  std::atomic<int> n{0};
  for (int i = 0; i < 100; i++) {
    marl::schedule([=, &n] {
      defer(wg.done());
      n += marl::blocking_call([=] {
        mutex->lock();
        defer(mutex->unlock());
        return i;
      });
    });
  }

  mutex->unlock();
  wg.wait();

  ASSERT_EQ(n.load(), 4950);
}

TEST_P(BlockingCallTestWithBound, ScheduleTask) {
  marl::WaitGroup wg(1);
  marl::schedule([=] {
    marl::blocking_call([=] {
      marl::schedule([=] {
        wg.done();
      });
    });
  });
  wg.wait();
}

