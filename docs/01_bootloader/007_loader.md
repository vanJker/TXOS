# 007 内核加载器

## 1. 原理说明

### 1.1 启动流程

`CPU Reset -> BIOS -> Boot -> Loader`

| 顺序 | 模块   | 功能              |
|------|-------|------------------|
| 1    | `CPU`   | CPU 重置寄存器的值 |
| 2    | `BIOS`  | BIOS 固定在 ROM 上，又被称为 firmware（固件），其作用是初始化基本的 I/O 功能，并加载 Boot 到 RAM 指定地址处 |
| 3    | `boot`  | 加载 loader 到 RAM 的指定地址处，并跳转到 loader 执行 |
| 4    | `loader` | 加载 kernel 到 RAM 的指定地址处，并跳转到 kernel 执行 |

有些实现上将 `boot` 和 `loader` 合并为单独一个 `bootloader`，并写入主引导扇区。但是主引导扇区容量仅为一个扇区 512 Bytes，实现一个 `bootloader` 需要极其精简，所以本项目不采取这样的实现方式。

### 1.2 x86 实模式下的内存布局

| 起始地址  | 结束地址  | 大小     | 用途               |
| --------- | --------- | -------- | ------------------ |
| `0x000`   | `0x3FF`   | 1KB      | 中断向量表         |
| `0x400`   | `0x4FF`   | 256B     | BIOS 数据区        |
| `0x500`   | `0x7BFF`  | 29.75 KB | 可用区域           |
| `0x7C00`  | `0x7DFF`  | 512B     | MBR 加载区域       |
| `0x7E00`  | `0x9FBFF` | 607.6KB  | 可用区域           |
| `0x9FC00` | `0x9FFFF` | 1KB      | 扩展 BIOS 数据区   |
| `0xA0000` | `0xAFFFF` | 64KB     | 用于彩色显示适配器 |
| `0xB0000` | `0xB7FFF` | 32KB     | 用于黑白显示适配器 |
| `0xB8000` | `0xBFFFF` | 32KB     | 用于文本显示适配器 |
| `0xC0000` | `0xC7FFF` | 32KB     | 显示适配器 BIOS    |
| `0xC8000` | `0xEFFFF` | 160KB    | 映射内存           |
| `0xF0000` | `0xFFFEF` | 64KB-16B | 系统 BIOS          |
| `0xFFFF0` | `0xFFFFF` | 16B      | 系统 BIOS 入口地址 |

从上面的内存布局可知，x86 实模式下总共有两个可用区域，分别是 `[0x500, 0x7BFF]` 和 `[0x7E00, 0x9FBFF]`。依据这两个可用空间的大小，在本项目的实现的方案是，`[0x500, 0x7BFF]` 放置 loader，`[0x7E00, 0x9FBFF]` 放置 kernel。

## 2. 代码分析

内核加载器实现流程：

- 写内核加载器 loader
- 将 loader 写入硬盘的指定扇区
- 在 boot 中将 loader 加载到 RAM 指定地址
- 检测加载 loader 的正确性
- 跳转到 loader 执行

### 2.1 写内核加载器 loader

本项目的方案为 boot 加载 loader 到的地址为 0x1000。

```x86asm
// loader.asm

[org 0x1000]

dw 0x55aa   ; 魔数，用于 boot 来检测加载 loader 是否正确

; 打印 loading 信息
mov si, loading
call print

; 程序结束，进行阻塞
jmp $


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

loading:
    db "Loading LoongOS...", 10, 13, 0  ; 在 ASCII 编码中，13:\n, 10:\r, 0:\0
```

### 2.2 将 loader 写入硬盘的指定扇区

修改 Makefile，将 loader 写入硬盘的扇区 2 开始的连续 4 个扇区。

```make
dd if=loader.bin of=master.img bs=512 count=4 seek=2 conv=notrunc
```

### 2.3 boot 加载 loader 并跳转到 loader 执行

在 boot 中将 loader 加载到 RAM 指定地址：
```x86asm
; 读取硬盘的内容到指定的内存地址处
mov edi, 0x1000     ; 读取硬盘到的目标内存地址
mov ecx, 2          ; 起始扇区的编号
mov bl,  4          ; 读取的扇区数量

call read_disk
```

检测加载 loader 的正确性：
```x86asm
cmp word [0x1000], 0x55aa
jnz error
...
error:
    mov si, .msg
    call print
    hlt     ; CPU 停机
    jmp $
    .msg db "Booting Error!", 10, 13, 0
```

跳转到 loader 执行（`[0x1000,0x1001]` 为魔术 0x55aa）：
```
jmp 0:0x1002
```

此时，可以通过查看 `0x1000` 地址处的内容，预期结果为该内容与 loader 的内容相同。