# 044 内核虚拟内存管理

## 1. 内存布局

将内核地址空间布置成如下布局：

![](./images/memory_map_03.drawio.svg)

即还需要使用一个位图，来标记内核地址空间中的 **可分配部分**，以实现内核页的分配。

## 2. 核心代码

在 `include/xos/memory.h` 声明以下函数原型：

```c
// 分配 count 个连续的内核页
u32 kalloc(u32 count);

// 释放 count 个连续的内核页
void kfree(u32 vaddr, u32 count);
```

## 3. 代码分析

### 3.1 内核地址空间管理器

在 `kernel/memory.c` 给内核地址空间管理器 `kmm` 增加新成员，记录内核虚拟内存空间的使用情况。

```c
// 内核地址空间管理器
typedef struct kmm_t {
    ...
    bitmap_t kernel_vmap;   // 内核虚拟内存空间位图
} kmm_t;
```

### 3.2 初始化内核虚拟内存空间位图

由于内核映射是 **恒等映射**，所以我们可以使用物理内存管理器 `mm` 中记录的数据，同时内核虚拟内存空间位图的初始化，在内核映射前后都可以进行，我们选择在内核映射后进行位图初始化。

依据原理说明，内核虚拟内存空间位图，存放于 **物理地址** 的 0x4000 处（由于是恒等映射，所以物理地址等于内核虚拟地址）。

同时借助位图的特性，将可分配内存起始页 `mm.start_page_idx` 之前的内核部分，均设置为不可分配和释放（位图中 `offset` 的作用），保证了安全性，十分精妙。

```c
// 初始化内核虚拟内存空间位图
static void kernel_vmap_init() {
    u8 *bits = (u8 *)0x4000;
    size_t size = div_round_up((PAGE_IDX(kmm.kernel_space_size) - mm.start_page_idx), 8);
    bitmap_init(&kmm.kernel_vmap, bits, size, mm.start_page_idx);
}

// 初始化内核地址空间映射（恒等映射）
void kernel_map_init() {
    ...
    // 初始化内核虚拟内存空间位图
    kernel_vmap_init();
}
```

### 3.3 分配 / 释放内核页

分配连续 count 个内核页。在内核虚拟内存位图中扫描，查看是否有连续 count 个空闲的内核页，并对特殊情况进行处理。

```c
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

// 分配 count 个连续的内核页
u32 kalloc(u32 count) {
    assert(count > 0);
    u32 vaddr = scan_pages(&kmm.kernel_vmap, count);
    LOGK("ALLOC kernel pages 0x%p count %d\n", vaddr, count);
    return vaddr;
}
```

释放从指定地址开始的连续 count 个内核页。在内核虚拟内存位图中，设定连续 count 个空闲的内核页为空闲，并对特殊情况进行处理。

```c
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

// 释放 count 个连续的内核页
void kfree(u32 vaddr, u32 count) {
    ASSERT_PAGE_ADDR(vaddr);
    assert(count > 0);
    reset_pages(&kmm.kernel_vmap, vaddr, count);
    LOGK("FREE kernel pages 0x%p count %d\n", vaddr, count);
}
```

> **`scan_pages()` 和 `reset_pages()` 后续也可以用于用户虚拟内存空间的分配和释放（这也是为什么将它们与 `kalloc()` 和 `kfree()` 分离的原因）。**

## 4. 功能测试

搭建测试框架：

```c
/* kernel/main,c */
void kernel_init() {
    ...
    memory_test();
    ...
}

/* kernel/memory.c */
void memory_test() {
    u32 *kpages = (u32 *)(0x200000);
    u32 count = PAGE_IDX(kmm.kernel_space_size) - mm.start_page_idx;

    // 分配内核页
    for (size_t i = 0; i < count; i++) {
        kpages[i] = kalloc(1);
    }

    // 释放内核页
    for (size_t i = 0; i < count; i++) {
        kfree(kpages[i], 1);
    }
}
```

使用调试进行测试，预期为：

- `count` 值为 0x6fe
- 在分配内核页后，内核虚拟内存位图对应的位为 1
- 在释放内核页后，内核虚拟内存位图对应的位为 0
- 第一个分配 / 释放的内核页，地址均为 `0x102000`
- 最后一个分配 / 释放的内核页，地址均为 `0x7ff000`
