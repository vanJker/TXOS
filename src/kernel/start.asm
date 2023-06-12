[bits 32]

global _start
_start:
    mov byte [0xb8000], 'K' ; 表示进入了内核
    xchg bx, bx ; 断点
    jmp $ ; 阻塞