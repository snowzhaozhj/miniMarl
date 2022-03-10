#include "marl/mutex.hpp"

#include "marl_test.hpp"

#include <thread>
#include <chrono>

using namespace std::chrono_literals;

class MutexTest : public testing::Test {
 public:
  marl::mutex mutex_;
};

class LockTest : public testing::Test {
 public:
  marl::mutex mutex_;
};

TEST_F(MutexTest, TryLock) {
  mutex_.lock();
  EXPECT_FALSE(mutex_.try_lock());
  mutex_.unlock();
  EXPECT_TRUE(mutex_.try_lock());
  mutex_.unlock();
}

TEST_F(MutexTest, WaitLocked) {
  std::condition_variable cv;
  int i = 5;
  std::thread t1([&] {
    mutex_.wait_locked(cv, [&] { return i == 3; });
    EXPECT_FALSE(mutex_.try_lock());
    mutex_.unlock();
  });
  std::thread t2([&] {
    std::this_thread::sleep_for(30ms);
    mutex_.lock();
    i = 3;
    mutex_.unlock();
    cv.notify_one();
  });
  t1.join();
  t2.join();
}

TEST_F(MutexTest, WaitUntilLockedNoTimeout) {
  std::condition_variable cv;
  int i = 5;
  std::thread t1([&] {
    EXPECT_TRUE(mutex_.wait_until_locked(cv, timeLater(30ms), [&] { return i == 3; }));
    EXPECT_FALSE(mutex_.try_lock());
    mutex_.unlock();
  });
  std::thread t2([&] {
    std::this_thread::sleep_for(10ms);
    mutex_.lock();
    i = 3;
    mutex_.unlock();
    cv.notify_one();
  });
  t1.join();
  t2.join();
}

TEST_F(MutexTest, WaitUntilLockedTimeout) {
  std::condition_variable cv;
  int i = 5;
  std::thread t1([&] {
    EXPECT_FALSE(mutex_.wait_until_locked(cv, timeLater(10ms), [&] { return i == 3; }));
    EXPECT_FALSE(mutex_.try_lock());
    mutex_.unlock();
  });
  std::thread t2([&] {
    std::this_thread::sleep_for(30ms);
    mutex_.lock();
    i = 3;
    mutex_.unlock();
    cv.notify_one();
  });
  t1.join();
  t2.join();
}

TEST_F(LockTest, Wait) {
  std::condition_variable cv;
  int i = 5;
  std::thread t1([&] {
    marl::lock lock(mutex_);
    EXPECT_TRUE(lock.owns_lock());
    lock.wait(cv, [&] { return i == 3; });
    EXPECT_TRUE(lock.owns_lock());
  });
  std::thread t2([&] {
    std::this_thread::sleep_for(30ms);
    marl::lock lock(mutex_);
    i = 3;
    lock.unlock_no_tsa();
    cv.notify_one();
  });
  t1.join();
  t2.join();
}

TEST_F(LockTest, WaitUntilNoTimeout) {
  std::condition_variable cv;
  int i = 5;
  std::thread t1([&] {
    marl::lock lock(mutex_);
    EXPECT_TRUE(lock.owns_lock());
    EXPECT_TRUE(lock.wait_until(cv, timeLater(30ms),[&] { return i == 3; }));
    EXPECT_TRUE(lock.owns_lock());
  });
  std::thread t2([&] {
    std::this_thread::sleep_for(10ms);
    marl::lock lock(mutex_);
    i = 3;
    lock.unlock_no_tsa();
    cv.notify_one();
  });
  t1.join();
  t2.join();
}

TEST_F(LockTest, WaitUntilTimeout) {
  std::condition_variable cv;
  int i = 5;
  std::thread t1([&] {
    marl::lock lock(mutex_);
    EXPECT_TRUE(lock.owns_lock());
    EXPECT_FALSE(lock.wait_until(cv, timeLater(10ms),[&] { return i == 3; }));
    EXPECT_TRUE(lock.owns_lock());
  });
  std::thread t2([&] {
    std::this_thread::sleep_for(30ms);
    marl::lock lock(mutex_);
    i = 3;
    lock.unlock_no_tsa();
    cv.notify_one();
  });
  t1.join();
  t2.join();
}

TEST_F(LockTest, NoTsa) {
  marl::lock lock(mutex_);
  EXPECT_TRUE(lock.owns_lock());
  lock.unlock_no_tsa();
  EXPECT_FALSE(lock.owns_lock());
  lock.lock_no_tsa();
  EXPECT_TRUE(lock.owns_lock());
}
