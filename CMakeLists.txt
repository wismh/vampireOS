cmake_minimum_required(VERSION 3.20)
project(vampire-os LANGUAGES NONE)

file(GLOB _nasm_hints "${CMAKE_SOURCE_DIR}/.tools/nasm*")
file(GLOB _qemu_hints "${CMAKE_SOURCE_DIR}/.tools/qemu*")

find_program(NASM_EXECUTABLE nasm REQUIRED
    HINTS
        ${_nasm_hints}
        "${CMAKE_SOURCE_DIR}/.tools"
        "C:/Program Files/NASM"
        "C:/Program Files (x86)/NASM"
)
find_program(CLANG_EXECUTABLE clang REQUIRED
    HINTS
        "C:/Program Files/LLVM/bin"
)
find_program(LLD_EXECUTABLE ld.lld REQUIRED
    HINTS
        "C:/Program Files/LLVM/bin"
)
find_program(OBJCOPY_EXECUTABLE llvm-objcopy REQUIRED
    HINTS
        "C:/Program Files/LLVM/bin"
)
find_program(QEMU_EXECUTABLE qemu-system-x86_64
    HINTS
        ${_qemu_hints}
        "${CMAKE_SOURCE_DIR}/.tools"
        "C:/Program Files/qemu"
        "C:/Program Files/QEMU"
)

