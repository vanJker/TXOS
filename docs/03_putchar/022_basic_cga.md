# 022 基础显卡驱动

## 1. 显卡模式

- CGA (Color Graphics Adapter)
    - 图形模式
        - 160 * 100
        - 320 * 240
        - 640 * 200
    - 文本模式
        - 40 * 25
        - 80 * 25
- EGA (Enhanced Graphics Adapter)
- MCGA (Multi Color Graphics Array)

本节仍然使用之前的 80 * 25 的文本模式来实现。

## 2. CRTC (Cathode Ray Tube Controller)

CRTC 又称为“阴极射线管控制器”。

CGA 使用的是 `MC6845` 芯片；

- CRT 索引寄存器 / 端口：`0x3D4`
- CRT 数据寄存器 / 端口：`0x3D5`
- CRT 光标位置：高 8 位位于 CRT 的索引 `0xE`
- CRT 光标位置：低 8 位位于 CRT 的索引 `0xF`
- CRT 屏幕显示开始位置：高 8 位位于 CRT 的索引 `0xC`
- CRT 屏幕显示开始位置：低 8 位位于 CRT 的索引 `0xD`

这部分关于 CRT 的端口详细介绍，可以参考 [<文档 20>](./020_input_and_output.md)。

## 3. 控制字符

控制字符是指 ASCII 码表开头的 32 个字符 (0x00 ~ 0x1F) 以及 DEL(0x7F)；

通常一个指定类型的终端都会采用其中的一个子集作为控制字符，而其它的控制字符将不起作用。例如 7 为 `\a`，10 为 `\n`，13 为 `\r`。

例如，对于 VT100 终端所采用的控制字符如下表所示：

| 控制字符 | 八进制 | 十六进制 | 描述                                                                 |
| -------- | ------ | -------- | -------------------------------------------------------------------- |
| NUL      | 0      | 0x00     | 在输入时忽略，不保存在输入缓冲中                                     |
| ENQ      | 5      | 0x05     | 传送应答消息                                                         |
| BEL      | 7      | 0x07     | 从键盘发声响                                                         |
| BS       | 10     | 0x08     | 将光标移向左边一个字符位置处；若光标已经处在左边沿，则无动作         |
| HT       | 11     | 0x09     | 将光标移到下一个制表位；若右侧已经没有制表位，则移到右边缘处         |
| LF       | 12     | 0x0A     | 此代码导致一个回车或换行操作                                         |
| VT       | 13     | 0x0B     | 作用如LF                                                             |
| FF       | 14     | 0x0C     | 作用如LF                                                             |
| CR       | 15     | 0x0D     | 将光标移到当前行的左边缘处                                           |
| SO       | 16     | 0x0E     | 使用由 SCS 控制序列设计的 G1 字符集                                  |
| SI       | 17     | 0x0F     | 选择 G0 字符集，由 ESC 序列选择                                      |
| XON      | 21     | 0x11     | 使终端重新进行传输                                                   |  |
| XOFF     | 23     | 0x13     | 使中断除发送 XOFF  和 XON 以外，停止发送其它所有代码                 |
| CAN      | 30     | 0x18     | 如果在控制序列期间发送，则序列不会执行而立刻终止，同时会显示出错字符 |
| SUB      | 32     | 0x1A     | 作用同 CAN                                                           |  |
| ESC      | 33     | 0x1B     | 产生一个控制序列                                                     |  |
| DEL      | 177    | 0x7F     | 在输入时忽略 不保存在输入缓冲中                                      |

## 4. 控制序列（选修）

控制序列已经由 ANSI(American National Standards Institute 美国国家标准局)制定为标准： X3.64-1977

控制序列是指由一些非控制字符构成的一个特殊字符序列，终端在收到这个序列时并不是将它们直接显示在屏幕上，而是采取一定的控制操作，比如：

- 移动光标
- 删除字符
- 删除行
- 插入字符
- 插入行

本节并不实现控制序列，以下为控制序列的详细介绍，感兴趣者自行阅读。

ANSI 控制序列由以下一些基本元素组成：

