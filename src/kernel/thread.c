#include <xos/interrupt.h>
#include <xos/syscall.h>
#include <xos/debug.h>

// 空闲任务 idle
void idle_thread() {
    irq_enable();

    size_t counter = 0;
    while (true) {
        LOGK("idle task... %d\n", counter++);
        asm volatile(
            "sti\n" // 使能外中断响应
            "hlt\n" // 暂停 CPU，等待外中断响应
        );
        yield();    // 放弃执行权，进行任务调度
    }
}

// 初始化任务 init
void init_thread() {
    irq_enable();

    while (true) {
        LOGK("init task...\n");
        // test();
    }
}