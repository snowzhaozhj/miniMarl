#if defined(__x86_64__)

#include "osfiber_asm_x64.h"

#include "marl/export.hpp"

MARL_EXPORT
void marl_fiber_trampoline(void (*target)(void *), void *arg) {
  target(arg);
}

MARL_EXPORT
void marl_fiber_set_target(struct marl_fiber_context *ctx,
                           void *stack,
                           uint32_t stack_size,
                           void (*target)(void *),
                           void *arg) {
  uintptr_t *stack_top = (uintptr_t *) ((uint8_t *) (stack) + stack_size);
  ctx->RIP = (uintptr_t)&marl_fiber_trampoline;
  ctx->RDI = (uintptr_t)target;
  ctx->RSI = (uintptr_t)arg;
  ctx->RSP = (uintptr_t)&stack_top[-3];
  stack_top[-2] = 0;  // No return target
  // 上述两行代码的逻辑可参考：https://github.com/google/marl/issues/199
  // 个人理解（可能有误）：
  // 1. 取&stack_top[-3]是为了满足对齐要求
  // 2. 设置stack_top[-2] = 0是为了防止函数返回到未知的地址, 如果返回的话，直接访问非法地址0
}

#endif  // defined(__x86_64__)
