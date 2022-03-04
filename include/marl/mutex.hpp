#ifndef MINIMARL_INCLUDE_MARL_MUTEX_HPP_
#define MINIMARL_INCLUDE_MARL_MUTEX_HPP_

#include "export.hpp"
#include "tsa.hpp"

#include <condition_variable>
#include <mutex>

namespace marl {

/// 一个提供线程安全分析注解的封装std::mutex的互斥锁
class CAPABILITY("mutex") mutex {
 public:
  MARL_NO_EXPORT inline void lock() ACQUIRE() { _.lock(); }
  MARL_NO_EXPORT inline void unlock() RELEASE() { _.unlock(); }
  MARL_NO_EXPORT inline bool try_lock() TRY_ACQUIRE(true) {
    return _.try_lock();
  }

  /// 在已锁的mutex上调用cv.wait()
  template<typename Predicate>
  MARL_NO_EXPORT inline void wait_locked(std::condition_variable &cv,
                                         Predicate &&p) REQUIRES(this) {
    std::unique_lock<std::mutex> ul(_, std::adopt_lock);
    cv.wait(ul, std::forward<Predicate>(p));
    ul.release(); // Keep lock held
  }

  /// 在已锁的mutex上调用cv.wait_until()，返回wait_until的调用结果
  template<typename Predicate, typename Time>
  MARL_NO_EXPORT inline bool wait_until_locked(std::condition_variable &cv,
                                               Time &&time,
                                               Predicate &&p) REQUIRES(this) {
    std::unique_lock<std::mutex> ul(_, std::adopt_lock);
    auto res = cv.wait_until(ul, std::forward<Time>(time), std::forward<Predicate>(p));
    ul.release();
    return res; // Keep lock held
  }

 private:
  friend class lock;
  std::mutex _;
};

/// 一个提供线程安全分析注解的RAII的锁
class SCOPED_CAPABILITY lock {
 public:
  inline lock(mutex &m) ACQUIRE(m): _(m._) {}
  inline ~lock() RELEASE() {}

  /// 在锁上调用cv.wait()
  template<typename Predicate>
  inline void wait(std::condition_variable &cv, Predicate &&p) {
    cv.wait(_, std::forward<Predicate>(p));
  }

  /// 在锁上调用cv.wait_until()
  template<typename Predicate, typename Time>
  inline bool wait_until(std::condition_variable &cv,
                         Time &&time,
                         Predicate &&p) {
    return cv.wait_until(_, std::forward<Time>(time),
                         std::forward<Predicate>(p));
  }

  [[nodiscard]] inline bool owns_lock() const { return _.owns_lock(); }

  /// 在没有线程安全分析的情况下进行lock，谨慎使用
  inline void lock_no_tsa() { _.lock(); }

  /// 在没有线程安全分析的情况下进行unlock，谨慎使用
  inline void unlock_no_tsa() { _.unlock(); }

 private:
  std::unique_lock<std::mutex> _;
};

} // namespace marl

#endif //MINIMARL_INCLUDE_MARL_MUTEX_HPP_
