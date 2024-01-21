#ifndef XOS_TIME_H
#define XOS_TIME_H

#include <xos/types.h>

/* CMOS */

#define CMOS_ADDR_PORT 0x70 // CMOS 索引端口
#define CMOS_DATA_PORT 0x71 // CMOS 数据端口

// 下面是 CMOS 消息的寄存器索引
#define CMOS_SECOND  0X00   // (0 ~ 59)
#define CMOS_MINUTE  0x02   // (0 ~ 59)
#define CMOS_HOUR    0x04   // (0 ~ 23)
#define CMOS_WEEKDAY 0x06   // (1 ~ 7) 星期天=1，星期六=7
#define CMOS_DAY     0x07   // (1 ~ 31)
#define CMOS_MONTH   0x08   // (1 ~ 12)
#define CMOS_YEAR    0x09   // (0 ~ 99)
#define CMOS_CENTURY 0x32   // 可能不存在

#define CMOS_RTC_SECOND 0x01// 闹钟秒值
#define CMOS_RTC_MINUTE 0x03// 闹钟分钟值
#define CMOS_RTC_HOUR   0x05// 闹钟小时值
#define CMOS_RTC_A   0x0a   // RTC 状态寄存器 A
#define CMOS_RTC_B   0x0b   // RTC 状态寄存器 B
#define CMOS_RTC_C   0x0c   // RTC 状态寄存器 C
#define CMOS_RTC_D   0x0d   // RTC 状态寄存器 D

// 屏蔽 NMI 中断
#define CMOS_NMI 0x80

// 获取 CMOS 的 addr 索引处的数据
u8 cmos_read(u8 addr);

// 向 CMOS 的 addr 索引处写入数据
void cmos_write(u8 addr, u8 value);

/* time.c */

typedef struct time_val {
    i32 tm_sec;     // 秒数 [0, 59]
    i32 tm_min;     // 分钟数 [0, 59]
    i32 tm_hour;    // 小时数 [0, 23]
    i32 tm_mday;    // 1 个月的天数 [1, 31]
    i32 tm_mon;     // 一年中月份 [1, 12]
    i32 tm_year;    // 从 1900 年开始的年数
    i32 tm_wday;    // 1 星期中的某天 [1, 7]
    i32 tm_yday;    // 1 年中的某天 [1, 365]
    i32 century;    // 当前世纪
    i32 tm_isdst;   // 夏令时标志
} time_val;

// 将当前时间值读进所给地址当中
void time_read(time_val *time);

// 获取当前时间，以秒数为单位表述，从 1970 年开始计数
time_t mktime(time_val *time);

// 系统初始化时，调用该函数打印当前时间
void time_init();


/* rtc.c */

// 设置 RTC 闹钟值为当前时间值 + secs 秒
void set_alarm(u32 secs);

// 初始化实时时钟中断
void rtc_init();


/* clock.c */

// 开始蜂鸣
void start_beep();

// 初始化 PIT
void pit_init();

// 初始化时钟
void clock_init();

#endif