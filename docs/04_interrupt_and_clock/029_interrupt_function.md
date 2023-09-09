# 029 中断函数

## 1. 简介 

### 1.1 中断分类

- 内中断
    - 软中断
        - 系统调用：例如，读写文件
    - 异常
        - 除零
        - 指令错误
        - 缺页错误
- 外中断
    - 时钟中断
    - 键盘中断
    - 硬盘中断

内中断都是和 CPU 的指令执行是同步的，而外中断都与 CPU 的指令执行是异步的。

所以，异常完成后会回到触发内中断的那条指令重新执行，而外中断相当于在两条指令之间插入了中断（或者说，触发外中断时，会先将当前指令执行完，再去执行中断函数），所以中断完成后，会直接执行下一条指令，不会重新执行指令。（软中断比较特殊，后面再讲）

### 1.2 硬盘读写

BTW，硬盘读写的主要方式有以下 3 种：

- 同步端口 IO
- 异步端口 IO
- DMA (Direct Memory Access)

其中异步端口 IO 是通过外中断来实现的。

### 1.3 内核代码构成示意图

![](./images/code_construction.svg)

## 2. 中断函数

中断函数，是在中断触发时，会被执行的函数。

与普通的函数调用不同，中断函数在调用时会在栈中压入更多的数据。

调用 / 返回时压入 / 弹出栈中的数据：

- `call / ret`
    - `eip`
- `int (interrupt) / iret (interrupt return)`
    - `eip`
    - `cs`
    - `eflags`

之所以要压入 `cs` 寄存器，是因为中断函数可能在任意地址处。而在实模式下，**线性地址 = 段地址 (cs) << 4 + 偏移地址 (ip)**。所以，必须要压入 `cs` 和 `eip` 以得到完整的线性地址，才能跳转回原先中断触发处。

压入 `eflags` 是不同类型的中断对于其种的 `IF` 位处理不同（屏蔽 / 不屏蔽中断），需要压入 `eflags`，以便在中断结束时对 `eflags` 寄存器值进行恢复。

## 3. 中断向量表（实模式）

中断向量就是中断函数的指针。

由 [<007 内核加载器>](../01_bootloader/007_loader.md) 可知中断向量表位于内存的以下区域：

> `0x000` ~ `0x3ff` 

4 个字节表示一个中断向量，其中低 2 个字节存放偏移地址 (`ip`)，高 2 个字节存放段地址 (`cs`)。总共有 256 个中断函数指针，所以总共需要 $2^2 * 2^8 = 2^{10} = 1KB$ 内存，与上面的内存区域符合。

```x86asm
int nr ; nr 是中断函数编号，范围为 0 ~ 255
```

中断使用 `invoke # 调用，引发，触发` 来描述会更准确，因为中断不一定是自动调用触发的（e.g. 外中断）。

## 4. 内存分段

16 bit 能访问的 64K 内存，需要使用分段才能访问 1MB 的内存空间。

32 bit 访问所有的 4G 内存，也就是可以不分段，但是为了向前兼容 8086 的 CPU，所以分段的概念就延续了下来。

> 奥卡姆剃刀原则！！！

这一部分说明为什么在中断时，需要将 `cs` 和 `eip` 同时压入栈中。

## 5. 实模式中断代码

### 5.1 函数调用

```x86asm
[org 0x7c00]

; 设置屏幕模式为文本模式，并清除屏幕
mov ax, 3
int 0x10

; 初始化所有的段寄存器
mov ax, 0
mov ds, ax
mov es, ax
mov ss, ax
mov sp, 0x7c00

xchg bx, bx ; bochs 魔术断点
call interrupt

; 程序结束，进行阻塞
jmp $

interrupt:
    mov si, string
    call print
    xchg bx, bx ; bochs 魔术断点
    ret

print:
    mov ah, 0x0e
.next:
    mov al, [si]
    cmp al, 0
    jz .done
    int 0x10
    inc si
    jmp .next
.done:
    ret

string:
    db ".", 0  ; 在 ASCII 编码中，0:\0

; 填充 0
times 510 - ($ - $$) db 0

; 主引导扇区的最后两个字节必须是 0x55 和 0xaa
; x86 的内存是小段模式
dw 0xaa55
```

在 Bochs 魔术断点处，通过 Bochs 查看栈的内容，以及栈的内容变化，对照原理观察不同调用 / 触发之间的异同。

### 5.2 软中断 / 系统调用

```x86asm
...
xchg bx, bx ; bochs 魔术断点

; 注册中断向量表
mov word [0x80 * 4], interrupt ; 偏移地址
mov word [0x80 * 4 + 2], 0     ; 段地址

int 0x80 ; 系统调用 0x80

; 程序结束，进行阻塞
jmp $

interrupt:
    mov si, string
    call print
    xchg bx, bx ; bochs 魔术断点
    iret
...
```

这里将 `interrupt` 注册给了编号为 0x80 的中断向量，并通过 `int 0x80` 触发 0x80 中断，从而进入 `interrupt` 函数体中执行。

同理，在断点处，对照原理，观察栈的内容变化。

### 5.3 异常（除零异常）

```x86asm
...
xchg bx, bx ; bochs 魔术断点

; 注册中断向量表
mov word [0x0 * 4], interrupt
mov word [0x0 * 4 + 2], 0

mov dx, 0
mov ax, 1
mov bx, 0

xchg bx, bx ; bochs 魔术断点

div bx ; 相当于 dx : ax / bx


; 程序结束，进行阻塞
jmp $

interrupt:
    mov si, string
    call print
    xchg bx, bx ; bochs 魔术断点
    iret
...
```

这里我们需要观察除零异常，所以这里将 `interrupt` 注册给了编号为 0x0 的中断向量（除零异常的中断向量编号为 0），并通过 `div bx` 触发除零异常（当然需要进行除法之前的一些数据安排），从而进入 `interrupt` 函数体中执行。

同理，在断点处，对照原理，观察栈的内容变化。

BTW，在观察异常时，我们可以发现异常处理（中断函数）结束后，会重新执行触发异常的那个除法指令。
