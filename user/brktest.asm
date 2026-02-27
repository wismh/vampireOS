; BASE 0x400000; brk past the stack page, store a byte, write it
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
    mov byte [rax], "b"
    mov byte [rax + 1], "r"
    mov byte [rax + 2], "k"
    mov byte [rax + 3], 0
    mov rdi, rax
    mov eax, 1
    int 0x30
    mov eax, 2
    xor edi, edi
    int 0x30
fail:
    mov eax, 1
    mov edi, bad
    int 0x30
    mov eax, 2
    xor edi, edi
    int 0x30

bad:
    db "X", 0

filesize equ $ - $$
