bits 64
default rel

extern exception_handler
extern irq_handler
extern syscall_handler

KERNEL_DS equ 0x18

%macro ISR_NOERR 1
global isr%1
isr%1:
    push qword 0
    push qword %1
    jmp isr_common
%endmacro

%macro ISR_ERR 1
global isr%1
isr%1:
    push qword %1
    jmp isr_common
%endmacro

%macro ISR_IRQ 1
global isr%1
isr%1:
    push qword 0
    push qword %1
    jmp irq_common
%endmacro

%macro ISR_SYSCALL 1
global isr%1
isr%1:
    push qword 0
    push qword %1
    jmp syscall_common
%endmacro

%macro PUSH_REGS 0
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rbp
    push rsi
    push rdi
    push rdx
    push rcx
    push rbx
    push rax
%endmacro

%macro POP_REGS 0
    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rdi
    pop rsi
    pop rbp
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
%endmacro

%assign vec 0
%rep 32
    %if vec == 8 || vec == 10 || vec == 11 || vec == 12 || vec == 13 || vec == 14 || vec == 17 || vec == 21
        ISR_ERR vec
    %else
        ISR_NOERR vec
    %endif
    %assign vec vec + 1
%endrep

%assign vec 32
%rep 16
    ISR_IRQ vec
    %assign vec vec + 1
%endrep

ISR_SYSCALL 48

isr_common:
    PUSH_REGS
    mov ax, KERNEL_DS
    mov ds, ax
    mov es, ax
    mov rdi, rsp
    cld
    call exception_handler
.hang:
    cli
    hlt
    jmp .hang

irq_common:
    PUSH_REGS
    mov ax, KERNEL_DS
    mov ds, ax
    mov es, ax
    mov rdi, rsp
    cld
    call irq_handler
    POP_REGS
    add rsp, 16
    iretq

syscall_common:
    PUSH_REGS
    mov ax, KERNEL_DS
    mov ds, ax
    mov es, ax
    mov rdi, rsp
    cld
    call syscall_handler
    POP_REGS
    add rsp, 16
    iretq

global isr_stubs
align 8
isr_stubs:
%assign vec 0
%rep 49
    dq isr %+ vec
    %assign vec vec + 1
%endrep
