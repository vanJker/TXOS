# 047 系统调用

## 1. 系统调用

系统调用，是用户程序与内核交互的一种方式。同时，通过系统调用，可以将 CPU 从用户态转向内核态。

x86 处理器是用 **中断门** 来实现系统调用的。为了与 Linux 兼容，我们也使用 `0x80` 号中断函数作为系统调用的入口。

系统调用的约定如下：

| 寄存器 | 功能 |
| :---: | :----------: |
| eax   | 存储系统调用号 |
| ebx   | 存储第一个参数 |
| ecx   | 存储第二个参数 |
| edx   | 存储第三个参数 |
| esi   | 存储第四个参数 |
| edi   | 存储第五个参数 |
| ebp   | 存储第六个参数 |
| eax   | 存储返回值    |

> 系统调用约定与函数调用约定不同，系统调用是通过寄存器传递参数，函数调用是通过栈传递参数。相同的是，系统调用和函数调用，都使用 eax 寄存器来存放返回值。

目前我们只实现有 3 个参数的系统调用，其系统调用的过程如下：

```x86asm
mov eax, 系统调用号
mov ebx, 第一个参数
mov ecx, 第二个参数
mov edx, 第三个参数
int 0x80
```

## 2. 调用流程

由于系统调用也是通过 **中断门** 实现的，所以我们可以复用之前中断处理流程的一些逻辑。下面是本节设计的系统调用流程，与之前的中断处理流程的对比示意图。

![](./images/syscall_path.drawio.svg)

观察示意图可以发现，我们在实现系统调用时，复用了一些中断处理逻辑，比如通过 `interrupt_exit` 来退出中断。为了保证使用复用逻辑的正确性，需要保证在进入复用逻辑之前，系统调用的栈结构与中断处理的栈结构相同，

以下为中断处理进入复用逻辑 `interrupt_exit` 之前的栈结构：

![](../04_interrupt_and_timer/images/interrupt_context.drawio.svg)

## 3. 代码分析

### 3.1 系统调用号

在 `include/xos/syscall.h` 中定义系统调用号相关的数据与功能。目前我们暂定实现 64 个系统调用，其中第 0 号系统调用为 `SYS_TEST`。

```c
// 共 64 个系统调用
#define SYSCALL_SIZE 64

typedef enum syscall_t {
    SYS_TEST = 0,
} syscall_t;

// 检测系统调用号是否合法
void syscall_check(u32 sys_num);

// 初始化系统调用
void syscall_init();
```

在 `kernel/syscall.c` 中的 `syscall_check()` 判断系统调用号是否合法。

```c
// 检测系统调用号是否合法
void syscall_check(u32 sys_num) {
    if (sys_num >= SYSCALL_SIZE) {
        panic("invalid syscall number!!!");
    }
}
```

### 3.2 系统调用处理

> 代码位于 `kernel/syscall.c`

依据原理说明处的流程图，实现两个处理函数：`sys_default()` 和 `sys_test()`，并在系统调用处理函数列表 `syscall_table` 中进行注册。

```c
// 系统调用处理函数列表
handler_t syscall_table[SYSCALL_SIZE];

// 默认系统调用处理函数
static void sys_default() {
    panic("syscall is not implemented!!!");
}

// 系统调用 test
static void sys_test() {
    LOGK("syscall test...\n");
}

// 初始化系统调用
void syscall_init() {
    for (size_t i = 0; i < SYSCALL_SIZE; i++) {
        syscall_table[i] = sys_default;
    }

    syscall_table[SYS_TEST] = sys_test;
}
```

### 3.3 系统调用入口

首先在 `kernel/handler.asm` 中分离出 `interrupt_exit` 逻辑（即 `interrupt_entry` 负责压栈保存，`interrupt_exit` 负责出栈恢复）。

```x86asm
interrupt_entry:
    ...
    ; 调用中断处理函数，handler_table 中存储了中断处理函数的指针
    call [handler_table + eax * 4]

interrupt_exit:
    ...
    iret
```

在保证与中断处理流程的栈结构相同的条件下，可以相对简单的实现系统调用入口 `syscall_entry`，其中出栈恢复数据逻辑，通过复用 `intrrupt_exit` 来实现。

> 可以与 `interrupt_entry%1` 以及 `interrupt_entry` 的代码进行对比。

