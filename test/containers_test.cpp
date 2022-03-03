#include "marl/containers.hpp"

#include "marl_test.hpp"

class ContainersVectorTest : public WithoutBoundScheduler {};

class ContainersListTest : public WithoutBoundScheduler {};

TEST_F(ContainersVectorTest, Empty) {
  marl::containers::vector<std::string, 4> vec(allocator_);
  EXPECT_EQ(vec.size(), 0);
}

TEST_F(ContainersVectorTest, WithinFixedCapIndex) {
  marl::containers::vector<std::string, 4> vec(allocator_);
  vec.resize(4);
  vec[0] = "A";
  vec[1] = "B";
  vec[2] = "C";
  vec[3] = "D";
  EXPECT_EQ(vec[0], "A");
  EXPECT_EQ(vec[1], "B");
  EXPECT_EQ(vec[2], "C");
  EXPECT_EQ(vec[3], "D");
}

TEST_F(ContainersVectorTest, BeyondFixedCapIndex) {
  marl::containers::vector<std::string, 1> vec(allocator_);
  vec.resize(4);
  vec[0] = "A";
  vec[1] = "B";
  vec[2] = "C";
  vec[3] = "D";
  EXPECT_EQ(vec[0], "A");
  EXPECT_EQ(vec[1], "B");
  EXPECT_EQ(vec[2], "C");
  EXPECT_EQ(vec[3], "D");
}

TEST_F(ContainersVectorTest, WithinFixedCapPushPop) {
  marl::containers::vector<std::string, 4> vec(allocator_);
  vec.push_back("A");
  vec.push_back("B");
  vec.push_back("C");
  vec.push_back("D");
  EXPECT_EQ(vec.size(), 4);
  EXPECT_EQ(vec.end() - vec.begin(), 4);
  EXPECT_EQ(vec.front(), "A");
  EXPECT_EQ(vec.back(), "D");

  vec.pop_back();
  EXPECT_EQ(vec.size(), 3);
  EXPECT_EQ(vec.end() - vec.begin(), 3);
  EXPECT_EQ(vec.front(), "A");
  EXPECT_EQ(vec.back(), "C");

  vec.pop_back();
  EXPECT_EQ(vec.size(), 2);
  EXPECT_EQ(vec.end() - vec.begin(), 2);
  EXPECT_EQ(vec.front(), "A");
  EXPECT_EQ(vec.back(), "B");

  vec.pop_back();
  EXPECT_EQ(vec.size(), 1);
  EXPECT_EQ(vec.end() - vec.begin(), 1);
  EXPECT_EQ(vec.front(), "A");
  EXPECT_EQ(vec.back(), "A");

  vec.pop_back();
  EXPECT_EQ(vec.size(), 0);
}

TEST_F(ContainersVectorTest, BeyondFixedCapPushPop) {
  marl::containers::vector<std::string, 2> vec(allocator_);
  vec.push_back("A");
  vec.push_back("B");
  vec.push_back("C");
  vec.push_back("D");
  EXPECT_EQ(vec.size(), 4);
  EXPECT_EQ(vec.end() - vec.begin(), 4);
  EXPECT_EQ(vec.front(), "A");
  EXPECT_EQ(vec.back(), "D");

  vec.pop_back();
  EXPECT_EQ(vec.size(), 3);
  EXPECT_EQ(vec.end() - vec.begin(), 3);
  EXPECT_EQ(vec.front(), "A");
  EXPECT_EQ(vec.back(), "C");

  vec.pop_back();
  EXPECT_EQ(vec.size(), 2);
  EXPECT_EQ(vec.end() - vec.begin(), 2);
  EXPECT_EQ(vec.front(), "A");
  EXPECT_EQ(vec.back(), "B");

  vec.pop_back();
  EXPECT_EQ(vec.size(), 1);
  EXPECT_EQ(vec.end() - vec.begin(), 1);
  EXPECT_EQ(vec.front(), "A");
  EXPECT_EQ(vec.back(), "A");

  vec.pop_back();
  EXPECT_EQ(vec.size(), 0);
}

TEST_F(ContainersVectorTest, CopyConstruct) {
  marl::containers::vector<std::string, 4> vec1(allocator_);
  vec1.resize(3);
  vec1[0] = "A";
  vec1[1] = "B";
  vec1[2] = "C";

  marl::containers::vector<std::string, 4> vec2(vec1, allocator_);
  EXPECT_EQ(vec2.size(), 3);
  EXPECT_EQ(vec2[0], "A");
  EXPECT_EQ(vec2[1], "B");
  EXPECT_EQ(vec2[2], "C");
}

TEST_F(ContainersVectorTest, CopyConstructDifferentBaseCapacity) {
  marl::containers::vector<std::string, 4> vec1(allocator_);
  vec1.resize(3);
  vec1[0] = "A";
  vec1[1] = "B";
  vec1[2] = "C";

  marl::containers::vector<std::string, 2> vec2(vec1, allocator_);
  EXPECT_EQ(vec2.size(), 3);
  EXPECT_EQ(vec2[0], "A");
  EXPECT_EQ(vec2[1], "B");
  EXPECT_EQ(vec2[2], "C");
}

TEST_F(ContainersVectorTest, CopyAssignment) {
  marl::containers::vector<std::string, 4> vec1(allocator_);
  vec1.resize(3);
  vec1[0] = "A";
  vec1[1] = "B";
  vec1[2] = "C";

  marl::containers::vector<std::string, 4> vec2(allocator_);
  vec2 = vec1;
  EXPECT_EQ(vec2.size(), 3);
  EXPECT_EQ(vec2[0], "A");
  EXPECT_EQ(vec2[1], "B");
  EXPECT_EQ(vec2[2], "C");
}

