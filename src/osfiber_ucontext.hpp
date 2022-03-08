#ifndef MINIMARL_SRC_OSFIBER_UCONTEXT_HPP_
#define MINIMARL_SRC_OSFIBER_UCONTEXT_HPP_

#if !defined(_XOPEN_SOURCE)
// 必须位于其他#includes之前
#define _XOPEN_SOURCE
#endif

#include "marl/debug.hpp"
#include "marl/memory.hpp"

#include <functional>
#include <memory>

#include <ucontext.h>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif  // defined(__clang__)

namespace marl {

class OSFiber {
 public:
  inline OSFiber(Allocator *allocator) : allocator_(allocator) {}
  inline ~OSFiber() {
    if (stack_.ptr != nullptr) {
      allocator_->free(stack_);
    }
  }

  /// 在当前线程上创建一个Fiber
  MARL_NO_EXPORT static inline Allocator::unique_ptr<OSFiber> createFiberFromCurrentThread(
      Allocator *allocator) {
    auto out = allocator->make_unique<OSFiber>(allocator);
    out->context_ = {};
    getcontext(&out->context_);
    return out;
  }

  /// 创建一个栈大小为stack_size的fiber, 切换到该fiber后，将会运行func函数
  /// @note func函数的末尾禁止返回，而必须切换到另一个fiber
  MARL_NO_EXPORT static inline Allocator::unique_ptr<OSFiber> createFiber(
      Allocator *allocator,
      size_t stack_size,
      const std::function<void()> &func) {
    // makecontext只接受整形参数
    union Args {
      OSFiber *self;
      struct {
        int a;
        int b;
      };
    };

    struct Target {
      static void Main(int a, int b) {
        Args u;
        u.a = a;
        u.b = b;
        u.self->target_();
      }
    };

    Allocation::Request request;
    request.size = stack_size;
    request.alignment = 16;
    request.usage = Allocation::Usage::Stack;
#if MARL_USE_FIBER_STACK_GUARDS
    request.use_guards = true;
#endif

    auto out = allocator->make_unique<OSFiber>(allocator);
    out->context_ = {};
    out->target_ = func;
    out->stack_ = allocator->allocate(request);

    auto res = getcontext(&out->context_);
    (void) res;
    MARL_ASSERT(res == 0, "getcontext() returned %d", int(res));
    out->context_.uc_stack.ss_sp = out->stack_.ptr;
    out->context_.uc_stack.ss_size = stack_size;
    out->context_.uc_link = nullptr;

    Args args{};
    args.self = out.get();
    makecontext(&out->context_, reinterpret_cast<void (*)()>(&Target::Main), 2, args.a, args.b);
    return out;
  }

  /// 切换到另一个fiber
  /// @note 必须在当前正在运行的fiber中调用
  MARL_NO_EXPORT inline void switchTo(OSFiber *fiber) {
    auto res = swapcontext(&context_, &fiber->context_);
    (void) res;
    MARL_ASSERT(res == 0, "swapcontext() returned %d", int(res));
  }

 private:
  Allocator *allocator_;
  ucontext_t context_;
  std::function<void()> target_;
  Allocation stack_;
};

} // namespace marl

#if defined(__clang__)
#pragma clang diagnostic pop
#endif  // defined(__clang__)

#endif //MINIMARL_SRC_OSFIBER_UCONTEXT_HPP_
