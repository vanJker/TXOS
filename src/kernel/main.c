#include <xos/xos.h>
#include <xos/types.h>
#include <xos/io.h>
#include <xos/string.h>

char msg[] = "Hello, XOS!!!";
u8 buf[1024];

void kernel_init() {
    int res;
    res = strcmp(buf, msg); // res == -1
    strcpy(buf, msg);
    res = strcmp(buf, msg); // res == 0
    strcat(buf, msg);
    res = strcmp(buf, msg); // res == 1

    res = strlen(msg); // res == 13
    res = sizeof(msg); // res == 14
    res = strlen(buf); // res == 26
    res = sizeof(buf); // res == 1024

    char *ptr;
    ptr = strchr(msg, '!');  // ptr, x: "!!!"
    ptr = strrchr(msg, '!'); // ptr, x: "!"

    memset(buf, 0, sizeof(buf));
    res = memcmp(buf, msg, sizeof(msg)); // res == -1
    memcpy(buf, msg, sizeof(msg));
    res = memcmp(buf, msg, sizeof(msg)); // res == 0
    ptr = memchr(buf, '!', sizeof(msg)); // ptr, x: "!!!"

    return;
}