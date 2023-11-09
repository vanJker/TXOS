#ifndef XOS_INTERRUPT_H
#define XOS_INTERRUPT_H

#include <xos/types.h>

#define IDT_SIZE 256        // 一共 256 个中断描述符
#define SYSCALL_VECTOR 0x80 // 系统调用的中断向量号

#define IRQ_MASTER_NR 0x20  // 主片起始向量号
#define IRQ_SLAVE_NR  0x28  // 从片起始向量号

// 外中断响应处于禁止状态
#define ASSERT_IRQ_DISABLE() assert(get_irq_state() == 0)

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

// 中断描述符
typedef struct gate_t /* 共 8 个字节 */
{
    u16 offset_low;     // 段内偏移 0 ~ 15 位
    u16 selector;       // 代码段选择子
    u8 reserved;        // 保留不用
    u8 type : 4;        // 任务门/中断门/陷阱门
    u8 segment : 1;     // segment = 0 表示系统段
    u8 DPL : 2;         // 使用 int 指令访问的最低权限
    u8 present : 1;     // 是否有效
    u16 offset_high;    // 段内偏移 16 ~ 31 位
} _packed gate_t;

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

// Page Fault 缺页异常的错误码
typedef struct page_error_code_t {
    u8 present : 1;
    u8 write : 1;
    u8 user : 1;
    u8 rsvd : 1;
    u8 fetch : 1;
    u8 protection : 1;
    u8 shadow : 1;
    u8 reserved0;
    u8 sgx : 1;
    u16 reserved1;
} _packed page_error_code_t;

// 中断处理函数
typedef void *handler_t;

void interrupt_init();

// 发送中断结束信号
void send_eoi(int vector);

// 设置 IRQ 对应的中断处理函数
void set_interrupt_handler(u32 irq, handler_t handler);

// 设置 IRQ 对应的中断屏蔽字
void set_interrupt_mask(u32 irq, bool enable);

// 获取当前的外中断响应状态，即获取 IF 位
u32 get_irq_state();

// 设置外中断响应状态，即设置 IF 位
void set_irq_state(u32 state);

// 关闭外中断响应，即清除 IF 位，并返回关中断之前的状态
u32 irq_disable();

// 打开外中断响应，即设置 IF 位
void irq_enable();

// 保存当前的外中断状态，并关闭外中断
void irq_save();

// 将外中断状态恢复为先前的外中断状态
void irq_restore();

#endif