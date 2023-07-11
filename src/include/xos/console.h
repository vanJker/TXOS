#ifndef XOS_CONSOLE_H
#define XOS_CONSOLE_H

#include <xos/types.h>

// 字符样式
#define ERASE 0x0720 // 空格
#define TEXT  0x07 // 白色，表示文本信息输出
#define ERROR 0x04 // 红色，表示发生严重错误，很可能或者已经导致程序崩溃
#define WARN  0x0e // 黄色，表示发生不常见情况，但是并不一定导致系统错误
#define INFO  0x09 // 蓝色，比较中庸的选项，输出比较重要的信息，比较常用
#define DEBUG 0x02 // 绿色，输出信息较多，在 debug 时使用
#define TRACE 0x08 // 灰色，最详细的输出，跟踪了每一步关键路径的执行

void console_init();  // 初始化 console
void console_clear(); // 清空 console
void console_write(char *buf, size_t count, u8 attr); // 向 console 当前光标处以 attr 样式写入一个字节序列

#endif