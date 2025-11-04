bits 16
org 0x7C00

start:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    cld

    mov si, msg
.print:
    lodsb
    test al, al
    jz .hang
    mov ah, 0x0E
    mov bh, 0
    mov bl, 0x07
    int 0x10
    jmp .print

.hang:
    cli
    hlt
    jmp .hang

msg:
    db "Vampire OS", 0

    times 510 - ($ - $$) db 0
    dw 0xAA55
