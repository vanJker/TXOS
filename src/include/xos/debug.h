#ifndef XOS_DEBUG_H
#define XOS_DEBUG_H

void debugk(char *file, int line, const char *fmt, ...);

#define DEBUGK(fmt, args...) debugk(__FILE__, __LINE__, fmt, ##args);

// Bochs Magic Breakpoint
#define BMB DEBUGK("BMB\n"); \
            asm volatile("xchgw %bx, %bx"); \

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#endif