# 037 时间

在计算机系统中，时间与时钟有一些差别：时钟是指一定时间间隔的时间片，而时间则是指实时时间。

## 1. CMOS 信息

PC 机的 CMOS (Complementary metal oxide semiconductor 互补金属氧化物半导体) 内存，实际上是由电池供电的 64 或 128 字节 RAM 内存块，是系统时钟芯片的一部分。有些机器还有更大的内存容量。由 IBM 于 1984 年引入 PC 机，当时使用的芯片是摩托罗拉 MC146818A。

该 64 字节的 CMOS 原先在 IBM PC-XT 机器上用于保存时钟和日期信息，存放的格式是 **BCD 码**。由于这些信息仅用去 14 字节，剩余的字节就用来存放一些系统配置数据了。这里有独立的电池为其供电。

CMOS 的地址空间是在基本地址空间之外的。因此其中不包括可执行的代码。它需要使用在端口 `0x70`，`0x71` 使用 `in` 和 `out` 指令来访问。

为了读取指定偏移位置的字节，首先需要使用 `out` 向端口 `0x70` 发送指定字节的偏移值，然后使用 `in` 指令从 `0x71` 端口读取指定的字节信息。不过在选择字节（寄存器）时最好屏蔽到 NMI 中断。

下面是 CMOS 内存信息的一张简表。

| 地址偏移值  | 内容说明                    |
| ----------- | --------------------------- |
| 0x00        | 当前秒值 (实时钟)           |
| 0x01        | 闹钟秒值                    |
| 0x02        | 当前分钟 (实时钟)           |
| 0x03        | 闹钟分钟值                  |
| 0x04        | 当前小时值 (实时钟)         |
| 0x05        | 闹钟小时值                  |
| 0x06        | 一周中的当前天 (实时钟)     |
| 0x07        | 一月中的当日日期 (实时钟)   |
| 0x08        | 当前月份 (实时钟)           |
| 0x09        | 当前年份 (实时钟)           |
| 0x0a        | RTC 状态寄存器 A            |
| 0x0b        | RTC 状态寄存器 B            |
| 0x0c        | RTC 状态寄存器 C            |
| 0x0d        | RTC 状态寄存器 D            |
| 0x0e        | POST 诊断状态字节           |
| 0x0f        | 停机状态字节                |
| 0x10        | 磁盘驱动器类型              |
| 0x11        | 保留                        |
| 0x12        | 硬盘驱动器类型              |
| 0x13        | 保留                        |
| 0x14        | 设备字节                    |
| 0x15        | 基本内存 (低字节)           |
| 0x16        | 基本内存 (高字节)           |
| 0x17        | 扩展内存 (低字节)           |
| 0x18        | 扩展内存 (高字节)           |
| 0x19 ~ 0x2d | 保留                        |
| 0x2e        | 校验和 (低字节)             |
| 0x2f        | 校验和 (高字节)             |
| 0x30        | 1Mb 以上的扩展内存 (低字节) |
| 0x31        | 1Mb 以上的扩展内存 (高字节) |
| 0x32        | 当前所处世纪值              |
| 0x33        | 信息标志                    |

## 2. NMI

NMI - Non-Maskable Interrupt / 不可屏蔽中断

这是一种特殊的中断，无法通过设置中断屏蔽字来屏蔽这类中断。但是可以通过向 `0x70` 端口写入的 8 位的最高位 `0x80`，即将最高位（第 7 位）置 1 实现屏蔽 NMI 中断。

## 3. 一些问题

- 散装芯片？

CMOS 芯片集成了很多功能，有些甚至和时间无关，其中保存了 BIOS 启动时需要的一些信息，所以 CMOS 寄存器的值尽量不要修改，仅作只读之用，除非你非常明白自己在做什么，否则你需要更新 CMOS **校验和**。另外 CMOS 芯片还集成了 NMI 的开关。总之，在那个勤俭节约的年代，芯片只要有空间就要充分利用。节约成本，不过这确实留下了很多兼容性的包袱，鱼和熊掌，不可兼得。**先让程序跑起来吧，剩下的以后再说**。

---

- 世纪寄存器？

最开始的 CMOS 中没有世纪寄存器，在接近 2000 年的时候，硬件厂商开始意识到这可能是个问题，所以添加了世纪寄存器 (`0x32`)，但是这并没有一个固定的标准，导致不同的硬件厂商使用了不同的寄存器。

---

- 日历？

公元历法，1582 年，时任罗马教皇格里高利十三世予以批准执行，替代了原先的儒略历(Julian Calender)，故又称为格里高利历(Gregorian calendar)。

其中每四年一个闰年，每四百年减去三个闰年。

这里主要的原因是每个太阳年的时间大约是 365.2422 天；

