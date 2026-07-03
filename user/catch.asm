; BASE 0x400000; SYS_SIGACTION SIGINT then sleep; handler writes caught once
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
    mov eax, 27
    mov edi, 2
    lea rsi, [rel handler]
    int 0x30
.loop:
    mov eax, 4
    mov edi, 100000
    int 0x30
    jmp .loop

handler:
    mov eax, 1
    mov edi, 1
    lea rsi, [rel msg]
    mov edx, 6
    int 0x30
    ret

msg:
    db "caught"

filesize equ $ - $$
