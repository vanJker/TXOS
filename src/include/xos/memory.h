#ifndef XOS_MEMORY_H
#define XOS_MEMORY_H

#include <xos/types.h>
#include <xos/bitmap.h>

#define PAGE_SIZE 0x1000 // 页大小为 4K
#define MEMORY_ALLOC_BASE   0x100000    // 32 位可用内存起始地址为 1M
#define KERNEL_MEMORY_SIZE  0x800000    // 内核占用的内存大小
#define USER_MEMORY_TOP     0x8800000   // 用户虚拟内存的最高地址 136M

#define USER_STACK_TOP  USER_MEMORY_TOP // 用户栈顶地址 136M
#define USER_STACK_SIZE 0xa00000        // 用户栈大小 10M
#define USER_STACK_BOOTOM (USER_MEMORY_TOP - USER_STACK_SIZE) // 用户栈底地址（136M - 8M）

// 获取 addr 的页索引
#define PAGE_IDX(addr) ((u32)addr >> 12) 

// 获取 idx 的页地址
#define PAGE_ADDR(idx) ((u32)idx << 12)

// 判断 addr 是否为页的起始地址
#define ASSERT_PAGE_ADDR(addr) ((addr & 0xfff) == 0)

// 获取 addr 的页目录索引
#define PDE_IDX(addr) (((u32)addr >> 22) & 0x3ff)

// 获取 addr 的页表索引
#define PTE_IDX(addr) (((u32)addr >> 12) & 0x3ff)

// 将 PTE 转换成 PA
#define PTE2PA(pte) PAGE_ADDR((pte).index)

// 索引类型（页索引 / 页目录向索引 / 页表项索引）
typedef u32 idx_t;

// 页表/页目录项
typedef struct page_entry_t {
    u8 present : 1;  // 0 不在内存，1 在内存中
    u8 write : 1;    // 0 只读，1 可读可写
    u8 user : 1;     // 0 超级用户 DPL < 3，1 所有人 - 访问权限
    u8 pwt : 1;      // (page write through) 1 直写模式，0 回写模式
    u8 pcd : 1;      // (page cache disable) 1 禁止该页缓存，0 不禁止
    u8 accessed : 1; // 1 被访问过，用于统计使用频率
    u8 dirty : 1;    // 1 脏页，表示该页缓存被写过
    u8 pat : 1;      // (page attribute table) 页大小 0 4K，1 4M
    u8 global : 1;   // 1 全局，所有进程都用到了，该页不刷新缓存
    u8 ignored : 3;  // 保留位（该安排的都安排了，送给操作系统吧）
    u32 index : 20;  // 页索引
} _packed page_entry_t;

// 一页中页表项的数量
#define PAGE_ENTRY_SIZE (PAGE_SIZE / sizeof(page_entry_t))

void memory_init();

void kernel_map_init();

// 获取 cr2 寄存器的值
u32 get_cr2();

// 获取 cr3 寄存器的值
u32 get_cr3();

// 设置 cr3 寄存器，参数是页目录的地址
void set_cr3(u32 pde);

// 刷新 TLB 中与 vaddr 相关的项
void flush_tlb(u32 vaddr);

// 分配 count 个连续的内核页
u32 kalloc_page(u32 count);

// 释放 count 个连续的内核页
void kfree_page(u32 vaddr, u32 count);

// 初始化页表项，设置为指定的页索引 | U | W | P
void page_entry_init(page_entry_t *entry, u32 index);

// 获取页目录
page_entry_t *get_pde();

// 获取虚拟内存 vaddr 所在的页表
page_entry_t *get_pte(u32 vaddr, bool create);

// 拷贝当前任务的页目录（表示的用户空间）
page_entry_t *copy_pde();

// 释放当前任务的页目录（表示的用户空间）
void free_pde();

// 将虚拟地址 vaddr 所在的页拷贝到一个空闲物理页，并返回该物理页的物理地址
u32 copy_page(u32 vaddr);

// 内核页目录的物理地址
u32 get_kernel_page_dir();

// 内核虚拟内存位图
bitmap_t *get_kernel_vmap();

// 物理内存数组
u8 *memory_map();

// 将虚拟地址 vaddr 起始的页映射到物理内存
void link_page(u32 vaddr);

// 取消虚拟地址 vaddr 起始的页对应的物理内存映射
void unlink_page(u32 vaddr);


/*** 实现的系统调用处理 ***/

i32 sys_brk(void *addr);

#endif