#ifndef MINIMARL_INCLUDE_MARL_MEMORY_HPP_
#define MINIMARL_INCLUDE_MARL_MEMORY_HPP_

#include "debug.hpp"
#include "export.hpp"

#include <cstdlib>
#include <cstdint>
#include <memory>
#include <mutex>

namespace marl {

template<typename T>
struct StlAllocator;

/// 返回虚拟内存的页大小
MARL_EXPORT
size_t pageSize();

/// 将val向上对齐到alignment
template<typename T>
MARL_NO_EXPORT inline T alighUp(T val, T alignment) {
  return alignment * ((val + alignment - 1) / alignment);
}

template<size_t SIZE, size_t ALIGNMENT>
struct aligned_storage {
  struct alignas(ALIGNMENT) type {
    unsigned char data[SIZE];
  };
};

/// 保存Allocator的一次内存分配的结果
struct Allocation {
  /// allocation的用途，用于allocation trackers
  enum class Usage : uint8_t {
    Undefined = 0,
    Stack,    ///< Fiber stack
    Create,   ///< Allocator::create(), make_unique(), make_shared()
    Vector,   ///< marl::containers::vector<T>
    List,     ///< marl::containers::list<T>
    Stl,      ///< marl::StlAllocator
    Count,    ///< 没有实际含义，用作upper bound
  };

  /// 存储进行内存分配需要的参数信息
  struct Request {
    size_t size{0};                 ///< 分配空间的大小
    size_t alignment{0};            ///< 分配的最小对齐单位
    bool use_guards{false};         ///< 分配是否被guarded
    Usage usage{Usage::Undefined};  ///< allocation的用途
  };

  void *ptr = nullptr;  ///< 指向分配的内存
  Request request;      ///< allocation对应的Request
};

/// 内存分配器的接口, Allocator::Default是提供的默认实现
class Allocator {
 public:
  /// 默认的内存分配器
  MARL_EXPORT static Allocator *Default;

  /// 一个兼容智能指针的内存删除器，可用于删除由Allocator::create()分配的内存，也可以用作unique_ptr和shared_ptr的删除器
  struct MARL_EXPORT Deleter {
    MARL_NO_EXPORT inline Deleter() = default;
    MARL_NO_EXPORT inline Deleter(Allocator *allocator, size_t count)
        : allocator(allocator), count(count) {}

    template<typename T>
    MARL_NO_EXPORT inline void operator()(T *object);

    Allocator *allocator{nullptr};
    size_t count{0};
  };

  template<typename T>
  using unique_ptr = std::unique_ptr<T, Deleter>;

  virtual ~Allocator() = default;

  /// 从分配器中分配内存
  /// @note 返回的Allocation::request和入参的Request必须是相等的
  virtual Allocation allocate(const Allocation::Request &) = 0;

  /// 释放由allocate()分配的内存
  virtual void free(const Allocation &) = 0;

  /// 基于T的对齐方式，分配内存，并构造一个T类型的对象
  /// @note 返回的指针必须被destroy()释放
  template<typename T, typename... Args>
  inline T *create(Args &&...args);

  /// 销毁并释放由create()分配的对象
  template<typename T>
  inline void destroy(T *object);

  /// 基于T的对齐方式，分配一个T类型的对象，并以unique_ptr的形式返回
  template<typename T, typename... Args>
  inline unique_ptr<T> make_unique(Args &&...args);

  /// 基于T的对齐方式，分配一个T类型的数组，并以unique_ptr的形式返回
  template<typename T, typename... Args>
  inline unique_ptr<T> make_unique_n(size_t n, Args &&...args);

  /// 基于T的对齐方式，分配一个T类型的对象，并以shared_ptr的形式返回
  template<typename T, typename... Args>
  inline std::shared_ptr<T> make_shared(Args &&...args);

 protected:
  Allocator() = default;
};

template<typename T>
void Allocator::Deleter::operator()(T *object) {
  object->~T();

  Allocation allocation;
  allocation.ptr = object;
  allocation.request.size = sizeof(T) * count;
  allocation.request.usage = Allocation::Usage::Create;
  allocator->free(allocation);
}

template<typename T, typename... Args>
T *Allocator::create(Args &&...args) {
  Allocation::Request request;
  request.size = sizeof(T);
  request.alignment = alignof(T);
  request.usage = Allocation::Usage::Create;

  auto alloc = allocate(request);
  new(alloc.ptr) T(std::forward<Args>(args)...);
  return reinterpret_cast<T *>(alloc.ptr);
}

template<typename T>
void Allocator::destroy(T *object) {
  object->~T();

  Allocation alloc;
  alloc.ptr = object;
  alloc.request.size = sizeof(T);
  alloc.request.alignment = alignof(T);
  alloc.request.usage = Allocation::Usage::Create;
  free(alloc);
}

template<typename T, typename... Args>
Allocator::unique_ptr<T> Allocator::make_unique(Args &&...args) {
  return make_unique_n<T>(1, std::forward<Args>(args)...);
}

template<typename T, typename... Args>
Allocator::unique_ptr<T> Allocator::make_unique_n(size_t n, Args &&... args) {
  if (n == 0) {
    return nullptr;
  }

  Allocation::Request request;
  request.size = sizeof(T) * n;
  request.alignment = alignof(T);
  request.usage = Allocation::Usage::Create;

  auto alloc = allocate(request);
  new(alloc.ptr) T(std::forward<Args>(args)...);
  return unique_ptr<T>(reinterpret_cast<T *>(alloc.ptr), Deleter{this, n});
}

template<typename T, typename... Args>
std::shared_ptr<T> Allocator::make_shared(Args &&... args) {
  Allocation::Request request;
  request.size = sizeof(T);
  request.alignment = alignof(T);
  request.usage = Allocation::Usage::Create;

  auto alloc = allocate(request);
  new(alloc.ptr) T(std::forward<Args>(args)...);
  return std::shared_ptr<T>(reinterpret_cast<T *>(alloc.ptr), Deleter{this, 1});
}

/// 包装Allocator，以追踪所有的内存分配
class TrackedAllocator : public Allocator {
 public:
  struct UsageStats {
    /// 内存分配的总次数
    size_t count{0};
    /// 总共分配的字节数
    size_t bytes{0};
  };

