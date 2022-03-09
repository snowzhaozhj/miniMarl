#include "marl/condition_variable.hpp"

#include "marl_test.hpp"

using namespace std::chrono_literals;

class ConditionVariableTestWithoutBound : public WithoutBoundScheduler {};

class ConditionVariableTestWithBound : public WithBoundScheduler {};

INSTANTIATE_WithBoundSchedulerTest(ConditionVariableTestWithBound);

TEST_F(ConditionVariableTestWithoutBound, Wait) {
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

TEST_F(ConditionVariableTestWithoutBound, WaitForNoTimeout) {
  bool signal = false;
  marl::mutex mutex;
  marl::ConditionVariable cv;
  std::thread thread([&] {
    marl::lock lock(mutex);
    bool res = cv.wait_for(lock, 30ms, [&] {
      EXPECT_TRUE(lock.owns_lock());
      return signal;
    });
    EXPECT_TRUE(res);
    EXPECT_TRUE(lock.owns_lock());
  });
  signal = true;
  cv.notify_one();
  thread.join();
}

TEST_F(ConditionVariableTestWithoutBound, WaitForTimeout) {
  bool signal = false;
  marl::mutex mutex;
  marl::ConditionVariable cv;
  std::thread thread([&] {
    marl::lock lock(mutex);
    auto res = cv.wait_for(lock, 20ms, [&] {
      EXPECT_TRUE(lock.owns_lock());
      return signal;
    });
    EXPECT_FALSE(res);
    EXPECT_TRUE(lock.owns_lock());
  });
  std::this_thread::sleep_for(30ms);
  signal = true;
  cv.notify_one();
  thread.join();
}

TEST_P(ConditionVariableTestWithBound, Wait) {
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

TEST_P(ConditionVariableTestWithBound, WaitForNoTimeout) {
  bool signal = false;
  marl::mutex mutex;
  marl::ConditionVariable cv;
  std::thread thread([&] {
    marl::lock lock(mutex);
    bool res = cv.wait_for(lock, 30ms, [&] {
      EXPECT_TRUE(lock.owns_lock());
      return signal;
    });
    EXPECT_TRUE(res);
    EXPECT_TRUE(lock.owns_lock());
  });
  signal = true;
  cv.notify_one();
  thread.join();
}

TEST_P(ConditionVariableTestWithBound, WaitForTimeout) {
  bool signal = false;
  marl::mutex mutex;
  marl::ConditionVariable cv;
  std::thread thread([&] {
    marl::lock lock(mutex);
    auto res = cv.wait_for(lock, 20ms, [&] {
      EXPECT_TRUE(lock.owns_lock());
      return signal;
    });
    EXPECT_FALSE(res);
    EXPECT_TRUE(lock.owns_lock());
  });
  std::this_thread::sleep_for(30ms);
  signal = true;
  cv.notify_one();
  thread.join();
}
