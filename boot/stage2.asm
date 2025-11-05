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
    cli

    ; Fast A20 (port 0x92): set bit 1, keep bit 0 clear (reset)
    in al, 0x92
    or al, 2
    and al, 0xFE
    out 0x92, al

    lgdt [gdt_ptr]

    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:pm32

bits 32
pm32:
    mov ax, 0x18
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, 0x7C00

    ; Zero PML4, PDPT, PD (3 pages)
    mov edi, PML4
    xor eax, eax
    mov ecx, (3 * 4096) / 4
    rep stosd

    ; Identity-map first 2 MiB with a huge page
    mov dword [PML4], PDPT | 0x03
    mov dword [PDPT], PD | 0x03
    mov dword [PD], 0x83            ; present | rw | PS

    mov eax, cr4
    or eax, 1 << 5                  ; PAE
    mov cr4, eax

    mov eax, PML4
    mov cr3, eax

    mov ecx, 0xC0000080             ; IA32_EFER
    rdmsr
    or eax, 1 << 8                  ; LME
    wrmsr

    mov eax, cr0
    or eax, 1 << 31                 ; PG
    mov cr0, eax
    jmp 0x10:lm64

bits 64
lm64:
    mov ax, 0x18
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov rsp, 0x7C00

    mov rdi, 0xB8000
    mov rcx, 80 * 25
    mov ax, 0x0F20
    rep stosw

    mov rdi, 0xB8000
    mov rsi, msg
    mov ah, 0x0F
.print:
    lodsb
    test al, al
    jz .hang
    stosw
    jmp .print

.hang:
    cli
    hlt
    jmp .hang

msg:
    db "Vampire OS", 0

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
