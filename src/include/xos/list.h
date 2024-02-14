#ifndef XOS_LIST_H
#define XOS_LIST_H

#include <xos/types.h>

#define element_offset(type, member) (u32)(&((type *)0)->member)
#define element_entry(type, member, ptr) (type *)((u32)ptr - element_offset(type, member))

// 计算结构体中 list node 和 key 字段的偏移量
#define list_node_offset(type, node, key) (element_offset(type, key) - element_offset(type, node))

// 根据 list_node_offset 获得的 key 字段偏移量以及 list node 的地址计算 key 字段的值
#define list_node_key(node, offset) *(int *)((void *)node + offset)

// 节点没有位于任一链表中
#define ASSERT_NODE_FREE(node) assert(((node)->prev == NULL) && ((node)->next == NULL))

// 链表节点
typedef struct list_node_t {
    struct list_node_t *prev; // 前驱节点
    struct list_node_t *next; // 后继节点
} list_node_t;

// 链表
typedef struct list_t {
    list_node_t head; // 头节点
    list_node_t tail; // 尾节点
} list_t;

// 初始化链表
void list_init(list_t *list);

// 在 anchor 节点前插入节点 node
void list_insert_before(list_node_t *anchor, list_node_t *node);

// 在 anchor 节点后插入节点 node
void list_insert_after(list_node_t *anchor, list_node_t *node);

// 在链表中删除节点 node
void list_remove(list_node_t *node);

// 插入到头节点后
void list_push_front(list_t *list, list_node_t *node);

// 删除头节点后的节点
list_node_t *list_pop_front(list_t *list);

// 插入到尾节点前
void list_push_back(list_t *list, list_node_t *node);

// 删除尾节点前的节点
list_node_t *list_pop_back(list_t *list);

// 链表的长度
size_t list_size(list_t *list);

// 查找链表中是否存在节点 node
bool list_contains(list_t *list, list_node_t *node);

// 判断链表是否为空
bool list_empty(list_t *list);

// 判断链表是否只有一个有效节点
bool list_singular(list_t *list);

// 判断节点是否为链表的有效尾节点
bool list_istail(list_t *list, list_node_t *node);

// 判断节点是否为链表的有效头节点
bool list_ishead(list_t *list, list_node_t *node);

// 链表插入排序
void list_insert_sort(list_t *list, list_node_t *node, int offset);

#endif