# 035 计数器和时钟

时间片：两个时钟中断之间的时间间隔

常用的可编程定时计数器 (PIT : Programmable Interval Timer) 有 Intel 8253/8254，其中 8254 可以称为 8253 的增强版。

在 8253 内部有 3 个独立的计数器，分别是计数器 0 ~ 2，端口号分别为 0x40 ~ 0x42；每个计数器完全相同，都是 16 位大小，相互独立，互不干涉。

8253 计数器是个减法计数器，从初值寄存器中得到初值，然后载入计数器中，然后随着时钟变化递减。计数器初值寄存器，计数器执行寄存器，和输出锁存器都是 16 位的寄存器，高八位和低八位可以单独访问。

## 1. 计数器

三个计数器有自己各自的用途：

- 计数器 0，端口号 0x40，用于产生时钟信号，它采用工作方式 3。
- 计数器 1，端口号 0x41，用于 DRAM 的定时刷新控制。
- 计数器 2，端口号 0x42，用于内部扬声器发出不同音调的声音，原理是给扬声器输送某频率的方波。

计数器 0 用于产生时钟中断，就是连接在 IRQ0 引脚上的时钟，也就是控制计数器 0 可以控制时钟发生的频率，以改变时间片的间隔；

## 2. 8253 控制字

控制字寄存器，端口号 0x43，是 8 位寄存器，控制字寄存器也成为模式控制寄存器，用于指定计数器的 工作方式、读写格式 及 数制。

控制字结构：

| 7   | 6   | 5   | 4   | 3   | 2   | 1   | 0   |
| --- | --- | --- | --- | --- | --- | --- | --- |
| SC1 | SC0 | RL1 | RL0 | M2  | M1  | M0  | BCD |

- SC(Select Counter) 0 ~ 1：计数器选择位
    - 00 计数器 0
    - 01 计数器 1
    - 10 计数器 2
    - 11 无效
- RL(Read Load) 0 ~ 1：读写操作位
    - 00 锁存数据，供 CPU 读
    - 01 只读写低字节
    - 10 只读写高字节
    - 11 先读写低字节，后读写高字节
- M (Mode) 0 ~ 2：模式选择
    - 000：模式 0
    - 001：模式 1
    - x10：模式 2
    - x11：模式 3
    - 100：模式 4
    - 101：模式 5
- BCD：(Binary Coded Decimal) 码
    - 0 表示二进制计数器
    - 1 二进制编码的十进制计数器

## 3. 模式

- 模式 0：计数结束时中断
- 模式 1：硬件可重触发单稳方式
- 模式 2：比率发生器，用于分频
- 模式 3：方波发生器
- 模式 4：软件触发选通
- 模式 5：硬件触发选通

模式时序图

![](./images/intel_8253_mode.jpg)

## 4. 振荡器

振荡器的频率大概是 1193182 Hz，假设希望中断发生的频率为 100Hz，那么计数器初值寄存器的值为：

$$V = {1193182 \over 100} = 11931$$

## 5. 其他的问题

- 为什么 振荡器 的频率是 1193182 Hz？

最初的 PC 机，使用一个基础振荡器来生成频率，14.31818 MHz，因为这个频率常用于电视线路，这个基础频率除以 3 就得到了频率 4.77272666 MHz 用于 CPU；除以 4 得到频率 3.579545 MHz 用于 CGA 显卡控制器。从逻辑上将前两个频率求最大公约数，就得到了频率 1.1931816666 MHz，这个方案极大的节约了成本，因为 14.31818 MHz 的振荡器可以大量的生产，所以就更便宜。

## 6. 代码分析

### 6.1 IRQ

使用 `enum` 和 `define` 来定义一些常量：

```c
/* include/xos/interrupt.h */

#define IRQ_MASTER_NR 0x20  // 主片起始向量号
#define IRQ_SLAVE_NR  0x28  // 从片起始向量号

// 外中断 IRQ(interrupt request)
enum irq_t {
    IRQ_CLOCK,      // 时钟
    IRQ_KEYBOARD,   // 键盘
    IRQ_CASCADE,    // 8259 从片控制器
    IRQ_SERIAL_2,   // 串口 2
    IRQ_SERIAL_1,   // 串口 2
    IRQ_PARALLEL_2, // 并口 2
    IRQ_FLOPPY,     // 软盘控制器
    IRQ_PARALLEL_1, // 并口 1
    IRQ_RTC,        // 实时时钟
    IRQ_REDIRECT,   // 重定向 IRQ2
    IRQ_10,
    IRQ_11,
    IRQ_MOUSE,      // 鼠标
    IRQ_MATH,       // 协处理器 x87
    IRQ_HARDDISK_1, // ATA 硬盘第一通道
    IRQ_HARDDISK_2, // ATA 硬盘第二通道
};
```

