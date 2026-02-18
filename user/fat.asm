%include "const.inc"

bits 16
org 0

fat1:
    db 0xF8, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    times 512 - ($ - fat1) db 0

fat2:
    db 0xF8, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    times 512 - ($ - fat2) db 0

root:
    db "HELLO   ", "   "
    db 0x20
    times 14 db 0
    dw 2
    dd 5

    db "MOTD    ", "   "
    db 0x20
    times 14 db 0
    dw 3
    dd 5

    db "ECHO    ", "   "
    db 0x20
    times 14 db 0
    dw 4
    dd echo_len

    db "A       ", "   "
    db 0x20
    times 14 db 0
    dw 5
    dd a_len

    db "B       ", "   "
    db 0x20
    times 14 db 0
    dw 6
    dd b_len

    db "C       ", "   "
    db 0x20
    times 14 db 0
    dw 7
    dd c_len

    db "OPENTEST", "   "
    db 0x20
    times 14 db 0
    dw 8
    dd opentest_len

    db "READTEST", "   "
    db 0x20
    times 14 db 0
    dw 9
    dd readtest_len

    db "CAT     ", "   "
    db 0x20
    times 14 db 0
    dw 10
    dd cat_len

    db "LS      ", "   "
    db 0x20
    times 14 db 0
    dw 11
    dd ls_len

    times 512 - ($ - root) db 0

cluster2:
    db "blood"
    times 512 - ($ - cluster2) db 0

cluster3:
    db "night"
    times 512 - ($ - cluster3) db 0

echo_data:
    incbin "echo.bin"
echo_len equ $ - echo_data
    times 512 - echo_len db 0

a_data:
    incbin "a.bin"
a_len equ $ - a_data
    times 512 - a_len db 0

b_data:
    incbin "b.bin"
b_len equ $ - b_data
    times 512 - b_len db 0

c_data:
    incbin "c.bin"
c_len equ $ - c_data
    times 512 - c_len db 0

opentest_data:
    incbin "opentest.bin"
opentest_len equ $ - opentest_data
    times 512 - opentest_len db 0

readtest_data:
    incbin "readtest.bin"
readtest_len equ $ - readtest_data
    times 512 - readtest_len db 0

cat_data:
    incbin "cat.bin"
cat_len equ $ - cat_data
    times 512 - cat_len db 0

ls_data:
    incbin "ls.bin"
ls_len equ $ - ls_data
    times 512 - ls_len db 0

    times (FAT_DATA_CLUSTERS - 10) * 512 db 0
