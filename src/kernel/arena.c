#include <xos/arena.h>
#include <xos/memory.h>
#include <xos/assert.h>
#include <xos/stdlib.h>
#include <xos/string.h>
#include <xos/xos.h>

// 普通内存块描述符的数量（共 7 种粒度：16B、32B、64B、128B、256B、512B、1024B）
#define ARENA_DESC_COUNT 7
// 普通内存块描述符数组
static arena_descriptor_t arena_descriptors[ARENA_DESC_COUNT];

// 普通内存块的最小粒度
#define MIN_BLOCK_SIZE 16
// 普通内存块的最大粒度
#define MAX_BLOCK_SIZE arena_descriptors[ARENA_DESC_COUNT - 1].block_size

// arena 初始化内核堆管理
void arena_init() {
    size_t block_size = MIN_BLOCK_SIZE;

    for (size_t i = 0; i < ARENA_DESC_COUNT; i++) {
        arena_descriptor_t *desc = &arena_descriptors[i];
        desc->block_size = block_size;
        desc->total_block = (PAGE_SIZE - sizeof(arena_t)) / block_size;
        list_init(&desc->free_list);

        block_size <<= 1; // 块粒度每次 x2 增长
    }
}

// 获取所给 arena 的第 idx 块内存的地址
static void *get_block_from_arena(arena_t *arena, size_t idx) {
    // 保证 idx 是合法的，没有超出 arena 的范围
    assert(idx < arena->desc->total_block);

    u32 addr = (u32)arena + sizeof(arena_t);
    u32 gap = idx * arena->desc->block_size;
    
    return (void *)(addr + gap);
}

// 获取所给 block 对应的 arena 的地址
static arena_t *get_arena_from_block(block_t *block) {
    return (arena_t *)((u32)block & 0xfffff000);
}

// 分配一块大小至少为 size 的内存块
void *kmalloc(size_t size) {
    void *addr;
    arena_t *arena;
    block_t *block;

    // 如果分配内存大小大于内存块描述符的最大粒度
    if (size > MAX_BLOCK_SIZE) {
        // 计算需要内存的总大小，以及对应的页数
        size_t asize = size + sizeof(arena_t);
        size_t count = div_round_up(asize, PAGE_SIZE);

        // 分配所需内存，以及清除原有的数据
        arena = (arena_t *)kalloc_page(count);
        memset(arena, 0, count * PAGE_SIZE);
        
        // 设置 arena 内存结构说明
        arena->large = true;
        arena->count = count;
        arena->desc  = NULL;
        arena->magic = XOS_MAGIC;

        addr = (void *)((u32)arena + sizeof(arena_t));
        return addr;
    }

    // 如果分配内存大小并没有超过内存块描述符的最大粒度
    arena_descriptor_t *desc = NULL;
    for (size_t i = 0; i < ARENA_DESC_COUNT; i++) {
        desc = &arena_descriptors[i];

        // 寻找恰好大于等于分配内存大小的内存块描述符
        if (desc->block_size >= size) {
            break;
        }
    }

    assert(desc != NULL); // 必会寻找到一个合适的内存块描述符

    // 如果该内存块描述符对应的空闲块链队列为空
    if (list_empty(&desc->free_list)) {
        // 分配一页内存，以及清除原有的数据
        arena = (arena_t *)kalloc_page(1);
        memset(arena, 0, PAGE_SIZE);

        // 设置 arena 内存结构说明
        arena->large = false;
        arena->desc = desc;
        arena->count = desc->total_block;
        arena->magic = XOS_MAGIC;

        // 将新分配页中的块加入对应的空闲块队列
        for (size_t i = 0; i < desc->total_block; i++) {
            block = get_block_from_arena(arena, i);
            assert(!list_contains(&desc->free_list, block));
            list_push_back(&desc->free_list, block);
            assert(list_contains(&desc->free_list, block));
        }
    }

    // 在对应的空闲块队列中获取一个空闲块
    block = list_pop_front(&desc->free_list);
    // 清除原有的数据，因为在回收块时并没有清除
    memset(block, 0, desc->block_size);

    // 更新 arena 对应的记录
    arena = get_arena_from_block(block);
    assert(arena->magic == XOS_MAGIC && !arena->large);
    arena->count--;

    return (void *)block;
}

// 释放指针 ptr 所指向的内存块
void kfree(void *ptr) {
    // 释放空地址 / 指针是非法操作
    assert(ptr != NULL);

    // 获取对应的 arena
    block_t *block = (block_t *)ptr;
    arena_t *arena = get_arena_from_block(block);
    assert(arena->magic == XOS_MAGIC);

    // 如果是超大块（即 large = 1）
    if (arena->large) {
        kfree_page((u32)arena, arena->count);
        return;
    }

    // 如果不是超大块（即 large = 0）
    list_push_back(&arena->desc->free_list, block); // 重新加入空闲队列
    arena->count++;                                 // 更新空闲块数

    // 如果该页内存的全部块都已经被回收，则释放该页内存
    if (arena->count == arena->desc->total_block) {
        // 将该页的空闲块从对应的空闲队列当中去除
        for (size_t i = 0; i < arena->count; i++) {
            block = get_block_from_arena(arena, i);
            assert(list_contains(&arena->desc->free_list, block));
            list_remove(block);
            assert(!list_contains(&arena->desc->free_list, block));
        }

        kfree_page((u32)arena, 1);
    }
}
