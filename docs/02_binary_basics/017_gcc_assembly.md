# GCC 汇编分析

## 1. CFI

CFI (Call Frame Information)，即调用栈帧信息，是一种 DWARF 信息，用于调试中获取调用异常。

如果不需要生成 CFI，可以给 `gcc` 加上参数 `-fno-asynchronous-unwind-tables`。

## 2. PIC

PIC (Position Independent Code)，即位置无关代码。例如

```s
call __x86.get_pc_thunk.ax
```

这段代码的功能是，将调用时寄存器 `eip` (extend instruction pointer) 的值，存入到寄存器 `eax` 中。

因为 `_GLOBAL_OFFSET_TABLE_` 是相对于当前指令的偏倚量，所以可以通过以下代码来得到全局符号表的地址，里面存储了符号的地址信息。

```s
addl	$_GLOBAL_OFFSET_TABLE_, %eax
```

如果不需要生成 PIC，可以给 `gcc` 加上参数 `-fno-pic`。

## 3. ident

`.ident` 段中存储了 GCC 的版本信息，可以通过指定参数 `-Qn` 来不生成 ident。

## 4. 栈对齐

以 16 字节对齐为例：

-16 = 0 - 16 = 0x0000_0000 - 0x10 = 0xffff_fff0

这样可以将栈对齐到 16 字节。字节对齐的好处是，访问内存更加高效，因为无需使用拼接，所以会使用更少的时钟周期。

如果不需要生成 PIC，可以给 `gcc` 加上参数 `-mpreferred-stack-boundary=2`。

## 5. 栈帧

一个栈由两个指针决定，一个是 `esp`，另一个是 `ebp`。示意图如下：

```
low address <- high address
|   | frame |   |
<-- ^esp    ^ebp
```

当加入新的栈帧时，都需要保存上一个栈帧的信息（即 `esp` 和 `ebp`）。相反的，退出一个栈帧时，都需要恢复上一个栈帧的信息。

```x86asm
# 保存上一个栈帧信息
push %ebp
mov  %esp, %ebp

# 恢复上一个栈帧的信息
leave
# 这个指令相当于
# mov %ebp, %esp
# pop %ebp
```

如果不需要生成 PIC，可以给 `gcc` 加上参数 `-fomit-frame-pointer`。

## 6. 代码分析

```makefile
/* tests/Makefile */

CFLAGS := -m32		# 生成 32 位的程序
CFLAGS += -Qn 		# 不生成 gcc 版本信息
CFLAGS += -fno-pic	# 不需要生成 PIC
CFLAGS += -fomit-frame-pointer 				# 不生产栈帧信息
CFLAGS += -mpreferred-stack-boundary=2		# 不需要进行栈对齐
CFLAGS += -fno-asynchronous-unwind-tables 	# 不需要生成 CFI
CFLAGS :=$(strip ${CFLAGS})

.PHONY: hello.s
hello.s: hello.c
	gcc $(CFLAGS) -S $< -o $@
```

```x86asm
/* tests/hello.s */

	.file	"hello.c" ; 文件名

.text # 代码段

.globl	msg     ; 声明 msg 为全局符号
.data           ; 数据段
	.align 4    ; 以 4 个字节对齐
	.type	msg, @object
	.size	msg, 15

msg:
	.string	"Hello, World!\n"

.globl	buf     ; 声明 buf 为全局符号
.bss            ; bss 段
	.align 32   ; 以 32 个字节对齐
	.type	buf, @object
	.size	buf, 1024

buf:
	.zero	1024

.text
.globl	main    ; 声明 main 为全局符号
	.type	main, @function

main:
	pushl	$msg        ; 将 msg 的地址压入栈中
	call	printf      ; 调用 printf
	addl	$4, %esp    ; 恢复栈指针
	movl	$0, %eax    ; 将函数返回值存入 eax
	ret                 ; 函数调用返回
	.size	main, .-main
	.section	.note.GNU-stack,"",@progbits ; 标记栈不可运行
```

## 7. 调试支持

gcc 天然支持调试 gcc 编译出来的汇编程序。但是为了能够调试汇编程序，需要在 `tasks.json` 中进行一些修改.

```json
/* .vscode/tasks.json */

{
    "type": "shell",
    "label": "C/C++: gcc build active file",
    "command": "/usr/bin/gcc",
	...
}
```

将 `cppbuild` 修改为 `shell` 即可，否则该 task 会认为汇编程序不是 C/C++ 程序而拒绝执行该 task。

## 8. 参考文献

- [DWARF - Wikipedia](https://en.wikipedia.org/wiki/DWARF)
- [Hardened/GNU stack quickstart](https://wiki.gentoo.org/wiki/Hardened/GNU_stack_quickstart)