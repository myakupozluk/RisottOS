# RisottOS

RisottOS is a small bare-metal operating system for x86-compatible systems. It boots into 32-bit protected mode and provides VGA text output, cursor management, keyboard input, and basic backspace support.

## Features

- 16-bit BIOS bootloader
- 32-bit protected mode
- Global Descriptor Table (GDT)
- Freestanding C kernel
- VGA text-mode output
- Hardware cursor management
- PS/2 keyboard input
- Modular source layout

## Requirements

- GCC with 32-bit compilation support
- NASM
- GNU binutils (`ld`)
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
sudo apt install build-essential gcc-multilib nasm binutils qemu-system-x86 git
```

### Fedora

```sh
sudo dnf install gcc glibc-devel.i686 libgcc.i686 nasm binutils qemu-system-x86 git
```

### Arch Linux

```sh
sudo pacman -S --needed base-devel lib32-gcc-libs nasm binutils qemu-desktop git
```

### openSUSE

```sh
sudo zypper install gcc gcc-32bit nasm binutils qemu-x86 git
```

Package names may vary slightly between distribution versions. The required tools are GCC, 32-bit GCC support, NASM, GNU binutils, QEMU, and Git.

## Build

Build the bootloader, kernel, and disk image with:

```sh
./scripts/build.sh
```

Generated files are written to the `build/` directory:

- `boot.bin`: bootloader binary
- `kernel.bin`: flat kernel binary
- `os.img`: bootable disk image

## Run

After building the image, start RisottOS in QEMU:

```sh
./scripts/run.sh
```

The kernel prints a startup message and accepts basic keyboard input. Pressing Backspace removes the previous character.

## Project Structure

```text
src/
├── arch/
│   ├── io.c
│   └── io.h
├── drivers/
│   ├── keyboard.c
│   ├── keyboard.h
│   ├── vga.c
│   └── vga.h
├── boot.asm
├── kernel.c
└── kernel_entry.asm

config/
└── linker.ld

scripts/
├── build.sh
├── run.sh
└── git.sh
```

## Development Notes

RisottOS is an educational project and is still under development. It currently uses BIOS services and a simple CHS disk read, so the bootloader is intended for a small disk image and does not yet provide a full filesystem or interrupt-driven input system.

## License

See [LICENSE](LICENSE) for license information.
