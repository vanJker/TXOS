# 024 printk

## 1. 输出格式

格式指示串的形式如下：

> `%[flags][width][.prec][h|l|L]<type>`

其中：

- `%`：格式引入字符
- `flags`：可选的标志字符序列
- `width`：可选的宽度指示符
- `.prec`：可选的精度指示符
- `h|l|L`：可选的长度修饰符
- `type`：转换类型

## 2. flags

`flags` 控制输出对齐方式、数值符号、小数点、尾零、二进制、八进制或十六进制等，具体格式如下：

- `-`：左对齐，默认为右对齐
- `+`：输出 + 号
- ` `：如果带符号的转换不以符号开头则添加空格，如果存在 `+` 则忽略
- `#`：特殊转换：
    - 八进制，转换后字符串首位必须是 `0`
    - 十六进制，转换后必须以 `0x` 或 `0X` 开头
- `0`：使用 `0` 代替空格

## 3. 变量解释

`width` 指定了输出字符串宽度，即指定了字段的最小宽度值。如果被转换的结果要比指定的宽度小，则在其左边（或者右边，如果给出了左调整标志）需要填充空格或零（由 `flags` 标志确定）的个数等。

除了使用数值来指定宽度域以外，也可以使用 `*` 来指出字段的宽度由下一个整型参数给出。当转换值宽度大于 `width` 指定的宽度时，在任何情况下小宽度值都不会截断结果。字段宽度会扩充以包含完整结果。

---

`precision` 是说明输出数字起码的个数

- 对于 `d,i,o,u,x,X` 转换，精度值指出了起码出现数字的个数
- 对于 `e,E,f, F`，该值指出在小数点之后出现的数字的个数
- 对于 `g, G`，指出最大有效数字个数
- 对于 `s, S` 转换，精度值说明输出字符串的最大字符数

---

`qualifier` 长度修饰指示符说明了整型数转换后的输出类型形式。

- `hh` 说明后面的整型数转换对应于一个带符号字符或无符号字符参数
- `h` 说明后面的整型数转换对应于一个带符号整数或无符号短整数参数
- `l` 说明后面的整型数转换对应于一个长整数或无符号长整数参数
- `ll` 说明后面的整型数转换对应于一个长长整数或无符号长长整数参数
- `L` 说明 e,E,f,F,g 或 G 转换结果对应于一个长双精度参数

---

`type` 是说明接受的输入参数类型和输出的格式。各个转换指示符的含义如下：

- `d,i` 整数型参数将被转换为带符号整数。如果有精度(`precision`)的话，则给出了需要输出的最少数字个数。如果被转换的值数字个数较少，就会在其左边添零。默认的精度值是 `1`
- `o,u,x,X` 会将无符号的整数转换为无符号八进制(`o`)、无符号十进制(`u`)或者是无符号十六进制(`x` 或 `X`)表示方式输出。x 表示要使用小写字母 `abcdef` 来表示十六进制数，`X` 表示用大写字母 `ABCDEF` 表示十六进制数。如果存在精度域的话，说明需要输出的最少数字个数。如果被转换的值数字个数较少，就会在其左边添零。默认的精度值是 `1`
- `e,E` 这两个转换字符用于经四舍五入将参数转换成 `[-]d.ddde+dd` 的形式。小数点之后的数字个数等于精度；如果没有精度域，就取默认值 `6`。如果精度是 `0`，则没有小数出现。`E` 表示用大写字母 `E` 来表示指数。指数部分总是用 `2` 位数字表示。如果数值为 `0`，那么指数就是 `00`。
- `f,F` 这两个转换字符用于经四舍五入将参数转换成[-]ddd.ddd 的形式；小数点之后的数字个数等于精度。如果没有精度域，就取默认值 `6`；如果精度是 `0`，则没有小数出现。如果有小数点，那么后面起码会有 `1` 位数字
- `g,G` 这两个转换字符将参数转换为 `f` 或 `e` 的格式（如果是 `G`，则是 `F` 或 `E` 格式）。精度值指定了整数的个数。如果没有精度域，则其默认值为 6。如果精度为 0，则作为 1 来对待。如果转换时指数小于 -4 或大于等于精度，则采用 `e` 格式。小数部分后拖的零将被删除。仅当起码有一位小数时才会出现小数点
- `c` 参数将被转换成无符号字符并输出转换结果
- `s` 要求输入为指向字符串的指针，并且该字符串要以 `NULL` 结尾；如果有精度域，则只输出精度所要求的字符个数，并且字符串无须以 `NULL` 结尾
- `p` 以指针形式输出十六进制数
- `n` 用于把到目前为止转换输出的字符个数保存到由对应输入指针指定的位置中，不对参数进行转换
- `%` 输出一个百分号 `%`，不进行转换。也即此时整个转换指示为 `%%`

