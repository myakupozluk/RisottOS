cmake_minimum_required(VERSION 3.16)
project(RisottOS ASM_NASM C)

set(CMAKE_C_STANDARD 11)
set(BUILD_DIR ${CMAKE_BINARY_DIR})
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(DAP_SECTORS 16)

enable_language(ASM_NASM)
set(CMAKE_ASM_NASM_OBJECT_FORMAT elf32)

set(C_FLAGS -m32 -ffreestanding -fno-pic -fno-pie)

set(C_SOURCES
        ${SRC_DIR}/kernel.c
        ${SRC_DIR}/arch/io.c
        ${SRC_DIR}/arch/idt.c
        ${SRC_DIR}/arch/pic.c
        ${SRC_DIR}/arch/interrupts.c
        ${SRC_DIR}/arch/shell.c
        ${SRC_DIR}/drivers/vga.c
        ${SRC_DIR}/drivers/keyboard.c
        ${SRC_DIR}/drivers/timer.c
        ${SRC_DIR}/drivers/rtc.c
)

set(ASM_SOURCES
        ${SRC_DIR}/boot.asm
        ${SRC_DIR}/kernel_entry.asm
        ${SRC_DIR}/arch/interrupts.asm
)

# headers, just for IDE indexing below
file(GLOB_RECURSE HEADER_FILES ${SRC_DIR}/*.h)

# dummy target so CLion indexes these files without trying to build them itself
add_custom_target(ide_sources SOURCES ${C_SOURCES} ${ASM_SOURCES} ${HEADER_FILES})

# boot.bin: flat binary, built separately from the kernel
add_custom_command(
        OUTPUT ${BUILD_DIR}/boot.bin
        COMMAND nasm -f bin ${SRC_DIR}/boot.asm -o ${BUILD_DIR}/boot.bin
        DEPENDS ${SRC_DIR}/boot.asm
        COMMENT "Assembling boot.bin"
)

# assemble each .asm file into an elf32 object (boot.asm is built separately above)
set(ASM_OBJS "")
foreach(asm_file ${ASM_SOURCES})
    if(NOT asm_file STREQUAL ${SRC_DIR}/boot.asm)
        get_filename_component(name ${asm_file} NAME_WE)
        set(obj_out ${BUILD_DIR}/${name}_asm.o)
        add_custom_command(
                OUTPUT ${obj_out}
                COMMAND nasm -f elf32 ${asm_file} -o ${obj_out}
                DEPENDS ${asm_file}
                COMMENT "Assembling ${name}_asm.o"
        )
        list(APPEND ASM_OBJS ${obj_out})
    endif()
endforeach()

# compile each .c file into an object
set(C_OBJS "")
foreach(c_file ${C_SOURCES})
    get_filename_component(name ${c_file} NAME_WE)
    set(obj_out ${BUILD_DIR}/${name}_c.o)
    add_custom_command(
            OUTPUT ${obj_out}
            COMMAND ${CMAKE_C_COMPILER} ${C_FLAGS} -c ${c_file} -o ${obj_out}
            DEPENDS ${c_file}
            COMMENT "Compiling ${name}_c.o"
    )
    list(APPEND C_OBJS ${obj_out})
endforeach()

# link all objects into a flat binary kernel.bin
add_custom_command(
        OUTPUT ${BUILD_DIR}/kernel.bin
        COMMAND ld -m elf_i386 -T ${CMAKE_SOURCE_DIR}/config/linker.ld
        -o ${BUILD_DIR}/kernel.bin --oformat=binary
        ${ASM_OBJS} ${C_OBJS}
        DEPENDS ${ASM_OBJS} ${C_OBJS} ${CMAKE_SOURCE_DIR}/config/linker.ld
        COMMENT "Linking kernel.bin"
)

# combine boot.bin + kernel.bin into os.img, check size, pad to sectors
add_custom_command(
        OUTPUT ${BUILD_DIR}/os.img
        COMMAND ${CMAKE_COMMAND}
        -DBOOT_BIN=${BUILD_DIR}/boot.bin
        -DKERNEL_BIN=${BUILD_DIR}/kernel.bin
        -DOS_IMG=${BUILD_DIR}/os.img
        -DDAP_SECTORS=${DAP_SECTORS}
        -P ${CMAKE_SOURCE_DIR}/config/make_image.cmake
        DEPENDS ${BUILD_DIR}/boot.bin ${BUILD_DIR}/kernel.bin
        COMMENT "Building os.img"
)

add_custom_target(os_image ALL DEPENDS ${BUILD_DIR}/os.img)

# run in QEMU
add_custom_target(run
        COMMAND qemu-system-i386 -drive format=raw,file=${BUILD_DIR}/os.img
        DEPENDS os_image
)   