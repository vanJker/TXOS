#include <xos/fs.h>
#include <xos/device.h>
#include <xos/buffer.h>
#include <xos/string.h>
#include <xos/assert.h>
#include <xos/debug.h>

// 初始化文件系统 (读取 superblock)
void super_init() {
    dev_t *dev = dev_find(DEV_ATA_PART, 0);
    assert(dev);

    // 读取引导块和超级块
    buffer_t *boot  = bread(dev->dev_id, 0);
    buffer_t *super = bread(dev->dev_id, 1);

    // 解析超级块，确认是 MINIX 文件系统
    super_desc_t *sb = (super_desc_t *)super->data;
    assert(sb->magic == MINIX_MAGIC);

    // 获取 inode 位图和 zone 位图对应的第一个块
    buffer_t *imap = bread(dev->dev_id, 2);
    buffer_t *zmap = bread(dev->dev_id, 2 + sb->imap_blocks);

    // 读取第一个 inode (对应根目录)
    buffer_t *buf_inode = bread(dev->dev_id, 2 + sb->imap_blocks + sb->zmap_blocks);
    inode_desc_t *inode = (inode_desc_t *)buf_inode->data;

    // 读取第一个 inode 对应的文件内容 (对应根目录的内容)
    buffer_t *buf_file = bread(dev->dev_id, inode->zone[0]);
    dentry_t *dir = (dentry_t *)buf_file->data;

    // 列出根目录的内容
    inode_desc_t *helloi = NULL;
    while (dir->inode) {
        LOGK("inode %04d, name %s\n", dir->inode, dir->name);
        // 修改 hello.txt 的文件名，并写回磁盘
        if (strcmp(dir->name, "hello.txt") == 0) {
            helloi = &((inode_desc_t *)buf_inode->data)[dir->inode - 1];
            strcpy(dir->name, "world.txt");
            buf_file->dirty = true;
            bwrite(buf_file);
        }
        dir++;
    }

    // 如果没找到 hello 文件，则直接返回
    if (!helloi) return;

    // 打印文件 hello.txt 的内容
    buffer_t *buf_hello = bread(dev->dev_id, helloi->zone[0]);
    LOGK("content: %s\n", buf_hello->data);

    // 修改文件 hello.txt 的内容
    strcpy((char *)buf_hello->data, "This is modified content...\n");
    buf_hello->dirty = true;
    bwrite(buf_hello);

    // 修改文件 hello.txt 的属性
    helloi->size = strlen(buf_hello->data);
    buf_inode->dirty = true;
    bwrite(buf_inode);
}