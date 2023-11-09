#include <xos/interrupt.h>
#include <xos/syscall.h>
#include <xos/debug.h>
#include <xos/mutex.h>
#include <xos/printk.h>
#include <xos/task.h>
#include <xos/stdio.h>
#include <xos/arena.h>

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

void test_recursion() {
    char temp[0x1000]; // 每次占用 4096 Bytes
    test_recursion();
}

static void user_init_thread() {
    size_t counter = 0;

    while (true) {
        printf("task in user mode can use printf! %d\n", counter++);
        test_recursion();
        sleep(1000);
    }
}

// 初始化任务 init
void init_thread() {
    task_to_user_mode((target_t)user_init_thread);
}

void test_thread() {
    irq_enable();
    u32 counter = 0;

    while (true) {
        printf("test task %d...\n", counter++);
        sleep(2000);
    }
}