#include <xos/interrupt.h>
#include <xos/debug.h>

extern void hang();
extern void console_init();
extern void gdt_init();
extern void interrupt_init();
extern void clock_init();
extern void time_init();
extern void rtc_init();
extern void memory_init();
extern void set_alarm(unsigned int);
extern void memory_test();
extern void kernel_map_init();

void kernel_init() {
    console_init();
    gdt_init();
    memory_init();
    kernel_map_init();
    interrupt_init();
    // clock_init();
    // time_init();
    // rtc_init();
    // set_alarm(2);

    irq_disable();
    LOGK("IRQ state: %d\n", get_irq_state());

    irq_enable();
    LOGK("IRQ state: %d\n", get_irq_state());

    irq_save();
    LOGK("IRQ state: %d\n", get_irq_state());

    irq_restore();
    LOGK("IRQ state: %d\n", get_irq_state());

    // asm volatile("sti"); // 打开中断

    hang();
    return;
}