#include <xos/fs.h>
#include <xos/device.h>
#include <xos/buffer.h>
#include <xos/string.h>
#include <xos/assert.h>
#include <xos/debug.h>

// 系统最多支持 16 个文件系统
#define SUPERBLOCK_NR 16

// 超级块表
static superblock_t superblock_table[SUPERBLOCK_NR];

// 根文件系统对应的超级块
static superblock_t *root;

// 从超级块表中获取一个空闲超级块
static superblock_t *get_free_superblock() {
    for (size_t i = 0; i < SUPERBLOCK_NR; i++) {
        superblock_t *sb = &superblock_table[i];
        if (sb->dev_id == -1) {
            return sb;
        }
    }
    panic("no more super block!!!");
}

// 在超级块表中查找设备号 dev_id 对应的超级块，没有则返回 NULL
superblock_t *get_superblock(devid_t dev_id) {
    for (size_t i = 0; i < SUPERBLOCK_NR; i++) {
        superblock_t *sb = &superblock_table[i];
        if (sb->dev_id == dev_id) {
            return sb;
        }
    }
    return NULL;
}

// 读取设备号 dev_id 对应的超级块
superblock_t *read_superblock(devid_t dev_id) {
    // 在超级块表中寻找所需的超级块，如果找到则直接返回
    superblock_t *sb = get_superblock(dev_id);
    if (sb) return sb;

    // 否则获取一个空闲超级块并对其进行设置
    LOGK("Reading super block of device %d\n", dev_id);
    
    // 获取空闲超级块
    sb = get_free_superblock();
    
    // 读取超级块并设置
    buffer_t *buf = bread(dev_id, 1);
    sb->buf = buf;
    sb->desc = (super_desc_t *)buf->data;
    sb->dev_id = dev_id;

    assert(sb->desc->magic == MINIX_MAGIC);

    // 因为可能不需要这么多的位图空间，所以需要将其设置为 0，防止非法访问
    memset(sb->imaps, 0, sizeof(sb->imaps));
    memset(sb->zmaps, 0, sizeof(sb->zmaps));

    // 块位图从第 2 块开始，第 0 块为 boot block，第 1 块为 super block
    size_t idx = 2;

    // 读取 inode 位图
    for (size_t i = 0; i < sb->desc->imap_blocks; i++) {
        assert(i < IMAP_MAX_BLOCKS);
        if (sb->imaps[i] = bread(dev_id, idx)) 
            idx++;
        else 
            panic("unreachable!!!");
    }

    // 读取块位图
    for (size_t i = 0; i < sb->desc->zmap_blocks; i++) {
        assert(i < ZMAP_MAX_BLOCKS);
        if (sb->zmaps[i] = bread(dev_id, idx)) 
            idx++;
        else 
            panic("unreachable!!!");
    }

    return sb;
}

// 挂载根文件系统
static void mount_root() {
    LOGK("Mount root file system...\n");
    
    // 假设主硬盘的首个分区是根文件系统
    dev_t *dev = dev_find(DEV_ATA_PART, 0);
    assert(dev);

    // 读取根文件系统的超级块
    root = read_superblock(dev->dev_id);

    // 初始化根目录 inode
    root->iroot = iget(dev->dev_id, 1);

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

// 初始化文件系统
void super_init() {
    // 初始化超级块表
    for (size_t i = 0; i < SUPERBLOCK_NR; i++) {
        superblock_t *sb = &superblock_table[i];
        sb->desc = NULL;
        sb->buf = NULL;
        sb->dev_id = -1;
        list_init(&sb->inode_list);
        sb->iroot = NULL;
        sb->imount = NULL;
    }

    // 挂载根文件系统
    mount_root();
}