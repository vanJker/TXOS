#ifndef XOS_MUTEX_H
#define XOS_MUTEX_H

#include <xos/types.h>
#include <xos/list.h>
#include <xos/task.h>

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


// 可重入的互斥锁
typedef struct mutexlock_t {
    task_t *holder; // 持有者
    mutex_t mutex;  // 互斥量
    size_t  repeat; // 重入次数
} mutexlock_t;

// 初始化互斥锁
void mutexlock_init(mutexlock_t *lock);

// 持有锁
void mutexlock_acquire(mutexlock_t *lock);

// 释放锁
void mutexlock_release(mutexlock_t *lock);

#endif