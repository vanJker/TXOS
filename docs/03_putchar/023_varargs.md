# 023 可变参数原理

## 1. 原理说明

在 C 语言中，`...` 表示支持不确定数量的参数，在下面的例子中，该不可变参数为 `format`。

```c
int printf(const char* format, ...);
```

在 C 语言中，可变参数要求函数中至少有一个固定参数。因为所有参数都会压入栈中，所以这个固定参数，可以用于确定可变参数的位置和数量，可以参考 [<019 函数调用约定>](../02_binary_basics/019_function_calling_convention.md) 来理解。

|   栈帧   |
| :-----: |
|   ...   |
| 可变参数 |
| 固定参数 |

## 2. 核心功能

- `va_list`：保存指向可变参数的指针
- `va_start`：启用 / 初始化可变参数
- `va_arg`：获取下一个可变参数
- `va_end`：结束可变参数

## 3. 代码分析

```c
typedef char *va_list;

#define va_start(ap, v) (ap = (va_list)&v + sizeof(char *))
#define va_arg(ap, t) (*(t *)((ap += sizeof(char *)) - sizeof(char *)))
#define va_end(ap) (ap = (va_list)0)
```

- `va_start` 为提供固定参数 `v` 的地址获取可变参数的起始地址
- `va_arg` 为取下一个可变参数的值，并更新可变参数指针
- `va_end` 为将可变参数指针置零

由具体实现可得，目前的实现只支持最大为 4 字节（`char *` 为 4 个字节）的参数。由于目前并没有使用更大的可变参数的需求，且 32 位机器对于函数的参数是以 4 个字节为单位分配的（即如果参数大小小于 4 个字节，栈也会分配 4 个字节来保存该参数），所以这个实现已经相当优雅了。

## 4. 调试测试

通过调试观察栈的内容，理解可变参数的使用过程，并测试其实现的正确性。

```c
/* src/kernel/main.c */
#include <xos/stdarg.h>

void test_varargs(int cnt, ...) {
    va_list args;
    va_start(args, cnt);

    int arg;
    while (cnt--) {
        arg = va_arg(args, int);
    }

    va_end(args);
}

void kernel_init() {
    ...
    test_varargs(5, 1, 0x55, 0xaa, 4, 0xffff);
    return;
}
```

上面例子中，固定参数 `cnt` 提供了可变参数的数量。但是 `printf` 中并没有这样的固定参数，它只有一个固定参数，即格式化的 `format`。但是我们可以在这个格式化的字符串中根据 `%d` 来确定可变参数的数量，并在栈中寻找参数。那么，来点黑魔法，试一试下面这个例子（doge）

```c
/* tests/varargs.c */
#include <stdio.h>

int main(int argc, char *argv[]) {
    printf("%d %d\n");
    return 0;
}
```

## 5. 参考文献

- <https://en.cppreference.com/w/cpp/header/cstdarg>