; BASE 0x400000; mmap, mprotect RO, write ro; store #PF-kills
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
    mov byte [rax], "A"
    mov eax, 28
    mov rdi, 0x500000
    mov rsi, 0x1000
    mov edx, 1
    int 0x30
    test rax, rax
    jnz fail
    mov eax, 1
    mov edi, ok
    int 0x30
    mov eax, 4
    mov edi, 50
    int 0x30
    mov byte [0x500000], "X"
    jmp fail
fail:
    mov eax, 1
    mov edi, bad
    int 0x30
    mov eax, 2
    xor edi, edi
    int 0x30

ok:
    db "ro", 0
bad:
    db "X", 0

filesize equ $ - $$
