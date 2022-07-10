# marl

## 协程的实现

### 汇编实现

以x86-64为例，我们需要实现对以下寄存器的保护：

```cpp
struct marl_fiber_context {
  // 被调用者保护寄存器
  uintptr_t RBX;
  uintptr_t RBP;
  uintptr_t R12;
  uintptr_t R13;
  uintptr_t R14;
  uintptr_t R15;

  // 参数寄存器
  uintptr_t RDI;
  uintptr_t RSI;

  // 栈寄存器和指令寄存器
  uintptr_t RSP;
  uintptr_t RIP;
};
```

接着我们可以基于上述的context实现marl_swap_context函数：

```assembly
// void marl_fiber_swap(marl_fiber_context *from, const marl_fiber_context *to)
// rdi: from
// rsi: to
.text
.global MARL_ASM_SYMBOL(marl_fiber_swap)
.align 4
MARL_ASM_SYMBOL(marl_fiber_swap):
  // 保存上下文from
  // 保存被调用者保护的寄存器
  movq %rbx, MARL_REG_RBX(%rdi)
  movq %rbp, MARL_REG_RBP(%rdi)
  movq %r12, MARL_REG_R12(%rdi)
  movq %r13, MARL_REG_R13(%rdi)
  movq %r14, MARL_REG_R14(%rdi)
  movq %r15, MARL_REG_R15(%rdi)
  // 保存IP和SP
  movq (%rsp), %rcx             // call指令(调用marl_fiber_swap时的call指令)在栈上已经保存好了返回地址
  movq %rcx, MARL_REG_RIP(%rdi)
  leaq 8(%rsp), %rcx            // 略过call指令保存好的地址(栈从高地址向地址增长)
  movq %rcx, MARL_REG_RSP(%rdi)

  // 加载上下文to
  movq %rsi, %r8
  // 加载被调用者保护寄存器
  movq MARL_REG_RBX(%r8), %rbx
  movq MARL_REG_RBP(%r8), %rbp
  movq MARL_REG_R12(%r8), %r12
  movq MARL_REG_R13(%r8), %r13
  movq MARL_REG_R14(%r8), %r14
  movq MARL_REG_R15(%r8), %r15
  // 加载两个调用参数
  movq MARL_REG_RDI(%r8), %rdi
  movq MARL_REG_RSI(%r8), %rsi
  // 加载栈指针
  movq MARL_REG_RSP(%r8), %rsp
  // 加载指令计数器RIP, 并且进行跳转
  movq MARL_REG_RIP(%r8), %rcx
  jmp *%rcx
```

接着实现OSFiber

```cpp
class OSFiber {
 public:
  inline OSFiber(Allocator *allocator) : allocator_(allocator) {}
  inline ~OSFiber() {
    if (stack_.ptr != nullptr) {
      allocator_->free(stack_);
    }
  }

  /// 在当前线程上创建一个Fiber
  MARL_NO_EXPORT static inline Allocator::unique_ptr<OSFiber> createFiberFromCurrentThread(
      Allocator *allocator) {
    auto out = allocator->make_unique<OSFiber>(allocator);
    out->context_ = {};
    return out;
  }

  /// 创建一个栈大小为stack_size的fiber, 切换到该fiber后，将会运行func函数
  /// @note func函数的末尾禁止返回，而必须切换到另一个fiber
  MARL_NO_EXPORT static inline Allocator::unique_ptr<OSFiber> createFiber(
      Allocator *allocator,
      size_t stack_size,
      const std::function<void()> &func) {
    Allocation::Request request;
    request.size = stack_size;
    request.alignment = 16;
    request.usage = Allocation::Usage::Stack;
#if MARL_USE_FIBER_STACK_GUARDS
    request.use_guards = true;
#endif

    auto out = allocator->make_unique<OSFiber>(allocator);
    out->context_ = {};
    out->target_ = func;
    out->stack_ = allocator->allocate(request);
    marl_fiber_set_target(&out->context_,
                          out->stack_.ptr,
                          static_cast<uint32_t>(stack_size),
                          reinterpret_cast<void (*)(void *)>(&OSFiber::run),
                          out.get());
    return out;
  }

  /// 切换到另一个fiber
  /// @note 必须在当前正在运行的fiber中调用
  MARL_NO_EXPORT inline void switchTo(OSFiber *fiber) {
    marl_fiber_swap(&context_, &fiber->context_);
  }

 private:
  MARL_NO_EXPORT
  static inline void run(OSFiber *self) {
    self->target_();
  }

  Allocator *allocator_;
  marl_fiber_context context_;
  std::function<void()> target_;
  Allocation stack_;
};
```

