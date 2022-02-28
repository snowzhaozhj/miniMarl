#ifndef MINIMARL_INCLUDE_EXPORT_HPP_
#define MINIMARL_INCLUDE_EXPORT_HPP_

#define MARL_EXPORT __attribute__((visibility("default")))
#define MARL_NO_EXPORT __attribute__((visibility("hidden")))

#endif //MINIMARL_INCLUDE_EXPORT_HPP_
