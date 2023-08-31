#include <xos/syscall.h>
#include <xos/interrupt.h>
#include <xos/assert.h>
#include <xos/debug.h>

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

// 系统调用 test
static void sys_test() {
    LOGK("syscall test...\n");
}

// 初始化系统调用
void syscall_init() {
    for (size_t i = 0; i < SYSCALL_SIZE; i++) {
        syscall_table[i] = sys_default;
    }

    syscall_table[SYS_TEST] = sys_test;
}
