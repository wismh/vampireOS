; open / close / dup2 / pipe / chdir / getcwd stubs for sh.
; Linked only into sh. System V x86_64: args in rdi, rsi; return in rax.
bits 64
section .text

global open
global close
global dup2
global pipe
global chdir
global getcwd

open:
    mov eax, 6
    int 0x30
    ret

close:
    mov eax, 7
    int 0x30
    ret

dup2:
    mov eax, 14
    int 0x30
    ret

pipe:
    mov eax, 11
    int 0x30
    ret

chdir:
    mov eax, 20
    int 0x30
    ret

getcwd:
    mov eax, 21
    int 0x30
    ret
