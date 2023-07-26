#include <xos/global.h>
#include <xos/string.h>
#include <xos/debug.h>

descriptor_t gdt[GDT_SIZE]; // 内核全局描述符表
pointer_t gdt_ptr;          // 内核全局描述符表指针

void gdt_init() {
    BMB; // 此时为 loader 的 GDT
    DEBUGK("init GDT!!!\n");

    asm volatile("sgdt gdt_ptr");

    memcpy((void *)&gdt, (void *)gdt_ptr.base, gdt_ptr.limit + 1);

    gdt_ptr.base = (u32)&gdt;
    gdt_ptr.limit = sizeof(gdt) - 1;

    asm volatile("lgdt gdt_ptr");
    BMB; // 此时为 kernel 的 GDT
}