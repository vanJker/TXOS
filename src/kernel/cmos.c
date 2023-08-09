#include <xos/cmos.h>
#include <xos/io.h>

u8 cmos_read(u8 addr) {
    outb(CMOS_ADDR_PORT, CMOS_NMI | addr);
    return inb(CMOS_DATA_PORT);
}

void cmos_write(u8 addr, u8 value) {
    outb(CMOS_ADDR_PORT, CMOS_NMI | addr);
    outb(CMOS_DATA_PORT, value);
}