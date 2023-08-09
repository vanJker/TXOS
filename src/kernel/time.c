#include <xos/time.h>
#include <xos/io.h>
#include <xos/debug.h>
#include <xos/stdlib.h>
#include <xos/cmos.h>

#define MINUTE 60           // 每分钟的秒数
#define HOUR   (60 * MINUTE)// 每小时的秒数
#define DAY    (24 * HOUR)  // 每天的秒数
#define YEAR   (365 * DAY)  // 每年的秒数（以 365 天计算）

// 每个月开始时的已经过去天数
static int month[13] = {
    0, // 这里占位，没有 0 月，从 1 月开始
    0,
    (31),
    (31 + 28),
    (31 + 29 + 31),
    (31 + 29 + 31 + 30),
    (31 + 29 + 31 + 30 + 31),
    (31 + 29 + 31 + 30 + 31 + 30),
    (31 + 29 + 31 + 30 + 31 + 30 + 31),
    (31 + 29 + 31 + 30 + 31 + 30 + 31 + 31),
    (31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30),
    (31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31),
    (31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30)
};

// 判断是否是闰年
static bool is_leap(i32 year) {
    return (year % 4 == 0 && year % 100) || year % 400 == 0;
}

// 将 CMOS 读取的年份转换成公历年份
static i32 ad_year(i32 year) {
    int res;
    if (year >= 70) {   // CMOS 读取的是从 1900 年开始的年数
        res = 1900 + year;
    } else {            // CMOS 读取的是从 2000 年开始的年数    
        res = 2000 + year;
    }
    return res;
}

// 获取一年中过去的天数
static i32 get_yday(time_val *time) {
    int res = month[time->tm_mon];  // 已经过去的月的天数
    res += time->tm_mday;           // 这个月过去的天数

    // 公元历年份
    int year = ad_year(time->tm_year);

    // 如果是闰年，并且 2 月已经过去了，则加上一天
    if (is_leap(year) && time->tm_mon > 2) {
        res += 1;
    }

    return res;
}

void time_read_bcd(time_val *time) {
    // CMOS 的访问速度很慢。为了减小时间误差，在读取了下面循环中所有数值后，
    // 若此时 CMOS 中秒值发生了变化，那么就重新读取所有值。
    // 这样内核就能把与 CMOS 的时间误差控制在 1 秒之内。
    do {
        time->tm_sec = cmos_read(CMOS_SECOND);
        time->tm_min = cmos_read(CMOS_MINUTE);
        time->tm_hour = cmos_read(CMOS_HOUR);
        time->tm_mday = cmos_read(CMOS_DAY);
        time->tm_mon = cmos_read(CMOS_MONTH);
        time->tm_year = cmos_read(CMOS_YEAR);
        time->tm_wday = cmos_read(CMOS_WEEKDAY);
        time->century = cmos_read(CMOS_CENTURY);
    } while (time->tm_sec != cmos_read(CMOS_SECOND));
}

void time_read(time_val *time) {
    time_read_bcd(time);
    time->tm_sec = bcd_to_bin(time->tm_sec);
    time->tm_min = bcd_to_bin(time->tm_min);
    time->tm_hour = bcd_to_bin(time->tm_hour);
    time->tm_mday = bcd_to_bin(time->tm_mday);
    time->tm_mon = bcd_to_bin(time->tm_mon);
    time->tm_year = bcd_to_bin(time->tm_year);
    time->tm_wday = bcd_to_bin(time->tm_wday);
    time->century = bcd_to_bin(time->century);
    time->tm_yday = get_yday(time);
    time->tm_isdst = -1;
}

time_t mktime(time_val *time) {
    time_t res;
    int year;       // 1970 年开始的年数

    // 以下逻辑为将公元历年份转换成相对 1970 年的偏移量
    if (time->tm_year >= 70)    // 这是从 1900 年开始的年数
        year = time->tm_year - 70;
    else                        // 这是从 2000 年开始的年数
        year = time->tm_year + 100 - 70;

    // 这些年经过的秒数
    res = year * YEAR;

    // 已经过去的闰年，每个加 1 天
    res += ((year + 1) / 4) * DAY;

    // 已经过完的月份的时间
    res += month[time->tm_mon] * DAY;

    // 如果是闰年，并且 2 月已经过去了，则加上一天
    if (is_leap(ad_year(time->tm_year)) && time->tm_mon > 2) {
        res += DAY;
    }

    // 这个月过去的天
    res += (time->tm_mday - 1) * DAY;

    // 今天过去的小时
    res += time->tm_hour * HOUR;

    // 这个小时过去的分钟
    res += time->tm_min * MINUTE;

    // 这个分钟过去的秒
    res += time->tm_sec;

    return res;
}

void time_init() {
    time_val time;
    time_read(&time);

    u32 startup_time = mktime(&time);

    LOGK("startup time: %d%d-%02d-%02d %02d:%02d:%02d\n",
            time.century,
            time.tm_year,
            time.tm_mon,
            time.tm_mday,
            time.tm_hour,
            time.tm_min,
            time.tm_sec);
    LOGK("startup seconds from 1970: %ds\n", startup_time);
}