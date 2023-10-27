# 063 进入用户模式

## 1. 任务特权级环

![](./images/Privilege_Level.drawio.svg)

## 2. 任务状态段

我们主要关心 TSS 的以下两个部分，因为我们需要使用它们来进入用户模式。

- `ss0` - 内核栈段选择子
- `esp0` - 内核栈顶指针

## 3. 中断门处理过程

如果处理程序运行在 **低特权级** 的时候，触发了中断，那么 **栈切换** 就会发生：

- **内核特权级** 的 **栈段选择子** 和 **栈顶指针** 将会从 **当前的 TSS** 中获得，将栈切换至内核栈，并在 **内核栈** 中将会压入 **用户态** 的 **栈段选择子** 和 **栈顶指针**。
- 保存当前的状态 `eflags`, `cs`, `eip` 到内核栈
- 如果存在错误码的话，压入 错误码

如果处理器运行在 **相同的特权级**，那么相同特权级的中断代码将被执行：

- 保存当前的状态 `eflags`, `cs`, `eip` 到内核栈
- 如果存在错误码的话，压入 错误码

> 是否为低特权级由中断描述符中的 `DPL` 来判断。如果触发中断时，当前处理器的特权级（由 `cr0` 寄存器的 `CPL` 来指定）比对应中断描述符的 `DPL` 低，即是低特权级触发中断，其它情况类似。

由上面可知，在低特权级触发中断会比在相同特权级下触发中断，会进行栈切换，切换到内核栈，并多压入用户态的栈段选择子以及用户态的栈顶指针。

中断返回的流程与上面恰好相反，不再赘述。

> 可以查阅 [<029 中断函数>](../04_interrupt_and_clock/029_interrupt_function.md) 和 [<034 中断上下文>](../04_interrupt_and_clock/034_interrupt_context.md) 来了解中断门的处理流程。

## 4. 进入用户模式

- 内核栈 Return Oriented Programming (ROP)
- 用户栈
- 中断返回

![](./images/interrupt_context.drawio.svg)

> 关于 `ROP` 技术，感兴趣的可以去了解一下 **漏洞利用**。

## 5. 代码分析

### 5.1 中断帧

根据原理说明，定义在低特权级下触发中断的 **中断帧** 的结构：

> 代码位于 `include/xos/interrupt.h`

```c
// 中断帧（在低特权级发生中断）
typedef struct intr_frame_t {
    // 中断向量号
    u32 vector;

    // 通用寄存器
    u32 edi;
    u32 esi;
    u32 ebp;
    u32 esp; // 虽然 pusha 会把 esp 也压入，但是 esp 在不断变化，所以在 popa 时被忽略
    u32 ebx;
    u32 edx;
    u32 ecx;
    u32 eax;

    // 段寄存器
    u32 gs;
    u32 fs;
    u32 es;
    u32 ds;

    // 错误码和中断向量号
    u32 vector0;
    u32 error; // 错误码或魔数（用于填充）

    // 中断压入
    u32 eip;
    u32 cs;
    u32 eflags;
    u32 esp3; // 低特权级（一般是 ring 3）的栈段选择子
    u32 ss3;  // 低特权级（一般是 ring 3）的栈顶指针
} intr_frame_t;
```

### 5.2 进入用户态

根据原理说明以及之前定义的中断帧，为了从内核态进入用户态，我们需要使用 ROP 技术来为内核任务虚拟出一个中断帧。这样我们就可以通过中断返回，来进入我们想要进入的用户程序了。

> 代码位于 `kernel/task.c`

