#include <xos/io.h>
#include <xos/interrupt.h>
#include <xos/assert.h>
#include <xos/debug.h>

// 对应计数器相关的端口
#define PIT_CHAN0_PORT 0x40
#define PIT_CHAN2_PORT 0x42
#define PIT_CTRL_PORT  0x43

#define HZ 100              // 时钟中断频率
#define OSCILLATOR 1193182  // 时钟震荡频率
#define CLOCK_COUNTER (OSCILLATOR / HZ)

// 时间片计数器
u32 volatile jiffies = 0;

// 时钟中断处理函数
void clock_handler(int vector) {
    assert(vector == 0x20);
    send_eoi(vector);

    jiffies++;
    DEBUGK("clock jiffies %d ...\n", jiffies);
}

void pit_init() {
    outb(PIT_CTRL_PORT, 0b00110100);
    outb(PIT_CHAN0_PORT, CLOCK_COUNTER & 0xff);
    outb(PIT_CHAN0_PORT, (CLOCK_COUNTER >> 8) & 0xff);
}

void clock_init() {
    pit_init();
    set_interrupt_handler(IRQ_CLOCK, clock_handler);
    set_interrupt_mask(IRQ_CLOCK, true);
}