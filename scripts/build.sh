#!/bin/sh

set -e

mkdir -p build

nasm -f bin src/boot.asm -o build/boot.bin
nasm -f elf32 src/kernel_entry.asm -o build/kernel_entry.o
gcc -m32 -ffreestanding -fno-pic -fno-pie -c src/kernel.c -o build/kernel.o
gcc -m32 -ffreestanding -fno-pic -fno-pie -c src/arch/io.c -o build/io.o
gcc -m32 -ffreestanding -fno-pic -fno-pie -c src/drivers/vga.c -o build/vga.o
gcc -m32 -ffreestanding -fno-pic -fno-pie -c src/drivers/keyboard.c -o build/keyboard.o
ld -m elf_i386 -T config/linker.ld -o build/kernel.bin --oformat=binary \
	build/kernel_entry.o build/kernel.o build/io.o build/vga.o build/keyboard.o
cat build/boot.bin build/kernel.bin > build/os.img
truncate -s 2560 build/os.img