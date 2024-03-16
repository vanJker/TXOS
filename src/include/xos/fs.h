#ifndef XOS_FS_H
#define XOS_FS_H

#include <xos/types.h>
#include <xos/buffer.h>
#include <xos/list.h>

#define SECTOR_SIZE 512                         // 扇区大小 512B
#define BLOCK_SECS  2                           // 一块占 2 个扇区
#define BLOCK_SIZE  (BLOCK_SECS * SECTOR_SIZE)  // 块大小 1024B

#define BLOCK_BITS (BLOCK_SIZE * 8)                         // 一个块的比特数
#define BLOCK_INODES (BLOCK_SIZE / sizeof(inode_desc_t))    // 一个块可容纳的 inode 数
#define BLOCK_DENTRIES (BLOCK_SIZE / sizeof(dentry_t))      // 一个块可容纳的 dentry 数
#define BLOCK_INDEXES (BLOCK_SIZE / sizeof(u16))            // 一个块可容纳的索引数

#define DIRECT_BLOCKS (7)                                       // 直接索引的块数
#define INDIRECT1_BLOCKS BLOCK_INDEXES                          // 一级间接索引的块数
#define INDIRECT2_BLOCKS (INDIRECT1_BLOCKS * INDIRECT1_BLOCKS)  // 二级间接索引的块数
#define TOTAL_BLOCKS (DIRECT_BLOCKS + INDIRECT1_BLOCKS + INDIRECT2_BLOCKS) // 一个 inode 可以索引的全部块数

#define MINIX_MAGIC     0x137F  // MINIX 文件系统魔数
#define FILENAME_LEN    14      // 文件名长度
#define IMAP_MAX_BLOCKS 8       // inode 位图可以占据的最大块个数
#define ZMAP_MAX_BLOCKS 8       // 块位图可以占据的最大块个数

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

// 内存中的 inode 格式 (只能用于内存，提供给内核使用)
typedef struct inode_t {
    inode_desc_t *desc;     // inode 描述符
    buffer_t *buf;          // inode 描述符所在 buffer
    devid_t dev_id;         // 设备号
    size_t nr;              // inode 号
    u32 count;              // 引用计数
    time_t atime;           // access time
    time_t ctime;           // create time
    list_node_t node;       // inode 链表节点
    devid_t mount;          // 安装设备
} inode_t;

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


// 内存中的 superblock 格式 (只能用于内存，提供给内核使用)
typedef struct superblock_t {
    super_desc_t *desc;     // superblock 描述符
    buffer_t *buf;          // superblock 描述符所在 buffer
    buffer_t *imaps[IMAP_MAX_BLOCKS];   // inode 位图对应的 buffer
    buffer_t *zmaps[ZMAP_MAX_BLOCKS];   // 块位图对应的 buffer
    devid_t dev_id;         // 设备号
    list_t inode_list;      // 使用中的 inode 链表
    inode_t *iroot;         // 根目录对应的 inode
    inode_t *imount;        // 挂载文件系统对应的 inode
} superblock_t;

// 磁盘中的目录项结构 (可用于磁盘和内存)
typedef struct dentry_t {
    u16 inode;              // inode 编号
    char name[FILENAME_LEN];// 文件名
} dentry_t;

/* super.c */
// 在超级块表中查找设备号 dev_id 对应的超级块，没有则返回 NULL
superblock_t *get_superblock(devid_t dev_id);
// 读取设备号 dev_id 对应的超级块
superblock_t *read_superblock(devid_t dev_id);


/* fsmap.c */
// 分配一个文件块并返回块号 (从 1 开始计数)
size_t balloc(devid_t dev_id);
// 释放一个文件块 (块号从 1 开始计数)
void bfree(devid_t dev_id, size_t nr);
// 分配一个文件系统 inode 并返回 inode 号 (从 1 开始计数)
size_t ialloc(devid_t dev_id);
// 释放一个文件系统 inode (inode 号从 1 开始计数)
void ifree(devid_t dev_id, size_t nr);
// 获取 inode 索引的第 nr 个块对应的块号
// 如果该块不存在且 create 为 true，则创建
size_t bmap(inode_t *inode, size_t nr, bool create);

/* inode.c */
// 获取根目录对应的 inode (约定 inode 池的第一个 inode 用于存放根目录对应的 inode)
inode_t *get_root_inode();
// 获取设备的第 nr 个 inode
inode_t *iget(devid_t dev_id, size_t nr);
// 释放 inode 会 inode 池
void iput(inode_t *inode);

#endif