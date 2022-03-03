#include "marl/memory.hpp"

#include "marl/debug.hpp"

#include <cstring>

#include <sys/mman.h>
#include <unistd.h>

namespace {

const size_t kPageSize = sysconf(_SC_PAGESIZE);

inline size_t pageSize() {
  return kPageSize;
}

inline void *allocatePages(size_t count) {
  // mmap的用法可参考TLPI第49章
  auto mapping = mmap(nullptr, count * pageSize(), PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  MARL_ASSERT(mapping != MAP_FAILED, "Failed to allocate %d pages", int(count));
  if (mapping == MAP_FAILED) {
    mapping = nullptr;
  }
  return mapping;
}

inline void freePages(void *ptr, size_t count) {
  auto res = munmap(ptr, count * pageSize());
  (void) res;
  MARL_ASSERT(res == 0, "Failed to free %d pages", int(count));
}

inline void protectPage(void *addr) {
  auto res = mprotect(addr, pageSize(), PROT_NONE);
  (void) res;
  MARL_ASSERT(res == 0, "Failed to protect page at %p", addr);
}

} // anonymous namespace

namespace {

/// @brief 通过OS特定的mapping API, 以alignment为最小对齐方式，分配size个字节的未初始化内存
/// @param guard_low 保护分配内存地址的下界，对低于下界地址的读写会导致page fault
/// @param guard_high 保护分配内存地址的上界，对高于上界地址的读写会导致page fault
void *pagedMalloc(size_t alignment, size_t size,
                  bool guard_low, bool guard_high) {
  (void) alignment;
  MARL_ASSERT(alignment < pageSize(),
              "alignment (0x%x) must be less than the page size (0x%x)",
              int(alignment), int (pageSize()));
  auto num_requested_pages = (size + pageSize() - 1) / pageSize();
  auto num_total_pages =
      num_requested_pages + (guard_low ? 1 : 0) + (guard_high ? 1 : 0);
  auto mem = reinterpret_cast<uint8_t *>(allocatePages(num_total_pages));
  if (guard_low) {
    protectPage(mem);
    mem += pageSize();
  }
  if (guard_high) {
    protectPage(mem + num_requested_pages * pageSize());
  }
  return mem;
}

/// 释放通过pagedMalloc分配的内存
void pagedFree(void *ptr, size_t alignment, size_t size,
               bool guard_low, bool guard_high) {
  (void) alignment;
  MARL_ASSERT(alignment < pageSize(),
              "alignment (0x%x) must be less than the page size (0x%x)",
              int(alignment), int (pageSize()));
  auto num_requested_pages = (size + pageSize() - 1) / pageSize();
  auto num_total_pages =
      num_requested_pages + (guard_low ? 1 : 0) + (guard_high ? 1 : 0);
  if (guard_low) {
    ptr = reinterpret_cast<uint8_t *>(ptr) - pageSize();
  }
  freePages(ptr, num_total_pages);
}

/// 通过标准库中的API(malloc)，以alignment为最低对齐单位，分配size个字节的未初始化内存
inline void *alignedMalloc(size_t alignment, size_t size) {
  // sizeof(void *)是为指针预留的空间
  size_t alloc_size = size + alignment + sizeof(void *);
  auto allocation = malloc(alloc_size);
  auto aligned = reinterpret_cast<uint8_t *>(
      marl::alignUp(reinterpret_cast<uintptr_t>(allocation),
                    alignment));
  // 将allocation指针复制到分配内存的尾端
  memcpy(aligned + size, &allocation, sizeof(void *));
  return aligned;
}

inline void alignedFree(void *ptr, size_t size) {
  void *base;
  // 原版本使用的是sizeof(size_t)，我改成了sizeof(void *)
  memcpy(&base, reinterpret_cast<uint8_t *>(ptr) + size, sizeof(void *));
  free(base);
}

class DefaultAllocator : public marl::Allocator {
 public:
  static DefaultAllocator instance;

  marl::Allocation allocate(const marl::Allocation::Request &request) override {
    void *ptr = nullptr;
    if (request.use_guards) {
      ptr = ::pagedMalloc(request.alignment, request.size, true, true);
    } else if (request.alignment > 1U) {
      ptr = ::alignedMalloc(request.alignment, request.size);
    } else {
      ptr = ::malloc(request.size);
    }

    MARL_ASSERT(ptr != nullptr, "Allocation failed");
    MARL_ASSERT(reinterpret_cast<uintptr_t>(ptr) % request.alignment == 0,
                "Allocation gave incorrect alignment");

    marl::Allocation allocation;
    allocation.ptr = ptr;
    allocation.request = request;
    return allocation;
  }

  void free(const marl::Allocation &allocation) override {
    if (allocation.request.use_guards) {
      ::pagedFree(allocation.ptr, allocation.request.alignment, allocation.request.size, true, true);
    } else if (allocation.request.alignment > 1U) {
      ::alignedFree(allocation.ptr, allocation.request.size);
    } else {
      ::free(allocation.ptr);
    }
  }
};

DefaultAllocator DefaultAllocator::instance;

} // anonymous namespace

namespace marl {

Allocator *Allocator::Default = &DefaultAllocator::instance;

size_t pageSize() {
  return ::pageSize();
}

} // namespace marl
