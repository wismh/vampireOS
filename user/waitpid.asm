; BASE 0x400000; fork two children (exit 1 and 2); wait each pid
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
    jz child1
    mov [pid1], rax

    mov eax, 13
    int 0x30
    test rax, rax
    js fail
    jz child2
    mov [pid2], rax

    ; Reap pid2 first so rdi must select, not the first DEAD slot.
    mov eax, 5
    mov rdi, [pid2]
    int 0x30
    cmp rax, 2
    jne fail
    add al, '0'
    mov [out + 1], al

    mov eax, 5
    mov rdi, [pid1]
    int 0x30
    cmp rax, 1
    jne fail
    add al, '0'
    mov [out], al

    mov eax, 1
    mov edi, out
    int 0x30
    mov eax, 2
    xor edi, edi
    int 0x30

child1:
    mov eax, 2
    mov edi, 1
    int 0x30

child2:
    mov eax, 2
    mov edi, 2
    int 0x30

fail:
    mov eax, 1
    mov edi, bad
    int 0x30
    mov eax, 2
    xor edi, edi
    int 0x30

pid1:
    dq 0
pid2:
    dq 0
out:
    db "00", 0
bad:
    db "X", 0

filesize equ $ - $$
