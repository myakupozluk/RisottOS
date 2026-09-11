#!/bin/sh

set -e

mkdir -p build

nasm -f bin src/boot.asm -o build/boot.bin
nasm -f elf32 src/kernel_entry.asm -o build/kernel_entry.o
nasm -f elf32 src/arch/interrupts.asm -o build/interrupt_stubs.o
gcc -m32 -ffreestanding -fno-pic -fno-pie -c src/kernel.c -o build/kernel.o
gcc -m32 -ffreestanding -fno-pic -fno-pie -c src/arch/io.c -o build/io.o
gcc -m32 -ffreestanding -fno-pic -fno-pie -c src/arch/idt.c -o build/idt.o
gcc -m32 -ffreestanding -fno-pic -fno-pie -c src/arch/pic.c -o build/pic.o
gcc -m32 -ffreestanding -fno-pic -fno-pie -c src/arch/interrupts.c -o build/interrupts.o
gcc -m32 -ffreestanding -fno-pic -fno-pie -c src/arch/shell.c -o build/shell.o
gcc -m32 -ffreestanding -fno-pic -fno-pie -c src/drivers/vga.c -o build/vga.o
gcc -m32 -ffreestanding -fno-pic -fno-pie -c src/drivers/keyboard.c -o build/keyboard.o
gcc -m32 -ffreestanding -fno-pic -fno-pie -c src/drivers/timer.c -o build/timer.o
ld -m elf_i386 -T config/linker.ld -o build/kernel.bin --oformat=binary \
    build/kernel_entry.o build/interrupt_stubs.o build/kernel.o build/io.o build/idt.o build/pic.o build/interrupts.o build/shell.o build/vga.o build/keyboard.o build/timer.o
cat build/boot.bin build/kernel.bin > build/os.img
truncate -s 4608 build/os.img