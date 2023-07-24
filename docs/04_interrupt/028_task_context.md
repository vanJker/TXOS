# 028 任务及上下文

## 1. 任务

任务就是进程或者线程，协程，就是一个执行流；

- 程序入口地址
- 堆栈 / 内核栈
- 寄存器信息

## 2. ABI 调用约定

Application Binary Interface

System V ABI

调用方保存（即调用完成后寄存器值可能会变化）：

- eax
- ecx
- edx

实现方保存（即调用完成后寄存器值不变）：

- ebx
- esi
- edi
- ebp
- esp

## 3. 内存分页

4G / 4K = 1M

32 位机器内存大小为 $2^{32} = 4G Bytes$，而我们使用 4K Bytes 作为一页，所以总共可以分成 1M 页。

## 4. 任务内存分布

![](../images/28-1.svg)

我们使用内核栈来保存任务的上下文，所以在 PCB (Process Control Block) 中只需要保存 esp 寄存器值就可以获得全部的任务上下文信息。

又因为现在我们只需在 PCB 中获得任务上下文，所以现在，我们的 PCB 只需保存一个内核栈地址即可（任务状态，文件描述符数量那些以后会添加）。

## 5. 代码分析

### 5.1 声明任务内存分布结构

```c
/* include/xos/task.h */

// 类似指针，即指令的地址
typedef u32 target_t;

// PCB
typedef struct task_t {
    u32 *stack; // 内核栈
} task_t;

// 内核栈帧
typedef struct task_frame_t {
    u32 edi;
    u32 esi;
    u32 ebx;
    u32 ebp;
    void (*eip)(void); // 函数指针形式
} task_frame_t;

void task_init();
```

注意与任务内存分布图结合，来理解任务内核栈帧中各个寄存器所存放的位置，这个在后续的汇编程序 task_switch 中会使用到。

### 5.2 任务切换

```c
extern void task_switch(task_t *next);
```

```x86asm
/* kernel/schedule.asm */

global task_switch
task_switch:
    push ebp
    mov ebp, esp

    push ebx
    push esi
    push edi

    mov eax, esp
    and eax, 0xfffff000 ; 获得 current task

    mov [eax], esp

    mov eax, [ebp + 8] ; 获得函数参数 next
    mov esp, [eax] ; 切换内核栈

    pop edi
    pop esi
    pop ebx
    pop ebp

    ret
```

任务切换时，高级语言在函数调用时，会自动保存那些 ABI 中规定调用方该保存的寄存器值（即编译后会加上这些保存寄存器值的指令）。所以我们在这汇编程序 `task_switch` 需要自行保存那些实现方应当保存的寄存器值，以保存完整的任务上下文。

在切换内核栈后，结合上面的任务内存分布图以及 [<019 函数调用约定>](../02_binary_basics/019_function_calling_convention.md) 来思考是如何实现跳转到另一任务的指令流中继续执行的。

下面是函数调用返回和任务切换的对比示意图：

![](../images/28-2.svg)

### 5.3 任务相关操作

```c
/* kernel/task.c */

// 页大小
#define PAGE_SIZE 0x1000

// 两个内核任务
task_t *a = (task_t *)0x1000;
task_t *b = (task_t *)0x2000;

extern void task_switch(task_t *next);

// 获得当前任务
task_t *current_task() {
    asm volatile(
        "movl %esp, %eax\n"
        "andl $0xfffff000, %eax\n"
    );
}

// 任务调度
void schedule() {
    task_t *current = current_task();
    task_t *next = (current == a ? b : a);
    task_switch(next);
}

u32 thread_a() {
    while (true) {
        printk("A");
        schedule();
    }
}

u32 thread_b() {
    while (true) {
        printk("B");
        schedule();
    }
}

static void task_create(task_t *task, target_t target) {
    u32 stack = (u32)task + PAGE_SIZE;

    stack -= sizeof(task_frame_t);
    task_frame_t *frame = (task_frame_t *)stack;
    // 以下的寄存器值仅作为调试需要 
    frame->ebx = 0x11111111;
    frame->esi = 0x22222222;
    frame->edi = 0x33333333;
    frame->ebp = 0x44444444;
    frame->eip = (void *)target;

    task->stack = (u32 *)stack;
}

void task_init() {
    task_create(a, (target_t)thread_a);
    task_create(b, (target_t)thread_b);
    schedule();
}
```

我们把任务 A 的内存放置地址 0x1000 处，那里原本是 loader，但是我们已经在上一节中把 GDT 移动到了内核空间，所以现在已经不需要 loader 了，可以重复使用那里的内存空间。

这里我们主要是创建了两个内核任务 A 和 B，来观察任务的协作式调度。注意的是，这里一个有 3 个任务，一个是从进入内核以来的任务（即从 boot 到 loader 到 kernel 的那个任务，我们称之为 Task K），另外两个显然是任务 A 和 任务 B。当然在进入 A 和 B 协作式调度后，我们就不会再回到原先的那个内核任务中了，详情看下面的调式观察部分。

`schedule()` 处的逻辑是，如果当前任务不是 A 就切换到 A，所以 Task K 调用该函数可以切换到 Task A。

## 4. 调试观察

```c
/* kernel/main.c */
...
#include <xos/task.h>

void kernel_init() {
    console_init();
    gdt_init();
    task_init();

    return;
}
```

- 在 `task_init()` 中的 `schedule()` 处设置断点，可以观察此时 Task K, Task A 和 Task B 的内核栈的内容。
- 在 Task A 和 Task B 相互切换过程中，可以观察它们内核栈的内容，以及是如何切换内核栈的。
- 对照生成的符号表，查询内核栈中返回地址对应的符号（主要查询初始任务所对应的返回地址是不是 thread_a / thread_b）。

## 5. 参考文献

- <https://en.wikipedia.org/wiki/Application_binary_interface>
- <https://stackoverflow.com/questions/2171177/what-is-an-application-binary-interface-abi>
- <https://wiki.osdev.org/System_V_ABI>