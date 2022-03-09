#include "marl/debug.hpp"
#include "marl/scheduler.hpp"

#include <cstdarg>
#include <cstdlib>
#include <cstdio>

namespace marl {

void fatal(const char *msg, ...) {
  va_list vararg;
  va_start(vararg, msg);
  vfprintf(stderr, msg, vararg);
  va_end(vararg);
  abort();
}

void warn(const char *msg, ...) {
  va_list vararg;
  va_start(vararg, msg);
  vfprintf(stderr, msg, vararg);
  va_end(vararg);
}

void assert_has_bound_scheduler(const char *feature) {
  (void)feature;  // 防止编译器警告
  MARL_ASSERT(Scheduler::get() != nullptr,
              "%s requires a marl::Scheduler to be bound", feature);
}

}
