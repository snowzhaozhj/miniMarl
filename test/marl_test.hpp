#ifndef MINIMARL_TEST_MARL_TEST_HPP_
#define MINIMARL_TEST_MARL_TEST_HPP_

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "marl/memory.hpp"

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

#endif //MINIMARL_TEST_MARL_TEST_HPP_