set(BOOT_INC_DIR "${CMAKE_SOURCE_DIR}/boot")
set(BOOT_ASM "${BOOT_INC_DIR}/boot.asm")
set(STAGE2_ASM "${BOOT_INC_DIR}/stage2.asm")
set(BOOT_INC "${BOOT_INC_DIR}/const.inc")
set(KERNEL_DIR "${CMAKE_SOURCE_DIR}/kernel")
set(KERNEL_LD "${KERNEL_DIR}/linker.ld")
set(KERNEL_PACK_ASM "${KERNEL_DIR}/pack.asm")
set(KERNEL_ISR_ASM "${KERNEL_DIR}/isr.asm")
set(BOOT_BIN "${CMAKE_BINARY_DIR}/boot.bin")
set(STAGE2_BIN "${CMAKE_BINARY_DIR}/stage2.bin")
set(ISR_OBJ "${CMAKE_BINARY_DIR}/isr.o")
set(KERNEL_ELF "${CMAKE_BINARY_DIR}/kernel.elf")
set(KERNEL_RAW "${CMAKE_BINARY_DIR}/kernel.raw.bin")
set(KERNEL_BIN "${CMAKE_BINARY_DIR}/kernel.bin")
set(DISK_IMG "${CMAKE_BINARY_DIR}/vampire.img")
set(ECHO_ASM "${CMAKE_SOURCE_DIR}/user/echo.asm")
set(ECHO_BIN "${CMAKE_BINARY_DIR}/echo.bin")
set(A_ASM "${CMAKE_SOURCE_DIR}/user/a.asm")
set(A_BIN "${CMAKE_BINARY_DIR}/a.bin")
set(B_ASM "${CMAKE_SOURCE_DIR}/user/b.asm")
set(B_BIN "${CMAKE_BINARY_DIR}/b.bin")
set(C_ASM "${CMAKE_SOURCE_DIR}/user/c.asm")
set(C_BIN "${CMAKE_BINARY_DIR}/c.bin")
set(OPENTEST_ASM "${CMAKE_SOURCE_DIR}/user/opentest.asm")
set(OPENTEST_BIN "${CMAKE_BINARY_DIR}/opentest.bin")
set(READTEST_ASM "${CMAKE_SOURCE_DIR}/user/readtest.asm")
set(READTEST_BIN "${CMAKE_BINARY_DIR}/readtest.bin")
set(CAT_ASM "${CMAKE_SOURCE_DIR}/user/cat.asm")
set(CAT_BIN "${CMAKE_BINARY_DIR}/cat.bin")
set(LS_ASM "${CMAKE_SOURCE_DIR}/user/ls.asm")
set(LS_BIN "${CMAKE_BINARY_DIR}/ls.bin")
set(EXECTEST_ASM "${CMAKE_SOURCE_DIR}/user/exectest.asm")
set(EXECTEST_BIN "${CMAKE_BINARY_DIR}/exectest.bin")
set(STATUS_ASM "${CMAKE_SOURCE_DIR}/user/status.asm")
set(STATUS_BIN "${CMAKE_BINARY_DIR}/status.bin")
set(WAITER_ASM "${CMAKE_SOURCE_DIR}/user/waiter.asm")
set(WAITER_BIN "${CMAKE_BINARY_DIR}/waiter.bin")
set(PIPETEST_ASM "${CMAKE_SOURCE_DIR}/user/pipetest.asm")
set(PIPETEST_BIN "${CMAKE_BINARY_DIR}/pipetest.bin")
set(BRKTEST_ASM "${CMAKE_SOURCE_DIR}/user/brktest.asm")
set(BRKTEST_BIN "${CMAKE_BINARY_DIR}/brktest.bin")
set(FORKTEST_ASM "${CMAKE_SOURCE_DIR}/user/forktest.asm")
set(FORKTEST_BIN "${CMAKE_BINARY_DIR}/forktest.bin")
set(DUP2TEST_ASM "${CMAKE_SOURCE_DIR}/user/dup2test.asm")
set(DUP2TEST_BIN "${CMAKE_BINARY_DIR}/dup2test.bin")
set(WAITPID_ASM "${CMAKE_SOURCE_DIR}/user/waitpid.asm")
set(WAITPID_BIN "${CMAKE_BINARY_DIR}/waitpid.bin")
set(FDTEST_ASM "${CMAKE_SOURCE_DIR}/user/fdtest.asm")
set(FDTEST_BIN "${CMAKE_BINARY_DIR}/fdtest.bin")
set(PIPEFORK_ASM "${CMAKE_SOURCE_DIR}/user/pipefork.asm")
set(PIPEFORK_BIN "${CMAKE_BINARY_DIR}/pipefork.bin")
set(COWTEST_ASM "${CMAKE_SOURCE_DIR}/user/cowtest.asm")
set(COWTEST_BIN "${CMAKE_BINARY_DIR}/cowtest.bin")
set(STAT_ASM "${CMAKE_SOURCE_DIR}/user/stat.asm")
set(STAT_BIN "${CMAKE_BINARY_DIR}/stat.bin")
set(SLEEPER_ASM "${CMAKE_SOURCE_DIR}/user/sleeper.asm")
set(SLEEPER_BIN "${CMAKE_BINARY_DIR}/sleeper.bin")
set(KILL_ASM "${CMAKE_SOURCE_DIR}/user/kill.asm")
set(KILL_BIN "${CMAKE_BINARY_DIR}/kill.bin")
set(MMAPTEST_ASM "${CMAKE_SOURCE_DIR}/user/mmaptest.asm")
set(MMAPTEST_BIN "${CMAKE_BINARY_DIR}/mmaptest.bin")
set(MMAPFILE_ASM "${CMAKE_SOURCE_DIR}/user/mmapfile.asm")
set(MMAPFILE_BIN "${CMAKE_BINARY_DIR}/mmapfile.bin")
set(MUNMAPTEST_ASM "${CMAKE_SOURCE_DIR}/user/munmaptest.asm")
set(MUNMAPTEST_BIN "${CMAKE_BINARY_DIR}/munmaptest.bin")
set(FBINFO_ASM "${CMAKE_SOURCE_DIR}/user/fbinfo.asm")
set(FBINFO_BIN "${CMAKE_BINARY_DIR}/fbinfo.bin")
set(FBTEST_ASM "${CMAKE_SOURCE_DIR}/user/fbtest.asm")
set(FBTEST_BIN "${CMAKE_BINARY_DIR}/fbtest.bin")
set(FBCLEAR_ASM "${CMAKE_SOURCE_DIR}/user/fbclear.asm")
set(FBCLEAR_BIN "${CMAKE_BINARY_DIR}/fbclear.bin")
set(FBHELLO_C "${CMAKE_SOURCE_DIR}/user/fbhello.c")
set(FONT_C "${CMAKE_SOURCE_DIR}/user/font.c")
set(FB_ASM "${CMAKE_SOURCE_DIR}/user/fb.asm")
set(FBHELLO_BIN "${CMAKE_BINARY_DIR}/fbhello.bin")
set(UPTIME_ASM "${CMAKE_SOURCE_DIR}/user/uptime.asm")
set(UPTIME_BIN "${CMAKE_BINARY_DIR}/uptime.bin")
set(CRT_ASM "${CMAKE_SOURCE_DIR}/user/crt.asm")
set(FD_ASM "${CMAKE_SOURCE_DIR}/user/fd.asm")
set(CRTSTART_ASM "${CMAKE_SOURCE_DIR}/user/crtstart.asm")
set(CRT0_ASM "${CMAKE_SOURCE_DIR}/user/crt0.asm")
set(HI_C "${CMAKE_SOURCE_DIR}/user/hi.c")
set(SH_C "${CMAKE_SOURCE_DIR}/user/sh.c")
set(INIT_C "${CMAKE_SOURCE_DIR}/user/init.c")
set(STRING_C "${CMAKE_SOURCE_DIR}/user/string.c")
set(USER_LD "${CMAKE_SOURCE_DIR}/user/user.ld")
set(CRT_OBJ "${CMAKE_BINARY_DIR}/crt.o")
set(FD_OBJ "${CMAKE_BINARY_DIR}/fd.o")
set(FB_OBJ "${CMAKE_BINARY_DIR}/userfb.o")
set(CRTSTART_OBJ "${CMAKE_BINARY_DIR}/crtstart.o")
set(CRT0_OBJ "${CMAKE_BINARY_DIR}/crt0.o")
set(HI_OBJ "${CMAKE_BINARY_DIR}/hi.o")
set(SH_OBJ "${CMAKE_BINARY_DIR}/sh.o")
set(INIT_OBJ "${CMAKE_BINARY_DIR}/init.o")
set(STRING_OBJ "${CMAKE_BINARY_DIR}/string.o")
set(FBHELLO_OBJ "${CMAKE_BINARY_DIR}/fbhello.o")
set(FONT_OBJ "${CMAKE_BINARY_DIR}/font.o")
set(CRT_BIN "${CMAKE_BINARY_DIR}/crt.bin")
set(HI_BIN "${CMAKE_BINARY_DIR}/hi.bin")
set(SH_BIN "${CMAKE_BINARY_DIR}/sh.bin")
set(INIT_BIN "${CMAKE_BINARY_DIR}/init.bin")
set(FAT_ASM "${CMAKE_SOURCE_DIR}/user/fat.asm")
set(FAT_BIN "${CMAKE_BINARY_DIR}/fat.bin")

