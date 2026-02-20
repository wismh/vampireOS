; BASE 0x400000; wait until rax is 42, write it, exit
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
    mov eax, 5
    int 0x30
    cmp eax, 42
    jne _start
    mov ecx, eax
    xor edx, edx
    mov ebx, 10
    mov eax, ecx
    div ebx
    add al, '0'
    add dl, '0'
    mov [digits], al
    mov [digits + 1], dl
    mov eax, 1
    mov edi, digits
    int 0x30
    mov eax, 2
    mov edi, 42
    int 0x30

digits:
    db "00", 0

filesize equ $ - $$
