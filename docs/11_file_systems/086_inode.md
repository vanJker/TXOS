# 086 文件系统 inode

这一节主要实现对文件系统最重要的 inode 进行操作，思想还是和超级块处理类似，构建内核内存版本的 inode 类型，并建立 inode 池，以提高存取效率。

```c
//--> include/xos/fs.h

/*** fsmap.c ***/
// 获取 inode 索引的第 nr 个块对应的块号，如果该块不存在且 create 为 true，则创建
size_t bmap(inode_t *inode, size_t nr, bool create);

/*** inode.c ***/
// 获取根目录对应的 inode (约定 inode 池的第一个 inode 用于存放根目录对应的 inode)
inode_t *get_root_inode();
// 获取设备的第 nr 个 inode
inode_t *iget(devid_t dev_id, size_t nr);
// 释放 inode 会 inode 池
void iput(inode_t *inode);
```

## 1. 代码分析

### 1.1 inode

### 1.1.1 inode 池

```c
//--> fs/inode.c

// 系统最多同时持有 64 个 inode
#define INODE_NR 64
// inode 池
static inode_t inode_table[INODE_NR];

// 获取根目录对应的 inode (约定 inode 池的第一个 inode 用于存放根目录对应的 inode)
inode_t *get_root_inode() {
    return &inode_table[0];
}

// 从 inode 池获取一个空闲的 inode
static inode_t *get_free_inode() {
    for (size_t i = 0; i < INODE_NR; i++) {
        inode_t *inode = &inode_table[i];
        if (inode->dev_id == EOF) {
            return inode;
        }
    }
    panic("no more inode!!!");
}

// 释放一个 inode 回 inode 池
static void put_free_inode(inode_t *inode) {
    assert(inode != &inode_table[0]);   // 不能释放根目录的 inode
    assert(inode->count == 0);          // 保证被释放 inode 的引用数为 0
    inode->dev_id = EOF;
}

// 初始化 inode 池
void inode_init() {
    for (size_t i = 0; i < inode_nr; i++) {
        inode_t *inode = &inode_table[i];
        // inode 对应的设备号为 0 表示该 inode 空闲
        inode->dev_id = eof;
    }
}
```

#### 1.1.2 inode 链表

除了 inode 池统一存放内核管理的 inode 之外，超级块和 inode (内存版本) 之间通过链表进行联系，超级块会对当前已使用的 inode 统一链接起来 (类似于 malloc/free 机制)。

```c
//--> fs/inode.c

// 从设备对应文件系统当前已使用的 inode 链表中，获取 inode 号为 nr 的 inode
static inode_t *find_inode(devid_t dev_id, size_t nr) {
    // 保证从已挂载的超级块中寻找
    superblock_t *sb = get_superblock(dev_id);
    assert(sb);

    list_t *list = &sb->inode_list;
    for (list_node_t *node = list->head.next; node != &list->tail; node = node->next) {
        inode_t *inode = element_entry(inode_t, node, node);
        if (inode->nr == nr) {
            return inode;
        }
    }
    return NULL;
}
```

#### 1.1.3 iget / iput

`iget() / iput()` 这两个函数是核心，它们将之前提到的 inode 池和 inode 链表这两个机制也联系起来，给内核提提供一个抽象的 inode 处理接口 (类似于高速缓冲 bcache 机制)。

因为可能需要直接读取硬盘的第 nr 个 inode，而对硬盘读取需要通过文件系统 / bcache 机制，所以需要对 inode 号和 block 号之间进行转换，以成功读取到期望的 inode 部分：

```c
//--> fs/inode.c

// 计算 inode 号为 nr 的 inode 所在文件系统上的块号
static _inline size_t inode_block(superblock_t *sb, size_t nr) {
    return 2 + sb->desc->imap_blocks + sb->desc->zmap_blocks + (nr - 1) / BLOCK_INODES;
}

// 获取设备的 inode 号为 nr 对应的 inode
inode_t *iget(devid_t dev_id, size_t nr) {
    // 先从已使用的 inode 链表中寻找
    inode_t *inode = find_inode(dev_id, nr);
    if (inode) {
        inode->count++;         // 更新引用计数
        inode->atime = time();  // 更新访问时间
        return inode;
    }

    // 否则就获取一个空闲 inode 并进行相应设置
    superblock_t *sb = get_superblock(dev_id);
    assert(sb);

    assert(nr <= sb->desc->ninodes); // 保证 inode 号合法

    inode = get_free_inode();
    // 加入超级块的使用 inode 链表
    list_push_back(&sb->inode_list, &inode->node);
    // 读取 inode 所在的块并设置
    size_t block = inode_block(sb, nr);
    buffer_t *buf = bread(dev_id, block);

    inode->buf = buf;
    inode->desc = &((inode_desc_t *)buf->data)[(nr - 1) % BLOCK_INODES];
    inode->dev_id = dev_id;
    inode->nr = nr;
    inode->count = 1;
    inode->atime = time();
    inode->ctime = inode->desc->mtime; 

    return inode;
}

// 释放 inode 会 inode 池
void iput(inode_t *inode) {
    if (!inode) {
        return;
    }

    inode->count--; // 更新引用计数
    assert(inode->count >= 0);

    // 如果引用计数不为 0，直接返回
    if (inode->count > 0) {
        return;
    }
    // 否则需要释放 inode 对应的高速缓冲
    brelse(inode->buf);
    // 从超级块的使用 inode 链表移除
    list_remove(&inode->node);
    // 释放 inode 回 inode 池
    put_free_inode(inode);
}
```

### 1.2 块索引

