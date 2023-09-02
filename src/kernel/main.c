extern void hang();
extern void irq_enable();

extern void console_init();
extern void gdt_init();
extern void interrupt_init();
extern void clock_init();
extern void time_init();
extern void rtc_init();
extern void memory_init();
extern void kernel_map_init();
extern void task_init();
extern void syscall_init();

void kernel_init() {
    console_init();
    gdt_init();
    memory_init();
    kernel_map_init();
    interrupt_init();
    clock_init();
    // time_init();
    // rtc_init();
    task_init();
    syscall_init();

    list_test();

    // irq_enable(); // 打开外中断响应

    hang();
    return;
}