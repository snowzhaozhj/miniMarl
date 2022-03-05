#include "marl/defer.hpp"

#include "marl_test.hpp"

class DeferTest : public WithoutBoundScheduler {};

TEST_F(DeferTest, Defer) {
  bool defer_called = false;
  {
    defer(defer_called = true);
    EXPECT_FALSE(defer_called);
  }
  EXPECT_TRUE(defer_called);
}

TEST_F(DeferTest, DeferOrder) {
  int counter = 0;
  int a = 0, b = 0, c = 0;
  {
    defer(a = ++counter);
    defer(b = ++counter);
    defer(c = ++counter);
  }
  ASSERT_EQ(a, 3);
  ASSERT_EQ(b, 2);
  ASSERT_EQ(c, 1);
}

TEST_F(DeferTest, SharedFinally) {
  bool defer_called = false;
  {
    std::shared_ptr<marl::Finally> p1;
    {
      std::shared_ptr<marl::Finally> p2;
      {
        auto p3 = marl::make_shared_finally([&] {
          defer_called = true;
        });
        EXPECT_FALSE(defer_called);
        p2 = p3;
      }
      EXPECT_FALSE(defer_called);
      p1 = p2;
    }
    EXPECT_FALSE(defer_called);
  }
  EXPECT_TRUE(defer_called);
}
