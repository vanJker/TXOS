#ifndef XOS_DEBUG_H
#define XOS_DEBUG_H

void debugk(char *file, int line, const char *fmt, ...);

#define BMB asm volatile("xchgw %bx, %bx") // bochs magic breakpoint
#define DEBUGK(fmt, args...) debugk(__FILE__, __LINE__, fmt, ##args);

#endif