# 052 任务睡眠和唤醒

## 1. 原理说明

本节我们实现一个功能：将当前任务睡眠指定的一段时间，睡眠结束后由系统主动唤醒该任务，使任务进入就绪状态。同时我们将该功能封装成系统调用 `sleep()`。

本节内容的实现也比较简单，只要明确一点，**系统内部的时间计数是由全局时间片来决定的**，那么只要将任务的睡眠时间转换成时间片数量，并且在每次触发 **时钟中断** 时判断是否需要唤醒任务，即可完成。

下面是本节的核心代码：

> 代码位于 `include/xos/task.h`

```c
// 任务睡眠一段时间
void task_sleep(u32 ms);

// 唤醒任务
void task_wakeup();
```

## 2. 代码分析

### 2.1 睡眠队列

我们定义睡眠队列，来记录当前处于睡眠状态的任务。同时引入全局时间片 `jiffies` 和一个时间片对应的毫秒数 `jiffy`，为后续计算睡眠时间对应的时间片做准备。

> 代码位于 `kernel/task.c`

```c
// 全局时间片
extern u32 volatile jiffies;

// 一个时间片对应的毫秒数
extern const u32 jiffy;

// 睡眠任务队列
static list_t sleeping_queue;
```

在 `task_init()` 处初始化睡眠队列（因为睡眠队列内部是由链表实现的）。

```c
// 初始化任务管理
void task_init() {
    ...
    list_init(&sleeping_queue);
    ...
}
```

### 2.2 任务睡眠

> **在本节中，我们将 PCB 中的 `ticks` 视为睡眠结束的全局时间片。**

任务睡眠有以下步骤（与任务阻塞不同，只有当前任务才可以主动进行睡眠）：

1. 将睡眠时间转换成时间片数量
2. 计算并记录唤醒时的全局时间片
3. 插入睡眠队列并设置为睡眠状态
4. 进行任务调度

> 代码位于 `kernel/task.c`

```c
// 任务睡眠一段时间
void task_sleep(u32 ms) {
    // 涉及睡眠队列这个临界区
    ASSERT_IRQ_DISABLE();

    // 睡眠时间不能为 0
    assert(ms > 0);

    // 睡眠的时间片数量向上取整
    u32 ticks = div_round_up(ms, jiffy);

    // 记录睡眠结束时的全局时间片，因为在那个时刻应该要唤醒任务
    task_t *current = current_task();
    current->ticks = jiffies + ticks;

    // 从睡眠链表中找到第一个比当前任务唤醒时间点更晚的任务，进行插入排序
    list_t *list = &sleeping_queue;

    list_node_t *anchor = &list->tail;
    for (list_node_t *ptr = list->head.next; ptr != &list->tail; ptr = ptr->next) {
        task_t *task = element_entry(task_t, node, ptr);
        
        if (task->ticks > current->ticks) {
            anchor = ptr;
            break;
        }
    }

    // 保证当前任务没有位于任何阻塞 / 睡眠队列当中
    ASSERT_NODE_FREE(&current->node);

    // 插入链表
    list_insert_before(anchor, &current->node);

    // 设置阻塞状态为睡眠
    current->state = TASK_SLEEPING;

    // 调度执行其它任务
    schedule();
}
```

将任务插入睡眠队列时，我们采用了插入排序，这是为了方便后续的任务唤醒实现。

> Update 2024/2/11
> ---
> 可以直接使用 list 库中的 `list_insert_sort()` 搭配宏 `list_node_offset` 来实现插入排序。 

### 2.3 任务唤醒

任务唤醒的主要内容为：扫描睡眠队列，将在当前全局时间片下，所有已经结束睡眠的任务，都进行唤醒，即设置为就绪状态，从睡眠队列中删除（这部分逻辑我们通过调用 `task_unblock` 来实现）。

> 代码位于 `kernel/task.c`

```c
// 唤醒任务
void task_wakeup() {
    // 涉及睡眠队列这个临界区
    ASSERT_IRQ_DISABLE();

    // 从睡眠队列中找到任务成员 ticks 小于或等于全局当前时间片 jiffies 的任务
    // 结束这些任务的阻塞 / 睡眠，进入就绪状态
    list_t *list = &sleeping_queue;

    for (list_node_t *ptr = list->head.next; ptr != &list->tail;) {
        task_t *task = element_entry(task_t, node, ptr);

        if (task->ticks > jiffies) {
            break;
        }

        // task_unblock() 会清空链表节点的 prev 和 next 指针，所以需要事先保存
        ptr = ptr->next;

        task_unblock(task);
    }
}
```

