#ifndef XOS_BUFFER_H
#define XOS_BUFFER_H

#include <xos/types.h>
#include <xos/list.h>
#include <xos/mutex.h>

#define SECTOR_SIZE 512                         // 扇区大小 512B
#define BLOCK_SECS  2                           // 一块占 2 个扇区
#define BLOCK_SIZE  (BLOCK_SECS * SECTOR_SIZE)  // 块大小 1024B

// 高速缓存
typedef struct buffer_t {
    void *data;         // 数据区
    devid_t dev_id;     // 设备号
    size_t block;       // 块号
    size_t count;       // 引用计数
    list_node_t hnode;  // 哈希表拉链节点
    list_node_t rnode;  // 空闲链表节点
    mutexlock_t lock;   // 锁
    bool dirty;         // 脏位，数据是否与磁盘不一致
    bool valid;         // 有效位，数据是否有效
} buffer_t;

// 读取设备号 dev_id 的第 block 块数据到高速缓存
buffer_t *bread(devid_t dev_id, size_t block);

// 将高速缓存 bf 的数据写回设备对应区域
void bwrite(buffer_t *bf);

// 释放高速缓存 bf
void brelse(buffer_t *bf);

#endif