add_custom_command(
    OUTPUT "${ECHO_BIN}"
    COMMAND "${NASM_EXECUTABLE}" -f bin -o "${ECHO_BIN}" "${ECHO_ASM}"
    DEPENDS "${ECHO_ASM}"
    COMMENT "Assembling user ELF echo"
    VERBATIM
)

add_custom_command(
    OUTPUT "${A_BIN}"
    COMMAND "${NASM_EXECUTABLE}" -f bin -o "${A_BIN}" "${A_ASM}"
    DEPENDS "${A_ASM}"
    COMMENT "Assembling user ELF a"
    VERBATIM
)

add_custom_command(
    OUTPUT "${B_BIN}"
    COMMAND "${NASM_EXECUTABLE}" -f bin -o "${B_BIN}" "${B_ASM}"
    DEPENDS "${B_ASM}"
    COMMENT "Assembling user ELF b"
    VERBATIM
)

add_custom_command(
    OUTPUT "${C_BIN}"
    COMMAND "${NASM_EXECUTABLE}" -f bin -o "${C_BIN}" "${C_ASM}"
    DEPENDS "${C_ASM}"
    COMMENT "Assembling user ELF c"
    VERBATIM
)

add_custom_command(
    OUTPUT "${OPENTEST_BIN}"
    COMMAND "${NASM_EXECUTABLE}" -f bin -o "${OPENTEST_BIN}" "${OPENTEST_ASM}"
    DEPENDS "${OPENTEST_ASM}"
    COMMENT "Assembling user ELF opentest"
    VERBATIM
)

add_custom_command(
    OUTPUT "${READTEST_BIN}"
    COMMAND "${NASM_EXECUTABLE}" -f bin -o "${READTEST_BIN}" "${READTEST_ASM}"
    DEPENDS "${READTEST_ASM}"
    COMMENT "Assembling user ELF readtest"
    VERBATIM
)

add_custom_command(
    OUTPUT "${CAT_BIN}"
    COMMAND "${NASM_EXECUTABLE}" -f bin -o "${CAT_BIN}" "${CAT_ASM}"
    DEPENDS "${CAT_ASM}"
    COMMENT "Assembling user ELF cat"
    VERBATIM
)

add_custom_command(
    OUTPUT "${LS_BIN}"
    COMMAND "${NASM_EXECUTABLE}" -f bin -o "${LS_BIN}" "${LS_ASM}"
    DEPENDS "${LS_ASM}"
    COMMENT "Assembling user ELF ls"
    VERBATIM
)

add_custom_command(
    OUTPUT "${EXECTEST_BIN}"
    COMMAND "${NASM_EXECUTABLE}" -f bin -o "${EXECTEST_BIN}" "${EXECTEST_ASM}"
    DEPENDS "${EXECTEST_ASM}"
    COMMENT "Assembling user ELF exectest"
    VERBATIM
)

add_custom_command(
    OUTPUT "${STATUS_BIN}"
    COMMAND "${NASM_EXECUTABLE}" -f bin -o "${STATUS_BIN}" "${STATUS_ASM}"
    DEPENDS "${STATUS_ASM}"
    COMMENT "Assembling user ELF status"
    VERBATIM
)

add_custom_command(
    OUTPUT "${WAITER_BIN}"
    COMMAND "${NASM_EXECUTABLE}" -f bin -o "${WAITER_BIN}" "${WAITER_ASM}"
    DEPENDS "${WAITER_ASM}"
    COMMENT "Assembling user ELF waiter"
    VERBATIM
)

