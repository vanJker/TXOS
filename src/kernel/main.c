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
extern void keyboard_init();
extern void tss_init();
extern void arena_init();
extern void ata_init();
extern void device_init();

void kernel_init() {
    device_init();
    console_init();
    gdt_init();
    tss_init();
    memory_init();
    kernel_map_init();
    arena_init();
    interrupt_init();
    clock_init();
    keyboard_init();
    time_init();
    // rtc_init();
    ata_init();
    task_init();
    syscall_init();

    irq_enable(); // 打开外中断响应

    hang();
    return;
}