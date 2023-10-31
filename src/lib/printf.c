#include <xos/stdio.h>
#include <xos/syscall.h>

// 用于存放格式化后的输出字符串
static char buf[1024];

int printf(const char *fmt, ...) {
    va_list args;
    int i;

    va_start(args, fmt);

    i = vsprintf(buf, fmt, args);

    va_end(args);

    write(STDOUT, buf, i);

    return i;
}