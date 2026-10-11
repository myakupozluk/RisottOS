# RisottOS

RisottOS is a small educational bare-metal operating system for 32-bit x86 PCs. It boots through a BIOS bootloader, switches to 32-bit protected mode, loads a freestanding C kernel, and provides VGA text output, interrupt-driven input, timer support, RTC access, and a basic interactive shell.

> **Current scope:** RisottOS targets BIOS-based i386-compatible systems and runs in QEMU. It is not an x86-64 operating system and does not currently use paging, user mode, or a filesystem.

## Features

- 16-bit BIOS bootloader
- BIOS Extended Disk Drive Services (LBA / `INT 13h`, function `42h`)
- 32-bit protected mode with a flat Global Descriptor Table (GDT)
- Interrupt Descriptor Table (IDT)
- Programmable Interrupt Controller (PIC) remapping
- Freestanding C kernel
- VGA text-mode output with hardware cursor management
- PS/2 keyboard input using IRQ1 and a circular input buffer
- Programmable Interval Timer (PIT) with tick-based sleep
- Real-time clock (RTC) time reading
- ATA PIO sector read/write support
- Interactive shell
- CMake-based build configuration
- A legacy Makefile-based build path

## Requirements

For the CMake build:

- GCC with 32-bit compilation support
- NASM
- GNU binutils, including `ld`
- CMake 3.16 or later
- QEMU with `qemu-system-i386`
- Git

The repository also contains a separate Makefile that expects an `i686-elf-gcc` cross-compiler and `i686-elf-ld` to be available in `PATH`.

## Installation

Clone the repository:

```sh
git clone https://github.com/risottos-team/RisottOS.git
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

Package names may vary between distribution versions. The required tools are a C compiler with 32-bit support, NASM, GNU binutils, CMake, QEMU, and Git.

## Build with CMake

The CMake workflow generates the bootloader, kernel, and disk image inside the build directory:

```sh
cmake -S . -B build
cmake --build build
```

The generated files are:

- `build/boot.bin` — BIOS boot sector
- `build/kernel.bin` — flat 32-bit kernel binary
- `build/os.img` — bootable raw disk image

Run the image with:

```sh
cmake --build build --target run
```

Or launch QEMU directly:

```sh
qemu-system-i386 -drive format=raw,file=build/os.img
```

To remove CMake-generated files:

```sh
rm -rf build
```

## Alternative Makefile workflow

The repository Makefile uses the `i686-elf-gcc` and `i686-elf-ld` cross-compiler toolchain:

```sh
make
make run
```

This workflow creates `os.img` in the repository root and uses the `build/` directory for intermediate files. Use the CMake workflow above unless you specifically need the cross-compiler build.

## Shell Commands

After booting, the shell displays a `RisottOS>` prompt.

| Command | Description |
|---|---|
| `clear` | Clear the VGA text screen |
| `help` | Show the available shell commands |
| `echo <message>` | Print a message |
| `rtc` | Show RTC-related commands |
| `rtc get-time` | Show the current time as `HH:MM:SS` |
| `rtc get-seconds` | Show the current seconds value |
| `rtc get-minutes` | Show the current minutes value |
| `rtc get-hours` | Show the current hours value |
| `sleep <seconds>` | Wait for the specified number of seconds |
| `ata` | Show ATA command usage |
| `ata read <lba>` | Read and display one 512-byte sector |
| `ata write <lba> <text>` | Write text to one sector |
| `ata test` | Write and read back a test sector |

The shell protects the bootloader and kernel area by refusing `ata write` operations below LBA 100. The current image layout supports LBAs 0 through 2879.

## Project Structure

```text
.
├── CMakeLists.txt          # CMake build configuration
├── Makefile                # Alternative cross-compiler build
├── config/
│   ├── linker.ld           # Kernel linker script
│   └── make_image.cmake    # Bootloader/kernel image assembly
├── scripts/
│   └── build_and_run.sh    # Local build-and-run helper
└── src/
    ├── arch/
    │   ├── idt.c/.h        # Interrupt Descriptor Table
    │   ├── interrupts.c    # C interrupt handlers
    │   ├── interrupts.asm  # Assembly interrupt stubs
    │   ├── io.c/.h         # Port I/O helpers
    │   ├── pic.c/.h        # PIC remapping and EOI handling
    │   └── shell.c/.h      # Interactive shell and command parsing
    ├── drivers/
    │   ├── ata.c/.h        # ATA PIO disk access
    │   ├── keyboard.c/.h   # PS/2 keyboard input
    │   ├── rtc.c/.h        # RTC access
    │   ├── timer.c/.h      # PIT timer and sleeping
    │   └── vga.c/.h        # VGA text output
    ├── boot.asm            # BIOS bootloader and protected-mode setup
    ├── kernel.c            # Kernel initialization
    └── kernel_entry.asm    # Kernel entry point
```

## Development Notes and Limitations

- The system currently boots through legacy BIOS; UEFI boot is not supported.
- The kernel runs in 32-bit protected mode and uses a flat memory model.
- There is no paging, process isolation, userspace, filesystem, or standard C library.
- The bootloader currently reads a fixed number of sectors from the boot disk. Keep the kernel image within the configured disk-read limit.
- ATA commands access the emulated primary IDE disk in QEMU and are intended for experimentation.
- There is no automated test suite. Manual smoke testing in QEMU is currently the primary validation method.
- RisottOS is an educational project and is under active development.

## Core Team

- **[Risotto](https://github.com/myakupozluk)** — Lead Developer / Owner
- **[Dev10n](https://github.com/dev10n)** — Core Maintainer / Owner

## License

See [LICENSE](LICENSE) for license information.
