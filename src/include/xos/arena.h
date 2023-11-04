#ifndef XOS_ARENA_H
#define XOS_ARENA_H

#include <xos/types.h>
#include <xos/list.h>


typedef list_node_t block_t; // 内存块

// 内存块描述符
typedef struct arena_descriptor_t {
    size_t total_block; // 一页内存可以分成多少块
    size_t block_size;  // 块大小 / 粒度
    list_t free_list;   // 该粒度的空闲块队列
} arena_descriptor_t;

// 一页或多页内存的结构说明
typedef struct arena_t {
    arena_descriptor_t *desc;   // 该 arena 的内存块描述符
    size_t count;               // 该 arena 当前剩余的块数（large = 0）或 页数（large = 1）
    bool   large;               // 表示是不是超过了 1024 字节（即超过了描述符的最大块粒度）
    u32    magic;               // 魔数，用于检测该结构体是否被篡改
} arena_t;

// arena 初始化内核堆管理
void arena_init();

// 分配一块大小至少为 size 的内存块
void *kmalloc(size_t size);

// 释放指针 ptr 所指向的内存块
void kfree(void *ptr);

#endif