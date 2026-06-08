; fbinfo / fbpix / fbpresent stubs for fbhello. System V x86_64: arg in rdi; return in rax.
bits 64
section .text

global fbinfo
global fbpix
global fbpresent

fbinfo:
    mov eax, 24
    int 0x30
    ret

fbpix:
    mov eax, 25
    int 0x30
    ret

fbpresent:
    mov eax, 26
    int 0x30
    ret
