#include <xos/xos.h>
#include <xos/memory.h>
#include <xos/assert.h>
#include <xos/debug.h>
#include <xos/stdlib.h>
#include <xos/string.h>
#include <xos/bitmap.h>
#include <xos/multiboot2.h>
#include <xos/task.h>

#define ZONE_VALID    1 // ards 可用内存区域
#define ZONE_RESERVED 2 // ards 不可用内存区域

// 魔数 - bootloader 启动时为 XOS_MAGIC，multiboot2 启动时为 MULTIBOOT2_MAGIC
extern u32 magic;
// 地址 - bootloader 启动时为 ARDS 的起始地址，bootloader 启动时为 Boot Information 的起始地址
extern u32 addr;

static void memory_map_init();

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
    bitmap_t kernel_vmap;   // 内核虚拟内存空间位图
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

void memory_init() {
    u32 cnt;

    if (magic == XOS_MAGIC) {
        // 如果是 XOS bootloader 进入的内核
        cnt = *(u32 *)addr;
        ards_t *ptr = (ards_t *)(addr + 4);

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
    } else if (magic == MULTIBOOT2_MAGIC) {
        // 如果是 multiboot2 进入的内核
        u32 total_size = *(u32 *)addr;
        multiboot2_tag_t *tag = (multiboot2_tag_t *)(addr + 8);

        LOGK("Multiboot2 Information Size: 0x%p\n", total_size);

        // 寻找类型为 mmap 的 tag
        while (tag->type != MULTIBOOT2_TAG_TYPE_END) {
            if (tag->type == MULTIBOOT2_TAG_TYPE_END) {
                // 如果到最后都没有找到 mmap 类型的 tag，则触发 panic 
                panic("Memory init without mmap tag!!!\n");
            }
            if (tag->type == MULTIBOOT2_TAG_TYPE_MMAP) {
                break;
            }
            // 需要填充，使得下一个 tag 以 8 字节对齐
            tag = (multiboot2_tag_t *)(ROUND_UP((u32)tag + tag->size, 8));
        }

        multiboot2_tag_mmap_t *mmap_tag = (multiboot2_tag_mmap_t *)tag;
        multiboot2_mmap_entry_t *entry = mmap_tag->entries;
        cnt = 0;
        while ((u32)entry < (u32)tag + tag->size) {
            LOGK("ZONE %d:[base]0x%p,[size]:0x%p,[type]:%d\n",
                 cnt++, (u32)entry->addr, (u32)entry->len, (u32)entry->type);
            
            if (entry->type == MULTIBOOT2_MEMORY_AVAILABLE
                && entry->addr >= MEMORY_ALLOC_BASE
                && entry->len > mm.alloc_size) {
                mm.alloc_base = entry->addr;
                mm.alloc_size = entry->len;
            }

            entry = (multiboot2_mmap_entry_t *)((u32)entry + mmap_tag->entry_size);
        }
    } else {
        panic("Memory init magic unknown 0x%p\n", magic);
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

u32 get_cr2() {
    // 根据函数调用约定，将 cr2 的值复制到 eax 作为返回值
    asm volatile("movl %cr2, %eax");
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

static void kernel_vmap_init();
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

        // 每次恒等映射 1024 个页，即 4MB 内存空间
        for (idx_t pte_idx = 0; pte_idx < PAGE_ENTRY_SIZE; pte_idx++, index++) {
            // 第 0 页不进行映射，这样使用空指针访问时，会触发缺页异常
            if (index == 0) continue;

            page_entry_t *pte = &kpage_table[pte_idx];
            page_entry_init(pte, index);

            if (mm.memory_map[index] == 0) mm.free_pages--;
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

    // 初始化内核虚拟内存空间位图
    kernel_vmap_init();
}

// 初始化内核虚拟内存空间位图
static void kernel_vmap_init() {
    u8 *bits = (u8 *)0x4000;
    size_t size = div_round_up((PAGE_IDX(kmm.kernel_space_size) - mm.start_page_idx), 8);
    bitmap_init(&kmm.kernel_vmap, bits, size, mm.start_page_idx);
}

// 获取页目录
static page_entry_t *get_pde() {
    // return (page_entry_t *)kmm.kernel_page_dir;
    return (page_entry_t *)(0xfffff000);
}

// 获取虚拟内存 vaddr 所在的页表
static page_entry_t *get_pte(u32 vaddr, bool create) {
    // return (page_entry_t *)kmm.kernel_page_table[PDE_IDX(vaddr)];
    // 获取对应的 pde
    page_entry_t *pde = get_pde();
    size_t idx = PDE_IDX(vaddr);
    page_entry_t *entry = &pde[idx];

    // 没 create 选项的话，必须保证 vaddr 对应的页表是有效的
    assert(create || (!create && entry->present));

    // 如果设置了 create 且 vaddr 对应的页表无效，则分配页作为页表
    if (!entry->present && create) {
        LOGK("Get and create a page table for 0x%p\n", vaddr);
        u32 paddr = alloc_page();
        page_entry_init(entry, PAGE_IDX(paddr));
    }

    return (page_entry_t *)(0xffc00000 | (PDE_IDX(vaddr) << 12));
}

// 刷新 TLB
static void flush_tlb(u32 vaddr) {
    asm volatile("invlpg (%0)" ::"r"(vaddr)
                 : "memory");
}

// 从位图中扫描 count 个连续的页
static u32 scan_pages(bitmap_t *map, u32 count) {
    assert(count > 0);
    i32 idx = bitmap_insert_nbits(map, count);

    if (idx == EOF) {
        panic("Scan page fail!!!");
    }

    u32 addr = PAGE_ADDR(idx);
    LOGK("Scan page 0x%p count %d\n", addr, count);
    return addr;
}

// 与 scan_page 相对，重置相应的页
static void reset_pages(bitmap_t *map, u32 addr, u32 count) {
    ASSERT_PAGE_ADDR(addr);
    assert(count > 0);
    u32 idx = PAGE_IDX(addr);

    for (size_t i = 0; i < count; i++) {
        assert(bitmap_contains(map, idx + i));
        bitmap_remove(map, idx + i);
    }
}

// 分配 count 个连续的内核页
u32 kalloc_page(u32 count) {
    assert(count > 0);
    u32 vaddr = scan_pages(&kmm.kernel_vmap, count);
    LOGK("ALLOC kernel pages 0x%p count %d\n", vaddr, count);
    return vaddr;
}

// 释放 count 个连续的内核页
void kfree_page(u32 vaddr, u32 count) {
    ASSERT_PAGE_ADDR(vaddr);
    assert(count > 0);
    reset_pages(&kmm.kernel_vmap, vaddr, count);
    LOGK("FREE kernel pages 0x%p count %d\n", vaddr, count);
}

// 将虚拟地址 vaddr 映射到物理内存
void link_page(u32 vaddr) {
    ASSERT_PAGE_ADDR(vaddr); // 保证是页的起始地址

    // 获取对应的 pte
    page_entry_t *pte = get_pte(vaddr, true);
    size_t idx = PTE_IDX(vaddr);
    page_entry_t *entry = &pte[idx];

    // 获取用户虚拟内存位图，以及 vaddr 对应的页索引
    task_t *current = current_task();
    bitmap_t *vmap = current->vmap;
    idx = PAGE_IDX(vaddr);

    // 如果页面已存在映射关系，则直接返回
    if (entry->present) {
        assert(bitmap_contains(vmap, idx));
        return;
    }

    // 否则分配物理内存页，并在页表中进行映射
    assert(!bitmap_contains(vmap, idx));
    bitmap_insert(vmap, idx); // 在用户虚拟内存位图中标记存在映射关系

    u32 paddr = alloc_page();
    page_entry_init(entry, PAGE_IDX(paddr));
    flush_tlb(vaddr); // 更新页表后，需要刷新 TLB

    LOGK("LINK from 0x%p to 0x%p\n", vaddr, paddr);
}

// 取消虚拟地址 vaddr 对应的物理内存映射
void unlink_page(u32 vaddr) {
    ASSERT_PAGE_ADDR(vaddr); // 保证是页的起始地址

    // 获取对应的 pte
    page_entry_t *pte = get_pte(vaddr, true);
    size_t idx = PTE_IDX(vaddr);
    page_entry_t *entry = &pte[idx];

    // 获取用户虚拟内存位图，以及 vaddr 对应的页索引
    task_t *current = current_task();
    bitmap_t *vmap = current->vmap;
    idx = PAGE_IDX(vaddr);


    // 如果页面不存在映射关系，则直接返回
    if (!entry->present) {
        assert(!bitmap_contains(vmap, idx));
        return;
    }

    // 否则取消映射，并释放对应的物理内存页
    assert(bitmap_contains(vmap, idx));
    bitmap_remove(vmap, idx); // 在用户虚拟内存位图中标记不存在映射关系

    entry->present = 0;
    u32 paddr = PTE2PA(*entry);
    free_page(paddr);
    flush_tlb(vaddr); // 更新页表后，需要刷新 TLB

    LOGK("UNLINK from 0x%p to 0x%p\n", vaddr, paddr);
}

// 拷贝当前任务的页目录
page_entry_t *copy_pde() {
    task_t *current = current_task();
    page_entry_t *pde = (page_entry_t *)kalloc_page(1); // TODO: free
    memcpy((void *)pde, (void *)current->page_dir, PAGE_SIZE);

    // 将最后一个页表项指向页目录自身，方便修改页目录和页表
    page_entry_t *entry = &pde[1023];
    page_entry_init(entry, PAGE_IDX(pde));

    return pde;
}

// 内核页目录的物理地址
u32 get_kernel_page_dir() {
    return kmm.kernel_page_dir;
}

// 内核虚拟内存位图
bitmap_t *get_kernel_vmap() {
    return &kmm.kernel_vmap;
}

/*******************************
 ***     实现的系统调用处理     ***
 *******************************/

i32 sys_brk(void *addr) {
    LOGK("task brk 0x%p\n", addr);
    
    // 保证 brk 的地址是页的起始地址
    u32 brk = (u32)addr;
    ASSERT_PAGE_ADDR(brk);

    // 保证触发 brk 的是用户态进程
    task_t *current = current_task();
    assert(current->uid != KERNEL_TASK);

    // 保证 brk 的地址位于合法范围
    assert(KERNEL_MEMORY_SIZE <= brk && brk < USER_STACK_BOOTOM);

    u32 old_brk = current->brk; // 原先的 brk 地址
    if (old_brk > brk) {
        // 如果原先的 brk 地址高于指定的 brk 地址
        for (u32 addr = brk; addr < old_brk; addr += PAGE_SIZE) {
            unlink_page(addr);
        }
    } else if (PAGE_IDX(brk - old_brk) > mm.free_pages) {
        // 如果指定的 brk 地址高于原先的 brk 地址，且需要扩展的内存大于空闲内存
        return -1; // out of memory
    }

    // 更新进程的 brk 地址
    current->brk = brk;
    return 0;
}