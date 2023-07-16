# 026 调试工具

## 1. 原理说明

本节的调试工具包含以下两种：

```c
asm volatile("xchgw %bx, %bx")
DEBUGK();
```

第一个是 Bochs 的魔术断点，第二个类似于 `printk`，但是会多输出一些额外信息。

本节的主要内容就是将这些工具进行封装，

## 2. 代码分析

### 2.1 声明 `BMB` & `DEBUGK`

```c
#ifndef XOS_DEBUG_H
#define XOS_DEBUG_H

void debugk(char *file, int line, const char *fmt, ...);

#define BMB asm volatile("xchgw %bx, %bx") // bochs magic breakpoint
#define DEBUGK(fmt, args...) debugk(__FILE__, __LINE__, fmt, ##args);

#endif
```

`DEBUGK` 宏接受一个格式字符串 fmt 和可变数量的参数 args，`args...` 表示宏接受任意数量的参数。

宏定义中的 `##args` 语法称为 "可变参数" 或 "变参" 语法，它允许宏接受任意数量的参数。在这个宏定义中，`##args` 将被展开为逗号分隔的参数列表，它们将传递给函数 `debugk` 作为可变参数列表。

同 `assert`，之所以要将 `debugk` 用一个宏进行封装，是因为我们需要之前打印出 `debugk` 语句所在的文件和行数。

### 2.2 `debugk` 分析

`debugk` 先打印所在文件名，再打印所在的代码函数，最后打印自定义的信息。

```c
/* kernel/debug.c */

static char buf[1024];

void debugk(char *file, int line, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);

    printk("[%s] [%d] %s", file, line, buf);
}
```

## 3. 测试

```c
/* kernel/main.c */
...
#include <xos/debug.h>

char buf[1024];

void kernel_init() {
    console_init();

    BMB;

    DEBUGK("debugk xos!!!\n");

    return;
}
```

- 测试 `BMB`：`BMB` 只能使用 Bochs 来测试，预期为停在 `BMB` 所在的位置。

- 测试 `DEBUGK`：使用 Qemu 可跳过 Bochs 的魔术断点，预期为打印 `DEBUGK` 所在的文件名，所在的代码行数，以及自定义的信息。


## 4. 参考文献

- <https://gcc.gnu.org/onlinedocs/cpp/Standard-Predefined-Macros.html>
- <https://gcc.gnu.org/onlinedocs/cpp/Common-Predefined-Macros.html>