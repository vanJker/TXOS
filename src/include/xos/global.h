#ifndef XOS_GLOBAL_H
#define XOS_GLOBAL_H

#include <xos/types.h>

#define GDT_SIZE 128

#define KERNEL_CODE_IDX 1 // 内核代码段描述符索引
#define KERNEL_DATA_IDX 2 // 内核数据段描述符索引
#define KERNEL_TSS_IDX  3 // 内核 TSS 描述符索引
#define USER_CODE_IDX   4 // 用户代码段描述符索引
#define USER_DATA_IDX   5 // 用户数据段描述符索引

#define KERNEL_CODE_SELECTOR (KERNEL_CODE_IDX << 3)     // 内核代码段选择子 (ring 0)
#define KERNEL_DATA_SELECTOR (KERNEL_DATA_IDX << 3)     // 内核数据段选择子 (ring 0)
#define KERNEL_TSS_SELECTOR  (KERNEL_TSS_IDX  << 3)     // 内核数据段选择子 (ring 0)
#define USER_CODE_SELECTOR   (USER_CODE_IDX << 3 | 0x3) // 内核代码段选择子 (ring 3)
#define USER_DATA_SELECTOR   (USER_DATA_IDX << 3 | 0x3) // 内核代码段选择子 (ring 3)

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
} _packed descriptor_t;

// 段选择子
typedef struct selector_t /* 16 位 */
{
    u8 RPL : 2;      // Request Privilege Level
    u8 TI : 1;       // 0 表示为全局描述符，1 表示为局部描述符
    u16 index : 13;  // 全局描述符表索引
} _packed selector_t;

// 描述符表指针
typedef struct pointer_t /* 48 位 */
{
    u16 limit; // size - 1
    u32 base;
} __attribute__((packed)) pointer_t;

// 任务状态段 (TSS)
typedef struct tss_t {
    u32 backlink;   // 前一个硬件任务的链接，保持了上一个任务状态段的段选择子
    u32 esp0;       // ring 0 的栈顶地址
    u32 ss0;        // ring 0 的栈段选择子
    u32 esp1;       // ring 1 的栈顶地址
    u32 ss1;        // ring 1 的栈段选择子
    u32 esp2;       // ring 2 的栈顶地址
    u32 ss2;        // ring 2 的栈段地址
    u32 cr3;
    u32 eip;
    u32 eflags;
    u32 eax;
    u32 ecx;
    u32 edx;
    u32 ebx;
    u32 esp;
    u32 ebp;
    u32 esi;
    u32 edi;
    u32 es;
    u32 cs;
    u32 ss;
    u32 ds;
    u32 fs;
    u32 gs;
    u32 ldtr;       // 局部描述符选择子
    u16 trace : 1;  // 如果置位，硬件任务切换时会触发一个调试异常
    u16 reserved : 15; // 保留区域
    u16 iobase;     // I/O Map Base Address（从 TSS 起始地址到 I/O 权限位图的偏移量，以字节为单位）
    u32 ssp;        // 任务影子栈指针
} _packed tss_t;

// 初始化内核全局描述符表以及指针
void gdt_init();

// 初始化任务状态段（TSS）及其描述符
void tss_init();

#endif