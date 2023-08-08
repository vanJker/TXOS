#ifndef XOS_TIME_H
#define XOS_TIME_H

#include <xos/types.h>

typedef u32 time_t;

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

#endif