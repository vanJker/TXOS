# 041 内存映射原理（分页机制）

## 1. 内存管理机制

### 1.1 分段与分页

- 分段：
    - 内存描述符
    - 平坦模型：只分一个段，也就是不分段
- 分页：一页大小为 4KB

### 1.2 内存映射机制图

![](./images/memory_paging_01.drawio.svg)

### 1.3 地址

- 逻辑地址：程序访问的地址
- 线性地址：程序访问的地址 + 描述符中的基地址
- 物理地址：实际内存的位置
- 虚拟地址：虚拟内存的地址

平坦模型中，逻辑地址和线性地址是相同的。

内存映射是将线性地址转换成物理地址的过程。

## 2. 内存分页

- 进程内存空间：程序员在编写程序的时候不知道具体运行的机器，也不知道程序将会被放置到何处的地址
- 虚拟内存：可以使用硬盘来模拟内存，即 Linux 系统中的交换区

由于一页内存是 4KB，而 32 位总共可以访问 4G 的内存，所以内存总共分成了 4G / 4K = 1M 个页；

$$
1M = 1024 \times 1024 = 2^{10} \times 2^{10} = 2^{20}
$$

现在要做线性地址和物理地址的一一映射关系，这个映射关系存储在内存中，由于系统中没有 20bit 的位宽（1M 需要使用至少 20 位来表示），所以用 32bit 的位宽来存，那么就需要一个如下的数组来存储：

```c
u32 page_table[1 << 20];
```

其中 `1 << 20` 就是 1M， 由于总共有 1M 个页，一页占用了 4B，所以这个数组尺寸是 4M，这个数组也就是页表。

这个数组也存储在内存中，4M = 1024 * 4K，也就是需要 1024 个页来存储。

由于每个进程都需要一个页表来映射内存，如果直接用这种方式的话，每个进程都需要至少 4M 的内存来存储页表，但是，并不是所有的进程都用到所有 4G 的空间，所以这种方式很不划算，而且 386 的年代，内存比较小，显然不能这样干。

所以就有了页目录，用来表示这 1024 个页中用到了哪些页。其中一页页表用 4B 表示，恰好是 4KB，页表 **恰好** 占用一页的内存。

如果进程只用到了很少的内存的话，就可以只用两个页来表示，这样可以表示 4M 的内存（一个页目录项表示 4M 内存：1K * 4KB = 4MB），一页页目录，一页页表，总共用到了 8K，比上面的 4M 节约了不少。当然，如果进程确实用到了全部 4G 的空间，那么就会比 4M 再多一页页目录，不过一般进程不会用到所有的内存，而且操作系统也不允许。

上面的恰好，实际上是有意设计的，之所以恰好，就是因为页的大小是 4KB，这也解释了为什么分页的大小是4KB，如果分成其他的大小，可能页表和页目录就不那么恰好了，当然 4M 的页也是恰好的，只需要一个页表就能表示全部的内存。

但是，分页，页表，页目录的这种策略也并不完全恰好，表示一页 20bit 就够了，但是却用了 32 bit，也就是说 12 bit 可以用来干别的事情。

事实上确实用来干别的事情了，用来表示这页内存的属性，这些属性如下：

**页表**

![](./images/memory_pte.drawio.svg)

**页目录**

![](./images/memory_pde.drawio.svg)

由于页表项 `PTE` 和页目录项 `PDE` 结构类似（页目录项只是比页表项少表示了几个位，将那些位在页目录项中设置为保留位即可），所以定义一个 `PTE` 结构体来表示页表项和页目录项。 

```c
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
```

这个结构体是有意构造的，恰好占 4 个字节，一页内存（4KB）可以表示下面这样一个数组；

```c
page_entry_t page_table[1024];
```

## 3. 内存映射

>CR - Control Register

### 3.1 CR3 寄存器 

![](./images/memory_cr3.drawio.svg)

### 3.2 CR0 寄存器

![](./images/memory_cr0.drawio.svg)

### 3.3 开启分页机制

1. 首先准备一个页目录，若干页表
2. 将映射的地址写入页表，将页表写入页目录
3. 将页目录写入 cr3 寄存器
4. 将 cr0 最高位（PG）置为 1，启用分页机制