  struct Stats {
    [[nodiscard]] inline size_t numAllocations() const;

    [[nodiscard]] inline size_t bytesAllocated() const;

    std::array<UsageStats, size_t(Allocation::Usage::Count)> by_usage;
  };

  inline explicit TrackedAllocator(Allocator *allocator)
      : allocator_(allocator) {}

  /// 返回当前allocator的分配数据
  inline Stats stats();

  inline Allocation allocate(const Allocation::Request &) override;
  inline void free(const Allocation &) override;

 private:
  Allocator *const allocator_;
  std::mutex mutex_;
  Stats stats_;
};

size_t TrackedAllocator::Stats::numAllocations() const {
  size_t out = 0;
  for (auto &stats : by_usage) {
    out += stats.count;
  }
  return out;
}

size_t TrackedAllocator::Stats::bytesAllocated() const {
  size_t out = 0;
  for (auto &stats : by_usage) {
    out += stats.bytes;
  }
  return out;
}

TrackedAllocator::Stats TrackedAllocator::stats() {
  // 原版采用std::unique_lock，我觉得没什么必要，所以改成更轻量的std::lock_guard，后面几个函数同
  std::lock_guard<std::mutex> lg(mutex_);
  return stats_;
}

Allocation TrackedAllocator::allocate(const Allocation::Request &request) {
  {
    std::lock_guard<std::mutex> lg(mutex_);
    auto &usage_stats = stats_.by_usage[int(request.usage)];
    ++usage_stats.count;
    usage_stats.bytes += request.size;
  }
  return allocator_->allocate(request);
}

void TrackedAllocator::free(const Allocation &allocation) {
  {
    std::lock_guard<std::mutex> lg(mutex_);
    auto &usage_stats = stats_.by_usage[int(allocation.request.usage)];
    MARL_ASSERT(usage_stats.count > 0,
                "TrackedAllocator detected abnormal free()");
    MARL_ASSERT(usage_stats.bytes >= allocation.request.size,
                "TrackedAllocator detected abnormal free()");
    --usage_stats.count;
    usage_stats.bytes -= allocation.request.size;
  }
  return allocator_->free(allocation);
}

/// 包装一个Allocator，提供兼容STL的API
template<typename T>
struct StlAllocator {
  using value_type = T;
  using pointer = T *;
  using const_pointer = const T *;
  using reference = T &;
  using const_reference = const T &;
  using size_type = size_t;
  using difference_type = size_t;

  /// 作为另一种类型的STL分配器
  template<typename U>
  struct rebind {
    typedef StlAllocator<U> other;
  };

  /// 通过allocator构造一个StlAllocator，allocator必须确保在StlAllocator被销毁前都是有效的
  inline explicit StlAllocator(Allocator *allocator)
      : allocator(allocator) {}

  template<typename U>
  inline explicit StlAllocator(const StlAllocator<U> &other)
      : allocator(other.allocator) {}

  /// 返回x的实际内存地址
  inline pointer address(reference x) const { return &x; }
  inline const_pointer address(const_reference x) const { return &x; }

  /// 分配n个T类型对象的内存，但不构造对象
  inline T *allocate(std::size_t n) {
    auto alloc = allocator->allocate(request(n));
    return reinterpret_cast<T *>(alloc.ptr);
  }

  /// 释放n个T类型对象的内存
  inline void deallocate(T *p, std::size_t n);

  /// 返回T对象理论上的最多个数
  [[nodiscard]] inline size_type max_size() const {
    return std::numeric_limits<size_type>::max() / sizeof(value_type);
  }

  /// 通过复制，在地址p处构造一个T类型的对象
  inline void constuct(pointer p, const_reference val) {
    new(p) T(val);
  }

  /// 在p处构造一个U类型的对象，并且转发参数给U的构造函数
  template<typename U, typename... Args>
  inline void construct(U *p, Args &&...args) {
    ::new((void *) p) U(std::forward<Args>(args)...);
  }

  /// 析构p位置的T类型对象，但并不释放内存
  inline void destroy(pointer p) { ((T *) p)->~T(); }

  /// 析构p位置的U类型对象，但并不释放内存
  template<typename U>
  inline void destroy(U *p) { p->~U(); }

 private:
  inline Allocation::Request request(size_t n);

  template<typename U>
  friend
  struct StlAllocator;

  Allocator *allocator;
};

template<typename T>
void StlAllocator<T>::deallocate(T *p, std::size_t n) {
  Allocation alloc;
  alloc.ptr = p;
  alloc.request = request(n);
  allocator->free(alloc);
}

template<typename T>
Allocation::Request StlAllocator<T>::request(size_t n) {
  Allocation::Request req{};
  req.size = sizeof(T) * n;
  req.alignment = alignof(T);
  req.usage = Allocation::Usage::Stl;
  return req;
}

} // namespace marl

#endif //MINIMARL_INCLUDE_MARL_MEMORY_HPP_
