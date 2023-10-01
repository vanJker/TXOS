# 058 键盘中断

## 1. 原理说明

键盘中断是用户在 **按下** 或 **释放** 键盘的按键时生成的中断信号，用于通知 CPU 有键盘事件发生。

键盘通过特定的端口与 CPU 连接。与 CPU 进行通信，常见的端口有：

- **键盘数据端口**：接收键盘发送的输入数据。
- **键盘状态端口**：获取键盘的状态信息。
- **键盘控制端口**：向键盘发送控制命令。

当用户按下或释放键时，键盘控制器会生成中断请求，通过系统总线将中断信号发送给中断控制器，然后传递给处理器。处理器会执行键盘中断服务程序，读取键盘数据端口获取键盘输入，并将其传递给操作系统的键盘驱动程序进行处理。

通过键盘中断，操作系统可以实时接收和响应用户的键盘输入，实现键盘输入与计算机系统的交互。

## 2. 8259a

![](../04_interrupt_and_clock/images/8259a.drawio.svg)

在 8259a 芯片中，键盘中断向量以及对应的通信端口如下：

- 0x21 键盘中断向量

| 端口 | 操作类型 | 用途       |
| ---- | -------- | ---------- |
| 0x60 | 读/写    | 数据端口   |
| 0x64 | 读       | 状态寄存器 |
| 0x64 | 写       | 控制寄存器 |

## 3. 代码分析

> 以下代码位于 `kernel/keyboard.c`

### 3.1 通信端口

定义键盘与 CPU 通信的端口，方便后续操作：

```c
#define KEYBOARD_DATA_PORT 0x60 // 键盘的数据端口
#define KEYBOARD_CTRL_PORT 0x64 // 键盘的状态/控制端口
```

### 3.2 键盘中断处理

实现键盘中断处理函数，主要逻辑为：读取键盘的数据端口，从而获取键盘输入，并将获取到的键盘输入打印出来。

从键盘的数据端口获得的数据，其实是键盘按键信息的扫描码。至于扫描码是什么，我们下一节再解释。

```c
// 键盘中断处理函数
void keyboard_handler(int vector) {
    // 键盘中断向量号
    assert(vector == IRQ_KEYBOARD + IRQ_MASTER_NR);

    // 向中断控制器发送中断处理完成的信号
    send_eoi(vector);

    // 从键盘的数据端口读取按键信息的扫描码
    u8 scan_code = inb(KEYBOARD_DATA_PORT);

    LOGK("Keyboard input 0x%x\n", scan_code);
}
```

### 3.3 注册键盘中断

最后需要将键盘中断注册到相对应的中断向量上，并使能键盘中断。

```c
// 初始化键盘中断
void keyboard_init() {
    set_interrupt_handler(IRQ_KEYBOARD, keyboard_handler);
    set_interrupt_mask(IRQ_KEYBOARD, true);
}
```

## 4. 功能测试

搭建测试框架：

> 代码位于 `kernel/main.c`

```c
void kernel_init() {
    ...
    keyboard_init();
    ...

    irq_enable(); // 打开外中断响应

    hang();
    return;
}
```

---

将 `init` 和 `test` 任务置于睡眠状态，只保留 `idle` 任务，方便测试键盘中断。

> 代码位于 `kernel/thread.c`

```c
void init_thread() {
    ...
    while (true) {
        sleep(500);
    }
}

void test_thread() {
    ...
    while (true) {
        sleep(800);
    }
}
```

---

修改 `console_write()`，通过禁止中断来防止显示区的数据竞争（当然也可以继续使用互斥锁来防止数据竞争，但是在单处理机上，禁止中断更加轻量）。

> 代码位于 `kernel/console.c`

```c
void console_write(char *buf, size_t count, u8 attr) {
    u32 irq = irq_disable();
    ...
    set_irq_state(irq);
}
```

---

预期为，每次按键时（包括按下和松开按键两个动作），会触发两次键盘中断，打印出的（按下或松开对应的）扫描码的值相差 0x80。


## 5. 参考文献

- <https://wiki.osdev.org/PS/2_Keyboard>
- <https://wiki.osdev.org/PS/2>
- <https://wiki.osdev.org/%228042%22_PS/2_Controller>