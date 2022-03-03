#ifndef MINIMARL_INCLUDE_MARL_CONTAINERS_HPP_
#define MINIMARL_INCLUDE_MARL_CONTAINERS_HPP_

#include "debug.hpp"
#include "memory.hpp"

#include <algorithm>
#include <deque>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace marl::containers {

template<typename T>
using deque = std::deque<T, StlAllocator<T>>;

template<typename K, typename V, typename C = std::less<K>>
using map = std::map<K, V, C, StlAllocator<std::pair<const K, V>>>;

template<typename K, typename C = std::less<K>>
using set = std::set<K, C, StlAllocator<K>>;

template<typename K,
    typename V,
    typename H = std::hash<K>,
    typename E = std::equal_to<K>>
using unordered_map = std::unordered_map<K, V, H, E, StlAllocator<std::pair<const K, V>>>;

template<typename K, typename H = std::hash<K>, typename E = std::equal_to<K>>
using unordered_set = std::unordered_set<K, H, E, StlAllocator<K>>;

/// 弹出deque队首的值，并返回该值
template<typename T>
MARL_NO_EXPORT inline T take(deque<T> &deq) {
  auto out = std::move(deq.front());
  deq.pop_front();
  return out;
}

/// 弹出unordered_set队首的值，并返回
template<typename T, typename H, typename E>
MARL_NO_EXPORT inline T take(unordered_set<T, H, E> &set) {
  auto it = set.begin();
  auto out = std::move(*it);
  set.erase(it);
  return out;
}

/// 与std::vector不同，marl::containers::vector将capacity作为一个模板参数，并且保存在类内部，以避免动态内存分配
/// 一旦vector的内存超过了capacity，vector将会向heap申请分配内存
template<typename T, int BASE_CAPACITY>
class vector {
 public:
  MARL_NO_EXPORT inline explicit vector(Allocator *allocator = Allocator::Default)
      : allocator_(allocator) {}

  template<int BASE_CAPACITY_2>
  MARL_NO_EXPORT inline vector(const vector<T, BASE_CAPACITY_2> &other,
                               Allocator *allocator = Allocator::Default)
      : allocator_(allocator) {
    *this = other;
  }

  template<int BASE_CAPACITY_2>
  MARL_NO_EXPORT inline vector(vector<T, BASE_CAPACITY_2> &&other,
                               Allocator *allocator = Allocator::Default)
      : allocator_(allocator) {
    *this = std::move(other);
  }

  MARL_NO_EXPORT inline ~vector() { free(); }

  MARL_NO_EXPORT inline vector &operator=(const vector &other);

  template<int BASE_CAPACITY_2>
  MARL_NO_EXPORT inline vector<T, BASE_CAPACITY> &operator=(
      const vector<T, BASE_CAPACITY_2> &other) {
    free();
    reserve(other.size());
    count_ = other.size();
    for (size_t i = 0; i < count_; ++i) {
      new(&reinterpret_cast<T *>(elements_)[i]) T(other[i]);
    }
    return *this;
  }

  template<int BASE_CAPACITY_2>
  MARL_NO_EXPORT inline vector<T, BASE_CAPACITY> &operator=(
      const vector<T, BASE_CAPACITY_2> &&other) {
    free();
    reserve(other.size());
    count_ = other.size();
    for (size_t i = 0; i < count_; ++i) {
      new(&reinterpret_cast<T *>(elements_)[i]) T(std::move(other[i]));
    }
    other.resize(0);
    return *this;
  }

