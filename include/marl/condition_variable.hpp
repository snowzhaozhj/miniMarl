#ifndef MINIMARL_INCLUDE_MARL_CONDITION_VARIABLE_HPP_
#define MINIMARL_INCLUDE_MARL_CONDITION_VARIABLE_HPP_

#include "containers.hpp"
#include "debug.hpp"
#include "memory.hpp"
#include "mutex.hpp"
#include "scheduler.hpp"
#include "tsa.hpp"

namespace marl {

/// 条件变量是一种同步方式，用于阻塞一个或多个fiber或线程
/// 直到另一个fiber或线程使得条件满足并且通知条件变量
class ConditionVariable {
 public:
  MARL_NO_EXPORT inline ConditionVariable(Allocator *allocator = Allocator::Default)
      : waiting_(allocator) {}

  /// 通知并且可能将一个正在等待的fiber或thread恢复过来
  MARL_NO_EXPORT inline void notify_one() {
    if (num_waiting_ == 0) return;
    {
      marl::lock lock(mutex_);
      if (waiting_.size() > 0) {
        (*waiting_.begin())->notify();
        return;
      }
    }
    if (num_waiting_on_condition > 0) {
      condition_.notify_one();
    }
  }

  /// 通知并且可能将所有正在等待的fiber或thread恢复过来
  MARL_NO_EXPORT inline void notify_all() {
    if (num_waiting_ == 0) return;
    {
      marl::lock lock(mutex_);
      for (auto fiber : waiting_) {
        fiber->notify();
      }
    }
    if (num_waiting_on_condition > 0) {
      condition_.notify_all();
    }
  }

  /// 阻塞当前fiber或者thread，直到Pred为真并且条件变量被通知
  template<typename Predicate>
  MARL_NO_EXPORT inline void wait(marl::lock &lock, Predicate &&pred) {
    if (pred()) return;
    ++num_waiting_;
    if (auto fiber = Scheduler::Fiber::current()) {
      // 在fiber执行环境中
      mutex_.lock();
      auto it = waiting_.emplace_front(fiber);
      mutex_.unlock();

      fiber->wait(lock, pred);

      mutex_.lock();
      waiting_.erase(it);
      mutex_.lock();
    } else {
      /// 运行在非fiber环境，直接委托给std::condition_variable
      ++num_waiting_on_condition;
      lock.wait(condition_, pred);
      --num_waiting_on_condition;
    }
    --num_waiting_;
  }

  /// 阻塞当前fiber或者thread，直到Pred为真并且条件变量被通知, 或者已经超时
  /// @return 如果超时的时候Pred仍为假的话，则返回false，其他情况返回true
  template<typename Rep, typename Period, typename Predicate>
  MARL_NO_EXPORT inline bool wait_for(
      marl::lock &lock,
      const std::chrono::duration<Rep, Period> &duration,
      Predicate &&pred) {
    return wait_until(lock, std::chrono::system_clock::now() + duration, pred);
  }

  /// 阻塞当前fiber或者thread，直到Pred为真并且条件变量被通知, 或者已经超时
  /// @return 如果超时的时候Pred仍为假的话，则返回false，其他情况返回true
  template<typename Clock, typename Duration, typename Predicate>
  MARL_NO_EXPORT inline bool wait_until(
      marl::lock &lock,
      const std::chrono::time_point<Clock, Duration> &timeout,
      Predicate &&pred) {
    if (pred()) return true;
    ++num_waiting_;
    bool res;
    if (auto fiber = Scheduler::Fiber::current()) {
      // 在fiber执行环境中
      mutex_.lock();
      auto it = waiting_.emplace_front(fiber);
      mutex_.unlock();

      res = fiber->wait(lock, timeout, pred);

      mutex_.lock();
      waiting_.erase(it);
      mutex_.lock();
    } else {
      /// 运行在非fiber环境，直接委托给std::condition_variable
      ++num_waiting_on_condition;
      res = lock.wait_until(condition_, timeout, pred);
      --num_waiting_on_condition;
    }
    --num_waiting_;
    return res;
  }

 private:
  ConditionVariable(const ConditionVariable &) = delete;
  ConditionVariable(ConditionVariable &&) = delete;
  ConditionVariable &operator=(const ConditionVariable &) = delete;
  ConditionVariable &operator=(ConditionVariable &&) = delete;

  marl::mutex mutex_;
  containers::list<Scheduler::Fiber *> waiting_;
  std::condition_variable condition_;
  std::atomic<int> num_waiting_{0};
  std::atomic<int> num_waiting_on_condition{0};
};

} // namespace marl

#endif //MINIMARL_INCLUDE_MARL_CONDITION_VARIABLE_HPP_
