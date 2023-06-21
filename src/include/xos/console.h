#ifndef XOS_CONSOLE_H
#define XOS_CONSOLE_H

#include <xos/types.h>

#define CRT_ADDR_PORT 0x3d4  // CRT(6845)索引端口
#define CRT_DATA_PORT 0x3d5  // CRT(6845)数据端口

#define CRT_CURSOR_H 0xe     // 光标位置高 8 位的索引
#define CRT_CURSOR_L 0xf     // 光标位置低 8 位的索引
#define CRT_START_ADDR_H 0xc // 屏幕显示内存起始位置的高 8 位的索引
#define CRT_START_ADDR_L 0xd // 屏幕显示内存起始位置的低 8 位的索引

#define CGA_MEM_BASE 0xb8000 // 显示内存的起始位置
#define CGA_MEM_SIZE 0x4000  // 显示内存的大小
#define CGA_END_BASE (CGA_MEM_BASE + CGA_MEM_SIZE) // 显示内存结束位置

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

void console_init();  // 初始化 console
void console_clear(); // 清空 console
void console_write(u8 *buf, u32 count); // 向 console 当前光标处写入一个字节序列

#endif