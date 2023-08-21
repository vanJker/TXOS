#include <xos/stdlib.h>
#include <xos/assert.h>

void delay(u32 count) {
    while (count--)
        ;
}

void hang() {
    while (true)
        ;
}

u8 bcd_to_bin(u8 value) {
    return (value & 0xf) + (value >> 4) * 10;
}

u8 bin_to_bcd(u8 value) {
    // 需要保证这个函数接受的 value 在十进制下至多为 2 位数
    assert(value < 100);
    return (value % 10) + ((value / 10) << 4);
}

u32 div_round_up(u32 a, u32 b) {
    return (a + b - 1) / b;
}