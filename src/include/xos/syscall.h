#ifndef XOS_SYSCALL_H
#define XOS_SYSCALL_H

#include <xos/types.h>

// #include <asm/unistd_32.h>

// 共 256 个系统调用
#define SYSCALL_SIZE 256

// 系统调用号
typedef enum syscall_t {
    SYS_TEST,
    SYS_EXIT    = 1,
    SYS_FORK    = 2,
    SYS_WRITE   = 4,
    SYS_WAITPID = 7,
    SYS_TIME    = 13,
    SYS_GETPID  = 20,
    SYS_BRK     = 45,
    SYS_GETPPID = 64,
    SYS_YIELD   = 158,
    SYS_SLEEP   = 162,
} syscall_t;

// 检测系统调用号是否合法
void syscall_check(u32 sys_num);

// 初始化系统调用
void syscall_init();


/***** 声明用户态封装后的系统调用原型 *****/

u32     test();
void    yield();
void    sleep(u32 ms);
i32     write(fd_t fd, char *buf, size_t len);
i32     brk(void *addr);
pid_t   get_pid();
pid_t   get_ppid();
pid_t   fork();
void    exit(int status);
pid_t   waitpid(pid_t pid, int *status);
time_t  time();

#endif