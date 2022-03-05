#ifndef MINIMARL_INCLUDE_MARL_FINALLY_HPP_
#define MINIMARL_INCLUDE_MARL_FINALLY_HPP_

#include "export.hpp"

#include <functional>
#include <memory>

namespace marl {

class Finally {
 public:
  virtual ~Finally() = default;
};

/// Finally的实现
/// @tparam F 函数类型，函数签名必须为void()
template<typename F>
class FinallyImpl : public Finally {
 public:
  MARL_NO_EXPORT inline FinallyImpl(const F &func) : func_(func) {}
  MARL_NO_EXPORT inline FinallyImpl(F &&func) : func_(std::move(func)) {}
  MARL_NO_EXPORT inline FinallyImpl(FinallyImpl &&other) : func_(std::move(other.func_)) {
    other.valid_ = false;
  }
  MARL_NO_EXPORT inline ~FinallyImpl() {
    if (valid_) {
      func_();
    }
  }

 private:
  FinallyImpl(const FinallyImpl &other) = delete;
  FinallyImpl &operator=(const FinallyImpl &other) = delete;
  FinallyImpl &operator=(FinallyImpl &&f) = delete;

  F func_;
  bool valid_{true};
};

template<typename F>
inline FinallyImpl<F> make_finally(F &&f) {
  return FinallyImpl<F>(std::forward<F>(f));
}

template<typename F>
inline std::shared_ptr<Finally> make_shared_finally(F &&f) {
  return std::make_shared<FinallyImpl<F>>(std::forward<F>(f));
}

} // namespace marl

#endif //MINIMARL_INCLUDE_MARL_FINALLY_HPP_
