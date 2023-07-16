# 010 进入内核

## 1. 重构工程目录

```bash
|__ docs/               # 文档
|__ src/                # 源代码
    |__ bootloader/     # bootloader 源代码
    |__ kernel/         # kernel 源代码
|__ target/             # 目标文件
    |__ bootloader/     # bootloader 目标文件
    |__ kernel/         # kernel 目标文件
    |__ master.img      # 磁盘文件
|__ tests/              # 测试文件
|__ .gitignore
|__ bochsrc
|__ bx_enh_dbg.ini
|__ Makefile
```

## 2. 编写内核的第一条指令

```x86asm
[bits 32]

global _start
_start:
    mov byte [0xb8000], 'K' ; 表示进入了内核
    xchg bx, bx ; 断点
    jmp $ ; 阻塞
```

## 3. loader 加载并进入内核

保护模式下的 `read_disk`，和实模式下的相同，不同的只是汇编出来的指令编码。

```x86asm
[bits 32]
read_disk:
    ; 设置读取扇区的数量
    mov dx, 0x1f2   ; 0x1f2 端口
    mov al, bl
    out dx, al

    ; 设置起始扇区的编号
    inc dx          ; 0x1f3 端口
    mov al, cl      ; 起始扇区编号的 0-7 位
    out dx, al

    inc dx          ; 0x1f4 端口
    shr ecx, 8      ; 寄存器 ecx 逻辑右移 8 位
    mov al, cl      ; 起始扇区编号的 8-15 位
    out dx, al

    inc dx          ; 0x1f5 端口
    shr ecx, 8      ; 寄存器 ecx 逻辑右移 8 位
    mov al, cl      ; 起始扇区编号的 16-23 位
    out dx, al

    inc dx          ; 0x1f6 端口
    shr ecx, 8      ; 寄存器 ecx 逻辑右移 8 位
    and cl, 0x0f    ; 将寄存器 cl 的高 4 位置零

    mov al, 0xe0    ; 将 0x1f6 端口 5-7 位置 1，即主盘 - LBA 模式
    or al, cl
    out dx, al

    inc dx          ; 0x1f7 端口
    mov al, 0x20    ; 设置为读硬盘操作
    out dx, al

    xor ecx, ecx    ; 将寄存器 ecx 清空
    mov cl, bl      ; 将 cl 置为读取扇区的数量，配合后续的 loop 指令使用

    .read:
        push cx     ; 保存 cx（因为 .reads 中修改了 cx 的值）
        call .waitr; 等待硬盘数据准备完毕
        call .reads
        pop cx      ; 恢复 cx
        loop .read

    ret

    .waitr:
        mov dx, 0x1f7   ; 0x1f7 端口
        .check:
            in al, dx
            jmp $+2         ; 直接跳转到下一条指令，相当于 nop，其作用为提供延迟
            jmp $+2
            jmp $+2
            and al, 0x08    ; 保留 0x1f7 端口的第 3 位
            cmp al, 0x08    ; 判断数据是否准备完毕
            jnz .check
        ret
    
    .reads:
        mov dx, 0x1f0   ; 0x1f0 端口（为 16 bit 的寄存器）
        mov cx, 256     ; 一个扇区一般是 512 字节（即 256 字）
        .readword:
            in ax, dx
            jmp $+2         ; 提供延迟
            jmp $+2
            jmp $+2
            mov [edi], ax   ; 将读取数据写入到指定的内存地址处
            add edi, 2
            loop .readword
        ret
```

本项目约定内核的入口地址为 `0x10000`，所以跳转到该地址执行内核指令。通过在 Makefile 中指定 ld 命令的参数 `-Ttext 0x10000`，可以生成起始地址为 `0x10000` 的内核目标文件。

```x86asm
protected_mode:
    ...
    ; 读取硬盘的内容到指定的内存地址处
    mov edi, 0x10000     ; 读取硬盘到的目标内存地址
    mov ecx, 10          ; 起始扇区的编号
    mov bl,  200         ; 读取的扇区数量

    call read_disk

    jmp dword code_selector:0x10000 ; 进入内核

    ud2 ; 表示出错
```

预期结果为屏幕的第一个字符被修改为 ‘K’，表示成功进入内核。