# 054 锁

## 1. 互斥锁

我们在本节实现的互斥锁是 **可重入的互斥锁**。

> 可重入是指一个线程在持有锁的情况下，可以再次请求该锁而不会被自己阻塞。也就是说，如果线程A已经获得了一个锁，那么在没有释放该锁之前，它可以继续多次获得同一个锁。
> 
> 可重入锁解决了线程在重复获取同一个锁时引发的死锁问题。在复杂的多线程应用程序中，一个线程可能需要多次获取同一个锁。如果锁是不可重入的，那么线程在第二次请求锁时将被阻塞，因为它已经拥有了该锁。这种情况下，该线程可能会因为无法获取该锁而导致程序发生死锁。
> 
> 通过使用可重入锁，一个线程可以多次获得同一个锁而不会被自己阻塞，从而避免了死锁问题的发生。这对于编写安全可靠的多线程代码非常重要。

> 以下代码位于 `include/xos/mutex.h`

```c
// 可重入的互斥锁
typedef struct mutexlock_t {
    task_t *holder; // 持有者
    mutex_t mutex;  // 互斥量
    size_t  repeat; // 重入次数
} mutexlock_t;

// 初始化互斥锁
void mutexlock_init(mutexlock_t *lock);

// 持有锁
void mutexlock_acquire(mutexlock_t *lock);

// 释放锁
void mutexlock_release(mutexlock_t *lock);
```

由上面互斥锁的结构可以看出，我们实现的互斥锁是以互斥量为中心，再加上成员锁的持有者 `holder` 和锁的重入次数 `repeat` 来实现锁的可重入机制。

## 2. 自旋锁

自旋锁最多只能被一个可执行线程持有。如果一个执行线程试图获得一个被已经持有（即所谓的争用）的自旋锁，那么该线程就会一直进行忙循环一旋转一等待锁重新可用。要是锁未被争用，请求锁的执行线程便能立刻得到它，继续执行。在任意时间，自旋锁都可以防止多于一个的执行线程同时进入临界区。同一个锁可以用在多个位置，例如，对于给定数据的所有访问都可以得到保护和同步。

使用 CAS (Compare And Swap) 原语实现，IA32 中是 `cmpxchg` 指令，但在多处理机系统中 `cmpxchg` 指令本身不是原子的，还需要加 `lock` 来锁定内存总线实现原子操作。

> On multiprocessor systems, ensuring atomicity exists is a little harder. It is still possible to use a lock (e.g. a spinlock) the same as on single processor systems, but merely using a single instruction or disabling interrupts will not guarantee atomic access. You must also ensure that no other processor or core in the system attempts to access the data you are working with. The easiest way to achieve this is to ensure that the instructions you are using assert the `'LOCK'` signal on the bus, which prevents any other processor in the system from accessing the memory at the same time. On x86 processors, some instructions automatically lock the bus (e.g. `'XCHG'`) while others require you to specify a `'LOCK'` prefix to the instruction to achieve this (e.g. `'CMPXCHG'`, which you should write as `'LOCK CMPXCHG op1, op2'`).

## 3. 读写锁

有时，锁的用途可以明确地分为读取和写入两个场景。并且绝大多数是读的情况，由于读并不影响数据内容，所以如果直接加锁就会影响性能，那么可以将读和写区别开来，这种形式的锁就是读写锁；

当对某个数据结构的操作可以像这样被划分为读／写 或者 消费者／生产者 两种类别时，类似读／写锁这样的机制就很有帮助了。这种自旋锁为读和写分别提供了不同的锁。一个或多个读任务可以并发地持有读者锁：相反，用于写的锁最多只能被一个写任务持有，而且此时不能有并发的读操作。有时把读／写锁叫做共享排斥锁，或者 并发／排斥锁，因为这种锁以共享（对读者而言）和排斥（对写者而言）的形式获得使用。

但在实现的时候需要注意，有可能会发生读者过多而饿死写着的情况。如果写的情况比较多就不应该使用这种锁。

## 4. 代码分析

### 4.1 初始化互斥锁

```c
// 初始化互斥锁
void mutexlock_init(mutexlock_t *lock) {
    lock->holder = NULL;
    mutex_init(&lock->mutex);
    lock->repeat = 0;
}
```

初始化互斥锁的逻辑十分简单，初始时锁并没有持有者，锁的重入次数当然为 0，并初始化锁的互斥量。

### 4.2 持有互斥锁

```c
// 持有锁
void mutexlock_acquire(mutexlock_t *lock) {
    task_t *current = current_task();

    if (lock->holder != current) {
        // 如果锁的持有者不是当前这个任务，则该任务尝试获持有锁
        mutex_acquire(&lock->mutex);
        // 当前任务成功持有锁
        lock->holder = current;
        assert(lock->repeat == 0);
        lock->repeat = 1;
    } else {
        // 如果当前任务已经是锁的持有者，则只需更新锁的重入次数
        lock->repeat++;
    }
}
```

