; BASE 0x400000; path at 0x401000 from shell; open/read/write/exit
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

USER_ARG equ 0x401000

_start:
    mov eax, 6
    mov edi, USER_ARG
    int 0x30
    test rax, rax
    js fail
    mov ebx, eax
    mov eax, 8
    mov edi, ebx
    mov esi, buf
    mov edx, 64
    int 0x30
    test rax, rax
    js fail
    mov ecx, eax
    mov eax, 1
    mov edi, 1
    mov esi, buf
    mov edx, ecx
    int 0x30
    mov eax, 7
    mov edi, ebx
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
buf:
    times 64 db 0

filesize equ $ - $$
