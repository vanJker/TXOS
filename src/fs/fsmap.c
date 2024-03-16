#include <xos/fs.h>
#include <xos/assert.h>
#include <xos/bitmap.h>

// 分配一个文件块并返回块号 (从 1 开始计数)，分配失败返回 0
size_t balloc(devid_t dev_id) {
    // 保证从已挂载的超级块中分配
    superblock_t *sb = get_superblock(dev_id);
    assert(sb);

    size_t nr = 0;
    buffer_t *buf = NULL;
    bitmap_t bitmap;

    for (size_t i = 0; i < ZMAP_MAX_BLOCKS; i++) {
        buf = sb->zmaps[i];
        size_t offset = (sb->desc->first_data_zone - 1) + i * BLOCK_BITS; // 计算位图的偏移量
        bitmap_new(&bitmap, buf->data, BLOCK_BITS, offset);

        nr = bitmap_insert_nbits(&bitmap, 1);
        // 如果在合法范围扫描到空闲的块，返回扫描到的空闲块的块号，并标记 zmap 相应的位
        if (nr != EOF && nr < sb->desc->nzones) {
            buf->dirty = true;
            bwrite(buf);
            return nr;
        }
    }
    // 如果没有扫描到空闲的块，返回 0
    return 0;
}

// 释放一个文件块 (块号从 1 开始计数)
void bfree(devid_t dev_id, size_t nr) {
    // 保证从已挂载的超级块中释放
    superblock_t *sb = get_superblock(dev_id);
    assert(sb);

    buffer_t *buf = NULL;
    bitmap_t bitmap;

    for (size_t i = 0; i < ZMAP_MAX_BLOCKS; i++) {
        // 如果是非所在范围的位图块，则跳过
        if (nr >= (sb->desc->first_data_zone - 1) + (i + 1) * BLOCK_BITS) {
            continue;
        }
        // 否则对位图设置相应位
        buf = sb->zmaps[i];
        size_t offset = (sb->desc->first_data_zone - 1) + i * BLOCK_BITS; // 计算位图的偏移量
        bitmap_new(&bitmap, buf->data, BLOCK_BITS, offset);

        assert(bitmap_contains(&bitmap, nr)); // 禁止释放未分配的块
        bitmap_remove(&bitmap, nr);
        buf->dirty = true;

        break;
    }
    // TODO: 调试时强行同步，其它时候可以释放时统一写回，提高效能
    bwrite(buf);
}

// 分配一个文件系统 inode 并返回 inode 号 (从 1 开始计数)
size_t ialloc(devid_t dev_id) {
    // 保证从已挂载的超级块中分配
    superblock_t *sb = get_superblock(dev_id);
    assert(sb);

    size_t nr = 0;
    buffer_t *buf = NULL;
    bitmap_t bitmap;

    for (size_t i = 0; i < IMAP_MAX_BLOCKS; i++) {
        buf = sb->imaps[i];
        bitmap_new(&bitmap, buf->data, BLOCK_BITS, 1 + i * BLOCK_BITS);

        nr = bitmap_insert_nbits(&bitmap, 1);
        // 如果在合法范围扫描到空闲的 inode，返回扫描到的空闲 inode 号，并标记 imap 相应的位
        if (nr != EOF && nr < sb->desc->ninodes) {
            buf->dirty = true;
            bwrite(buf);
            return nr;
        }
    }
    // 如果没有扫描到空闲的 inode，返回 0
    return 0;
}

// 释放一个文件系统 inode (inode 号从 1 开始计数)
void ifree(devid_t dev_id, size_t nr) {
    // 保证从已挂载的超级块中释放
    superblock_t *sb = get_superblock(dev_id);
    assert(sb);

    buffer_t *buf = NULL;
    bitmap_t bitmap;   

    for (size_t i = 0; i < IMAP_MAX_BLOCKS; i++) {
        // 如果是非所在范围的位图块，则跳过
        if (nr >= 1 + (i + 1) * BLOCK_BITS) {
            continue;
        }

        buf = sb->imaps[i];
        bitmap_new(&bitmap, buf->data, BLOCK_BITS, 1 + i * BLOCK_BITS);

        assert(bitmap_contains(&bitmap, nr)); // 禁止释放未分配的块
        bitmap_remove(&bitmap, nr);
        buf->dirty = true;

        break;
    }
    // TODO: 调试时强行同步，其它时候可以释放时统一写回，提高效能
    bwrite(buf);
}

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