- 控制序列引入码(Control Sequence Introducer - CSI)：表示一个转移序列，提供辅助的控制并且本身是影响随后一系列连续字符含义解释的前缀。通常，一般 CSI 都使用 `ESC[`
- 参数(Parameter)：零个或多个数字字符组成的一个数值
- 数值参数(Numeric Parameter)：表示一个数的参数，使用 `n` 表示
- 选择参数(Selective Parameter)：用于从一功能子集中选择一个子功能，一般用 `s` 表示；通常，具有多个选择参数的一个控制序列所产生的作用，如同分立的几个控制序列；例如：`CSI sa;sb;sc F` 的作用是与 `CSI sa F CSI sb F CSI sc F` 完全一样的
- 参数字符串(Parameter String)：用分号 `;` 隔开的参数字符串
- 默认值(Default)：当没有明确指定一个值或者值是 0 的话，就会指定一个与功能相关的值
- 最后字符(Final character)：用于结束一个转义或控制序列

下图是一个控制序列的例子：取消所有字符的属性，然后开启下划线和反显属性。`ESC [ 0;4;7m`

![ESC \[ 0;4;7m](./images/csi.svg)

下表是一些常用的控制序列列表，其中 E 表示 0x1B，如果 n 是 0 的话，则可以省略： `E[0j == E[J`

| 转义序列 | 功能                           |
| -------- | ------------------------------ |
| E[nA     | 光标上移 n 行                  |
| E[nB     | 光标下移 n 行                  |
| E[nC     | 光标右移 n 个字符位置          |
| E[nD     | 光标左移 n 个字符位置          |
| E[n`     | 光标移动到字符 n 位置          |
| E[na     | 光标右移 n 个字符位置          |
| E[nd     | 光标移动到行 n 上              |
| E[ne     | 光标下移 n 行                  |
| E[nF     | 光标上移 n 行，停在行开始处    |
| E[nE     | 光标下移 n 行，停在行开始处    |
| E[y;xH   | 光标移到 x,y 位置              |
| E[H      | 光标移到屏幕左上角             |
| E[y;xf   | 光标移到位置 x,y               |
| E[nZ     | 光标后移 n 制表位              |
| E[nL     | 插入 n 条空白行                |
| E[n@     | 插入 n 个空格字符              |
| E[nM     | 删除 n 行                      |
| E[nP     | 删除 n 个字符                  |
| E[nJ     | 檫除部分或全部显示字符         |
|          | n = 0 从光标处到屏幕底部；     |
|          | n = 1 从屏幕上端到光标处；     |
|          | n = 2 屏幕上所有字符           |
| E[s      | 保存光标位置                   |
| E[nK     | 删除部分或整行：               |
|          | n = 0 从光标处到行末端         |
|          | n = 1 从行开始到光标处         |
|          | n = 2 整行                     |
| E[nX     | 删除 n 个字符                  |
| E[nS     | 向上卷屏 n 行（屏幕下移）      |
| E[nT     | 向下卷屏 n 行（屏幕上移）      |
| E[nm     | 设置字符显示属性：             |
|          | n = 0 普通属性（无属性）       |
|          | n = 1 粗（bold）               |
|          | n = 4 下划线（underscore）     |
|          | n = 5 闪烁（blink）            |
|          | n = 7 反显（reverse）          |
|          | n = 3X 设置前台显示色彩        |
|          | n = 4X 设置后台显示色彩        |
|          | X = 0 黑 black X = 1 红 red    |
|          | X = 2 绿 green X = 3 棕 brown  |
|          | X = 4 蓝 blue X = 5 紫 magenta |
|          | X = 6 青 cyan X = 7 白 white   |
|          | 使用分号可以同时设置多个属性， |
|          | 例如：E[0;1;33;40m             |

## 5. 原理说明

显示内存的范围为：`[0b8000, 0xbc000)`，可以得出显示内存的大小为 16KB。本节选择的显卡驱动为 80 * 25 的文本模式，所以一行可以显示 80 个文本，而一个文本又由 2 个字节构成（第一个字节为文本的字符，第二个字节为文本的样式），所以一行有 160 个字符。可得，屏幕显示需要 160 * 25 = 4000B，这远小于显示内存的 16 KB，所以我们可以选择一部分显示内存来显示屏幕，而这一屏幕显示的起始位置记录在 CRT 的索引 `0xC` 和 `0xD` 处。

屏幕显示文本的原理为：将在屏幕显示范围的 4000B 按照格式进行显示，如果修改了屏幕显示起始位置，那么屏幕显示范围也将发生变化。滚屏操作十分简单，即以行为单位来改变屏幕显示的起始位置。类似于网页，屏幕显示范围相对于当前阅读范围，显示内存的范围相当于整个页面，滚屏就是在网页进行滚屏。本节的 `console` 相当于整个网页的控制器，需要对整个显示内存以及屏幕显示范围进行管理控制。

>**！！！注意！！！**
>
>**CRT 屏幕显示的开始位置，以及光标位置，都是相对于显示内存起始位置的偏移量，并且是以文本为单位的，即以两个字节为一个单位，转换为内存地址时需要注意。**

本节仅实现一部分的控制字符。

下图是滚屏操作的示意图，从中也可以看出显示区域和屏幕区域的关系。

滚屏操作：

![](./images/scroll.svg)

## 6. 代码分析

### 6.1 主要功能

本节主要实现以下 3 个函数：

```c
/* src/include/xos/console.h */

void console_init();  // 初始化 console
void console_clear(); // 清空 console
void console_write(char *buf, size_t count, u8 attr); // 向 console 当前光标处以 attr 样式写入一个字节序列
```

### 6.2 常量

根据原理示意图，定义一些有意义的常数：

```c
/* src/include/xos/console.h */

#define CRT_ADDR_PORT 0x3d4  // CRT(6845)索引端口
#define CRT_DATA_PORT 0x3d5  // CRT(6845)数据端口

#define CRT_CURSOR_H 0xe     // 光标位置高 8 位的索引
#define CRT_CURSOR_L 0xf     // 光标位置低 8 位的索引
#define CRT_START_ADDR_H 0xc // 屏幕显示内存起始位置的高 8 位的索引
#define CRT_START_ADDR_L 0xd // 屏幕显示内存起始位置的低 8 位的索引

#define CGA_MEM_BASE 0xb8000 // 显示内存的起始位置
#define CGA_MEM_SIZE 0x4000  // 显示内存的大小
#define CGA_MEM_END (CGA_MEM_BASE + CGA_MEM_SIZE) // 显示内存结束位置

#define SCR_WIDTH  80    // 屏幕文本列数
#define SCR_HEIGHT 25    // 屏幕文本行数
#define SCR_ROW_SIZE (CONSOLE_WIDTH * 2) // 每行字节数
#define SCR_SIZE (CONSOLE_ROW_SIZE * CONSOLE_HEIGHT) // 屏幕字节数

// 控制字符
#define ASCII_NUL 0X00 // '\0'
#define ASCII_ENQ 0x05
#define ASCII_BEL 0x07 // '\a'
#define ASCII_BS  0x08 // '\b'
#define ASCII_HT  0x09 // '\t'
#define ASCII_LF  0x0a // '\n'
#define ASCII_VT  0x0b // '\v'
#define ASCII_FF  0x0c // '\f'
#define ASCII_CR  0x0d // '\r'
#define ASCII_DEL 0x7f
```

### 6.3 显示内存管理器

设计一个管理显示内存区域的 console 结构：

```c
/**
 * console 负责管理 [0xb8000, 0xbc000) 的显示内存区域，
 * 记录屏幕位置和光标位置信息
 */
typedef struct {
    u32 screen_base; // 屏幕区域的内存基地址
    u32 cursor_addr; // 光标的内存地址
    u32 cursor_x, cursor_y; // 光标的坐标（以文本为单位）
} console_t;

static console_t console;
```

### 6.4 读取/设置 屏幕/光标的位置信息

CRT 屏幕显示的开始位置和光标位置，都是相对于显示内存起始位置的偏移量，并且是以文本为单位的，即以两个字节为一个单位，转换为内存地址时需要注意。

```c
// 获取屏幕区域的起始位置
static void get_screen_base(console_t *c) {
    outb(CRT_ADDR_PORT, CRT_START_ADDR_H); // 屏幕起始位置高位的索引
    u32 screen = inb(CRT_DATA_PORT) << 8;  // 屏幕起始位置的高 8 位
    outb(CRT_ADDR_PORT, CRT_START_ADDR_L); // 屏幕起始位置低位的索引
    screen |= inb(CRT_DATA_PORT);          // 屏幕起始位置

    // 从文本偏移量转换为内存地址
    screen <<= 1;               // 转为字节偏移量
    screen += CGA_MEM_BASE;     // 转为内存地址
    c->screen_base = screen;
}
```

设置屏幕起始位置的高 8 位的值为：`(screen >> 9) & 0xff`，这相当于 `(screen >> 1 >> 8) & 0xff`，`>> 1` 的作用为转换成文本偏移量，`>> 8` 的作用为取高 8 位，最后 `& 0xff` 的作用为只取 8 位`screen`，当然这一步可以省略。

```c
// 设置屏幕区域的起始位置
static void set_screen_base(console_t *c) {
    u32 screen = c->screen_base;
    screen -= CGA_MEM_BASE; // 转换成字节偏移量

    outb(CRT_ADDR_PORT, CRT_START_ADDR_H);        // 屏幕起始位置高位的索引
    outb(CRT_DATA_PORT, ((screen >> 9) & 0xff));  // 设置屏幕起始位置的高 8 位
    outb(CRT_ADDR_PORT, CRT_START_ADDR_L);        // 屏幕起始位置低位的索引
    outb(CRT_DATA_PORT, ((screen >> 1) && 0xff)); // 设置屏幕起始位置的低 8 位
}
```

同理，实现光标位置信息的相对应函数，其中光标的坐标，是以相对于屏幕区域的起始位置的文本偏移量来计算的。

```c
// 获取光标的位置
static void get_cursor_addr(console_t *c) {
    outb(CRT_ADDR_PORT, CRT_CURSOR_H);    // 光标位置高位的索引
    u32 cursor = inb(CRT_DATA_PORT) << 8; // 光标位置的高 8 位
    outb(CRT_ADDR_PORT, CRT_CURSOR_L);
    cursor |= inb(CRT_DATA_PORT);
    
    // 从文本偏移量转换为内存地址
    cursor <<= 1;
    cursor += CGA_MEM_BASE;
    c->cursor_addr = cursor;

    // 设置光标的坐标
    u32 delta = (cursor - c->screen_base) >> 1;
    c->cursor_x = delta % SCR_WIDTH;
    c->cursor_y = delta / SCR_WIDTH;
}

// 设置光标的位置
static void set_cursor_addr(console_t *c) {
    u32 cursor = c->cursor_addr;
    cursor -= CGA_MEM_BASE; // 转成字节偏移量

    outb(CRT_ADDR_PORT, CRT_CURSOR_H);
    outb(CRT_DATA_PORT, ((cursor >> 9) & 0xff));
    outb(CRT_ADDR_PORT, CRT_CURSOR_L);
    outb(CRT_DATA_PORT, ((cursor >> 1) && 0xff));
}
```

### 6.5 清空 console

该函数的功能为将显示内存的数据全部改为空格文本（白色前景色，黑色背景色），并将屏幕位置和光标位置设置为显示内存的起始位置。

先定义一个空白文本：

```c
/* src/include/xos/console.h */

#define ERASE 0x0720 // 空格
```

```c
// 清空 console
void console_clear() {
    // 重置屏幕位置
    console.screen_base = CGA_MEM_BASE;
    set_screen_base(&console);

    // 重置光标位置
    console.cursor_addr = CGA_MEM_BASE;
    console.cursor_x = 0;
    console.cursor_y = 0;
    set_cursor_addr(&console);

    // 清空显示内存
    for (u16 *ptr = (u16 *)CGA_MEM_BASE; ptr < (u16 *)CGA_END_BASE; ptr++) {
        *ptr = ERASE;
    }
}
```

### 6.6 向当前光标处写入一个字节序列

参考 [rCore][rcore]，对不同信息进行不同样式/颜色的输出。先对文本的样式进行定义：

```c
/* src/include/xos/console.h */

#define ERASE 0x0720 // 空格
#define TEXT  0x07 // 白色，表示文本信息输出
#define ERROR 0x04 // 红色，表示发生严重错误，很可能或者已经导致程序崩溃
#define WARN  0x0e // 黄色，表示发生不常见情况，但是并不一定导致系统错误
#define INFO  0x01 // 蓝色，比较中庸的选项，输出比较重要的信息，比较常用
#define DEBUG 0x02 // 绿色，输出信息较多，在 debug 时使用
#define TRACE 0x08 // 灰色，最详细的输出，跟踪了每一步关键路径的执行
```

初步实现 `console_write()` 的逻辑：

```c
// 向 console 当前光标处以 attr 样式写入一个字节序列
void console_write(char *buf, size_t count, u8 attr) {
    char ch;
    char *ptr = (char *)console.cursor_addr;

    while (count--) {
        ch = *buf++;
        switch (ch) {
            case ASCII_NUL:
            case ASCII_ENQ:          
            case ASCII_BEL:
            case ASCII_BS: 
            case ASCII_HT: 
            case ASCII_LF: 
            case ASCII_VT: 
            case ASCII_FF: 
            case ASCII_CR: 
            case ASCII_DEL:
                // TODO:
                break;
            default:
                *ptr++ = ch;   // 写入字符
                *ptr++ = attr; // 写入样式

                console.cursor_addr += 2;
                console.cursor_x++;
                if (console.cursor_x >= SCR_WIDTH) {
                    // TODO:
                }
                break;
        }
    }

    set_cursor_addr(&console);
}
```

可以看出，里面控制字符和换行操作并未实现。

### 6.7 分别实现控制字符的功能

控制字符的 `NUL` 即什么都不做，`BEL` 为发出蜂鸣声，这个留作后续来实现。

```c
// 光标移动到下一行的同一位置
static void command_lf(console_t *c) {
    // 超过屏幕需要进行向上滚屏
    if (c->cursor_y + 1 >= SCR_HEIGHT) {
        scroll_up(c);
    }
    c->cursor_y++;
    c->cursor_addr += SCR_ROW_SIZE;
}

// 光标移到行首
static void command_cr(console_t *c) {
    c->cursor_addr -= (c->cursor_x << 1);
    c->cursor_x = 0;
}

// 光标退格
static void command_bs(console_t *c) {
    if (c->cursor_x) {
        c->cursor_x--;
        c->cursor_addr -= 2;
        *(u16 *)c->cursor_addr = ERASE;
    }
}

// 删除光标所在位置的文本
static void command_del(console_t *c) {
    *(u16 *)c->cursor_addr = ERASE;
}
```

### 6.8 滚屏操作

上面的 `command_lf()` 的向上滚屏实现的原理为，如果没到显示内存的边界，则将屏幕区域向下移动一行（移动前需要先清空该片区域），否则回滚到显示内存的起始位置。

```c
// 向上滚屏
static void scroll_up(console_t *c) {
    // 回滚
    if (c->screen_base + SCR_SIZE + SCR_ROW_SIZE < CGA_MEM_END) {
        memcpy(CGA_MEM_BASE, c->screen_base, SCR_SIZE);
        c->cursor_addr -= (c->screen_base - CGA_MEM_BASE);
        c->screen_base = CGA_MEM_BASE;
    }

    u16 *ptr = (u16 *)(c->screen_base + SCR_SIZE);
    // 清空下一行的区域
    for (size_t i = 0; i < SCR_WIDTH; i++) {
        *ptr++ = ERASE;
    }
    // 移动屏幕和光标
    c->screen_base += SCR_ROW_SIZE;
    set_screen_base(c);
}
```

### 6.9 完成 `console_write()`

```c
// 向 console 当前光标处以 attr 样式写入一个字节序列
void console_write(char *buf, size_t count, u8 attr) {
    char ch;
    char *ptr = (char *)console.cursor_addr;

    while (count--) {
        ch = *buf++;
        switch (ch) {
            case ASCII_NUL:
                break;
            case ASCII_ENQ:
                break;
            case ASCII_BEL:
                // TODO:
                break;
            case ASCII_BS: 
                command_bs(&console);
                break;
            case ASCII_HT:
                break; 
            case ASCII_LF:
                command_lf(&console);
                command_cr(&console);
                break; 
            case ASCII_VT:
                break; 
            case ASCII_FF:
                command_lf(&console);
                break; 
            case ASCII_CR: 
                command_cr(&console);
                break;
            case ASCII_DEL:
                command_del(&console);
                break;
            default:
                *ptr++ = ch;   // 写入字符
                *ptr++ = attr; // 写入样式

                console.cursor_addr += 2;
                console.cursor_x++;

                // 到达行末进行换行
                if (console.cursor_x >= SCR_WIDTH) {
                    command_lf(&console);
                    command_cr(&console);
                }
                break;
        }
    }

    set_cursor_addr(&console);
}
```

## 7. 调试测试

每完成一个功能，应该使用调试对当前实现的功能进行测试，并修正错误，这又称为“测试驱动”。

在入口程序设置测试接口：

```c
/* src/kernel/main.c */
void kernel_init() {
    console_init();
    return;
}
```

---

### 7.1 测试 `get_screen_base()`

```c
/* src/kernel/console.c */
void console_init() {
    // console_clear();
    get_screen_base(&console);
}
```

预期为 `console.screen.base: 0xb8000`。

---

### 7.2 测试 `set_screen_base()`

```c
/* src/kernel/console.c */
void console_init() {
    // console_clear();
    console.screen_base = 80 * 2 + CGA_MEM_BASE;
    set_screen_base(&console);
}
```

预期为屏幕仅输出两行信息，原先的第一行信息被滚屏覆盖了。

---

### 7.3 测试 `get_cursor_addr()`

```c
/* src/kernel/console.c */
void console_init() {
    // console_clear();
    get_screen_base(&console); // 必须进行初始化
    get_cursor_addr(&console);
}
```

预期为 

- `console.cursor_addr: 0xb81e0 (0xb81e0 = 0xb800 + 3*80*2)`
- `console.cursor_x: 0`
- `console.cursor_y: 3`

---

### 7.4 测试 `set_cursor_addr()`

```c
/* src/kernel/console.c */
void console_init() {
    // console_clear();
    get_screen_base(&console); // 必须进行初始化
    console.cursor_addr = 124 + CGA_MEM_BASE;
    set_cursor_addr(&console);
}
```

预期为光标位于屏幕第一行的某一位置。

---

### 7.4 测试 `console_clear()`

```c
/* src/kernel/console.c */
void console_init() {
    console_clear();
}
```

预期为

- 屏幕显示为清空状态，光标位于开始处
- `console.screen_base: 0xb800`
- `console.cursor_addr: 0xb800`
- `console.cursor_x: 0`
- `console.cursor_y: 0`

---

### 7.5 测试 `console_write()` 的主要逻辑

```c
/* src/kernel/main.c */
char msg[] = "Hello, XOS!!!";
void kernel_init() {
    console_init();
    console_write(msg, strlen(msg), DEBUG);
    return;
}
```

预期为

- 屏幕显示一行绿色（`DEBUG` 级别输出）的 `Hello, XOS!!!`
- `console.cursor_x: 13`
- `console.cursor_y: 0`

---

### 7.6 测试 `console_write()`

```c
/* src/kernel/main.c */
char msg[] = "Hello, XOS!!!\n";
u8 buf[1024];

void kernel_init() {
    console_init();

    size_t count = 20;
    while (count--) {
        console_write(msg, strlen(msg), DEBUG);
    }

    return;
}
```

预期为输出 20 行 `Hello, XOS!!!`。

## 8. 参考文献

- <http://www.osdever.net/FreeVGA/home.htm>
- <http://www.osdever.net/FreeVGA/vga/crtcreg.htm>
- <https://bochs.sourceforge.io/techspec/PORTS.LST>
- <https://en.wikipedia.org/wiki/Color_Graphics_Adapter>
- <https://en.wikipedia.org/wiki/Enhanced_Graphics_Adapter>
- <https://en.wikipedia.org/wiki/Multi-Color_Graphics_Array>
- 赵炯 - 《Linux内核完全注释》
- 李忠 - x86 汇编语言：从实模式到保护模式

[rcore]: http://rcore-os.cn/rCore-Tutorial-Book-v3/chapter1/7exercise.html#log