## 4. 代码分析

### 4.1 声明 `printk`

```c
/* include/xos/printk.h */
#ifndef XOS_PRINTK_H
#define XOS_PRINTK_H

int printk(const char *fmt, ...);

#endif
```

### 4.2 `stdio`

类似 `Linux` 的 `printk` 实现，我们声明 `vsprintf()` 来辅助 `printk()` 实现。此外，还声明了另一个函数 `sprintf()`，这个函数的作用类似于 `printf()`，不同的是它将格式化后的字符串写入指定地址处，而不是打印到屏幕。

```c
/* include/xos/stdio.h */
#ifndef XOS_STDIO_H
#define XOS_STDIO_H

#include <xos/stdarg.h>

// 将格式化后的字符串写入到 buf，使用可变参数指针，返回字符串长度
int vsprintf(char *buf, const char *fmt, va_list args);
// 将格式化后的字符串写入到 buf，使用可变参数，返回字符串长度
int sprintf(char *buf, const char *fmt, ...);

#endif
```

### 4.3 分析 `printk`

```c
/* kernel/printk.c */
#include <xos/printk.h>
#include <xos/stdio.h>
#include <xos/console.h>

// 用于存放格式化后的输出字符串
static char buf[1024];

int printk(const char *fmt, ...) {
    va_list args;
    int i;

    va_start(args, fmt);

    i = vsprintf(buf, fmt, args);
    
    va_end(args);

    console_write(buf, i, TEXT);

    return i;
}
```

#### 4.4 `flags` & `is_digit`

仿照 `Linux` 中的实现，先定义一些格式化相关的 `flag`，使用 2 的次方可以极其方便地使用位操作，来增加或清除 `flag`，这也是系统编程的一个技巧。宏 `is_digit()` 用于判断是否为数字字符。

```c
/* lib/vsprintf.c */

/* vsprintf.c -- Lars Wirzenius & Linus Torvalds. */
/*
 * Wirzenius wrote this portably, Torvalds fucked it up :-)
 */

#include <xos/stdio.h>
#include <xos/string.h>

#define ZEROPAD (1 << 0)    // 填充 0
#define SIGN    (1 << 1)    // unsigned/signed long
#define PLUS    (1 << 2)    // 显示 +
#define SPACE   (1 << 3)    // 如果是加，则置空格
#define LEFT    (1 << 3)    // 左对齐
#define SPECIAL (1 << 4)    // 八进制左补 0，十六进制左补 0x
#define SMALL   (1 << 5)    // 使用小写字母输出十六进制数

#define is_digit(c) ((c) >= '0' && (c) <= '9')
```

### 4.5 `skip_atoi`

```c
/* lib/vsprintf.c */

// 将数值的字符串转换成整数，并将指针前移
static int skip_atoi(const char **s) {
    int i = 0;
    while (is_digit(**s)) {
        i = i * 10 + **s - '0';
        (*s)++;
    }
    return i;
}
```

### 4.6 格式化数值 `number` 分析

```c
// 将整数转换为指定进制的字符串
// str - 输出字符串指针
// num - 整数
// base - 进制基数
// size - 字符串长度
// precision - 数字长度(精度)
// flags - 选项
static char *number(char *str, unsigned long num, int base, int size, int precision, int flags) {
    ...
}
```

建议对照源文件（`lib/vsprintf.c`）以及注释，并配合下面的讲解进行阅读。

---

格式化数字图示（**注：左对齐没法填充 0**）：

```
| 空格（填充空格） | [-/+/空格][0x/0] | 数值 |   # 右对齐，填充空格
| [-/+/空格][0x/0] | 0（填充 0） | 数值 |       # 右对齐，填充 0 
| [-/+/空格][0x/0] | 数值 | 空格（填充空格） |   # 左对齐，填充空格
```

---

进制基数：

```c
// 如果进制基数小于 2 或大于 36，则退出处理
// 即本函数只能处理基数在 2-36 之间的数
if (base < 2 || base > 36)
    return 0;
```

进制基数最大为 36，是因为 0-9 和 26 个字母的总数为 36，即可以使用数字和字母字符来表示 0-36。

---

有/无符号数输出：