inode 的 `zone` 存放者块索引，并且包含直接、一级间接、二级间接索引这三种类型。`bamp()` 函数提供了一个抽象的接口，用于操作 inode 索引的第 nr 块，而无需考虑该块被索引的类型。

为了处理时的方便，需要定义一些常量：

```c
//--> include/xos/fs.h

define BLOCK_BITS (BLOCK_SIZE * 8)                          // 一个块的比特数
#define BLOCK_INODES (BLOCK_SIZE / sizeof(inode_desc_t))    // 一个块可容纳的 inode 数
#define BLOCK_DENTRIES (BLOCK_SIZE / sizeof(dentry_t))      // 一个块可容纳的 dentry 数
#define BLOCK_INDEXES (BLOCK_SIZE / sizeof(u16))            // 一个块可容纳的索引数

#define DIRECT_BLOCKS (7)                                       // 直接索引的块数
#define INDIRECT1_BLOCKS BLOCK_INDEXES                          // 一级间接索引的块数
#define INDIRECT2_BLOCKS (INDIRECT1_BLOCKS * INDIRECT1_BLOCKS)  // 二级间接索引的块数
#define TOTAL_BLOCKS (DIRECT_BLOCKS + INDIRECT1_BLOCKS + INDIRECT2_BLOCKS) // 一个 inode 可以索引的全部块数
```

分别对直接索引、一级间接索引、二级间接索引进行处理（类似于多级页表的 map 机制）：

```c
//--> fs/fsmap.c

// 获取 inode 索引的第 nr 个块对应的块号
// 如果该块不存在且 create 为 true，则创建
size_t bmap(inode_t *inode, size_t nr, bool create) {
    // 保证块号合法
    assert(nr >= 0 && nr < TOTAL_BLOCKS);

    // 获取 zone 索引数组
    u16 *array = inode->desc->zone;
    size_t index;   // zone 数组下标
    size_t level;   // 索引间接层级
    
    // 下一层级的单位块数 (用于定位一级/二级间接索引)
    size_t blocks[2] = {1, BLOCK_INDEXES};

    // 获取 inode 对应的缓冲区
    buffer_t *buf = inode->buf;

    // 直接索引
    if (nr < DIRECT_BLOCKS) {
        index = nr;
        level = 0;
        goto reckon;
    }

    // 一级间接索引
    nr -= DIRECT_BLOCKS; // 转换成一级间接索引的第 nr 块
    if (nr < INDIRECT1_BLOCKS) {
        index = DIRECT_BLOCKS;
        level = 1;
        goto reckon;
    }

    // 二级间接索引
    nr -= INDIRECT1_BLOCKS; // 转换成二级间接索引的第 nr 块
    if (nr < INDIRECT2_BLOCKS) {
        index = DIRECT_BLOCKS + 1;
        level = 2;
        goto reckon;
    }

    panic("unreachable!!!");
reckon:
    while (level--) {
        if (!array[index] && create) {
            array[index] = balloc(inode->dev_id);
            buf->dirty = true;
        }
        bwrite(buf);

        // 索引不存在且 create 为 false 直接返回
        if (!array[index]) break;

        // 读取下一层级对应的块
        buf = bread(inode->dev_id, array[index]);
        array = (u16 *)buf->data;
        index = nr / blocks[level];
        nr = nr % blocks[level];
    }

    if (!array[index] && create) {
        array[index] = balloc(inode->dev_id);
        buf->dirty = true;
    }
    bwrite(buf);
    return array[index];
}
```

### 1.3 超级块

补充根文件系统挂载逻辑：

```c
//-> fs/super.c

// 挂载根文件系统
static void mount_root() {
    ...
    // 初始化根目录 inode
    root->iroot = iget(dev->dev_id, 1);
}
```

### 1.4 进程

补充进程关于 inode 的相关逻辑：

```c
//--> include/xos/task.h

// 任务控制块 TCB
typedef struct task_t {
    ...
    struct inode_t *ipwd;              // 进程当前目录对应 inode
    struct inode_t *iroot;             // 进程根目录对应 inode
    ...
}

//--> kernel/task.c

// 创建一个默认的任务 TCB
static task_t *task_create(target_t target, const char *name, u32 priority, u32 uid) {
    ...
    task->ipwd = get_root_inode();
    task->iroot = get_root_inode();
    ...
}
```

### 1.5 内核初始化

补充初始化 inode 池的逻辑，并且需要在初始化超级块之前，因为超级块初始化需要使用 inode 池：

```c
//--> kernel/main.c

void kernel_init() {
    ...
    inode_init();
    super_init();
    ...
}
```

## 2. 功能测试

在挂载根文件系统时对 `iget`, `iput`, `bmap` 进行测试：

```c
//--> fs/super.c

static void mount_root() {
    ...
    size_t nr = 0;
    inode_t *inode = iget(dev->dev_id, 1);

    // 获取直接块
    nr = bmap(inode, 3, true);
    // 一级间接块
    nr = bmap(inode, 7 + 7, true);
    // 二级间接块
    nr = bmap(inode, 7 + 512 * 3 + 510, true);

    iput(inode);
}
```

使用调试追踪执行流程，确认执行流程是否符合预期。另外，`nr` 的预期分别为：

- `nr = bmap(inode, 3, true);` -> 171（只分配了一个块）
- `nr = bmap(inode, 7 + 7, true);` -> 173（分配了两个块，一个是一级间接块，一个是数据块）
- `nr = bmap(inode, 7 + 512 * 3 + 510, true);` -> 176（分配了三个块，一个二级间接块，一个一级间接块，一个数据块）

## 3. 参考文献

- EXT文件系统机制原理详解: https://www.51cto.com/article/603104.html
