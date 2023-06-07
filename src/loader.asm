[org 0x1000]

dw 0x55aa   ; 魔数，用于 boot 来检测加载 loader 是否正确

; 打印 loading 信息
mov si, loading
call print

xchg bx, bx

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

    xchg bx, bx

    ; 打印各个 ards 结构体信息
    mov cx, [ards_cnt]  ; ards 结构体数量
    mov si, ards_buf    ; ards 结构体指针

.record:
    mov eax, [si]   ; 记录基地址的低 32 位
    mov ebx, [si+8] ; 记录内存长度的低 32 位
    mov edx, [si+16]; 记录本段内存的类型
    add si, 20      ; 指向下一个结构体
    xchg bx, bx
    loop .record


; 程序结束，进行阻塞
jmp $


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

ards_cnt:
    dw 0
ards_buf:
