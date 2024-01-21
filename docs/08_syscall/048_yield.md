# 048 系统调用 yield

系统调用 `yield()` - 进程通过这个系统调用主动交出执行权，调度到其它进程执行。这是实现协作式调度的核心系统调用。

## 1. syscall 封装

在 `lib/syscall.c` 中对系统调用进行封装。因为系统调用本质是通过汇编来触发的，而手动写汇编来触发过于繁琐，所以我们将 **触发软中断，进入系统调用入口** 封装为 C 语言中的函数，方便调用。

```c
// _syscall0 表示封装有 0 个参数的系统调用
static _inline u32 _syscall0(u32 sys_num) {
    u32 ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)
        : "a"(sys_num)
    );
    return ret;
}
```

- `"=a"(ret)` 为输出操作符，表示在汇编语句结束后，将 eax 寄存器的值存入变量 `ret` 中。
- `"a"(sys_num)` 为输入操作符，表示在汇编语句开始前，将变量 `ret` 的值存入 eax 寄存器中。

所以，这部分内联汇编对应的伪代码如下：

```x86asm
mov eax, [sys_num]
int 0x80
mov [ret], eax
```

---

我们封装一下 `test()` 系统调用：

```c
/* include/xos/syscall.h */
u32 test();

/* lib/syscall.c */
u32 test() {
    return _syscall0(SYS_TEST);
}
```

---

现在，我们可以直接通过函数调用来触发软中断了，不需要通过手写汇编来触发了。我们在 `kernel/main.c` 中试验一下。

```c
void kernel_init() {
    ....
    u32 ret = test();
    printk("ret: %d\n", ret);
    ...
}
```

预期为，打印 `"syscall test..."` 以及 `"ret: 255"` 这两行语句。

## 2. 代码分析

### 2.1 实现 yield

`yield()` 是主动放弃执行权，即主动进行调度，通过 `schedule()` 可以快速实现。

> 代码位于 `kernel/task.c`

```c
// 系统调用 yield 的处理函数
void sys_yield() {
    task_yield();
}
```

其中我们使用 `task_yield()` 来封装 `schedule()`，这样使得 `schedule()` 会在中断门中被调用，需要保证进入 `schedule()` 时，外中断响应已被关闭。

> 代码位于 `kernel/task.c`

```c
void schedule() {
    assert(get_irq_state() == 0);
    ...
}

// 任务主动放弃执行权
void task_yield() {
    // 即主动调度到其它空闲任务执行
    schedule();
}
```

### 2.2 封装 yield

和封装 `test()` 类似，将系统调用 `yield()` 的触发逻辑，封装成函数形式。

```c
/* include/xos/syscall.h */
void yield();

/* lib/syscall.c */
void yield() {
    _syscall0(SYS_YIELD);
}
```

## 3. 功能测试

在 `kernel/main.c` 处搭建测试框架：

```c
void kernel_init() {
    ...
    task_init();
    syscall_init();

    irq_enable(); // 打开外中断响应

    hang();
    return;
}
```

在 `kernel/task.c` 中将内核线程 A, B, C 改为协作式调度（用于测试 `yield`）。

```c
u32 thread_a() {
    irq_enable();

    while (true) {
        printk("A");
        yield();
    }
}
...
```

预期为，每次打印的字符均不一致，例如 `ABACAC...` 这类的字符序列（这个字符序列取决于实现的调度算法）。

---

在 `yield()` 处打断点，并通过 C/C++ 插件的反汇编功能，查看对应的汇编代码，检查封装是否正确。

```c
/* lib/syscall.c */
void yield() {
    _syscall0(SYS_YIELD);
    // 在此处断点
}
```

在断点处查看反汇编，预期为，反汇编得到的代码应该类似于以下格式：

```x86asm
mov DWORD PTR [ebp-0x4], 0x1
mov eax, DWORD PTR [ebp-0x4]
int 0x80
mov DWORD PTR [ebp-0x4], eax
```
