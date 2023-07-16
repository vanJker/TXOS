#include <xos/xos.h>
#include <xos/types.h>
#include <xos/io.h>
#include <xos/string.h>
#include <xos/console.h>
#include <xos/stdio.h>

char buf[1024];

void kernel_init() {
    console_init();
    int len = sprintf(buf, "hello xos %#010x", 1024);
   return;
}