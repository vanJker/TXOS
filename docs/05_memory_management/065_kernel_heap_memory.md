# 065 内核堆内存管理

在 [<044 内核堆内存管理>](./044_kernel_virtual_memory.md) 一节中，我们已经实现了以页为单位的，分配和释放内存的功能。但是以页为单位进行内存分配和释放，会导致内存的利用率低（试想一下，你只是想要1 Byte 内存来存储一个字符，却给你分配了一页内存 4096 Bytes）。

> `include/xos/memory.h`

```c
// 分配 count 个连续的内核页
u32 kalloc_page(u32 count);

// 释放 count 个连续的内核页
void kfree_page(u32 vaddr, u32 count);
```

所以本节我们在之前以页为单位的分配 / 释放机制的基础上，来实现更小粒度的内存分配 / 释放机制。

> `include/xos/arena.h`

```c
// 分配一块大小至少为 size 的内存块
void *kmalloc(size_t size);

// 释放指针 ptr 所指向的内存块
void kfree(void *ptr);
```

## 1. 原理说明

下面是内存分配模型的图示：

![](./images/arena_descriptors.drawio.svg)

我们将可分配 / 释放的内存块分成两种类型：**普通块** 和 **超大块**。

**普通块** 的最小粒度为 `16 Bytes`，最大粒度为 `1024 Bytes`，中间还有 `32 Bytes`、`64 Bytes`、`128 Bytes`、`256 Bytes`、`512 Bytes` 这些普通块粒度。普通块一共有 7 种粒度。这些粒度都可以通过分配 **一页内存**，来获取多个普通块（即对于任意粒度，一页内存都可以包括数个普通块）。

**超大块** 则是所需内存的大小，超过了普通块的最大粒度（即 `1024 Bytes`），这时我们使用 **按页分配算法** 来分配超大块。一个超大块所需的内存空间，**至少需要分配一个页**（即一个超大块可能会包块数个页）。

空闲的普通块之间使用链表进行连接，链表头尾都置于对于粒度的 **普通内存块描述符** 结构体中。虽然内部实现是使用链表，但对于空闲的普通块管理，使用的是 **队列机制**，即先进后出（仅使用链表的 `push_back` 和 `pop_front` 方法）。

超大块则不需要进行空闲块管理，因为超大块的最小粒度为一页，直接使用页释放算法进行回收即可。当然我们可以反推得出，普通块需要进行空闲块管理，是因为我们需要记录每一页上面的普通块释放都处于空闲状态，如果都处于空闲状态，则需要回收该页内存。

> 代码位于 `include/xos/arena.h`

### 1.1 内存块描述符

内存块描述符，是用于描述不同粒度的 **普通内存块** 对应的信息，比如一页内存可以分成多少对于粒度的普通块，普通块的粒度，以及该粒度的空闲块队列。

```c
// 内存块描述符
typedef struct arena_descriptor_t {
    size_t total_block; // 一页内存可以分成多少块
    size_t block_size;  // 块大小 / 粒度
    list_t free_list;   // 该粒度的空闲块队列
} arena_descriptor_t;
```

### 1.2 arena

![](./images/arena_and_block.drawio.svg)

`arena` 是对分配的一页（普通块）或多页（超大块）内存进行说明的结构体，**它位于页的起始处（如果是超大块，则位于分配数页的首页的起始处）**。使用 `kmalloc` 进行分配的内存空间，都会有一个对应的 `arena`，用于说明内存结构，并用于后续的内存释放 `kfree`。

```c
// 一页或多页内存的结构说明
typedef struct arena_t {
    arena_descriptor_t *desc;   // 该 arena 粒度的内存块描述符
    size_t count;               // 该 arena 当前剩余的块数（large = 0）或 页数（large = 1）
    bool   large;               // 表示超大块，即是不是超过了 1024 字节
    u32    magic;               // 魔数，用于检测该结构体是否被篡改
} arena_t;
```

1.3 空闲内存块

因为对应空闲的普通内存块，我们需要通过链表来将它们连接起来管理，所以将空闲块的起始部分，作为链表节点，连接其它的空闲块。

> **注：在将空闲块分配出去后，这个链表节点信息会被抹除（实际上对整个块都进行了抹除），防止数据泄露。**

```c
typedef list_node_t block_t; // 内存块
```

## 2. 代码分析

### 2.1 普通内存块描述符

根据原理说明，定义一个内存描述符块数组，其不同的元素对应不同的粒度。

```c
// 普通内存块描述符的数量（共 7 种粒度：16B、32B、64B、128B、256B、512B、1024B）
#define ARENA_DESC_COUNT 7
// 普通内存块描述符数组
static arena_descriptor_t arena_descriptors[ARENA_DESC_COUNT];

// 普通内存块的最小粒度
#define MIN_BLOCK_SIZE 16
// 普通内存块的最大粒度
#define MAX_BLOCK_SIZE arena_descriptors[ARENA_DESC_COUNT - 1].block_size
```

