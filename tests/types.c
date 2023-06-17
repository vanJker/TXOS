#include <xos/types.h>
#include <stdio.h>

typedef struct descriptor /* 共 8 个字节 */
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
} _packed descriptor;

int main(int argc, char *argv[]) {
    printf("size of size_t: %d Byte(s)\n", sizeof(size_t));

    // 无符号数
    printf("size of u8:  %d Byte(s)\n", sizeof(u8));
    printf("size of u16: %d Byte(s)\n", sizeof(u16));
    printf("size of u32: %d Byte(s)\n", sizeof(u32));
    printf("size of u64: %d Byte(s)\n", sizeof(u64));

    // 有符号数
    printf("size of i8:  %d Byte(s)\n", sizeof(i8));
    printf("size of i16: %d Byte(s)\n", sizeof(i16));
    printf("size of i32: %d Byte(s)\n", sizeof(i32));
    printf("size of i64: %d Byte(s)\n", sizeof(i64));

    // 浮点数
    printf("size of f32: %d Byte(s)\n", sizeof(f32));
    printf("size of f64: %d Byte(s)\n", sizeof(f64));
    
    printf("size of descriptor: %d Byte(s)\n", sizeof(descriptor));

    descriptor des;

    return 0;
}