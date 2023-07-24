
global task_switch
task_switch:
    push ebp
    mov ebp, esp

    push ebx
    push esi
    push edi

    mov eax, esp
    and eax, 0xfffff000 ; 获得 current task

    mov [eax], esp

    mov eax, [ebp + 8] ; 获得函数参数 next
    mov esp, [eax] ; 切换内核栈

    pop edi
    pop esi
    pop ebx
    pop ebp

    ret

