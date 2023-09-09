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

u32 test() {
    return _syscall0(SYS_TEST);
}

void yield() {
    _syscall0(SYS_YIELD);
}

void sleep(u32 ms) {
    _syscall1(SYS_SLEEP, ms);
}