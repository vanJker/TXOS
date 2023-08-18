#include <xos/xos.h>
#include <xos/memory.h>
#include <xos/assert.h>
#include <xos/debug.h>

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
    u32 alloc_base;  // 可用内存基址（应该等于 1M）
    u32 alloc_size;  // 可用内存大小
    u32 free_pages;  // 可用内存页数
    u32 total_pages; // 所有内存页数
} memory_manager_t;

static memory_manager_t mm;


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
    assert((mm.alloc_base & 0xfff) == 0); // 可用内存按页对齐

    LOGK("ARDS count: %d\n", cnt);
    LOGK("Free memory base: 0x%p\n", (u32)mm.alloc_base);
    LOGK("Free memory size: 0x%p\n", (u32)mm.alloc_size);
    LOGK("Total pages: %d\n", mm.total_pages);
    LOGK("Free  pages: %d\n", mm.free_pages);
}