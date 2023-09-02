#include <xos/list.h>
#include <xos/assert.h>

// 初始化链表
void list_init(list_t *list) {
    list->head.prev = NULL;
    list->head.next = &list->tail;
    list->tail.prev = &list->head;
    list->tail.next = NULL;
}

// 在 anchor 节点前插入节点 node
void list_insert_before(list_node_t *anchor, list_node_t *node) {
    node->prev = anchor->prev;
    node->next = anchor;

    anchor->prev->next = node;
    anchor->prev = node;
}

// 在 anchor 节点后插入节点 node
void list_insert_after(list_node_t *anchor, list_node_t *node) {
    node->prev = anchor;
    node->next = anchor->next;

    anchor->next->prev = node;
    anchor->next = node;
}

// 在链表中删除节点 node
void list_remove(list_node_t *node) {
    assert(node->prev != NULL);
    assert(node->next != NULL);

    node->prev->next = node->next;
    node->next->prev = node->prev;

    node->prev = NULL;
    node->next = NULL;
}

// 插入到头节点后
void list_push_front(list_t *list, list_node_t *node) {
    assert(!list_contains(list, node));
    list_insert_after(&list->head, node);
}

// 删除头节点后的节点
list_node_t *list_pop_front(list_t *list) {
    assert(!list_empty(list));

    list_node_t *node = list->head.next;
    list_remove(node);

    return node;
}

// 插入到尾节点前
void list_push_back(list_t *list, list_node_t *node) {
    assert(!list_contains(list, node));
    list_insert_before(&list->tail, node);
}

// 删除尾节点前的节点
list_node_t *list_pop_back(list_t *list) {
    assert(!list_empty(list));

    list_node_t *node = list->tail.prev;
    list_remove(node);

    return node;
}

// 链表的长度
size_t list_size(list_t *list) {
    size_t size = 0;
    list_node_t *next = list->head.next;
    while (next != &list->tail) {
        size++;
        next = next->next;
    }
    return size;
}

// 查找链表中是否存在节点 node
bool list_contains(list_t *list, list_node_t *node) {
    list_node_t *next = list->head.next;
    while (next != &list->tail) {
        if (next == node) {
            return true;
        }
        next = next->next;
    }
    return false;
}

// 判断链表是否为空
bool list_empty(list_t *list) {
    return (list->head.next == &list->tail && list->tail.prev == &list->head);
}
