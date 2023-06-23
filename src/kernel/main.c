#include <xos/xos.h>
#include <xos/types.h>
#include <xos/io.h>
#include <xos/string.h>
#include <xos/console.h>

char msg[] = "[INFO] Hello, XOS!!!\n";
u8 buf[1024];

void kernel_init() {
    console_init();

    while (true) {
        console_write(msg, strlen(msg), INFO);
    }

    return;
}