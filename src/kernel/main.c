#include <xos/xos.h>
#include <xos/types.h>
#include <xos/io.h>

#define CRT_ADDR_PORT 0x3d4
#define CRT_DATA_PORT 0x3d5

#define CRT_CURSOR_H 0xe
#define CRT_CURSOR_L 0xf

void kernel_init() {
    outb(CRT_ADDR_PORT, CRT_CURSOR_H);
    u16 pos = inb(CRT_DATA_PORT) << 8; // 光标高 8 位
    outb(CRT_ADDR_PORT, CRT_CURSOR_L);
    pos |= inb(CRT_DATA_PORT);

    outb(CRT_ADDR_PORT, CRT_CURSOR_H);
    outb(CRT_DATA_PORT, 0);
    outb(CRT_ADDR_PORT, CRT_CURSOR_L);
    outb(CRT_DATA_PORT, 123);

    return;
}