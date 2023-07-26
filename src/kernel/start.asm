[bits 32]

section .text

extern kernel_init

global _start
_start:
    call kernel_init
    xchg bx, bx ; Bochs Magic Breakpoint
    int 0x80
    jmp $ ; 阻塞