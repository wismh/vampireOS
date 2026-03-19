; BASE 0x400000; brk, fork, parent and child store distinct heap bytes
bits 64
org 0x400000

ehdr:
    db 0x7F, "ELF", 2, 1, 1, 0
    times 8 db 0
    dw 2
    dw 62
    dd 1
    dq _start
    dq phdr - $$
    dq 0
    dd 0
    dw 64
    dw 56
    dw 1
    dw 0
    dw 0
    dw 0

phdr:
    dd 1
    dd 7
    dq 0
    dq $$
    dq $$
    dq filesize
    dq filesize
    dq 0x1000

_start:
    mov eax, 12
    xor edi, edi
    int 0x30
    test rax, rax
    js fail
    mov rdi, rax
    add rdi, 0x1000
    mov eax, 12
    int 0x30
    cmp rax, -1
    je fail
    sub rax, 0x1000
    mov [heap], rax
    mov byte [rax], "?"
    mov byte [rax + 1], 0

    mov eax, 13
    int 0x30
    test rax, rax
    js fail
    jz child

    mov rsi, pval
    mov rdi, [heap]
.pcopy:
    mov al, [rsi]
    mov [rdi], al
    inc rsi
    inc rdi
    test al, al
    jnz .pcopy
    mov ecx, 4
.y:
    mov eax, 3
    xor edi, edi
    int 0x30
    dec ecx
    jnz .y
    mov eax, 1
    mov rdi, [heap]
    int 0x30
.ploop:
    mov eax, 3
    xor edi, edi
    int 0x30
    jmp .ploop

child:
    mov rsi, cval
    mov rdi, [heap]
.ccopy:
    mov al, [rsi]
    mov [rdi], al
    inc rsi
    inc rdi
    test al, al
    jnz .ccopy
    mov ecx, 4
.cy:
    mov eax, 3
    xor edi, edi
    int 0x30
    dec ecx
    jnz .cy
    mov eax, 1
    mov rdi, [heap]
    int 0x30
.cloop:
    mov eax, 3
    xor edi, edi
    int 0x30
    jmp .cloop

fail:
    mov eax, 1
    mov edi, bad
    int 0x30
    mov eax, 2
    xor edi, edi
    int 0x30

heap:
    dq 0
pval:
    db "parent", 0
cval:
    db "child", 0
bad:
    db "X", 0

filesize equ $ - $$
