#ifndef MINIMARL_INCLUDE_MARL_EVENT_HPP_
#define MINIMARL_INCLUDE_MARL_EVENT_HPP_

#include "condition_variable.hpp"
#include "containers.hpp"
#include "export.hpp"
#include "memory.hpp"

namespace marl {

/// Event是一种同步原语，用于阻塞直到产生信号
class Event {
 public:
  enum class Mode : uint8_t {
    Auto,     ///< 在wait返回后，信号会自动重置，一个signal只能唤醒一个wait
    Manual,   ///< 需要通过clear()来重置
  };

  MARL_NO_EXPORT inline Event(Mode mode = Mode::Auto,
                              bool initial_state = false,
                              Allocator *allocator = Allocator::Default)
      : shared_(allocator->make_shared<Shared>(allocator, mode, initial_state)) {
  }

  /// 发出信号，可能会解除一个wait
  MARL_NO_EXPORT inline void signal() const {
    shared_->signal();
  }

  /// 清除信号状态
  MARL_NO_EXPORT inline void clear() const {
     marl::lock lock(shared_->mutex);
     shared_->signalled = false;
  }

  /// 阻塞，直到发出信号
  MARL_NO_EXPORT inline void wait() const {
    shared_->wait();
  }

  /// 阻塞，直到发出信号或者已经超时
  /// @return 如果是超时导致返回，则返回false，否则返回true
  template<typename Rep, typename Period>
  MARL_NO_EXPORT inline bool wait_for(const std::chrono::duration<Rep, Period> &duration) const {
    return shared_->wait_for(duration);
  }

  template<typename Clock, typename Duration>
  MARL_NO_EXPORT inline bool wait_until(const std::chrono::time_point<Clock, Duration> &timeout) const {
    return shared_->wait_until(timeout);
  }

  /// 如果信号已经发出，则返回true，否则返回false
  /// 如果是Auto模式，并且信号已经发出，则信号的发出状态会重置
  [[nodiscard]] MARL_NO_EXPORT inline bool test() const {
    marl::lock lock(shared_->mutex);
    if (!shared_->signalled) {
      return false;
    }
    if (shared_->mode == Mode::Auto) {
      shared_->signalled = false;
    }
    return true;
  }

  /// 如果信号已经发出，则返回true，否则返回false
  /// 与test不同，Auto模式时不会自动重置
  [[nodiscard]] MARL_NO_EXPORT inline bool isSignalled() const {
    marl::lock lock(shared_->mutex);
    return shared_->signalled;
  }

  /// 返回一个event，当列表中的任意一个event发出信号时该event会自动发出信号
  template<typename Iterator>
  MARL_NO_EXPORT inline static Event any(Mode mode,
                                         const Iterator &begin,
                                         const Iterator &end) {
    Event any(mode, false);
    for (auto it = begin; it != end; ++it) {
      auto s = it->shared_;
      marl::lock lock(s->mutex);
      if (s->signalled) {
        any.signal();
      }
      s->deps.push_back(any.shared_);
    }
    return any;
  }

  /// 返回一个event，当列表中的任意一个event发出信号时该event会自动发出信号
  /// @note 默认使用Auto模式
  template<typename Iterator>
  MARL_NO_EXPORT inline static Event any(const Iterator &begin,
                                         const Iterator &end) {
    return any(Mode::Auto, begin, end);
  }

 private:
  struct Shared {
    MARL_NO_EXPORT inline Shared(Allocator *allocator,
                                 Mode mode,
                                 bool initial_state)
        : cv(allocator),
          mode(mode),
          signalled(initial_state) {}
    MARL_NO_EXPORT inline void signal() {
      marl::lock lock(mutex);
      if (signalled) return;
      signalled = true;
      if (mode == Mode::Auto) {
        cv.notify_one();
      } else {
        cv.notify_all();
      }
      for (const auto &dep : deps) {
        dep->signal();
      }
    }
    MARL_NO_EXPORT inline void wait() {
      marl::lock lock(mutex);
      cv.wait(lock, [&] { return signalled; });
      if (mode == Mode::Auto) {
        signalled = false;
      }
    }

    template<typename Rep, typename Period>
    MARL_NO_EXPORT inline bool wait_for(const std::chrono::duration<Rep, Period> &duration) {
      marl::lock lock(mutex);
      if (!cv.wait_for(lock, duration, [&] { return signalled; })) {
        return false;
      }
      if (mode == Mode::Auto) {
        signalled = false;
      }
      return true;
    }

    template<typename Clock, typename Duration>
    MARL_NO_EXPORT inline bool wait_until(const std::chrono::time_point<Clock, Duration> &timeout) {
      marl::lock lock(mutex);
      if (!cv.wait_until(lock, timeout, [&] { return signalled; })) {
        return false;
      }
      if (mode == Mode::Auto) {
        signalled = false;
      }
      return true;
    }

    marl::mutex mutex;
    ConditionVariable cv;
    containers::vector<std::shared_ptr<Shared>, 1> deps;
    const Mode mode;
    bool signalled;
  };

  const std::shared_ptr<Shared> shared_;
};

} // namespace marl

#endif //MINIMARL_INCLUDE_MARL_EVENT_HPP_
