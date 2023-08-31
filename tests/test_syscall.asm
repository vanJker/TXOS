[bits 32]

section .text
global main
main:
    ; write(stdout, message, len)
    mov eax, 4 ; write
    mov ebx, 1 ; stdout
    mov ecx, message ; buffer
    mov edx, message_end - message ; len
    int 0x80

    ; exit(status)
    mov eax, 1 ; exit
    mov ebx, 0 ; status
    int 0x80

section .data
message:
    db "hello world!!!", 10, 13, 0 ; \n, \r, \0
message_end: