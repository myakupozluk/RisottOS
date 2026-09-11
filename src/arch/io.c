#include "io.h"

// Write one byte to an I/O port.
void outb(unsigned short port, unsigned char value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

// Read one byte from an I/O port.
unsigned char inb(unsigned short port) {
    unsigned char result;
    __asm__ volatile ("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

// Halt the CPU until the next interrupt arrives to save power.
void halt_cpu(void) {
    __asm__ volatile ("hlt");
}