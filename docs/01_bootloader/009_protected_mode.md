# 009 保护模式和全局描述符

## 1. 原理说明

- 8086 处理器只有 1M 内存，16 位实模式，理论上病毒可以完全控制计算机
- 80286 处理器为 16 位保护模式

![80286 segment descriptor](./images/80286-segment-descriptor.jpg)

### 1.1 保护模式 Protected Mode

- 信息：IT Information Technology /《信息论》
- 寄存器：有一些寄存器（控制寄存器）只能被操作系统访问
- 高速缓存：段寄存器除了有可见的 16 bit 存放段选择子外，还有一部分不可见的高速缓存来存放段描述符，这样无需每次都访问内存去获得段描述符。
- 内存：描述符，描述内存段的范围和访问权限等等
- 外部设备：硬盘、in/out 操作

### 1.2 全局描述符

- 内存的起始位置
- 内存的长度：界限 = 长度 - 1
- 内存属性 / 访问权限

![80386 segment descriptor](./images/80386-segment-descriptor.jpg)

```c
typedef struct descriptor /* 共 8 个字节 */
{
    unsigned short limit_low : 16;  // 段界限 0 ~ 15 位
    unsigned int base_low : 24;     // 基地址 0 ~ 23 位
    unsigned char type : 4;         // 段类型
    unsigned char segment : 1;      // 1 表示代码段或数据段，0 表示系统段
    unsigned char DPL : 2;          // Descriptor Privilege Level 描述符特权等级 0 ~ 3
    unsigned char present : 1;      // 存在位，1 在内存中，0 在磁盘上
    unsigned char limit_high : 4;   // 段界限 16 ~ 19;
    unsigned char available : 1;    // 该安排的都安排了，送给操作系统吧
    unsigned char long_mode : 1;    // 64 位扩展标志
    unsigned char big : 1;          // 1 表示 32 位，0 表示 16 位;
    unsigned char granularity : 1;  // 1 表示粒度为 4KB，0 表示粒度为 1B
    unsigned char base_high : 8;    // 基地址 24 ~ 31 位
} __attribute__((packed)) descriptor;
```

#### 1.2.1 type segment

仅当描述符的 type 部分为 1 时，才按以下格式布置。

`| X | C/E | R/W | A |`

- A: Accessed 1 表示被 CPU 访问过，0 反之
- X: 1 表示代码，0 表示数据
- X = 1：代码段
    - C: 1 表示是依从代码段，0 反之
    - R: 1 表示可读，0 反之
- X = 0: 数据段
    - E: 1 表示向下扩展，0 表示向上扩展
    - W: 1 表示可写，0 反之

### 1.3 全局描述符表 GDT (Global Descriptor Table)

#### 1.3.1 数据结构

- 数组 / 顺序表
- 链表 
- 哈希表

```c
descriptor gdt[8192];
```

- gdt[0] 必须全为 0，表示 NULL 描述符
- 8191 描述符为最后一个有效的描述符（因为段选择子的 index 是 13 位的，2^13 = 8192）

#### 1.3.2 gdtr 全局描述符表寄存器

该寄存器记录全局描述符的起始位置（基地址）和长度（界限）。

```c
typedef struct pointer /* 48 位 */
{
    unsigned short limit : 16; // size - 1
    unsigned int base : 32;
} __attribute__((packed)) pointer;
```

相关操作：

```x86asm
lgdt [gdt_ptr]  ; 加载 gdt
sgdt [gdt_ptr]  ; 保存 gdt
```

### 1.4 段选择子

- 只需要一个代码段
- 需要一个或多个数据段、栈段
- 加载到段寄存器中，校验特权级

```c
typedef struct selector /* 16 位 */
{
    unsigned char RPL : 2;      // Request PL
    unsigned char TI : 1;       // 0 表示为全局描述符，1 表示为局部描述符
    unsigned short index : 13;  // 全局描述符表索引
} __attribute__((packed)) selector;
```

段寄存器：

- cs / ds / es / ss
- fs / gs

### 1.5 A20 线

为了兼容 8086 处理器，A20 线默认关闭（因为 A20 线可以用于访问超过 1M 的内存地址）。

- 8086 处理器：16 根地址线，只能表示 1M 内存（通过段偏移实现），如果段地址 * 16 + 偏移地址 > 1M，则会取低 20 位作为目的地址（地址回绕）。
- 80286 处理器：24 根地址线，可以表示 16M 内存。
- 80386 处理器：32 根地址线，可以表示 4G 内存。

