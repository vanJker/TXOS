#include <xos/xos.h>
#include <xos/memory.h>
#include <xos/assert.h>
#include <xos/debug.h>
#include <xos/stdlib.h>
#include <xos/string.h>

#define ZONE_VALID    1 // ards 可用内存区域
#define ZONE_RESERVED 2 // ards 不可用内存区域

// 地址描述符
typedef struct ards_t {
    u64 base; // 内存基址
    u64 size; // 内存大小
    u32 type; // 内存类型
} ards_t;

// 内存管理器
typedef struct memory_manager_t {
    u32 alloc_base;         // 可分配物理内存基址（应该等于 1M）
    u32 alloc_size;         // 可分配物理内存大小
    u32 free_pages;         // 空闲物理内存页数
    u32 total_pages;        // 所有物理内存页数
    u32 start_page_idx;     // 可分配物理内存的起始页索引
    u8 *memory_map;         // 物理内存数组
    u32 memory_map_pages;   // 物理内存数组占用的页数
} memory_manager_t;

static memory_manager_t mm;


static void memory_map_init();
extern u32 kernel_magic;
extern u32 ards_addr;
void memory_init() {
    u32 cnt;
    ards_t *ptr;

    // 如果是 onix loader 进入的内核
    if (kernel_magic == XOS_MAGIC) {
        cnt = *(u32 *)ards_addr;
        ptr = (ards_t *)(ards_addr + 4);

        for (size_t i = 0; i < cnt; i++, ptr++) {
            LOGK("ZONE %d:[base]0x%p,[size]:0x%p,[type]:%d\n",
                 i, (u32)ptr->base, (u32)ptr->size, (u32)ptr->type);
            
            if (ptr->type == ZONE_VALID 
                && ptr->base >= MEMORY_ALLOC_BASE 
                && ptr->size > mm.alloc_size) {
                mm.alloc_base = ptr->base;
                mm.alloc_size = ptr->size;
            }
        }
    } else {
        panic("Memory init magic unknown 0x%p\n", kernel_magic);
    }

    mm.free_pages = PAGE_IDX(mm.alloc_size);
    mm.total_pages = mm.free_pages + PAGE_IDX(mm.alloc_base);

    assert(mm.alloc_base == MEMORY_ALLOC_BASE); // 可用内存起始地址为 1M
    ASSERT_PAGE_ADDR(mm.alloc_base); // 可用内存按页对齐

    LOGK("ARDS count: %d\n", cnt);
    LOGK("Free memory base: 0x%p\n", (u32)mm.alloc_base);
    LOGK("Free memory size: 0x%p\n", (u32)mm.alloc_size);
    LOGK("Total pages: %d\n", mm.total_pages);
    LOGK("Free  pages: %d\n", mm.free_pages);

    // 初始化物理内存数组
    memory_map_init();
}

static void memory_map_init() {
    // 初始化物理内存数组
    mm.memory_map = (u8 *)mm.alloc_base;

    // 计算物理内存数组占用的页数
    mm.memory_map_pages = div_round_up(mm.total_pages, PAGE_SIZE);
    LOGK("Memory map pages count: %d\n", mm.memory_map_pages);

    // 更新空闲页数
    mm.free_pages -= mm.memory_map_pages;

    // 清空物理内存数组
    memset((void *)mm.memory_map, 0, mm.memory_map_pages * PAGE_SIZE);

    // 设置前 1M 的内存和物理内存数组所在的内存部分为占用状态
    mm.start_page_idx = PAGE_IDX(mm.alloc_base) + mm.memory_map_pages;
    for (size_t i = 0; i < mm.start_page_idx; i++) {
        mm.memory_map[i] = 1;
    }

    LOGK("Total pages: %d\n", mm.total_pages);
    LOGK("Free  pages: %d\n", mm.free_pages);
}

// 分配一页物理内存，返回该页的起始地址
static u32 alloc_page() {
    for (size_t i = mm.start_page_idx; i < mm.total_pages; i++) {
        // 寻找没被占用的物理页
        if (!mm.memory_map[i]) {
            mm.memory_map[i] = 1;
            assert(mm.free_pages > 0);
            mm.free_pages--;
            LOGK("Alloc page 0x%p\n", PAGE_ADDR(i));
            return PAGE_ADDR(i);
        }
    }
    panic("Out of Memory!!!");
}

// 释放一页物理内存，提供的地址必须是该页的起始地址
static void free_page(u32 addr) {
    // 提供的地址必须是该页的起始地址
    ASSERT_PAGE_ADDR(addr);

    // 获取页索引 idx
    u32 idx = PAGE_IDX(addr);

    // 页索引 idx 在可分配内存范围内
    assert(idx >= mm.start_page_idx && idx < mm.total_pages);

    // 不释放空闲页
    assert(mm.memory_map[idx] >= 1);

    // 更新页引用次数
    mm.memory_map[idx]--;

    // 如果该页引用次数为 0，则更新空闲页个数
    if (!mm.memory_map[idx]) {
        mm.free_pages++;
        assert(mm.free_pages > 0 && mm.free_pages < mm.total_pages);
    }

    LOGK("Free page 0x%p\n", addr);
}

void memory_test() {
    size_t cnt = 10;
    u32 pages[cnt];
    for (size_t i = 0; i < cnt; i++) {
        pages[i] = alloc_page();
    }
    for (size_t i = 0; i < cnt; i++) {
        free_page(pages[i]);
    }
}