#ifndef MINIMARL_TEST_MARL_TEST_HPP_
#define MINIMARL_TEST_MARL_TEST_HPP_

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "marl/scheduler.hpp"

auto timeLater(const std::chrono::system_clock::duration &duration) {
  return std::chrono::system_clock::now() + duration;
}

class WithoutBoundScheduler : public testing::Test {
 public:
  void SetUp() override {
    allocator_ = new marl::TrackedAllocator(marl::Allocator::Default);
  }
  void TearDown() override {
    auto stats = allocator_->stats();
    EXPECT_EQ(stats.numAllocations(), 0U);
    EXPECT_EQ(stats.bytesAllocated(), 0U);
    delete allocator_;
  }

  marl::TrackedAllocator *allocator_ = nullptr;
};

struct SchedulerParams {
  int num_worker_threads;

  friend std::ostream &operator<<(std::ostream &os,
                                  const SchedulerParams &params) {
    return os << "SchedulerParams{"
              << "numWorkerThreads: " << params.num_worker_threads << "}";
  }
};

class WithBoundScheduler : public testing::TestWithParam<SchedulerParams> {
 public:
  void SetUp() override {
    allocator_ = new marl::TrackedAllocator(marl::Allocator::Default);
    auto &params = GetParam();
    marl::Scheduler::Config cfg;
    cfg.setAllocator(allocator_)
        .setWorkerThreadCount(params.num_worker_threads)
        .setFiberStackSize(0x10000);
    auto scheduler = new marl::Scheduler(cfg);
    scheduler->bind();
  }
  void TearDown() override {
    auto scheduler = marl::Scheduler::get();
    marl::Scheduler::unbind();
    delete scheduler;

    auto stats = allocator_->stats();
    ASSERT_EQ(stats.numAllocations(), 0);
    ASSERT_EQ(stats.bytesAllocated(), 0);
    delete allocator_;
  }
  marl::TrackedAllocator *allocator_ = nullptr;
};

#define INSTANTIATE_WithBoundSchedulerTest(SubClass) \
  INSTANTIATE_TEST_SUITE_P(SchedulerParams,  \
                           SubClass,         \
                           testing::Values(SchedulerParams{0},  \
                                           SchedulerParams{1},  \
                                           SchedulerParams{2},  \
                                           SchedulerParams{4},  \
                                           SchedulerParams{8},  \
                                           SchedulerParams{64}));

#endif //MINIMARL_TEST_MARL_TEST_HPP_
