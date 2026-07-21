; BASE 0x400000; SYS_DATE (29) CMOS RTC into buf; write `YYYY-MM-DD HH:MM:SS`
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
    mov eax, 29
    mov edi, buf
    mov esi, 20
    int 0x30
    test rax, rax
    js fail
    mov ecx, eax
    mov eax, 1
    mov edi, 1
    mov esi, buf
    mov edx, ecx
    int 0x30
    mov eax, 2
    xor edi, edi
    int 0x30
fail:
    mov eax, 1
    mov edi, 2
    mov esi, bad
    mov edx, 1
    int 0x30
    mov eax, 2
    xor edi, edi
    int 0x30

bad:
    db "?"
buf:
    times 20 db 0

filesize equ $ - $$