### ucontext实现

```cpp
class OSFiber {
 public:
  inline OSFiber(Allocator *allocator) : allocator_(allocator) {}
  inline ~OSFiber() {
    if (stack_.ptr != nullptr) {
      allocator_->free(stack_);
    }
  }

  /// 在当前线程上创建一个Fiber
  MARL_NO_EXPORT static inline Allocator::unique_ptr<OSFiber> createFiberFromCurrentThread(
      Allocator *allocator) {
    auto out = allocator->make_unique<OSFiber>(allocator);
    out->context_ = {};
    getcontext(&out->context_);
    return out;
  }

  /// 创建一个栈大小为stack_size的fiber, 切换到该fiber后，将会运行func函数
  /// @note func函数的末尾禁止返回，而必须切换到另一个fiber
  MARL_NO_EXPORT static inline Allocator::unique_ptr<OSFiber> createFiber(
      Allocator *allocator,
      size_t stack_size,
      const std::function<void()> &func) {
    // makecontext只接受整形参数
    union Args {
      OSFiber *self;
      struct {
        int a;
        int b;
      };
    };

    struct Target {
      static void Main(int a, int b) {
        Args u;
        u.a = a;
        u.b = b;
        u.self->target_();
      }
    };

    Allocation::Request request;
    request.size = stack_size;
    request.alignment = 16;
    request.usage = Allocation::Usage::Stack;
#if MARL_USE_FIBER_STACK_GUARDS
    request.use_guards = true;
#endif

    auto out = allocator->make_unique<OSFiber>(allocator);
    out->context_ = {};
    out->target_ = func;
    out->stack_ = allocator->allocate(request);

    auto res = getcontext(&out->context_);
    (void) res;
    MARL_ASSERT(res == 0, "getcontext() returned %d", int(res));
    out->context_.uc_stack.ss_sp = out->stack_.ptr;
    out->context_.uc_stack.ss_size = stack_size;
    out->context_.uc_link = nullptr;

    Args args{};
    args.self = out.get();
    makecontext(&out->context_, reinterpret_cast<void (*)()>(&Target::Main), 2, args.a, args.b);
    return out;
  }

  /// 切换到另一个fiber
  /// @note 必须在当前正在运行的fiber中调用
  MARL_NO_EXPORT inline void switchTo(OSFiber *fiber) {
    auto res = swapcontext(&context_, &fiber->context_);
    (void) res;
    MARL_ASSERT(res == 0, "swapcontext() returned %d", int(res));
  }

 private:
  Allocator *allocator_;
  ucontext_t context_;
  std::function<void()> target_;
  Allocation stack_;
};
```

## 调度器实现

### Worker

一个Scheduler可以包含多个Worker，每个Worker对应一个线程。

Worker有两种模式：MultiThreaded，SingleThreaded

两种模式的区别：MultiThreaded模式的Worker是拥有一个后台线程去执行任务的，而SingleThreaded模式的Worker只会在当前线程yield()的之后才会去执行任务。

调用了Scheduler::bind()的线程的Worker是SingleThreaded模式的，在创建Scheduler时，会根据配置中指定的线程数来创建指定数量的MultiThreaded模式的Worker。

