#ifndef MINIMARL_INCLUDE_MARL_DEFER_HPP_
#define MINIMARL_INCLUDE_MARL_DEFER_HPP_

#include "finally.hpp"

namespace marl {

#define MARL_CONCAT_(a, b) a##b
#define MARL_CONCAT(a, b) MARL_CONCAT_(a, b)

/// 传入defer中的代码，将会在作用域结束时运行
#define defer(x) \
  auto MARL_CONCAT(defer_, __LINE__) = marl::make_finally([&] { x; })

} // namespace marl

#endif //MINIMARL_INCLUDE_MARL_DEFER_HPP_
