; BASE 0x400000; open hello, lseek past 'bl', read, legacy write, exit
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
    mov eax, 6
    mov edi, path
    int 0x30
    test rax, rax
    js fail
    mov ebx, eax
    mov eax, 15
    mov edi, ebx
    mov esi, 2
    xor edx, edx
    int 0x30
    cmp eax, 2
    jne fail
    mov eax, 8
    mov edi, ebx
    mov esi, buf
    mov edx, 16
    int 0x30
    cmp eax, 3
    jne fail
    mov byte [buf + 3], 0
    mov eax, 1
    mov edi, buf
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

path:
    db "hello", 0
bad:
    db "X", 0
buf:
    times 32 db 0

filesize equ $ - $$
