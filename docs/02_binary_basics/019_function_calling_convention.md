# 019 函数调用约定

函数调用约定主要包括局部变量和参数传递，上一节已经初步介绍了函数调用约定，这一节将进一步详细说明。

## 1. 代码分析

编写一个简单的函数调用程序：

```c
/* tests/params.c */

int add(int x, int y) {
    int z = x + y;
    return z;
}

int main(int argc, char *argv[]) {
    int a = 5;
    int b = 3;
    int c = add(a, b);
    return 0;
}
```

在 `Makefile` 中配置生成栈帧信息：

```makefile
/* tests/Makefile */

CFLAGS := -m32		# 生成 32 位的程序
CFLAGS += -Qn 		# 不生成 gcc 版本信息
CFLAGS += -fno-pic	# 不需要生成 PIC
# CFLAGS += -fomit-frame-pointer 				# 不生产栈帧信息
CFLAGS += -mpreferred-stack-boundary=2		# 不需要进行栈对齐
CFLAGS += -fno-asynchronous-unwind-tables 	# 不需要生成 CFI
CFLAGS :=$(strip ${CFLAGS})

.PHONY: params.s
params.s: params.c
	gcc $(CFLAGS) -S $< -o $@
```

对生成的汇编程序进行分析：

```x86asm
/* tests/params.s */

	.file	"params.c"
	.text

	.globl	add
	.type	add, @function
add:
    ; 保存栈帧
	pushl	%ebp
	movl	%esp, %ebp

    ; 栈分配 4 个字节，保存 1 个局部变量 z
	subl	$4, %esp 
	movl	8(%ebp), %edx  ; 获取参数 a
	movl	12(%ebp), %eax ; 获取参数 b
	addl	%edx, %eax     ; eax += edx
	movl	%eax, -4(%ebp) ; 保存 z = x + y
    ; 将函数返回值保存在寄存器 eax 中
	movl	-4(%ebp), %eax

	leave ; 恢复栈帧
	ret   ; 函数返回
	.size	add, .-add

	.globl	main
	.type	main, @function
main:
    ; 保存栈帧
	pushl	%ebp
	movl	%esp, %ebp

    ; 栈分配 12 个字节，保存 3 个局部变量 a, b, c
	subl	$12, %esp   
	movl	$5, -12(%ebp)  ; 保存 a = 5
	movl	$3, -8(%ebp)   ; 保存 b = 3
	pushl	-8(%ebp)       ; 传递参数 b
	pushl	-12(%ebp)      ; 传递参数 a
	call	add            ; 调用函数 add
    ; 恢复栈为函数调用前，栈中不保存参数
	addl	$8, %esp
	movl	%eax, -4(%ebp) ; 保存 c = add(a, b)
    ; 将函数返回值保存在寄存器 eax 中
	movl	$0, %eax

	leave ; 恢复栈帧
	ret   ; 函数返回
	.size	main, .-main

	.section	.note.GNU-stack,"",@progbits
```

## 2. 栈帧信息

栈帧保存函数局部变量的信息，可以用于回溯调用函数。如果在 `gcc` 的参数加入 `-fomit-frame-pointer `，即不产生栈帧信息，那么在调试时，无法在调用堆栈选项那里观察到函数调用情况。

根据调试（使用调试选项 `gcc - Build and debug active file`），可以得知遵循 x86 函数调用约定的栈帧如下：

| 函数调用栈帧 |
| :------------: |
| **栈帧 1**      |
| 局部变量若干     |
| 函数调用参数若干 |
| return address |
| ebp            |
| **栈帧 2**      |
| 局部变量若干     |
| ...            |

其中栈帧 1 中的 return address 为栈帧 2 对应函数返回时一个跳转到的地址，ebp 为栈帧 1 的基地址，ebp 由栈帧 2 对应的函数保存在栈帧 1 中。

保存函数调用参数部分，如果函数原型为 `f(a, b, c)`，则 `a` 的地址位于最低端，`b` 地址位于中间，`c` 地址位于最高端。

```
| c | <--- 高地址 
| b |
| a | <--- 低地址 
```

## 3. 寄存器传递参数

除了上面介绍的栈传递参数之外，还有另一种传递参数的方式，寄存器传递参数。

在 8 个通用寄存器中，我们可以自由使用其中的 6 个寄存器：`eax ecx edx ebx esi edi`，这导致了寄存器传递参数方式只能支持有限个参数传递。所以类似于 `printf` 这种支持多参数的函数调用，无法使用寄存器来传递参数。

而寄存器传递参数一般用于实现系统调用，这是因为系统调用参数十分有限，可以通过寄存器来传递。另一方面，由于内核和用户的内存隔离，无法很便捷的通过栈/内存来传递参数，只能通过调用约定和寄存器来传递参数。
