# 085 文件系统位图操作

## 位图结构

![](./images/fsmap.drawio.svg)

imap 第一个比特索引 inode 1，第二个比特索引 inode 2，以此类推。所以以位图 `bitmap_t` 对 imap 进行操作时，需要设置偏移量 `offset` 为 1，使得位图索引和索引的 inode 号一致，简化计算。

zmap 也类似，但是它的第一个比特恒为 1，第二个比特索引 first data block，第三个比特索引 (first data block + 1)，以此类推。所以以位图 `bitmap_t` 对 zmap 进行操作时，需要设置偏移量 `offset` 为 (first data block - 1)，使得位图索引和索引的 data block 号一致（除了第一个比特，因为它恒为 1，所以并没有影响），简化计算。

Stack Overflow: [Why is root directory always stored in inode two?](https://stackoverflow.com/questions/12768371/why-is-root-directory-always-stored-in-inode-two)

> The first inode number is 1. 0 is used as a NULL value, to indicate that there is no inode. Inode 1 is used to keep track of any bad blocks on the disk; it is essentially a hidden file containing the bad blocks, so that they will not be used by another file. The bad blocks can be recorded using `e2fsck -c`. The filesystem root directory is inode 2.

## 代码分析

主要完成以下四个函数：

```c
// 分配一个文件块并返回块号 (从 1 开始计数)
size_t balloc(devid_t dev_id);

// 释放一个文件块 (块号从 1 开始计数)
void bfree(devid_t dev_id, size_t nr);

// 分配一个文件系统 inode 并返回 inode 号 (从 1 开始计数)
size_t ialloc(devid_t dev_id);

// 释放一个文件系统 inode (inode 号从 1 开始计数)
void ifree(devid_t dev_id, size_t nr);
```

需要注意的有两个点：

- 块号和 inode 号都是从 1 开始计数，而我们用于解析 zmap 和 imap 的位图需要从合适的偏移量开始，例如 imap 从偏移量 1 开始，zmap 从偏移量 (first data block - 1) 开始。

- 在 `bfree` 和 `ifree` 中，为了调试观察的方便，我们使用 `bwrite` 进行了强行同步，其实可以在位图对应的 buf 释放时再一起写回的，因为后续读写的都是 buf 的 data 字段（类似于 cache 的写回策略）。

## 功能测试

> **进行测试前必须要进行 `make clean` 操作，因为测试时会修改原有的磁盘内容。**

- mount_root

对第 1 个分区，即从硬盘的分区，进行块位图和 inode 位图的测试。因为该分区只创建了一个文件 hello.txt

```makefile
	echo "slave root direcotry file..." > /mnt/hello.txt
```

所以预期为：

- 初始时前 3 个 inode 被占用，inode 1 指向 bad blocks，inode 2 指向 root directory，inode 3 指向 file hello.txt
- 分配 inode 后前 4 个 inode 被占用，释放 inode 后则恢复为前 3 个 inode 被占用（因为我们进行了强行写回同步）
- 过程中分配的 inode 号为 4
> `(char *)sb->imaps[0]->data,128` (因为 sizeof(char) * 128 = 1024 Byte 刚好是一块的大小)

- 初始时 first data block 开始的前 2 个 block 被占用，zmap 中会显示前 3 个比特均为 1，这是因为 zmap 的第一个比特恒为 1
- 分配 block 后前 3 个 data block 被占用，释放 data block 后则恢复为前 2 个 data block 被占用（因为我们进行了强行写回同步）
- 过程中分配的 data block 块号为 (first data block + 2)，因为第一个 data block 是根目录 `/` 的内容，第二个 data block 是文件 hello.txt 的内容
> `(char *)sb->zmaps[0]->data,128` (因为 sizeof(char) * 128 = 1024 Byte 刚好是一块的大小)




