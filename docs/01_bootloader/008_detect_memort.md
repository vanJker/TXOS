# 008 内存检测

## 1. 原理说明

通过 **BIOS 系统调用：0x15，子功能号：0xe820** 可以实现内存检测。下面对使用该系统调用获得的的 ARDS 结构体进行解释说明。

### 1.1 Address Range Descriptor Structure / ARDS

ARDS 结构体各字段说明：

| 字节偏移量  | 属性名称       | 描述                          |
| ---------- | ------------ | ---------------------------- |
| 0          | BaseAddrLow  | 基地址的低 32 位               |
| 4          | BaseAddrHigh | 基地址的高 32 位               |
| 8          | LengthLow    | 内存长度的低 32 位，以字节为单位 |
| 12         | LengthHigh   | 内存长度的高 32 位，以字节为单位 |
| 16         | Type         | 本段内存的类型                 |

### 1.2 Type 字段

| Type 值 | 名称                  | 描述                                                                         |
| ------- | -------------------- | --------------------------------------------------------------------------- |
| 1       | AddressRangeMemory   | 这段内存可以被操作系统使用                                                      |
| 2       | AddressRangeReserved | 内存使用中或者被系统保留，操作系统不可以用此内存                                   |
| 其他     | 未定义               | 未定义，将来会用到．目前保留． 但是需要操作系统一样将其视为ARR(AddressRangeReserved) |


### 1.3 系统调用前的输入参数

| 寄存器或状态位 | 参数用途                                            |
| ------------ | -------------------------------------------------- |
| EAX          | 子功能号： EAX 寄存器用来指定子功能号，此处输入为 0xe820 |
| EBX          | 内存信息需要按类型分多次返回，由于每次执行一次中断都只返回一种类型内存的ARDS 结构，所以要记录下一个待返回的内存ARDS，在下一次中断调用时通过此值告诉 BIOS 该返回哪个 ARDS，这就是后续值的作用。第一次调用时一定要置为0，EBX 具体值我们不用关注，字取决于具体 BIOS 的实现，每次中断返回后，BIOS 会更新此值 |
| ES: DI       | ARDS 缓冲区：BIOS 将获取到的内存信息写入此寄存器指向的内存，每次都以 ARDS 格式返回 |
| ECX          | ARDS 结构的字节大小：用来指示 BIOS 写入的字节数。调用者和 BIOS 都同时支持的大小是 20 字节，将来也许会扩展此结构 |
| EDX          | 固定为签名标记 `0x534d4150`，此十六进制数字是字符串 `SMAP` 的ASCII 码： BIOS 将调用者正在请求的内存信息写入 ES: DI 寄存器所指向的ARDS 缓冲区后，再用此签名校验其中的信息 |

### 1.4 系统调用后的返回值

| 寄存器或状态位 | 参数用途                                                          |
| ------------ | ---------------------------------------------------------------- |
| CF 位        | 若CF 位为 0 表示调用未出错，CF 为1，表示调用出错                       |
| EAX          | 字符串 SMAP 的 ASCII 码 `0x534d4150`                               |
| ES:DI        | ARDS 缓冲区地址，同输入值是一样的，返回时此结构中己经被BIOS 填充了内存信息 |
| ECX          | BIOS 写入到 ES:DI 所指向的 ARDS 结构中的字节数，BIOS 最小写入 20 字节   |
| EBX          | 后续值：下一个 ARDS 的位置。每次中断返回后，BIOS 会更新此值， BIOS 通过此值可以找到下一个待返回的 ARDS 结构，咱们不需要改变 EBX 的值，下一次中断调用时还会用到它。在 CF 位为 0 的情况下，若返回后的 EBX 值为 0，表示这是最后一个 ARDS 结构 |

## 2. 代码分析

### 2.1 主要逻辑

```x86asm
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

    xchg bx, bx     ; 断点

    ; 打印各个 ards 结构体信息
    mov cx, [ards_cnt]  ; ards 结构体数量
    mov si, ards_buf    ; ards 结构体指针

.record:
    mov eax, [si]   ; 记录基地址的低 32 位
    mov ebx, [si+8] ; 记录内存长度的低 32 位
    mov edx, [si+16]; 记录本段内存的类型
    add si, 20      ; 指向下一个结构体
    xchg bx, bx     ; 断点
    loop .record
```

### 2.2 错误信息打印

```x86asm
error:
    mov si, .msg
    call print
    hlt     ; CPU 停机
    jmp $
    .msg db "Loading Error!", 10, 13, 0
```

### 2.3 数据域

```x86asm
detecting:
    db "Detecting Memory Success...", 10, 13, 0  ; 在 ASCII 编码中，13:\n, 10:\r, 0:\0

ards_cnt:
    dw 0
ards_buf:
```

### 2.4 调试验证

通过 bochs 的调试功能可得，一共获得 6 个内存段的 ARDS 结构体，其中有 2 个内存段的 Type 值为 1，即该内存段可以被操作系统使用。