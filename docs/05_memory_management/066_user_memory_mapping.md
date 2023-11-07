# 066 用户内存映射

## 1. 原理说明

### 1.1 用户虚拟内存布局

**用户内存映射** 与 **内核内存映射** 不同，用户内存映射不是恒等映射，所以在用户内存映射当中，需要使用到 **物理管理内存机制** 来分配 / 释放可用的物理内存页，并通过 **分页机制** 来将虚拟内存页映射到物理内存页。

> **物理内存管理机制可用参考 [<040 物理内存管理>](./040_physical_memory.md) 这一小节。**

> **分页机制可以参考 [<041 内存映射机制（分页机制）>](./041_memory_paging.md) 这一小节。**

![](./images/memory_map_04.drawio.svg)

### 1.2 虚拟内存映射

这一小节需要实现两个功能：

- `link_page(vaddr)`：将 `vaddr` 对应的虚拟内存页，在当前用户进程的页表中，映射到某一可用的物理内存页（可用的物理内存页由内核分配，无需我们手动指定）。
- `unlink_page(vaddr)`：将 `vaddr` 对应的虚拟内存页，在当前用户进程的页表中，取消对应的物理内存页映射（内核会自动回收该物理内存页）。

> 代码位于 `include/xos/memory.h`

```c
// 将虚拟地址 vaddr 起始的页映射到物理内存
void link_page(u32 vaddr);

// 取消虚拟地址 vaddr 起始的页对应的物理内存映射
void unlink_page(u32 vaddr);
```

## 2. 代码分析

### 2.1 内存布局

根据原理说明，在 `include/xos/memory.h` 定义一些与内存空间布局相关的常量。

```c
#define KERNEL_MEMORY_SIZE  0x800000    // 内核占用的内存大小（内核恒等映射前 8M 内存）
#define USER_MEMORY_TOP     0x8800000   // 用户虚拟内存的最高地址 136M
```

