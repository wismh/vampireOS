%include "const.inc"

bits 16
org 0

db "VRD1"
dd 3

db 5
db "hello"
db 0
dd 5
db "blood"

db 4
db "motd"
db 0
dd 5
db "night"

db 4
db "echo"
db 0
dd echo_len
echo_data:
    incbin "echo.bin"
echo_len equ $ - echo_data

times (INITRD_SECTORS * 512) - ($ - $$) db 0
