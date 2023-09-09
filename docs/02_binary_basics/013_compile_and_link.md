# 013 编译和链接

![compile_and_link](./images/compile_and_link.svg)

## 1. 预处理

`-E` 参数表示进行预处理，即进行宏展开和宏替换；`-I` 参数指示头文件搜索路径。

```bash
gcc -m32 -E main.c -I ../include > test.c
```

```c
/* tests/test.c */

# 0 "main.c"
# 0 "<built-in>"
# 0 "<command-line>"
# 1 "/usr/include/stdc-predef.h" 1 3 4
# 0 "<command-line>" 2
# 1 "main.c"
# 1 "../include/xos/xos.h" 1

void kernel_init();
# 2 "main.c" 2

int magic = 20230614;
char msg[] = "Hello XOS!!!";
char buf[1024];

void kernel_init() {
    int i;
    char *tty = (char *)0xb8000;
    for (i = 0; i < sizeof(msg); i++) {
        tty[i * 2] = msg[i];
        tty[i*2+1] = 0x02;
    }
    return;
}
```

## 2. 编译

`-S` 参数表示进行汇编，即将源文件编译成汇编语言的指令和伪指令。其中实际使用的可执行程序是 `/usr/lib/gcc/x86_64-pc-linux-gnu/13.1.1/cc1`，在最后的 `gcc` 集成的输出中可以知道。

```bash
gcc -m32 -S test.c > test.S
```

## 3. 汇编

使用汇编器 `as` 将汇编指令转换成二进制的机器指令。

```bash
as -32 test.s -o test.o
```

## 4. 链接

使用链接器 `ld` 将目标文件链接起来，其中实际使用的可执行程序是 `/usr/lib/gcc/x86_64-pc-linux-gnu/13.1.1/collect2`，在最后的 `gcc` 集成的输出中可以知道。

`-static` 参数表示进行静态链接，`-e` 参数表示该可执行程序的入口点为 `kernel_init`。

```bash
ld -m elf_i386 -static test.o -o test.out -e kernel_init
```

## 5. gcc 集成

`--verbose` 参数表示输出详细信息，`-nostartfiles` 表示无需链接起始文件（例如运行时库）。

```bash
gcc --verbose -m32 main.c -I../include -o main.out -e kernel_init -nostartfiles
```

以下为详细输出：

