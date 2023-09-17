#include <xos/interrupt.h>
#include <xos/syscall.h>
#include <xos/debug.h>
#include <xos/mutex.h>

// 空闲任务 idle
void idle_thread() {
    irq_enable();

    size_t counter = 0;
    while (true) {
        // LOGK("idle task... %d\n", counter++);
        asm volatile(
            "sti\n" // 使能外中断响应
            "hlt\n" // 暂停 CPU，等待外中断响应
        );
        yield();    // 放弃执行权，进行任务调度
    }
}

mutexlock_t lock;

// 初始化任务 init
void init_thread() {
    irq_enable();
    u32 counter = 0;

    while (true) {
        mutexlock_acquire(&lock);
        LOGK("init task %d...\n", counter++);
        mutexlock_release(&lock);
        // sleep(500);
    }
}

void test_thread() {
    irq_enable();
    u32 counter = 0;

    while (true) {
        mutexlock_acquire(&lock);
        LOGK("test task %d...\n", counter++);
        mutexlock_release(&lock);
        // sleep(800);
    }
}