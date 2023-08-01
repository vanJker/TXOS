#ifndef XOS_STDLIB_H
#define XOS_STDLIB_H

#include <xos/types.h>

// 阻塞 count 下
void delay(u32 count);

// 一直阻塞
void hang();

#endif