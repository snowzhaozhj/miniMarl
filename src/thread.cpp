#include "marl/thread.hpp"

#include "marl/debug.hpp"
#include "marl/defer.hpp"
#include "marl/trace.hpp"

#include <algorithm>
#include <cstdarg>
#include <cstdio>

#include <pthread.h>
#include <unistd.h>
#include <thread>

namespace {

struct CoreHasher {
  inline uint64_t operator()(const marl::Thread::Core &core) const {
    return core.pthread.index;
  }
};

} // anonymous namespace

namespace marl {

Thread::Affinity::Affinity(Allocator *allocator) : cores(allocator) {}
Thread::Affinity::Affinity(Affinity &&other) : cores(std::move(other.cores)) {}
Thread::Affinity::Affinity(const Affinity &other, Allocator *allocator)
    : cores(other.cores, allocator) {}
Thread::Affinity::Affinity(std::initializer_list<Core> core_list, Allocator *allocator)
    : cores(allocator) {
  cores.reserve(core_list.size());
  for (auto core : core_list) {
    cores.push_back(core);
  }
}
Thread::Affinity::Affinity(const containers::vector<Core, 32> &core_vec, Allocator *allocator)
    : cores(core_vec, allocator) {}

Thread::Affinity Thread::Affinity::all(Allocator *allocator) {
  Thread::Affinity affinity(allocator);
  auto thread = pthread_self();
  cpu_set_t cpu_set;
  CPU_ZERO(&cpu_set);
  if (pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpu_set) == 0) {
    int count = CPU_COUNT(&cpu_set);
    for (int i = 0; i < count; ++i) {
      Core core{};
      core.pthread.index = static_cast<uint16_t>(i);
      // 原版本中下方使用了std::move，但是Core本身是一个简单的struct，拥有trivial的复制构造函数，所以没必要使用移动语义
      affinity.cores.push_back(core);
    }
  }
  return affinity;
}

std::shared_ptr<Thread::Affinity::Policy> Thread::Affinity::Policy::anyOf(Affinity &&affinity, Allocator *allocator) {
  struct Policy : public Thread::Affinity::Policy {
    Affinity affinity;
    Policy(Affinity &&affinity) : affinity(std::move(affinity)) {}
    Affinity get(uint32_t thread_id, Allocator *allocator) const override {
      return Affinity(affinity, allocator);
    }
  };

  return allocator->make_shared<Policy>(std::move(affinity));
}

std::shared_ptr<Thread::Affinity::Policy> Thread::Affinity::Policy::oneOf(Affinity &&affinity, Allocator *allocator) {
  struct Policy : public Thread::Affinity::Policy {
    Affinity affinity;
    Policy(Affinity &&affinity) : affinity(std::move(affinity)) {}
    Affinity get(uint32_t thread_id, Allocator *allocator) const override {
      auto count = affinity.count();
      if (count == 0) {
        return Affinity(affinity, allocator);
      }
      return Affinity({affinity[thread_id % affinity.count()]}, allocator);
    }
  };

  return allocator->make_shared<Policy>(std::move(affinity));
}

size_t Thread::Affinity::count() const {
  return cores.size();
}

Thread::Core Thread::Affinity::operator[](size_t index) const {
  return cores[index];
}

Thread::Affinity &Thread::Affinity::add(const Affinity &other) {
  containers::unordered_set<Core, CoreHasher> set(cores.allocator_);
  for (auto core : cores) {
    set.emplace(core);
  }
  for (auto core : other.cores) {
    if (set.count(core) == 0) {
      cores.push_back(core);
    }
  }
  std::sort(cores.begin(), cores.end());
  return *this;
}

Thread::Affinity &Thread::Affinity::remove(const Affinity &other) {
  containers::unordered_set<Core, CoreHasher> set(cores.allocator_);
  for (auto core : other.cores) {
    set.emplace(core);
  }
  for (size_t i = 0; i < cores.size(); ++i) {
    if (set.count(cores[i]) != 0) {
      cores[i] = cores.back();
      cores.resize(cores.size() - 1);
    }
  }
  std::sort(cores.begin(), cores.end());
  return *this;
}

class Thread::Impl {
 public:
  Impl(Affinity &&affinity, Thread::Func &&f)
      : affinity_(std::move(affinity)), func_(std::move(f)), thread_([this] {
    setAffinity();
    func_();
  }) {}

  Affinity affinity_;
  Func func_;
  std::thread thread_;

  void setAffinity() {
    auto count = affinity_.count();
    if (count == 0) {
      return;
    }
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    for (size_t i = 0; i < count; ++i) {
      CPU_SET(affinity_[i].pthread.index, &cpu_set);
    }
    auto thread_id = pthread_self();
    pthread_setaffinity_np(thread_id, sizeof(cpu_set_t), &cpu_set);
  }
};

Thread::Thread(Affinity &&affinity, Func &&func)
    : impl(new Thread::Impl(std::move(affinity), std::move(func))) {}

Thread::Thread(Thread &&other) : impl(other.impl) {
  other.impl = nullptr;
}

Thread &Thread::operator=(Thread &&rhs) {
  if (impl) {
    delete impl;
    impl = nullptr;
  }
  impl = rhs.impl;
  rhs.impl = nullptr;
  return *this;
}

Thread::~Thread() {
  MARL_ASSERT(!impl, "Thread::join() was not called before destruction");
}

void Thread::join() {
  impl->thread_.join();
  delete impl;
  impl = nullptr;
}

void Thread::setName(const char *fmt, ...) {
  char name[1024];
  va_list vararg;
  va_start(vararg, fmt);
  vsnprintf(name, sizeof(name), fmt, vararg);
  va_end(vararg);
  pthread_setname_np(pthread_self(), name);

  MARL_NAME_THREAD("%s", name);
}

unsigned int Thread::numLogicalCPUs() {
  return static_cast<unsigned int>(sysconf(_SC_NPROCESSORS_ONLN));
}

} // namspace marl
