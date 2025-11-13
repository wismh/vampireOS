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

set(KERNEL_C_SOURCES
    "${KERNEL_DIR}/kmain.c"
    "${KERNEL_DIR}/vga.c"
    "${KERNEL_DIR}/idt.c"
    "${KERNEL_DIR}/pic.c"
    "${KERNEL_DIR}/pit.c"
    "${KERNEL_DIR}/kbd.c"
    "${KERNEL_DIR}/e820.c"
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
            -mcmodel=small
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
        -DFILE1=${BOOT_BIN}
        -DFILE2=${STAGE2_BIN}
        -DFILE3=${KERNEL_BIN}
        -DOUT=${DISK_IMG}
        -P "${CMAKE_SOURCE_DIR}/cmake/concat.cmake"
    DEPENDS "${BOOT_BIN}" "${STAGE2_BIN}" "${KERNEL_BIN}" "${CMAKE_SOURCE_DIR}/cmake/concat.cmake"
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
