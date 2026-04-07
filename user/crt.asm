; C-callable syscall stubs. System V x86_64: args in rdi, rsi, rdx; return in rax.
; Each wrapper loads the syscall number and issues int 0x30.
; write / read / exit / fork / brk / wait / exec.
bits 64
section .text

global write
global read
global exit
global fork
global brk
global wait
global exec

write:
    mov eax, 1
    int 0x30
    ret

read:
    mov eax, 8
    int 0x30
    ret

exit:
    mov eax, 2
    int 0x30
    ret

fork:
    mov eax, 13
    int 0x30
    ret

brk:
    mov eax, 12
    int 0x30
    ret

; `wait` is an x87 mnemonic; `$wait` is the C symbol.
$wait:
    mov eax, 5
    int 0x30
    ret

exec:
    mov eax, 10
    int 0x30
    ret
