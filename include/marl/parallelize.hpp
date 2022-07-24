#ifndef MINIMARL_INCLUDE_MARL_PARALLELIZE_HPP_
#define MINIMARL_INCLUDE_MARL_PARALLELIZE_HPP_

#include "export.hpp"
#include "scheduler.hpp"
#include "wait_group.hpp"

namespace marl {

template<typename F0, typename ...FN>
MARL_NO_EXPORT inline void parallelize(F0 &&f0, FN &&...fn) {
  WaitGroup wg(sizeof...(FN));
  auto schedule_f = [&wg](auto &&f) {
    marl::schedule([=] {
      f();
      wg.done();
    });
  };
  (..., schedule_f(fn));
  f0();
  wg.wait();
}

}

#endif // MINIMARL_INCLUDE_MARL_PARALLELIZE_HPP_
