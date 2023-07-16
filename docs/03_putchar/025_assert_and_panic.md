# 025 断言 assert 和 告警 panic

## 1. 断言 `assert`

用于确定程序的运行状态，防止错误蔓延！！！

并且提供尽可能多的出错信息，以供排错。

## 2. 告警 `painc`

用于警告用户操作系统内核处于严重错误当中。

一般是由于用户不当操作，才会使内核陷入 panic 中。

## 3. 代码分析

### 3.1 声明 `assert` 和 `panic`

```c
/* include/xos/assert.h */

#ifndef XOS_ASSERT_H
#define XOS_ASSERT_H

void assertion_failure(char *exp, char *file, char *base, int line);

#define assert(exp) \
    if (exp)        \
        ;           \
    else            \
        assertion_failure(#exp, __FILE__, __BASE_FILE__, __LINE__); \

void panic(const char *fmt, ...);

#endif
```

在 `assert` 宏中，

- `\` 与 `Linux` 环境中语义相同，表示不换行拼接。
- `#exp` 表示将 `exp` 转换成字符串字面量。即 `3 < 5` -> `"3 < 5"`。
- `__FILE__` 为预定义的宏，表示当前所在的文件名。
- `__BASIC_FILE__` 为预定义的宏，表示所在的当前文件名？。
- `__LINE__` 为预定义的宏，表示当前所在的代码行数。

之所以要将 `assert` 用一个宏进行封装，是因为我们需要之前打印出 `assert` 语句所在的文件和行数。

### 3.2 `spin`

当陷入 `assert` 或 `painc` 时，需要进行强制阻塞。

`spin` 函数通过无限循环，将内核陷入阻塞状态。

```c
/* kernel/assert.c */

static void spin(char *info) {
    printk("spinning in %s ...\n", info);
    while (true)
        ;
}
```

### 3.3 `assertion_failure`

陷入 `assert` 时，需要打印一些信息，再陷入阻塞态。

`ud2` 是 `x86` 汇编中表示出错的一个指令。

```c
/* kernel/assert.c */

void assertion_failure(char *exp, char *file, char *base, int line) {
    printk(
        "\n--> assert(%s) failed!!!\n"
        "--> file: %s \n"
        "--> base: %s \n"
        "--> line: %d \n",
        exp, file, base, line);
    
    spin("assertion_failure()");

    // 不可能运行到这里，否则出错
    asm volatile("ud2");
}
```

### 3.4 `panic`

`panic` 与上述的 `assertion_failure` 类似。

```c
/* kernel/assert.c */

static u8 buf[1024];

void panic(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int i = vsprintf(buf, fmt, args);
    va_end(args);

    printk("!!! panic !!!\n--> %s \n", buf);
    spin("panic()");

    // 不可能运行到这里，否则出错
    asm volatile("ud2");
}
```

## 4. 测试

### 4.1 测试 `assert`

```c
/* kernel/main.c */

#include <xos/xos.h>
#include <xos/types.h>
#include <xos/io.h>
#include <xos/string.h>
#include <xos/console.h>
#include <xos/printk.h>
#include <xos/assert.h>

char buf[1024];

void kernel_init() {
    console_init();
    assert(3 < 5);
    assert(3 > 5);
   return;
}
```

预期为，在 `assert(3 > 5)` 处陷入 assert 状态。

### 4.2 测试 `panic`

```c
/* kernel/main.c */
...
#include <xos/assert.h>

char buf[1024];

void kernel_init() {
    console_init();
    panic("Out of Memory");
    return;
}
```

预期为，在 `panic("Out of Memory")` 处陷入 panic 状态。

## 5. 使用 assert 进行检查

在 `vsprintf.c` 中使用 `assert`，以保证写入的字符数不会超过缓冲区的大小。

```c
/* lib/vsprintf.c */

int vsprintf(char *buf, const char *fmt, va_list args) {
    ...
    // 返回转换好的字符串长度
    i = str - buf;
    assert(i < 1024);
    return i;
}
```

## 6. 参考文献

- <https://en.cppreference.com/w/cpp/error/assert>