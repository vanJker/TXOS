#include <xos/assert.h>
#include <xos/types.h>
#include <xos/printk.h>
#include <xos/stdarg.h>
#include <xos/stdio.h>

static u8 buf[1024];

static void spin(char *info) {
    printk("spinning in %s ...\n", info);
    while (true)
        ;
}

void assertion_failure(char *exp, char *file, char *base, int line) {
    printk(
        "\n--> assert(%s) failed!!!\n"
        "--> file: %s \n"
        "--> base: %s \n"
        "--> line: %d \n",
        exp, file, base, line);
    
    spin("assertion_failure()");

    // 不可能运行到这里，否则出错
    asm volatile("ud2");
}

void panic(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int i = vsprintf(buf, fmt, args);
    va_end(args);

    printk("!!! panic !!!\n--> %s \n", buf);
    spin("panic()");

    // 不可能运行到这里，否则出错
    asm volatile("ud2");
}