[bits 32]

section .text

extern kernel_init

global _start
_start:
    call kernel_init
    xchg bx, bx ; Bochs Magic Breakpoint

    ; 0x80 系统调用，会触发一般性保护异常
    ; int 0x80

    ; 除零异常
    ; mov bx, 0
    ; div bx

    jmp $ ; 阻塞