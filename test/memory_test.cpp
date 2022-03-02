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

struct SimpleStruct {
  std::string name;
  int value;
};

class ClassWithoutArgs {
 public:
  ClassWithoutArgs() = default;
  ClassWithoutArgs(const ClassWithoutArgs &) {}
  ClassWithoutArgs &operator=(const ClassWithoutArgs &) = default;

  [[nodiscard]] const std::string &GetName() const { return name_; }
  void SetName(const std::string &name) { name_ = name; }
 private:
  std::string name_{};
};

class ClassWithArgs {
 public:
  ClassWithArgs(std::string name, int value)
      : name_(std::move(name)), value_(value) {}
  [[nodiscard]] const std::string &GetName() const { return name_; }
  [[nodiscard]] int GetValue() const { return value_; }
  void SetName(const std::string &name) { name_ = name; }
  void SetValue(int value) { value_ = value; }
 private:
  std::string name_;
  int value_;
};

TEST_F(AllocatorTest, MakeUnique) {
  auto simple_struct = allocator_->make_unique<SimpleStruct>();
  simple_struct->name = "unique";
  simple_struct->value = 2;
  EXPECT_EQ(simple_struct->name, "unique");
  EXPECT_EQ(simple_struct->value, 2);

  auto class_without_args = allocator_->make_unique<ClassWithoutArgs>();
  class_without_args->SetName("unique");
  EXPECT_EQ(class_without_args->GetName(), "unique");

  auto class_with_args = allocator_->make_unique<ClassWithArgs>("unique", 2);
  EXPECT_EQ(class_with_args->GetName(), "unique");
  EXPECT_EQ(class_with_args->GetValue(), 2);
}

// TODO: Test Failed, Need To Fix
TEST_F(AllocatorTest, MakeUniqueN) {
  size_t array_size = 2;
  auto struct_array = allocator_->make_unique_n<SimpleStruct>(array_size);
  SimpleStruct simple_struct;
  simple_struct.name = "unique";
  simple_struct.value = 2;
  for (int i = 0; i < array_size; ++i) {
    struct_array.get()[i] = simple_struct;
  }
  for (int i = 0; i < array_size; ++i) {
    EXPECT_EQ(struct_array.get()[i].name, "unique");
    EXPECT_EQ(struct_array.get()[i].value, 2);
  }

  auto no_args_array = allocator_->make_unique_n<ClassWithoutArgs>(array_size);
  ClassWithoutArgs class_without_args;
  class_without_args.SetName("Lily");
  for (int i = 0; i < array_size; ++i) {
    no_args_array.get()[i] = class_without_args;
  }
  for (int i = 0; i < array_size; ++i) {
    EXPECT_EQ(no_args_array.get()[i].GetName(), "Lily");
  }

  auto args_array = allocator_->make_unique_n<ClassWithArgs>(array_size, "unique", 2);
  EXPECT_EQ(args_array->GetName(), "unique");
  EXPECT_EQ(args_array->GetValue(), 2);
  ClassWithArgs class_with_args{"Lily", 3};
  for (int i = 0; i < array_size; ++i) {
    args_array.get()[i] = class_with_args;
  }
  for (int i = 0; i < array_size; ++i) {
    EXPECT_EQ(args_array.get()[i].GetName(), "Lily");
    EXPECT_EQ(args_array.get()[i].GetValue(), 3);
  }
}
