# 083 Minix 文件系统

一般常见的文件系统分为两种类型：

- 文件分配表 (File Allocation Table FAT)
- 索引表

minux 文件系统使用索引表结构

> **以下的文件系统特指 MINIX 文件系统**

## 块 (block/zone)

![](./images/block.drawio.svg)

文件系统的 **块** (block / zone) 与高速缓冲 (bcache) 中的块概念类似，都是一个或多个扇区的组合。高速缓冲 (bcache) 的块大小是由 **系统内核** 来决定的，例如 XOS 高速缓冲采用的块大小为 1K，其它系统可能会采用 2K， 4K 的块大小。文件系统 (file system) 的块大小则是由 **文件系统** 来决定，例如 MINIX 文件系统采用的块大小为 1K，其它文件系统可能采用 4K 的块大小。

因为 XOS 的 bcache 和 MINIX 文件系统的块大小恰好均为 1K，所以简化了本节的操作，无需进行 bcache 和 fs 之间块的转换（例如，如果 bcache 块大小为 1K，文件系统块大小为 2K，则需要使用 bcache 进行两次读取才能获取文件系统的一个块）。

fs 与 bcache 块还有一个区别点是：**bcache 的块是从 0 开始计数的，而 fs 的块是从 1 开始计数。** 但是在 fs 的块位图 zmap 中，又是以 0 开始计数，这时候需要进行一些转换。inode 也是类似的，从 1 开始计数，但是在 inode 位图 imap 中，又是以 0 开始计数（这样有一些好处，当目录项中 inode 为 0 时表示该目录项无效）

## inode

```c
// 磁盘中的 inode 格式 (可用于磁盘和内存)
typedef struct inode_desc_t {
    u16 mode;               // 文件类型和属性 (rwx)
    u16 uid;                // 文件拥有者 id
    u32 size;               // 文件大小 (/Bytes)
    u32 mtime;              // 修改时间戳
    u8 gid;                 // 文件拥有者所在组 id
    u8 nlinks;              // 文件链接数 (多少个目录项指向该 inode)
    u16 zone[9];            // 直接索引 (0-6)，一级间接索引 (7)，二级简洁索引 (8)
} inode_desc_t;
```

因为一个索引占据的空间为 2 个字节，所以将一个 block/zone 全部作为索引表可以包含$1024/2 = 512$ 个索引。因此 MINIX 文件系统总索引数量为 $512 = 7 + 512 + 512 * 512$ 个。

![](./images/inode.drawio.svg)

## 超级块 (super block)

```c
// 磁盘中的 superblock 格式 (可用于磁盘和内存)
typedef struct super_desc_t {
    u16 ninodes;            // Number of inodes
    u16 nzones;             // Number of zones
    u16 imap_blocks;        // Space used by inode map (blocks)
    u16 zmap_blocks;        // Spaceused by zone map (blocks)
    u16 first_data_zone;    // First zone with file data
    u16 log_zone_size;      // Size of data zone = (block_size << log_zone_size)
    u32 max_size;           // Maxium file size (bytes)
    u16 magic;              // Minix ID/magic number
} super_desc_t;
```
![](./images/block1.drawio.svg)

第 0 个块是该文件系统的引导块 (boot block)，第 1 个块则是该文件系统的超级块 (super block)，用于指示文件系统的结构。

这里说明一下 `zmap_block` 和 `log_zone_size` 字段：

- `zmap_blcock` 是块 (blcok / zone) 的位图，但是它的偏移量是从 `first_data_zone` 所指的那个块开始的（因为 `first_data_zone` 之前的块显然都被使用了，而且也不能被释放，所以无需使用位图记录它们的使用信息）。事实上正如上面所说的，块位图 `zmap` 是从 0 开始计数的，所以进行转换，`zmap` 的起始偏移量为 `first_data_zone` - 1。

- `log_zone_size` 则是用于在保持索引数不变时，扩大可以表示的文件大小。一般来说 `log_zone_size` 为 0，所以一个索引表示 `1K << 0` 大小，如果一个索引为 63 则表示该索引指向第 63 块。但是如果 `log_zone_size` 为 1 则一个索引表示 `1K << 1 == 2K` 大小，如果索引为 63，则表示第 63, 64 块，显然可以表示的文件大小变为了原来的 2 倍。

事实上 `log_zone_size` 很少用于增大文件大小，因为在 MINIX 文件系统中可以表示 $(7 + 512 + 512 * 512)K = 263M$，但是并没有这么多的块可供索引，所以最大表示 $64M$ 的文件。

## 目录 (directory)

如果 inode 文件类型是目录，那么 inode 索引的块内容就是下面这种数据结构。

```c
// 磁盘中的目录项结构 (可用于磁盘和内存)
typedef struct dentry_t {
    u16 inode;              // inode 编号
    char name[FILENAME_LEN];// 文件名
} dentry_t;
```

## 代码分析

> **请根据以下的说明，从代码中寻找逻辑**

### MINIX 文件系统

相关常量和结构定义：

- include/fs.h

### 超级块操作

读取 MINIX 文件系统的超级块并进行相应操作：

- fs/super.c

## 功能测试

> 每次测试前都必须要要进行 `make clean`，因为都修改原先文件系统的内容，而测试是针对原先文件系统的内容进行的。

在 `super_init()` 处进行调试观察：

- ninodes 与 nzones 比例大致为 $1:3$
- imap_blocks 为 1，zmap_blocks 为 2，log_zone_size 为 0
- 在 `super_init()` 结束后，原先文件系统根目录的 hello.txt 被改名为 world.txt，其内容也被修改了

## 参考文献

- <http://ohm.hgesser.de/sp-ss2012/Intro-MinixFS.pdf>
- <https://en.wikipedia.org/wiki/MINIX_file_system>
- [赵炯 / Linux内核完全注释 / 机械工业出版社 / 2005](https://book.douban.com/subject/1231236/)
- <https://wiki.osdev.org/FAT>