  MARL_NO_EXPORT inline void push_back(const T &elem) {
    reserve(count_ + 1);
    new(&reinterpret_cast<T *>(elements_)[count_]) T(elem);
    ++count_;
  }
  // 原版函数名叫做emplace_back，为了和std::vector保持一致，改名为push_back
  MARL_NO_EXPORT inline void push_back(T &&elem) {
    reserve(count_ + 1);
    new(&reinterpret_cast<T *>(elements_)[count_]) T(std::move(elem));
    ++count_;
  }
  MARL_NO_EXPORT inline void pop_back() {
    MARL_ASSERT(count_ > 0, "pop_back() called on empty vector");
    --count_;
    reinterpret_cast<T *>(elements_)[count_].~T();
  }
  MARL_NO_EXPORT inline T &front() {
    MARL_ASSERT(count_ > 0, "front() called on empty vector");
    return reinterpret_cast<T *>(elements_)[0];
  }
  MARL_NO_EXPORT inline const T &front() const {
    MARL_ASSERT(count_ > 0, "front() called on empty vector");
    return reinterpret_cast<T *>(elements_)[0];
  }
  MARL_NO_EXPORT inline T &back() {
    MARL_ASSERT(count_ > 0, "back() called on empty vector");
    return reinterpret_cast<T *>(elements_)[count_ - 1];
  }
  MARL_NO_EXPORT inline const T &back() const {
    MARL_ASSERT(count_ > 0, "back() called on empty vector");
    return reinterpret_cast<T *>(elements_)[count_ - 1];
  }
  MARL_NO_EXPORT inline T *begin() {
    return reinterpret_cast<T *>(elements_);
  }
  MARL_NO_EXPORT inline const T *begin() const {
    return reinterpret_cast<T *>(elements_);
  }
  MARL_NO_EXPORT inline T *end() {
    return reinterpret_cast<T *>(elements_) + count_;
  }
  MARL_NO_EXPORT inline const T *end() const {
    return reinterpret_cast<T *>(elements_) + count_;
  }
  MARL_NO_EXPORT inline T &operator[](size_t i) {
    MARL_ASSERT(i < count_, "index %d exceeds vector size %d", int(i), int(count_));
    return reinterpret_cast<T *>(elements_)[i];
  }
  MARL_NO_EXPORT inline const T &operator[](size_t i) const {
    MARL_ASSERT(i < count_, "index %d exceeds vector size %d", int(i), int(count_));
    return reinterpret_cast<T *>(elements_)[i];
  }
  [[nodiscard]] MARL_NO_EXPORT inline size_t size() const {
    return count_;
  }
  [[nodiscard]] MARL_NO_EXPORT inline size_t capacity() const {
    return capacity_;
  }
  MARL_NO_EXPORT inline void resize(size_t n) {
    reserve(n);
    while (count_ < n) {
      new(&reinterpret_cast<T *>(elements_)[count_]) T();
      ++count_;
    }
    while (n < count_) {
      --count_;
      reinterpret_cast<T *>(elements_)[count_].~T();
    }
  }
  MARL_NO_EXPORT inline void reserve(size_t n) {
    if (n > capacity_) {
      capacity_ = std::max<size_t>(n * 2, 8);
      Allocation::Request request;
      request.size = sizeof(T) * capacity_;
      request.alignment = alignof(T);
      request.usage = Allocation::Usage::Vector;

      auto alloc = allocator_->allocate(request);
      auto grown = reinterpret_cast<TStorage>(alloc.ptr);
      for (size_t i = 0; i < count_; ++i) {
        new(&reinterpret_cast<T *>(grown)[i])
            T(std::move(reinterpret_cast<T *>(elements_)[i]));
      }
      free();
      elements_ = grown;
      allocation_ = alloc;
    }
  }
  MARL_NO_EXPORT inline T *data() { return elements_; }
  MARL_NO_EXPORT inline const T *data() const { return elements_; }

  Allocator *const allocator_;

 private:
  using TStorage = typename marl::aligned_storage<sizeof(T), alignof(T)>::type;

  vector(const vector &) = delete;

  MARL_NO_EXPORT inline void free() {
    for (size_t i = 0; i < count_; ++i) {
      reinterpret_cast<T *>(elements_)[i].~T();
    }
    if (allocation_.ptr != nullptr) {
      allocator_->free(allocation_);
      allocation_ = {};
      elements_ = nullptr;
    }
  }

  size_t count_{0};
  size_t capacity_{BASE_CAPACITY};
  TStorage buffer_[BASE_CAPACITY];
  TStorage *elements_ = buffer_;
  Allocation allocation_;
};

template<typename T>
class list {
  struct Entry {
    T data;
    Entry *next;
    Entry *prev;
  };