add_custom_command(
    OUTPUT "${PIPETEST_BIN}"
    COMMAND "${NASM_EXECUTABLE}" -f bin -o "${PIPETEST_BIN}" "${PIPETEST_ASM}"
    DEPENDS "${PIPETEST_ASM}"
    COMMENT "Assembling user ELF pipetest"
    VERBATIM
)

add_custom_command(
    OUTPUT "${BRKTEST_BIN}"
    COMMAND "${NASM_EXECUTABLE}" -f bin -o "${BRKTEST_BIN}" "${BRKTEST_ASM}"
    DEPENDS "${BRKTEST_ASM}"
    COMMENT "Assembling user ELF brktest"
    VERBATIM
)

add_custom_command(
    OUTPUT "${FORKTEST_BIN}"
    COMMAND "${NASM_EXECUTABLE}" -f bin -o "${FORKTEST_BIN}" "${FORKTEST_ASM}"
    DEPENDS "${FORKTEST_ASM}"
    COMMENT "Assembling user ELF forktest"
    VERBATIM
)

add_custom_command(
    OUTPUT "${DUP2TEST_BIN}"
    COMMAND "${NASM_EXECUTABLE}" -f bin -o "${DUP2TEST_BIN}" "${DUP2TEST_ASM}"
    DEPENDS "${DUP2TEST_ASM}"
    COMMENT "Assembling user ELF dup2test"
    VERBATIM
)

add_custom_command(
    OUTPUT "${WAITPID_BIN}"
    COMMAND "${NASM_EXECUTABLE}" -f bin -o "${WAITPID_BIN}" "${WAITPID_ASM}"
    DEPENDS "${WAITPID_ASM}"
    COMMENT "Assembling user ELF waitpid"
    VERBATIM
)

add_custom_command(
    OUTPUT "${FDTEST_BIN}"
    COMMAND "${NASM_EXECUTABLE}" -f bin -o "${FDTEST_BIN}" "${FDTEST_ASM}"
    DEPENDS "${FDTEST_ASM}"
    COMMENT "Assembling user ELF fdtest"
    VERBATIM
)

add_custom_command(
    OUTPUT "${PIPEFORK_BIN}"
    COMMAND "${NASM_EXECUTABLE}" -f bin -o "${PIPEFORK_BIN}" "${PIPEFORK_ASM}"
    DEPENDS "${PIPEFORK_ASM}"
    COMMENT "Assembling user ELF pipefork"
    VERBATIM
)

add_custom_command(
    OUTPUT "${COWTEST_BIN}"
    COMMAND "${NASM_EXECUTABLE}" -f bin -o "${COWTEST_BIN}" "${COWTEST_ASM}"
    DEPENDS "${COWTEST_ASM}"
    COMMENT "Assembling user ELF cowtest"
    VERBATIM
)

add_custom_command(
    OUTPUT "${STAT_BIN}"
    COMMAND "${NASM_EXECUTABLE}" -f bin -o "${STAT_BIN}" "${STAT_ASM}"
    DEPENDS "${STAT_ASM}"
    COMMENT "Assembling user ELF stat"
    VERBATIM
)

add_custom_command(
    OUTPUT "${SLEEPER_BIN}"
    COMMAND "${NASM_EXECUTABLE}" -f bin -o "${SLEEPER_BIN}" "${SLEEPER_ASM}"
    DEPENDS "${SLEEPER_ASM}"
    COMMENT "Assembling user ELF sleeper"
    VERBATIM
)

add_custom_command(
    OUTPUT "${KILL_BIN}"
    COMMAND "${NASM_EXECUTABLE}" -f bin -o "${KILL_BIN}" "${KILL_ASM}"
    DEPENDS "${KILL_ASM}"
    COMMENT "Assembling user ELF kill"
    VERBATIM
)

add_custom_command(
    OUTPUT "${MMAPTEST_BIN}"
    COMMAND "${NASM_EXECUTABLE}" -f bin -o "${MMAPTEST_BIN}" "${MMAPTEST_ASM}"
    DEPENDS "${MMAPTEST_ASM}"
    COMMENT "Assembling user ELF mmaptest"
    VERBATIM
)

add_custom_command(
    OUTPUT "${MMAPFILE_BIN}"
    COMMAND "${NASM_EXECUTABLE}" -f bin -o "${MMAPFILE_BIN}" "${MMAPFILE_ASM}"
    DEPENDS "${MMAPFILE_ASM}"
    COMMENT "Assembling user ELF mmapfile"
    VERBATIM
)

add_custom_command(
    OUTPUT "${MUNMAPTEST_BIN}"
    COMMAND "${NASM_EXECUTABLE}" -f bin -o "${MUNMAPTEST_BIN}" "${MUNMAPTEST_ASM}"
    DEPENDS "${MUNMAPTEST_ASM}"
    COMMENT "Assembling user ELF munmaptest"
    VERBATIM
)

