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
    ; 保存上文寄存器信息
    push ds
    push es
    push fs
    push gs
    pusha

    ; 获取之前 push %1 保存的中断向量
    mov eax, [esp + 12 * 4]

    ; 向中断处理函数传递参数
    push eax

    ; 调用中断处理函数，handler_table 中存储了中断处理函数的指针
    call [handler_table + eax * 4]

global interrupt_exit
interrupt_exit:
    ; 对应先前的 push eax，调用结束后恢复栈
    add esp, 4

    ; 恢复下文寄存器信息
    popa
    pop gs
    pop fs
    pop es
    pop ds

    ; 对应 push %1
    ; 对应 error code 或 magic
    add esp, 8

    iret

; 异常
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

; 外中断
INTERRUPT_ENTRY 0x20, 0 ; clock 时钟中断
INTERRUPT_ENTRY 0x21, 0
INTERRUPT_ENTRY 0x22, 0
INTERRUPT_ENTRY 0x23, 0

INTERRUPT_ENTRY 0x24, 0
INTERRUPT_ENTRY 0x25, 0
INTERRUPT_ENTRY 0x26, 0
INTERRUPT_ENTRY 0x27, 0

INTERRUPT_ENTRY 0x28, 0 ; RTC 实时中断
INTERRUPT_ENTRY 0x29, 0
INTERRUPT_ENTRY 0x2a, 0
INTERRUPT_ENTRY 0x2b, 0

INTERRUPT_ENTRY 0x2c, 0
INTERRUPT_ENTRY 0x2d, 0
INTERRUPT_ENTRY 0x2e, 0
INTERRUPT_ENTRY 0x2f, 0

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
    dd interrupt_entry_0x20
    dd interrupt_entry_0x21
    dd interrupt_entry_0x22
    dd interrupt_entry_0x23
    dd interrupt_entry_0x24
    dd interrupt_entry_0x25
    dd interrupt_entry_0x26
    dd interrupt_entry_0x27
    dd interrupt_entry_0x28
    dd interrupt_entry_0x29
    dd interrupt_entry_0x2a
    dd interrupt_entry_0x2b
    dd interrupt_entry_0x2c
    dd interrupt_entry_0x2d
    dd interrupt_entry_0x2e
    dd interrupt_entry_0x2f

section .text

extern syscall_check
extern syscall_table

global syscall_entry
syscall_entry:
    ; xchg bx, bx ; BMB

    ; 验证系统调用号
    push eax
    call syscall_check
    add esp, 4 ; 函数调用结束后恢复栈

    ; 压入魔数和系统调用号，使得栈结构与 interrupt_entry_%1 相同
    push 0x20230830
    push 0x80

    ; 保存上文寄存器信息
    push ds
    push es
    push fs
    push gs
    pusha

    ; 压入中断向量，保证栈结构符合 interrupt_exit 要求
    push 0x80
    
    ; xchg bx, bx ; BMB
    
    ; 压入系统调用的参数
    push edx ; 第三个参数
    push ecx ; 第二个参数
    push ebx ; 第一个参数

    ; 调用系统调用对应的处理函数
    call [syscall_table + eax * 4]

    ; xchg bx, bx ; BMB
    add esp, 3 * 4 ; 系统调用处理结束恢复栈

    ; 修改栈中的 eax 寄存器值，设置系统调用的返回值
    mov dword [esp + 8 * 4], eax

    ; 跳转到中断返回
    jmp interrupt_exit
