; BASE 0x400000; mmap 0x500000, munmap, write ok; unmapped VA write is -1
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
    mov rsi, 0x1000
    xor edx, edx
    int 0x30
    cmp rax, 0x500000
    jne fail
    mov eax, 23
    mov rdi, 0x500000
    mov rsi, 0x1000
    int 0x30
    test rax, rax
    jnz fail
    mov eax, 1
    mov edi, 1
    mov rsi, 0x500000
    mov edx, 1
    int 0x30
    cmp rax, -1
    jne fail
    mov eax, 1
    mov edi, ok
    int 0x30
    mov eax, 4
    mov edi, 300
    int 0x30
    mov byte [0x500000], "x"
    jmp fail
fail:
    mov eax, 1
    mov edi, bad
    int 0x30
    mov eax, 2
    xor edi, edi
    int 0x30

ok:
    db "ok", 0
bad:
    db "X", 0

filesize equ $ - $$
