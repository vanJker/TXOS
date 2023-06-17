[bits 32]

extern exit

test:
    ; 测试单独使用 ret 指令
    ; push $
    ret

global main
main:
    ; 测试基本的 push 和 pop 指令
    ; push 5
    ; push eax

    ; pop ebx
    ; pop ecx

    ; 测试 pusha 和 popa 指令
    ; pusha
    ; popa

    ; 测试 call 和 ret 指令联合使用
    call test

    push 0 ; 传递参数
    call exit