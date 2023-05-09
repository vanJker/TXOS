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

; 0xb8000 是文本显示器的内存映射区域
mov ax, 0xb800
mov ds, ax
mov byte [0], 'H'
mov byte [2], 'e'
mov byte [4], 'l'
mov byte [6], 'l'
mov byte [8], 'o'
mov byte [10], ','
mov byte [12], ' '
mov byte [14], 'L'
mov byte [16], 'o'
mov byte [18], 'o'
mov byte [20], 'n'
mov byte [22], 'g'
mov byte [24], 'O'
mov byte [26], 'S'
mov byte [28], '!'

; 程序结束，进行阻塞
jmp $

; 填充 0
times 510 - ($ - $$) db 0

; 主引导扇区的最后两个字节必须是 0x55 和 0xaa
; x86 的内存是小段模式
dw 0xaa55