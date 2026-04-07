; C start: argc/argv from the user stack, call main, then hang so exit's
; "done" at column 2 does not stomp.
bits 64
section .text

global _start
extern main

_start:
    xor ebp, ebp
    mov rdi, [rsp]
    lea rsi, [rsp+8]
    and rsp, ~0xF
    call main
.hang:
    jmp .hang
