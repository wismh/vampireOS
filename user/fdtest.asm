; BASE 0x400000; open hello five times so the fifth open returns fd 4
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
    xor ebx, ebx
opens:
    mov eax, 6
    mov edi, path
    int 0x30
    test rax, rax
    js fail
    add al, '0'
    mov [out + ebx], al
    inc ebx
    cmp ebx, 5
    jb opens
    cmp byte [out + 4], '4'
    jne fail
    ; Write "4" (out+4) so column 0 keeps the extra fd after exit's "done".
    mov eax, 1
    mov edi, out + 4
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
out:
    db "00000", 0
bad:
    db "X", 0

filesize equ $ - $$
