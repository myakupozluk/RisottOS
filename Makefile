
CC      := i686-elf-gcc
LD      := i686-elf-ld
AS      := nasm
QEMU    := qemu-system-i386

BUILD   := build
OS_IMG  := os.img

# VirtualBox
VBOXMANAGE := /c/Program Files/Oracle/VirtualBox/VBoxManage.exe
VDI_DIR    := /d/VirtualBox
VDI_IMG    := $(VDI_DIR)/RisottOS.vdi

# Bootloader
BOOT_SRC := src/boot.asm
KERNEL_ADDR := 0x1000

# Compiler flags
CFLAGS := -m32 -std=gnu99 -ffreestanding -fno-pic \
          -fno-stack-protector -fno-builtin \
          -fno-asynchronous-unwind-tables -fno-unwind-tables \
          -O0 -Wall -Wextra -Isrc

# C source files
C_SRC := $(wildcard \
    src/*.c \
    src/arch/*.c \
    src/drivers/*.c \
    src/drivers/gfx/*.c \
    src/mem/*.c \
    src/rfs/*.c \
)

C_OBJ := $(patsubst src/%.c,$(BUILD)/%.o,$(C_SRC))

# Assembly object files
ASM_OBJ := \
    $(BUILD)/kernel_entry.asm.o \
    $(BUILD)/arch/interrupts.asm.o

# Default target
all: $(OS_IMG)

# Build boot sector
$(BUILD)/boot.bin: $(BOOT_SRC)
	@mkdir -p $(dir $@)
	$(AS) -f bin $< -o $@

# Compile C files
$(BUILD)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble ELF object files
$(BUILD)/%.asm.o: src/%.asm
	@mkdir -p $(dir $@)
	$(AS) -f elf32 $< -o $@

# Link kernel as a flat binary
$(BUILD)/kernel.bin: $(ASM_OBJ) $(C_OBJ)
	$(LD) -m elf_i386 -Ttext $(KERNEL_ADDR) \
	    --oformat binary -o $@ \
	    $(BUILD)/kernel_entry.asm.o \
	    $(filter-out $(BUILD)/kernel_entry.asm.o,$^)

# Create disk image
$(OS_IMG): $(BUILD)/boot.bin $(BUILD)/kernel.bin
	cat $^ > $@
	truncate -s 64M $@

# Run with QEMU
run: all
	$(QEMU) -drive file=$(OS_IMG),format=raw,if=ide,index=0,media=disk

# Compile memory manager components
mem: $(filter $(BUILD)/mem/%.o,$(C_OBJ))
	@echo "Memory manager components compiled successfully."

# Convert raw disk image to VirtualBox VDI
vdi: $(OS_IMG)
	@test -f "$(VBOXMANAGE)" || (echo "ERROR: VBoxManage.exe not found at $(VBOXMANAGE)"; exit 1)
	mkdir -p "$(VDI_DIR)"
	"$(VBOXMANAGE)" convertfromraw "$(OS_IMG)" "$(VDI_IMG)" --format VDI

# Clean generated files
clean:
	rm -rf $(BUILD) $(OS_IMG)

.PHONY: all run clean mem vdi
