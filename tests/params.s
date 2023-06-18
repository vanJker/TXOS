	.file	"params.c"
	.text
	.globl	add
	.type	add, @function
add:
	subl	$4, %esp
	movl	8(%esp), %edx
	movl	12(%esp), %eax
	addl	%edx, %eax
	movl	%eax, (%esp)
	movl	(%esp), %eax
	addl	$4, %esp
	ret
	.size	add, .-add
	.globl	main
	.type	main, @function
main:
	subl	$12, %esp
	movl	$5, (%esp)
	movl	$3, 4(%esp)
	pushl	4(%esp)
	pushl	4(%esp)
	call	add
	addl	$8, %esp
	movl	%eax, 8(%esp)
	movl	$0, %eax
	addl	$12, %esp
	ret
	.size	main, .-main
	.section	.note.GNU-stack,"",@progbits
