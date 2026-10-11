# RisottOS Makefile
# ASSUMPTIONS (edit the variables below if your setup differs):
#   - cross compiler i686-elf-gcc / i686-elf-ld and nasm are installed
#   - bootloader source is src/boot.asm (flat binary)
#   - kernel is linked flat at 0x1000 with src/kernel_entry.asm first
#   - os.img boots as the primary IDE master (-hda). The ATA driver talks to
#     that same disk, so "ata test" uses a sector far away from the kernel.

CC      := i686-elf-gcc
LD      := i686-elf-ld
AS      := nasm
QEMU    := qemu-system-i386

BOOT_SRC     := src/boot.asm
KERNEL_ADDR  := 0x1000

CFLAGS := -ffreestanding -fno-pic -fno-stack-protector -O0 -Wall -Wextra -Isrc

BUILD := build

# C sources: kernel, arch and drivers (ata.c is picked up by the wildcard)
C_SRC   := $(wildcard src/*.c src/arch/*.c src/drivers/*.c)
C_OBJ   := $(patsubst src/%.c,$(BUILD)/%.o,$(C_SRC))

# Assembly that is linked into the kernel must be ELF objects (NOT -f bin).
# They use the suffix .asm.o so interrupts.asm does not collide with
# interrupts.c (both would otherwise become build/arch/interrupts.o).
ASM_OBJ := $(BUILD)/kernel_entry.asm.o $(BUILD)/arch/interrupts.asm.o

OS_IMG   := os.img

all: $(OS_IMG)

# ---- bootloader ----
$(BUILD)/boot.bin: $(BOOT_SRC)
	@mkdir -p $(dir $@)
	$(AS) -f bin $< -o $@

# ---- kernel ----
$(BUILD)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.asm.o: src/%.asm
	@mkdir -p $(dir $@)
	$(AS) -f elf32 $< -o $@

# kernel_entry must come first so execution starts there
$(BUILD)/kernel.bin: $(ASM_OBJ) $(C_OBJ)
	$(LD) -Ttext $(KERNEL_ADDR) --oformat binary -o $@ $(BUILD)/kernel_entry.asm.o $(filter-out $(BUILD)/kernel_entry.asm.o,$^)

# ---- image ----
# boot sector + kernel, padded to 1.44 MB (2880 sectors) so the ATA test
# sector exists and the BIOS never reads past the end of the disk
$(OS_IMG): $(BUILD)/boot.bin $(BUILD)/kernel.bin
	cat $^ > $@
	truncate -s 1474560 $@

run: all
	$(QEMU) -drive file=$(OS_IMG),format=raw,if=ide,index=0,media=disk

clean:
	rm -rf $(BUILD) $(OS_IMG)

.PHONY: all run clean