# 081 哈希表和高速缓冲 (Block Cache)

## 哈希表概述

哈希表：散列表，以键值对 (key - value) 的形式直接进行访问的数据结构。

- 哈希函数：需要将 key 转换成整型，并且不同的 key 之间尽可能减少冲突
- 冲突的解决方法：
    - 开放地址法
    - 拉链法
- 访问的平均时间复杂度 $O(1)$
- 装载因子 load factor 需要尽可能小
- DDoS 攻击：会使得哈希表退化为一个链表，访问平均时间复杂度为 $O(n)$

![](./images/hashmap.drawio.svg)

## 高速缓冲

一般来说，性能不同的两个系统之间应该有一个缓冲区

- 缓存：
    - memcache / redis
- 缓冲：
    - 消息队列 / rabbitMQ / kafka

文件系统以块为单位访问磁盘，块一般是 $2^n, n \in \mathbb{N}$ 个扇区。其中 4K 比较常见；这里我们使用 1K，也就是 2 个扇区作为一块。

高速缓冲将块存储在哈希表中，以降低对磁盘的访问频率，提高性能。

另外还有一个空闲块链表：

![](./images/list.drawio.svg)

## 内存布局

![](./images/memory_map.drawio.svg)

增加 bcache 和 ramdisk (后续用于实现 ramfs 文件系统) 空间。

- memory.h
- memory.c

由于扩展了内核空间的布局，所以需要将页目录进行相应的扩展：

```c
//--> kernel/memory.c

static u32 KERNEL_PAGE_TABLE[] = {
    ...
    0x4000,
    0x5000,
};
```

## 高速缓冲布局

![](./images/buffer_map.drawio.svg)

块号的概念与扇区的 LBA 类似，即第 0, 1 个扇区构成第 0 块，第 2, 3 个扇区构成第 1 块，以此类推。

在这个 bcache 空间中，左侧的 buffer_t 每个占据 `sizeof(buffer_t)` 个字节，右侧的 data 每个占据 `BLOCK_SIZE (1024)` 个字节。

- buffer.h

## bcache 机制

> buffer.c

**请根据以下的机制说明，从代码中寻找逻辑**

### 哈希表相关操作

- hash
- hash_insert
- hash_remove
- buffer_init

### 空闲高速缓冲

获取空闲的高速缓冲时优先使用 Lazy Allocation 来获取分配区域，如果没有多余空间则从空闲链表获取 (使用 LRU 机制，获取空闲链表尾端节点)，如果空闲链表为空则进行阻塞等待直到有高速缓冲被释放 (先进先出机制)。

- get_new_buffer
- get_free_buffer

### 高速缓冲机制

bcache 类似于 Cache，在使用完高速缓存后，需要将其进行释放并加入空闲链表 (使用 LRU 机制，加入空闲链表的头端节点)，但是无需将其从哈希表中移除，因为该高速缓冲很有可能被立即使用到 (回想一下 cache 的机制)。所以获取高速缓冲时，如果其还在哈希表中，只需将其从空闲链表中移除即可。

同理，在空闲链表中获取的高速缓冲，需要将其从哈希表中移除再进行设置 (因为高速缓冲在哈希表中的位置由设备号和块号决定)

- get_from_hash_table
- getblk
- brelse

### Block 操作

封装块操作，注意有效位 valid, 脏位 dirty 这两个标识以及引用计数 count 的使用。有效位 valid 表示高速缓冲的数据是否为从对应设备区域读取的数据，脏位 dirty 表示高速缓冲的数据是否在对应设备区域数据基础上进行了修改，引用计数 count 表示使用 getblk 函数获取对应高速缓冲的次数 (一般是通过 bread 调用 getblk)。

当有效位 valid 为真时，bread 可以直接返回，否则需要读取对应设备区域的数据到高速缓冲的数据区。当脏位 dirty 为假时，bwrite 可以直接返回，否则需要将高速缓冲的数据写回到对应设备区域。当引用计数 count 为 0 时，brelse 需要将高速缓冲加入到空闲链表中 (使用 LRU 机制)，并检查等待列表，如果存在任务被阻塞等待空闲的高速缓冲，则唤醒该任务，因为此时存在了空闲的高速缓冲 (先进先出机制)。

- bread
- bwrite
- brelse

## 功能测试

预期结果为，主磁盘的第 1 个扇区被写入 0x5a 覆盖。

- syscall.c
- task.c
- thread.c
- master.img

过程中可以自行调试观察 bread, bwrite, brelse 是否按预期工作。

- `-exec x/512xb <addr>`
- `-exec x/1024xb <addr>`

## 参考文献

1. [赵炯 / Linux内核完全注释 / 机械工业出版社 / 2005](https://book.douban.com/subject/1231236/)
2. [Maurice J.Bach / UNIX操作系统设计 / 机械工业出版社 / 2000](https://book.douban.com/subject/1035710/)