add_custom_command(
    OUTPUT "${FBINFO_BIN}"
    COMMAND "${NASM_EXECUTABLE}" -f bin -o "${FBINFO_BIN}" "${FBINFO_ASM}"
    DEPENDS "${FBINFO_ASM}"
    COMMENT "Assembling user ELF fbinfo"
    VERBATIM
)

add_custom_command(
    OUTPUT "${FBTEST_BIN}"
    COMMAND "${NASM_EXECUTABLE}" -f bin -o "${FBTEST_BIN}" "${FBTEST_ASM}"
    DEPENDS "${FBTEST_ASM}"
    COMMENT "Assembling user ELF fbtest"
    VERBATIM
)

add_custom_command(
    OUTPUT "${FBCLEAR_BIN}"
    COMMAND "${NASM_EXECUTABLE}" -f bin -o "${FBCLEAR_BIN}" "${FBCLEAR_ASM}"
    DEPENDS "${FBCLEAR_ASM}"
    COMMENT "Assembling user ELF fbclear"
    VERBATIM
)

add_custom_command(
    OUTPUT "${FB_OBJ}"
    COMMAND "${NASM_EXECUTABLE}" -f elf64 -o "${FB_OBJ}" "${FB_ASM}"
    DEPENDS "${FB_ASM}"
    COMMENT "Assembling fbinfo/fbpix/fbpresent stubs"
    VERBATIM
)

add_custom_command(
    OUTPUT "${FONT_OBJ}"
    COMMAND "${CLANG_EXECUTABLE}"
        --target=x86_64-unknown-none
        -ffreestanding
        -fno-stack-protector
        -fno-pic
        -fno-pie
        -mno-red-zone
        -mno-mmx
        -mno-sse
        -mno-sse2
        -fno-asynchronous-unwind-tables
        -fno-builtin
        -Os
        -c
        -o "${FONT_OBJ}"
        "${FONT_C}"
    DEPENDS "${FONT_C}" "${CMAKE_SOURCE_DIR}/user/font.h"
    COMMENT "Compiling user/font.c"
    VERBATIM
)

add_custom_command(
    OUTPUT "${FBHELLO_OBJ}"
    COMMAND "${CLANG_EXECUTABLE}"
        --target=x86_64-unknown-none
        -ffreestanding
        -fno-stack-protector
        -fno-pic
        -fno-pie
        -mno-red-zone
        -mno-mmx
        -mno-sse
        -mno-sse2
        -fno-asynchronous-unwind-tables
        -fno-builtin
        -Os
        -c
        -o "${FBHELLO_OBJ}"
        "${FBHELLO_C}"
    DEPENDS "${FBHELLO_C}" "${CMAKE_SOURCE_DIR}/user/font.h"
    COMMENT "Compiling user/fbhello.c"
    VERBATIM
)

add_custom_command(
    OUTPUT "${FBHELLO_BIN}"
    COMMAND "${LLD_EXECUTABLE}"
        -m elf_x86_64
        -T "${USER_LD}"
        -nostdlib
        -n
        --strip-all
        -o "${FBHELLO_BIN}"
        "${CRT0_OBJ}"
        "${FBHELLO_OBJ}"
        "${FONT_OBJ}"
        "${CRT_OBJ}"
        "${FB_OBJ}"
    DEPENDS "${CRT0_OBJ}" "${FBHELLO_OBJ}" "${FONT_OBJ}" "${CRT_OBJ}" "${FB_OBJ}" "${USER_LD}"
    COMMENT "Linking user ELF fbhello"
    VERBATIM
)

add_custom_command(
    OUTPUT "${UPTIME_BIN}"
    COMMAND "${NASM_EXECUTABLE}" -f bin -o "${UPTIME_BIN}" "${UPTIME_ASM}"
    DEPENDS "${UPTIME_ASM}"
    COMMENT "Assembling user ELF uptime"
    VERBATIM
)

add_custom_command(
    OUTPUT "${CRT_OBJ}"
    COMMAND "${NASM_EXECUTABLE}" -f elf64 -o "${CRT_OBJ}" "${CRT_ASM}"
    DEPENDS "${CRT_ASM}"
    COMMENT "Assembling CRT syscall stubs"
    VERBATIM
)

add_custom_command(
    OUTPUT "${FD_OBJ}"
    COMMAND "${NASM_EXECUTABLE}" -f elf64 -o "${FD_OBJ}" "${FD_ASM}"
    DEPENDS "${FD_ASM}"
    COMMENT "Assembling open/close/dup2/pipe/chdir/getcwd stubs"
    VERBATIM
)

