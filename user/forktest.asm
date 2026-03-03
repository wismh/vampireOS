; BASE 0x400000; fork, parent and child each write a distinct line
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
    mov eax, 13
    int 0x30
    test rax, rax
    js fail
    jz child
    mov eax, 1
    mov edi, pmsg
    int 0x30
.ploop:
    mov eax, 3
    xor edi, edi
    int 0x30
    jmp .ploop
child:
    mov eax, 1
    mov edi, cmsg
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

pmsg:
    db "parent", 0
cmsg:
    db "child", 0
bad:
    db "X", 0

filesize equ $ - $$
