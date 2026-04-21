; BASE 0x400000; write argv[1] (or "E") to fd 1, exit
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
    cmp qword [rsp], 2
    jb noarg
    mov rsi, [rsp+16]
    test rsi, rsi
    jz noarg
    xor ecx, ecx
.len:
    cmp byte [rsi + rcx], 0
    je .got
    inc ecx
    cmp ecx, 80
    jb .len
.got:
    test ecx, ecx
    jz noarg
    mov eax, 1
    mov edi, 1
    mov edx, ecx
    int 0x30
    jmp done
noarg:
    mov eax, 1
    mov edi, 1
    mov esi, str
    mov edx, 1
    int 0x30
done:
    mov eax, 2
    xor edi, edi
    int 0x30

str:
    db "E"

filesize equ $ - $$
