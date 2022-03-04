#include "marl/task.hpp"

#include "marl_test.hpp"

class TaskTest : public testing::Test {};

TEST_F(TaskTest, Construct) {
  int num{0};

  marl::Task task1;
  EXPECT_FALSE(task1.operator bool());
  EXPECT_TRUE(task1.is(marl::Task::Flags::None));

  marl::Task task2([&] { ++num; });
  EXPECT_TRUE(task2.operator bool());
  EXPECT_TRUE(task2.is(marl::Task::Flags::None));
  task2();
  EXPECT_EQ(num, 1);

  marl::Task::Function f = [&] { --num; };
  marl::Task task3(f, marl::Task::Flags::SameThread);
  EXPECT_TRUE(task3.operator bool());
  EXPECT_TRUE(task3.is(marl::Task::Flags::SameThread));
  task3();
  EXPECT_EQ(num, 0);

  marl::Task task4(task3);
  EXPECT_TRUE(task4.operator bool());
  EXPECT_TRUE(task4.is(marl::Task::Flags::SameThread));
  task4();
  EXPECT_EQ(num, -1);
  EXPECT_TRUE(task3.operator bool());
  EXPECT_TRUE(task3.is(marl::Task::Flags::SameThread));
  task3();
  EXPECT_EQ(num, -2);

  marl::Task task5(std::move(task3));
  EXPECT_TRUE(task4.operator bool());
  EXPECT_TRUE(task4.is(marl::Task::Flags::SameThread));
  task5();
  EXPECT_EQ(num, -3);
}

TEST_F(TaskTest, Assign) {
  int num{0};

  marl::Task::Function f = [&] { ++num; };
  marl::Task task1(f);
  marl::Task task2;
  task2 = task1;
  EXPECT_TRUE(task2.operator bool());
  EXPECT_TRUE(task2.is(marl::Task::Flags::None));
  task2();
  EXPECT_EQ(num, 1);

  marl::Task task3;
  task3 = std::move(task1);
  EXPECT_TRUE(task3.operator bool());
  EXPECT_TRUE(task3.is(marl::Task::Flags::None));
  task3();
  EXPECT_EQ(num, 2);

  marl::Task task4;
  task4 = f;
  EXPECT_TRUE(task4.operator bool());
  EXPECT_TRUE(task4.is(marl::Task::Flags::None));
  task4();
  EXPECT_EQ(num, 3);

  marl::Task task5;
  task5 = std::move(f);
  EXPECT_TRUE(task5.operator bool());
  EXPECT_TRUE(task5.is(marl::Task::Flags::None));
  task5();
  EXPECT_EQ(num, 4);
}
