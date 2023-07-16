#include <xos/console.h>
#include <xos/io.h>
#include <xos/string.h>

#define CRT_ADDR_PORT 0x3d4  // CRT(6845)索引端口
#define CRT_DATA_PORT 0x3d5  // CRT(6845)数据端口

#define CRT_CURSOR_H 0xe     // 光标位置高 8 位的索引
#define CRT_CURSOR_L 0xf     // 光标位置低 8 位的索引
#define CRT_START_ADDR_H 0xc // 屏幕显示内存起始位置的高 8 位的索引
#define CRT_START_ADDR_L 0xd // 屏幕显示内存起始位置的低 8 位的索引

#define CGA_MEM_BASE 0xb8000 // 显示内存的起始位置
#define CGA_MEM_SIZE 0x4000  // 显示内存的大小
#define CGA_MEM_END (CGA_MEM_BASE + CGA_MEM_SIZE) // 显示内存结束位置

#define SCR_WIDTH  80    // 屏幕文本列数
#define SCR_HEIGHT 25    // 屏幕文本行数
#define SCR_ROW_SIZE (SCR_WIDTH * 2) // 每行字节数
#define SCR_SIZE (SCR_ROW_SIZE * SCR_HEIGHT) // 屏幕字节数

// 控制字符
#define ASCII_NUL 0X00 // '\0'
#define ASCII_ENQ 0x05
#define ASCII_BEL 0x07 // '\a'
#define ASCII_BS  0x08 // '\b'
#define ASCII_HT  0x09 // '\t'
#define ASCII_LF  0x0a // '\n'
#define ASCII_VT  0x0b // '\v'
#define ASCII_FF  0x0c // '\f'
#define ASCII_CR  0x0d // '\r'
#define ASCII_DEL 0x7f

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

// 向上滚屏
static void scroll_up(console_t *c) {
    // 回滚
    if (c->screen_base + SCR_SIZE + SCR_ROW_SIZE < CGA_MEM_END) {
        memcpy((void *)CGA_MEM_BASE, (void *)c->screen_base, SCR_SIZE);
        c->cursor_addr -= (c->screen_base - CGA_MEM_BASE);
        c->screen_base = CGA_MEM_BASE;
    }

    u16 *ptr = (u16 *)(c->screen_base + SCR_SIZE);
    // 清空下一行的区域
    for (size_t i = 0; i < SCR_WIDTH; i++) {
        *ptr++ = ERASE;
    }
    // 移动屏幕和光标
    c->screen_base += SCR_ROW_SIZE;
    set_screen_base(c);
}

// 光标移动到下一行的同一位置
static void command_lf(console_t *c) {
    // 超过屏幕需要进行向上滚屏
    if (c->cursor_y + 1 >= SCR_HEIGHT) {
        scroll_up(c);
    }
    c->cursor_y++;
    c->cursor_addr += SCR_ROW_SIZE;
}

// 光标移到行首
static void command_cr(console_t *c) {
    c->cursor_addr -= (c->cursor_x << 1);
    c->cursor_x = 0;
}

// 光标退格
static void command_bs(console_t *c) {
    if (c->cursor_x) {
        c->cursor_x--;
        c->cursor_addr -= 2;
        *(u16 *)c->cursor_addr = ERASE;
    }
}

// 删除光标所在位置的文本
static void command_del(console_t *c) {
    *(u16 *)c->cursor_addr = ERASE;
}

// 向 console 当前光标处以 attr 样式写入一个字节序列
void console_write(char *buf, size_t count, u8 attr) {
    char ch;

    while (count--) {
        ch = *buf++;
        switch (ch) {
            case ASCII_NUL:
                break;
            case ASCII_ENQ:
                break;
            case ASCII_BEL:
                // TODO:
                break;
            case ASCII_BS: 
                command_bs(&console);
                break;
            case ASCII_HT:
                break; 
            case ASCII_LF:
                command_lf(&console);
                command_cr(&console);
                break; 
            case ASCII_VT:
                break; 
            case ASCII_FF:
                command_lf(&console);
                break; 
            case ASCII_CR: 
                command_cr(&console);
                break;
            case ASCII_DEL:
                command_del(&console);
                break;
            default:
                char *ptr = (char *)console.cursor_addr;
                *ptr++ = ch;   // 写入字符
                *ptr++ = attr; // 写入样式

                console.cursor_addr += 2;
                console.cursor_x++;

                // 到达行末进行换行
                if (console.cursor_x >= SCR_WIDTH) {
                    command_lf(&console);
                    command_cr(&console);
                }
                break;
        }
    }

    set_cursor_addr(&console);
}

// 清空 console
void console_clear() {
    // 重置屏幕位置
    console.screen_base = CGA_MEM_BASE;
    set_screen_base(&console);

    // 重置光标位置
    console.cursor_addr = CGA_MEM_BASE;
    console.cursor_x = 0;
    console.cursor_y = 0;
    set_cursor_addr(&console);

    // 清空显示内存
    for (u16 *ptr = (u16 *)CGA_MEM_BASE; ptr < (u16 *)CGA_MEM_END; ptr++) {
        *ptr = ERASE;
    }
}

// 初始化 console
void console_init() {
    console_clear();
}