这里需要注意的是，`task_unblock()` 会清空链表节点的 `prev` 和 `next` 指针，所以需要事先保存 `ptr` 的下一个指针 `ptr->next`，以继续进行链表遍历。

### 2.4 时钟中断

与蜂鸣器类似，我们需要在每次触发时钟中断时，都进行任务唤醒，这样才能保证任务在正确的时刻被唤醒。

> **所有涉及时间的功能，都绕不开时钟中断，这是因为只有在时钟中断里才会更新全局时间片，也就是说，只有时钟中断才掌管了系统视角里的时间流动概念。**

> 代码位于 `kernel/clock.c`

```c
// 时钟中断处理函数
void clock_handler(int vector) {
    ...
    // 唤醒睡眠结束的任务
    task_wakeup();
    ...
}
```

## 3. 系统调用

现在我们将利用 `task_sleep()` 来实现系统调用 `sleep()`。系统调用 `sleep()` 的函数原型如下：

```c
// @param ms 任务睡眠的时间（以毫秒为单位）
void sleep(u32 ms);
```

### 3.1 系统调用入口

由以上函数原型可知，系统调用 `sleep()` 有一个参数，所以需要一个新的入口封装。按照系统调用约定，需要将第一个参数存入寄存器 `ebx` 中。

> 代码位于 `lib/syscall.c`

```c
// _syscall1 表示封装有 1 个参数的系统调用
static _inline u32 _syscall1(u32 sys_num, u32 arg) {
    u32 ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)
        : "a"(sys_num), "b"(arg)
    );
    return ret;
}
```

利用 `_syscall1` 将系统调用 `sleep()` 的入口 / 触发封装为函数形式：

```c
void sleep(u32 ms) {
    _syscall1(SYS_SLEEP, ms);
}
```

### 3.2 系统调用处理

根据 [<047 系统调用>][047_syscall] 中所述的系统调用处理流程，我们通过调用 `task_sleep()` 来实现系统调用 `sleep()` 的处理函数。

> 代码位于 `kernel/task.c`

```c
// 系统调用 sleep 的处理函数
void sys_sleep(u32 ms) {
    task_sleep(ms);
}
```

并在系统调用初始化时，将 `sys_sleep()` 添加到系统调用处理列表对应的项中。

> 代码位于 `kernel/syscall.c`

```c
// 初始化系统调用
void syscall_init() {
    ...
    syscall_table[SYS_SLEEP] = sys_sleep;
    ...
}
```

## 4. 功能测试

新建一个任务 `test`，并修改任务 `init`，使得这两个任务按不同的时间间隔进行睡眠。


> 代码位于 `kernel/thread.c`

```c
void init_thread() {
    irq_enable();
    u32 counter = 0;

    while (true) {
        LOGK("init task %d...\n", counter++);
        sleep(500);
    }
}

void test_thread() {
    irq_enable();
    u32 counter = 0;

    while (true) {
        LOGK("test task %d...\n", counter++);
        sleep(800);
    }
}
```

任务 `init` 每次睡眠 500 ms，而任务 `test` 每次睡眠 800 ms。

> 代码位于 `kernel/task.c`

```c
// 初始化任务管理
void task_init() {
    ...
    idle_task = task_create((target_t)idle_thread, "idle", 1, KERNEL_TASK);
    task_create((target_t)init_thread, "init", 5, USER_TASK);
    task_create((target_t)test_thread, "test", 5, KERNEL_TASK);
}
```

因为在本节，我们将 PCB 中的 `ticks` 视为睡眠结束的全局时间片。所以我们不考虑任务 `init` 和 `test` 的成员 `ticks` 作为剩余时间片，而是将它视为睡眠结束时间片（这两个任务在本节无需考虑时钟中断）。但是对于 `setup` 和 `idle` 任务来说，`ticks` 仍然作为剩余时间片来使用，用于在时钟中断时进行处理（例如，进行任务调度）。

由于我们本节使用 `ticks` 作为其它用途。导致了使用 `sleep()` 的任务无法正常进入时钟中断处理。所以本节实现的系统调用 `sleep()` 是个试验性质的实现，我们后续会通过改进 `task_sleep()` 和 `task_wakeup()` 的实现，来完成系统调用 `sleep()` 的实现。

---

搭建测试框架：

> 代码位于 `kernel/main.c`

```c
void kernel_init() {
    ...
    interrupt_init();
    clock_init();
    task_init();
    syscall_init();

    irq_enable(); // 打开外中断响应

    hang();
    return;
}
```

预期为，按照各个任务睡眠的时间间隔，打印 `"init task %d...“` 或 `"test task %d..."`。


[047_syscall]: ../08_syscall/047_syscall.md