至于为什么用户虚拟内存的最高地址为 $136M$, 这个我们在后面的 [<2.4 用户虚拟内存位图>](#24-用户虚拟内存位图) 当中进行说明。

### 2.2 页表项

为了方便后续的操作，定义一个宏，用于 **将页表项转换为对应的物理地址**。这样我们可以快捷地从页表项获取对应的物理地址，在遍历页表时非常高效。

```c
// 将 PTE 转换成 PA
#define PTE2PA(pte) PAGE_ADDR((pte).index)
```

### 2.3 获取页表

因为本节涉及的新增映射和取消映射操作，都涉及到修改页表，所以我们将 `get_pte()` 函数进行改造，使得其适用与新增映射和取消映射操作。

原理为，新增一个 `create` 参数，如果查询的虚拟地址对应的页表不存在（即此时并不存在所给虚拟地址相应的地址映射），我们根据 `create` 参数的值，来分配一页可用的物理内存页来作为页表，并在页目录当中建立与这个新页表的映射。

> **关于 `get_pde()` 和 `get_pte()` 这两个函数的原理，可以参考 [<042 内核内存映射>](./042_kernel_memory_mapping.md) 这一小节。**

> 代码位于 `kernel/memory.c`

```c
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
```

### 2.4 用户虚拟内存位图

在任务控制块 TCB 当中，成员 `vmap` 表示的是任务虚拟内存位图，根据任务的性质不同，它的意义也不同：

- 内核线程：`vmap` 表示内核虚拟内存缓存，用于管理内核当中可用的内存，这部分可以参考 [<044 内核虚拟内存管理>](./044_kernel_virtual_memory.md) 这一小节。

- 用户线程：`vmap` 表示用户虚拟内存位图，类似一个一级页表，但只有有效位（即每页对应的项只使用一个比特），通过页索引来判断对应的用户虚拟内存页是否存在映射。这样就无需遍历页表，来判断某一虚拟地址是否存在映射，而遍历页表非常低效。

```c
// 任务控制块 TCB
typedef struct task_t {
    ...
    bitmap_t *vmap;             // 任务虚拟内存位图
    u32 magic;                  // 内核魔数（用于检测栈溢出）
} task_t;
```

在本项目当中，我们分配一页内存来作为用户虚拟内存位图，因为一页拥有 $4096 Bytes = 2^{15} bits$，所以这个位图可以表示 $2^{15} * 2^{12} = 128 MB$ 的内存空间。又因为内核占据前 $8 MB$ 内存，所以用户虚拟内存的最高地址为 $128 + 8 = 136 M$。

在切换至用户态函数 `real_task_to_user_mode()` 当中补充设置用户虚拟内存位图逻辑：

```c
static void real_task_to_user_mode(target_t target) {
    task_t *current = current_task();

    // 设置用户性能内存位图
    current->vmap = (bitmap_t *)kmalloc(sizeof(bitmap_t)); // TODO: kfree()
    u8 *buf = (u8 *)kalloc_pages(1); // TODO: kfree_pages()
    bitmap_init(current->vmap, buf, PAGE_SIZE, KERNEL_MEMORY_SIZE / PAGE_SIZE);
    ...
}
```

- 用户虚拟内存位图的有效起始索引为 $2^{23} / 2^{12} = 2^{11} = 2048$，这是因为前 $8M$ 虚拟内存是内核的恒等映射，用户无需判断这些虚拟页的有效性（这部分是由内核负责的）。

> 注：因为使用了内存分配机制，例如 `kmalloc()` 和 `kalloc_pages()`，所以在后续实现结束任务时需要释放这些内存。

### 2.5 建立映射

`link_page()` 的主要功能为，在虚拟地址和物理地址之间建立新的映射。

1. 虚拟地址必须为页的起始地址（因为分页机制是以页为单位进行映射）。
1. 获取虚拟地址对应的页表项，如果该页表项表示该页已被映射，返回即可。
2. 否则需要分配一可用物理内存页，修改页表项以实现在页表中建立映射，同时需要在用户虚拟内存位图中标记存在。

```c
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
```

### 2.6 取消映射

`unlink_page()` 的主要功能为，在虚拟地址和物理地址之间取消原有的映射。

1. 虚拟地址必须为页的起始地址（因为分页机制是以页为单位进行映射）。
1. 获取虚拟地址对应的页表项，如果该页表项表示该页未被映射，返回即可。
2. 否则需要修改页表项以实现在页表中取消映射，同时需要在用户虚拟内存位图中删除存在标记，最后需要释放对应的物理内存页。

```c
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
```

> 注意
> ---
> 这里只是将对应的物理内存页取消了映射，并没有取消页表所在页的映射，这是因为运行的**局部性原理**！以及页目录和全部页表占用的内存空间并不大。

## 3. 功能测试

### 3.1 测试框架

因为 `link_page()` 和 `unlink_page()` 是内核态的功能函数，所以如果我们需要在用户线程 `user_init_thread` 对它们进行测试的话，需要通过系统调用。所以我们将系统调用 `test()` 的内核态处理逻辑改造为测试这两个函数：

修改 `user_init_thread` 的逻辑为触发系统调用 `test()`：

```c
static void user_init_thread() {
    size_t counter = 0;

    while (true) {
        test();
        sleep(1000);
    }
}
```

并修改其它线程的逻辑为睡眠，防止干扰测试。


### 3.2 缺页异常

```c
// 系统调用 test 的处理函数
static u32 sys_test() {
    u32 vaddr = 0x1600000;
    char *ptr;

    ptr = (char *)vaddr;
    ptr[3] = 'T';

    return 255;
}
```

因为未映射 `0x1600000` 这个虚拟地址，所以会触发 `Page Fault` 缺页异常。

### 3.3 用户虚拟内存映射

```c
// 系统调用 test 的处理函数
static u32 sys_test() {
    u32 vaddr = 0x1600000;
    char *ptr;

    link_page(vaddr);
    BMB;                // BMB 1

    ptr = (char *)vaddr;
    ptr[3] = 'T';
    BMB;                // BMB 2

    unlink_page(vaddr);
    BMB;                // BMB 3

    return 255;
}
```

使用 Bochs 进行测试：

- 在 BMB 1 处，查看页表是否建立了新的映射（两个新映射：所在页表和虚拟页的映射）
- 在 BMB 2 处，不会触发缺页异常，并查看虚拟地址 `0x1600000` 处的数据是否写入 `T`
- 在 BMB 3 处，查看页表是否取消了相应的映射（只取消了虚拟页的映射，并没有取消所在页表的映射）
