[org 0x1000]

dw 0x55aa   ; 魔数，用于 boot 来检测加载 loader 是否正确

; 打印 loading 信息
mov si, loading
call print

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

loading:
    db "Loading LoongOS...", 10, 13, 0  ; 在 ASCII 编码中，13:\n, 10:\r, 0:\0