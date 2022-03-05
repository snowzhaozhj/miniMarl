#ifndef MINIMARL_INCLUDE_MARL_TRACE_HPP_
#define MINIMARL_INCLUDE_MARL_TRACE_HPP_

#define MARL_TRACE_ENABLED 0

#if MARL_TRACE_ENABLED

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdarg>
#include <string>
#include <mutex>
#include <ostream>
#include <memory>
#include <queue>
#include <thread>

namespace marl {

/// 写入一个trace event文件，可以用chrome://tracing来分析
class Trace {
 public:
  static constexpr size_t MaxEventNameLength = 64;

  static Trace *get();

  void nameThread(const char *fmt, ...);
  void beginEvent(const char *fmt, ...);
  void endEvent();
  void beginAsyncEvent(uint32_t id, const char *fmt, ...);
  void endAsyncEvent(uint32_t id, const char *fmt, ...);

  class ScopedEvent {
   public:
    inline ScopedEvent(const char *fmt, ...) : trace_(Trace::get()) {
      if (trace_ != nullptr) {
        char name[Trace::MaxEventNameLength];
        va_list vararg;
        va_start(vararg, fmt);
        vsnprintf(name, Trace::MaxEventNameLength, fmt, vararg);
        va_end(vararg);

        trace_->beginEvent(name);
      }
    }
    inline ~ScopedEvent() {
      if (trace_ != nullptr) {
        trace_->endEvent();
      }
    }

   private:
    Trace *const trace_;
  };

  class ScopedAsyncEvent {
   public:
    inline ScopedAsyncEvent(uint32_t id, const char *fmt, ...)
        : trace_(Trace::get()), id_(id) {
      if (trace_ != nullptr) {
        char buf[Trace::MaxEventNameLength];
        va_list vararg;
        va_start(vararg, fmt);
        vsnprintf(buf, Trace::MaxEventNameLength, fmt, vararg);
        va_end(vararg);
        name_ = buf;

        trace_->beginAsyncEvent(id, "%s", buf);
      }
    }
    inline ~ScopedAsyncEvent() {
      if (trace_ != nullptr) {
        trace_->endAsyncEvent(id_, "%s", name_.c_str());
      }
    }

   private:
    Trace *const trace_;
    const uint32_t id_;
    std::string name_;
  };

 private:
  Trace();
  ~Trace();
  Trace(const Trace &) = delete;
  Trace &operator=(const Trace &) = delete;

  struct Event {
    enum class Type : uint8_t {
      Begin = 'B',
      End = 'E',
      Complete = 'X',
      Instant = 'i',
      Counter = 'C',
      AsyncStart = 'b',
      AsyncInstant = 'n',
      AsyncEnd = 'e',
      FlowStart = 's',
      FlowStep = 't',
      FlowEnd = 'f',
      Sample = 'P',
      ObjectCreated = 'N',
      ObjectSnapshot = 'O',
      ObjectDestroyed = 'D',
      Metadata = 'M',
      GlobalMemoryDump = 'V',
      ProcessMemoryDump = 'v',
      Mark = 'R',
      ClockSync = 'c',
      ContextEnter = '(',
      ContextLeave = ')',

      // Internal types
      Shutdown = 'S',
    };

    Event();
    virtual ~Event() = default;
    [[nodiscard]] virtual Type type() const = 0;
    virtual void write(std::ostream &out) const;

    char name[MaxEventNameLength]{};
    const char **categories{nullptr};
    uint64_t timestamp_{0};
    uint32_t process_id_{0};
    uint32_t thread_id_;
    uint32_t fiber_id_;
  };

  struct BeginEvent : public Event {
    [[nodiscard]] Type type() const override { return Type::Begin; }
  };
  struct EndEvent : public Event {
    [[nodiscard]] Type type() const override { return Event::Type::End; }
  };
  struct MetadataEvent : public Event {
    [[nodiscard]] Type type() const override { return Type::Metadata; }
  };
  struct Shutdown : public Event {
    [[nodiscard]] Type type() const override { return Type::Shutdown; }
  };
  struct AsyncEvent : public Event {
    void write(std::ostream &out) const override;
    uint32_t id;
  };
  struct AsyncStartEvent : public AsyncEvent {
    [[nodiscard]] Type type() const override { return Type::AsyncStart; }
  };
  struct AsyncEndEvent : public AsyncEvent {
    Type type() const override { return Type::AsyncEnd; }
  };

  struct NameThreadEvent : public MetadataEvent {
    void write(std::ostream &out) const override;
  };

  /// @note 以ms为单位
  uint64_t timestamp();

  void put(Event *);
  std::unique_ptr<Event> take();

  struct EventQueue {
    std::queue<std::unique_ptr<Event>> data;
    std::condition_variable condition;
    std::mutex mutex;
  };
  std::array<EventQueue, 1> event_queues_;
  std::atomic<unsigned int> event_queue_write_index_{0};
  unsigned int event_queue_read_index_{0};
  std::chrono::time_point<std::chrono::high_resolution_clock> created_at
      {std::chrono::high_resolution_clock::now()};
  std::thread thread_;
  std::atomic<bool> stopped_{false};
};

} // namespace marl

#define MARL_CONCAT_(a, b) a##b
#define MARL_CONCAT(a, b) MARL_CONCAT_(a, b)

#define MARL_SCOPED_EVENT(...) \
  marl::Trace::ScopedEvent MARL_CONCAT(scoped_event, __LINE__)(__VA_ARGS__);
#define MARL_BEGIN_ASYNC_EVENT(id, ...)    \
  do {                                     \
    if (auto t = marl::Trace::get()) {     \
      t->beginAsyncEvent(id, __VA_ARGS__); \
    }                                      \
  } while (false);
#define MARL_END_ASYNC_EVENT(id, ...)    \
  do {                                   \
    if (auto t = marl::Trace::get()) {   \
      t->endAsyncEvent(id, __VA_ARGS__); \
    }                                    \
  } while (false);
#define MARL_SCOPED_ASYNC_EVENT(id, ...) \
  marl::Trace::ScopedAsyncEvent MARL_CONCAT(defer_, __LINE__)(id, __VA_ARGS__);
#define MARL_NAME_THREAD(...)          \
  do {                                 \
    if (auto t = marl::Trace::get()) { \
      t->nameThread(__VA_ARGS__);      \
    }                                  \
  } while (false);

#else

#define MARL_SCOPED_EVENT(...)
#define MARL_BEGIN_ASYNC_EVENT(id, ...)
#define MARL_END_ASYNC_EVENT(id, ...)
#define MARL_SCOPED_ASYNC_EVENT(id, ...)
#define MARL_NAME_THREAD(...)

#endif

#endif //MINIMARL_INCLUDE_MARL_TRACE_HPP_
