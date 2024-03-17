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
    SYS_UMASK   = 60,
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

// exit() terminates the calling process "immediately".
void    exit(int status);

// fork() creates a new process by duplicating the calling process.
pid_t   fork();

// write() writes up to count bytes from the buffer starting at buf 
// to the file referred to by the file descriptor fd.
i32     write(fd_t fd, const void *buf, size_t len);

// waitpid() suspends execution of the calling thread until a child 
// specified by pid argument has changed state.
pid_t   waitpid(pid_t pid, int *status);

// time() returns the time as the number of seconds since the Epoch, 
// 1970-01-01 00:00:00 +0000 (UTC). 
time_t  time();

// getpid() returns the process ID (PID) of the calling process.
pid_t   getpid();

// brk() change the location of the program break, which defines the 
// end of the process's data segment.
i32     brk(void *addr);

// umask() sets the calling process's file mode creation mask (umask) to 
// mask & 0777 (i.e., only the file permission bits of mask are used), and 
// returns the previous value of the mask.
mode_t  umask(mode_t mask);

// getppid() returns the process ID of the parent of the calling process.
pid_t   getppid();

// yield() causes the calling thread to relinquish the CPU.
void    yield();

// sleep() suspends the execution of the calling thread until either at least 
// the time specified in `ms` has elapsed, or the delivery of a signal that 
// triggers the invocation of a handler in the calling thread or that terminates 
// the process.
void    sleep(u32 ms);

#endif