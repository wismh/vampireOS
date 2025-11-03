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
find_program(QEMU_EXECUTABLE qemu-system-x86_64
    HINTS
        ${_qemu_hints}
        "${CMAKE_SOURCE_DIR}/.tools"
        "C:/Program Files/qemu"
        "C:/Program Files/QEMU"
)

set(BOOT_ASM "${CMAKE_SOURCE_DIR}/boot/boot.asm")
set(BOOT_BIN "${CMAKE_BINARY_DIR}/boot.bin")
set(DISK_IMG "${CMAKE_BINARY_DIR}/vampire.img")

add_custom_command(
    OUTPUT "${BOOT_BIN}"
    COMMAND "${NASM_EXECUTABLE}" -f bin -o "${BOOT_BIN}" "${BOOT_ASM}"
    DEPENDS "${BOOT_ASM}"
    COMMENT "Assembling boot sector"
    VERBATIM
)

add_custom_command(
    OUTPUT "${DISK_IMG}"
    COMMAND "${CMAKE_COMMAND}" -E copy "${BOOT_BIN}" "${DISK_IMG}"
    DEPENDS "${BOOT_BIN}"
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
