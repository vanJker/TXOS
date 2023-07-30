# 031 异常

- 故障
- 陷阱
- 终止

## 1. 故障 Fault

这种错误是可以被修复的一种类型，属于最轻的一种异常。

## 2. 陷阱 Trap

此异常通常用于调试。

## 3. 终止 Abort

这是最严重的异常类型，一旦出现，则会由于错误无法修复，程序将无法继续运行。

## 4. 异常列表

| 编号              | 名称           | 类型      | 助记符  | 错误码    |
| ----------------- | -------------- | --------- | ------- | --------- |
| 0 (0x0)           | 除零错误       | 故障      | #DE     | 无        |
| 1 (0x1)           | 调试           | 故障/陷阱 | #DB     | 无        |
| 2 (0x2)           | 不可屏蔽中断   | 中断      | -       | 无        |
| 3 (0x3)           | 断点           | 陷阱      | #BP     | 无        |
| 4 (0x4)           | 溢出           | 陷阱      | #OF     | 无        |
| 5 (0x5)           | 越界           | 故障      | #BR     | 无        |
| 6 (0x6)           | 指令无效       | 故障      | #UD     | 无        |
| 7 (0x7)           | 设备不可用     | 故障      | #NM     | 无        |
| 8 (0x8)           | 双重错误       | 终止      | #DF     | 有 (Zero) |
| 9 (0x9)           | 协处理器段超限 | 故障      | -       | 无        |
| 10 (0xA)          | 无效任务状态段 | 故障      | #TS     | 有        |
| 11 (0xB)          | 段无效         | 故障      | #NP     | 有        |
| 12 (0xC)          | 栈段错误       | 故障      | #SS     | 有        |
| 13 (0xD)          | 一般性保护异常 | 故障      | #GP     | 有        |
| 14 (0xE)          | 缺页错误       | 故障      | #PF     | 有        |
| 15 (0xF)          | 保留           | -         | -       | 无        |
| 16 (0x10)         | 浮点异常       | 故障      | #MF     | 无        |
| 17 (0x11)         | 对齐检测       | 故障      | #AC     | 有        |
| 18 (0x12)         | 机器检测       | 终止      | #MC     | 无        |
| 19 (0x13)         | SIMD 浮点异常  | 故障      | #XM/#XF | 无        |
| 20 (0x14)         | 虚拟化异常     | 故障      | #VE     | 无        |
| 21 (0x15)         | 控制保护异常   | 故障      | #CP     | 有        |
| 22-31 (0x16-0x1F) | 保留           | -         | -       | 无        |

说明：

0. 当进行除以零的操作时产生
1. 当进行程序单步跟踪调试时，设置了标志寄存器 eflags 的 T 标志时产生这个中断
2. 由不可屏蔽中断 NMI 产生
3. 由断点指令 int3 产生，与 debug 处理相同
4. eflags 的溢出标志 OF 引起
5. 寻址到有效地址以外时引起
6. CPU 执行时发现一个无效的指令操作码
7. 设备不存在，指协处理器，在两种情况下会产生该中断：
    1. CPU 遇到一个转意指令并且 EM 置位时，在这种情况下处理程序应该模拟导致异常的指令
    2. MP 和 TS 都在置位状态时，CPU 遇到 WAIT 或一个转移指令。在这种情况下，处理程序在必要时应该更新协处理器的状态
8. 双故障出错
9.  协处理器段超出，只有 386 会产生此异常
10. CPU 切换时发觉 TSS 无效
11. 描述符所指的段不存在
12. 堆栈段不存在或寻址堆栈段越界
13. 没有符合保护机制（特权级）的操作引起
14. 页不在内存或不存在
15. 保留
16. 协处理器发出的出错信号引起
17. 对齐检测只在 CPL 3 执行，于 486 引入
18. 与模型相关，于奔腾处理器引入
19. 与浮点操作相关，于奔腾 3 引入
20. 只在可以设置 EPT - violation 的处理器上产生
21. ret, iret 等指令可能会产生该异常

> 最后一列表示为是否额外压入错误码。

## 5. 调试器

调试器的实现原理：

- 不能影响程序执行
- 可以在断点的地方停下来

## 6. GP(0x0D) 异常错误码

![](../images/31-1.svg)

| 索引  | 长度 | 名称              | 描述           |
| ----- | ---- | ----------------- | -------------- |
| E     | 1    | 外部(External)    | 异常由外部触发 |
| TBL   | 2    | IDT/GDT/LDT Table | 见下列表       |
| INDEX | 13   | 选择子索引        |

TBL:

- 00 GDT
- 01 IDT
- 10 LDT
- 11 IDT

解析一个 GP 异常错误码的例子：

```bash
0x402;
0b_0100_0000_0010
0b_10000000_01_0
```

## 7. 中断流程

为了编写代码的整洁和方便，本项目采用以下的中断流程：

```bash
int -> INTERRUPT_ENTRY_%n -> interrupt_entry -> 具体的中断处理函数 -> interrupt_enrty -> iret
```

![](../images/31-2.svg)

## 8. 代码分析

### 8.1 中断入口

