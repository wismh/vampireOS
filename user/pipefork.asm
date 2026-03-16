; BASE 0x400000; pipe, fork; child writes, parent reads, VGA fd 1. No kernel |
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
    mov eax, 11
    mov edi, fds
    int 0x30
    test rax, rax
    js fail

    mov eax, 13
    int 0x30
    test rax, rax
    js fail
    jz child
    mov [pid], rax

    ; Parent: drop the write end so fd 1 is VGA again, then read the ring.
    mov eax, 7
    mov edi, [fds + 4]
    int 0x30
    test rax, rax
    js fail

    mov eax, 8
    mov edi, [fds]
    mov esi, buf
    mov edx, 16
    int 0x30
    cmp eax, 5
    jne fail
    mov ebx, eax

    mov eax, 1
    mov edi, 1
    mov esi, buf
    mov edx, ebx
    int 0x30
    cmp eax, 5
    jne fail

    mov eax, 5
    mov rdi, [pid]
    int 0x30
    test rax, rax
    jnz fail

    mov eax, 2
    xor edi, edi
    int 0x30

child:
    mov eax, 7
    mov edi, [fds]
    int 0x30
    test rax, rax
    js fail

    mov eax, 1
    mov edi, [fds + 4]
    mov esi, msg
    mov edx, 5
    int 0x30
    cmp eax, 5
    jne fail

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

fds:
    dd 0
    dd 0
pid:
    dq 0
msg:
    db "child"
bad:
    db "X", 0
buf:
    times 16 db 0

filesize equ $ - $$
