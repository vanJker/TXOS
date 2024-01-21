#include <xos/syscall.h>

// _syscall0 表示封装有 0 个参数的系统调用
static _inline u32 _syscall0(u32 sys_num) {
    u32 ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)
        : "a"(sys_num)
    );
    return ret;
}

// _syscall1 表示封装有 1 个参数的系统调用
static _inline u32 _syscall1(u32 sys_num, u32 arg) {
    u32 ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)
        : "a"(sys_num), "b"(arg)
    );
    return ret;
}

// _syscall2 表示封装有 2 个参数的系统调用
static _inline u32 _syscall2(u32 sys_num, u32 arg1, u32 arg2) {
    u32 ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)
        : "a"(sys_num), "b"(arg1), "c"(arg2)
    );
    return ret;
}

// _syscall3 表示封装有 3 个参数的系统调用
static _inline u32 _syscall3(u32 sys_num, u32 arg1, u32 arg2, u32 arg3) {
    u32 ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)
        : "a"(sys_num), "b"(arg1), "c"(arg2), "d"(arg3)
    );
    return ret;
}

u32 test() {
    return _syscall0(SYS_TEST);
}

void yield() {
    _syscall0(SYS_YIELD);
}

void sleep(u32 ms) {
    _syscall1(SYS_SLEEP, ms);
}

i32 write(fd_t fd, const void *buf, size_t len) {
    return _syscall3(SYS_WRITE, (u32)fd, (u32)buf, (u32)len);
}

i32 brk(void *addr) {
    return _syscall1(SYS_BRK, (u32)addr);
}

pid_t get_pid() {
    return _syscall0(SYS_GETPID);
}

pid_t get_ppid() {
    return _syscall0(SYS_GETPPID);
}

pid_t fork() {
    return _syscall0(SYS_FORK);
}

void exit(int status) {
    _syscall1(SYS_EXIT, status);
}

pid_t waitpid(pid_t pid, int *status) {
    return _syscall2(SYS_WAITPID, pid, (u32)status);
}

time_t time() {
    return _syscall0(SYS_TIME);
}
