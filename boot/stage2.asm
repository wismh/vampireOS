%include "const.inc"

bits 16
org STAGE2_OFFSET

start:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    cld

    mov [boot_drive], dl

    call serial_boot

    mov si, kernel_dap
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    mov si, kernel_dap2
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    call detect_memory
    call set_vbe

    cli

    in al, 0x92
    or al, 2
    and al, 0xFE
    out 0x92, al

    lgdt [gdt_ptr]

    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:pm32

detect_memory:
    xor ebx, ebx
    mov di, E820_BASE + 8
    mov dword [E820_BASE], 0
    mov dword [E820_BASE + 4], E820_ENTRY_SIZE

.next:
    mov eax, 0xE820
    mov edx, 0x534D4150
    mov ecx, E820_ENTRY_SIZE
    mov dword [es:di + 20], 1
    int 0x15
    jc .done
    cmp eax, 0x534D4150
    jne .fail

    cmp cl, 20
    jb .skip
    cmp cl, 24
    jb .use
    test byte [es:di + 20], 1
    jz .skip

.use:
    mov eax, [es:di + 8]
    or eax, [es:di + 12]
    jz .skip

    add di, E820_ENTRY_SIZE
    inc dword [E820_BASE]
    cmp dword [E820_BASE], E820_MAX
    jae .done

.skip:
    test ebx, ebx
    jnz .next
    jmp .done

.fail:
    mov dword [E820_BASE], 0
.done:
    ret

; Prefer 640x480x32, then 800x600x32, then 24 bpp. Leave FB_INFO magic 0 on fail.
set_vbe:
    xor ax, ax
    mov ds, ax
    mov es, ax

    mov di, FB_INFO
    mov cx, 12
    xor ax, ax
    rep stosw

    mov di, VBE_INFO_BUF
    mov cx, 256
    xor ax, ax
    rep stosw
    mov di, VBE_INFO_BUF
    mov dword [di], 'VBE2'
    mov ax, 0x4F00
    int 0x10
    cmp ax, 0x004F
    jne .done
    cmp dword [VBE_INFO_BUF], 'VESA'
    jne .done

    mov word [want_w], 640
    mov word [want_h], 480
    mov byte [want_bpp], 32
    call vbe_try
    jnc .done
    mov word [want_w], 800
    mov word [want_h], 600
    mov byte [want_bpp], 32
    call vbe_try
    jnc .done
    mov word [want_w], 640
    mov word [want_h], 480
    mov byte [want_bpp], 24
    call vbe_try
    jnc .done
    mov word [want_w], 800
    mov word [want_h], 600
    mov byte [want_bpp], 24
    call vbe_try

.done:
    xor ax, ax
    mov ds, ax
    mov es, ax
    ret

vbe_try:
    mov ax, [VBE_INFO_BUF + 14]
    mov [ml_off], ax
    mov ax, [VBE_INFO_BUF + 16]
    mov [ml_seg], ax

.scan:
    xor ax, ax
    mov ds, ax
    mov ax, [ml_seg]
    mov es, ax
    mov si, [ml_off]
    mov cx, [es:si]
    add word [ml_off], 2
    cmp cx, 0xFFFF
    je .fail
    mov [cur_mode], cx

    xor ax, ax
    mov es, ax
    mov di, VBE_MODE_BUF
    mov ax, 0x4F01
    int 0x10
    cmp ax, 0x004F
    jne .scan

    mov ax, [VBE_MODE_BUF]
    and ax, 0x0091
    cmp ax, 0x0091
    jne .scan

    mov al, [VBE_MODE_BUF + 27]
    cmp al, 4
    je .geom
    cmp al, 6
    jne .scan

.geom:
    mov ax, [VBE_MODE_BUF + 18]
    cmp ax, [want_w]
    jne .scan
    mov ax, [VBE_MODE_BUF + 20]
    cmp ax, [want_h]
    jne .scan
    mov al, [VBE_MODE_BUF + 25]
    cmp al, [want_bpp]
    jne .scan

    mov eax, [VBE_MODE_BUF + 40]
    test eax, eax
    jz .scan

    mov bx, [cur_mode]
    or bx, 0x4000
    mov ax, 0x4F02
    int 0x10
    cmp ax, 0x004F
    jne .fail

    xor ax, ax
    mov ds, ax
    mov es, ax
    mov dword [FB_INFO], FB_MAGIC
    movzx eax, word [want_w]
    mov [FB_INFO + 4], eax
    movzx eax, word [want_h]
    mov [FB_INFO + 8], eax
    movzx eax, word [VBE_MODE_BUF + 16]
    movzx ebx, word [VBE_MODE_BUF + 50]
    test bx, bx
    jz .store_pitch
    mov eax, ebx
