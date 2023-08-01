#include <xos/interrupt.h>
#include <xos/debug.h>
#include <xos/printk.h>
#include <xos/stdlib.h>
#include <xos/io.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)
// #define LOGK(fmt, args...)

#define EXCEPTION_SIZE 0x20 // 异常数量
#define ENTRY_SIZE     0x30 // 中断入口数量

#define PIC_M_CTRL 0x20     // 主片的控制端口
#define PIC_M_DATA 0x21     // 主片的数据端口
#define PIC_S_CTRL 0xa0     // 从片的控制端口
#define PIC_S_DATA 0xa1     // 从片的数据端口
#define PIC_EOI    0x20     // 通知中断控制器中断结束

gate_t idt[IDT_SIZE];       // 中断描述符表
pointer_t idt_ptr;          // 中断描述符表指针

handler_t handler_table[IDT_SIZE];                  // 中断处理函数表
extern handler_t handler_entry_table[ENTRY_SIZE];   // 中断入口函数表

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
    hang();
}

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

void default_handler(int vector) {
    static u32 counter = 0;
    send_eoi(vector);
    LOGK("[%d] default interrupt called %d...\n", vector, counter++);
}

// 初始化中断控制器
void pic_init() {
    // 主片 ICW
    outb(PIC_M_CTRL, 0b00010001); // ICW1: 边沿触发, 级联 8259, 需要ICW4.
    outb(PIC_M_DATA, 0x20);       // ICW2: 起始端口/中断向量号 0x20
    outb(PIC_M_DATA, 0b00000100); // ICW3: IR2接从片.
    outb(PIC_M_DATA, 0b00000001); // ICW4: 8086模式, 正常EOI

    // 从片 ICW
    outb(PIC_S_CTRL, 0b00010001); // ICW1: 边沿触发, 级联 8259, 需要ICW4.
    outb(PIC_S_DATA, 0x28);       // ICW2: 起始端口/中断向量号 0x28
    outb(PIC_S_DATA, 2);          // ICW3: 设置从片连接到主片的 IR2 引脚
    outb(PIC_S_DATA, 0b00000001); // ICW4: 8086模式, 正常EOI

    // 主片/从片 OCW1
    outb(PIC_M_DATA, 0b11111110); // OCW1: 关闭所有中断（时钟中断除外）
    outb(PIC_S_DATA, 0b11111111); // OCW1: 关闭所有中断
}

// 初始化中断描述符表，以及中断处理函数表
void idt_init() {
    // 初始化中断描述符表
    for (size_t i = 0; i < ENTRY_SIZE; i++) {
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

    // 初始化异常处理函数表
    for (size_t i = 0; i < EXCEPTION_SIZE; i++) {
        handler_table[i] = exception_handler;
    }
    // 初始化外中断处理函数表
    for (size_t i = EXCEPTION_SIZE; i < ENTRY_SIZE; i++) {
        handler_table[i] = default_handler;
    }

    // 加载中断描述符表指针
    idt_ptr.base = (u32)idt;
    idt_ptr.limit = sizeof(idt) - 1;
    asm volatile("lidt idt_ptr");
}

// 中断初始化
void interrupt_init() {
    pic_init();
    idt_init();

    // 打开中断
    asm volatile("sti");
}