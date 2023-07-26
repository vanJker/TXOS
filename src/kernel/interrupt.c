#include <xos/interrupt.h>
#include <xos/debug.h>

gate_t idt[IDT_SIZE]; // 中断描述符表
pointer_t idt_ptr;    // 中断描述符表指针

extern void interrupt_handler(); // 中断处理函数

void interrupt_init() {
    for (size_t i = 0; i < IDT_SIZE; i++) {
        gate_t *gate = &idt[i];
        gate->offset_low = (u32)interrupt_handler & 0xffff;
        gate->offset_high = ((u32)interrupt_handler >> 16) & 0xffff;
        gate->selector = 1 << 3; // 1 号段为代码段
        gate->reserved = 0;      // 保留不用
        gate->type = 0b1110;     // 中断门
        gate->segment = 0;       // 系统段
        gate->DPL = 0;           // 内核态权级
        gate->present = 1;       // 有效位
    }
    idt_ptr.base = (u32)idt;
    idt_ptr.limit = sizeof(idt) - 1;
    BMB; // Bochs Magic Breakpoint
    asm volatile("lidt idt_ptr");
}