```c
// 如果 flags 指出带符号数并且数值 num 小于 0，则置符号变量 sign = '-'，并将 num 取绝对值
if (flags & SIGN && num < 0) {
    sign = '-';
    num = -num;
} else 
    // 否则如果 flags 指出是加号，则置 sign = '+'，否则如果 flags 带空格标志，则置 sign = ' '，否则 sign = 0
    sign = (flags & PLUS) ? '+' : (flags & SPACE) ? ' ' : 0;
```

这里的 `else` 对应 3 种情况：

1. `flags & SIGN && num >= 0`
2. `!(flags & SIGN) && num < 0`
3. `!(flags & SIGN) && num >= 0`

`else` 对应第一种情况很好理解，对于剩余两种情况，没有符号位则代表当成无符号数进行处理，进一步可以视为非负数进行处理。

---

左对齐输出：

```c
// 如果 flags 中没有填零（ZEROPAD）和左对齐（LEFT）标志
// 则在 str 中首先填放剩余宽度指出的空格数
if (!(flags & (ZEROPAD | LEFT)))
    while (size-- > 0)
        *str++ = ' ';
...
// 如果 flags 中没有左对齐（LEFT）标志，则在剩余宽度中存放 c 字符（'0' 或空格）
if (!(flags & LEFT))
    while (size-- > 0)
        *str++ = c;
```

这下面的情况只能发生在 `ZEROPAD & !LEFT` 条件下，因为 `ZEROPAD` 和 `LEFT` 互斥，即同时最多只能有一个为 1，而这种情况又不可能是 `LEFT`。如果是 `!ZEROPAD & !LEFT` 条件，也不可能，因为在之前就把 `size` 减到 0 了，不会执行这里面的逻辑。

同时注意两个循环 `while (size-- > 0)` 均显式说明了当 `size-- > 0` 时才进行操作，这是保证，不会因为 `size` 为负数而进入无限循环之中。

### 4.7 `vsprintf` 分析

```c
int vsprintf(char *buf, const char *fmt, va_list args) {
    ...
}
```

建议对照源文件（`lib/vsprintf.c`）以及注释，并配合下面的讲解进行阅读。

---

转换格式为字符串：

```c
// 如果其超过了精度域值，则将字符串长度等于精度域值
if (precision < 0)
    precision = len;
else
    len = precision;
```

- `precision` 小于 0，表示之前并没有设置精度值，则置精度值等于字符串长度。
- `precision` 大于字符串长度，则需要对字符串输出进行截断，只输出前（精度值）个字符。

---

非格式转换符：

```c
default:
    // 如果格式指示符不是 '%'，则表示格式字符串有错
    if (*fmt != '%')
        // 直接将一个 '%' 写入到输出中
        *str++ = '%';
    // 如果格式转换符的位置处还有字符，则也直接将该字符写入输出串中
    // 然后继续循环处理格式字符串
    if (*fmt)
        *str++ = *fmt;
    else
        // 否则表示已经处理到格式字符串的结尾处，则退出循环
        --fmt;
    break;
```

这里可以用例子来说明，`printf("%%")` 输出 `%`，`printf("%z")` 输出 `%z`。所以如果遇到非格式转换符（不是 `%`），需要先将 `%` 输出，然后再输出那个非格式转换符。

这里没有对 `fmt` 进行递增，以及如果 `*fmt == \0` 对 `fmt` 进行递减，是因为循环 `for (str = buf; *fmt; ++fmt)` 的要求。

### 4.8 `sprintf` 分析

```c
// 结果按格式输出字符串到 buf，返回格式化后字符串的长度
int sprintf(char *buf, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int i = vsprintf(buf, fmt, args);
    va_end(args);
    return i;
}
```

## 5. 测试

### 5.1 测试 `printk`

```c
/* kernel/main.c */
...
#include <xos/printk.h>

void kernel_init() {
    console_init();

    int cnt = 30;
    while (cnt--) {
        printk("hello xos %#010x\n", cnt);
    }
    return;
}
```

预期为按照格式打印 30 条字符串。如果没有按预期输出，则自行调试寻找 bug 并修复。

### 5.2 测试 `sprintf`

通过调试测试 `sprintf`，观察调用 `sprintf` 后 `buf` 的内容，以及返回的字符串长度是否符合预期。

```c
/* kernel/main.c */
...
#include <xos/stdio.h>

char buf[1024];

void kernel_init() {
    console_init();
    int len = sprintf(buf, "hello xos %#010x", 1024);
   return;
}
```

## 6. 参考文献

- <https://en.cppreference.com/w/c/io/vfprintf>
- 赵炯 - 《Linux内核完全注释》