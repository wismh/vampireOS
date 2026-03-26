%include "const.inc"

bits 16
org 0

fat1:
    db 0xF8, 0x2F, 0x01
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0x0F
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0x0F, 0x00
    times FAT_SEC_PER_FAT * 512 - ($ - fat1) db 0

fat2:
    db 0xF8, 0x2F, 0x01
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0x0F
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0x0F, 0x00
    times FAT_SEC_PER_FAT * 512 - ($ - fat2) db 0

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

    db "EXECTEST", "   "
    db 0x20
    times 14 db 0
    dw 12
    dd exectest_len

    db "STATUS  ", "   "
    db 0x20
    times 14 db 0
    dw 13
    dd status_len

    db "WAITER  ", "   "
    db 0x20
    times 14 db 0
    dw 14
    dd waiter_len

    db "PIPETEST", "   "
    db 0x20
    times 14 db 0
    dw 15
    dd pipetest_len

    db "BRKTEST ", "   "
    db 0x20
    times 14 db 0
    dw 16
    dd brktest_len

    db "FORKTEST", "   "
    db 0x20
    times 14 db 0
    dw 17
    dd forktest_len

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

exectest_data:
    incbin "exectest.bin"
exectest_len equ $ - exectest_data
    times 512 - exectest_len db 0

status_data:
    incbin "status.bin"
status_len equ $ - status_data
    times 512 - status_len db 0

waiter_data:
    incbin "waiter.bin"
waiter_len equ $ - waiter_data
    times 512 - waiter_len db 0

pipetest_data:
    incbin "pipetest.bin"
pipetest_len equ $ - pipetest_data
    times 512 - pipetest_len db 0

brktest_data:
    incbin "brktest.bin"
brktest_len equ $ - brktest_data
    times 512 - brktest_len db 0

forktest_data:
    incbin "forktest.bin"
forktest_len equ $ - forktest_data
    times 512 - forktest_len db 0

root_extra:
    db "DUP2TEST", "   "
    db 0x20
    times 14 db 0
    dw 19
    dd dup2test_len
    db "WAITPID ", "   "
    db 0x20
    times 14 db 0
    dw 20
    dd waitpid_len
    db "FDTEST  ", "   "
    db 0x20
    times 14 db 0
    dw 21
    dd fdtest_len
    db "PIPEFORK", "   "
    db 0x20
    times 14 db 0
    dw 22
    dd pipefork_len
    db "COWTEST ", "   "
    db 0x20
    times 14 db 0
    dw 23
    dd cowtest_len
    db "STAT    ", "   "
    db 0x20
    times 14 db 0
    dw 24
    dd stat_len
    db "SLEEPER ", "   "
    db 0x20
    times 14 db 0
    dw 25
    dd sleeper_len
    db "KILL    ", "   "
    db 0x20
    times 14 db 0
    dw 26
    dd kill_len
    times 512 - ($ - root_extra) db 0

dup2test_data:
    incbin "dup2test.bin"
dup2test_len equ $ - dup2test_data
    times 512 - dup2test_len db 0

waitpid_data:
    incbin "waitpid.bin"
waitpid_len equ $ - waitpid_data
    times 512 - waitpid_len db 0

fdtest_data:
    incbin "fdtest.bin"
fdtest_len equ $ - fdtest_data
    times 512 - fdtest_len db 0

pipefork_data:
    incbin "pipefork.bin"
pipefork_len equ $ - pipefork_data
    times 512 - pipefork_len db 0

cowtest_data:
    incbin "cowtest.bin"
cowtest_len equ $ - cowtest_data
    times 512 - cowtest_len db 0

stat_data:
    incbin "stat.bin"
stat_len equ $ - stat_data
    times 512 - stat_len db 0

sleeper_data:
    incbin "sleeper.bin"
sleeper_len equ $ - sleeper_data
    times 512 - sleeper_len db 0

kill_data:
    incbin "kill.bin"
kill_len equ $ - kill_data
    times 512 - kill_len db 0

    times (FAT_DATA_CLUSTERS - 25) * 512 db 0
