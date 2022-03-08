#define MARL_REG_RBX 0x00
#define MARL_REG_RBP 0x08
#define MARL_REG_R12 0x10
#define MARL_REG_R13 0x18
#define MARL_REG_R14 0x20
#define MARL_REG_R15 0x28
#define MARL_REG_RDI 0x30
#define MARL_REG_RSI 0x38
#define MARL_REG_RSP 0x40
#define MARL_REG_RIP 0x48

#if defined(__APPLE__)
#define MARL_ASM_SYMBOL(x) _##x
#else
#define MARL_ASM_SYMBOL(x) x
#endif

#ifndef MARL_BUILD_ASM

#include <stdint.h>

struct marl_fiber_context {
  // 被调用者保护寄存器
  uintptr_t RBX;
  uintptr_t RBP;
  uintptr_t R12;
  uintptr_t R13;
  uintptr_t R14;
  uintptr_t R15;

  // 参数寄存器
  uintptr_t RDI;
  uintptr_t RSI;

  // 栈寄存器和指令寄存器
  uintptr_t RSP;
  uintptr_t RIP;
};

#ifdef __cplusplus
#include <cstddef>
static_assert(offsetof(marl_fiber_context, RBX) == MARL_REG_RBX,
              "Bad register offset");
static_assert(offsetof(marl_fiber_context, RBP) == MARL_REG_RBP,
              "Bad register offset");
static_assert(offsetof(marl_fiber_context, R12) == MARL_REG_R12,
              "Bad register offset");
static_assert(offsetof(marl_fiber_context, R13) == MARL_REG_R13,
              "Bad register offset");
static_assert(offsetof(marl_fiber_context, R14) == MARL_REG_R14,
              "Bad register offset");
static_assert(offsetof(marl_fiber_context, R15) == MARL_REG_R15,
              "Bad register offset");
static_assert(offsetof(marl_fiber_context, RDI) == MARL_REG_RDI,
              "Bad register offset");
static_assert(offsetof(marl_fiber_context, RSI) == MARL_REG_RSI,
              "Bad register offset");
static_assert(offsetof(marl_fiber_context, RSP) == MARL_REG_RSP,
              "Bad register offset");
static_assert(offsetof(marl_fiber_context, RIP) == MARL_REG_RIP,
              "Bad register offset");
#endif  // __cplusplus

#endif  // MARL_BUILD_ASM