0x92 端口：8 bit 寄存器，控制打开 A20 线，将 0x92 端口的第 1 位设为 1，表示将 A20 线打开。

- <https://wiki.osdev.org/A20>
- <https://wiki.osdev.org/Non_Maskable_Interrupt>

### 1.6 PE (Protect Enable)

进入 / 使能保护模式，需要将 cr0 寄存器的第 0 位置为 1。

使能保护模式流程：

- 关闭中断，使能保护模式过程需要关闭中断（因为实模式和保护模式的中断向量表不一样）。
- 打开 A20 线。
- 将全局描述符表加载到 gdtr 寄存器。
- 设置 cr0 寄存器的第 0 位为 1。
- 长跳转刷新流水线（清空流水线中的实模式指令）。
- 加载段选择子到相应的段寄存器。

## 2. 代码分析

在内核加载器中，实现从 16 位实模式进入 32 位保护模式（内存段以 4K 为粒度）。

### 2.1 定义内存基地址和界限

```x86asm
; 内存起始位置
memory_base equ 0

; 粒度为4K，所以界限为 (4G/4K)-1
memory_limit equ ((1024 * 1024 * 1024 * 4) / (1024 * 4)) - 1
```

### 2.2 定义全局描述符表

暂时只有一个代码段描述符和一个数据段描述符，以及必需的 NULL 描述符。其中代码段和数据段的基地址和界限是一样的，

```x86asm
gdt_base:
    ; NULL 描述符
    dd 0, 0 
gdt_code:
    ; 段界限 0-15 位
    dw memory_limit & 0xffff 
    ; 基地址 0-15 位
    dw memory_base & 0xffff 
    ; 基地址 16-23 位
    db (memory_base >> 16) & 0xff 
    ; 存在内存 | DLP=0 | 代码段 | 非依从 | 可读 | 没有被访问过
    db 0b1_00_1_1010
    ; 粒度 4K | 32 位 | 不是 64 位 | 段界限 16-19 位
    db 0b1_1_1_0_0000 | (memory_limit >> 16) & 0xf
    ; 基地址 24-31 位
    db (memory_base >> 24) & 0xff 
gdt_data:
    ; 段界限 0-15 位
    dw memory_limit & 0xffff 
    ; 基地址 0-15 位
    dw memory_base & 0xffff 
    ; 基地址 16-23 位
    db (memory_base >> 16) & 0xff 
    ; 存在内存 | DLP=0 | 数据段 | 向上扩展 | 可写 | 没有被访问过
    db 0b1_00_1_0010
    ; 粒度 4K | 32 位 | 不是 64 位 | 段界限 16-19 位
    db 0b1_1_0_0_0000 | (memory_limit >> 16) & 0xf
    ; 基地址 24-31 位
    db (memory_base >> 24) & 0xff 
gdt_end:
```

```x86asm
; 全局描述符指针，可以被加载进 gdtr 寄存器中
gdt_ptr:
    ; gdt 界限
    dw (gdt_end - gdt_base) - 1 
    ; gdt 基地址
    dd gdt_base 
```

### 2.3 定义段选择子

段选择子的 RPL 和 TI 均置为 0。

```x86asm
; 代码段选择子
code_selector equ (1 << 3) 

; 数据段选择子
data_selector equ (2 << 3) 
```

### 2.4 使能 32 位保护模式

```x86asm
enable_protected_mode:
    ; 关闭中断
    cli

    ; 打卡 A20 线
    in al, 0x92
    or al, 0x02
    out 0x92, al

    ; 加载 gdt 指针到 gdtr 寄存器
    lgdt [gdt_ptr]

    ; 使能保护模式
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax

    ; 通过跳转来刷新缓存，同时加载代码段选择子到代码段寄存器，从而执行保护模式下的指令
    jmp dword code_selector:protected_mode
```

### 2.5 执行 32 位保护模式下的代码

```x86asm
[bits 32]
protected_mode:
    ; 在 32 位保护模式下初始化段寄存器
    mov ax, data_selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; 修改栈顶，这里只是随意制定了一个地址，因为还没有用到栈
    mov esp, 0x10000

    ; 现在可以随意访问 32 位的地址了
    mov byte [0xb8000], 'P'
    mov byte [0x200000], 'P'

    jmp $ ; 阻塞
```

预期结果为屏幕的第一个字符被替换为 ‘P’，0x200000 地址（即 2M 地址，超过了 16 位实模式可以访存的范围）处的值为 50。
