#ifndef MINIMARL_INCLUDE_MARL_DEBUG_HPP_
#define MINIMARL_INCLUDE_MARL_DEBUG_HPP_

#include "export.hpp"

namespace marl {

MARL_EXPORT
void fatal(const char *msg, ...);

MARL_EXPORT
void warn(const char *msg, ...);

MARL_EXPORT
void assert_has_bound_scheduler(const char *feature);

#define MARL_FATAL(msg, ...) marl::fatal(msg "\n", ##__VA_ARGS__)
#define MARL_ASSERT(cond, msg, ...) \
  do {                              \
    if (!(cond)) {                  \
      MARL_FATAL("ASSERT: " msg, ##__VA_ARGS__); \
    }                               \
  } while (false)
#define MARL_ASSERT_HAS_BOUND_SCHEDULER(feature) \
  assert_has_bound_scheduler(feature)
#define MARL_UNREACHABLE() MARL_FATAL("UNREACHABLE")
#define MARL_WARN(msg, ...) marl::warn("WARNING: " msg "\n", ##__VA_ARGS__);

}
#endif //MINIMARL_INCLUDE_MARL_DEBUG_HPP_
