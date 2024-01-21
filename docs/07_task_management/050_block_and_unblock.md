# 050 任务阻塞和就绪

## 1. 原理说明

任务阻塞，就是将要阻塞的任务加入指定的 **阻塞队列**，并将任务状态改为 **阻塞状态**。

同理，结束阻塞任务，就是将任务从 **阻塞队列** 中删除，并将任务状态改为 **就绪状态**。

> 为什么要加入阻塞队列？
> ***
> 假设事件 A 导致了一批任务被阻塞，那么当事件 A 结束时，我们可以通过事件 A 导致的阻塞队列，来使因为事件 A 而阻塞的任务，都改为就绪状态。

> 阻塞状态是是什么？
> ***
> 任务的阻塞状态，就是指任务当前无法运行的状态，例如睡眠，等待这些状态。更广泛的说，除了就绪和运行这两个状态外，其它的状态均可视为阻塞（因为都可以认为当前无法运行）。

我们采用 [<049 链表>][049_list] 中实现的链表，来实现阻塞队列。为了能将 PCB 链接起来，我们在 PCB 结构体中新增了一个成员 `node`。

> 代码位于 `include/xos/task.h`

```c
// 任务控制块 TCB
typedef struct task_t {
    u32 *stack;                 // 内核栈
    list_node_t node;           // 任务阻塞节点
    task_state_t state;         // 任务状态
    char name[TASK_NAME_LEN];   // 任务名称
    u32 priority;               // 任务优先级
    u32 ticks;                  // 剩余时间片
    u32 jiffies;                // 上次执行时的全局时间片
    u32 uid;                    // 用户 ID
    u32 page_dir;               // 页目录的物理地址
    bitmap_t *vmap;             // 任务虚拟内存位图
    u32 magic;                  // 内核魔数（用于检测栈溢出）
} task_t;
```

那么通过链表节点连接起来的阻塞队列，它的示意图如下：

![](./images/blocked_list.drawio.svg)

正如 [<049 链表>][049_list] 中所说，我们实现的链表节点并没有包括数据域，这是为了实现链表节点的数据可以是泛型。所以我们通过链表节点 `node` 在 PCB 结构体中的相对偏移，来引用 PCB 结构体中的其它成员。这个操作的原理与普通链表节点中直接引用数据成员，是一样的，本质上都是对指针的巧妙使用。

![](./images/list_node_data.drawio.svg)

> 如何引用 PCB 结构体中的其它成员？
> ***
> 这个可以阅读 [049 链表][049_list] 中那两个神秘的宏：`element_offset` 和 `element_entry`。

## 2. 核心代码

> 代码位于 `include/xos/task.h`

```c
// 阻塞任务
void task_block(task_t *task, list_t *blocked_list, task_state_t state);

// 结束阻塞任务
void task_unblock(task_t *task);
```

## 3. 代码分析

### 3.1 阻塞队列

在 `kernel/task.c` 定义一个默认的阻塞队列，即当没有特定事件的阻塞队列时，将阻塞认为加入到这个默认队列中。

```c
// 默认的阻塞任务队列
static list_t blocked_queue;
```

### 3.2 阻塞任务

依据原理说明，在 `kernel/task.c` 中实现阻塞任务的功能，即加入阻塞队列，修改为阻塞状态。

- 由于涉及阻塞队列这个数据竞争区，需要禁止外中断响应，使得阻塞队列成为临界区。
- 在阻塞任务时，需要注意不能将已经阻塞的任务（即在其它阻塞队列），再次阻塞。
- 修改任务状态时，需要注意指定的阻塞状态是否为合法的阻塞状态。
- 如果阻塞的任务恰好是当前任务，那么需要立即进行调度（阻塞了，当前任务就无法运行了，所以需要调度）。

```c
// 阻塞任务
void task_block(task_t *task, list_t *blocked_list, task_state_t state) {
    // 涉及阻塞队列这个临界区
    ASSERT_IRQ_DISABLE();

    // 任务没有位于任一阻塞队列中
    ASSERT_NODE_FREE(&task->node);

    // 如果加入的阻塞队列为 NULL，则加入默认的阻塞队列
    if (blocked_list == NULL) {
        blocked_list = &blocked_queue;
    }

    // 加入阻塞队列，并将任务状态修改为阻塞
    list_push_back(blocked_list, &task->node);
    ASSERT_BLOCKED_STATE(state);
    task->state = state;

    // 如果阻塞的是当前任务，则立即进行调度
    task_t *current = current_task();
    if (current == task) {
        schedule();
    }
}
```

