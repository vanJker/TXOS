# 053 互斥与信号量

## 1. 原子操作（单处理机）

在单处理机系统中，单个指令总是原子的，所以对于单指令例如 `xchg` 或者 `inc` 等都是原子的。因为中断只会在指令结束后才触发。

但如果一个操作需要多个 CPU 指令，由于每条指令结束之后都可能发生中断，而发生上下文切换，那么这个操作就可能不会很好的完成。所以就可能需要加锁机制来防止这种情况发生。但加锁很可能不高效，所以为了执行原子操作的指令序列，有时候禁止中断可能反而更高效。

比如说，任务 A 对某一个数据区 D 加锁后，（主动 / 被动地）进行了任务调度，但是调度到的任务 B 恰好也要访问这个数据区 D，但是任务 B只能进行等待，直到任务 B 持有锁，但由于任务 A 在此时持有锁，所以只有在调度回任务 A 释放锁，任务 B 才有可能持有锁，这就浪费了任务 B 的那部分 CPU 时间（如果锁是自旋锁的话，即使不是自旋锁，上下文切换的开销也很大）。

![](./images/lock_01.drawio.svg)

## 2. 信号量

信号量是一种最古老而且应用最广泛的保证互斥的方法，此概念是由荷兰人 E.W.Dijkstra 在 1965 年提出的，其原型来自于铁路上的信号灯。计算机中信号量是个整数值，由于 Dijkstra 是荷兰人，他用 P (Proberen) 尝试减少 和 V (Verhogen) 增加，来表示信号的变化，但 PV 操作语义并不明显，因此这里使用 `up` 和 `down` 来表示对信号量的增减，当然也有人使用 `wait` 和 `signal` 来表示信号量的操作。

## 3. 互斥量

当信号量使用 0/1 二值来表示时，就是互斥量，表示临界区只能有一个进程可以取得。

> 以下代码位于 `include/xos/mutex.h`

```c
// 互斥量
typedef struct mutex_t {
    bool semphore;  // 信号量，也即剩余资源数
    list_t waiters; // 等待队列
} mutex_t;

// 初始化互斥量
void mutex_init(mutex_t *mutex);

// 尝试持有互斥量
void mutex_acquire(mutex_t *mutex);

// 释放互斥量
void mutex_release(mutex_t *mutex);
```

互斥量的等待队列机制是 “先进先出” 的，即持有互斥量的任务保证强制轮换，防止等待持有互斥量的任务 “饿死”。示意图如下：

![](./images/mutex_waiters.drawio.svg)

## 4.  Dijkstra

![](./images/Edsger_Wybe_Dijkstra.jpg)

> The academic study of concurrent computing started in the 1960s, with Dijkstra (1965) credited with being the first paper in this field, identifying and solving the **mutual exclusion problem**. He was also one of the early pioneers of the research on principles of d**istributed computing**. His foundational work on concurrency, semaphores, mutual exclusion, deadlock (deadly embrace), finding **shortest paths in graphs**, fault-tolerance, **self-stabilization**, among many other contributions comprises many of the pillars upon which the field of distributed computing is built. Shortly before his death in 2002, he received the ACM PODC Influential-Paper Award in distributed computing for his work on self-stabilization of program computation. This annual award was renamed the Dijkstra Prize (Edsger W. Dijkstra Prize in Distributed Computing) the following year. As the prize, sponsored jointly by the Association for Computing Machinery (ACM) Symposium on Principles of Distributed Computing (PODC) and the European Association for Theoretical Computer Science (EATCS) International Symposium on Distributed Computing (DISC), recognizes that "No other individual has had a larger influence on research in principles of distributed computing".

## 5. 代码分析

> 以下代码位于 `kernel/mutex.c`

### 5.1 初始化互斥量

`mutex` 中的成员 `semphore` 表示信号量，即剩余资源的数量。由于 `mutex` 是互斥量，即可用资源数为 1，所以 `semphore` 的初始值为 1。

```c
// 初始化互斥量
void mutex_init(mutex_t *mutex) {
    mutex->semphore = 1; // 初始时剩余资源数为 1
    list_init(&mutex->waiters);
}
```

### 5.2 尝试持有互斥量

- 为了保证该操作的原子性，所以需要禁止外中断响应。
- 当无可用资源时，需要将尝试持有互斥量的任务，加入该互斥量的等待队列，并进行阻塞（注意我们的 `task_block()` 采用了 `pushback` 作为加入方式）。

```c
// 尝试持有互斥量
void mutex_acquire(mutex_t *mutex) {
    // 禁止外中断响应，保证原子操作
    u32 irq = irq_disable();

    // 尝试持有互斥量
    task_t *current = current_task();
    while (mutex->semphore == 0) {
        // 如果 semphore 为 0，则表明当前无可用资源
        // 需要将当前进程加入该互斥量的等待队列
        task_block(current, &mutex->waiters, TASK_BLOCKED);
    }

    // 该互斥量当前无人持有
    assert(mutex->semphore == 1);

    // 持有该互斥量
    mutex->semphore--;
    assert(mutex->semphore == 0);

    // 恢复为之前的中断状态
    set_irq_state(irq);
}
```

### 5.3 释放互斥量

