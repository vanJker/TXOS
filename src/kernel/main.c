#include <xos/xos.h>

int magic = XOS_MAGIC;
char msg[] = "Hello XOS!!!"; // .data
char buf[1024];              // .bss

void kernel_init() {
    int i;
    char *tty = (char *)0xb8000; // 文本显示器所在的内存地址
    for (i = 0; i < sizeof(msg); i++) {
        tty[i * 2] = msg[i];
        tty[i*2+1] = 0x02; // 文本显示为绿色
    }
    return;
}