; C start: call main, then hang so exit's "done" at column 2 does not stomp.
bits 64
section .text

global _start
extern main

_start:
    xor ebp, ebp
    call main
.hang:
    jmp .hang
