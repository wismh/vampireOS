; BASE 0x400000; SYS_UPTIME (19) seconds from PIT ticks; write decimal
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
    mov eax, 19
    int 0x30
    mov ecx, out + 10
    mov byte [ecx], 0
    mov ebx, 10
    test eax, eax
    jnz conv
    dec ecx
    mov byte [ecx], '0'
    jmp write
conv:
    xor edx, edx
    div ebx
    add dl, '0'
    dec ecx
    mov [ecx], dl
    test eax, eax
    jnz conv
write:
    mov eax, 1
    mov edi, ecx
    int 0x30
.hang:
    mov eax, 3
    xor edi, edi
    int 0x30
    jmp .hang

out:
    times 12 db 0

filesize equ $ - $$
