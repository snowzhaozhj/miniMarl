#ifndef MINIMARL_SRC_OSFIBER_ASM_HPP_
#define MINIMARL_SRC_OSFIBER_ASM_HPP_

#if defined(__x86_64__)
#include "arch/osfiber_asm_x64.h"
#elif defined(__i386__)
#include "osfiber_asm_x86.h"
#elif defined(__aarch64__)
#include "osfiber_asm_aarch64.h"
#elif defined(__arm__)
#include "osfiber_asm_arm.h"
#elif defined(__powerpc64__)
#include "osfiber_asm_ppc64.h"
#elif defined(__mips__) && _MIPS_SIM == _ABI64
#include "osfiber_asm_mips64.h"
#elif defined(__riscv) && __riscv_xlen == 64
#include "osfiber_asm_rv64.h"
#elif defined(__loongarch__) && _LOONGARCH_SIM == _ABILP64
#include "osfiber_asm_loongarch64.h"
#else
#error "Unsupported target"
#endif

#include "marl/export.hpp"
#include "marl/memory.hpp"

#include <functional>
#include <memory>

extern "C" {

MARL_EXPORT
extern void marl_fiber_set_target(marl_fiber_context *ctx,
                                  void *stack,
                                  uint32_t stack_size,
                                  void (*target)(void *),
                                  void *arg);

MARL_EXPORT
extern void marl_fiber_swap(marl_fiber_context *from,
                            const marl_fiber_context *to);

} // extern "C"

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
    return out;
  }

  /// 创建一个栈大小为stack_size的fiber, 切换到该fiber后，将会运行func函数
  /// @note func函数的末尾禁止返回，而必须切换到另一个fiber
  MARL_NO_EXPORT static inline Allocator::unique_ptr<OSFiber> createFiber(
      Allocator *allocator,
      size_t stack_size,
      const std::function<void()> &func) {
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
    marl_fiber_set_target(&out->context_,
                          out->stack_.ptr,
                          static_cast<uint32_t>(stack_size),
                          reinterpret_cast<void (*)(void *)>(&OSFiber::run),
                          out.get());
    return out;
  }

  /// 切换到另一个fiber
  /// @note 必须在当前正在运行的fiber中调用
  MARL_NO_EXPORT inline void switchTo(OSFiber *fiber) {
    marl_fiber_swap(&context_, &fiber->context_);
  }

 private:
  MARL_NO_EXPORT
  static inline void run(OSFiber *self) {
    self->target_();
  }

  Allocator *allocator_;
  marl_fiber_context context_;
  std::function<void()> target_;
  Allocation stack_;
};

} // namespace marl


#endif //MINIMARL_SRC_OSFIBER_ASM_HPP_
