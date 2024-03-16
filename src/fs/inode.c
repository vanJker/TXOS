#include <xos/fs.h>
#include <xos/syscall.h>
#include <xos/assert.h>

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

// 初始化 inode 池
void inode_init() {
    for (size_t i = 0; i < INODE_NR; i++) {
        inode_t *inode = &inode_table[i];
        // inode 对应的设备号为 0 表示该 inode 空闲
        inode->dev_id = EOF;
    }
}
