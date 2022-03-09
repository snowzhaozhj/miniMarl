#include "marl/condition_variable.hpp"

#include "marl_test.hpp"

class ConditionVariableTestWithoutBound : public WithoutBoundScheduler {};

class ConditionVariableTestWithBound : public WithBoundScheduler {};

INSTANTIATE_WithBoundSchedulerTest(ConditionVariableTestWithBound);

TEST_F(ConditionVariableTestWithoutBound, Base) {
  bool trigger[3] = {false, false, false};
  bool signal[3] = {false, false, false};
  marl::mutex mutex;
  marl::ConditionVariable cv;

  std::thread thread([&] {
    for (int i = 0; i < 3; ++i) {
      marl::lock lock(mutex);
      cv.wait(lock, [&] {
        EXPECT_TRUE(lock.owns_lock());
        return trigger[i];
      });
      EXPECT_TRUE(lock.owns_lock());
      signal[i] = true;
      cv.notify_one();
    }
  });

  ASSERT_FALSE(signal[0]);
  ASSERT_FALSE(signal[1]);
  ASSERT_FALSE(signal[2]);

  for (int i = 0; i < 3; ++i) {
    {
      marl::lock lock(mutex);
      trigger[i] = true;
      cv.notify_one();
      cv.wait(lock, [&] {
        EXPECT_TRUE(lock.owns_lock());
        return signal[i];
      });
      EXPECT_TRUE(lock.owns_lock());
    }

    EXPECT_EQ(signal[0], 0 <= i);
    EXPECT_EQ(signal[1], 1 <= i);
    EXPECT_EQ(signal[2], 2 <= i);
  }

  thread.join();
}

TEST_P(ConditionVariableTestWithBound, Base) {
  bool trigger[3] = {false, false, false};
  bool signal[3] = {false, false, false};
  marl::mutex mutex;
  marl::ConditionVariable cv;

  std::thread thread([&] {
    for (int i = 0; i < 3; i++) {
      marl::lock lock(mutex);
      cv.wait(lock, [&] {
        EXPECT_TRUE(lock.owns_lock());
        return trigger[i];
      });
      EXPECT_TRUE(lock.owns_lock());
      signal[i] = true;
      cv.notify_one();
    }
  });

  ASSERT_FALSE(signal[0]);
  ASSERT_FALSE(signal[1]);
  ASSERT_FALSE(signal[2]);

  for (int i = 0; i < 3; i++) {
    {
      marl::lock lock(mutex);
      trigger[i] = true;
      cv.notify_one();
      cv.wait(lock, [&] {
        EXPECT_TRUE(lock.owns_lock());
        return signal[i];
      });
      EXPECT_TRUE(lock.owns_lock());
    }

    ASSERT_EQ(signal[0], 0 <= i);
    ASSERT_EQ(signal[1], 1 <= i);
    ASSERT_EQ(signal[2], 2 <= i);
  }

  thread.join();
}
