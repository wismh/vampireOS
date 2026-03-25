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
    OUTPUT "${FAT_BIN}"
    COMMAND "${NASM_EXECUTABLE}"
        -f bin
        -I "${BOOT_INC_DIR}/"
        -I "${CMAKE_BINARY_DIR}/"
        -o "${FAT_BIN}"
        "${FAT_ASM}"
    DEPENDS "${FAT_ASM}" "${ECHO_BIN}" "${A_BIN}" "${B_BIN}" "${C_BIN}" "${OPENTEST_BIN}" "${READTEST_BIN}" "${CAT_BIN}" "${LS_BIN}" "${EXECTEST_BIN}" "${STATUS_BIN}" "${WAITER_BIN}" "${PIPETEST_BIN}" "${BRKTEST_BIN}" "${FORKTEST_BIN}" "${DUP2TEST_BIN}" "${WAITPID_BIN}" "${FDTEST_BIN}" "${PIPEFORK_BIN}" "${COWTEST_BIN}" "${STAT_BIN}" "${BOOT_INC}"
    COMMENT "Packing FAT12 volume"
    VERBATIM
)

set(KERNEL_C_SOURCES
    "${KERNEL_DIR}/kmain.c"
    "${KERNEL_DIR}/vga.c"
    "${KERNEL_DIR}/idt.c"
    "${KERNEL_DIR}/pic.c"
    "${KERNEL_DIR}/pit.c"
    "${KERNEL_DIR}/kbd.c"
    "${KERNEL_DIR}/e820.c"
    "${KERNEL_DIR}/pmm.c"
    "${KERNEL_DIR}/vmm.c"
    "${KERNEL_DIR}/heap.c"
    "${KERNEL_DIR}/user.c"
    "${KERNEL_DIR}/sched.c"
    "${KERNEL_DIR}/fs.c"
    "${KERNEL_DIR}/elf.c"
    "${KERNEL_DIR}/ata.c"
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
            "${KERNEL_DIR}/idt.h"
            "${KERNEL_DIR}/pic.h"
            "${KERNEL_DIR}/pit.h"
            "${KERNEL_DIR}/kbd.h"
            "${KERNEL_DIR}/e820.h"
            "${KERNEL_DIR}/pmm.h"
            "${KERNEL_DIR}/vmm.h"
            "${KERNEL_DIR}/heap.h"
            "${KERNEL_DIR}/user.h"
            "${KERNEL_DIR}/sched.h"
            "${KERNEL_DIR}/fs.h"
            "${KERNEL_DIR}/elf.h"
            "${KERNEL_DIR}/ata.h"
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
            -drive "format=raw,file=${DISK_IMG},if=ide"
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
