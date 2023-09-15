#ifndef XOS_MUTEX_H
#define XOS_MUTEX_H

#include <xos/types.h>
#include <xos/list.h>

// 互斥量
typedef struct mutex_t {
    bool semphore;  // 信号量，也即剩余资源数
    list_t waiters; // 等待队列
} mutex_t;

// 初始化互斥量
void mutex_init(mutex_t *mutex);

// 尝试持有互斥量
void mutex_acquire(mutex_t *mutex);

// 释放互斥量
void mutex_release(mutex_t *mutex);

#endif