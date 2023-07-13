#ifndef XOS_STDIO_H
#define XOS_STDIO_H

#include <xos/stdarg.h>

// 将格式化后的字符串写入到 buf，使用可变参数指针，返回字符串长度
int vsprintf(char *buf, const char *fmt, va_list args);
// 将格式化后的字符串写入到 buf，使用可变参数，返回字符串长度
int sprintf(char *buf, const char *fmt, ...);

#endif