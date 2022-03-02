#include "marl/memory.hpp"

#include "marl_test.hpp"

class AllocatorTest : public testing::Test {
 public:
  marl::Allocator *allocator_ = marl::Allocator::Default;
};

TEST_F(AllocatorTest, AlignedAllocate) {
  for (auto use_guards : {false, true}) {
    for (auto alignment : {1, 2, 4, 8, 16, 32, 64, 128}) {
      for (auto size : {1, 2, 3, 4, 5, 7, 8, 14, 16, 17,
                        31, 34, 50, 63, 64, 65, 100, 127, 128, 129,
                        200, 255, 256, 257, 500, 511, 512, 513}) {
        marl::Allocation::Request request;
        request.alignment = alignment;
        request.size = size;
        request.use_guards = use_guards;

        auto allocation = allocator_->allocate(request);
        auto ptr = allocation.ptr;
        ASSERT_EQ(allocation.request.size, request.size);
        ASSERT_EQ(allocation.request.alignment, request.alignment);
        ASSERT_EQ(allocation.request.use_guards, request.use_guards);
        ASSERT_EQ(allocation.request.usage, request.usage);
        ASSERT_EQ(reinterpret_cast<uintptr_t>(ptr) & (alignment - 1), 0U);
        memset(ptr, 0, size); // 检查是否实际分配了内存
        allocator_->free(allocation);
      }
    }
  }
}

struct alignas(16) StructWith16ByteAlignment {
  uint8_t i;
  uint8_t padding[15];
};

struct alignas(32) StructWith32ByteAlignment {
  uint8_t i;
  uint8_t padding[31];
};

struct alignas(64) StructWith64ByteAlignment {
  uint8_t i;
  uint8_t padding[31];
};

TEST_F(AllocatorTest, Create) {
  auto s16 = allocator_->create<StructWith16ByteAlignment>();
  auto s32 = allocator_->create<StructWith32ByteAlignment>();
  auto s64 = allocator_->create<StructWith64ByteAlignment>();
  ASSERT_EQ(alignof(StructWith16ByteAlignment), 16);
  ASSERT_EQ(alignof(StructWith32ByteAlignment), 32);
  ASSERT_EQ(alignof(StructWith64ByteAlignment), 64);
  ASSERT_EQ(reinterpret_cast<uintptr_t>(s16) & 15U, 0U);
  ASSERT_EQ(reinterpret_cast<uintptr_t>(s32) & 31U, 0U);
  ASSERT_EQ(reinterpret_cast<uintptr_t>(s64) & 63U, 0U);
  allocator_->destroy(s64);
  allocator_->destroy(s32);
  allocator_->destroy(s16);
}

TEST_F(AllocatorTest, Guards) {
  marl::Allocation::Request request;
  request.alignment = 16;
  request.size = 16;
  request.use_guards = true;
  auto alloc = allocator_->allocate(request);
  auto ptr = reinterpret_cast<uint8_t *>(alloc.ptr);
  EXPECT_DEATH(ptr[-1] = 1, "");
  EXPECT_DEATH(ptr[marl::pageSize()] = 1, "");
}