但是，如果每年以 365 天计算的话，那么四年就要少 0.2422 * 4 = 0.9688 天，于是每隔四年会加一个闰年，这样每四年多计算了 0.0312 天；

但是，如果每四年都加天的话，那么四百年之后又多了 0.0312 * 100 = 3.12 天，于是在前面的每 100 年，又去掉一个闰年；

这样 400 年的误差被控制在了 0.12 天；这 0.12 天的误差要起效果，需要等待至少 3200 年，所以可能就忽略了吧。

## 4. 代码分析

### 4.1 BCD 码和二进制编码互相转换

由于在外设读取的实时时间有可能是使用 BCD 码的，所以需要实现这个功能：

```c
/* include/xos/stdlib.h */

// BCD 码转为二进制编码
u8 bcd_to_bin(u8 value);

// 二进制编码转成 BCD 码
u8 bin_to_bcd(u8 value);
```

BCD 码和二进制编码的相互转换：

```c
/* kernel/stdlib.c */

u8 bcd_to_bin(u8 value) {
    return (value & 0xf) + (value >> 4) * 10;
}

u8 bin_to_bcd(u8 value) {
    // 需要保证这个函数接受的 value 在十进制下至多为 2 位数
    assert(value < 100);
    return (value % 10) + ((value / 10) << 4);
}
```

**注意：在 `(value / 10) << 4` 处必须再用一个括号括起来，保证运算的优先级（C语言中位运算的优先级低于算术运算，大坑）。**

### 4.2 声明时间相关类型和函数

声明 `time_val` 的结构体类型，以及相关功能：

```c
/* include/xos/time.h */

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

// 启动系统时，调用该函数打印当前时间
void time_init();
```

### 4.3 CMOS

根据原理说明，定义一些与 CMOS 操作相关的常量和函数：

```c
/* kernel/time.c */

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

// 屏蔽 NMI 中断
#define CMOS_NMI 0x80

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

// 获取 CMOS 的 addr 索引处的数据
u8 cmos_read(u8 addr) {
    outb(CMOS_ADDR_PORT, CMOS_NMI | addr);
    return inb(CMOS_DATA_PORT);
}
```

### 4.4 读取实时时间

通过 CMOS 读取到实时时间的 BCD 码，再转换为二进制编码的时间值：

```c
/* kernel/time.c */

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
```

其中的函数 `get_yday()` 的功能为，获取当前时间在本年中已经过去的天数：

```c
/* kernel/time.c */

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
```

### 4.5 `mktime`

`mktime()` 这个函数的功能为，将当前时间转换成相对于 1970 年开始的秒数：

```c
/* kernel/time.c */

time_t mktime(time_val *time) {
    time_t res;
    int year;       // 1970 年开始的年数

    // 以下逻辑为将公元历年份转换成相对 1970 年的偏移量
    if (time->tm_year >= 70) {  // 这是从 1900 年开始的年数
        year = time->tm_year - 70;
    } else {                    // 这是从 2000 年开始的年数
        year = time->tm_year + 100 - 70;
    }

    // 这些年经过的秒数
    res = year * YEAR;
    // 已经过去的闰年，每个加 1 天
    res += ((year + 1) / 4) * DAY;
    // 已经过完的月份的时间
    res += month[time->tm_mon] * DAY;
    // 如果是闰年，并且 2 月已经过去了，则加上一天
    if (is_leap(ad_year(tm->tm_year)) && time->tm_mon > 2) {
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
```

### 4.6 `time_init`

这个函数的功能为在系统初始化时打印当前时间：

```c
/* kernel/time.c */

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
```

### 4.7 其它辅助函数

剩余的为辅助函数，即判断是否为闰年的函数 `is_leap()`，以及将 CMOS 读取的年数转换成公历年份的函数 `ad_year()`：

```c
// 判断是否是闰年，year 是公历年份
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
```

## 5. 功能测试

```c
void kernel_init() {
    console_init();
    gdt_init();
    interrupt_init();
    // clock_init();
    time_init();

    // asm volatile("sti"); // 打开中断

    return;
}
```

预期为，系统启动后时会打印系统初始化时的实时时间。

--- 

- 时区问题

但是在 qemu 上测试时，打印的实时时间会比现实的实时时间相差几个小时。这是因为 qemu 的时区设置导致的，qemu 默认是时区为 UTC。所以，我们需要加入一个新参数将 qemu 的时区，设置成我们当前所在地的时区。

```make
QFLAGS := -m 32M \
			-boot c \
            ...
# 将时区设置成当前所在地时区
			-rtc base=localtime \ 
```

现在，在 qemu 上测试会符合我们当前的时区时间。

## 6. 参考引用

- 赵炯 - 《Linux 内核完全注释》
- <https://wiki.osdev.org/CMOS>
- <https://en.wikipedia.org/wiki/Calendar>
- <https://en.wikipedia.org/wiki/Gregorian_calendar>