```console
Using built-in specs.
COLLECT_GCC=gcc
COLLECT_LTO_WRAPPER=/usr/lib/gcc/x86_64-pc-linux-gnu/13.1.1/lto-wrapper
Target: x86_64-pc-linux-gnu
Configured with: /build/gcc/src/gcc/configure --enable-languages=ada,c,c++,d,fortran,go,lto,objc,obj-c++ --enable-bootstrap --prefix=/usr --libdir=/usr/lib --libexecdir=/usr/lib --mandir=/usr/share/man --infodir=/usr/share/info --with-bugurl=https://bugs.archlinux.org/ --with-build-config=bootstrap-lto --with-linker-hash-style=gnu --with-system-zlib --enable-__cxa_atexit --enable-cet=auto --enable-checking=release --enable-clocale=gnu --enable-default-pie --enable-default-ssp --enable-gnu-indirect-function --enable-gnu-unique-object --enable-libstdcxx-backtrace --enable-link-serialization=1 --enable-linker-build-id --enable-lto --enable-multilib --enable-plugin --enable-shared --enable-threads=posix --disable-libssp --disable-libstdcxx-pch --disable-werror
Thread model: posix
Supported LTO compression algorithms: zlib zstd
gcc version 13.1.1 20230429 (GCC) 
COLLECT_GCC_OPTIONS='-v' '-m32' '-I' '../include' '-o' 'main.out' '-e' 'kernel_init' '-nostartfiles' '-mtune=generic' '-march=x86-64' '-dumpdir' 'main.out-'
 /usr/lib/gcc/x86_64-pc-linux-gnu/13.1.1/cc1 -quiet -v -I ../include -imultilib 32 main.c -quiet -dumpdir main.out- -dumpbase main.c -dumpbase-ext .c -m32 -mtune=generic -march=x86-64 -version -o /tmp/cc8WGJ51.s
GNU C17 (GCC) version 13.1.1 20230429 (x86_64-pc-linux-gnu)
        compiled by GNU C version 13.1.1 20230429, GMP version 6.2.1, MPFR version 4.2.0, MPC version 1.3.1, isl version isl-0.26-GMP

GGC heuristics: --param ggc-min-expand=100 --param ggc-min-heapsize=131072
ignoring nonexistent directory "/usr/lib/gcc/x86_64-pc-linux-gnu/13.1.1/../../../../x86_64-pc-linux-gnu/include"
#include "..." search starts here:
#include <...> search starts here:
 ../include
 /usr/lib/gcc/x86_64-pc-linux-gnu/13.1.1/include
 /usr/local/include
 /usr/lib/gcc/x86_64-pc-linux-gnu/13.1.1/include-fixed
 /usr/include
End of search list.
Compiler executable checksum: f7ab8f6abad0db9962575524ae915978
COLLECT_GCC_OPTIONS='-v' '-m32' '-I' '../include' '-o' 'main.out' '-e' 'kernel_init' '-nostartfiles' '-mtune=generic' '-march=x86-64' '-dumpdir' 'main.out-'
 as -v -I ../include --32 -o /tmp/ccmNs1IG.o /tmp/cc8WGJ51.s
GNU assembler version 2.40.0 (x86_64-pc-linux-gnu) using BFD version (GNU Binutils) 2.40.0
COMPILER_PATH=/usr/lib/gcc/x86_64-pc-linux-gnu/13.1.1/:/usr/lib/gcc/x86_64-pc-linux-gnu/13.1.1/:/usr/lib/gcc/x86_64-pc-linux-gnu/:/usr/lib/gcc/x86_64-pc-linux-gnu/13.1.1/:/usr/lib/gcc/x86_64-pc-linux-gnu/
LIBRARY_PATH=/usr/lib/gcc/x86_64-pc-linux-gnu/13.1.1/32/:/usr/lib/gcc/x86_64-pc-linux-gnu/13.1.1/../../../../lib32/:/lib/../lib32/:/usr/lib/../lib32/:/usr/lib/gcc/x86_64-pc-linux-gnu/13.1.1/:/usr/lib/gcc/x86_64-pc-linux-gnu/13.1.1/../../../:/lib/:/usr/lib/
COLLECT_GCC_OPTIONS='-v' '-m32' '-I' '../include' '-o' 'main.out' '-e' 'kernel_init' '-nostartfiles' '-mtune=generic' '-march=x86-64' '-dumpdir' 'main.out.'
 /usr/lib/gcc/x86_64-pc-linux-gnu/13.1.1/collect2 -plugin /usr/lib/gcc/x86_64-pc-linux-gnu/13.1.1/liblto_plugin.so -plugin-opt=/usr/lib/gcc/x86_64-pc-linux-gnu/13.1.1/lto-wrapper -plugin-opt=-fresolution=/tmp/cc9uYai2.res -plugin-opt=-pass-through=-lgcc -plugin-opt=-pass-through=-lgcc_s -plugin-opt=-pass-through=-lc -plugin-opt=-pass-through=-lgcc -plugin-opt=-pass-through=-lgcc_s --build-id --eh-frame-hdr --hash-style=gnu -m elf_i386 -dynamic-linker /lib/ld-linux.so.2 -pie -o main.out -e kernel_init -L/usr/lib/gcc/x86_64-pc-linux-gnu/13.1.1/32 -L/usr/lib/gcc/x86_64-pc-linux-gnu/13.1.1/../../../../lib32 -L/lib/../lib32 -L/usr/lib/../lib32 -L/usr/lib/gcc/x86_64-pc-linux-gnu/13.1.1 -L/usr/lib/gcc/x86_64-pc-linux-gnu/13.1.1/../../.. /tmp/ccmNs1IG.o -lgcc --push-state --as-needed -lgcc_s --pop-state -lc -lgcc --push-state --as-needed -lgcc_s --pop-state
COLLECT_GCC_OPTIONS='-v' '-m32' '-I' '../include' '-o' 'main.out' '-e' 'kernel_init' '-nostartfiles' '-mtune=generic' '-march=x86-64' '-dumpdir' 'main.out.'
```

