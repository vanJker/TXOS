# 056 multiboot2 引导

## 1. i386 状态

上一节我们已经成功实现使用 multiboot2 引导启动进入内核，此时内核的状态为：

- EAX：魔数 `0x36d76289`
- EBX：包含 bootloader 存储 multiboot2 信息结构体的，32 位 物理地址
- CS：32 位 可读可执行的代码段，尺寸 4G
- DS/ES/FS/GS/SS：32 位可读写的数据段，尺寸 4G
- A20 线：启用
- CR0：PG = 0, PE = 1，其他未定义
- EFLAGS：VM = 0, IF = 0, 其他未定义
- ESP：内核必须尽早切换栈顶地址
- GDTR：内核必须尽早使用自己的全局描述符表
- IDTR：内核必须在设置好自己的中断描述符表之前关闭中断

也就是说此时已经启用了 32 位保护模式，此时 multiboot2 也为我们提供了 GDT 和 ESP，但是为了和 XOS bootloader 兼容，它要求我们必须尽早切换到自己的 GDT 和 ESP。

还需要注意的是，使用 bootloader 引导启动时，我们进行了内存检测，并且在初始化内存管理 `memory_init()` 中使用了相对应的地址描述符 ARDS。multiboot2 并没有提供 ARDS 给我们，但是在 boot information 中也提供了内存信息。如果我们是通过 multiboot2 引导，就必须使用 boot information 所提供的内存信息来对内存管理进行初始化。

> **注：boot information 的起始地址即为 multiboot2 引导完毕时 ebx 中所存的值。**

## 2. 引导信息

### 2.1 引导信息格式

当使用 multiboot2 引导进入操作系统时，EBX 寄存器包含一个 multiboot2 引导信息的物理地址，multiboot2 引导加载程序提供这些引导信息，来与操作系统沟通。操作系统可以根据需要使用或忽略结构的任何部分，例如本节我们只需要使用与内存检测相关的 `mmap` 部分即可。

multiboot2 信息结构体及其相关子结构可以由 multiboot2 引导加载程序放置在内存的任何位置（当然，不包括为内核和引导模块保留的内存）。操作系统有责任避免在使用完所需的信息之前覆盖这块内存。

### 2.2 基本标签格式

multiboot2 的引导信息由固定部分和一系列标签（Tag）组成。其起始地址必须按 8 字节对齐。

引导信息的固定部分如下所示：

```
        +-------------------+
u32     | total_size        |
u32     | reserved          |
        +-------------------+
```

- `total_size` 包含 multiboot2 引导信息的总大小（包括该字段和终止标签），它是以字节为单位。
- `reserved` 始终设置为零，操作系统镜像必须忽略它。

引导信息的固定部分之后，就是一系列标签（Tag）。每个标签（Tag）以以下字段开始：

```
        +-------------------+
u32     | type              |
u32     | size              |
        +-------------------+
```

- `type` 是标识符，用于判断解释标签的内容。
- `size` 是标签的大小，包括头部字段，但不包括填充字段。

> **注：标签之间会根据需要进行填充，以便每个标签都以 8 字节对齐的地址开始。标签以类型 `type` 为 `0`、大小 `size` 为 `8` 的标签作为终止标志。**

## 2.3 Memory Map 标签

我们只需关心与内存检测相关的标签（Tag），即 Memory Map 这个标签。因为这个标签提供了内存映射信息。

这个标签（Tag）的结构如下：

```
        +-------------------+
u32     | type = 6          |
u32     | size              |
u32     | entry_size        |
u32     | entry_version     |
varies  | entries           |
        +-------------------+
```

- `entry_size` 包含一个条目（entry）的大小，以便将来可以在这个标签添加新的字段。`entry_szie` 保证是 8 的倍数。
- `entry_version` 目前设置为 '0'。将来的版本会递增这个字段。未来的版本保证与旧格式向后兼容。
- `entries` 是接下来一系列的条目（entry）的起始位置。

每个条目（entry）具有以下结构：

```
        +-------------------+
u64     | base_addr         |
u64     | length            |
u32     | type              |
u32     | reserved          |
        +-------------------+
```

- `base_addr` 是内存区域的起始物理地址。
- `length` 是该内存区域的大小，以字节为单位。
- `type` 表示该内存区域的类型，
    - 值为 1 表示可用的 RAM，
    - 值为 3 表示包含 ACPI 信息的可用内存，
    - 值为 4 表示需要在休眠期间保留的保留内存，
    - 值为 5 表示被有缺陷的 RAM 模块占用的内存，
    - 其他值表示保留区域（通常设值为 2 表示保留区域）。
- `reserved` 由引导加载程序设置为'0'，操作系统必须忽略它。

这些条目提供的内存信息与 ARDS 提供的信息类似（实际上检测出的内存信息完全一致），都可以用于初始化内存管理。

## 3. 初始化内存管理

依据以上的原理说明，我们可以实现 multiboot2 引导启动时的内存管理初始化。

