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
    u32 memory_size;        // 物理内存大小
} memory_manager_t;
// 内存管理器
static memory_manager_t mm;

// 内核地址空间管理器
typedef struct kmm_t {
    u32 kernel_page_dir;    // 内核页目录所在物理地址
    u32 *kernel_page_table; // 内核页表所在的物理地址数组（内核页表连续存储）
    size_t kpgtbl_len;      // 内核页表地址数组的长度
    u32 kernel_space_size;  // 内核地址空间大小
} kmm_t;
// 内核页表索引
static u32 KERNEL_PAGE_TABLE[] = {
    0x2000,
    0x3000,
};
// 内核地址空间管理器
static kmm_t kmm = {
    .kernel_page_dir = 0x1000,
    .kernel_page_table = KERNEL_PAGE_TABLE,
    .kpgtbl_len = NELEM(KERNEL_PAGE_TABLE),
    .kernel_space_size = NELEM(KERNEL_PAGE_TABLE) * 1024 * PAGE_SIZE,
};

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
    mm.memory_size = mm.total_pages * PAGE_SIZE;

    assert(mm.alloc_base == MEMORY_ALLOC_BASE); // 可用内存起始地址为 1M
    ASSERT_PAGE_ADDR(mm.alloc_base); // 可用内存按页对齐

    LOGK("ARDS count: %d\n", cnt);
    LOGK("Free memory base: 0x%p\n", mm.alloc_base);
    LOGK("Free memory size: 0x%p\n", mm.alloc_size);
    LOGK("Total pages: %d\n", mm.total_pages);
    LOGK("Free  pages: %d\n", mm.free_pages);

    // 判断物理内存是否足够
    if (mm.memory_size < kmm.kernel_space_size) {
        panic("Physical memory is %dM to small, at least %dM needed.\n",
                mm.memory_size / (1 * 1024 * 1024),
                kmm.kernel_space_size / (1 * 1024 * 1024)
        );
    }

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

u32 get_cr3() {
    // 根据函数调用约定，将 cr3 的值复制到 eax 作为返回值
    asm volatile("movl %cr3, %eax");
}

void set_cr3(u32 pde) {
    ASSERT_PAGE_ADDR(pde);
    // 先将 pde 复制到 eax，再将 eax 的值复制到 cr3
    asm volatile("movl %%eax, %%cr3"::"a"(pde));
}

// 将 cr0 的最高位 PG 置为 1，启用分页机制
static _inline void enable_page() {
    asm volatile(
        "movl %cr0, %eax\n"
        "orl $0x80000000, %eax\n"
        "movl %eax, %cr0\n"
    );
}

// 初始化页表项，设置为指定的页索引 | U | W | P
static void page_entry_init(page_entry_t *entry, u32 index) {
    *(u32 *)entry = 0;
    entry->present = 1;
    entry->write = 1;
    entry->user = 1;
    entry->index = index;
}

// 初始化内核地址空间映射（恒等映射）
void kernel_map_init() {
    page_entry_t *kpage_dir = (page_entry_t *)(kmm.kernel_page_dir);
    memset(kpage_dir, 0, PAGE_SIZE); // 清空内核页目录

    idx_t index = 0; // 页索引
    // 将内核页目录项设置为对应的内核页表索引
    for (idx_t pde_idx = 0; pde_idx < kmm.kpgtbl_len; pde_idx++) {
        page_entry_t *pde = &kpage_dir[pde_idx];
        page_entry_t *kpage_table = (page_entry_t *)(kmm.kernel_page_table[pde_idx]);

        page_entry_init(pde, PAGE_IDX(kpage_table));
        memset(kpage_table, 0, PAGE_SIZE); // 清空当前的内核页表

        // 恒等映射前 1024 个页，即前 4MB 内存空间
        for (idx_t pte_idx = 0; pte_idx < PAGE_ENTRY_SIZE; pte_idx++, index++) {
            // 第 0 页不进行映射，这样使用空指针访问时，会触发缺页异常
            if (index == 0) continue;

            page_entry_t *pte = &kpage_table[pte_idx];
            page_entry_init(pte, index);
            mm.memory_map[index] = 1; // 设置物理内存数组，该页被占用
        }
    }
    
    // 将最后一个页表指向页目录自己，方便修改页目录个页表
    page_entry_t *entry = &kpage_dir[PAGE_ENTRY_SIZE - 1];
    page_entry_init(entry, PAGE_IDX(kmm.kernel_page_dir));

    // 设置 cr3 寄存器
    set_cr3((u32)kpage_dir);

    // 启用分页机制
    enable_page();
    
    BMB;
}

// 获取内核页目录
static page_entry_t *get_pde() {
    // return (page_entry_t *)kmm.kernel_page_dir;
    return (page_entry_t *)(0xfffff000);
}

// 获取 vaddr 所在的内核页表
static page_entry_t *get_pte(u32 vaddr) {
    // return (page_entry_t *)kmm.kernel_page_table[PDE_IDX(vaddr)];
    return (page_entry_t *)(0xffc00000 | (PDE_IDX(vaddr) << 12));
}

// 刷新 TLB
static void flush_tlb(u32 vaddr) {
    asm volatile("invlpg (%0)" ::"r"(vaddr)
                 : "memory");
}

void memory_test() {
    BMB;

    // 将 20M，即 0x140_0000 地址映射到 64M 0x400_0000 处

    // 我们还需要一个物理页来存放额外的页表

    u32 vaddr = 0x4000000; // 线性地址几乎可以是任意的（在 4G 内即可）
    u32 paddr = 0x1400000; // 物理地址必须要确定存在（必须在 32M 内）
    u32 paddr2 = 0x1500000; // 物理地址必须要确定存在（必须在 32M 内）
    u32 table = 0x900000;  // 页表也必须是物理地址

    page_entry_t *kpage_dir = get_pde();
    page_entry_t *pde = &kpage_dir[PDE_IDX(vaddr)];
    page_entry_init(pde, PAGE_IDX(table));

    page_entry_t *kpage_table = get_pte(vaddr);
    page_entry_t *pte = &kpage_table[PTE_IDX(vaddr)];
    page_entry_init(pte, PAGE_IDX(paddr));

    BMB;

    char *ptr = (char *)vaddr;
    ptr[0] = 'a';

    BMB;

    page_entry_init(&pte[PTE_IDX(vaddr)], PAGE_IDX(paddr2));
    flush_tlb(vaddr);

    BMB;

    ptr[2] = 'b';

    BMB;
}