```c
// 切换到用户模式的任务执行
// 注意：该函数只能在函数体末尾被调用，因为它会修改栈内容，从而影响调用函数中局部变量的使用，而且这个函数不会返回。
static void real_task_to_user_mode(target_t target) {
    task_t *current = current_task();

    // 内核栈的最高有效地址
    u32 addr = (u32)current + PAGE_SIZE;

    // 在内核栈中构造中断帧（低特权级发生中断）
    addr -= sizeof(intr_frame_t);
    intr_frame_t *iframe = (intr_frame_t *)addr;

    iframe->vector = 0x20;

    iframe->edi = 1;
    iframe->esi = 2;
    iframe->ebp = 3;
    iframe->esp = 4;
    iframe->ebx = 5;
    iframe->edx = 6;
    iframe->ecx = 7;
    iframe->eax = 8;

    iframe->gs = 0;
    iframe->fs = USER_DATA_SELECTOR;
    iframe->es = USER_DATA_SELECTOR;
    iframe->ds = USER_DATA_SELECTOR;

    iframe->vector0 = iframe->vector;
    iframe->error = 0x20231024; // 魔数

    iframe->eip = (u32)target;
    iframe->cs = USER_CODE_SELECTOR;
    iframe->eflags = (0 << 12 | 1 << 9 | 1 << 1); // 非 NT | IOPL = 0 | 中断使能

    u32 stack3 = kalloc(1); // 用户栈 TODO: use user alloc instead
    iframe->ss3 = USER_DATA_SELECTOR;
    iframe->esp3 = stack3 + PAGE_SIZE; // 栈从高地址向低地址生长

    asm volatile(
        "movl %0, %%esp\n"
        "jmp interrupt_exit\n"
        ::"m"(iframe)
    );
}
```

这个函数的主要作用是，为我们的内核任务虚拟出一个 **”先前触发中断的低特权级任务“**，通过配置中断帧的 `eip`, `cs` 部分，以及 `ss3`, `esp3` 部分，可以配置这个低特权级任务触发中断的位置，该低特权级任务的栈顶位置，以及最重要的，该任务的特权级（使用 `USER` 段选择子可以在中断返回时进入用户态）。

同时注意，我们也虚拟该低特权级任务触发中断的中断向量号为 `0x20`，即时钟中断。这是为了简单起见，实际上你可以指定中断向量号为任意有效的中断向量号。

我们虚拟低特权级任务的通用寄存器的值，为了后续调试、测试的便利，我们将它们设置为一些简单的数值。

同时任务需要一个用户栈，我们目前使用 `kalloc()` 来分配了一页作为任务的用户态栈。当然这里应该使用用户空间的内存分配方法来进行分配，这个我们后续会进行实现。

最后将 `esp` 设置为中断帧的位置，然后通过 `interrupt_exit` 来进行中断上下文的恢复，以及中断返回。

当然，为了可以进入 `interrupt_exit`，我们需要声明一下：

> 代码位于 `kernel/handler.asm`

```x86asm
global interrupt_exit
interrupt_exit:
    ...
```

> **注意：**
> ---
> - **这个函数不会进行函数返回，因为它最后通过 `interrupt_exit` 进行了中断返回，所以调用这个函数时需要将调用逻辑放置在函数体的尾部（不放置在函数体尾部也可以，但是该函数调用后的逻辑均无法被执行）。**
> 
> - **我们将中断帧放置在内核栈的最高位置，这是为了在中断返回后，让内核栈为空。这是有考量的，因为每一次低特权级任务触发中断（比如系统调用），与先前它触发的中断没有任何联系（比如这一次的系统调用，需要用到上一次的系统调用产生的局部变量吗？显然是不需要的），所以保证内核栈为空是正确的。但这也引来了一个问题，因为是直接栈顶部分，所以可能会导致函数的局部变量的值被修改，我们接下来就解决最高问题。**

### 5.3 准备缓冲区

为了避免上文所提到的，函数局部变量的值被修改这种情况，我们需要保证在 `read_task_to_user_mode()` 函数中的局部变量，它们的地址均不位于内核栈顶中断帧区域内。

因为中断帧大小为 80 个字节，所以我们只需要在内核栈中，分配至少 80 个字节的缓冲区（缓冲区的数据，在配置中断帧时，可能会被修改），即可保证后续声明的局部变量，不会位于中断帧区域内。

```c
// 切换到用户模式
// 本函数用于为 real_task_to_user_mode() 准备足够的栈空间，以方便 real.. 函数使用局部变量。
// 注意：该函数只能在函数体末尾被调用，因为该函数也不会返回。
void task_to_user_mode(target_t target) {
    u8 temp[100]; // sizeof(intr_frame_t) == 80，所以至少准备 80 个字节的栈空间。
    real_task_to_user_mode(target);
}
```

