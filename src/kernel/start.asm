[bits 32]

extern kernel_init

section .text.entry
global _start
_start:
    xchg bx, bx
    call kernel_init
    xchg bx, bx
    jmp $ ; 阻塞