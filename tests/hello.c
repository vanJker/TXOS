#include <stdio.h>

char msg[] = "Hello, World!\n"; // 已初始化的数据，位于 .data
char buf[1024]; // 未初始化的数据，位于 .bss

int main(int argc, char *argv[]) {
    printf(msg);
    return 0;
}