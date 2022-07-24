#ifndef MINIMARL_INCLUDE_MARL_DAG_HPP_
#define MINIMARL_INCLUDE_MARL_DAG_HPP_

#include <iostream>

#include "containers.hpp"
#include "export.hpp"
#include "memory.hpp"
#include "scheduler.hpp"
#include "wait_group.hpp"

namespace marl {
namespace detail {
using DAGCounter = std::atomic<uint32_t>;

template<typename T>
struct DAGRunContext {
  T data;
  Allocator::unique_ptr<DAGCounter> counters;

  template<typename F>
  MARL_NO_EXPORT inline void invoke(F &&f) {
    f(data);
  }
};

template<>
struct DAGRunContext<void> {
  Allocator::unique_ptr<DAGCounter> counters;

  template<typename F>
  MARL_NO_EXPORT inline void invoke(F &&f) {
    f();
  }
};

template<typename T>
struct DAGWork {
  using type = std::function<void(T)>;
};
template<>
struct DAGWork<void> {
  using type = std::function<void()>;
};

} // namespace detail

template<typename T>
class DAG;

template<typename T>
class DAGBuilder;

template<typename T>
class DAGNodeBuilder;

template<typename T>
class DAGBase {
 protected:
  friend DAGBuilder<T>;
  friend DAGNodeBuilder<T>;

  using RunContext = detail::DAGRunContext<T>;
  using Counter = detail::DAGCounter;
  using NodeIndex = size_t;
  using Work = typename detail::DAGWork<T>::type;
  static constexpr size_t NumReservedNodes = 32;
  static constexpr size_t NumReservedNumOuts = 4;
  static constexpr size_t InvalidCounterIndex = ~static_cast<size_t>(0);
  static constexpr NodeIndex RootIndex = 0;
  static constexpr NodeIndex InvalidNodeIndex = ~static_cast<NodeIndex>(0);

  // DAG work node
  struct Node {
    MARL_NO_EXPORT inline Node() = default;
    MARL_NO_EXPORT inline Node(Work &&work) : work(std::move(work)) {}

    // 当前节点要运行的任务
    Work work;

    // RunContext中的counter的index,当前置任务完成时，counter会-1，当counter为0时，节点会被触发
    size_t counterIndex = InvalidCounterIndex;

    // 后置任务的nodeIndex
    containers::vector<NodeIndex, NumReservedNumOuts> outs;
  };

  MARL_NO_EXPORT inline void initCounters(RunContext *ctx,
                                          Allocator *allocator) {
    auto numCounters = initialCounters.size();
    ctx->counters = allocator->template make_unique_n<Counter>(numCounters);
    for (size_t i = 0; i < numCounters; ++i) {
      ctx->counters.get()[i] = {initialCounters[i]};
    }
  }

  // index代表节点的前置任务完成时，将会调用该方法
  // 如果节点的所有前置任务都已经完成，则会返回true，调用者接下来应该调用invoke方法
  MARL_NO_EXPORT inline bool notify(RunContext *ctx, NodeIndex nodeIdx) {
    Node *node = &nodes[nodeIdx];
    if (node->counterIndex == InvalidCounterIndex) {
      return true;
    }
    auto counters = ctx->counters.get();
    auto couter = --counters[node->counterIndex];
    return couter == 0;
  }

  // 调用index对应节点上的任务, 接着调用notify，将会启动后置任务
  MARL_NO_EXPORT inline void invoke(RunContext *ctx, NodeIndex nodeIdx, WaitGroup *wg) {
    Node *node = &nodes[nodeIdx];
    if (node->work) {
      ctx->invoke(node->work);
    }

    // 缓存要notify的node，直接调用最后一个invoke，可以避免调度的开销
    NodeIndex toInvoke = InvalidNodeIndex;
    for (NodeIndex idx : node->outs) {
      if (notify(ctx, idx)) {
        if (toInvoke != InvalidNodeIndex) {
          wg->add(1);
          schedule(
              [=](WaitGroup w) {
                invoke(ctx, toInvoke, &w);
                w.done();
              },
              *wg);
        }
        toInvoke = idx;
      }
    }
    if (toInvoke != InvalidNodeIndex) {
      invoke(ctx, toInvoke, wg);
    }
  }