### 3.1 魔数

> 以下代码位于 `include/xos/multiboot2.h`

```c
// 魔数，multiboot2 引导时存放在 EAX 
#define MULTIBOOT2_MAGIC 0x36d76289
```

### 3.2 基本标签格式

标签（Tag）的结构体以及本节使用到的标签（Tag）的两种类型：

```c
// multiboot2 tag
typedef struct multiboot2_tag_t {
    u32 type;   // tag 的类型
    u32 size;   // tag 的大小
} multiboot2_tag_t;

// multiboot2 tag 类型
#define MULTIBOOT2_TAG_TYPE_END 0
#define MULTIBOOT2_TAG_TYPE_MAP 6
```

### 3.3 mmap 标签

`mmap` 类型的标签的结构体：

```c
// multiboot2 memory-map tag
typedef struct multiboot2_tag_mmap_t {
    u32 type;           // tag 的类型（为 6）
    u32 size;           // tag 的大小
    u32 entry_size;     // entry 的大小
    u32 entry_version;  // entry 的版本（目前为 0）
    multiboot2_mmap_entry_t entries[0];
} multiboot2_tag_mmap_t;
```

`mmap` 标签的条目（entry）的结构体：

```c
// multiboot2 memory-map entry
typedef struct multiboot2_mmap_entry_t {
    u64 addr;   // 地址区域的起始地址
    u64 len;    // 地址区域的长度
    u32 type;   // 地址区域的类型
    u32 zero;   // 保留（为 0）
} multiboot2_mmap_entry_t;
```

`mmap` 标签的条目所代表的内存区域的类型：

```c
// multiboot2 memory-map 的类型
#define MULTIBOOT2_MEMORY_AVAILABLE 1
#define MULTIBOOT2_MEMORY_RESERVED  2
#define MULTIBOOT2_MEMORY_ACPI_RECLAIMABLE 3
#define MULTIBOOT2_MEMORY_NVS       4
#define MULTIBOOT2_MEMORY_BADRAM    5
```

> 注：结构体 `multiboot2_tag_mmap_t` 的最后一个成员 `entries` 是柔性数组。

---

在 C 语言中，柔性数组（flexible array）是一种特殊的数组形式，它允许在结构体的末尾定义一个长度可变的数组。柔性数组可以用于动态分配内存，并在结构体中保存不同长度的数据。

要声明一个包含柔性数组的结构体，需要遵循以下规则：

1. 柔性数组只能作为结构体的 **最后一个成员**，它不能有任何后续成员。
2. 柔性数组本身不能被直接访问，只能通过指针来操作。
3. 结构体中不能有任何其他的数组成员。
4. 柔性数组长度为 0，形如 `data[]` 或 `data[0]`。

下面是一个使用柔性数组的简单示例：

```c
#include <stdio.h>
#include <stdlib.h>

struct MyStruct {
    int length;
    int data[];  // 柔性数组
};

int main() {
    int size = 5;
    struct MyStruct* myStruct = malloc(sizeof(struct MyStruct) + size * sizeof(int));

    myStruct->length = size;
    for (int i = 0; i < size; i++) {
        myStruct->data[i] = i;
    }

    printf("Length: %d\n", myStruct->length);
    for (int i = 0; i < size; i++) {
        printf("Data[%d]: %d\n", i, myStruct->data[i]);
    }

    free(myStruct);
    return 0;
}
```

在上面的示例中，我们定义了一个包含柔性数组的结构体 `MyStruct`。在 `main` 函数中，我们动态分配了足够的内存来容纳结构体和柔性数组的数据。然后，我们可以通过指针 `myStruct` 来访问和操作这个柔性数组。

请注意，柔性数组在结构体内部没有分配实际的内存空间，而是通过结构体后续的内存来存储数据。因此，在动态分配内存时，我们需要使用 `sizeof(struct MyStruct) + size * sizeof(int)` 来确保为柔性数组分配足够的空间。

柔性数组在处理变长数据时非常方便，但需要小心使用，避免越界访问和内存泄漏等问题。

### 3.4 memory_init

在内存管理初始化中增加 multiboot2 引导启动时的逻辑：

```c
// 魔数 - bootloader 启动时为 XOS_MAGIC，multiboot2 启动时为 MULTIBOOT2_MAGIC
extern u32 magic;

// 地址 - bootloader 启动时为 ARDS 的起始地址，bootloader 启动时为 Boot Information 的起始地址
extern u32 addr;
```

