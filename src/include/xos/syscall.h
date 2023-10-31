#ifndef XOS_SYSCALL_H
#define XOS_SYSCALL_H

#include <xos/types.h>

// 共 64 个系统调用
#define SYSCALL_SIZE 64

// 系统调用号
typedef enum syscall_t {
    SYS_TEST,
    SYS_SLEEP,
    SYS_YIELD,
    SYS_WRITE,
} syscall_t;

// 检测系统调用号是否合法
void syscall_check(u32 sys_num);

// 初始化系统调用
void syscall_init();


/***** 声明用户态封装后的系统调用原型 *****/

u32 test();
void yield();
void sleep(u32 ms);
i32 write(fd_t fd, char *buf, size_t len);

#endif