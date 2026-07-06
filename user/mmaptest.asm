; BASE 0x400000; SYS_MMAP 3 pages at 0x500000, store on last, write it
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
    mov eax, 18
    mov rdi, 0x500000
    mov rsi, 0x3000
    xor edx, edx
    int 0x30
    cmp rax, 0x500000
    jne fail
    lea rbx, [rax + 0x2000]
    mov byte [rbx], "m"
    mov byte [rbx + 1], "m"
    mov byte [rbx + 2], "a"
    mov byte [rbx + 3], "p"
    mov byte [rbx + 4], 0
    mov rdi, rbx
    mov eax, 1
    int 0x30
.hang:
    mov eax, 3
    xor edi, edi
    int 0x30
    jmp .hang
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
