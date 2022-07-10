在构造一个Scheduler之后：

* Scheduler::Scheduler(Config)

    * 将spining_workers数组(大小为8的array)全部设置为-1
    * 根据config中的线程数，创建指定个Worker（工作线程），工作模式为MultiThreaded)，将每个Worker中绑定的Scheduler设置为当前Scheduler
    * **依次调用每个Worker的start()函数。**

* Worker::start()

    * 如果是MultiThreaded模式的话（否则进入下一步），根据配置中的线程亲和性，创建一个marl::Thread对象（包含一个std::thread对象），thread会执行以下任务
        * 调用配置中的初始化函数
        * 将该线程绑定的Scheduler设置为Worker绑定的Scheduler
        * 根据当前线程创建一个main_fiber，并且将current_fiber设置成main_fiber，（Fiber的初始状态为Running）
        * 获取Worker中的Work中的锁
        * **调用Worker的run()函数**
    * 如果是SingleThreaded模式的话：
        * 根据当前线程创建一个main_fiber，并且将current_fiber设置成main_fiber

* Worker::run()，调用前需持有Work.mutex

    * 如果当前Worker是MultiThreaded模式的话（否则进入下一步），调用Work的wait函数，一直等到任务队列不为空，或者存在处于Queued状态的fiber(位于Work::fibers队列)，或者存在处于Waiting状态的fiber（位于Work::waiting队列），或者当前Worker的shutdown变量为true（由Worker::stop()函数设置）。等待的过程会将Work.mutex释放，等待完成又会重新获取Work.mutex，类似std::condition_variable的行为（实际上就是借助条件变量实现的）
    * 确保current_fiber的状态为Running
    * **调用Worker::runUntilShutdown函数**
    * 切换到main_fiber

* Worker::runUntilShutdown()，调用前需持有Work.mutex

    * 若满足：shutdown为false 或者 任务队列不为空 或者 存在处于Queued状态的fiber 或者 存在处于阻塞状态中的fiber(通过Worker::suspend函数)

      则进入循环，处理一下任务：

        * **调用Worker::waitForWork()来等待可用的任务**
        * **调用Worker::runUntilIdle()来处理任务**

* Worker::waitForWork()，调用前需持有Work.mutex

    * 如果任务队列不为空或者存在处于Queued状态的fiber，则立马返回
    * 如果是MultiThreaded模式的话（否则进入下一步），则进行以下处理：
        * 将当前Worker的id放到绑定的Scheduler的spinning_workers数组中
        * 释放Work.mutex
        * **调用Worker::spinForWork()函数来自旋等待任务**
        * 重新锁住Work.mutex
    * 调用Work::wait，一直等到任务队列不为空，或者存在处于Queued状态的fiber或 （已经shutdown并且不存在阻塞的fiber）
    * **如果存在处于Waiting状态的Fiber的话，则调用Worker::enqueueFiberTimeouts函数**

* Worker::spinForWork()

    * 循环以下行为，直到消耗时间超过1ms：
        * 循环256次：
            * 进行32的nop操作（汇编里的nop）
            * 判断任务队列是否有任务，或者有处于Queued状态的fiber，如果有的话，立即返回，否则继续循环
        * 随机从一个Scheduler绑定的Worker中偷取一个任务，如果偷取成功（偷取失败则进入下一步）：
            * 通过marl::lock（一个RAII类型的锁）获取Work.mutex
            * 将偷取的任务放到Work的任务队列中，然后立即返回
            * return的前一刻，会调用marl::lock的析构函数：将Work.mutex释放
        * 调用std::this_thread::yield()

* Worker::enqueueFiberTimeouts，调用前需持有Work.mutex

    * 循环取出work_.waiting队列中所有已经超时的Fiber，然后对每个Fiber进行如下操作：
        * 将Fiber的状态从Waiting切换到Queued
        * 然后将Fiber添加到work.fibers队列中

* Worker::runUntilIdle()，调用前需持有Work.mutex

    * 确保current_fiber处于Running状态
    * 当  存在处于Queued状态的fiber 或者 任务队列不为空时 进行循环：
        * 一直循环进行以下处理，直到不再有处于Queued状态的fiber
            * 取出一个处于Queued状态的fiber(从Work.fibers中)
            * 确保取出的fiber和current_fiber不相同
            * 将current_fiber的状态从Running切换为Idle，并且将current_fiber加入到idle_fibers容器中
            * **将执行流由current_fiber切换到取出来的fiber上**
            * 将current_fiber的状态由Idle切换到Running
        * 如果当前的任务队列不为空的话，则进行以下处理：
            * 从任务队列中取出一个任务task
            * 将Work.mutex解锁
            * **运行task**
            * 重置task对象，以将原来的task进行析构（因为std::function的析构函数比较复杂，所以最好在没有持有锁的时候进行析构）
            * 重新锁住Work.mutex