下面是一个图示：

![](./images/intr_frame.drawio.svg)

### 5.4 配置 TSS

每一个低特权级任务均有对应的用户栈和内核栈，当一个低特权级用户触发中断时，需要切换到其自身的内核栈，而不是其它任务的内核栈。

所以，在任务调度时，如果下一个任务是用户态的话，需要配置 TSS 中的 `esp0` 部分的值，以使得该低特权级用户，在触发中断时，正确切换到它对应的内核栈当中。

```c
// 任务状态段
extern tss_t tss;

// 将 tss 的 esp0 激活为所给任务的内核栈顶
// 如果下一个任务是用户态的任务，需要将 tss 的栈顶位置修改为该任务对应的内核栈顶
void task_tss_activate(task_t *task) {
    assert(task->magic == XOS_MAGIC);   // 检测栈溢出
    
    if (task->uid != KERNEL_TASK) {
        // 如果任务不是内核任务
        tss.esp0 = (u32)task + PAGE_SIZE;
    }
}

// 任务调度
void schedule() {
    ...

    task_tss_activate(next);

    task_switch(next);
}
```

## 6. 功能测试

### 6.1 测试框架

在 `kernel/thread.c` 中给任务 `init` 增加用户线程 `real_init_thread`：

```c
// 初始化任务 init（用户线程）
static void real_init_thread() {
    while (true) {
    }
}

// 初始化任务 init（内核线程）
void init_thread() {
    task_to_user_mode((target_t)real_init_thread);
}
```

其中 `init` 的内核线程 `init_thread` 的作用就是将该任务切换到用户态执行。

### 6.2 栈帧变化

分别在 `task_to_user_mode()`，`real_task_to_user_mode()` 和 `interrupt_exit` 处打断点，并观察在这个过程当中栈的变化，是否符合预期。

### 6.3 段寄存器

在用户线程 `real_init_thread()` 部分加入 `BMB` 断点，并使用 Bochs 进行测试。

```c
static void real_init_thread() {
    while (true) {
        asm volatile("xchgw %bx, %bx");
    }
}
```

在 `BMB` 断点处，我们通过 Bochs 查看此时段寄存器的值，均位于用户态（`gs` 寄存器除外），即 Ring 3。

> 注：这里我们不使用 `BMB` 宏，而是直接使用内联汇编。这是因为我们的 `BMB` 宏掉眼泪 `printk`，进而使用了 I/O 指令，而我们此时位于用户态，且 `eflags` 的 `IOPL` 为 0，所以直接使用 `BMB` 宏会触发异常，我们接下来就讲解这个。

### 6.4 I/O 指令

在用户线程 `real_init_thread()` 部分尝试使用 I/O 指令：

```c
static void real_init_thread() {
    while (true) {
        asm volatile("in $0x92, %ax");
    }
}
```

因为此时处于 Ring 3，且 `eflags` 中的 `IOPL` 为 0，所以在 I/O 指令处会触发 GP 异常。

### 6.5 系统调用

由于系统调用和中断处理，共用入口逻辑和结束逻辑，所以系统调用机制可以正常运行。在 `syscall_entry` 处打断点，观察系统调用过程中，栈帧的变化。

```c
static void real_init_thread() {
    while (true) {
        sleep(100);
    }
}
```

> 在系统调用过程中，进入内核态后，只有 `cs` 寄存器中的选择子，切换至内核特权级选择子。当然现在在用户态也可以修改内存的值，所以其它段寄存器没有切换至内核态选择子，目前来说可以接受。但是为了安全性和隔离性，后续我们需要将内核态和用户态进行隔离。

## 7. 参考文献

- <https://wiki.osdev.org/TSS>
- [Intel® 64 and IA-32 Architectures Software Developer's Manual Volume 3 Chapter 6 Interrupt and Exception Handling](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)
- [郑刚 / 操作系统真象还原 / 人民邮电出版社 / 2016](https://book.douban.com/subject/26745156/)