#include <xos/console.h>
#include <xos/io.h>

/**
 * console 负责管理 [0xb8000, 0xbc000) 的显示内存区域，记录屏幕位置和光标位置信息
 */
typedef struct {
    u32 screen_base;
    u32 cursor_addr;
    u32 cursor_x, cursor_y;
} console_t;

static console_t console;

// 获取屏幕区域的起始位置
static void get_screen_base(console_t *c) {
    outb(CRT_ADDR_PORT, CRT_START_ADDR_H); // 屏幕起始位置高位的索引
    u32 screen = inb(CRT_DATA_PORT) << 8;  // 屏幕起始位置的高 8 位
    outb(CRT_ADDR_PORT, CRT_START_ADDR_L);
    screen |= inb(CRT_DATA_PORT);

    // 从文本偏移量转换为内存地址
    screen <<= 1;
    screen += CGA_MEM_BASE;
    c->screen_base = screen;
}

// 设置屏幕区域的起始位置
static void set_screen_base(console_t *c) {
    u32 screen = c->screen_base;
    screen -= CGA_MEM_BASE; // 转成字节偏移量

    outb(CRT_ADDR_PORT, CRT_START_ADDR_H);
    outb(CRT_DATA_PORT, ((screen >> 9) & 0xff));
    outb(CRT_ADDR_PORT, CRT_START_ADDR_L);
    outb(CRT_DATA_PORT, ((screen >> 1) & 0xff));
}

// 获取光标的位置
static void get_cursor_addr(console_t *c) {
    outb(CRT_ADDR_PORT, CRT_CURSOR_H);    // 光标位置高位的索引
    u32 cursor = inb(CRT_DATA_PORT) << 8; // 光标位置的高 8 位
    outb(CRT_ADDR_PORT, CRT_CURSOR_L);
    cursor |= inb(CRT_DATA_PORT);
    
    // 从文本偏移量转换为内存地址
    cursor <<= 1;
    cursor += CGA_MEM_BASE;
    c->cursor_addr = cursor;

    // 设置光标的坐标
    u32 delta = (cursor - c->screen_base) >> 1;
    c->cursor_x = delta % SCR_WIDTH;
    c->cursor_y = delta / SCR_WIDTH;
}

// 设置光标的位置
static void set_cursor_addr(console_t *c) {
    u32 cursor = c->cursor_addr;
    cursor -= CGA_MEM_BASE; // 转成字节偏移量

    outb(CRT_ADDR_PORT, CRT_CURSOR_H);
    outb(CRT_DATA_PORT, ((cursor >> 9) & 0xff));
    outb(CRT_ADDR_PORT, CRT_CURSOR_L);
    outb(CRT_DATA_PORT, ((cursor >> 1) & 0xff));
}

// 向 console 当前光标处写入一个字节序列
void console_write(u8 *buf, u32 count) {
    // TODO:
}

// 清空 console
void console_clear() {
    // TODO:
}

// 初始化 console
void console_init() {
    // console_clear();
    get_screen_base(&console);
    get_cursor_addr(&console);
}