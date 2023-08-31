#ifndef XOS_SYSCALL_H
#define XOS_SYSCALL_H

#include <xos/types.h>

// 共 64 个系统调用
#define SYSCALL_SIZE 64

typedef enum syscall_t {
    SYS_TEST = 0,
} syscall_t;

// 检测系统调用号是否合法
void syscall_check(u32 sys_num);

// 初始化系统调用
void syscall_init();

#endif