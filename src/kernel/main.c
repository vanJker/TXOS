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
extern void kernel_map();

void kernel_init() {
    console_init();
    gdt_init();
    memory_init();
    kernel_map();
    interrupt_init();
    // clock_init();
    // time_init();
    // rtc_init();
    // set_alarm(2);

    memory_test();

    // asm volatile("sti"); // 打开中断

    hang();
    return;
}