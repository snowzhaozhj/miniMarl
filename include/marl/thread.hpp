#ifndef MINIMARL_INCLUDE_MARL_THREAD_HPP_
#define MINIMARL_INCLUDE_MARL_THREAD_HPP_

#include "containers.hpp"
#include "export.hpp"

#include <functional>

namespace marl {

/// Thread类提供了执行线程的OS抽象
class Thread {
 public:
  using Func = std::function<void()>;

  struct Core {
    struct Windows {
      uint8_t group;  ///< group number
      uint8_t index;  ///< 处理器组中的核
    };
    struct Pthread {
      uint16_t index; ///< Core number
    };
    union {
      Windows windows;
      Pthread pthread;
    };

    MARL_NO_EXPORT inline bool operator==(const Core &other) const {
      return pthread.index == other.pthread.index;
    }
    MARL_NO_EXPORT inline bool operator<(const Core &other) const {
      return pthread.index < other.pthread.index;
    }
  };

  /// 线程亲和性
  struct Affinity {
    static constexpr bool supported = true; ///< linux系统是受支持的

    /// 提供了一个get方法，用于根据指定线程的id来获取线程亲和性
    class Policy {
     public:
      virtual ~Policy() = default;

      /// 返回一个Policy，该Policy可以通过get()得到一个affinity,该affinity包含了传入affinity的所有核
      MARL_EXPORT static std::shared_ptr<Policy> anyOf(
          Affinity &&affinity,
          Allocator *allocator = Allocator::Default);

      /// 返回一个Policy，该Policy可以通过get()来获取一个affinity，该affinity包含了传入affinity的一个核
      /// 选区的核为：affinity[thread_id % affinity.count()]
      MARL_EXPORT static std::shared_ptr<Policy> oneOf(
          Affinity &&affinity,
          Allocator *allocator = Allocator::Default);

      /// 根据线程id，返回指定线程的亲和性
      MARL_EXPORT virtual Affinity get(uint32_t thread_id,
                                       Allocator *allocator) const = 0;
    };

    /// 返回一个包含所有核的亲和性
    MARL_EXPORT static Affinity all(Allocator* allocator = Allocator::Default);

    MARL_EXPORT Affinity(Allocator *allocator);

    MARL_EXPORT Affinity(Affinity &&other);

    MARL_EXPORT Affinity(const Affinity &other, Allocator *allocator);

    MARL_EXPORT Affinity(std::initializer_list<Core> core_list, Allocator *allocator);

    MARL_EXPORT Affinity(const containers::vector<Core, 32> &core_vec, Allocator *allocator);

    /// 返回亲和性中包含的核数
    MARL_EXPORT size_t count() const;

    /// 返回第index个亲和的核
    MARL_EXPORT Core operator[](size_t index) const;

    /// 将other中包含的核添加到当前affinity中
    MARL_EXPORT Affinity &add(const Affinity &other);

    // 将affinity中包含的核从当前affinity中删去
    MARL_EXPORT Affinity &remove(const Affinity &affinity);

   private:
    Affinity(const Affinity &) = delete;

    containers::vector<Core, 32> cores;
  };

  MARL_EXPORT Thread() = default;

  MARL_EXPORT Thread(Thread &&);

  MARL_EXPORT Thread &operator=(Thread &&);

  MARL_EXPORT Thread(Affinity &&affinity, Func &&func);

  MARL_EXPORT ~Thread();

  /// 阻塞直到线程结束
  MARL_EXPORT void join();

  /// 为当前正在运行的线程设置一个名字，以在调试器中显示
  MARL_EXPORT static void setName(const char *fmt, ...);

  MARL_EXPORT static unsigned int numLogicalCPUs();

 private:
  Thread(const Thread &) = delete;
  Thread &operator=(const Thread &) = delete;

  class Impl;
  Impl *impl = nullptr;
};

} // namespace marl

#endif //MINIMARL_INCLUDE_MARL_THREAD_HPP_
