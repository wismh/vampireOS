; BASE 0x404000; stack page at BASE+0x1000
bits 64
org 0x404000

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
    mov ecx, 8
.loop:
    mov eax, 1
    mov edi, str
    int 0x30
    dec ecx
    jz .done
    mov eax, 4
    mov edi, 5
    int 0x30
    jmp .loop
.done:
    mov eax, 2
    int 0x30

str:
    db "C", 0

filesize equ $ - $$
