# 040 物理内存管理

## 1. 原理说明

本节主要实现以下两个功能：

### 1.1 物理页的引用计数

从可用内存开始的位置分配一些页用于管理物理内存。在这些页中，我们对每个物理页使用一个字节来表示引用数量，所以一个物理页最多被引用 255 次。

页的引用次数，是指该页被进程引用的次数。例如，在 `fork` 时会增加页的引用次数（当然也可以使用更大的位宽来表示引用数量，但是作用不大，没必要）。

我们使用占据连续内存页的物理内存数组，来记录物理内存的引用次数。

### 1.2 物理页的分配与释放

实现对物理内存按页为单位进行分配和释放的功能，函数原型如下：

```c
// 分配一页物理内存
static u32 alloc_page();

// 释放一页物理内存
static void free_page(u32 addr);
```

## 2. 代码分析

### 2.1 数学功能

在 `stdlib` 库中实现一些常用的数学功能。

```c
/* include/xos/stdlib.h */

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/* lib/stdlib.c */

// 向上取整除法
u32 div_round_up(u32 a, u32 b) {
    return (a + b - 1) / b;
}
```

### 2.2 页相关计算

在 `include/xos/memory.h` 增加页相关计算的宏。

```c
// 获取 addr 的页索引
#define PAGE_IDX(addr) ((u32)addr >> 12) 

// 获取 idx 的页地址
#define PAGE_ADDR(idx) ((u32)idx << 12)

// 判断 addr 是否为页的起始地址
#define ASSERT_PAGE_ADDR(addr) ((addr & 0xfff) == 0)
```

### 2.3 物理内存数组

修改内存管理器 `mm`（位于 `kernel/memory.c`），使得它能记录物理内存数组的相关信息。

```c
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
```

对物理内存数组 `mm.memory_map` 进行初始化，将该数组放置在可分配物理内存起始地址处（即 `mm.alloc_base`）。

```c
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
```

将物理内存数组的初始化作为内存初始化的一部分。

```c
void memory_init() {
    ...
    // 初始化物理内存数组
    memory_map_init();
}
```

### 2.4 分配 / 释放页

在实现物理内存数组之后，我们可以借助物理内存数组中记录的页引用信息，来对物理页进行分配和释放。

```c
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
```

在分配 / 释放页时，我们使用 `assert` 对空闲页数进行了判断。在更新空闲页数 `mm.free_pages` 前后，都必须通过 `assert` 来保证操作的正确性。

## 3. 功能测试

### 3.1 物理内存数组

在 `kernel/main.c` 搭建测试框架：

```c
void kernel_init() {
    console_init();
    gdt_init();
    memory_init();
    interrupt_init();
    clock_init();

    hang();
    return;
}
```

预期为，输出

- 物理内存数组占据的页数
- 物理内存的总页数
- 去除物理内存数组占用页数之后剩余的空闲页数

>Qemu 模拟的物理内存共有 8160 页，所以物理内存数组占有 2 页物理内存。

### 3.2 分配 / 释放页

提供连续分配和释放页，来对该功能进行测试。

```c
/* kernel/memory.c */

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
```

在内核主函数 `kernel/main.c` 引入该测试。

```c
void kernel_init() {
    console_init();
    gdt_init();
    memory_init();
    interrupt_init();
    clock_init();

    memory_test();

    hang();
    return;
}
```
预期为，打印分配、释放页相关的地址信息。

## 4. 参考文献

- 赵炯 - 《Linux 内核完全注释》