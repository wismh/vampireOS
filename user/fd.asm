; open / close / dup2 / pipe stubs for sh redirects and nested `|`.
; Linked only into sh. System V x86_64: args in rdi, rsi; return in rax.
bits 64
section .text

global open
global close
global dup2
global pipe

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