.store_pitch:
    mov [FB_INFO + 12], eax
    movzx eax, byte [want_bpp]
    mov [FB_INFO + 16], eax
    mov eax, [VBE_MODE_BUF + 40]
    mov [FB_INFO + 20], eax
    clc
    ret

.fail:
    xor ax, ax
    mov ds, ax
    mov es, ax
    stc
    ret

want_w:
    dw 0
want_h:
    dw 0
want_bpp:
    db 0
    db 0
cur_mode:
    dw 0
ml_off:
    dw 0
ml_seg:
    dw 0

; 16550 COM1 at 0x3F8, 115200 8N1. Print "boot" so a hang before VGA
; still shows on QEMU -serial stdio / -serial file.
serial_boot:
    mov dx, 0x3F9
    xor al, al
    out dx, al
    mov dx, 0x3FB
    mov al, 0x80
    out dx, al
    mov dx, 0x3F8
    mov al, 0x01
    out dx, al
    mov dx, 0x3F9
    xor al, al
    out dx, al
    mov dx, 0x3FB
    mov al, 0x03
    out dx, al
    mov dx, 0x3FA
    mov al, 0x07
    out dx, al
    mov dx, 0x3FC
    mov al, 0x0B
    out dx, al
    mov dx, 0x3FF
    mov al, 0xAE
    out dx, al
    in al, dx
    cmp al, 0xAE
    jne .done
    xor al, al
    out dx, al
    mov si, boot_msg
.print:
    lodsb
    test al, al
    jz .done
    call serial_putc
    jmp .print
.done:
    ret

serial_putc:
    push ax
    push cx
    push dx
    mov cx, 0xFFFF
    mov dx, 0x3FD
.wait:
    in al, dx
    test al, 0x20
    jnz .ready
    loop .wait
.ready:
    pop dx
    pop cx
    pop ax
    mov dx, 0x3F8
    out dx, al
    ret

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

bits 32
pm32:
    mov ax, 0x18
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, 0x7C00

    mov edi, PML4
    xor eax, eax
    mov ecx, (3 * 4096) / 4
    rep stosd

    mov dword [PML4], PDPT | 0x03
    mov dword [PDPT], PD | 0x03
    mov dword [PD], 0x83

    mov edi, HIGH_PDPT
    xor eax, eax
    mov ecx, (2 * 4096) / 4
    rep stosd

    mov dword [PML4 + 511 * 8], HIGH_PDPT | 0x03
    mov dword [HIGH_PDPT + 510 * 8], HIGH_PD | 0x03
    mov dword [HIGH_PD], 0x83

    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    mov eax, PML4
    mov cr3, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax
    jmp 0x10:lm64

bits 64
lm64:
    mov ax, 0x18
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov rsp, 0x7C00

    mov rsi, KERNEL_TEMP
    mov rdi, KERNEL_PHYS
    mov rcx, KERNEL_SECTORS * 512
    rep movsb

    mov rdi, E820_BASE
    mov rax, KERNEL_VIRT
    jmp rax

err_msg:
    db "disk error", 0

boot_msg:
    db "boot", 13, 10, 0

boot_drive:
    db 0

    align 16
kernel_dap:
    db 0x10
    db 0
    dw KERNEL_READ1
    dw KERNEL_OFFSET
    dw KERNEL_SEGMENT
    dq KERNEL_LBA

kernel_dap2:
    db 0x10
    db 0
    dw KERNEL_READ2
    dw KERNEL_OFFSET
    dw KERNEL_SEGMENT2
    dq KERNEL_LBA2

align 8
gdt:
    dq 0
.code32:
    dw 0xFFFF
    dw 0
    db 0
    db 10011010b
    db 11001111b
    db 0
.code64:
    dw 0
    dw 0
    db 0
    db 10011010b
    db 00100000b
    db 0
.data:
    dw 0xFFFF
    dw 0
    db 0
    db 10010010b
    db 11001111b
    db 0
gdt_end:

gdt_ptr:
    dw gdt_end - gdt - 1
    dd gdt

    times (STAGE2_SECTORS * 512) - ($ - $$) db 0
