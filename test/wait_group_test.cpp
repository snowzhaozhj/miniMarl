#include "marl/wait_group.hpp"

#include "marl_test.hpp"

class WaitGroupTestWithoutBound : public WithoutBoundScheduler {};

class WaitGroupTestWithBound : public WithBoundScheduler {};

INSTANTIATE_WithBoundSchedulerTest(WaitGroupTestWithBound)

TEST_F(WaitGroupTestWithoutBound, Done) {
  marl::WaitGroup wg(2);  // 不需要在scheduler环境下运行
  wg.done();
  wg.done();
}

TEST_F(WaitGroupTestWithoutBound, DoneTooMany) {
  marl::WaitGroup wg(2);
  wg.done();
  wg.done();
  EXPECT_DEATH(wg.done(), "done\\(\\) called too many times");
}

TEST_P(WaitGroupTestWithBound, OneTask) {
  marl::WaitGroup wg(1);
  std::atomic<int> counter{0};
  marl::schedule([&counter, wg] {
    ++counter;
    wg.done();
  });
  wg.wait();
  EXPECT_EQ(counter.load(), 1);
}

TEST_P(WaitGroupTestWithBound, ManyTasks) {
  marl::WaitGroup wg(10);
  std::atomic<int> counter{0};
  for (int i = 0; i < 10; i++) {
    marl::schedule([&counter, wg] {
      counter++;
      wg.done();
    });
  }
  wg.wait();
  ASSERT_EQ(counter.load(), 10);
}
