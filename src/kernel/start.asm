[bits 32]

header_magic    equ 0xe85250d6
header_arch     equ 0
header_length   equ (multiboot2_header_end - multiboot2_header_start)
header_checksum equ -(header_magic + header_arch + header_length)

section .multiboot2
multiboot2_header_start:
    dd header_magic     ; multiboot2 魔数
    dd header_arch      ; multiboot2 引导架构，0 表示 32 位保护模式的 i386
    dd header_length    ; multiboot2 header 的长度
    dd header_checksum  ; multiboot2 校验和

    ; multiboot2 header 结束标记
    dw 0    ; type  (16 bit)
    dw 0    ; flags (16 bit)
    dd 8    ; size  (32 bit)
multiboot2_header_end:


section .text

extern kernel_init

global _start
_start:
    mov [magic], eax ; magic
    mov [addr],  ebx ; ards_count

    call kernel_init

    jmp $ ; 阻塞


section .data
; 内核魔数
global magic
magic:
    dd 0
; 地址描述符地址
global addr
addr:
    dd 0