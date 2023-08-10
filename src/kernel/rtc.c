#include <xos/time.h>
#include <xos/cmos.h>
#include <xos/assert.h>
#include <xos/interrupt.h>
#include <xos/debug.h>
#include <xos/stdlib.h>

// 设置 RTC 闹钟值为当前时间值 + secs 秒
void set_alarm(u32 secs) {
    LOGK("beeping after %d seconds\n", secs);

    // 读取当前时间
    time_val time;
    time_read(&time);

    // 计算时间间隔
    u8 sec = secs % 60;
    secs /= 60;
    u8 min = secs % 60;
    secs /= 60;
    u32 hour = secs;

    // 更新闹钟
    time.tm_sec += sec;
    if (time.tm_sec >= 60) {
        time.tm_sec %= 60;
        time.tm_min += 1;
    }

    time.tm_min += min;
    if (time.tm_min >= 60) {
        time.tm_min %= 60;
        time.tm_hour += 1;
    }

    time.tm_hour += hour;
    if (time.tm_hour >= 24) {
        time.tm_hour %= 24;
    }

    // 设置 CMOS 闹钟
    cmos_write(CMOS_RTC_SECOND, bin_to_bcd(time.tm_sec));
    cmos_write(CMOS_RTC_MINUTE, bin_to_bcd(time.tm_min));
    cmos_write(CMOS_RTC_HOUR,   bin_to_bcd(time.tm_hour));
    
    // 打开闹钟中断
    cmos_write(CMOS_RTC_B, 0b00100010); 
    // 读 CMOS 寄存器 C，从而允许 CMOS 继续产生中断
    cmos_read(CMOS_RTC_C);              
}

// 实时时钟中断处理函数
void rtc_handler(int vector) {
    // 实时时钟中断向量号
    assert(vector == IRQ_RTC + IRQ_MASTER_NR);

    // 向中断控制器发送中断处理完成的信号
    send_eoi(vector);

    // 读 CMOS 寄存器 C，从而允许 CMOS 继续产生中断
    // cmos_read(CMOS_RTC_C);

    // 触发闹钟中断，发出蜂鸣
    start_beep();
}

void rtc_init() {
    set_interrupt_handler(IRQ_RTC, rtc_handler);
    set_interrupt_mask(IRQ_RTC, true);
    set_interrupt_mask(IRQ_CASCADE, true);
}