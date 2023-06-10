[org 0x1000]

dw 0x55aa   ; 魔数，用于 boot 来检测加载 loader 是否正确

; 打印 loading 信息
mov si, loading
call print

detect_memory:
    ; 将 ebx 置零
    xor ebx, ebx

    ; 将 ex:di 指向 ards 结构体缓存的地址处
    mov ax, 0
    mov es, ax
    mov edi, ards_buf

    ; 将 edx 固定为签名 0x534d4150
    mov edx, 0x534d4150

.next:
    mov eax, 0xe820 ; 设置子功能号
    mov ecx, 20     ; 设置 args 结构体的大小
    int 0x15        ; 系统调用 0x15

    jc error        ; 如果 CF 位为 1，则调用出错
    add di, cx      ; 将缓存指针指向下一个结构体地址处
    inc word [ards_cnt] ; 更新记录的 ards 结构体数量
    cmp ebx, 0      ; ebx 为 0 表示当前为最后一个 ards 结构体
    jnz .next
    
    ; 打印 detecting 信息
    mov si, detecting
    call print

enable_protected_mode:
    xchg bx, bx ; 断点

    ; 关闭中断
    cli

    ; 打卡 A20 线
    in al, 0x92
    or al, 0x02
    out 0x92, al

    ; 加载 gdt 指针到 gdtr 寄存器
    lgdt [gdt_ptr]

    ; 使能保护模式
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax

    xchg bx, bx ; 断点
    ; 通过跳转来刷新缓存，同时加载代码段选择子到代码段寄存器，从而执行保护模式下的指令
    jmp dword code_selector:protected_mode


print:
    mov ah, 0x0e
.next:
    mov al, [si]
    cmp al, 0
    jz .done
    int 0x10
    inc si
    jmp .next
.done:
    ret

error:
    mov si, .msg
    call print
    hlt     ; CPU 停机
    jmp $
    .msg db "Loading Error!", 10, 13, 0

loading:
    db "Loading LoongOS...", 10, 13, 0  ; 在 ASCII 编码中，13:\n, 10:\r, 0:\0

detecting:
    db "Detecting Memory Success...", 10, 13, 0  ; 在 ASCII 编码中，13:\n, 10:\r, 0:\0


[bits 32]
protected_mode:
    xchg bx, bx ; 断点
    ; 在 32 位保护模式下初始化段寄存器
    mov ax, data_selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; 修改栈顶，这里只是随意制定了一个地址，因为还没有用到栈
    mov esp, 0x10000

    ; 现在可以随意访问 32 位的地址了
    mov byte [0xb8000], 'P'
    mov byte [0x200000], 'P'

    jmp $ ; 阻塞


code_selector equ (1 << 3) ; 代码段选择子
data_selector equ (2 << 3) ; 数据段选择子

memory_base equ 0 ; 内存起始位置
memory_limit equ ((1024 * 1024 * 1024 * 4) / (1024 * 4)) - 1 ; 粒度为4K，所以界限为 (4G/4K)-1

gdt_ptr:
    dw (gdt_end - gdt_base) - 1 ; gdt 界限
    dd gdt_base ; gdt 基地址

gdt_base:
    dd 0, 0 ; NULL 描述符
gdt_code:
    dw memory_limit & 0xffff ; 段界限 0-15 位
    dw memory_base & 0xffff ; 基地址 0-15 位
    db (memory_base >> 16) & 0xff ; 基地址 16-23 位
    ; 存在内存 | DLP=0 | 代码段 | 非依从 | 可读 | 没有被访问过
    db 0b1_00_1_1010
    ; 粒度 4K | 32 位 | 不是 64 位 | 段界限 16-19 位
    db 0b1_1_0_0_0000 | (memory_limit >> 16) & 0xf
    db (memory_base >> 24) & 0xff ; 基地址 24-31 位
gdt_data:
    dw memory_limit & 0xffff ; 段界限 0-15 位
    dw memory_base & 0xffff ; 基地址 0-15 位
    db (memory_base >> 16) & 0xff ; 基地址 16-23 位
    ; 存在内存 | DLP=0 | 数据段 | 向上扩展 | 可写 | 没有被访问过
    db 0b1_00_1_0010
    ; 粒度 4K | 32 位 | 不是 64 位 | 段界限 16-19 位
    db 0b1_1_0_0_0000 | (memory_limit >> 16) & 0xf
    db (memory_base >> 24) & 0xff ; 基地址 24-31 位
gdt_end:

ards_cnt:
    dw 0
ards_buf:
