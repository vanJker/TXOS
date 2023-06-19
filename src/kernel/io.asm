[bits 32]

section .text ; 代码段

global inb ; 声明 inb 为全局变量
inb:
    ; 保存栈帧
    push ebp
    mov ebp, esp

    xor eax, eax       ; 清空 eax
    mov edx, [ebp + 8] ; 参数 port
    in al, dx          ; 将端口号为 dx 的外设的寄存器的值输入到 al

    jmp $+2 ; 一些延迟
    jmp $+2 ; 一些延迟
    jmp $+2 ; 一些延迟

    leave ; 恢复栈帧
    ret

global outb ; 声明 outb 为全局变量
outb:
    ; 保存栈帧
    push ebp
    mov ebp, esp

    mov edx, [ebp + 8] ; 参数 port
    mov eax, [ebp + 12]; 参数 value
    out dx, al         ; 将 al 的值输出到端口号为 dx 的外设的寄存器

    jmp $+2 ; 一些延迟
    jmp $+2 ; 一些延迟
    jmp $+2 ; 一些延迟

    leave ; 恢复栈帧
    ret

global inw ; 声明 inw 为全局变量
inw:
    ; 保存栈帧
    push ebp
    mov ebp, esp

    xor eax, eax       ; 清空 eax
    mov edx, [ebp + 8] ; 参数 port
    in ax, dx          ; 将端口号为 dx 的外设的寄存器的值输入到 ax

    jmp $+2 ; 一些延迟
    jmp $+2 ; 一些延迟
    jmp $+2 ; 一些延迟

    leave ; 恢复栈帧
    ret

global outw ; 声明 outw 为全局变量
outw:
    ; 保存栈帧
    push ebp
    mov ebp, esp

    mov edx, [ebp + 8] ; 参数 port
    mov eax, [ebp + 12]; 参数 value
    out dx, ax         ; 将 ax 的值输出到端口号为 dx 的外设的寄存器

    jmp $+2 ; 一些延迟
    jmp $+2 ; 一些延迟
    jmp $+2 ; 一些延迟

    leave ; 恢复栈帧
    ret