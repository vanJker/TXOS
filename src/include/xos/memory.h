#ifndef XOS_MEMORY_H
#define XOS_MEMORY_H

#include <xos/types.h>

#define PAGE_SIZE   0x1000 // 页大小为 4K
#define MEMORY_ALLOC_BASE 0x100000 // 32 位可用内存起始地址为 1M

// 获取 addr 的页索引
#define PAGE_IDX(addr) ((u32)addr >> 12) 

// 获取 idx 的页地址
#define PAGE_ADDR(idx) ((u32)idx << 12)

// 判断 addr 是否为页的起始地址
#define ASSERT_PAGE_ADDR(addr) ((addr & 0xfff) == 0)

#endif