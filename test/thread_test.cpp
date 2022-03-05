#include "marl/thread.hpp"

#include "marl_test.hpp"

#include <thread>
#include <chrono>

using namespace std::chrono_literals;

namespace {

marl::Thread::Core core(int index) {
  marl::Thread::Core c;
  c.pthread.index = static_cast<uint16_t>(index);
  return c;
}

} // anonymous namespace

class ThreadTest : public WithoutBoundScheduler {};

TEST_F(ThreadTest, AffinityCount) {
  auto affinity = marl::Thread::Affinity(
      {
          core(10),
          core(20),
          core(30),
          core(40),
      },
      allocator_);
  EXPECT_EQ(affinity.count(), 4);
}

TEST_F(ThreadTest, AffinityAdd) {
  auto affinity = marl::Thread::Affinity(
      {
          core(10),
          core(20),
          core(30),
          core(40),
      },
      allocator_);
  affinity
      .add(marl::Thread::Affinity(
          {
              core(25),
              core(15),
          },
          allocator_))
      .add(marl::Thread::Affinity({core(35)}, allocator_));

  EXPECT_EQ(affinity.count(), 7);
  EXPECT_EQ(affinity[0], core(10));
  EXPECT_EQ(affinity[1], core(15));
  EXPECT_EQ(affinity[2], core(20));
  EXPECT_EQ(affinity[3], core(25));
  EXPECT_EQ(affinity[4], core(30));
  EXPECT_EQ(affinity[5], core(35));
  EXPECT_EQ(affinity[6], core(40));
}

TEST_F(ThreadTest, AffinityRemove) {
  auto affinity = marl::Thread::Affinity(
      {
          core(10),
          core(20),
          core(30),
          core(40),
      },
      allocator_);

  affinity
      .remove(marl::Thread::Affinity(
          {
              core(25),
              core(20),
          },
          allocator_))
      .remove(marl::Thread::Affinity({core(40)}, allocator_));

  EXPECT_EQ(affinity.count(), 2);
  EXPECT_EQ(affinity[0], core(10));
  EXPECT_EQ(affinity[1], core(30));
}

TEST_F(ThreadTest, AffinityAllCount) {
  auto affinity = marl::Thread::Affinity::all(allocator_);
  if (marl::Thread::Affinity::supported) {
    EXPECT_NE(affinity.count(), 0);
  } else {
    EXPECT_EQ(affinity.count(), 0);
  }
}

TEST_F(ThreadTest, AffinityFromVector) {
  marl::containers::vector<marl::Thread::Core, 32> cores(allocator_);
  cores.push_back(core(10));
  cores.push_back(core(20));
  cores.push_back(core(30));
  cores.push_back(core(40));
  auto affinity = marl::Thread::Affinity(cores, allocator_);
  EXPECT_EQ(affinity.count(), cores.size());
  EXPECT_EQ(affinity[0], core(10));
  EXPECT_EQ(affinity[1], core(20));
  EXPECT_EQ(affinity[2], core(30));
  EXPECT_EQ(affinity[3], core(40));
}

TEST_F(ThreadTest, AffinityCopy) {
  auto affinity = marl::Thread::Affinity(
      {
          core(10),
          core(20),
          core(30),
          core(40),
      },
      allocator_);
  marl::Thread::Affinity affinity2(affinity, allocator_);

  EXPECT_EQ(affinity2.count(), 4);
  EXPECT_EQ(affinity2[0], core(10));
  EXPECT_EQ(affinity2[1], core(20));
  EXPECT_EQ(affinity2[2], core(30));
  EXPECT_EQ(affinity2[3], core(40));
}

TEST_F(ThreadTest, AffinityPolicyAnyOf) {
  auto all = marl::Thread::Affinity(
      {
          core(10),
          core(20),
          core(30),
          core(40),
      },
      allocator_);

  auto policy =
      marl::Thread::Affinity::Policy::anyOf(std::move(all), allocator_);
  auto affinity = policy->get(0, allocator_);
  EXPECT_EQ(affinity.count(), 4);
  EXPECT_EQ(affinity[0], core(10));
  EXPECT_EQ(affinity[1], core(20));
  EXPECT_EQ(affinity[2], core(30));
  EXPECT_EQ(affinity[3], core(40));
}

TEST_F(ThreadTest, AffinityPolicyOneOf) {
  auto all = marl::Thread::Affinity(
      {
          core(10),
          core(20),
          core(30),
          core(40),
      },
      allocator_);

  auto policy =
      marl::Thread::Affinity::Policy::oneOf(std::move(all), allocator_);
  EXPECT_EQ(policy->get(0, allocator_).count(), 1);
  EXPECT_EQ(policy->get(0, allocator_)[0].pthread.index, 10);
  EXPECT_EQ(policy->get(1, allocator_).count(), 1);
  EXPECT_EQ(policy->get(1, allocator_)[0].pthread.index, 20);
  EXPECT_EQ(policy->get(2, allocator_).count(), 1);
  EXPECT_EQ(policy->get(2, allocator_)[0].pthread.index, 30);
  EXPECT_EQ(policy->get(3, allocator_).count(), 1);
  EXPECT_EQ(policy->get(3, allocator_)[0].pthread.index, 40);
}

TEST_F(ThreadTest, Default) {
  marl::Thread thread;
  EXPECT_DEATH(thread.join(), "");
}

TEST_F(ThreadTest, Run) {
  bool thread_has_run = false;
  marl::Thread thread(marl::Thread::Affinity::all(allocator_), [&] {
    thread_has_run = true;
  });
  thread.join();
  EXPECT_TRUE(thread_has_run);
  EXPECT_DEATH(thread.join(), "");
}

TEST_F(ThreadTest, MoveConstruct) {
  bool thread_has_run = false;
  marl::Thread thread1(marl::Thread::Affinity::all(allocator_), [&] {
    std::this_thread::sleep_for(30ms);
    thread_has_run = true;
  });
  EXPECT_FALSE(thread_has_run);
  marl::Thread thread2(std::move(thread1));
  thread2.join();
  EXPECT_TRUE(thread_has_run);
  EXPECT_DEATH(thread1.join(), "");
  EXPECT_DEATH(thread2.join(), "");
}

TEST_F(ThreadTest, MoveAssign) {
  bool thread_has_run = false;
  marl::Thread thread1(marl::Thread::Affinity::all(allocator_), [&] {
    std::this_thread::sleep_for(30ms);
    thread_has_run = true;
  });
  EXPECT_FALSE(thread_has_run);
  marl::Thread thread2;
  thread2 = std::move(thread1);
  thread2.join();
  EXPECT_TRUE(thread_has_run);
  EXPECT_DEATH(thread1.join(), "");
  EXPECT_DEATH(thread2.join(), "");
}
