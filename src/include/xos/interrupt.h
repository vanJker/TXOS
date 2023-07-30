#ifndef XOS_INTERRUPT_H
#define XOS_INTERRUPT_H

#include <xos/types.h>

#define IDT_SIZE 256    // 一共 256 个中断描述符

// 中断描述符
typedef struct gate_t /* 共 8 个字节 */
{
    u16 offset_low;     // 段内偏移 0 ~ 15 位
    u16 selector;       // 代码段选择子
    u8 reserved;        // 保留不用
    u8 type : 4;        // 任务门/中断门/陷阱门
    u8 segment : 1;     // segment = 0 表示系统段
    u8 DPL : 2;         // 使用 int 指令访问的最低权限
    u8 present : 1;     // 是否有效
    u16 offset_high;    // 段内偏移 16 ~ 31 位
} _packed gate_t;

// 中断处理函数
typedef void *handler_t;

void interrupt_init();

#endif