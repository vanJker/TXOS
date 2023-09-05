# 051 基础任务

## 1. 空闲进程 idle

在所有进程都处于阻塞状态时，调度到 idle 进程执行，一般它的进程 id 为 0。

idle 进程的主要工作为：

1. 使能外中断响应
2. 暂停 CPU，等待外中断
3. 如果有就绪任务，则调度到就绪任务执行

## 2. 初始化进程 init

init 进程负责执行初始化操作，进入到用户态等操作，一般它的进程 id 为 1。

> 注意要将 init 进程与之前的 setup 内核线程相区分，init 进程的权级为 USER。

## 3. 代码分析

### 3.1 任务调度

当在任务队列中，无法寻找到任一就绪的任务时，则返回 idle 这个空闲任务。因为 idle 任务就是为了这种情况而诞生的（思考下 [<050 任务阻塞与就绪>](./050_block_and_unblock.md) 中所提出的问题）。

需要注意的是，返回 idle 任务，也是 `task_search()` 会返回当前任务的唯一一种情况（以当前的设计来看），因为一般情况下的任务搜寻，会排除当前任务自身。

> 以下代码位于 `kernel/task.c`

```c
// 空闲任务
static task_t *idle_task;

static task_t *task_search(task_state_t state) {
    ...
    // 当无法寻找到任一就绪的任务时，返回 idle 这个空闲任务
    if (result == NULL && state == TASK_READY) {
        result = idle_task;
    }
    ...
}
```

### 3.2 空闲任务 idle

按照原理说明，实现空闲进程 idle 的执行流。

> 以下代码位于 `kernel/thread.c`

```c
// 空闲任务 idle
void idle_thread() {
    irq_enable();

    size_t counter = 0;
    while (true) {
        LOGK("idle task... %d\n", counter++);
        asm volatile(
            "sti\n" // 使能外中断响应
            "hlt\n" // 暂停 CPU，等待外中断响应
        );
        yield();    // 放弃执行权，进行任务调度
    }
}
```

在 `while` 循环体内，每次使用 `hlt` 指令暂停 CPU 之前，都需要使用 `sti` 指令来使能外中断响应，这看起来与之前的 `irq_enable()` 重复了。其实不然，这是因为在调度函数 `schedule()`（由 **时钟中断** 或 **系统调用 `yield()`** 来调用） 中，如果没有找到其它合适的就绪任务，就直接返回原先的任务（的触发中断的位置）继续执行（无需进行任务上下文切换），而这个过程经过了中断门，所以重新调度回时，需要重新开中断。

```c
// 任务调度
void schedule() {
    ...
    next->state = TASK_RUNNING;
    if (next == current) { // 如果下一个任务还是当前任务，则无需进行上下文切换
        return;
    }

    task_switch(next);
}
```

idle 进程的 `while` 循环体中的最后一条语句，是 `yield()` 系统调用，这个是 idle 进程主动搜寻当前的就绪任务，如果有，就调度到那个就绪任务执行，否则，就继续执行 idle 的指令流。这个主动调用 `yield()` 的优势在于，可以提前调度到已经就绪的任务执行，而不用等待到当前时间片结束，触发时钟中断进行调度，节省了一部分 CPU 时间。

### 3.3 初始化任务 init

本节的 init 进程仅用于测试，它的主要功能为，通过系统调用 `test()` 来阻塞自身，并调度到其它就绪任务执行。

```c
// 初始化任务 init
void init_thread() {
    irq_enable();

    while (true) {
        LOGK("init task...\n");
        test();
    }
}
```

### 3.4 创建基础任务

在 `kernel/task.c` 中创建基础任务 idle 和 init，并进行初始化。

```c
extern void idle_thread();
extern void init_thread();

// 初始化任务管理
void task_init() {
    ...
    idle_task = task_create((target_t)idle_thread, "idle", 1, KERNEL_TASK);
    task_create((target_t)init_thread, "init", 5, USER_TASK);
}
```

## 4. 功能测试

使用调试来跟踪系统中任务切换流程（这个流程因调度算法而异）：

```
                                   _____
                                  |     |
                                  v     |
--> setup --> idle --> init --> idle ___|
```

1. 启动系统，在 `setup` 任务完成初始化操作，调度到 `idle` 任务执行。
2. `idle` 任务通过系统调用 `yield()` 寻找到当前的就绪任务 `init`，调度到 `init` 任务执行。
3. `init` 任务通过系统调用 `test()` 阻塞自身，重新调度到 `idle` 任务执行。
4. 由于 `init` 任务被阻塞，并且没有机会被结束阻塞，所以一直在 `idle` 任务中执行。

---

预期为，依次打印：

- `"idle task... 0"`
- `"init task..."`
- `"idle task... 1"`
- `"idle task... 2"`
- ......

---

> 可以尝试将 `init_thread()` 中的系统调用 `test()` 语句删除，观察在这种情况下的任务调度流程。