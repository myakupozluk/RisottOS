# RisottOS

RisottOS is a small bare-metal operating system for x86-compatible systems. It boots into 32-bit protected mode and provides VGA text output, cursor management, keyboard input, a hardware timer, a real-time clock, and an interactive shell.

## Features

- 16-bit BIOS bootloader with LBA (extended INT 13h) disk reads
- 32-bit protected mode
- Global Descriptor Table (GDT)
- Interrupt Descriptor Table (IDT) and PIC remapping
- Freestanding C kernel
- VGA text-mode output with hardware cursor management
- PS/2 keyboard input (interrupt-driven, circular buffer)
- Programmable Interval Timer (PIT) with tick-based sleep
- Real-time clock (RTC) reading
- Interactive shell with built-in commands (`clear`, `help`, `echo`, `rtc`, `sleep`)
- CMake-based build system
- Modular source layout

## Requirements

- GCC with 32-bit compilation support
- NASM
- GNU binutils (`ld`)
- CMake (3.16 or later)
- QEMU
- Git

## Installation

Clone the repository:

```sh
git clone https://github.com/myakupozluk/RisottOS.git
cd RisottOS
```

### Debian and Ubuntu

```sh
sudo apt update
sudo apt install build-essential gcc-multilib nasm binutils cmake qemu-system-x86 git
```

### Fedora

```sh
sudo dnf install gcc glibc-devel.i686 libgcc.i686 nasm binutils cmake qemu-system-x86 git
```

### Arch Linux

```sh
sudo pacman -S --needed base-devel lib32-gcc-libs nasm binutils cmake qemu-full git
```

### openSUSE

```sh
sudo zypper install gcc gcc-32bit nasm binutils cmake qemu-x86 git
```

Package names may vary slightly between distribution versions. The required tools are GCC, 32-bit GCC support, NASM, GNU binutils, CMake, QEMU, and Git.

## Build

Build the bootloader, kernel, and disk image with:

```sh
mkdir -p build && cd build
cmake ..
make
```

Generated files are written to the `build/` directory (or your chosen build directory):

- `boot.bin`: bootloader binary
- `kernel.bin`: flat kernel binary
- `os.img`: bootable disk image

## Run

After building, start RisottOS in QEMU:

```sh
make run
```

This launches `os.img` in QEMU. The kernel prints a startup message and drops into an interactive shell.

## Shell Commands

| Command | Description |
|---|---|
| `clear` | Clear the screen |
| `help` | Show available commands |
| `echo <msg>` | Print text |
| `rtc get-time` | Show current time from the real-time clock |
| `rtc get-seconds` / `get-minutes` / `get-hours` | Show individual RTC fields |
| `sleep <seconds>` | Pause execution for the given number of seconds |

## Project Structure

```text
src/
├── arch/
│   ├── idt.c / idt.h
│   ├── interrupts.c / interrupts.asm
│   ├── io.c / io.h
│   ├── pic.c / pic.h
│   └── shell.c / shell.h
├── drivers/
│   ├── keyboard.c / keyboard.h
│   ├── rtc.c / rtc.h
│   ├── timer.c / timer.h
│   └── vga.c / vga.h
├── boot.asm
├── kernel.c
└── kernel_entry.asm

config/
├── linker.ld
└── make_image.cmake

CMakeLists.txt
```

## Development Notes

RisottOS is an educational project and is still under active development. The bootloader uses BIOS extended (LBA) disk reads rather than CHS addressing, avoiding the geometry limitations of older BIOS calls. There is currently no filesystem; the kernel is loaded as a flat binary directly from fixed disk sectors.

## License

See [LICENSE](LICENSE) for license information.