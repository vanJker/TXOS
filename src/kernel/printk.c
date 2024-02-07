#include <xos/printk.h>
#include <xos/stdio.h>
#include <xos/console.h>

// 用于存放格式化后的输出字符串
static char buf[1024];

int printk(const char *fmt, ...) {
    va_list args;
    int i;

    va_start(args, fmt);

    i = vsprintf(buf, fmt, args);
    
    va_end(args);

    console_write(NULL, buf, i, 0, TEXT);

    return i;
}