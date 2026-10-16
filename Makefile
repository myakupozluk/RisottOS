
# RisottOS Makefile
# Toolchain: i686-elf-gcc, i686-elf-ld, NASM, QEMU

CC      := i686-elf-gcc
LD      := i686-elf-ld
AS      := nasm
QEMU    := qemu-system-i386

BUILD   := build
OS_IMG  := os.img

BOOT_SRC    := src/boot.asm
KERNEL_ADDR := 0x1000

# Freestanding 32-bit kernel compilation flags
CFLAGS := -m32 -std=gnu99 -ffreestanding -fno-pic \
          -fno-stack-protector -fno-builtin \
          -fno-asynchronous-unwind-tables -fno-unwind-tables \
          -O0 -Wall -Wextra -Isrc

# All kernel C sources, including the RFS filesystem.
C_SRC := $(wildcard \
    src/*.c \
    src/arch/*.c \
    src/drivers/*.c \
    src/mem/*.c \
    src/rfs/*.c \
)

C_OBJ := $(patsubst src/%.c,$(BUILD)/%.o,$(C_SRC))

# Assembly files linked as ELF32 objects.
ASM_OBJ := \
    $(BUILD)/kernel_entry.asm.o \
    $(BUILD)/arch/interrupts.asm.o

.PHONY: all run clean mem

all: $(OS_IMG)

# --------------------------------------------------
# Bootloader
# --------------------------------------------------

$(BUILD)/boot.bin: $(BOOT_SRC)
	@mkdir -p $(dir $@)
	$(AS) -f bin $< -o $@

# --------------------------------------------------
# C source compilation
# --------------------------------------------------

$(BUILD)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# --------------------------------------------------
# Assembly compilation
# --------------------------------------------------

$(BUILD)/%.asm.o: src/%.asm
	@mkdir -p $(dir $@)
	$(AS) -f elf32 $< -o $@

# --------------------------------------------------
# Kernel linking
# --------------------------------------------------

$(BUILD)/kernel.bin: $(ASM_OBJ) $(C_OBJ)
	$(LD) -m elf_i386 -Ttext $(KERNEL_ADDR) \
	    --oformat binary -o $@ \
	    $(BUILD)/kernel_entry.asm.o \
	    $(filter-out $(BUILD)/kernel_entry.asm.o,$^)

# --------------------------------------------------
# Disk image
# --------------------------------------------------

$(OS_IMG): $(BUILD)/boot.bin $(BUILD)/kernel.bin
	cat $^ > $@
	truncate -s 64M $@

# --------------------------------------------------
# Run in QEMU
# --------------------------------------------------

run: all
	$(QEMU) -drive file=$(OS_IMG),format=raw,if=ide,index=0,media=disk

# --------------------------------------------------
# Build memory manager objects
# --------------------------------------------------

mem: $(filter $(BUILD)/mem/%.o,$(C_OBJ))
	@echo "Memory manager components compiled successfully."

# --------------------------------------------------
# Clean generated files
# --------------------------------------------------

clean:
	rm -rf $(BUILD) $(OS_IMG)
# Convert raw disk image to VirtualBox VDI
VDI_DIR := /d/VirtualBox
VDI_IMG := $(VDI_DIR)/RisottOS.vdi

.PHONY: vdi
vdi: os.img
	mkdir -p "$(VDI_DIR)"
	VBoxManage convertfromraw "os.img" "$(VDI_IMG)" --format VDI