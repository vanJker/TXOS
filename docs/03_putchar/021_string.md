# 021 字符串处理

## 1. 字符串库

实现一个 C 语言字符串库的子集，方便后续内核的实现。

首先增加类型支持，将 `NULL` 改为 `((void *)0)` 是为了编译器不输出 warning。

```c
/* src/include/xos/types.h */

...
#define NULL ((void *)0) // 空指针
#define EOS '\0' // 字符串结束符
...
```

声明字符串处理函数：

```c
/* src/include/xos/string.h */

char *strcpy(char *dest, const char *src); // copies one string to another
char *strncpy(char *dest, const char *src, size_t count); // copies a certain amount of characters from one string to another
char *strcat(char* dest, const char *src); // concatenates two strings
size_t strlen(const char *str);            // returns the length of a given string
int strcmp(const char *lhs, const char *rhs); // compares two strings
char *strchr(const char *str, int ch);     // finds the first occurrence of a character
char *strrchr(const char *str, int ch);    // finds the last occurrence of a character

int memcmp(const void *lhs, const void *rhs, size_t count); // compares two buffers
void *memset(void *dest, int ch, size_t count);          // fills a buffer with a character
void *memcpy(void *dest, const void *src, size_t count); // copies one buffer to another
void *memchr(const void *ptr, int ch, size_t count);     // searches an array for the first occurrence of a character
```

在 `src/lib/string.c` 中实现字符串处理函数。

## 2. 调试测试

通过调试来测试字符串函数实现的正确性：

```c
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
```

## 3. 参考文献

- <https://en.cppreference.com/w/c/string/byte>