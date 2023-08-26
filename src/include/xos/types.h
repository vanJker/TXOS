#ifndef XOS_TYPES_H
#define XOS_TYPES_H

#define EOF -1 // End Of File 文件结束符
#define NULL ((void *)0) // 空指针
#define EOS '\0' // 字符串结束符

// __cplusplus 宏是判定当前打开语言是否为 c++
#ifndef __cplusplus
#define bool  _Bool // C99 定义的关键字是 _Bool
#define true  1
#define false 0
#endif

// gcc 用于定义紧凑的结构体
#define _packed __attribute__((packed))

// gcc 用于省略函数的栈帧
#define _ofp __attribute__((optimize("omit-frame-pointer")))

// gcc 用于定义内联函数
#define _inline __attribute__((always_inline)) inline

// 获取非空数组的元素个数
#define NELEM(a) (sizeof(a) / sizeof(a[0]))

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

// 描述符表指针
typedef struct pointer_t /* 48 位 */
{
    u16 limit; // size - 1
    u32 base;
} __attribute__((packed)) pointer_t;

#endif