持有互斥锁的逻辑由两部分组成：

- 如果当前尝试持有锁的任务不是锁的持有者，那么该任务将尝试获持有锁（通过尝试持有锁的互斥量）；如果该任务成功取到互斥量，则更新锁的相关数据（持有者、重入次数）；如果取锁失败，则因为互斥量的设置，该任务被阻塞。
- 如果当前尝试持有锁的任务已经是锁的持有者，则只需更新锁的重入次数，与后续的释放锁的操作相对应。

> 注：第一次取锁我们设定锁的重入次数为 1，这是为了方便后续解锁的操作。

### 4.3 释放互斥锁

```c
// 释放锁
void mutexlock_release(mutexlock_t *lock) {
    task_t *current = current_task();

    // 只有锁的持有者才能释放锁
    assert(current == lock->holder);

    if (lock->repeat > 1) {
        // 如果锁的重入次数大于 1，则表示已经进行了多次重入，只需更新重入次数
        lock->repeat--;
    } else {
        // 如果锁的重入次数为 1，则需要释放锁
        assert(lock->repeat == 1);
        lock->holder = NULL;
        lock->repeat = 0;
        mutex_release(&lock->mutex);
    }
}
```

释放互斥锁的逻辑也由两部分组成：

- 如果当前锁的重入次数大于 1，则表示锁的持有者已经对该锁进行了多次重入，那么只需更新锁的重入次数即可。
- 如果当前锁的重入次数为 1，则需要释放锁，并更新锁的数据（持有者、重入次数）。这里也是通过互斥量的释放操作来使其他任务获取锁，防止任务“饿死”。

## 5. 功能测试

```c
/* kernel/thread.c */

mutexlock_t lock; // 显示内存区域互斥锁

void init_thread() {
    irq_enable();
    u32 counter = 0;

    while (true) {
        mutexlock_acquire(&lock);
        LOGK("init task %d...\n", counter++);
        mutexlock_release(&lock);
    }
}

void test_thread() {
    irq_enable();
    u32 counter = 0;

    while (true) {
        mutexlock_acquire(&lock);
        LOGK("test task %d...\n", counter++);
        mutexlock_release(&lock);
    }
}

/********************************************************************/

/* kernel/task.c */

extern mutexlock_t lock; // 显示内存区域互斥锁

// 初始化任务管理
void task_init() {
    ...
    mutexlock_init(&lock);
    ...
    task_create((target_t)init_thread, "init", 5, USER_TASK);
    task_create((target_t)test_thread, "test", 5, KERNEL_TASK);
}
```

测试没有使用互斥锁和使用互斥锁的情况，进行对比测试（可能需要使用 bochs 或 vmware 来测试，因为 qemu 有可能不会出现问题）。

- 没有使用互斥锁的情况，打印时信息可能会出现错位、屏幕颜色变化、无法显示等问题。
- 使用互斥锁的情况，正常打印信息，没有出现因为数据竞争而导致的打印问题。

## 6. FAQ

> `mutexlock_acquire()` 和 `mutexlock_release()` 都没有使用禁止中断来保证原子性，那它们是怎么防止数据竞争的？
> ***
> `mutexlock_acquire()` 和 `mutexlock_release()` 是通过 **互斥量操作的原子性** 以及 **锁的持有者是唯一的**，这两点来保证不会发生数据竞争。加锁和解锁这两个函数，都只有锁的持有者才能完整执行函数体的逻辑。
> 
> `mutexlock_release()` 相对比较简单，它只有锁的持有者才能进入函数体执行，而锁的持有者是唯一的，所以这样就保证了只有一个任务才能执行这个函数，所以无数据竞争，再配合互斥量的 `mutex_release()` 操作的原子性，保证了解锁操作的原子性和防止了数据竞争。
> 
> `mutexlock_acquire()` 也同理，更新重入次数只有锁的持有者才能执行，无数据竞争；尝试取锁部分，如果任务没有成功取锁，则被阻塞（`mutex_lock()` 的逻辑），如果成功取锁，那么由于只能有一个任务能成功取锁，所以也没有数据竞争。


## 7. 参考文献

- <https://wiki.osdev.org/Spinlock>
- [[美] Robert Love / Linux内核设计与实现 / 机械工业出版社 / 2011](https://book.douban.com/subject/6097773/)
- <https://wiki.osdev.org/Atomic_operation>
- [什么是可重入，什么是可重入锁? 它用来解决什么问题? - bilibili](https://www.bilibili.com/read/cv24676489)