#include <xos/interrupt.h>
#include <xos/syscall.h>
#include <xos/debug.h>
#include <xos/mutex.h>
#include <xos/printk.h>
#include <xos/task.h>
#include <xos/stdio.h>
#include <xos/arena.h>
#include <xos/stdlib.h>

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

#define UBMB asm volatile("xchgw %bx, %bx");

// 初始化任务 init 的用户态线程
static void user_init_thread() {
    while (true) {
        sleep(1000);
    }
}

// 初始化任务 init
void init_thread() {
    task_to_user_mode((target_t)user_init_thread);
}

// 测试任务 test
void test_thread() {
    irq_enable();
    // u32 counter = 0;

    mode_t mode = umask(0002);
    while (true) {
        // printf("test task %d...\n", counter++);
        // printf("test thread pid: %d, ppid: %d, counter: %d\n", getpid(), getppid(), counter++);
        // test();
        sleep(20);
    }
}