add_custom_command(
    OUTPUT "${CRTSTART_OBJ}"
    COMMAND "${NASM_EXECUTABLE}" -f elf64 -o "${CRTSTART_OBJ}" "${CRTSTART_ASM}"
    DEPENDS "${CRTSTART_ASM}"
    COMMENT "Assembling CRT demo"
    VERBATIM
)

add_custom_command(
    OUTPUT "${CRT_BIN}"
    COMMAND "${LLD_EXECUTABLE}"
        -m elf_x86_64
        -T "${USER_LD}"
        -nostdlib
        -n
        --strip-all
        -o "${CRT_BIN}"
        "${CRTSTART_OBJ}"
        "${CRT_OBJ}"
    DEPENDS "${CRTSTART_OBJ}" "${CRT_OBJ}" "${USER_LD}"
    COMMENT "Linking user ELF crt"
    VERBATIM
)

add_custom_command(
    OUTPUT "${CRT0_OBJ}"
    COMMAND "${NASM_EXECUTABLE}" -f elf64 -o "${CRT0_OBJ}" "${CRT0_ASM}"
    DEPENDS "${CRT0_ASM}"
    COMMENT "Assembling C start"
    VERBATIM
)

add_custom_command(
    OUTPUT "${HI_OBJ}"
    COMMAND "${CLANG_EXECUTABLE}"
        --target=x86_64-unknown-none
        -ffreestanding
        -fno-stack-protector
        -fno-pic
        -fno-pie
        -mno-red-zone
        -mno-mmx
        -mno-sse
        -mno-sse2
        -fno-asynchronous-unwind-tables
        -fno-builtin
        -c
        -o "${HI_OBJ}"
        "${HI_C}"
    DEPENDS "${HI_C}"
    COMMENT "Compiling user/hi.c"
    VERBATIM
)

add_custom_command(
    OUTPUT "${STRING_OBJ}"
    COMMAND "${CLANG_EXECUTABLE}"
        --target=x86_64-unknown-none
        -ffreestanding
        -fno-stack-protector
        -fno-pic
        -fno-pie
        -mno-red-zone
        -mno-mmx
        -mno-sse
        -mno-sse2
        -fno-asynchronous-unwind-tables
        -fno-builtin
        -c
        -o "${STRING_OBJ}"
        "${STRING_C}"
    DEPENDS "${STRING_C}"
    COMMENT "Compiling user/string.c"
    VERBATIM
)

add_custom_command(
    OUTPUT "${HI_BIN}"
    COMMAND "${LLD_EXECUTABLE}"
        -m elf_x86_64
        -T "${USER_LD}"
        -nostdlib
        -n
        --strip-all
        -o "${HI_BIN}"
        "${CRT0_OBJ}"
        "${HI_OBJ}"
        "${STRING_OBJ}"
        "${CRT_OBJ}"
    DEPENDS "${CRT0_OBJ}" "${HI_OBJ}" "${STRING_OBJ}" "${CRT_OBJ}" "${USER_LD}"
    COMMENT "Linking user ELF hi"
    VERBATIM
)

add_custom_command(
    OUTPUT "${SH_OBJ}"
    COMMAND "${CLANG_EXECUTABLE}"
        --target=x86_64-unknown-none
        -ffreestanding
        -fno-stack-protector
        -fno-pic
        -fno-pie
        -mno-red-zone
        -mno-mmx
        -mno-sse
        -mno-sse2
        -fno-asynchronous-unwind-tables
        -fno-builtin
        -Os
        -c
        -o "${SH_OBJ}"
        "${SH_C}"
    DEPENDS "${SH_C}"
    COMMENT "Compiling user/sh.c"
    VERBATIM
)

add_custom_command(
    OUTPUT "${SH_BIN}"
    COMMAND "${LLD_EXECUTABLE}"
        -m elf_x86_64
        -T "${USER_LD}"
        -nostdlib
        -n
        --strip-all
        -o "${SH_BIN}"
        "${CRT0_OBJ}"
        "${SH_OBJ}"
        "${STRING_OBJ}"
        "${CRT_OBJ}"
        "${FD_OBJ}"
    DEPENDS "${CRT0_OBJ}" "${SH_OBJ}" "${STRING_OBJ}" "${CRT_OBJ}" "${FD_OBJ}" "${USER_LD}"
    COMMENT "Linking user ELF sh"
    VERBATIM
)

add_custom_command(
    OUTPUT "${INIT_OBJ}"
    COMMAND "${CLANG_EXECUTABLE}"
        --target=x86_64-unknown-none
        -ffreestanding
        -fno-stack-protector
        -fno-pic
        -fno-pie
        -mno-red-zone
        -mno-mmx
        -mno-sse
        -mno-sse2
        -fno-asynchronous-unwind-tables
        -fno-builtin
        -c
        -o "${INIT_OBJ}"
        "${INIT_C}"
    DEPENDS "${INIT_C}"
    COMMENT "Compiling user/init.c"
    VERBATIM
)