在调用Scheduler::bind之后：

* Scheduler::bind()，需确保调用该函数的线程之前没有绑定过Scheduler
    * 将当前线程绑定的Scheduler设置为当前Scheduler
    * 获取SingleThreadedWorkers.mutex
    * 创建一个SingleThreaded模式的Worker
    * **调用Worker::start()**
    * 将该Worker放到SingleThreadedWorkers中

在调用marl::schedule或者Scheduler::enqueue之后：

* Scheduler::enqueue(Task)
    * 如果Task是SameThread的（否则进入下一步），则会将任务加入到当前线程对应的Worker的任务队列中。
    * 如果创建Scheduler时的配置中工作线程数>0（否则进入下一步），则会一直循环如下操作：
        * 首先尝试从spining_workers中找出一个合法的worker，如果没找到的话，则从worker_threads中选一个
        * 调用Worker::tryLock函数，让Worker获取它的Work.mutex，如果获取成功的话，则调用Worker::enqueueAndUnlock函数（将任务放到Worker的任务队列里，并且通过条件变量通知那些正在等待任务的线程，然后把Work.mutex释放），并且立即返回；如果获取失败的话，进入下一轮循环。
    * 如果创建Scheduler时的配置中工作线程数<=0的话，则会将任务放到当前线程对应的Worker的任务队列中（因此必须先进行bind操作）



在调用Scheduler::unbind之后

* Scheduler::unbind()
    * 首先必须确保当前线程绑定了Scheduler
    * 获取当前线程对应的Worker
    * **调用Worker.stop()**
    * 通过marl::lock获取线程绑定的Scheduler的SingleThreadedWorkers.mutex
    * 将Worker从SingleThreadedWorkers中删除
    * 如果SingleThreadedWorkers中的Worker数量为0的话，则通过条件变量唤醒正在等待SingleThreadedWorkers中Worker为空的线程（处于Scheduler的析构函数）
    * marl::lock的析构函数将SingleThreadedWorkers.mutex释放
* Worker::stop()
    * 如果Worker是MultiThreade模式的话（否则进入下一步），则向Worker的任务队列中添加一个将shutdown设置为true的任务，并且调用对应线程的join函数
    * 如果Worker是SingleThreaded模式的话，会先获取Work.mutex，然后将shutdown设置为true，接着调用Worker::runUntilShutdown()

在作用域末尾，调用Scheduler的析构函数之后

* Scheduler::~Scheduler()
    * 通过marl::lock获取SingleThreadedWorkers.mutex
    * 等待SingleThreadedWorkers中Worker的数量为0
    * marl::lock的析构函数将SingleThreadedWorkers.mutex释放
    * **依次调用每个MultiThreaded Worker的stop函数**



调用Fiber::wait()之后，会调用Worker::wait

* Worker::wait(wait_lock, timeout, pred)

    * 在不满足pred()为true时，一直循环以下操作：
        * 锁住Work.mutex
        * 释放wait mutex
        * **调用Worker::suspend(timeout)**
        * 释放Work.mutex
        * 如果没有设置超时时间（timeout == nullptr）或者已经超时，则返回false
        * 否则进入下一轮循环
    * pred()为true退出循环后，返回true

* Worker::suspend(timeout)

    * 如果设置了超时时间(timeout != nullptr)，则将current_fiber_的状态从Running切换成Waiting，并且将当前fiber添加到Work::waiting容器中。

      如果没有设置超时时间的话，将current_fiber的状态设置成Yielded

    * **调用Worker::waitForWork()**

    * 如果存在处于Queued状态的fiber的话，则取出一个，并且切换到该fiber

      否则，如果存在处于Idle状态的fiber的话，则取出一个，并切换到该fiber

      否则，创建一个新的fiber，并且切换到该fiber

    * 将current_fiber的状态设置成Running



调用Fiber::notify之后：

* Fiber::notify

    * **调用Worker::enqueue(this)**

* Fiber::enqueue(fiber)

    * 通过marl::lock获取Work.mutex

    * 如果fiber的状态为Running或Queued，则直接返回。

      如果fiber的状态为Waiting，则将该fiber从Work.waiting容器中删除

      如果fiber的状态为Idle或Yielded，什么都不做

    * 读取Work.notify_add，存到notify中

    * 将fiber放到Work.fibers容器中

    * 将fiber的状态设置为Queued

    * marl::lock的析构函数释放Work.mutex

    * 如果notify为true，则通知一个等待于Work::wait函数的线程。

