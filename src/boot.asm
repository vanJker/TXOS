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

; bochs 魔术阻塞 magic_break
; xchg bx, bx

mov si, booting
call print

xchg bx, bx

; 读取硬盘的内容到指定的内存地址处
mov edi, 0x1000     ; 读取硬盘到的目标内存地址
mov ecx, 0          ; 起始扇区的编号
mov bl, 1           ; 读取的扇区数量

call read_disk

xchg bx, bx

; 程序结束，进行阻塞
jmp $

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
        call .waits
        call .reads
        pop cx      ; 恢复 cx
        loop .read

    ret

    .waits:
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


booting:
    db "Booting LoongOS...", 10, 13, 0  ; 在 ASCII 编码中，13:\n, 10:\r, 0:\0

; 填充 0
times 510 - ($ - $$) db 0

; 主引导扇区的最后两个字节必须是 0x55 和 0xaa
; x86 的内存是小段模式
dw 0xaa55