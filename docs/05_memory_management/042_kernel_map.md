# 042 内核内存映射

## 1. 内核地址空间

### 1.1 修改内核页目录和页表

上一节我们提出了一个问题，如何修改内核的 **页目录** 和 **页表**？

- 除了上一节所说的直接修改（因为内核空间是恒等映射，虚拟地址 == 物理地址）之外，还有一种比较巧妙的方法。
- 将最后一个页表指向页目录自己，方便修改。
- 不过，这样会浪费掉最后 4M 的线性地址空间，只能用来管理页表；

```c
page_entry_t *entry = &pde[1023];
entry_init(entry, IDX(KERNEL_PAGE_DIR));
```

### 1.2 内核空间映射

将前 8M 的内存进行恒等映射，这部分空间供内核使用，即内核空间。

![](./images/memory_map_02.drawio.svg)

将内核页目录放置在物理地址 0x1000 处，第一个内核页表置于 0x2000，以此类推。页表映射完成之后的页面分布：

![](./images/memory_paging_02.drawio.svg)

### 1.3 内核空间分布

![](./images/kernel_map_01.drawio.svg)

使用分页机制的虚拟地址和物理地址的转换算法，来理解为何页目录和页表被映射到最后 4M 的地址空间。

## 2. 刷新快表

在更新页表之后，需要刷新快表 TLB 才能使得新的页表生效（TLB 是页表的缓存）。有两种方法可以刷新 TLB。

- `mov cr3, eax`：重新载入 cr3 寄存器，这样会刷新全部的页表。性能较低
- `invlpg`：可以只刷新指定虚拟地址所在的页表。性能较高。

```c++
// 刷新虚拟地址 vaddr 的快表 TLB
static void flush_tlb(u32 vaddr) {
    asm volatile("invlpg (%0)" ::"r"(vaddr)
                 : "memory");
}
```

这段代码是使用 AT&T 语法编写的内联汇编代码，用于x86处理器。具体来说，它使用 `invlpg` 指令来使处理器的 TLB 中的某一页无效化。该指令用于确保对指定的虚拟地址 `vaddr` 的任何未来内存访问都会经过完整的页表查找过程。

- `asm volatile` 语句用于告诉编译器这段汇编代码具有副作用，不应进行优化或重排序。
- `invlpg (%0)` 是汇编指令，其中 `invlpg` 是无效化TLB中一页的助记符，`(%0)` 是指定要无效化的内存位置的操作数。`%0` 是一个占位符，将被第一个输入约束的值替换。
- `::"r"(vaddr)` 是输入约束，指定 `vaddr` 变量应放置在一个通用寄存器中，用于汇编指令。`"r"` 表示寄存器约束。
- `"memory"` 是破坏列表，表示汇编代码可能以一种未指定的方式修改内存，告知编译器不要对内存访问进行优化或重排，这样可以使得 TLB 刷新之后的内存访问依据的是最新的内存映射。

## 3. 代码分析

### 3.1 Types

在 `include/xos/types.h` 中增加两个功能：支持内联函数，以及计算数组中元素个数。

```c
// gcc 用于定义内联函数
#define _inline __attribute__((always_inline)) inline

// 获取非空数组的元素个数
#define NELEM(a) (sizeof(a) / sizeof(a[0]))
```

并在 `kernel/memory.c` 中将分页机制启动功能 `enable_page()` 设置为内联函数（因为该函数体就是几行汇编）。

```c
// 将 cr0 的最高位 PG 置为 1，启用分页机制
static _inline void enable_page() {
    asm volatile(
        "movl %cr0, %eax\n"
        "orl $0x80000000, %eax\n"
        "movl %eax, %cr0\n"
    );
}
```

### 3.2 索引

在 `include/xos/memory.h` 中增加，由虚拟地址获取页目录 / 页表项索引的功能。

```c
// 获取 addr 的页目录索引
#define PDE_IDX(addr) (((u32)addr >> 22) & 0x3ff)

// 获取 addr 的页表索引
#define PTE_IDX(addr) (((u32)addr >> 12) & 0x3ff)

// 索引类型（页索引 / 页目录向索引 / 页表项索引）
typedef u32 idx_t;

// 一页中页表项的数量
#define PAGE_ENTRY_SIZE (PAGE_SIZE / sizeof(page_entry_t))
```

### 3.3 内存管理器

调整 `kernel/memory.c` 中的内存管理器 `mm`，使得其包含物理内存大小的信息。

```c
// 内存管理器
typedef struct memory_manager_t {
    ...
    u32 memory_size;        // 物理内存大小
} memory_manager_t;

static memory_manager_t mm;
```

修改 `memory_init()`，使得 `mm` 中保存由物理内存大小的信息。

```c
void memory_init() {
    ...
    mm.free_pages = PAGE_IDX(mm.alloc_size);
    mm.total_pages = mm.free_pages + PAGE_IDX(mm.alloc_base);
    mm.memory_size = mm.total_pages * PAGE_SIZE;
    ...
}
```

### 3.4 内核空间管理器

在 `kernel/memory.c` 中定义一个内核地址空间管理器 `kmm`，用于进行内核空间的地址映射，以及内核空间的管理。