TEST_F(ContainersVectorTest, CopyAssignmentDifferentCapacity) {
  marl::containers::vector<std::string, 4> vec1(allocator_);
  vec1.resize(3);
  vec1[0] = "A";
  vec1[1] = "B";
  vec1[2] = "C";

  marl::containers::vector<std::string, 2> vec2(allocator_);
  vec2 = vec1;
  EXPECT_EQ(vec2.size(), 3);
  EXPECT_EQ(vec2[0], "A");
  EXPECT_EQ(vec2[1], "B");
  EXPECT_EQ(vec2[2], "C");
}

TEST_F(ContainersVectorTest, MoveConstruct) {
  marl::containers::vector<std::string, 4> vec1(allocator_);
  vec1.resize(3);
  vec1[0] = "A";
  vec1[1] = "B";
  vec1[2] = "C";

  marl::containers::vector<std::string, 2> vec2(std::move(vec1), allocator_);
  EXPECT_EQ(vec2.size(), 3);
  EXPECT_EQ(vec2[0], "A");
  EXPECT_EQ(vec2[1], "B");
  EXPECT_EQ(vec2[2], "C");
}

TEST_F(ContainersVectorTest, Copy) {
  marl::containers::vector<std::string, 4> vec1(allocator_);
  vec1.resize(3);
  vec1[0] = "A";
  vec1[1] = "B";
  vec1[2] = "C";

  marl::containers::vector<std::string, 2> vec2(allocator_);
  vec2.resize(1);
  vec2[0] = "Z";

  vec2 = vec1;
  EXPECT_EQ(vec2.size(), 3);
  EXPECT_EQ(vec2[0], "A");
  EXPECT_EQ(vec2[1], "B");
  EXPECT_EQ(vec2[2], "C");
}

TEST_F(ContainersVectorTest, Move) {
  marl::containers::vector<std::string, 4> vec1(allocator_);
  vec1.resize(3);
  vec1[0] = "A";
  vec1[1] = "B";
  vec1[2] = "C";

  marl::containers::vector<std::string, 2> vec2(allocator_);
  vec2.resize(1);
  vec2[0] = "Z";

  vec2 = std::move(vec1);
  EXPECT_EQ(vec1.size(), 0);  // 实现确保已移动状态vector为空
  EXPECT_EQ(vec2.size(), 3);
  EXPECT_EQ(vec2[0], "A");
  EXPECT_EQ(vec2[1], "B");
  EXPECT_EQ(vec2[2], "C");
}

TEST_F(ContainersListTest, Empty) {
  marl::containers::list<std::string> list(allocator_);
  EXPECT_EQ(list.size(), 0);
}

TEST_F(ContainersListTest, EmplaceOne) {
  marl::containers::list<std::string> l(allocator_);
  auto it = l.emplace_front("hello");
  EXPECT_EQ(*it, "hello");
  EXPECT_EQ(l.size(), 1);

  auto cur = l.begin();
  EXPECT_EQ(cur, it);
  ++cur;
  EXPECT_EQ(cur, l.end());
}

TEST_F(ContainersListTest, EmplaceThree) {
  marl::containers::list<std::string> l(allocator_);
  auto it1 = l.emplace_front("a");
  auto it2 = l.emplace_front("b");
  auto it3 = l.emplace_front("c");
  EXPECT_EQ(*it1, "a");
  EXPECT_EQ(*it2, "b");
  EXPECT_EQ(*it3, "c");
  EXPECT_EQ(l.size(), 3);

  auto cur = l.begin();
  EXPECT_EQ(cur, it3);
  ++cur;
  EXPECT_EQ(cur, it2);
  ++cur;
  EXPECT_EQ(cur, it1);
  ++cur;
  EXPECT_EQ(cur, l.end());
}

TEST_F(ContainersListTest, EraseFront) {
  marl::containers::list<std::string> l(allocator_);
  auto it1 = l.emplace_front("a");
  auto it2 = l.emplace_front("b");
  auto it3 = l.emplace_front("c");
  l.erase(it3);
  EXPECT_EQ(l.size(), 2);

  auto cur = l.begin();
  EXPECT_EQ(cur, it2);
  ++cur;
  EXPECT_EQ(cur, it1);
  ++cur;
  EXPECT_EQ(cur, l.end());
}

TEST_F(ContainersListTest, EraseBack) {
  marl::containers::list<std::string> l(allocator_);
  auto it1 = l.emplace_front("a");
  auto it2 = l.emplace_front("b");
  auto it3 = l.emplace_front("c");
  l.erase(it1);
  EXPECT_EQ(l.size(), 2);

  auto cur = l.begin();
  EXPECT_EQ(cur, it3);
  ++cur;
  EXPECT_EQ(cur, it2);
  ++cur;
  EXPECT_EQ(cur, l.end());
}

TEST_F(ContainersListTest, EraseMid) {
  marl::containers::list<std::string> l(allocator_);
  auto it1 = l.emplace_front("a");
  auto it2 = l.emplace_front("b");
  auto it3 = l.emplace_front("c");
  l.erase(it2);
  EXPECT_EQ(l.size(), 2);

  auto cur = l.begin();
  EXPECT_EQ(cur, it3);
  ++cur;
  EXPECT_EQ(cur, it1);
  ++cur;
  EXPECT_EQ(cur, l.end());
}

TEST_F(ContainersListTest, Grow) {
  marl::containers::list<std::string> l(allocator_);
  for (int i = 0; i < 256; i++) {
    l.emplace_front(std::to_string(i));
  }
  ASSERT_EQ(l.size(), size_t(256));
}