### 3.3 结束阻塞任务

依据原理说明，在 `kernel/task.c` 中实现结束阻塞任务的功能，即在阻塞队列中删除，修改为就绪状态。

- 由于涉及阻塞队列这个数据竞争区，需要禁止外中断响应，使得阻塞队列成为临界区。
- 在结束阻塞任务时后，保证任务已经不处于任一阻塞队列中了（如果任务还处于任一阻塞队列，那么与后面任务状态改为就绪的逻辑冲突）。

```c
// 结束阻塞任务
void task_unblock(task_t *task) {
    // 涉及阻塞队列这个临界区
    ASSERT_IRQ_DISABLE();

    // 在任务所处的阻塞队列进行删除
    list_remove(&task->node);

    // 任务此时没有位于任一阻塞队列当中
    ASSERT_NODE_FREE(&task->node);

    // 任务状态修改为就绪
    task->state = TASK_READY;
}
```

### 3.4 初始化任务管理

对 `kernel/task.c` 的任务管理初始化，进行逻辑补充，加入初始化默认任务阻塞队列的逻辑。

```c
// 初始化任务管理
void task_init() {
    list_init(&blocked_queue);
    ...
}
```

## 4. 功能测试

按照惯例，在内核主函数中搭建测试框架：

> 代码位于 `kernel/main.c`

```c
void kernel_init() {
    console_init();
    gdt_init();
    memory_init();
    kernel_map_init();
    interrupt_init();
    clock_init();
    task_init();
    syscall_init();

    irq_enable(); // 打开外中断响应

    hang();
    return;
}
```

---

修改系统调用 `test()` 对于的处理函数 `sys_test()` 的内部逻辑：

> 代码位于 `kernel/syscall.c`

```c
static task_t *task = NULL; // 当前阻塞任务

// 系统调用 test 的处理函数
static u32 sys_test() {
    // LOGK("syscall test...\n");

    if (task == NULL) { // 如果当前没有任务被阻塞，则阻塞当前任务
        task = current_task();
        // LOGK("block task 0x%p\n", task);
        task_block(task, NULL, TASK_BLOCKED);
    } else {            // 否则结束阻塞当前的阻塞任务
        task_unblock(task);
        // LOGK("unblock task 0x%p\n", task);
        task = NULL;
    }

    return 255;
}
```

---

修改内核线程 A、B、C 的内部逻辑，改为使用系统调用 `test()` 来测试阻塞：

> 代码位于 `kernel/task.c`

```c
u32 thread_a() {
    irq_enable();

    while (true) {
        printk("A");
        test();
    }
}
...
```

---

测试预期为，连续打印两个相同字符后，打印另一不同的字符。例如 `BBCCAABB...`。

但是这个也不绝对，比如一开始可能会打印字符 `ABBAA..`，这取决于你实现的调度算法，以及线程 A、B、C 内部调用 `test()` 的时机，还有时钟中断的触发时机。

---

如果我们只使用 2 个内核线程来测试，那么运行到某一个时刻，会触发无法找到任何就绪任务的 `panic`。

这是因为，当我们使用 3 个线程时，保证了在最坏情况下，一个线程处于运行，一个线程处于阻塞，一个线程处于就绪。这就保证了无论如何都可以寻找到一个就绪任务。

而当我们使用 2 个线程时，那么在最坏情况下，一个线程处于运行，而另一个线程处于阻塞。如果此时触发时钟中断，那么调度算法会无法寻找到就绪任务，而触发 `panic`。

--- 

**那我们现在提个问题：如果所有任务都阻塞，怎么办？**

## 5. FAQ

> 为什么在修改后构建运行系统，会触发栈溢出？
> ---
> 这是因为我们的 PCB 结构体定义在头文件 `include/xos/task.h` 中，所以源文件 `kernel/clock.c` 不会被重新编译。
>
> 因为 makefile 只追踪源文件的修改，而头文件是直接文本替换，所以在 makefile 看来，`kernel/clock.c` 没有重新编译的需要。
>
> 但是 `kernel/clock.c` 需要重新编译，否则它获得的 PCB 结构体还是没更新的版本。当你使用 GDB 去调试追踪栈溢出这个奇怪错误时，会发现 `kernel/clock.c` 中的 PCB 结构体并没有 `node` 成员，这造成了引用的 `magic` 位置有误（少了一个成员 `node` 的偏移），从而导致了栈溢出的误报。
>
> 解决方法很简单，先对系统进行清除，然后再构建运行系统，这样系统就按预期一样运行了。


[049_list]: ../06_data_structures/049_list.md