```c
void memory_init() {
    u32 cnt; // 检测到的内存区域个数

    if (magic == XOS_MAGIC) {
        // 如果是 XOS bootloader 进入的内核
        cnt = *(u32 *)addr;
        ...
    } else if (magic == MULTIBOOT2_MAGIC) {
        // 如果是 multiboot2 进入的内核

        // 这里是读取引导信息的固定部分的数据
        u32 total_size = *(u32 *)addr;
        multiboot2_tag_t *tag = (multiboot2_tag_t *)(addr + 8);

        LOGK("Multiboot2 Information Size: 0x%p\n", total_size);

        // 寻找类型为 mmap 的 tag
        while (tag->type != MULTIBOOT2_TAG_TYPE_END) {
            if (tag->type == MULTIBOOT2_TAG_TYPE_END) {
                // 如果到最后都没有找到 mmap 类型的 tag，则触发 panic 
                panic("Memory init without mmap tag!!!\n");
            }
            if (tag->type == MULTIBOOT2_TAG_TYPE_MMAP) {
                break;
            }
            // 需要填充，使得下一个 tag 以 8 字节对齐
            tag = (multiboot2_tag_t *)(ROUND_UP((u32)tag + tag->size, 8));
        }

        // 寻找到类型为 mmap 的 tag
        multiboot2_tag_mmap_t *mmap_tag = (multiboot2_tag_mmap_t *)tag;
        
        // 遍历 mmap tag 的 entry（即内存区域）
        // 这部分与 bootloader 的遍历 ARDS 的逻辑相同）
        multiboot2_mmap_entry_t *entry = mmap_tag->entries;
        cnt = 0;
        while ((u32)entry < (u32)tag + tag->size) {
            LOGK("ZONE %d:[base]0x%p,[size]:0x%p,[type]:%d\n",
                 cnt++, (u32)entry->addr, (u32)entry->len, (u32)entry->type);
            
            if (entry->type == MULTIBOOT2_MEMORY_AVAILABLE
                && entry->addr >= MEMORY_ALLOC_BASE
                && entry->len > mm.alloc_size) {
                mm.alloc_base = entry->addr;
                mm.alloc_size = entry->len;
            }

            entry = (multiboot2_mmap_entry_t *)((u32)entry + mmap_tag->entry_size);
        }
    }
    ...
}
```

> 注：这里使用了向上取整的功能 `ROUND_UP()`，它的定义如下（代码位于 `include/xos/stdlib.h`）：
>
> ```c
> // 将 x 以 k 为单位向上取整
> #define ROUND_UP(x, k) (((x) + (k)-1) & -(k))
> ```
> 关于其原理，可以查看 [FAQ](#faq) 部分。

## 4. GDT & 段寄存器 & 栈

正如先前所述，multiboot2 引导启动进入内核时，已经启用了 32 位保护模式，此时 multiboot2 也为我们提供了 GDT 和 ESP，但是为了和 XOS bootloader 兼容，它要求我们必须尽早切换到自己的 GDT 和 ESP。

为了后续操作与 bootloader 引导启动时相同（即无需改动），尤其是内存布局部分，我们需要将 **GDT**、**段寄存器** 和 **栈** 设置成与 bootloader 引导启动时一致。

这个任务十分简单，只需在进入内核初始化 `kernel_init()` 之前，判断十是否为 multiboot2 引导启动，如果是则重新设置相关状态（即只需要在进入 `kernel_init()` 之前保证 **GDT**、**段寄存器** 和 **栈** 的状态与 bootloader 此时一致即可）。

两者的引导启动流程对比如下：

```
bootloader:
+-------------------------------------------------------------------+
|                                                                   |
| GDT -> PE -> Segment Registers -> Stack -> _start -> kernel_init  |
|                                                                   |
+-------------------------------------------------------------------+

multiboot2:
+-------------------------------------------------------------------+
|                                                                   |
| PE -> _start -> GDT -> Segment Registers -> Stack -> kernel_init  |
|                                                                   |
+-------------------------------------------------------------------+
```

### 4.1 GDT

在数据段中设置与 XOS loader 中一致的 GDT：

> 以下代码位于 `kernel/start.asm`

```x86asm
section .data
...
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
```

### 4.2 进入 kernel_init

在 `_start` 处增加 multiboot2 引导启动的分支逻辑。如果是 multiboot2 引导启动的，进入 `_start` 后需要先重新加载 GDT，设置段寄存器，设置栈顶，保证在进入 `kerne_init` 之前与 bootloader 引导启动的状态一致。

```x86asm
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
```

## 6. 功能测试

使用 GRUB 进入系统，目前预期为，与 bootloader 进入系统，两者行为一致。

## 5. FAQ

> **`ROUND_UP()` 宏的原理是？**
> ***
> 这个宏的完整定义为 `#define ROUND_UP(x, k) (((x) + (k)-1) & -(k))`，表示将 x 以 k 为单位向上取整。
> 
> 正常来说，如果我们需要实现这个功能，我们会更倾向于使用 `(x + k - 1) / k` 来实现。但是众所周知，除法的复杂度很高，而所以为了高性能，我们需要尽可能的使用位运算。
>
> 因为 `x / k = x & ~(k-1)`，且 `~(k-1) + 1 = -(k-1)`，可得 `~(k-1) = -k`，所以代入得 `x / k = x & -k`。

## 6. 参考文献

- <https://www.gnu.org/software/grub/manual/multiboot2/multiboot.pdf>