#include "marl/memory.hpp"

#include "marl_test.hpp"

#include <vector>

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

TEST_F(AllocatorTest, MakeShared) {
  auto simple_struct = allocator_->make_shared<SimpleStruct>();
  simple_struct->name = "unique";
  simple_struct->value = 2;
  EXPECT_EQ(simple_struct->name, "unique");
  EXPECT_EQ(simple_struct->value, 2);

  auto class_without_args = allocator_->make_shared<ClassWithoutArgs>();
  class_without_args->SetName("unique");
  EXPECT_EQ(class_without_args->GetName(), "unique");

  auto class_with_args = allocator_->make_shared<ClassWithArgs>("unique", 2);
  EXPECT_EQ(class_with_args->GetName(), "unique");
  EXPECT_EQ(class_with_args->GetValue(), 2);
}

TEST_F(AllocatorTest, TrackedAllocator) {
  marl::TrackedAllocator tracked_allocator{allocator_};
  auto simple_struct1 = tracked_allocator.create<SimpleStruct>();
  auto stats = tracked_allocator.stats();
  EXPECT_EQ(stats.numAllocations(), 1);
  EXPECT_EQ(stats.bytesAllocated(), sizeof(SimpleStruct));

  auto simple_struct2 = tracked_allocator.create<SimpleStruct>();
  stats = tracked_allocator.stats();
  EXPECT_EQ(stats.numAllocations(), 2);
  EXPECT_EQ(stats.bytesAllocated(), 2 * sizeof(SimpleStruct));

  tracked_allocator.destroy(simple_struct2);
  stats = tracked_allocator.stats();
  EXPECT_EQ(stats.numAllocations(), 1);
  EXPECT_EQ(stats.bytesAllocated(), sizeof(SimpleStruct));

  tracked_allocator.destroy(simple_struct1);
  stats = tracked_allocator.stats();
  EXPECT_EQ(stats.numAllocations(), 0);
  EXPECT_EQ(stats.bytesAllocated(), 0);
}

TEST_F(AllocatorTest, StlAllocator) {
  std::vector<int, marl::StlAllocator<int>> int_vec{marl::StlAllocator<int>(allocator_)};
  int element_num = 20;
  int_vec.reserve(element_num);
  for (int i = 0; i < element_num; ++i) {
    int_vec.push_back(1);
  }
  EXPECT_EQ(int_vec.size(), element_num);
  int_vec.insert(int_vec.begin(), 1);
  EXPECT_EQ(int_vec.size(), element_num + 1);
  int_vec.resize(200);
  int_vec.shrink_to_fit();
  int_vec.clear();
  EXPECT_EQ(int_vec.size(), 0);

  std::vector<std::string, marl::StlAllocator<std::string>> str_vec{marl::StlAllocator<std::string>(allocator_)};
  str_vec.push_back("hello, world");
  EXPECT_EQ(str_vec.size(), 1);
  str_vec.emplace_back("world, hello");
  EXPECT_EQ(str_vec.size(), 2);
}
