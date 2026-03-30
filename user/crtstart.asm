; Tiny caller linked with crt.asm. Calls write (not open-coded int 0x30).
; BASE 0x400000; `run crt` prints the line on VGA. Hang so exit's "done"
; at column 2 does not stomp the text.
bits 64
section .text

global _start
extern write

_start:
    mov edi, 1
    lea rsi, [rel msg]
    mov edx, msg_len
    call write
.hang:
    jmp .hang

msg:
    db "crt"
msg_len equ $ - msg