声明一些与外中断（IRQ）处理相关的函数：

```c
/* include/xos/interrupt.h */

// 发送中断结束信号
void send_eoi(int vector);

// 设置 IRQ 对应的中断处理函数
void set_interrupt_handler(u32 irq, handler_t handler);

// 设置 IRQ 对应的中断屏蔽字
void set_interrupt_mask(u32 irq, bool enable);
```

### 6.2 IRQ 相关功能

概念问题：

- vector（即中断向量），是在整个中断描述符表中的序号。
- irq（外中断请求），是在外中断的序号。
- 两者偏移量为 0x20，vector 0x20 即为 irq 0，时钟中断。

```c
void send_eoi(int vector) {
    // 如果中断来自主片，只需要向主片发送 EOI
    if (vector >= 0x20 && vector < 0x28) {
        outb(PIC_M_CTRL, PIC_EOI);
    }
    // 如果中断来自从片，除了向从片发送 EOI 以外，还要再向主片发送 EOI
    if (vector >= 0x28 && vector < 0x30) {
        outb(PIC_M_CTRL, PIC_EOI);
        outb(PIC_S_CTRL, PIC_EOI);
    }
}

void set_interrupt_handler(u32 irq, handler_t handler) {
    assert(irq >= 0 && irq < 16);
    handler_table[IRQ_MASTER_NR + irq] = handler;
}

void set_interrupt_mask(u32 irq, bool enable) {
    assert(irq >= 0 && irq < 16);
    u16 port;

    // 计算端口和屏蔽字
    if (irq < 8) {  // 位于主片
        port = PIC_M_DATA;
    } else {        // 位于从片
        port = PIC_S_DATA;
        irq -= 8;   // 消除偏移量
    }

    // 开启/屏蔽指定 IRQ
    if (enable) outb(port, inb(port) & ~(1 << irq));
    else        outb(port, inb(port) | (1 << irq));
}
```

### 6.3 时钟 / clock

声明 PIT 相关的常量：

```c
/* kernel/clock.c */

// 对应计数器相关的端口
#define PIT_CHAN0_PORT 0x40
#define PIT_CHAN2_PORT 0x42
#define PIT_CTRL_PORT  0x43

#define HZ 100              // 时钟中断频率
#define OSCILLATOR 1193182  // 时钟震荡频率
#define CLOCK_COUNTER (OSCILLATOR / HZ)
```

CPU 的时钟一秒计数 `OSCILLATOR` 次，而我们需要时钟中断发生频率为一秒 `HZ` 次，所以要求 CPU 计数器每间隔 `(OSCILLATOR / HZ)` 次计数，就发生时钟中断。

---

时钟中断处理函数：

```c
/* kernel/clock.c */

// 时钟中断处理函数
void clock_handler(int vector) {
    assert(vector == 0x20);
    send_eoi(vector);

    jiffies++;
    DEBUGK("clock jiffies %d ...\n", jiffies);
}
```

---

初始化 PIT：

```c
/* kernel/clock.c */

void pit_init() {
    outb(PIT_CTRL_PORT, 0b00110100);
    outb(PIT_CHAN0_PORT, CLOCK_COUNTER & 0xff);
    outb(PIT_CHAN0_PORT, (CLOCK_COUNTER >> 8) & 0xff);
}
```

根据上文中的原理说明

- 往 `PIT_CTRL_PORT` 即 8253 控制字写入 `0b00110100`，表示：选择计数器 0，先写低字节再写高字节，选择模式2，使用二进制编码。
- 往 `PIT_CHAN0_PORT` 即 计数器 0，先写入计数的低字节，再写入高字节。

---

初始化时钟中断：

```c
/* kernel/clock.c */

void clock_init() {
    // 初始化 PIT
    pit_init();
    // 设置时钟中断处理函数
    set_interrupt_handler(IRQ_CLOCK, clock_handler);
    // 打开时钟中断
    set_interrupt_mask(IRQ_CLOCK, true);
}
```

## 7. 测试

```c
/* kernel/main.c */

extern void console_init();
extern void gdt_init();
extern void interrupt_init();
extern void clock_init();

void kernel_init() {
    console_init();
    gdt_init();
    interrupt_init();
    clock_init();

    // 阻塞，以测试时钟中断
    hang();

    return;
}
```

预期为，每间隔 10 ms 打印 `"clock jiffies %d ..."` 这样格式的句子。

可尝试把时钟中断频率调高一些，来对比观察时钟中断的发生。

## 8. 参考文献

1. <https://www.cpcwiki.eu/imgs/e/e3/8253.pdf>
2. <https://wiki.osdev.org/Programmable_Interval_Timer>
2. 郑刚 - 《操作系统真象还原》