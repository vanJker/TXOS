	.file	"hello.c"
	.text
	.globl	msg
	.data
	.align 4
	.type	msg, @object
	.size	msg, 15
msg:
	.string	"Hello, World!\n"
	.globl	buf
	.bss
	.align 32
	.type	buf, @object
	.size	buf, 1024
buf:
	.zero	1024
	.text
	.globl	main
	.type	main, @function
main:
	pushl	$msg
	call	printf
	addl	$4, %esp
	movl	$0, %eax
	ret
	.size	main, .-main
	.section	.note.GNU-stack,"",@progbits
