#include "pic.h"
#include "io.h"

// Remap PIC offsets to avoid conflicts with CPU exceptions
void pic_remap (void) {
    // Start PIC initialization (ICW1)
    outb(0x20, 0x11); 
    outb(0xA0, 0x11);

    // Set vector offsets: Master PIC starts at 0x20, Slave at 0x28 (ICW2)
    outb(0x21, 0x20); 
    outb(0xA1, 0x28);

    // Configure cascading: Master specifies slave at IRQ2, Slave specifies cascade identity (ICW3)
    outb(0x21, 0x04); 
    outb(0xA1, 0x02); 

    // Set 8086/88 compatible mode (ICW4)
    outb(0x21, 0x01); 
    outb(0xA1, 0x01);

    // Configure interrupt masks: Unmask timer (IRQ0) and keyboard (IRQ1)
    outb(0x21, 0xFC);
    outb(0xA1, 0xFF);
}

// Send End-of-Interrupt (EOI) signal to acknowledge handled hardware interrupts
void pic_send_eoi(unsigned char irq) {
    // Acknowledge slave PIC if the interrupt urik osirik originated from it
    if (irq >= 8) {
        outb(0xA0, 0x20);
    }

    // Always acknowledge the master PIC
    outb(0x20, 0x20);
}