- 为了保证该操作的原子性，所以需要禁止外中断响应。
- 当任务释放互斥量时，需要从该互斥量的等待队列中取出最先进入的任务，将其设置为就绪，同时使当前任务主动放弃执行权，防止排队的任务 ”饿死”（因为是 `pushback` 的加入方式，所以头节点的下一个就是最先进入的任务）。

```c
// 释放互斥量
void mutex_release(mutex_t *mutex) {
    // 禁止外中断响应，保证原子操作
    u32 irq = irq_disable();

    // 已持有该互斥量
    assert(mutex->semphore == 0);

    // 释放该互斥量
    mutex->semphore++;
    assert(mutex->semphore == 1);

    // 如果该互斥量的等待队列不为空，则按照 “先进先出“ 的次序结束等待进程的阻塞
    if (!list_empty(&mutex->waiters)) {
        task_t *task = element_entry(task_t, node, mutex->waiters.head.next);
        assert(task->magic == XOS_MAGIC); // 检测栈溢出
        task_unblock(task);
        // 保证新进程可以获取互斥量，否则可能会被饿死
        task_yield();
    }

    // 恢复为之前的中断状态
    set_irq_state(irq);
}
```

### 5.4 `console` 临界区

由于现在可一使用互斥量来限制临界区的访问，所以我们无需保证 `console_write()` 的原子操作（先前是通过 **禁止外中断** 实现的），而是将这个原子性保证留给进程来实现（通过 **互斥量 / 锁**）。

## 6. 功能测试

```c
/* kernel/thread.c */

mutex_t mutex; // 显示内存区域互斥量

void init_thread() {
    irq_enable();
    u32 counter = 0;

    while (true) {
        mutex_acquire(&mutex);
        LOGK("init task %d...\n", counter++);
        mutex_release(&mutex);
    }
}

void test_thread() {
    irq_enable();
    u32 counter = 0;

    while (true) {
        mutex_acquire(&mutex);
        LOGK("test task %d...\n", counter++);
        mutex_release(&mutex);
    }
}

/********************************************************************/

/* kernel/task.c */

extern mutex_t mutex; // 显示内存区域互斥量

// 初始化任务管理
void task_init() {
    ...
    mutex_init(&mutex);
    ...
    task_create((target_t)init_thread, "init", 5, USER_TASK);
    task_create((target_t)test_thread, "test", 5, KERNEL_TASK);
}
```

测试没有使用互斥锁和使用互斥锁的情况，进行对比测试（可能需要使用 bochs 或 vmware 来测试，因为 qemu 有可能不会出现问题）。

- 没有使用互斥锁的情况，打印时信息可能会出现错位、屏幕颜色变化、无法显示等问题。
- 使用互斥锁的情况，正常打印信息，没有出现因为数据竞争而导致的打印问题。

测试没有使用互斥量和使用互斥量的情况，进行对比测试（可能需要使用 bochs 或 vmware 来测试，因为 qemu 有可能不会出现问题）。

- 没有使用互斥量的情况，打印时信息可能会出现错位、屏幕颜色变化、无法显示等问题。
- 使用互斥量的情况，正常打印信息，没有出现因为数据竞争而导致的打印问题。

## 7. FAQ

> 由于本节当中使用 **任务上下文切换** 来进行任务切换，而任务上下文切换不涉及 **中断状态**，但是互斥量的操作为了保证原子性有需要保证中断禁止，所以这部分十分混乱。
> 
> 典型的问题如：任务再次进入 `mutex_acquire()` 或 `mutex_release()` 时，外中断响应状态是否仍然为禁止？因为第一次进入 `mutex_acquire()` 或 `mutex_release()` 时，是通过 **任务上下文切换** 到其它的任务执行的，没有涉及保存中断状态的操作。
> 
> 我们通过下面的流程来理解这个设计的正确性。

![](./images/mutex_acquire_release.drawio.svg)

---

> **问题：思考当有更多任务时，这个设计的正确性是否仍被保证？**
> ***
> 依然成立。
> 
> 因为第二次进入 `mutex_acquire()` 时，必定是通过 `mutex_release()` 中的任务调度进入的，而进入 `mutex_release()` 时，中断已经处于禁止状态，所以第二次进入 `mutex_acquire()` 的中断禁止状态得以保证。
> 
> 同理，第二次进入 `mutex_release()` 时，必定是通过时钟中断的任务调度进入的，而触发时钟中断时，中断已经处于禁止状态，所以第二次进入 `mutex_release()` 的中断禁止状态得以保证。（这里并不完全准确，实际上只要是通过中断门进入 `mutex_release()` 的话，都可以保证处于中断禁止态，因为字中断门已经禁止了中断）

## 8. 参考文献

1. <https://en.wikipedia.org/wiki/Edsger_W._Dijkstra>
2. <https://wiki.osdev.org/Atomic_operation>
3. <https://wiki.osdev.org/Synchronization_Primitives>
4. <https://wiki.osdev.org/Mutual_Exclusion>
5. <https://wiki.osdev.org/Semaphore>
6. [郑刚 / 操作系统真象还原 / 人民邮电出版社 / 2016](https://book.douban.com/subject/26745156/)