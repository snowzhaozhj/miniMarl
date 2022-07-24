#include "marl/dag.hpp"

#include "marl_test.hpp"

namespace {

struct Data {
  std::mutex mutex;
  std::vector<std::string> order;

  void push(std::string &&s) {
    std::lock_guard<std::mutex> lg(mutex);
    order.emplace_back(std::move(s));
  }
};

template<typename T>
std::vector<T> slice(const std::vector<T> &in, size_t from, size_t to) {
  return {in.begin() + from, in.begin() + to};
}

}

class DAGTestWithBound : public WithBoundScheduler {};

INSTANTIATE_WithBoundSchedulerTest(DAGTestWithBound);

TEST_P(DAGTestWithBound, ChainWithoutArg) {
  marl::DAG<>::Builder builder;

  Data data;
  builder.root()
      .then([&] { data.push("A"); })
      .then([&] { data.push("B"); })
      .then([&] { data.push("C"); });

  auto dag = builder.build();
  dag->run();

  ASSERT_THAT(data.order, testing::ElementsAre("A", "B", "C"));
}

TEST_P(DAGTestWithBound, Chain) {
  marl::DAG<Data &>::Builder builder;

  builder.root()
      .then([](Data &data) { data.push("A"); })
      .then([](Data &data) { data.push("B"); })
      .then([](Data &data) { data.push("C"); });

  auto dag = builder.build();

  Data data;
  dag->run(data);

  ASSERT_THAT(data.order, testing::ElementsAre("A", "B", "C"));
}

TEST_P(DAGTestWithBound, RepeatRun) {
  marl::DAG<Data &>::Builder builder;

  builder.root()
      .then([](Data &data) { data.push("A"); })
      .then([](Data &data) { data.push("B"); })
      .then([](Data &data) { data.push("C"); });

  auto dag = builder.build();

  Data dataA, dataB;
  dag->run(dataA);
  dag->run(dataB);
  dag->run(dataA);

  ASSERT_THAT(dataA.order, testing::ElementsAre("A", "B", "C", "A", "B", "C"));
  ASSERT_THAT(dataB.order, testing::ElementsAre("A", "B", "C"));
}

TEST_P(DAGTestWithBound, FanOutFromRoot) {
  marl::DAG<Data &>::Builder builder;

  auto root = builder.root();
  root.then([](Data &data) { data.push("A"); });
  root.then([](Data &data) { data.push("B"); });
  root.then([](Data &data) { data.push("C"); });

  auto dag = builder.build();

  Data data;
  dag->run(data);

  ASSERT_THAT(data.order, testing::UnorderedElementsAre("A", "B", "C"));
}

TEST_P(DAGTestWithBound, FanOutFromNonRoot) {
  marl::DAG<Data &>::Builder builder;

  auto root = builder.root();
  auto node = root.then([](Data &data) { data.push("N"); });
  node.then([](Data &data) { data.push("A"); });
  node.then([](Data &data) { data.push("B"); });
  node.then([](Data &data) { data.push("C"); });

  auto dag = builder.build();

  Data data;
  dag->run(data);

  ASSERT_THAT(data.order, testing::UnorderedElementsAre("N", "A", "B", "C"));
  ASSERT_EQ(data.order[0], "N");
  ASSERT_THAT(slice(data.order, 1, 4), testing::UnorderedElementsAre("A", "B", "C"));
}

TEST_P(DAGTestWithBound, FanOutFanIn) {
  marl::DAG<Data &>::Builder builder;

  auto root = builder.root();
  auto a0 = root.then([](Data &data) { data.push("A0"); });
  auto a1 = root.then([](Data &data) { data.push("A1"); });

  auto b = builder.node([](Data &data) { data.push("B"); }, {a0, a1});

  auto c0 = b.then([](Data &data) { data.push("C0"); });
  auto c1 = b.then([](Data &data) { data.push("C1"); });
  auto c2 = b.then([](Data &data) { data.push("C2"); });
  auto p = std::make_index_sequence<4>();

  auto d = builder.node([](Data &data) { data.push("D"); }, {c0, c1, c2});

  auto e0 = d.then([](Data &data) { data.push("E0"); });
  auto e1 = d.then([](Data &data) { data.push("E1"); });
  auto e2 = d.then([](Data &data) { data.push("E2"); });
  auto e3 = d.then([](Data &data) { data.push("E3"); });

  builder.node([](Data &data) { data.push("F"); }, {e0, e1, e2, e3});

  auto dag = builder.build();

  Data data;
  dag->run(data);

  ASSERT_THAT(data.order,
              testing::UnorderedElementsAre("A0", "A1", "B", "C0", "C1", "C2", "D", "E0",
                                            "E1", "E2", "E3", "F"));
  ASSERT_THAT(slice(data.order, 0, 2), testing::UnorderedElementsAre("A0", "A1"));
  ASSERT_THAT(data.order[2], "B");
  ASSERT_THAT(slice(data.order, 3, 6), testing::UnorderedElementsAre("C0", "C1", "C2"));
  ASSERT_THAT(data.order[6], "D");
  ASSERT_THAT(slice(data.order, 7, 11),
              testing::UnorderedElementsAre("E0", "E1", "E2", "E3"));
  ASSERT_THAT(data.order[11], "F");
}
