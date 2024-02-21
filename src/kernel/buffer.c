#include <xos/buffer.h>
#include <xos/memory.h>
#include <xos/device.h>
#include <xos/assert.h>
#include <xos/debug.h>

#define HASH_COUNT 31   // 哈希表容量

// 高速缓存的起始地址
static uptr buffer_base = (uptr)KERNEL_BUFFER_BASE;
// 高速缓存的当前待分配地址
static buffer_t *buffer_ptr  = (buffer_t *)KERNEL_BUFFER_BASE;
// 高速缓存数据区的当前待分配地址
static uptr buffer_data = (uptr)(KERNEL_BUFFER_BASE + KERNEL_BUFFER_SIZE - BLOCK_SIZE);
// 高速缓存分配个数
static size_t buff_cnt = 0;

static list_t hash_table[HASH_COUNT];   // 高速缓存哈希表
static list_t free_list;    // 空闲链表
static list_t wait_list;    // 等待链表

// 哈希函数
static size_t hash(devid_t dev_id, size_t block) {
    return (dev_id ^ block) % HASH_COUNT;
}

// 将 bf 加入哈希表
static void hash_insert(buffer_t *bf) {
    size_t idx = hash(bf->dev_id, bf->block);
    list_t *list = &hash_table[idx];
    assert(!list_contains(list, &bf->hnode));
    list_push_front(list, &bf->hnode);
}

// 将 bf 从哈希表中移除
static void hash_remove(buffer_t *bf) {
    size_t idx = hash(bf->dev_id, bf->block);
    list_t *list = &hash_table[idx];
    assert(list_contains(list, &bf->hnode));
    list_remove(&bf->hnode);
} 

// 在哈希表中获取设备号 dev_id 第 block 块对应缓存，如果没有直接返回 NULL
static buffer_t *get_from_hash_table(devid_t dev_id, size_t block) {
    size_t idx = hash(dev_id, block);
    list_t *list = &hash_table[idx];
    buffer_t *bf = NULL;

    for (list_node_t *node = list->head.next; node != &list->tail; node = node->next) {
        buffer_t *ptr = element_entry(buffer_t, hnode, node);
        if (ptr->dev_id == dev_id && ptr->block == block) {
            bf = ptr;
            break;
        }
    }

    // 不在哈希表中
    if (bf == NULL) {
        return NULL;
    }

    // 如果 bf 在空闲链表中，则移除出空闲链表
    if (list_contains(&free_list, &bf->rnode)) {
        list_remove(&bf->rnode);
    }

    return bf;
}

// 高速缓存 Lazy Allocation，如果没有多余空间，返回 NULL
static buffer_t *get_new_buffer() {
    buffer_t *bf = NULL;

    if ((uptr)buffer_ptr + sizeof(buffer_t) < buffer_data) {
        bf = (buffer_t *)buffer_ptr;
        bf->data = (void *)buffer_data;
        bf->dev_id = -1;
        bf->block = 0;
        bf->count = 0;
        mutexlock_init(&bf->lock);
        bf->dirty = false;
        bf->valid = false;

        // 更新 buffer_ptr 和 buffer_data 所指地址
        buffer_ptr++;
        buffer_data -= BLOCK_SIZE;

        LOGK("buffer count %d\n", ++buff_cnt);
    }

    return bf;
}

// 获取空闲的 buffer
static buffer_t *get_free_buffer() {
    buffer_t *bf = NULL;
    while (true) {
        // 如果空闲内存足够，直接分配缓存
        bf = get_new_buffer();
        if (bf) return bf;
        
        // 否则从空闲链表中获取
        if (!list_empty(&free_list)) {
            // LRU 取最近最少被访问的块
            bf = element_entry(buffer_t, rnode, list_pop_back(&free_list));
            // 从哈希表移除
            hash_remove(bf);
            // 进行设置
            bf->valid = false;
            return bf;
        }

        // 如果当前没有空闲块，则阻塞等待直到有块被释放
        task_block(current_task(), &wait_list, TASK_BLOCKED);
    }
}

// 获取设备号 dev_id 第 block 块对应的缓存
static buffer_t *getblk(devid_t dev_id, size_t block) {
    // 先在哈希表中寻找，如果找到了则增加缓存引用计数
    buffer_t *bf = get_from_hash_table(dev_id, block);
    if (bf) {
        bf->count++;
        return bf;
    }

    // 否则获取空闲缓存
    bf = get_free_buffer();
    assert(bf->count == 0);
    assert(bf->dirty == false);

    // 设置块并加入哈希表
    bf->dev_id = dev_id;
    bf->block = block;
    bf->count = 1;
    hash_insert(bf);

    return bf;
}

// 读取设备号 dev_id 的第 block 块数据到高速缓存
buffer_t *bread(devid_t dev_id, size_t block) {
    buffer_t *bf = getblk(dev_id, block);
    assert(bf != NULL);

    // 如果缓存有效，直接返回
    if (bf->valid) {
        return bf;
    }

    // 否则请求读取对应的数据，并设置相应标识
    dev_request(bf->dev_id, bf->data, BLOCK_SECS, bf->block * BLOCK_SECS, 0, REQ_READ);
    bf->dirty = false;
    bf->valid = true;

    return bf;
}

// 将高速缓存 bf 的数据写回设备对应区域
void bwrite(buffer_t *bf) {
    assert(bf != NULL);

    // 如果缓存不为脏，则直接返回
    if (!bf->dirty) {
        return;
    }

    // 否则请求写入对应的数据，并设置相应标识
    dev_request(bf->dev_id, bf->data, BLOCK_SECS, bf->block * BLOCK_SECS, 0, REQ_WRITE);
    bf->dirty = false;
    bf->valid = true;
}

// 释放高速缓存 bf
void brelse(buffer_t *bf) {
    if (bf == NULL) return;

    // 减少缓存引用计数
    bf->count--;
    assert(bf->count >= 0);

    // 如果缓存引用计数为 0，则加入空闲链表
    if (bf->count == 0) {
        ASSERT_NODE_FREE(&bf->rnode);
        list_push_front(&free_list, &bf->rnode);
    }

    // 如果缓存为脏，则先进行写回
    if (bf->dirty) {
        bwrite(bf);
    }

    // 如果空闲链表不为空，即有等待缓存的任务，则进行唤醒
    if (!list_empty(&wait_list)) {
        task_t *task = element_entry(task_t, node, list_pop_front(&wait_list));
        task_unblock(task);
    }
}

// 高速缓冲初始化
void buffer_init() {
    LOGK("buffer_t size if %d\n", sizeof(buffer_t));

    // 初始化空闲链表
    list_init(&free_list);
    // 初始化等待链表
    list_init(&wait_list);
    // 初始化哈希表
    for (size_t i = 0; i < HASH_COUNT; i++) {
        list_init(&hash_table[i]);
    }
}
