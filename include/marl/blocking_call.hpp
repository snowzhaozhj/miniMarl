#ifndef MINIMARL_INCLUDE_MARL_BLOCKING_CALL_HPP_
#define MINIMARL_INCLUDE_MARL_BLOCKING_CALL_HPP_

#include "export.hpp"
#include "scheduler.hpp"
#include "wait_group.hpp"

namespace marl {
namespace detail {

template<typename ReturnType>
class OnNewThread {
 public:
  template<typename F, typename ...Args>
  MARL_NO_EXPORT inline static ReturnType call(F &&f, Args &&...args) {
    ReturnType result;
    WaitGroup wg(1);
    auto scheduler = Scheduler::get();
    auto thread = std::thread([&, wg](Args &&...args) {
      if (scheduler != nullptr) {
        scheduler->bind();
      }
      result = f(std::forward<Args>(args)...);
      if (scheduler != nullptr) {
        Scheduler::unbind();
      }
      wg.done();
    }, std::forward<Args>(args)...);
    wg.wait();
    thread.join();
    return result;
  }
};

template<>
class OnNewThread<void> {
 public:
  template<typename F, typename ...Args>
  MARL_NO_EXPORT inline static void call(F &&f, Args &&...args) {
    WaitGroup wg(1);
    auto scheduler = Scheduler::get();
    auto thread = std::thread([&, wg](Args &&...args) {
      if (scheduler != nullptr) {
        scheduler->bind();
      }
      f(std::forward<Args>(args)...);
      if (scheduler != nullptr) {
        Scheduler::unbind();
      }
      wg.done();
    }, std::forward<Args>(args)...);
    wg.wait();
    thread.join();
  }
};

} // namespace marl::detail

/// blocking_call在一个新的线程上调用函数F，然后yield当前fiber去执行其他任务，直到F返回
template<typename F, typename ...Args>
MARL_NO_EXPORT auto inline blocking_call(F &&f, Args &&...args)
-> decltype(f(args...)) {
  return detail::OnNewThread<decltype(f(args...))>::call(
      std::forward<F>(f), std::forward<Args>(args)...);
}

} // namespace marl

#endif //MINIMARL_INCLUDE_MARL_BLOCKING_CALL_HPP_
