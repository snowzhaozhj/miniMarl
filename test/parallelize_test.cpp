#include "marl/parallelize.hpp"

#include "marl_test.hpp"

class ParallelizeTestWithBound : public WithBoundScheduler {};
INSTANTIATE_WithBoundSchedulerTest(ParallelizeTestWithBound);

TEST_P(ParallelizeTestWithBound, Parallelize) {
  bool a = false;
  bool b = false;
  bool c = false;
  marl::parallelize([&] { a = true; },
                    [&] { b = true; },
                    [&] { c = true; });
  ASSERT_TRUE(a);
  ASSERT_TRUE(b);
  ASSERT_TRUE(c);
}
