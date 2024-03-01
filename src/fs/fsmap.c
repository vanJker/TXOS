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
