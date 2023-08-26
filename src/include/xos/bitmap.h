#ifndef XOS_BITMAP_H
#define XOS_BITMAP_H

#include <xos/types.h>

typedef struct bitmap_t {
    u8 *bits;   // 位图缓冲区
    u32 size;   // 位图缓冲区长度（以字节为单位）
    u32 offset; // 位图开始的偏移（以比特为单位）
    u32 length; // 位图的长度
} bitmap_t;

// 构造一个位图
void bitmap_new(bitmap_t *map, u8 *bits, u32 size, u32 offset);

// 将位图全部初始化为零
void bitmap_init(bitmap_t *map, u8 *bits, u32 size, u32 offset);

// 位图是否包含某一位
bool bitmap_contains(bitmap_t *map, u32 index);

// 将某一位插入位图
void bitmap_insert(bitmap_t *map, u32 index);

// 将某一位从位图中删除
void bitmap_remove(bitmap_t *map, u32 index);

// 往位图中插入连续 n 位的 1。返回满足条件的起始位。
// 如果没有满足条件的空闲空间，返回 EOF。
size_t bitmap_insert_nbits(bitmap_t *map, u32 n);

#endif