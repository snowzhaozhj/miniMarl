#ifndef MINIMARL_SRC_OS_FIBER_HPP_
#define MINIMARL_SRC_OS_FIBER_HPP_

#include "marl/sanitizer.hpp"

#ifndef MARL_USE_FIBER_STACK_GUARDS
#define MARL_USE_FIBER_STACK_GUARDS 1
#endif

#if defined(MARL_FIBERS_USE_UCONTEXT)
#include "osfiber_ucontext.hpp"
#else
#include "osfiber_asm.hpp"
#endif

#endif //MINIMARL_SRC_OS_FIBER_HPP_
