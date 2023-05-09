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
xchg bx, bx

mov si, booting
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

booting:
    db "Booting LoongOS...", 10, 13, 0  ; 在 ASCII 编码中，13:\n, 10:\r, 0:\0

; 填充 0
times 510 - ($ - $$) db 0

; 主引导扇区的最后两个字节必须是 0x55 和 0xaa
; x86 的内存是小段模式
dw 0xaa55