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
    mov [addr],  ebx ; addr

    ; 判断是否为 multiboot2 启动
    cmp eax, 0x36d76289
    jnz _init

    ; 加载 gdt 指针到 gdtr 寄存器
    lgdt [multiboot2_gdt_ptr]
    ; 通过跳转来加载新的代码段选择子到代码段寄存器
    jmp dword code_selector:_next
_next:
    ; 在 32 位保护模式下初始化段寄存器
    mov ax, data_selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; 修改栈顶，与 bootloader 启动设置的栈顶保持一致
    mov esp, 0x10000

_init:
    call kernel_init

    jmp $ ; 阻塞


section .data
; 内核魔数 - bootloader 启动时为 XOS_MAGIC，multiboot2 启动时为 MULTIBOOT2_MAGIC
global magic
magic:
    dd 0
; 地址 - bootloader 启动时为 ARDS 的起始地址，bootloader 启动时为 Boot Information 的起始地址
global addr
addr:
    dd 0


; multiboot2 需要设置成与 bootloader 一样的 GDT
code_selector equ (1 << 3) ; 代码段选择子
data_selector equ (2 << 3) ; 数据段选择子

memory_base equ 0 ; 内存起始位置
memory_limit equ ((1024 * 1024 * 1024 * 4) / (1024 * 4)) - 1 ; 粒度为4K，所以界限为 (4G/4K)-1

multiboot2_gdt_ptr:
    dw (multiboot2_gdt_end - multiboot2_gdt_base) - 1 ; gdt 界限
    dd multiboot2_gdt_base ; gdt 基地址

multiboot2_gdt_base:
    dd 0, 0 ; NULL 描述符
multiboot2_gdt_code:
    dw memory_limit & 0xffff ; 段界限 0-15 位
    dw memory_base & 0xffff ; 基地址 0-15 位
    db (memory_base >> 16) & 0xff ; 基地址 16-23 位
    ; 存在内存 | DLP=0 | 代码段 | 非依从 | 可读 | 没有被访问过
    db 0b1_00_1_1010
    ; 粒度 4K | 32 位 | 不是 64 位 | 段界限 16-19 位
    db 0b1_1_0_0_0000 | (memory_limit >> 16) & 0xf
    db (memory_base >> 24) & 0xff ; 基地址 24-31 位
multiboot2_gdt_data:
    dw memory_limit & 0xffff ; 段界限 0-15 位
    dw memory_base & 0xffff ; 基地址 0-15 位
    db (memory_base >> 16) & 0xff ; 基地址 16-23 位
    ; 存在内存 | DLP=0 | 数据段 | 向上扩展 | 可写 | 没有被访问过
    db 0b1_00_1_0010
    ; 粒度 4K | 32 位 | 不是 64 位 | 段界限 16-19 位
    db 0b1_1_0_0_0000 | (memory_limit >> 16) & 0xf
    db (memory_base >> 24) & 0xff ; 基地址 24-31 位
multiboot2_gdt_end:
