#ifndef MINIMARL_INCLUDE_MARL_TASK_HPP_
#define MINIMARL_INCLUDE_MARL_TASK_HPP_

#include "export.hpp"

#include <functional>
#include <utility>

namespace marl {

/// Scheduler以Task为基本工作单位
class Task {
 public:
  using Function = std::function<void()>;

  enum class Flags {
    None = 0,
    SameThread = 1, ///< 确保任务会在调度该任务的线程上运行
  };

  MARL_NO_EXPORT inline Task() = default;
  MARL_NO_EXPORT inline Task(const Task &other) = default;
  MARL_NO_EXPORT inline Task(Task &&other)
      : function_(std::move(other.function_)), flags_(other.flags_) {}
  MARL_NO_EXPORT inline Task(Function function,
                             Flags flags = Flags::None)
      : function_(std::move(function)), flags_(flags) {}

  MARL_NO_EXPORT inline Task &operator=(const Task &rhs) {
    function_ = rhs.function_;
    flags_ = rhs.flags_;
    return *this;
  }
  MARL_NO_EXPORT inline Task &operator=(Task &&rhs) {
    function_ = std::move(rhs.function_);
    flags_ = rhs.flags_;
    return *this;
  }
  MARL_NO_EXPORT inline Task &operator=(const Function &function) {
    function_ = function;
    flags_ = Flags::None;
    return *this;
  }
  MARL_NO_EXPORT inline Task &operator=(Function &&function) {
    function_ = std::move(function);
    flags_ = Flags::None;
    return *this;
  }

  /// @return 如果Task拥有一个合法的function则返回true,否则返回false
  MARL_NO_EXPORT inline operator bool() const {
    return function_.operator bool();
  }

  /// 运行Task
  MARL_NO_EXPORT inline void operator()() const {
    function_();
  }

  /// @return 如果创建Task时的Flags包含了flag则返回true，否则返回false
  [[nodiscard]] MARL_NO_EXPORT inline bool is(Flags flag) const {
    return (static_cast<int>(flags_) & static_cast<int>(flag)) ==
        static_cast<int>(flag);
  }

 private:
  Function function_;
  Flags flags_{Flags::None};
};

} // namespace marl

#endif //MINIMARL_INCLUDE_MARL_TASK_HPP_
