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

    mov si, kernel_dap
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    call detect_memory

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
    mov rax, KERNEL_PHYS
    jmp rax

err_msg:
    db "disk error", 0

boot_drive:
    db 0

    align 16
kernel_dap:
    db 0x10
    db 0
    dw KERNEL_SECTORS
    dw KERNEL_OFFSET
    dw KERNEL_SEGMENT
    dq KERNEL_LBA

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
