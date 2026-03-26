; BASE 0x400000; argv[1] pid; SYS_KILL rdi=pid rsi=status; exit
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
    jb fail
    mov rsi, [rsp+16]
    test rsi, rsi
    jz fail
    xor edi, edi
.parse:
    movzx eax, byte [rsi]
    test al, al
    jz .got
    cmp al, '0'
    jb fail
    cmp al, '9'
    ja fail
    sub al, '0'
    imul edi, edi, 10
    add edi, eax
    inc rsi
    jmp .parse
.got:
    mov eax, 17
    xor esi, esi
    int 0x30
    test rax, rax
    js fail
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

filesize equ $ - $$
