# 064 printf

本节来实现用户态的打印函数：`printf`，它和之前实现的 `printk` 的区别，仅在于 `printk` 是内核态的函数（所以在用户线程使用 `printk` 会触发 GP 异常）。

因为 `printf` 是用户态的功能函数，而管理显示区域的输入输出是需要内核特权级的（比如 I/O 指令，禁止/使能中断指令），所以需要一个系统调用，来触发中断进入内核态来输出所指定的内容。

## 1. 系统调用 write

我们使用系统调用 `write` 作为输出功能，它的定义如下：

```c
i32 write(fd_t fd, const void *buf, size_t len);
```

这个系统调用的功能是，将 `buf` 中的内容（长度为 `len`）输出到 `fd` 所指定的文件当中。

- 参数 `fd` 表示文件描述符，其中 0 表示标准输入，1 表示标准输出，2 表示标准错误
- 参数 `buf` 表示需要输出数据的缓冲区
- 参数 `len` 表示需要输出数据的长度（单位为字节）
- 返回值是实际输出数据的长度（单位为字节）

---

**文件描述符（File Descriptor）** 是操作系统中用于标识和访问打开文件或输入 / 输出设备的抽象概念。它是一个非负整数，用于 **唯一标识** 每个打开的文件或设备。

一般来说，文件描述符遵守以下约定：

- 标准输入（stdin）的文件描述符通常是 0。
- 标准输出（stdout）的文件描述符通常是 1。
- 标准错误输出（stderr）的文件描述符通常是 2。
- 其他打开的文件或设备会分配一个大于等于 3 的文件描述符。
- 打开文件 / 设备失败通常会返回一个小于 0 的值。

> 代码位于 `include/xos/types`

```c
// 文件描述符
typedef i32 fd_t;
// 系统保留文件描述符
typedef enum std_fd_t {
    STDIN,  // 标准输入 - 0
    STDOUT, // 标准输出 - 1
    STDERR, // 标准错误 - 2
} std_fd_t;
```

---

关于系统调用 `write` 的实现在后面进行说明，现在先讲解 `printf` 的实现。

## 2. printf

### 2.1 原型声明

> 代码位于 `include/xos/stdio.h`

```c
// 用户态的格式化输出函数
int printf(const char *fmt, ...);
```

### 2.2 代码分析

> 代码位于 `lib/printf.c`

```c
// 用于存放格式化后的输出字符串
static char buf[1024];

int printf(const char *fmt, ...) {
    va_list args;
    int i;

    va_start(args, fmt);

    i = vsprintf(buf, fmt, args);

    va_end(args);

    write(STDOUT, buf, i);

    return i;
}
```

`printf` 的实现与 `printk` 类似，都是使用了可变参数，以及格式化字符串函数 `vsprintf` 来实现。唯一不同的在于 `printf` 使用的是 `write` 系统调用，来将字符串输出到标准输出（即屏幕）上的。

以下为 `printk` 的实现，可以对比查看：

> 代码位于 `kernel/printk.c`

```c
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

## 3. write 的实现

由 [<047 系统调用>](../08_syscall/047_syscall.md) 中的流程，系统调用的实现分为内核态的处理，以及用户态的接口。

![](./images/syscall_path.drawio.svg)

### 3.1 系统调用号

系统调用号是内核态处理和用户态接口均遵守的约定。给 `write` 分配一个系统调用号：

> 代码位于 `include/xos/syscall.h`

```c
// 系统调用号
typedef enum syscall_t {
    ...
    SYS_WRITE,
} syscall_t;
```

> 注：目前规定的系统调用号只是用于测试的，后续会将系统调用号进行重新分配。

### 3.2 内核态处理

系统调用 `write` 的内核态处理逻辑非常简单，如果文件描述符是标准输出或标准错误，则使用 `console_write()` 来将内容输出到屏幕上（此时是可以使用 `console_write()` 的，因为已经进入了内核态）。

如果文件描述符是其它值，目前的处理是进行 `panic`，这是因为当前并没有实现文件系统，后续在实现文件系统时，会完善这部分的处理。

> 代码位于 `kernel/syscall.c`

```c
// 系统调用 write 的处理函数
static i32 sys_write(fd_t fd, const void *buf, size_t len) {
    if (fd == STDOUT || fd == STDERR) {
        return console_write((const char *)buf, len, TEXT);
    }
    // TODO:
    panic("unimplement write!!!");
    return 0;
}
```

当然，由于返回值是实际输出数据的字节数，所以需要将 `console_write()` 进行稍加修改，以符合我们的使用需求。

```c
/* include/xos/console.h */

// 向 console 当前光标处以 attr 样式写入一个长度为 count 的字节序列
i32 console_write(char *buf, size_t count, u8 attr);


/* kernel/console.c */

i32 console_write(char *buf, size_t count, u8 attr) {
    i32 ret = count;
    ...
    return ret;
}
```

最后在系统调用处理表 `syscall_table` 中注册一下处理函数 `sys_write()`。

```c
// 初始化系统调用
void syscall_init() {
    ...
    syscall_table[SYS_WRITE] = sys_write;
}
```

### 3.3 用户态接口

在用户空间声明系统调用 `write` 的接口原型：

> 代码位于 `include/xos/syscall.h`

```c
/***** 声明用户态封装后的系统调用原型 *****/
...
i32 write(fd_t fd, char *buf, size_t len);
```

在用户空间实现系统调用 `write` 的用户态逻辑，即传入参数和进行中断触发：

> 代码位于 `lib/syscall.c`

```c
i32 write(fd_t fd, const char *buf, size_t len) {
    return _syscall3(SYS_WRITE, fd, (u32)buf, len);
}
```

其中使用到的辅助函数定义如下：

```c
// _syscall2 表示封装有 2 个参数的系统调用
static _inline u32 _syscall2(u32 sys_num, u32 arg1, u32 arg2) {
    u32 ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)
        : "a"(sys_num), "b"(arg1), "c"(arg2)
    );
    return ret;
}

// _syscall3 表示封装有 3 个参数的系统调用
static _inline u32 _syscall3(u32 sys_num, u32 arg1, u32 arg2, u32 arg3) {
    u32 ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)
        : "a"(sys_num), "b"(arg1), "c"(arg2), "d"(arg3)
    );
    return ret;
}
```

## 4. 功能测试

搭建测试框架，将用户线程使用 `printf` 来打印一些信息到屏幕。

```c
static void real_init_thread() {
    size_t counter = 0;

    while (true) {
        sleep(100);
        printf("task in user mode can use printf! %d\n", counter++);
    }
}
```

---

预期为，在屏幕正常打印出指定信息，没有触发 GP 异常。

```bash
task in user mode can use printf! 0
task in user mode can use printf! 1
task in user mode can use printf! 2
...
```
