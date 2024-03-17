#ifndef XOS_TASK_H
#define XOS_TASK_H

#include <xos/types.h>
#include <xos/bitmap.h>
#include <xos/xos.h>
#include <xos/list.h>

#define KERNEL_TASK 0 // 内核任务
#define USER_TASK   1 // 用户任务

// 任务名称的长度
#define TASK_NAME_LEN 16

// 任务数量
#define NUM_TASKS 64

// 任务是否处于阻塞状态
#define ASSERT_BLOCKED_STATE(state) (((state) != TASK_RUNNING) && ((state) != TASK_READY))

typedef u32 target_t;

// 任务状态
typedef enum task_state_t {
    TASK_INIT,      // 初始化
    TASK_RUNNING,   // 运行
    TASK_READY,     // 就绪
    TASK_BLOCKED,   // 阻塞
    TASK_SLEEPING,  // 睡眠
    TASK_WAITING,   // 等待
    TASK_DIED,      // 消亡
} task_state_t;

// 任务控制块 TCB
typedef struct task_t {
    u32 *stack;                 // 内核栈
    list_node_t node;           // 任务阻塞节点
    task_state_t state;         // 任务状态
    char name[TASK_NAME_LEN];   // 任务名称
    u32 priority;               // 任务优先级
    u32 ticks;                  // 剩余时间片
    u32 jiffies;                // 上次执行时的全局时间片
    u32 uid;                    // 用户 ID
    u32 gid;                    // 用户组 ID
    pid_t pid;                  // 进程 id
    pid_t ppid;                 // 父进程 id
    u32 page_dir;               // 页目录的物理地址
    bitmap_t *vmap;             // 任务虚拟内存位图
    u32 brk;                    // 任务堆内存最高地址
    i32 status;                 // 进程结束状态
    pid_t waitpid;              // 进程等待的子进程 pid
    struct inode_t *ipwd;       // 进程当前目录对应 inode
    struct inode_t *iroot;      // 进程根目录对应 inode
    u16 umask;                  // 进程用户权限
    u32 magic;                  // 内核魔数（用于检测栈溢出）
} task_t;

// 任务上下文
typedef struct task_frame_t {
    u32 edi;
    u32 esi;
    u32 ebx;
    u32 ebp;
    void (*eip)(void); // 函数指针形式
} task_frame_t;

// 初始化任务管理
void task_init();

// 当前任务
task_t *current_task();

// 任务调度
void schedule();

// 任务主动放弃执行权
void task_yield();

// 阻塞任务
void task_block(task_t *task, list_t *blocked_list, task_state_t state);

// 结束阻塞任务
void task_unblock(task_t *task);

// 任务睡眠一段时间
void task_sleep(u32 ms);

// 唤醒任务
void task_wakeup();

// 切换到用户模式
void task_to_user_mode(target_t target);

#endif