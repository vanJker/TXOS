#ifndef XOS_STDLIB_H
#define XOS_STDLIB_H

#include <xos/types.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

// 将 x 以 k 为单位向上取整
#define ROUND_UP(x, k) (((x) + (k)-1) & -(k))

// 阻塞 count 下
void delay(u32 count);

// 一直阻塞
void hang();

// BCD 码转为二进制编码
u8 bcd_to_bin(u8 value);

// 二进制编码转成 BCD 码
u8 bin_to_bcd(u8 value);

// 向上取整除法
u32 div_round_up(u32 a, u32 b);

#endif