#ifndef XOS_CMOS_H
#define XOS_CMOS_H

#include <xos/types.h>

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

#endif