>cr0 最低位（PE）使能会开启保护模式，可以参考 [<009 保护模式和全局描述符>](../01_bootloader/009_protected_mode.md)。

## 4. 恒等映射

一个实际的操作就是 **映射完成后，低端 1M 的内存要映射的原来的位置**，因为内核放在那里，映射完成之后就可以继续执行了。即映射后的地址与未映射时的物理地址相等，这个又被称为 **恒等映射**。

在启用分页机制时涉及的功能原型如下：

```c
// 得到 cr3 寄存器的值
u32 get_cr3();

// 设置 cr3 寄存器，参数是页目录的地址
void set_cr3(u32 pde);

// 将 cr0 寄存器最高位 PG 置为 1，启用分页
static void enable_page();
```

>思考：在分页启用时，如何修改 **页目录** 和 **页表**？
>
>可以直接修改页目录和页表，因为是恒等映射，可以直接通过地址去修改。除此之外，下一节我们会介绍另一种修改方法。

## 5. 关键词

- 快表：Translation Lookaside Buffers (TLB)
- 页目录 (Page Directory)
- 页表 (Page Table)
- 页框 (Page Frame)：就是被映射的物理页

## 6. 代码分析

### 6.1 相关声明

按照原理说明，在 `include/xos/memory.h` 构造页表 / 页目录项的结构体，以及声明对 cr3 寄存器相关的操作。

```c
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

// 获取 cr3 寄存器的值
u32 get_cr3();

// 设置 cr3 寄存器，参数是页目录的地址
void set_cr3(u32 pde);
```

### 6.2 内核页表

在内存管理器 `mm`（位于 `kernel/memory.c`） 中记录内核页表相关的物理地址信息。

```c
// 内存管理器
typedef struct memory_manager_t {
    ...
    u32 kernel_page_dir;    // 内核页目录所在物理地址
    u32 kernel_page_table;  // 内核第一个页表所在的物理地址
} memory_manager_t;

static memory_manager_t mm;
```

### 6.3 cr3 寄存器

内联汇编的写法是 AT&T 风格语法，与之前的 Nasm 风格语法不同，注意其语义。

```c
// 获取 cr3 寄存器的值
u32 get_cr3() {
    // 根据函数调用约定，将 cr3 的值复制到 eax 作为返回值
    asm volatile("movl %cr3, %eax");
}

// 设置 cr3 寄存器，参数是页目录的地址
void set_cr3(u32 pde) {
    ASSERT_PAGE_ADDR(pde);
    // 先将 pde 复制到 eax，再将 eax 的值复制到 cr3
    asm volatile("movl %%eax, %%cr3"::"a"(pde));
}
```

### 6.4 使能分页

根据原理说明，设置 cr0 寄存器的最高位为 1，可以使能分页机制。由于 cr 寄存器的限制，无法直接对 cr0 寄存器进行位运算，需要将 cr0 的值复制到 eax（或者其它通用寄存器），进行运算后，再复制回 cr0，以此使能分页。

```c
// 将 cr0 的最高位 PG 置为 1，启用分页机制
static void enable_page() {
    asm volatile(
        "movl %cr0, %eax\n"
        "orl $0x80000000, %eax\n"
        "movl %eax, %cr0\n"
    );
}
```

### 6.5 初始化页表 / 页目录项

暂时实现一个简易版的页表项设置功能，直接将页表项的属性设置为 `U|W|P`，即所有人均可访问，可读可写，该页位于内存。

```c
// 初始化页表项，设置为指定的页索引 | U | W | P
static void page_entry_init(page_entry_t *entry, u32 index) {
    *(u32 *)entry = 0;
    entry->present = 1;
    entry->write = 1;
    entry->user = 1;
    entry->index = index;
}
```

### 6.6 内核地址空间映射

将物理内存的前 4M 内存空间，进行恒等映射。

