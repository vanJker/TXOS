# 034 中断上下文

## 1. 原理说明

### 1.1 省略栈帧

由于不省略栈帧，会使得中断切换上下文发生一些小 bug，所以使用以下选项来省略栈帧：

```c++
// 用于省略函数的栈帧
#define _ofp __attribute__((optimize("omit-frame-pointer")))
```

### 1.2 中断上下文

在中断上下文中，保存 / 恢复的寄存器信息（或者说，栈的结构）如下：

![](./images/interrupt_context.drawio.svg)

在栈的结构中，`vector0` 和 `vector` 是一样的，之所以要重复压入，是为了满足函数调用的参数顺序需要。

中断上下文和之前的任务上下文相比较，要在栈中保存 / 恢复的信息更多。这是因为，先前的任务上下文切换（协作式调度），在调用 `task_switch()` 时，编译器在编译时会自动补充保存 **函数调用者保存** 的寄存器的指令。但是中断上下文切换（抢占式调度），并不会编译补充这些指令，所以需要我们在中断函数里面，保存所有的通用寄存器。

再者，和函数调用与中断触发的区别类似，在中断函数里面，我们也需要保存所有的 **段寄存器**（这里面也有不同进程使用不同的段寄存器值的考虑）。

**注意：中断上下文，即进入 / 退出中断处理函数，都必须进行相对应的保存 / 恢复上下文信息，使得中断函数的功能对正常的指令流是“透明的”。**

![](./images/interrupt_context_switch.drawio.svg)

### 1.3 打开中断

在 Task A 和 B 的函数体中插入 `sti` 指令，而不是在中断函数恢复上下文信息之后，打开中断。

这是因为，Task A 和 B 的初始状态是在函数体里面（即首次切换会直接切换到函数体执行，而无需经过中断函数恢复上下文信息，再跳转回函数体），而不是在中断函数。

如果在中断函数末尾加入 `sti` 指令，那么第一次触发中断从 Task A 切入 Task B 时，中断关闭后（触发中断会自动将 `IF` 位置 0）没有打开，会一直在 B 的函数体中执行（首次切换到 B 无需经过中断函数）。

## 2. 代码分析

### 2.1 保存 / 恢复中断上下文

```x86asm
/* kernel/handler.asm */

interrupt_entry:
    ; 保存上文寄存器信息
    push ds
    push es
    push fs
    push gs
    pusha

    ; 获取之前 push %1 保存的中断向量
    mov eax, [esp + 12 * 4]

    ; 向中断处理函数传递参数
    push eax

    ; 调用中断处理函数，handler_table 中存储了中断处理函数的指针
    call [handler_table + eax * 4]

    ; 对应先前的 push eax，调用结束后恢复栈
    add esp, 4

    ; 恢复下文寄存器信息
    popa
    pop gs
    pop fs
    pop es
    pop ds

    ; 对应 push %1
    ; 对应 error code 或 magic
    add esp, 8

    iret
```

### 2.2 时钟中断处理函数

由于目前我们只接受时钟中断（其它类型中断在 PIC 处设置屏蔽了），所以中断处理函数即为，时钟中断的处理函数，我们在这里实现抢占式调度：

```c
/* kernel/interrupt.c */

void default_handler(int vector) {
    send_eoi(vector);
    schedule(); // 任务调度
}
```

### 2.3 异常处理函数

根据我们在这一节所定义的，中断触发时栈的结构，我们可以改进异常处理函数（借助函数参数传递约定），使得其能打印更多信息：

```c
/* kernel/interrupt.c */

void exception_handler(
    int vector,
    u32 edi, u32 esi, u32 ebp, u32 esp,
    u32 ebx, u32 edx, u32 ecx, u32 eax,
    u32 gs, u32 fs, u32 es, u32 ds,
    u32 vector0, u32 error, u32 eip, u32 cs, u32 eflags) {
    char *message = NULL;
    if (vector < 0x16) {
        message = messages[vector];
    } else {
        message = messages[0x0f]; // 输出reversed 信息
    }

    printk("Exception: [0x%02x] %s \n", vector, message);
    printk("\nException : %s \n", message);
    printk("  VECTOR : 0x%02X\n", vector);
    printk("   ERROR : 0x%08X\n", error);
    printk("  EFLAGS : 0x%08X\n", eflags);
    printk("      CS : 0x%02X\n", cs);
    printk("     EIP : 0x%08X\n", eip);
    printk("     ESP : 0x%08X\n", esp);

    // 阻塞
    hang();
}
```

### 2.4 任务函数体

```c
/* kernel/interrupt.c */

u32 _ofp thread_a() {
    asm volatile("sti"); // 打开中断

    while (true) {
        printk("A");
    }
}

u32 _ofp thread_b() {
    asm volatile("sti"); // 打开中断

    while (true) {
        printk("B");
    }
}
```

## 3. 测试

### 3.1 抢占式调度

```c
/* kernel/main.c */

#include <xos/task.h>

void kernel_init() {
    console_init();
    gdt_init();
    interrupt_init();
    task_init();

    return;
}
```

预期为，一连串的 A 和 一连串的 B 交替出现，实现定时任务抢占式调度。

### 3.2 异常信息

```x86asm
/* kernel/start.asm */

_start:
    call kernel_init
    xchg bx, bx ; Bochs Magic Breakpoint

    ; 0x80 系统调用，会触发一般性保护异常
    int 0x80

    ; 除零异常
    mov bx, 0
    div bx

    jmp $ ; 阻塞
```

预期为，改进后的异常处理函数，在异常触发时会打印更多的信息。

## 4. 参考文献

- <https://gcc.gnu.org/onlinedocs/gcc-4.7.0/gcc/Function-Attributes.html>
- <https://pdos.csail.mit.edu/6.828/2018/readings/i386/PUSHA.htm>