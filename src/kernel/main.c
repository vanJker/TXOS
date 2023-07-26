#include <xos/xos.h>
#include <xos/types.h>
#include <xos/io.h>
#include <xos/string.h>
#include <xos/console.h>
#include <xos/printk.h>
#include <xos/assert.h>
#include <xos/debug.h>
#include <xos/global.h>
#include <xos/task.h>
#include <xos/interrupt.h>

void kernel_init() {
    console_init();
    gdt_init();
    // task_init();
    interrupt_init();

    return;
}