创建了Worker之后，MultiThreaded Worker会创建一个后台线程，该线程会创建好`main_fiber_`，然后一直循环执行任务，直到Worker Stop。SingleThreaded Worker会创建好`main_fiber_`，然后立即返回。

### Fiber

Fiber是对OSFiber的封装，具有私有的构造函数，只能由Worker和Scheduler间接的创建。

Fiber有5种状态：

1. Idle: 空闲状态，此时Fiber会被存放在Worker的idle_fibers队列中（实际是放在unordered_set中）
2. Yielded：阻塞在wait函数中，并且没有设置超时时间
3. Waiting:  阻塞在wait函数中，设置了超时时间，此时fiber存放在Worker的waiting队列中
4. Queued: 正在等待执行，位于Worker的fibers队列中
5. Running: 正在运行

Fiber有两个重要的函数：

* wait: 提供了多种重载，用来挂起当前fiber，有点类似于协程中yield函数，不过拥有更加强大的功能，支持指定等待时间，以及等待条件（通过循环判断条件是否满足）

  * 如果指定了等待时间，则会将Fiber的状态修改为Waiting，并放到Worker的waiting队列中
  * 如果没有指定等待时间，则将Fiber的状态修改为Yielded

  并且如果Fiber在wait之后发现当前Worker不存在处于Idle和Queued状态的Fiber，则Worker会自动新建一个Fiber，并切换到新的Fiber

* notify: 将当前fiber的状态修改为Queued，并放到Worker的fibers队列中

### Worker循环执行任务的逻辑

一直循环，直到Worker被stop，并且Worker的任务队列为空，且不存在处于阻塞状态的Fiber

* 循环首先会等待到有任务可执行为止，或者Worker已经stop了（通过条件变量）

  如果当前Worker是MultiThreaded模式的话，在发现没有任务可执行之后还会进行任务窃取，任务窃取是通过自旋来实现的，会在1ms内一直尝试窃取任务，直到窃取成功，在窃取的过程中，会把当前Worker放到Scheduler的spining_workers中，提高任务分配的优先级。

* 等到有任务后，Worker会一直执行任务，或者切换到处于Queued状态的协程（切换前会把当前fiber设置成Idle状态）

### 一个Task是如何被执行的

首先通过全局函数marl::schedule函数进行任务的提交，schedule将会调用当前线程绑定的Scheduler的enqueue函数

enqueue函数会根据任务的提交策略：SameThread或者not SameThread，如果任务是SameThread的话，Scheduler会把任务提交到当前线程对应的Worker的任务队列中，否则使用以下策略来分配任务：

* 首先看是否存在spinning_workers，如果有，则使用RoundRobin策略选择一个spinning_workers中的Worker
* 否则使用RoundRobin选择一个Worker
* 如果Scheduler总共只有一个线程的话，也就是说只有一个SingleThreaded的线程的话，会将任务提交到当前Worker的队列中，任务不会马上执行，而是会等到调用Scheduler::unbind的时候执行

## 一些同步原语

亮点：可以在std::thread/fiber混合的环境下进行同步

### ConditionVariable

包含一个std::condition_variable和一个Fiber队列

在wait的时候，判断当前是否是fiber环境，如果是，则调用Fiber::wait函数，并且将Fiber放到队列中，等待返回后，将Fiber从队列中移除，如果不是，则直接调用std::condition_variable的接口。

在notify_one和notify_all的时候，根据当前等待的Fiber数和线程数，调用fiber->notify和std::condition_variable的接口进行通知。

### WaitGroup

WaitGroup是可复制的，每个WaitGroup包含一个指向共享资源的shared_ptr。

共享资源包括当前的计数器，以及一个ConditionVariable，ConditionVariable会在计数器变为0的时候进行notify

### 有趣的内容

defer的实现

thread_annotation

Allocator的设计：使用mmap, 并且以页为单位进行内存分配