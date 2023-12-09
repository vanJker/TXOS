#include <xos/syscall.h>
#include <xos/interrupt.h>
#include <xos/task.h>
#include <xos/assert.h>
#include <xos/debug.h>
#include <xos/console.h>
#include <xos/memory.h>

// 系统调用处理函数列表
handler_t syscall_table[SYSCALL_SIZE];

// 检测系统调用号是否合法
void syscall_check(u32 sys_num) {
    if (sys_num >= SYSCALL_SIZE) {
        panic("invalid syscall number!!!");
    }
}

// 默认系统调用处理函数
static void sys_default() {
    panic("syscall is not implemented!!!");
}

static task_t *task = NULL; // 当前阻塞任务

// 系统调用 test 的处理函数
static u32 sys_test() {
    // LOGK("syscall test...\n");

    u32 vaddr = 0x1600000;
    char *ptr;
    BMB;

    // ptr = (char *)vaddr;
    // ptr[3] = 'T';
    // BMB;

    link_page(vaddr);
    BMB;    

    ptr = (char *)vaddr;
    ptr[3] = 'T';
    BMB;

    unlink_page(vaddr);
    BMB;

    return 255;
}

// 系统调用 yield 的处理函数
static void sys_yield() {
    task_yield();
}

// 系统调用 sleep 的处理函数
static void sys_sleep(u32 ms) {
    task_sleep(ms);
}

// 系统调用 write 的处理函数
static i32 sys_write(fd_t fd, char *buf, size_t len) {
    if (fd == STDOUT || fd == STDERR) {
        return console_write(buf, len, TEXT);
    }
    // TODO:
    panic("unimplement write!!!");
    return 0;
}

// 初始化系统调用
void syscall_init() {
    for (size_t i = 0; i < SYSCALL_SIZE; i++) {
        syscall_table[i] = sys_default;
    }

    syscall_table[SYS_TEST]     = sys_test;
    syscall_table[SYS_SLEEP]    = sys_sleep;
    syscall_table[SYS_YIELD]    = sys_yield;
    syscall_table[SYS_WRITE]    = sys_write;
    syscall_table[SYS_BRK]      = sys_brk;
    syscall_table[SYS_GETPID]   = sys_getpid;
    syscall_table[SYS_GETPPID]  = sys_getppid;
    syscall_table[SYS_FORK]     = sys_fork;
    syscall_table[SYS_EXIT]     = sys_exit;
    syscall_table[SYS_WAITPID]  = sys_waitpid;
}