```c
// 初始化内核地址空间映射（恒等映射）
void kernel_map() {
    // 设置内核页目录和页表所在物理地址
    mm.kernel_page_dir = 0x200000;
    mm.kernel_page_table = mm.kernel_page_dir + PAGE_SIZE;

    page_entry_t *pde = (page_entry_t *)mm.kernel_page_dir;
    memset(pde, 0, PAGE_SIZE); // 清空内核页目录

    // 将页目录的第一项设置为内核页表
    page_entry_init(&pde[0], PAGE_IDX(mm.kernel_page_table));

    page_entry_t *pte = (page_entry_t *)mm.kernel_page_table;
    memset(pte, 0, PAGE_SIZE); // 清空内核页表

    // 恒等映射前 1024 个页，即前 4MB 内存空间
    for (size_t i = 0; i < 1024; i++) {
        page_entry_init(&pte[i], i);
        mm.memory_map[i] = 1; // 设置物理内存数组，该页被占用
    }

    // 设置 cr3 寄存器
    set_cr3((u32)pde);

    // 启用分页机制
    enable_page();
}
```

我们发现了一个有趣的点，我们现在已经将前 4M 物理内存恒等映射到了内核地址空间，而我们设置的内核页目录及页表位于 2M 物理地址起始的连续 2 页，所以此时我们可以直接通过地址对内核页目录和页表进行修改（因为它们也被恒等映射到了与物理地址相等的虚拟地址处）。

## 7. 功能测试

使用 Bochs 可以方便的查看页表信息，所以本节使用 Bochs 进行测试。

### 7.1 Bochs Unlock

首先，在 `Makefile` 文件对 Bochs 增加 `-unlock` 参数，这个参数可以避免 Bochs 在异常退出时，Bochs 会锁住资源的问题。

```makefile
bochs-run: $(IMG)
	bochs -q -f ./bochs/bochsrc -unlock

bochs-debug: $(IMG)
	bochs-gdb -q -f ./bochs/bochsrc-gdb -unlock
```

### 7.2 BMB

BMB：Bochs Magic Breakpoint，是 Bochs 的魔术断点指令。由于现在内核文件日益增多，我们需要对 `BMB` 进行强化，使得其在断点的同时，打印出当前文件和行数，方便调试（借助 `DEBUGK` 宏可以轻松实现）。

```c
// Bochs Magic Breakpoint
#define BMB DEBUGK("BMB\n"); \
            asm volatile("xchgw %bx, %bx"); \
```

### 7.3 分页启用

在内核映射 `kernel_map()` 中，设置相应断点，并在 Bochs 中观察分页机制相关的信息。

```c
/* kernel/memory.c */
void kernel_map() {
    BMB;
    // 设置 cr3 寄存器
    set_cr3((u32)pde);
    BMB;
    // 启用分页机制
    enable_page();
    BMB;
}

/* kernel/main.c */
void kernel_init() {
    console_init();
    gdt_init();
    memory_init();
    kernel_map();
    interrupt_init();

    hang();
    return;
}
```

在 Bochs 中观察 cr3 寄存器的值，cr0 寄存器的值，page table 中的映射信息。并观察内核页目录和页表所在的物理地址处的数据。

### 7.4 分页保护

我们只恒等映射了前 4M 内存，所以在分页机制下，当我们试图去访问超过这个范围的地址时，会触发缺页异常。

```c
/* kernel/memory.c */
void memory_test() {
    BMB;
    // 访问未被映射的 20M 地址处
    char *ptr = (char *)(0x100000 * 20);
    ptr[0] = 'a';
}

/* kernel/main.c */
void kernel_init() {
    console_init();
    gdt_init();
    memory_init();
    kernel_map();
    interrupt_init();

    memory_test();

    hang();
    return;
}
```

预期为，打印缺页异常的信息，同时可以观察到，触发缺页异常的地址（即访问该地址造成了缺页异常）会被记录在 cr2 寄存器中。

>异常处理完后，会陷入阻塞，此时需要使用 Bochs 的 break 选项跳出阻塞，方可观察 cr2 寄存器的值。

## 8. 参考文献

- Intel® 64 and IA-32 Architectures Software Developer's Manual Volume 3 Chapter 4 Paging
- <https://en.wikipedia.org/wiki/I386>