[bits 32]

section .text

extern kernel_init

global _start
_start:
    mov [kernel_magic], eax ; magic
    mov [ards_addr], ebx ; ards_count

    call kernel_init

    jmp $ ; 阻塞


section .data
; 内核魔数
global kernel_magic
kernel_magic:
    dd 0
; 地址描述符地址
global ards_addr
ards_addr:
    dd 0