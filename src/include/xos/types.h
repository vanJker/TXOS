#ifndef XOS_TYPES_H
#define XOS_TYPES_H

#define EOF -1 // End Of File 文件结束符
#define NULL 0 // 空指针

#define bool  _Bool // C99 定义的关键字是 _Bool
#define true  1
#define false 0

#define _packed __attribute__((packed)) // gcc 用于定义紧凑的结构体

typedef unsigned int size_t;

// 有符号数
typedef char      i8;
typedef short     i16;
typedef int       i32;
typedef long long i64;

// 无符号数
typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;

// 浮点数
typedef float  f32;
typedef double f64;

#endif