 public:
  class iterator {
   public:
    MARL_NO_EXPORT inline iterator(Entry *entry)
        : entry_(entry) {}
    MARL_NO_EXPORT inline T *operator->() {
      return &entry_->data;
    }
    MARL_NO_EXPORT inline T &operator*() {
      return entry_->data;
    }
    MARL_NO_EXPORT inline iterator &operator++() {
      entry_ = entry_->next;
    }
    MARL_NO_EXPORT inline bool operator==(const iterator &rhs) {
      return entry_ == rhs.entry_;
    }
    MARL_NO_EXPORT inline bool operator!=(const iterator &rhs) {
      return entry_ != rhs.entry_;
    }

   private:
    friend list;
    Entry *entry_;
  };

  MARL_NO_EXPORT inline list(Allocator *allocator = Allocator::Default)
      : allocator_(allocator) {}
  MARL_NO_EXPORT inline ~list() {
    for (auto elem = head; elem != nullptr; ++elem) {
      elem->data.~T();
    }

    auto cur = allocations_;
    while (cur != nullptr) {
      auto next = cur->next;
      allocator_->free(cur->allocation);
      cur = next;
    }
  }

  MARL_NO_EXPORT inline iterator begin() { return {head}; }
  MARL_NO_EXPORT inline iterator end() { return {nullptr}; }
  [[nodiscard]] MARL_NO_EXPORT inline size_t size() const { return size_; }

  template<typename... Args>
  MARL_NO_EXPORT inline iterator emplace_front(Args &&...args) {
    if (free == nullptr) {
      grow(std::max<size_t>(capacity_, 8));
    }
    auto entry = free;
    unlink(entry, free);
    link(entry, head);
    new(&entry->data) T(std::forward<Args>(args)...);
    ++size_;
    return entry;
  }
  MARL_NO_EXPORT inline void erase(iterator it) {
    auto entry = it.entry_;
    unlink(entry, head);
    link(entry, free);
    entry->data.~T();
    --size_;
  }

 private:
  // 暂不支持复制和移动操作
  list(const list &) = delete;
  list(list &&) = delete;
  list &operator=(const list &) = delete;
  list &operator=(list &&) = delete;

  struct AllocationChain {
    Allocation allocation;
    AllocationChain *next;
  };

  MARL_NO_EXPORT inline void grow(size_t count);

  MARL_NO_EXPORT static inline void unlink(Entry *entry, Entry *&_list) {
    if (_list == entry) {
      _list = _list->next;
    }
    if (entry->prev) {
      entry->prev->next = entry->next;
    }
    if (entry->next) {
      entry->next->prev = entry->prev;
    }
    entry->prev = nullptr;
    entry->next = nullptr;
  }
  MARL_NO_EXPORT static inline void link(Entry *entry, Entry *&_list) {
    MARL_ASSERT(entry->next == nullptr, "link() called on entry already linked");
    MARL_ASSERT(entry->prev == nullptr, "link() called on entry already linked");
    if (_list) {
      entry->next = _list;
      _list->prev = entry;
    }
    _list = entry;
  }

  Allocator *const allocator_;
  size_t size_{0};
  size_t capacity_{0};
  AllocationChain *allocations_{nullptr};
  Entry *free{nullptr}; ///< 空闲节点链表
  Entry *head{nullptr}; ///< 正在使用的节点链表
};

template<typename T>
void list<T>::grow(size_t count) {
  const auto entries_size = sizeof(Entry) * count;
  const auto alloc_chain_offset = alignUp(entries_size, alignof(AllocationChain));
  const auto alloc_size = alloc_chain_offset + sizeof(AllocationChain);

  Allocation::Request request;
  request.size = alloc_size;
  request.alignment = std::max(alignof(Entry), alignof(AllocationChain));
  request.usage = Allocation::Usage::List;
  auto alloc = allocator_->allocate(request);

  auto entries = reinterpret_cast<Entry *>(alloc.ptr);
  for (size_t i = 0; i < count; ++i) {
    auto entry = &entries[i];
    entry->prev = nullptr;
    entry->next = free;
    if (free) {
      free->prev = entry;
    }
    free = entry;
  }

  auto alloc_chain = reinterpret_cast<AllocationChain *>(
      reinterpret_cast<uint8_t *>(alloc.ptr) + alloc_chain_offset);
  alloc_chain->allocation = alloc;
  alloc_chain->next = allocations_;
  allocations_ = alloc_chain;

  capacity_ += count;
}

} // namespace marl::containers

#endif //MINIMARL_INCLUDE_MARL_CONTAINERS_HPP_