## 6. 内核链接

目前我们的 `src/kernel/` 一共有 2 个文件：`start.asm` 和 `main.c`。

```x86asm
/* start.asm */

[bits 32]

extern kernel_init

global _start
_start:
    xchg bx, bx
    call kernel_init
    xchg bx, bx
    jmp $ ; 阻塞
```

```c
/* main.c */

#include <xos/xos.h>

int magic = 20230614;
char msg[] = "Hello XOS!!!";
char buf[1024];

void kernel_init() {
    int i;
    char *tty = (char *)0xb8000;
    for (i = 0; i < sizeof(msg); i++) {
        tty[i * 2] = msg[i];
        tty[i*2+1] = 0x02;
    }
    return;
}
```

在 `kernel` 中引入链接脚本，以确保内核的第一条指令位于我们约定的 `0x10000` 地址处。之前我们通过 `x86` 汇编的 `_start` 符号和 `ld` 的参数 `-Ttext=0x10000` 将 `_start` 处指令链接至 `0x10000` 地址。接下来通过链接脚本更加规范化的完成这一过程。

通过引入 `section .text.entry` 表示 `start.asm` 的指令位于 `.text.extny` section。接下来编写链接脚本 `linker.ld`。

```ini
/* src/linker.ld */

ENTRY(_start)
BASE_ADDRESS = 0x10000;

SECTIONS
{
    . = BASE_ADDRESS;
    skernel = .;

    stext = .;
    .text : {
        *(.text.entry)
        *(.text .text.*)
    }

    . = ALIGN(4K);
    etext = .;

    srodata = .;
    .rodata : {
        *(.rodata .rodata.*)
        *(.srodata .srodata.*)
    }

    . = ALIGN(4K);
    erodata = .;

    sdata = .;
    .data : {
        *(.data .data.*)
        *(.sdata .sdata.*)
    }

    . = ALIGN(4K);
    edata = .;

    .bss : {
        *(.bsss.stack)
        sbss = .;
        *(.bss .bss.*)
        *(.sbss .sbss.*)
    }

    . = ALIGN(4K);
    ebss = .;

    ekernel = .;

    /DISCARD/ : {
        *(.eh_frame)
    }
}
```

通过这个脚本确保了 `.text.entry` section 位于 `0x10000` 地址起始的内存空间，并记录了一些有用的内核空间分布的地址信息。

## 7. 链接次序

`ld` 的原理是，按照目标文件参数的次序，从 `-Ttext` 参数指定的地址处开始，依次将目标文件中的 `.text` 段放入内核目标文件的 `.text` 段。如果不按照次序链接，会导致内核不按预期执行，以及调试无法正常进行。

所以，上面的链接脚本虽然记录了有用的地址信息，但它会导致在调试内核变得极其困难（应该是没有保存调试信息所在的段），所以我们通过指定 `ld` 参数的次序和使用 `-Ttext 0x10000` 来链接内核。因为要求 `start.asm` 必须在 `0x10000` 处，所以将编译出来的 `start.o` 置于 `ld` 目标文件参数的第一位。

```makefile
KERNEL_ENTRY := 0x10000

# ld 的目标文件参数次序必须满足依赖次序，且保证第一条指令在 0x10000 地址处
KERNEL_OBJS := $(TARGET)/kernel/start.o \
			   $(TARGET)/kernel/main.o

$(KERNEL_ELF): $(KERNEL_OBJS)
	$(shell mkdir -p $(dir $@))
	ld -m elf_i386 -static $^ -o $@ -Ttext $(KERNEL_ENTRY)
```