接下来设置这个内充描述符数组，同时这也是内存堆内存管理的初始化。主要逻辑是根据各个普通块粒度来设置各个内存块描述符。

```c
// arena 初始化内核堆管理
void arena_init() {
    size_t block_size = MIN_BLOCK_SIZE;

    for (size_t i = 0; i < ARENA_DESC_COUNT; i++) {
        arena_descriptor_t *desc = &arena_descriptors[i];
        desc->block_size = block_size;
        desc->total_block = (PAGE_SIZE - sizeof(arena_t)) / block_size;
        list_init(&desc->free_list);

        block_size <<= 1; // 块粒度每次 x2 增长
    }
}
```

需要注意的是，计算一页内存可以包括的对应粒度普通块数，需要先减去 `arena` 结构体的大小，因为在页的起始处是 `arena` 结构体，这部分是不能用于分配内存块的。

### 2.2 辅助函数

![](./images/arena_and_block.drawio.svg)

从所给 `arena` 地址，来获取的第 `idx` 块 **普通内存块** 的起始地址。这个函数只能用于普通块。

```c
// 获取所给 arena 的第 idx 块内存的地址
static void *get_block_from_arena(arena_t *arena, size_t idx) {
    // 保证 idx 是合法的，没有超出 arena 的范围
    assert(idx < arena->desc->total_block);

    u32 addr = (u32)arena + sizeof(arena_t);
    u32 gap = idx * arena->desc->block_size;
    
    return (void *)(addr + gap);
}
```

从所给 `block` 地址，获取对应的 `arena` 的地址。主要逻辑是取 `block` 所在页的起始地址。这个函数可用于普通块和超大块，这是因为在超大块的起始地址和 `arena` 是处于同一页的，而且 `arena` 位于页的起始处。

```c
// 获取所给 block 对应的 arena 的地址
static arena_t *get_arena_from_block(block_t *block) {
    return (arena_t *)((u32)block & 0xfffff000);
}
```

2.3 KMALLOC

`kmalloc()` 对于不同大小的内存需求，分别对应分配 **普通块** 和 **超大块** 这两个逻辑。

分配超大块的逻辑比较简单，计算所需页数（注意需要加上 `arena` 的大小）进行分配，并设置对应的 `arena` 即可。

分配普通块相对来说比较复杂，需要先寻找到合适的普通块粒度，然后在该粒度对应的空闲块队列中获取一个空闲块。如果对应的空闲块队列为空，则需要分配一页内存作为切割为空闲块，填充进对应的空闲块队列。

> **注：分配的内存块都需要进行数据清除操作，防止原有的数据被恶意利用。**

```c
// 分配一块大小至少为 size 的内存块
void *kmalloc(size_t size) {
    void *addr;
    arena_t *arena;
    block_t *block;

    // 如果分配内存大小大于内存块描述符的最大粒度
    if (size > MAX_BLOCK_SIZE) {
        // 计算需要内存的总大小，以及对应的页数
        size_t asize = size + sizeof(arena_t);
        size_t count = div_round_up(asize, PAGE_SIZE);

        // 分配所需内存，以及清除原有的数据
        arena = (arena_t *)kalloc_page(count);
        memset(arena, 0, count * PAGE_SIZE);
        
        // 设置 arena 内存结构说明
        arena->large = true;
        arena->count = count;
        arena->desc  = NULL;
        arena->magic = XOS_MAGIC;

        addr = (void *)((u32)arena + sizeof(arena_t));
        return addr;
    }

    // 如果分配内存大小并没有超过内存块描述符的最大粒度
    arena_descriptor_t *desc = NULL;
    for (size_t i = 0; i < ARENA_DESC_COUNT; i++) {
        desc = &arena_descriptors[i];

        // 寻找恰好大于等于分配内存大小的内存块描述符
        if (desc->block_size >= size) {
            break;
        }
    }

    assert(desc != NULL); // 必会寻找到一个合适的内存块描述符

    // 如果该内存块描述符对应的空闲块链队列为空
    if (list_empty(&desc->free_list)) {
        // 分配一页内存，以及清除原有的数据
        arena = (arena_t *)kalloc_page(1);
        memset(arena, 0, PAGE_SIZE);

        // 设置 arena 内存结构说明
        arena->large = false;
        arena->desc = desc;
        arena->count = desc->total_block;
        arena->magic = XOS_MAGIC;

        // 将新分配页中的块加入对应的空闲块队列
        for (size_t i = 0; i < desc->total_block; i++) {
            block = get_block_from_arena(arena, i);
            assert(!list_contains(&desc->free_list, block));
            list_push_back(&desc->free_list, block);
            assert(list_contains(&desc->free_list, block));
        }
    }

    // 在对应的空闲块队列中获取一个空闲块
    block = list_pop_front(&desc->free_list);
    // 清除原有的数据，因为在回收块时并没有清除
    memset(block, 0, desc->block_size);

    // 更新 arena 对应的记录
    arena = get_arena_from_block(block);
    assert(arena->magic == XOS_MAGIC && !arena->large);
    arena->count--;

    return (void *)block;
}
```