  // 包含DAG中所有的节点，node[0]永远是root节点，没有任何前置依赖
  containers::vector<Node, NumReservedNodes> nodes;

  // 计数器的初始值列表，将会被复制到RunContext::counters
  containers::vector<uint32_t, NumReservedNodes> initialCounters;
};

template<typename T>
class DAGNodeBuilder {
  using NodeIndex = typename DAGBase<T>::NodeIndex;
 public:
  template<typename F>
  MARL_NO_EXPORT inline DAGNodeBuilder then(F &&work) {
    auto node = builder_->node(std::move(work));
    builder_->addDependency(*this, node);
    return node;
  }

 private:
  friend DAGBuilder<T>;

  MARL_NO_EXPORT inline DAGNodeBuilder(DAGBuilder<T> *builder, NodeIndex index)
      : builder_(builder), index_(index) {}

  DAGBuilder<T> *builder_;
  NodeIndex index_;
};

template<typename T>
class DAGBuilder {
 public:
  MARL_NO_EXPORT inline DAGBuilder(Allocator *allocator = Allocator::Default)
      : dag(allocator->template make_unique<DAG<T>>()), numIns(allocator) {
    // 添加root节点
    dag->nodes.push_back(Node{});
    numIns.push_back(0);
  }

  MARL_NO_EXPORT inline DAGNodeBuilder<T> root() {
    return DAGNodeBuilder<T>{this, DAGBase<T>::RootIndex};
  }

  template<typename F>
  MARL_NO_EXPORT inline DAGNodeBuilder<T> node(F &&work) {
    return node(std::forward<F>(work), {});
  }

  template<typename F>
  MARL_NO_EXPORT inline DAGNodeBuilder<T> node(F &&work,
                                               std::initializer_list<DAGNodeBuilder<T>> after) {
    MARL_ASSERT(numIns.size() == dag->nodes.size(),
                "NodeBuilder vectors out of sync");
    auto index = dag->nodes.size();
    numIns.push_back(0);
    dag->nodes.push_back(Node{std::move(work)});
    auto node = DAGNodeBuilder<T>{this, index};
    for (auto in : after) {
      addDependency(in, node);
    }
    return node;
  }

  MARL_NO_EXPORT inline void addDependency(DAGNodeBuilder<T> parent,
                                           DAGNodeBuilder<T> child) {
    ++numIns[child.index_];
    dag->nodes[parent.index_].outs.push_back(child.index_);
  }

  // 构造并返回DAG
  MARL_NO_EXPORT inline Allocator::unique_ptr<DAG<T>> build() {
    auto numNodes = dag->nodes.size();
    MARL_ASSERT(numIns.size() == dag->nodes.size(),
                "NodeBuilder vectors out of sync");
    for (size_t i = 0; i < numNodes; ++i) {
      if (numIns[i] > 1) {
        auto &node = dag->nodes[i];
        node.counterIndex = dag->initialCounters.size();
        dag->initialCounters.push_back(numIns[i]);
      }
    }
    return std::move(dag);
  }

 private:
  static constexpr size_t NumReservedNumIns = 4;
  using Node = typename DAG<T>::Node;

  Allocator::unique_ptr<DAG<T>> dag;
  containers::vector<uint32_t, NumReservedNumIns> numIns;
};

template<typename T = void>
class DAG : public DAGBase<T> {
 public:
  using Builder = DAGBuilder<T>;
  using NodeBuilder = DAGNodeBuilder<T>;

  MARL_NO_EXPORT inline void run(T &data,
                                 Allocator *allocator = Allocator::Default) {
    typename DAGBase<T>::RunContext ctx{data};
    this->initCounters(&ctx, allocator);
    WaitGroup wg;
    this->invoke(&ctx, this->RootIndex, &wg);
    wg.wait();
  }
};

template<>
class DAG<void> : public DAGBase<void> {
 public:
  using Builder = DAGBuilder<void>;
  using NodeBuilder = DAGNodeBuilder<void>;

  MARL_NO_EXPORT inline void run(Allocator *allocator = Allocator::Default) {
    typename DAGBase<void>::RunContext ctx{};
    this->initCounters(&ctx, allocator);
    WaitGroup wg;
    this->invoke(&ctx, this->RootIndex, &wg);
    wg.wait();
  }
};

} // namespace marl

#endif // MINIMARL_INCLUDE_MARL_DAG_HPP_