```c
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
```

在物理内存初始化时，判断物理内存的大小是否达到内核空间的大小（本节中设定为 8M）。

```c
void memory_init() {
    ...
    // 判断物理内存是否足够
    if (mm.memory_size < kmm.kernel_space_size) {
        panic("Physical memory is %dM to small, at least %dM needed.\n",
                mm.memory_size / (1 * 1024 * 1024),
                kmm.kernel_space_size / (1 * 1024 * 1024)
        );
    }
    ...
}
```

### 3.5 内核空间映射

将物理内存的前 8M 进行恒等映射，作为内核的地址空间。

```c
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
}
```

### 3.6 访问内核页目录或页表

由于内核页目录和内核页表均被映射了两次（参考原理部分的映射图），所以可以通过两种方法来访问。

```c
// 获取内核页目录
static page_entry_t *get_pde() {
    // 方法 1：直接访问
    // return (page_entry_t *)kmm.kernel_page_dir;
    // 方法 2：通过最后的页目录项访问
    return (page_entry_t *)(0xfffff000);
}

// 获取 vaddr 所在的内核页表
static page_entry_t *get_pte(u32 vaddr) {
    // 方法 1：直接访问
    // return (page_entry_t *)kmm.kernel_page_table[PDE_IDX(vaddr)];
    // 方法 2：通过最后的页目录项访问
    return (page_entry_t *)(0xffc00000 | (PDE_IDX(vaddr) << 12));
}
```

### 3.7 刷新 TLB

这部分在原理部分已经进行了讲解，不再赘述。

```c
// 刷新 TLB
static void flush_tlb(u32 vaddr) {
    asm volatile("invlpg (%0)" ::"r"(vaddr)
                 : "memory");
}
```

## 4. 功能测试

在 `kernel/main.c` 搭建测试框架。

```c
void kernel_init() {
    console_init();
    gdt_init();
    memory_init();
    kernel_map_init();
    interrupt_init();

    memory_test();

    hang();
    return;
}
```

---

与上一节同理，Bochs 可以轻松查看映射的页表，以及查看物理地址、虚拟地址处的数据，所以本节继续使用 Bochs 来调试测试。

在 `kernel/memory.c` 中部署 `BMB`。

```c
void kernel_map_init() {
    ...
    // 启用分页机制
    enable_page();
    
    BMB;    /* BMB 1 */
}
```

在断点 `BMB 1` 处，查看内核页目录、内核页表的内容，查看 cr3 的值，查看地址映射内容。

---

接下来，对地址映射原理进行进一步的测试。

由于目前我们的机器只有 32M 内存，所以物理内存的地址必须在 32M 以内，才是有效的物理地址。而由于 32 位机器上，可以表示 $2^32 = 4G$ 大小的内存空间，所以虚拟内存地址在 4G 内即可。

```c
void memory_test() {
    BMB;    /* BMB 2 */

    // 将 20M，即 0x140_0000 地址映射到 64M 0x400_0000 处
    // 我们还需要一个物理页来存放额外的页表

    u32 vaddr = 0x4000000;  // 线性地址几乎可以是任意的（在 4G 内即可）
    u32 paddr = 0x1400000;  // 物理地址必须要确定存在（必须在 32M 内）
    u32 paddr2 = 0x1500000; // 物理地址必须要确定存在（必须在 32M 内）
    u32 table = 0x900000;   // 页表也必须是物理地址

    page_entry_t *kpage_dir = get_pde(); // 获取内核页目录
    page_entry_t *pde = &kpage_dir[PDE_IDX(vaddr)];
    page_entry_init(pde, PAGE_IDX(table)); // 映射 vaddr 对应的内核页目录项

    page_entry_t *kpage_table = get_pte(vaddr); // 获取 vaddr 对应的内核页表
    page_entry_t *pte = &kpage_table[PTE_IDX(vaddr)];
    page_entry_init(pte, PAGE_IDX(paddr)); // 映射 vaddr 对应的内核页表项

    BMB;    /* BMB 3 */

    char *ptr = (char *)vaddr;
    ptr[0] = 'a';

    BMB;    /* BMB 4 */

    // 重新映射 vaddr 对应的内核页表项
    page_entry_init(&pte[PTE_IDX(vaddr)], PAGE_IDX(paddr2));
    // 由于先前访问过 vaddr，需要刷新 TLB 中 vaddr 对应的缓存
    flush_tlb(vaddr);

    BMB;    /* BMB 5 */

    ptr[2] = 'b';

    BMB;    /* BMB 6 */
}
```

- 在断点 `BMB 3` 处，观察地址映射的内容。
- 在断点 `BMB 4` 处，观察 `vaddr` 和 `paddr` 地址对应的数据。
- 在断点 `BMB 5` 处，观察地址重新映射后的内容。
- 在断点 `BMB 6` 处，观察 `vaddr`、`paddr2` 以及 `paddr` 地址对应的数据。

通过这个测试，深刻理解地址映射的原理。

## 5. 参考文献

- 郑刚 - 《操作系统真象还原》，人民邮电出版社
- <https://wiki.osdev.org/TLB>
- <https://www.felixcloutier.com/x86/invlpg>