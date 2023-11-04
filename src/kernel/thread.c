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

static void user_init_thread() {
    size_t counter = 0;

    while (true) {
        // asm volatile("xchgw %bx, %bx");
        // asm volatile("in $0x92, %ax");
        sleep(100);
        // printk("task in user mode cann't use printk!\n");
        // printf("task in user mode can use printf! %d\n", counter++);
    }
}

// 初始化任务 init
void init_thread() {
    task_to_user_mode((target_t)user_init_thread);
}

void test_thread() {
    irq_enable();
    u32 counter = 0;

    void *ptr1 = kmalloc(1280);
    LOGK("kmalloc 0x%p...\n", ptr1);

    void *ptr2 = kmalloc(1024);
    LOGK("kmalloc 0x%p...\n", ptr2);

    void *ptr3 = kmalloc(54);
    LOGK("kmalloc 0x%p...\n", ptr3);

    kfree(ptr2);
    kfree(ptr1);
    kfree(ptr3);

    while (true) {
    }
}