```x86asm
/* kernel/handler.asm */
[bits 32]

; 中断处理函数入口
extern handler_table

section .text

; 用于生产中断入口函数的宏
%macro INTERRUPT_ENTRY 2
interrupt_handler_%1:
%ifn %2
    ; 如果没有压入错误码，则压入该数作为占位符以保证栈结构相同
    push 0x20230730 
%endif
    push %1 ; 压入中断向量，跳转到中断入口
    jmp interrupt_entry
%endmacro

interrupt_entry:
    mov eax, [esp] ; 获取中断向量
    ; 调用中断处理函数，handler_table 中存储了中断处理函数的指针
    call [handler_table + eax * 4]
    ; 对应先前的 push，调用结束后恢复栈
    add esp, 8
    iret
```

`INTERRUPT_ENTRY` 宏是用于生成中断入口函数的，而 `interrupt_entry` 则是统一中断入口，在这里根据中断向量跳转到具体的中断处理函数进行处理（通过 `handler_table` 这个数组进行分发）。

nasm 的宏定义格式：

```x86asm
%macro 宏名 参数个数
宏体
%endmacro

; 在宏体中使用 %1 代表第一个参数，%2 代表第二个参数，以此类推
```

在跳转到 `interrupt_entry` 之前，栈的结构如下：

```bash
|------------|
|   eflags   |
|------------|
|     cs     |
|------------|
|    eip     |
|------------|
|   errno    |
|     or     |
| 0x20230730 |
|------------|
|   vector   |
|------------| <- sp
```

生成所需要的异常入口，以及归并到一个入口表中，便于后续的中断初始化：

```x86asm
INTERRUPT_ENTRY 0x00, 0 ; divide by zero
...


section .data

; 下面的数组记录了每个中断入口函数的指针
global handler_entry_table
handler_entry_table:
    dd interrupt_entry_0x00
    ...
```

### 8.2 中断处理

根据之前说明的中断流程，我们现在不仅需要初始化 GDT，同时也需要初始化中断处理函数表：

```c
/* kernel/interrupt.c */

#define EXCEPTION_SIZE 0x20 // 异常数量

handler_t handler_table[IDT_SIZE];                    // 中断处理函数表
extern handler_t handler_entry_table[EXCEPTION_SIZE]; // 中断入口函数表

void interrupt_init() {
    // 初始化中断描述符表
    for (size_t i = 0; i < EXCEPTION_SIZE; i++) {
        gate_t *gate = &idt[i];
        handler_t handler = handler_entry_table[i];

        gate->offset_low = (u32)handler & 0xffff;
        gate->offset_high = ((u32)handler >> 16) & 0xffff;
        gate->selector = 1 << 3; // 1 号段为代码段
        gate->reserved = 0;      // 保留不用
        gate->type = 0b1110;     // 中断门
        gate->segment = 0;       // 系统段
        gate->DPL = 0;           // 内核态权级
        gate->present = 1;       // 有效位
    }
    idt_ptr.base = (u32)idt;
    idt_ptr.limit = sizeof(idt) - 1;
    asm volatile("lidt idt_ptr");

    // 初始化异常处理函数表
    for (size_t i = 0; i < 0x20; i++) {
        handler_table[i] = exception_handler;
    }
}
```

具体的异常处理函数：

```c
// 中断输出的错误信息表
static char *messages[] = {
    [0x00] "#DE Divide Error",
    [0x01] "#DB RESERVED",
    [0x02] "--  NMI Interrupt",
    [0x03] "#BP Breakpoint",
    [0x04] "#OF Overflow",
    [0x05] "#BR BOUND Range Exceeded",
    [0x06] "#UD Invalid Opcode (Undefined Opcode)",
    [0x07] "#NM Device Not Available (No Math Coprocessor)",
    [0x08] "#DF Double Fault",
    [0x09] "    Coprocessor Segment Overrun (reserved)",
    [0x0a] "#TS Invalid TSS",
    [0x0b] "#NP Segment Not Present",
    [0x0c] "#SS Stack-Segment Fault",
    [0x0d] "#GP General Protection",
    [0x0e] "#PF Page Fault",
    [0x0f] "--  (Intel reserved. Do not use.)",
    [0x10] "#MF x87 FPU Floating-Point Error (Math Fault)",
    [0x11] "#AC Alignment Check",
    [0x12] "#MC Machine Check",
    [0x13] "#XF SIMD Floating-Point Exception",
    [0x14] "#VE Virtualization Exception",
    [0x15] "#CP Control Protection Exception",
};

void exception_handler(int vector) {
    char *message = NULL;
    if (vector < 0x16) {
        message = messages[vector];
    } else {
        message = messages[0x0f]; // 输出reversed 信息
    }

    printk("Exception: [0x%02x] %s \n", vector, message);

    // 阻塞
    while (true)
        ;
}
```

## 9. 调试测试

### 9.1 一般性保护异常

因为只初始化了前 0x20 个中断描述符，所以调用 0x80 系统调用，会触发一般性保护异常。

```x86asm
/* kernel/start.asm */

_start:
    call kernel_init
    xchg bx, bx ; Bochs Magic Breakpoint

    ; 0x80 系统调用，会触发一般性保护异常
    int 0x80

    jmp $ ; 阻塞
```

### 9.2 除零异常

```x86asm
/* kernel/start.asm */

_start:
    call kernel_init
    xchg bx, bx ; Bochs Magic Breakpoint

    ; 除零异常
    mov bx, 0
    div bx

    jmp $ ; 阻塞
```

## 10. 参考文献

- <https://wiki.osdev.org/Exceptions>
- Intel® 64 and IA-32 Architectures Software Developer's Manual, Volume 3 (System Programming Guide), Chapter 6 (Interrupt and exception handling)
- <https://en.wikipedia.org/wiki/Second_Level_Address_Translation>