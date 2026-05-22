; BASE 0x400000; SYS_FBINFO (24) packed width/height/pitch/phys; write WxH
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
    mov ecx, out + 20
    mov byte [ecx], 0
    mov ebx, 10
    mov eax, [info + 4]
    test eax, eax
    jnz height
    dec ecx
    mov byte [ecx], '0'
    jmp xmark
height:
    xor edx, edx
    div ebx
    add dl, '0'
    dec ecx
    mov [ecx], dl
    test eax, eax
    jnz height
xmark:
    dec ecx
    mov byte [ecx], 'x'
    mov eax, [info]
    test eax, eax
    jnz width
    dec ecx
    mov byte [ecx], '0'
    jmp write
width:
    xor edx, edx
    div ebx
    add dl, '0'
    dec ecx
    mov [ecx], dl
    test eax, eax
    jnz width
write:
    mov eax, 1
    mov edi, ecx
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
bad:
    db "X", 0
out:
    times 24 db 0

filesize equ $ - $$
