%include "const.inc"

bits 16
org 0x7C00

jmp short start
nop

start:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    cld

    mov [boot_drive], dl

    mov si, dap
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    jmp STAGE2_SEGMENT:STAGE2_OFFSET

disk_error:
    mov si, err_msg
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

err_msg:
    db "disk error", 0

boot_drive:
    db 0

    align 16
dap:
    db 0x10
    db 0
    dw STAGE2_SECTORS
    dw STAGE2_OFFSET
    dw STAGE2_SEGMENT
    dq STAGE2_LBA

    times 446 - ($ - $$) db 0
    db 0x80
    db 0, 0, 1
    db PART_TYPE
    db 0, 0, 0
    dd PART_LBA
    dd FAT_TOTAL_SECS
    times 16 * 3 db 0
    dw 0xAA55