add_custom_command(
    OUTPUT "${INIT_BIN}"
    COMMAND "${LLD_EXECUTABLE}"
        -m elf_x86_64
        -T "${USER_LD}"
        -nostdlib
        -n
        --strip-all
        -o "${INIT_BIN}"
        "${CRT0_OBJ}"
        "${INIT_OBJ}"
        "${CRT_OBJ}"
    DEPENDS "${CRT0_OBJ}" "${INIT_OBJ}" "${CRT_OBJ}" "${USER_LD}"
    COMMENT "Linking user ELF init"
    VERBATIM
)

add_custom_command(
    OUTPUT "${FAT_BIN}"
    COMMAND "${NASM_EXECUTABLE}"
        -f bin
        -I "${BOOT_INC_DIR}/"
        -I "${CMAKE_BINARY_DIR}/"
        -o "${FAT_BIN}"
        "${FAT_ASM}"
    DEPENDS "${FAT_ASM}" "${ECHO_BIN}" "${A_BIN}" "${B_BIN}" "${C_BIN}" "${OPENTEST_BIN}" "${READTEST_BIN}" "${CAT_BIN}" "${LS_BIN}" "${EXECTEST_BIN}" "${STATUS_BIN}" "${WAITER_BIN}" "${PIPETEST_BIN}" "${BRKTEST_BIN}" "${FORKTEST_BIN}" "${DUP2TEST_BIN}" "${WAITPID_BIN}" "${FDTEST_BIN}" "${PIPEFORK_BIN}" "${COWTEST_BIN}" "${STAT_BIN}" "${SLEEPER_BIN}" "${KILL_BIN}" "${MMAPTEST_BIN}" "${MMAPFILE_BIN}" "${MUNMAPTEST_BIN}" "${FBINFO_BIN}" "${FBTEST_BIN}" "${FBCLEAR_BIN}" "${FBHELLO_BIN}" "${UPTIME_BIN}" "${CRT_BIN}" "${HI_BIN}" "${SH_BIN}" "${INIT_BIN}" "${BOOT_INC}"
    COMMENT "Packing FAT12 volume"
    VERBATIM
)

set(KERNEL_C_SOURCES
    "${KERNEL_DIR}/kmain.c"
    "${KERNEL_DIR}/vga.c"
    "${KERNEL_DIR}/fb.c"
    "${KERNEL_DIR}/idt.c"
    "${KERNEL_DIR}/pic.c"
    "${KERNEL_DIR}/pit.c"
    "${KERNEL_DIR}/kbd.c"
    "${KERNEL_DIR}/mouse.c"
    "${KERNEL_DIR}/e820.c"
    "${KERNEL_DIR}/pmm.c"
    "${KERNEL_DIR}/vmm.c"
    "${KERNEL_DIR}/heap.c"
    "${KERNEL_DIR}/user.c"
    "${KERNEL_DIR}/sched.c"
    "${KERNEL_DIR}/fs.c"
    "${KERNEL_DIR}/elf.c"
    "${KERNEL_DIR}/ata.c"
    "${KERNEL_DIR}/ahci.c"
)
set(KERNEL_C_OBJECTS "")
foreach(_src ${KERNEL_C_SOURCES})
    cmake_path(GET _src STEM _name)
    set(_obj "${CMAKE_BINARY_DIR}/${_name}.o")
    add_custom_command(
        OUTPUT "${_obj}"
        COMMAND "${CLANG_EXECUTABLE}"
            --target=x86_64-unknown-none
            -ffreestanding
            -fno-stack-protector
            -fno-pic
            -mno-red-zone
            -mno-mmx
            -mno-sse
            -mno-sse2
            -mcmodel=kernel
            -I "${KERNEL_DIR}"
            -c
            -o "${_obj}"
            "${_src}"
        DEPENDS
            "${_src}"
            "${KERNEL_DIR}/vga.h"
            "${KERNEL_DIR}/fb.h"
            "${KERNEL_DIR}/idt.h"
            "${KERNEL_DIR}/pic.h"
            "${KERNEL_DIR}/pit.h"
            "${KERNEL_DIR}/kbd.h"
            "${KERNEL_DIR}/mouse.h"
            "${KERNEL_DIR}/e820.h"
            "${KERNEL_DIR}/pmm.h"
            "${KERNEL_DIR}/vmm.h"
            "${KERNEL_DIR}/heap.h"
            "${KERNEL_DIR}/user.h"
            "${KERNEL_DIR}/sched.h"
            "${KERNEL_DIR}/fs.h"
            "${KERNEL_DIR}/elf.h"
            "${KERNEL_DIR}/ata.h"
            "${KERNEL_DIR}/ahci.h"
            "${KERNEL_DIR}/io.h"
        COMMENT "Compiling ${_name}.c"
        VERBATIM
    )
    list(APPEND KERNEL_C_OBJECTS "${_obj}")
