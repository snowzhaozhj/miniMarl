#if defined(__x86_64__)

#define MARL_BUILD_ASM 1
#include "osfiber_asm_x64.h"

// void marl_fiber_swap(marl_fiber_context *from, const marl_fiber_context *to)
// rdi: from
// rsi: to
.text
.global MARL_ASM_SYMBOL(marl_fiber_swap)
.align 4
MARL_ASM_SYMBOL(marl_fiber_swap):
  // 保存上下文from
  // 保存被调用者保护的寄存器
  movq %rbx, MARL_REG_RBX(%rdi)
  movq %rbp, MARL_REG_RBP(%rdi)
  movq %r12, MARL_REG_R12(%rdi)
  movq %r13, MARL_REG_R13(%rdi)
  movq %r14, MARL_REG_R14(%rdi)
  movq %r15, MARL_REG_R15(%rdi)
  // 保存IP和SP
  movq (%rsp), %rcx             // call指令(调用marl_fiber_swap时的call指令)在栈上已经保存好了返回地址
  movq %rcx, MARL_REG_RIP(%rdi)
  leaq 8(%rsp), %rcx            // 略过call指令保存好的地址(栈从高地址向地址增长)
  movq %rcx, MARL_REG_RSP(%rdi)

  // 加载上下文to
  movq %rsi, %r8
  // 加载被调用者保护寄存器
  movq MARL_REG_RBX(%r8), %rbx
  movq MARL_REG_RBP(%r8), %rbp
  movq MARL_REG_R12(%r8), %r12
  movq MARL_REG_R13(%r8), %r13
  movq MARL_REG_R14(%r8), %r14
  movq MARL_REG_R15(%r8), %r15
  // 加载两个调用参数
  movq MARL_REG_RDI(%r8), %rdi
  movq MARL_REG_RSI(%r8), %rsi
  // 加载栈指针
  movq MARL_REG_RSP(%r8), %rsp
  // 加载指令计数器RIP, 并且进行跳转
  movq MARL_REG_RIP(%r8), %rcx
  jmp *%rcx

#endif