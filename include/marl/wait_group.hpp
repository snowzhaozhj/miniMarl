#ifndef MINIMARL_INCLUDE_MARL_WAIT_GROUP_HPP_
#define MINIMARL_INCLUDE_MARL_WAIT_GROUP_HPP_

#include "condition_variable.hpp"
#include "debug.hpp"

namespace marl {

/// WaitGroup是一个同步原语，内部有一个计数器，可以增、减、以及等待计数器变为0
/// 可以用于等待一系列并发任务完成
class WaitGroup {
 public:
  MARL_NO_EXPORT inline WaitGroup(unsigned int initial_count = 0,
                                  Allocator *allocator = Allocator::Default)
      : data_(std::make_shared<Data>(allocator)) {
    data_->count = initial_count;
  }

  /// 使内部的计数器增加count
  MARL_NO_EXPORT inline void add(unsigned int count = 1) const {
    data_->count += count;
  }

  /// 使内部的计数器减1
  MARL_NO_EXPORT inline bool done() const {
    MARL_ASSERT(data_->count > 0, "marl::WaitGroup::done() called too many times");
    auto count = --data_->count;
    if (count == 0) {
      marl::lock lock(data_->mutex);
      data_->cv.notify_all();
      return true;
    }
    return false;
  }

  /// 阻塞，直到内部计数器为0
  MARL_NO_EXPORT inline void wait() const {
    marl::lock lock(data_->mutex);
    data_->cv.wait(lock, [this] { return data_->count == 0; });
  }

 private:
  struct Data {
    MARL_NO_EXPORT inline Data(Allocator *allocator)
        : cv(allocator) {}

    std::atomic<unsigned int> count{0};
    ConditionVariable cv;
    marl::mutex mutex;
  };
  const std::shared_ptr<Data> data_;  ///< 通过使用shared_ptr使得WaitGroup在复制的情况下也能正确运行
};

} // namespace marl

#endif //MINIMARL_INCLUDE_MARL_WAIT_GROUP_HPP_
