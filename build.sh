nasm -f bin boot.asm -o boot.bin
nasm -f elf32 kernel_entry.asm -o kernel_entry.o
gcc -m32 -ffreestanding -fno-pic -fno-pie -c kernel.c -o kernel.o
ld -m elf_i386 -T linker.ld -o kernel.bin --oformat=binary kernel_entry.o kernel.o
cat boot.bin kernel.bin > os.img