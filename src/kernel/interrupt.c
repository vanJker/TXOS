#include <xos/interrupt.h>
#include <xos/debug.h>
#include <xos/printk.h>

#define EXCEPTION_SIZE 0x20 // 异常数量

gate_t idt[IDT_SIZE]; // 中断描述符表
pointer_t idt_ptr;    // 中断描述符表指针

handler_t handler_table[IDT_SIZE];                    // 中断处理函数表
extern handler_t handler_entry_table[EXCEPTION_SIZE]; // 中断入口函数表

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