#include <xos/xos.h>
#include <xos/types.h>
#include <xos/io.h>
#include <xos/string.h>
#include <xos/console.h>
#include <xos/stdarg.h>

void test_varargs(int cnt, ...) {
    va_list args;
    va_start(args, cnt);

    int arg;
    while (cnt--) {
        arg = va_arg(args, int);
    }

    va_end(args);
}

void kernel_init() {
    console_init();
    test_varargs(5, 1, 0x55, 0xaa, 4, 0xffff);
    return;
}