#ifndef XOS_STDLIB_H
#define XOS_STDLIB_H

#include <xos/types.h>

// 阻塞 count 下
void delay(u32 count);

// 一直阻塞
void hang();

// BCD 码转为二进制编码
u8 bcd_to_bin(u8 value);

// 二进制编码转成 BCD 码
u8 bin_to_bcd(u8 value);

#endif