#include <xos/io.h>
#include <xos/interrupt.h>
#include <xos/assert.h>
#include <xos/debug.h>
#include <xos/task.h>

// 对应计数器相关的端口
#define PIT_CHAN0_PORT 0x40
#define PIT_CHAN2_PORT 0x42
#define PIT_CTRL_PORT  0x43

// PC Speaker 对应的端口
#define PC_SPEAKER_PORT 0x61

#define OSCILLATOR 1193182  // 时钟震荡频率
#define CLOCK_HZ 100        // 时钟中断频率
#define BEEP_HZ 440         // 蜂鸣器频率
#define CLOCK_COUNTER (OSCILLATOR / CLOCK_HZ)
#define BEEP_COUNTER  (OSCILLATOR / BEEP_HZ)

// 时间片计数器
u32 volatile jiffies = 0;
// 蜂鸣器计数器
u32 volatile beeping = 0;

// 开始蜂鸣
void start_beep() {
    if (!beeping) {
        outb(PC_SPEAKER_PORT, inb(PC_SPEAKER_PORT) | 0b11);
        // 蜂鸣持续 5 个时间片
        beeping = jiffies + 5;
    }
}

// 结束蜂鸣
void stop_beep() {
    if (jiffies > beeping) {
        outb(PC_SPEAKER_PORT, inb(PC_SPEAKER_PORT) & ~0b11);
        beeping = 0;
    }
}

// 时钟中断处理函数
void clock_handler(int vector) {
    // 时钟中断向量号
    assert(vector == IRQ_CLOCK + IRQ_MASTER_NR);

    // 向中断控制器发送中断处理完成的信号
    send_eoi(vector);

    // 每个时间片结束前都需要检查当前蜂鸣是否完成（蜂鸣持续 5 个时间片）
    stop_beep();

    // 更新时间片计数
    jiffies++;
    // DEBUGK("clock jiffies %d ...\n", jiffies);

    /***** 任务管理 *****/
    task_t *current = current_task();
    assert(current->magic == XOS_MAGIC); // 检测栈溢出

    // 更新 PCB 中与时间片相关的数据
    current->jiffies = jiffies;
    current->ticks--;
    if (current->ticks == 0) {
        schedule(); // 任务调度
    }
}

void pit_init() {
    // 配置计数器 0 时钟
    outb(PIT_CTRL_PORT, 0b00110100);
    outb(PIT_CHAN0_PORT, CLOCK_COUNTER & 0xff);
    outb(PIT_CHAN0_PORT, (CLOCK_COUNTER >> 8) & 0xff);

    // 配置计数器 2 蜂鸣器
    outb(PIT_CTRL_PORT, 0b10110110);
    outb(PIT_CHAN2_PORT, BEEP_COUNTER & 0xff);
    outb(PIT_CHAN2_PORT, (BEEP_COUNTER >> 8) & 0xff);
}

void clock_init() {
    pit_init();
    set_interrupt_handler(IRQ_CLOCK, clock_handler);
    set_interrupt_mask(IRQ_CLOCK, true);
}