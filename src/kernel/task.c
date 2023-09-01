#include <xos/task.h>
#include <xos/printk.h>
#include <xos/debug.h>
#include <xos/assert.h>
#include <xos/memory.h>
#include <xos/interrupt.h>
#include <xos/string.h>
#include <xos/syscall.h>

extern void task_switch(task_t *next);

// 任务队列
static task_t *task_queue[NUM_TASKS];

// 从任务队列获取一个空闲的任务，并分配 TCB
static task_t *get_free_task() {
    for (size_t i = 0; i < NUM_TASKS; i++) {
        if (task_queue[i] == NULL) {
            task_queue[i] = (task_t *)kalloc(1);
            return task_queue[i];
        }
    }
    return NULL;
}

// 在任务队列中查找某种状态的任务（不查找自己）
// 在符合条件的任务中，取优先级最高并且最久没运行的
// 如果没有符合条件的任务，返回 NULL
static task_t *task_search(task_state_t state) {
    // 查找过程保证中断关闭，防止查找有误
    assert(get_irq_state() == 0);

    task_t *result = NULL;
    task_t *current = current_task();

    for (size_t i = 0; i < NUM_TASKS; i++) {
        task_t *task = task_queue[i];

        if (task == NULL || task == current || task->state != state) {
            continue;
        }

        if (result == NULL || task->priority > result->priority || task->jiffies < result->jiffies) {
            result = task;
        }
    }

    return result;
}

// 当前任务
task_t *current_task() {
    // (sp - 4) 保证获取到正确的 TCB
    asm volatile(
        "movl %esp, %eax\n"
        "subl $4, %eax\n"
        "andl $0xfffff000, %eax\n"
    );
}

// 任务调度
void schedule() {
    assert(get_irq_state() == 0);

    task_t *current = current_task();       // 获取当前任务
    task_t *next = task_search(TASK_READY); // 查找一个就绪任务

    assert(next != NULL);               // 保证任务非空
    assert(next->magic == XOS_MAGIC);   // 检测栈溢出

    // 如果当前任务状态为运行，则将状态置为就绪
    if (current->state == TASK_RUNNING) {
        current->state = TASK_READY;
    }
    // 重置当前任务的剩余时间片，为下一次调度准备
    if (current->ticks == 0) {
        current->ticks = current->priority;
    }

    next->state = TASK_RUNNING;
    if (next == current) { // 如果下一个任务还是当前任务，则无需进行上下文切换
        return;
    }

    task_switch(next);
}

// 创建一个默认的任务 TCB
static task_t *task_create(target_t target, const char *name, u32 priority, u32 uid) {
    task_t *task = get_free_task();
    memset(task, 0, PAGE_SIZE); // 清空 TCB 所在的页

    u32 stack = (u32)task + PAGE_SIZE;

    stack -= sizeof(task_frame_t);
    task_frame_t *frame = (task_frame_t *)stack;
    frame->ebx = 0x11111111;
    frame->esi = 0x22222222;
    frame->edi = 0x33333333;
    frame->ebp = 0x44444444;
    frame->eip = (void *)target;

    strcpy((char *)task->name, name);
    task->stack = (u32 *)stack;
    task->state = TASK_READY;
    task->priority = priority;
    task->ticks = task->priority; // 默认剩余时间 = 优先级
    task->jiffies = 0;
    task->uid = uid;
    task->page_dir = get_kernel_page_dir();
    task->vmap = get_kernel_vmap();
    task->magic = XOS_MAGIC;

    return task;
}

// 配置开机运行至今的 setup 任务，使得其能进行任务调度
static void task_setup() {
    task_t *current = current_task();
    current->magic = XOS_MAGIC;
    current->ticks = 1;

    // 初始化任务队列
    memset(task_queue, 0, sizeof(task_queue));
}

u32 thread_a() {
    irq_enable();

    while (true) {
        printk("A");
        yield();
    }
}

u32 thread_b() {
    irq_enable();

    while (true) {
        printk("B");
        yield();
    }
}

u32 thread_c() {

    irq_enable();
    while (true) {
        printk("C");
        yield();
    }
}

// 初始化任务管理
void task_init() {
    task_setup();

    task_create((target_t)thread_a, "a", 5, KERNEL_TASK);
    task_create((target_t)thread_b, "b", 5, KERNEL_TASK);
    task_create((target_t)thread_c, "c", 5, KERNEL_TASK);
}