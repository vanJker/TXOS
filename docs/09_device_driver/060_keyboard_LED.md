# 060 键盘 LED 灯

## 1. 原理说明

PS/2 键盘接受很多种类型的命令，命令是由一个字节组成的。一些命令可以附带额外的数据（数据也是以一字节为单位，但是可以多次发送字节来实现传输多字节的数据），但是这些数据必须在命令字节发送之后再发送。键盘通过一个 ACK(0xFA)（表示命令已收到）或者 Resend(0xFE)（表示前一个命令有错误）来对接收到的命令 / 数据进行反馈。在发送命令、数据以及接收反馈之间需要等待缓冲区为空。

| Command Byte | Data Byte/s | Meaning | Reponse |
| ------------ | ----------- | ------- | ------- |
| 0xED         | LED states  | Set LEDs | 0xFA (ACK) or 0xFE (Resend) |

---

**LED States:**

| 位  | 表示     |
| --- | ------- |
| 0   | 滚动锁定 |
| 1   | 数字锁定 |
| 2   | 大写锁定 |

## 2. 代码分析

> 以下代码位于 `kernel/keyboard.c`

### 2.1 缓冲区状态

根据上一节对于键盘状态寄存器的说明，我们实现等待缓冲区直到满足某一状态的功能，以满足向键盘写入 / 读取数据的要求。

**状态寄存器**（只列出低 2 位）：

| 比特位 | 功能 | 额外信息 |
| ----- | ---- | ------- |
| 0 | 输出缓冲区状态 | 0 表示缓冲区空，1 表示缓冲区满。只有该位被设置为 1 时，才能尝试从端口 0x60 读取数据 |
| 1 | 输入缓冲区状态 | 0 表示缓冲区空，1 表示缓冲区满。只有该位被设置为 0 时，才能尝试向端口 0x60 或 0x64 写入数据 |

```c
// 等待键盘的输出缓冲区为满
static void keyboard_output_wait() {
    u8 state;
    do {
        state = inb(KEYBOARD_CTRL_PORT);
    } while (!(state & 0x01));
}

// 等待键盘的输入缓冲区为空
static void keyboard_input_wait() {
    u8 state;
    do {
        state = inb(KEYBOARD_CTRL_PORT);
    } while (state & 0x02);
}
```

并在键盘中断处理当中，在读取扫描码之前，加入等待键盘输出缓冲区满的逻辑：

```c
void keyboard_handler(int vector) {
    ...
    // 从键盘的数据端口读取按键信息的扫描码
    keyboard_output_wait();
    u16 scan_code = inb(KEYBOARD_DATA_PORT);
    ...
}
```

### 2.2 Command

定义键盘的设置 LED 命令，以及对命令的应答：

```c
#define KEYBOARD_CMD_LED    0xED // 设置 LED 状态
#define KEYBOARD_CMD_ACK    0xFA // ACK 上一条命令
#define KEYBOARD_CMD_RESEND 0xFE // 重传上一条命令（一般是上一条命令发生了错误）
```

并结合之前缓冲区等待功能，实现等待键盘命令反馈：

```c
// 等待直到键盘返回对上一条命令的处理结果
static u8 keyboard_cmd_respond() {
    keyboard_output_wait();
    return inb(KEYBOARD_DATA_PORT);
}
```

### 2.3 设置 LED

实现设置键盘 LED 灯的功能：

```c
// 设置键盘的 LED 灯
static void keyboard_set_leds() {
    u8 leds = (keyboard.capslock << 2) | (keyboard.numlock << 1) | keyboard.scrlock;
    u8 state;
    
    // 设置 LED 命令
    do {
        keyboard_input_wait();
        outb(KEYBOARD_DATA_PORT, KEYBOARD_CMD_LED);
        state = keyboard_cmd_respond();
    } while (state == KEYBOARD_CMD_RESEND);
    assert(state == KEYBOARD_CMD_ACK); // 保证命令被 ACK

    // 设置 LED 状态
    do {
        keyboard_input_wait();
        outb(KEYBOARD_DATA_PORT, leds);
        state = keyboard_cmd_respond();
    } while (state == KEYBOARD_CMD_RESEND);
    keyboard_input_wait(); // 保证数据被成功输入
}
```

并在键盘招中断处理中加入设置 LED 灯的逻辑：

```c
void keyboard_handler(int vector) {
    ...
    // 是否需要设置 LED 灯
    bool led = false;
    if (make_code == KEY_NUMLOCK) {
        keyboard.numlock = !(keyboard.numlock);
        led = true;
    } else if (make_code == KEY_CAPSLOCK) {
        keyboard.capslock = !(keyboard.capslock);
        led = true;
    } else if (make_code == KEY_SCRLOCK) {
        keyboard.scrlock = !(keyboard.scrlock);
        led = true;
    }

    // 至少一个 LED 灯状态发送变化
    if (led) keyboard_set_leds();
    ...
}
```

## 3. 功能测试

测试框架与上一节相同：

```c
/* kernel/main.c */

void kernel_init() {
    console_init();
    gdt_init();
    memory_init();
    kernel_map_init();
    interrupt_init();
    clock_init();
    keyboard_init();
    task_init();
    syscall_init();

    irq_enable(); // 打开外中断响应

    hang();
    return;
}
```

启动系统后，操作键盘的锁定灯，会发生锁定键显示灯变化：

> 注：本节只能使用 Bochs 来测试，因为 Qemu 并没有模拟键盘锁定的 LED 灯。

## 4. 参考文献

- <https://wiki.osdev.org/PS/2_Keyboard>
- <https://forum.osdev.org/viewtopic.php?t=10053>
- [赵炯 / Linux内核完全注释 / 机械工业出版社 / 2005](https://book.douban.com/subject/1231236/)