endforeach()

add_custom_command(
    OUTPUT "${ISR_OBJ}"
    COMMAND "${NASM_EXECUTABLE}" -f elf64 -o "${ISR_OBJ}" "${KERNEL_ISR_ASM}"
    DEPENDS "${KERNEL_ISR_ASM}"
    COMMENT "Assembling ISR stubs"
    VERBATIM
)

add_custom_command(
    OUTPUT "${BOOT_BIN}"
    COMMAND "${NASM_EXECUTABLE}" -f bin -I "${BOOT_INC_DIR}/" -o "${BOOT_BIN}" "${BOOT_ASM}"
    DEPENDS "${BOOT_ASM}" "${BOOT_INC}"
    COMMENT "Assembling MBR (stage 1)"
    VERBATIM
)

add_custom_command(
    OUTPUT "${STAGE2_BIN}"
    COMMAND "${NASM_EXECUTABLE}" -f bin -I "${BOOT_INC_DIR}/" -o "${STAGE2_BIN}" "${STAGE2_ASM}"
    DEPENDS "${STAGE2_ASM}" "${BOOT_INC}"
    COMMENT "Assembling stage 2"
    VERBATIM
)

add_custom_command(
    OUTPUT "${KERNEL_ELF}"
    COMMAND "${LLD_EXECUTABLE}"
        -m elf_x86_64
        -T "${KERNEL_LD}"
        -nostdlib
        -o "${KERNEL_ELF}"
        ${KERNEL_C_OBJECTS}
        "${ISR_OBJ}"
    DEPENDS ${KERNEL_C_OBJECTS} "${ISR_OBJ}" "${KERNEL_LD}"
    COMMENT "Linking kernel ELF"
    VERBATIM
)

add_custom_command(
    OUTPUT "${KERNEL_RAW}"
    COMMAND "${OBJCOPY_EXECUTABLE}" -O binary "${KERNEL_ELF}" "${KERNEL_RAW}"
    DEPENDS "${KERNEL_ELF}"
    COMMENT "Extracting kernel binary"
    VERBATIM
)

add_custom_command(
    OUTPUT "${KERNEL_BIN}"
    COMMAND "${NASM_EXECUTABLE}"
        -f bin
        -I "${BOOT_INC_DIR}/"
        -I "${CMAKE_BINARY_DIR}/"
        -o "${KERNEL_BIN}"
        "${KERNEL_PACK_ASM}"
    DEPENDS "${KERNEL_PACK_ASM}" "${BOOT_INC}" "${KERNEL_RAW}"
    COMMENT "Padding kernel to disk sectors"
    VERBATIM
)

add_custom_command(
    OUTPUT "${DISK_IMG}"
    COMMAND "${CMAKE_COMMAND}"
        "-DFILE1=${BOOT_BIN}"
        "-DFILE2=${STAGE2_BIN}"
        "-DFILE3=${KERNEL_BIN}"
        "-DFILE4=${FAT_BIN}"
        "-DOUT=${DISK_IMG}"
        -P "${CMAKE_SOURCE_DIR}/cmake/concat.cmake"
    DEPENDS "${BOOT_BIN}" "${STAGE2_BIN}" "${KERNEL_BIN}" "${FAT_BIN}" "${CMAKE_SOURCE_DIR}/cmake/concat.cmake"
    COMMENT "Creating disk image"
    VERBATIM
)

add_custom_target(boot ALL DEPENDS "${DISK_IMG}")

if(QEMU_EXECUTABLE)
    cmake_path(GET QEMU_EXECUTABLE PARENT_PATH QEMU_DIR)
    set(_qemu_cmd "${QEMU_EXECUTABLE}")
    if(EXISTS "${QEMU_DIR}/share")
        list(APPEND _qemu_cmd -L "${QEMU_DIR}/share")
    endif()
    add_custom_target(run
        COMMAND ${_qemu_cmd}
            -drive "format=raw,file=${DISK_IMG},if=ide,cache=writethrough,file.locking=off"
            -device ahci,id=ahci
            -drive "id=ahci-hd,file=${DISK_IMG},if=none,format=raw,cache=writethrough,file.locking=off"
            -device ide-hd,drive=ahci-hd,bus=ahci.0
            -vga std
            -m 128
            -no-reboot
            -no-shutdown
        DEPENDS "${DISK_IMG}"
        USES_TERMINAL
        COMMENT "Booting vampire.img in QEMU"
    )
else()
    message(WARNING "qemu-system-x86_64 not found; the 'run' target is unavailable")
endif()
