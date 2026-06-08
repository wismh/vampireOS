; BASE 0x400000; SYS_FBINFO then SYS_FBPIX full-rect fill; SYS_FBPRESENT; write ok
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
    mov eax, 24
    mov edi, info
    int 0x30
    test rax, rax
    js fail
    mov eax, [info]
    mov [rect + 8], eax
    mov eax, [info + 4]
    mov [rect + 12], eax
    mov dword [rect + 16], 0x001E6FA8
    mov eax, 25
    mov edi, rect
    int 0x30
    test rax, rax
    js fail
    mov eax, 26
    int 0x30
    test rax, rax
    js fail
    mov eax, 1
    mov edi, ok
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

info:
    dd 0
    dd 0
    dd 0
    dd 0
rect:
    dd 0
    dd 0
    dd 0
    dd 0
    dd 0
ok:
    db "ok", 0
bad:
    db "X", 0

filesize equ $ - $$
