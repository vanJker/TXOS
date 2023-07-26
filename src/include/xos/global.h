#ifndef XOS_GLOBAL_H
#define XOS_GLOBAL_H

#include <xos/types.h>

#define GDT_SIZE 128

// 全局描述符
typedef struct descriptor_t /* 共 8 个字节 */
{
    u16 limit_low : 16; // 段界限 0 ~ 15 位
    u32 base_low : 24;  // 基地址 0 ~ 23 位
    u8 type : 4;        // 段类型
    u8 segment : 1;     // 1 表示代码段或数据段，0 表示系统段
    u8 DPL : 2;         // Descriptor Privilege Level 描述符特权等级 0 ~ 3
    u8 present : 1;     // 存在位，1 在内存中，0 在磁盘上
    u8 limit_high : 4;  // 段界限 16 ~ 19;
    u8 available : 1;   // 该安排的都安排了，送给操作系统吧
    u8 long_mode : 1;   // 64 位扩展标志
    u8 big : 1;         // 1 表示 32 位，0 表示 16 位;
    u8 granularity : 1; // 1 表示粒度为 4KB，0 表示粒度为 1B
    u8 base_high : 8;   // 基地址 24 ~ 31 位
} __attribute__((packed)) descriptor_t;

// 段选择子
typedef struct selector_t /* 16 位 */
{
    u8 RPL : 2;      // Request PL
    u8 TI : 1;       // 0 表示为全局描述符，1 表示为局部描述符
    u16 index : 13;  // 全局描述符表索引
} __attribute__((packed)) selector_t;

// 初始化内核全局描述符表以及指针
void gdt_init();

#endif