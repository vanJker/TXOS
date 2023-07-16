#include <xos/xos.h>
#include <xos/types.h>
#include <xos/io.h>
#include <xos/string.h>
#include <xos/console.h>
#include <xos/printk.h>
#include <xos/assert.h>
#include <xos/debug.h>

char buf[1024];

void kernel_init() {
    console_init();

    BMB;

    DEBUGK("debugk xos!!!\n");

    return;
}