### 2.4 KFREE

与 `kmalloc()` 类似，`kfree()` 对于不同大小的内存块，分别对应释放 **普通块** 和 **超大块** 这两个逻辑。

释放超大块的逻辑比较简单，直接释放原先分配的页数（可从对应的 `arena` 获取）即可。

释放普通块相对来说也比较复杂，一般情况下将内存块重新加入空闲块队列即可。但是如果释放该内存块后，该块对应的那一页内存均为空闲块的话，则需要释放该页内存。

```c
// 释放指针 ptr 所指向的内存块
void kfree(void *ptr) {
    // 释放空地址 / 指针是非法操作
    assert(ptr != NULL);

    // 获取对应的 arena
    block_t *block = (block_t *)ptr;
    arena_t *arena = get_arena_from_block(block);
    assert(arena->magic == XOS_MAGIC);

    // 如果是超大块（即 large = 1）
    if (arena->large) {
        kfree_page((u32)arena, arena->count);
        return;
    }

    // 如果不是超大块（即 large = 0）
    list_push_back(&arena->desc->free_list, block); // 重新加入空闲队列
    arena->count++;                                 // 更新空闲块数

    // 如果该页内存的全部块都已经被回收，则释放该页内存
    if (arena->count == arena->desc->total_block) {
        // 将该页的空闲块从对应的空闲队列当中去除
        for (size_t i = 0; i < arena->count; i++) {
            block = get_block_from_arena(arena, i);
            assert(list_contains(&arena->desc->free_list, block));
            list_remove(block);
            assert(!list_contains(&arena->desc->free_list, block));
        }

        kfree_page((u32)arena, 1);
    }
}
```

### 2.5 内核初始化

在 `kernel_init()` 当中加入 `arena_init()` 逻辑，以初始化内核堆内存管理。

> 代码位于 `kernel/main.c`

```c
void kernel_init() {
    console_init();
    gdt_init();
    tss_init();
    memory_init();
    kernel_map_init();
    arena_init();
    interrupt_init();
    clock_init();
    keyboard_init();
    task_init();
    syscall_init();

    irq_enable(); // 打开外中断响应

    hang();
    return;
}
```

## 3. 功能测试

将 `user_init_thread` 用户线程的功能改为睡眠，因为我们需要使用 `test_thread` 内核线程来测试。

```c
static void user_init_thread() {
    size_t counter = 0;

    while (true) {
        sleep(100);
    }
}
```

在内核线程 `test_thread` 当中对 `kmalloc()` 和 `kfree()` 进行测试。

```c
void test_thread() {
    irq_enable();
    u32 counter = 0;

    void *ptr1 = kmalloc(1280);
    LOGK("kmalloc 0x%p...\n", ptr1);

    void *ptr2 = kmalloc(1024);
    LOGK("kmalloc 0x%p...\n", ptr2);

    void *ptr3 = kmalloc(54);
    LOGK("kmalloc 0x%p...\n", ptr3);

    kfree(ptr2);
    kfree(ptr1);
    kfree(ptr3);

    while (true) {
    }
}
```

预期类似于如下格式：

```bash
# ptr1 = kmalloc(1280)
Scan page 0x00106000 count 1
Alloc kernel page 0x00106000 count 1
kmalloc 0x00106010...
# ptr2 = kmalloc(1024)
Scan page 0x00107000 count 1
Alloc kernel page 0x00107000 count 1
kmalloc 0x00107010...
# pt3 = kmalloc(54)
Scan page 0x00108000 count 1
Alloc kernel page 0x00108000 count 1
kmalloc 0x00108010...
# kfree(ptr2)
FREE kernel pages 0x00107000 count 1
# kfree(ptr1)
FREE kernel pages 0x00106000 count 1
# kfree(ptr3)
FREE kernel pages 0x00108000 count 1
```

> 可以调换各个 `kmalloc` 以及 `kfree` 的调用顺序，来进行观察。同时可以进一步理解空闲块的队列机制。
 
## 4. FAQ

> **为什么直接使用链表来管理一页内存里面的空闲块？这样在释放内存块对应的页时消耗极大。**
> ***
> 这是为了简单起见，当然可以使用位图来管理一页内存当中的普通块，这样会提高性能。这个我们后续会进行改进。

## 5. 参考文献

- [郑刚 / 操作系统真象还原 / 人民邮电出版社 / 2016](https://book.douban.com/subject/26745156/)
