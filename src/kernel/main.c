#include <xos/printk.h>
extern void console_init();
extern void gdt_init();
extern void interrupt_init();
extern void clock_init();
extern void time_init();

void kernel_init() {
    console_init();
    gdt_init();
    interrupt_init();
    // clock_init();
    time_init();

    // asm volatile("sti"); // 打开中断

    return;
}