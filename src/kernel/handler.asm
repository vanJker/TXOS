[bits 32]

; 中断处理函数入口
extern handler_table

section .text

; 用于生产中断入口函数的宏
%macro INTERRUPT_ENTRY 2
interrupt_entry_%1:
%ifn %2
    ; 如果没有压入错误码，则压入该数以保证栈结构相同
    push 0x20230730 
%endif
    push %1 ; 压入中断向量，跳转到中断入口
    jmp interrupt_entry
%endmacro

interrupt_entry:
    mov eax, [esp] ; 获取中断向量
    ; 调用中断处理函数，handler_table 中存储了中断处理函数的指针
    call [handler_table + eax * 4]
    ; 对应先前的 push，调用结束后恢复栈
    add esp, 8
    iret

INTERRUPT_ENTRY 0x00, 0 ; divide by zero
INTERRUPT_ENTRY 0x01, 0 ; debug
INTERRUPT_ENTRY 0x02, 0 ; non maskable interrupt
INTERRUPT_ENTRY 0x03, 0 ; breakpoint

INTERRUPT_ENTRY 0x04, 0 ; overflow
INTERRUPT_ENTRY 0x05, 0 ; bound range exceeded
INTERRUPT_ENTRY 0x06, 0 ; invalid opcode
INTERRUPT_ENTRY 0x07, 0 ; device not avilable

INTERRUPT_ENTRY 0x08, 1 ; double fault
INTERRUPT_ENTRY 0x09, 0 ; coprocessor segment overrun
INTERRUPT_ENTRY 0x0a, 1 ; invalid TSS
INTERRUPT_ENTRY 0x0b, 1 ; segment not present

INTERRUPT_ENTRY 0x0c, 1 ; stack segment fault
INTERRUPT_ENTRY 0x0d, 1 ; general protection fault
INTERRUPT_ENTRY 0x0e, 1 ; page fault
INTERRUPT_ENTRY 0x0f, 0 ; reserved

INTERRUPT_ENTRY 0x10, 0 ; x86 floating point exception
INTERRUPT_ENTRY 0x11, 1 ; alignment check
INTERRUPT_ENTRY 0x12, 0 ; machine check
INTERRUPT_ENTRY 0x13, 0 ; SIMD Floating - Point Exception

INTERRUPT_ENTRY 0x14, 0 ; Virtualization Exception
INTERRUPT_ENTRY 0x15, 1 ; Control Protection Exception
INTERRUPT_ENTRY 0x16, 0 ; reserved
INTERRUPT_ENTRY 0x17, 0 ; reserved

INTERRUPT_ENTRY 0x18, 0 ; reserved
INTERRUPT_ENTRY 0x19, 0 ; reserved
INTERRUPT_ENTRY 0x1a, 0 ; reserved
INTERRUPT_ENTRY 0x1b, 0 ; reserved

INTERRUPT_ENTRY 0x1c, 0 ; reserved
INTERRUPT_ENTRY 0x1d, 0 ; reserved
INTERRUPT_ENTRY 0x1e, 0 ; reserved
INTERRUPT_ENTRY 0x1f, 0 ; reserved


section .data

; 下面的数组记录了每个中断入口函数的指针
global handler_entry_table
handler_entry_table:
    dd interrupt_entry_0x00
    dd interrupt_entry_0x01
    dd interrupt_entry_0x02
    dd interrupt_entry_0x03
    dd interrupt_entry_0x04
    dd interrupt_entry_0x05
    dd interrupt_entry_0x06
    dd interrupt_entry_0x07
    dd interrupt_entry_0x08
    dd interrupt_entry_0x09
    dd interrupt_entry_0x0a
    dd interrupt_entry_0x0b
    dd interrupt_entry_0x0c
    dd interrupt_entry_0x0d
    dd interrupt_entry_0x0e
    dd interrupt_entry_0x0f
    dd interrupt_entry_0x10
    dd interrupt_entry_0x11
    dd interrupt_entry_0x12
    dd interrupt_entry_0x13
    dd interrupt_entry_0x14
    dd interrupt_entry_0x15
    dd interrupt_entry_0x16
    dd interrupt_entry_0x17
    dd interrupt_entry_0x18
    dd interrupt_entry_0x19
    dd interrupt_entry_0x1a
    dd interrupt_entry_0x1b
    dd interrupt_entry_0x1c
    dd interrupt_entry_0x1d
    dd interrupt_entry_0x1e
    dd interrupt_entry_0x1f