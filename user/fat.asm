%include "const.inc"

bits 16
org 0

; Volume boot sector at PART_LBA. Reserved count is 1, so FAT1 follows.
vbr:
    jmp short vbr_end
    nop
    db "VAMPIRE "
    dw 512
    db FAT_SPC
    dw FAT_RESERVED
    db FAT_COUNT
    dw FAT_ROOT_ENT
    dw FAT_TOTAL_SECS
    db FAT_MEDIA
    dw FAT_SEC_PER_FAT
    dw 32
    dw 2
    dd PART_LBA
    dd 0
    db 0x80
    db 0
    db 0x29
    dd 0x19910000
    db "VAMPIRE OS "
    db "FAT12   "
vbr_end:
    times 510 - ($ - vbr) db 0
    dw 0xAA55

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
    db 0x2A, 0xF0, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0x1D, 0xF0, 0xFF
    db 0x1F, 0x00, 0x02
    db 0x21, 0x20, 0x02
    db 0x23, 0x40, 0x02
    db 0x25, 0xF0, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0x0F, 0x03
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0x0F
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
    db 0x2A, 0xF0, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0x1D, 0xF0, 0xFF
    db 0x1F, 0x00, 0x02
    db 0x21, 0x20, 0x02
    db 0x23, 0x40, 0x02
    db 0x25, 0xF0, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0x0F, 0x03
    db 0xFF, 0xFF, 0xFF
    db 0xFF, 0xFF, 0x0F
    times FAT_SEC_PER_FAT * 512 - ($ - fat2) db 0

root:
    ; VFAT LFN longname.txt, 8.3 alias LONGNA~1.TXT
    db 0x41
    db "l", 0, "o", 0, "n", 0, "g", 0, "n", 0
    db 0x0F, 0, 0xF4
    db "a", 0, "m", 0, "e", 0, ".", 0, "t", 0, "x", 0
    dw 0
    db "t", 0, 0, 0
    db "LONGNA~1", "TXT"
    db 0x20
    times 14 db 0
    dw 41
    dd 4

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
    db "CRT     ", "   "
    db 0x20
    times 14 db 0
    dw 27
    dd crt_len
    db "HI      ", "   "
    db 0x20
    times 14 db 0
    dw 28
    dd hi_len
    db "SH      ", "   "
    db 0x20
    times 14 db 0
    dw 30
    dd sh_len
    db "MMAPTEST", "   "
    db 0x20
    times 14 db 0
    dw 38
    dd mmaptest_len
    db "UPTIME  ", "   "
    db 0x20
    times 14 db 0
    dw 39
    dd uptime_len
    db "INIT    ", "   "
    db 0x20
    times 14 db 0
    dw 40
    dd init_len
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

crt_data:
    incbin "crt.bin"
crt_len equ $ - crt_data
    times 512 - crt_len db 0

hi_data:
    incbin "hi.bin"
hi_len equ $ - hi_data
    times 1024 - hi_len db 0

sh_data:
    incbin "sh.bin"
sh_len equ $ - sh_data
    times 4096 - sh_len db 0

mmaptest_data:
    incbin "mmaptest.bin"
mmaptest_len equ $ - mmaptest_data
    times 512 - mmaptest_len db 0

uptime_data:
    incbin "uptime.bin"
uptime_len equ $ - uptime_data
    times 512 - uptime_len db 0

init_data:
    incbin "init.bin"
init_len equ $ - init_data
    times 512 - init_len db 0

longname_data:
    db "long"
    times 512 - ($ - longname_data) db 0

root_extra2:
    db "MMAPFILE", "   "
    db 0x20
    times 14 db 0
    dw 43
    dd mmapfile_len
    ; VFAT LFN munmaptest, 8.3 alias MUNMAP~1
    db 0x41
    db "m", 0, "u", 0, "n", 0, "m", 0, "a", 0
    db 0x0F, 0, 0x23
    db "p", 0, "t", 0, "e", 0, "s", 0, "t", 0, 0, 0
    dw 0
    db 0xFF, 0xFF, 0xFF, 0xFF
    db "MUNMAP~1", "   "
    db 0x20
    times 14 db 0
    dw 44
    dd munmaptest_len
    db "FBINFO  ", "   "
    db 0x20
    times 14 db 0
    dw 45
    dd fbinfo_len
    db "FBTEST  ", "   "
    db 0x20
    times 14 db 0
    dw 46
    dd fbtest_len
    db "FBHELLO ", "   "
    db 0x20
    times 14 db 0
    dw 47
    dd fbhello_len
    db "FBCLEAR ", "   "
    db 0x20
    times 14 db 0
    dw 49
    dd fbclear_len
    db "CATCH   ", "   "
    db 0x20
    times 14 db 0
    dw 50
    dd catch_len
    ; VFAT LFN mprottest, 8.3 alias MPROTT~1
    db 0x41
    db "m", 0, "p", 0, "r", 0, "o", 0, "t", 0
    db 0x0F, 0, 0x94
    db "t", 0, "e", 0, "s", 0, "t", 0, 0, 0, 0xFF, 0xFF
    dw 0
    db 0xFF, 0xFF, 0xFF, 0xFF
    db "MPROTT~1", "   "
    db 0x20
    times 14 db 0
    dw 51
    dd mprottest_len
    ; Cluster 0x0F00 is past the 1024-cluster volume (and past the 1307-sector
    ; image). `$ cat bad` must bread that LBA, fail, print `?`, and return `$`.
    db "BAD     ", "   "
    db 0x20
    times 14 db 0
    dw 0x0F00
    dd 5
    times 512 - ($ - root_extra2) db 0

mmapfile_data:
    incbin "mmapfile.bin"
mmapfile_len equ $ - mmapfile_data
    times 512 - mmapfile_len db 0

munmaptest_data:
    incbin "munmaptest.bin"
munmaptest_len equ $ - munmaptest_data
    times 512 - munmaptest_len db 0

fbinfo_data:
    incbin "fbinfo.bin"
fbinfo_len equ $ - fbinfo_data
    times 512 - fbinfo_len db 0

fbtest_data:
    incbin "fbtest.bin"
fbtest_len equ $ - fbtest_data
    times 512 - fbtest_len db 0

fbhello_data:
    incbin "fbhello.bin"
fbhello_len equ $ - fbhello_data
    times 1024 - fbhello_len db 0

fbclear_data:
    incbin "fbclear.bin"
fbclear_len equ $ - fbclear_data
    times 512 - fbclear_len db 0

catch_data:
    incbin "catch.bin"
catch_len equ $ - catch_data
    times 512 - catch_len db 0

mprottest_data:
    incbin "mprottest.bin"
mprottest_len equ $ - mprottest_data
    times 512 - mprottest_len db 0

    times (FAT_DATA_CLUSTERS - 50) * 512 db 0