```x86asm
extern syscall_check
extern syscall_table

global syscall_entry
syscall_entry:
    ; 验证系统调用号
    push eax
    call syscall_check
    add esp, 4 ; 函数调用结束后恢复栈

    ; 压入魔数和系统调用号，使得栈结构与 interrupt_entry_%1 相同
    push 0x20230830
    push 0x80

    ; 保存上文寄存器信息
    push ds
    push es
    push fs
    push gs
    pusha
    
    ; 压入中断向量，保证栈结构符合 interrupt_exit 要求
    push 0x80
    
    ; 压入系统调用的参数
    push edx ; 第三个参数
    push ecx ; 第二个参数
    push ebx ; 第一个参数

    ; 调用系统调用对应的处理函数
    call [syscall_table + eax * 4]

    add esp, 3 * 4 ; 系统调用处理结束恢复栈

    ; 修改栈中的 eax 寄存器值，设置系统调用的返回值
    mov dword [esp + 8 * 4], eax

    ; 跳转到中断返回
    jmp interrupt_exit
```

### 3.4 中断注册

在 IDT 中，将系统调用入口注册到中断向量 0x80。

注册系统调用类似于之前的异常、外中断注册，区别在于设置系统调用入口的 `DPL` 权级为 3，即用户态。

因为系统调用是用户与系统交互的一种手段，所以设置 `DPL = 3` 使得用户可以通过 `int 0x80` 来触发系统调用，从而与系统交互（而 `DPL` 为 0 的一次和外中断，只能由系统来触发）。

```c
/* include/xos/interrupt.h */
#define SYSCALL_VECTOR 0x80 // 系统调用的中断向量号

/* kernel/interrupt.c */
void idt_init() {
    ...
    // 初始化系统调用
    gate_t *gate = &idt[SYSCALL_VECTOR];
    gate->offset_low = (u32)syscall_entry & 0xffff;
    gate->offset_high = ((u32)syscall_entry >> 16) & 0xffff;
    gate->selector = 1 << 3; // 1 号段为代码段
    gate->reserved = 0;      // 保留不用
    gate->type = 0b1110;     // 中断门
    gate->segment = 0;       // 系统段
    gate->DPL = 3;           // 用户态权级
    gate->present = 1;       // 有效位
    ...
}
```

## 4. 功能测试

在 `kernel/main.c` 搭建测试框架：

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

    return;
}
```

--- 

> 以下代码位于 `kernel/start.asm`

```x86asm
_start:
    ...
    call kernel_init

    xchg bx, bx ; BMB
    
    jmp $ ; 阻塞
```

在 `_start` 处设置 BMB 断点，因为已经在 `kernel_init()` 中调用 `interrupt_init()`，所以此时通过 bochs 查看当前的是否将 `syscall_entry` 注册到 IDT 中（0x80 号中断向量）。

---

在 `syscall_entry` 中设置以下几个 BMB 断点，追踪观察系统调用的流程。

```x86asm
syscall_entry:
    xchg bx, bx ; BMB 1

    ...
    
    ; 压入中断向量，保证栈结构符合 interrupt_exit 要求
    push 0x80
    
    xchg bx, bx ; BMB 2
    
    ...

    ; 调用系统调用对应的处理函数
    call [syscall_table + eax * 4]

    xchg bx, bx ; BMB 3

    add esp, 3 * 4 ; 系统调用处理结束恢复栈

    ; 修改栈中的 eax 寄存器值，设置系统调用的返回值
    mov dword [esp + 8 * 4], eax

    ; 跳转到中断返回
    jmp interrupt_exit
```

- 在断点 `BMB 1` 处，观察刚进入系统调用入口时的栈结构
- 在断点 `BMB 2` 处，观察此时（调用系统调用处理函数前）的栈结构，是否与中断入口的栈结构符合
- 在断点 `BMB 2` 处，观察此时（调用系统调用处理函数后）的栈结构

---

测试 0 号系统调用（注册的处理函数为 `sys_test()`）：

```x86asm
_start:
    ...
    call kernel_init

    ; 0 号系统调用
    mov eax, 0
    int 0x80

    jmp $ ; 阻塞
```

预期为，打印 `syscall test...`。

--- 

测试其它系统调用（目前注册的处理函数为 `sys_default`）：

```x86asm
_start:
    ...
    call kernel_init

    ; 1 号系统调用
    mov eax, 1
    int 0x80

    jmp $ ; 阻塞
```

预期为，触发 `panic: syscall is not implemented!!!`。