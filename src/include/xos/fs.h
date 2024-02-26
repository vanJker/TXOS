#ifndef XOS_FS_H
#define XOS_FS_H

#include <xos/types.h>

#define SECTOR_SIZE 512                         // 扇区大小 512B
#define BLOCK_SECS  2                           // 一块占 2 个扇区
#define BLOCK_SIZE  (BLOCK_SECS * SECTOR_SIZE)  // 块大小 1024B

#define MINIX_MAGIC     0x137F  // MINIX 文件系统魔数
#define FILENAME_LEN    14      // 文件名长度

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

// 磁盘中的目录项结构 (可用于磁盘和内存)
typedef struct dentry_t {
    u16 inode;              // inode 编号
    char name[FILENAME_LEN];// 文件名
} dentry_t;

#endif