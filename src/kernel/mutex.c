#include <xos/mutex.h>
#include <xos/interrupt.h>
#include <xos/task.h>
#include <xos/assert.h>

// 初始化互斥量
void mutex_init(mutex_t *mutex) {
    mutex->semphore = 1; // 初始时剩余资源数为 1
    list_init(&mutex->waiters);
}

// 尝试持有互斥量
void mutex_acquire(mutex_t *mutex) {
    // 禁止外中断响应，保证原子操作
    u32 irq = irq_disable();

    // 尝试持有互斥量
    task_t *current = current_task();
    while (mutex->semphore == 0) {
        // 如果 semphore 为 0，则表明当前无可用资源
        // 需要将当前进程加入该互斥量的等待队列
        task_block(current, &mutex->waiters, TASK_BLOCKED);
    }

    // 该互斥量当前无人持有
    assert(mutex->semphore == 1);

    // 持有该互斥量
    mutex->semphore--;
    assert(mutex->semphore == 0);

    // 恢复为之前的中断状态
    set_irq_state(irq);
}

// 释放互斥量
void mutex_release(mutex_t *mutex) {
    // 禁止外中断响应，保证原子操作
    u32 irq = irq_disable();

    // 已持有该互斥量
    assert(mutex->semphore == 0);

    // 释放该互斥量
    mutex->semphore++;
    assert(mutex->semphore == 1);

    // 如果该互斥量的等待队列不为空，则按照 “先进先出“ 的次序结束等待进程的阻塞
    if (!list_empty(&mutex->waiters)) {
        task_t *task = element_entry(task_t, node, mutex->waiters.head.next);
        assert(task->magic == XOS_MAGIC); // 检测栈溢出
        task_unblock(task);
        // 保证新进程可以获取互斥量，否则可能会被饿死
        task_yield();
    }

    // 恢复为之前的中断状态
    set_irq_state(irq);
}

// 初始化互斥锁
void mutexlock_init(mutexlock_t *lock) {
    lock->holder = NULL;
    mutex_init(&lock->mutex);
    lock->repeat = 0;
}

// 持有锁
void mutexlock_acquire(mutexlock_t *lock) {
    task_t *current = current_task();

    if (lock->holder != current) {
        // 如果锁的持有者不是当前这个任务，则该任务尝试获持有锁
        mutex_acquire(&lock->mutex);
        // 当前任务成功持有锁
        lock->holder = current;
        assert(lock->repeat == 0);
        lock->repeat = 1;
    } else {
        // 如果当前任务已经是锁的持有者，则只需更新锁的重入次数
        lock->repeat++;
    }
}

// 释放锁
void mutexlock_release(mutexlock_t *lock) {
    task_t *current = current_task();

    // 只有锁的持有者才能释放锁
    assert(current == lock->holder);

    if (lock->repeat > 1) {
        // 如果锁的重入次数大于 1，则表示已经进行了多次重入，只需更新重入次数
        lock->repeat--;
    } else {
        // 如果锁的重入次数为 1，则需要释放锁
        assert(lock->repeat == 1);
        lock->holder = NULL;
        lock->repeat = 0;
        mutex_release(&lock->mutex);
    }
}