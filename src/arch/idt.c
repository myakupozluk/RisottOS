#include "idt.h"
extern void interrupt_handler(void);
extern void timer_interrupt_handler(void);

struct idt_entry
{
    // Split the 32-bit handler address into two parts.
    unsigned short base_low;
    unsigned short selector;
    unsigned char zero;
    unsigned char type_attributes;
    unsigned short base_high;
} __attribute__((packed));

struct idt_pointer
{
    // CPU uses this descriptor to locate the IDT.
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_pointer idtr;

void idt_init (void) {
    int i;

    // Clear all interrupt gates before installing handlers.
    for (i = 0; i < 256; i++) {
        idt[i].base_low = 0;
        idt[i].selector = 0;
        idt[i].zero = 0;
        idt[i].type_attributes = 0;
        idt[i].base_high = 0;
    }
    idt_set_gate(0x21, (unsigned int)interrupt_handler);
    idt_set_gate(0x20, (unsigned int)timer_interrupt_handler);

    // Load the IDT address and size into the CPU.
    idtr.limit = sizeof(idt) - 1;
    idtr.base = (unsigned int)idt;
    __asm__ volatile ("lidt %0" : : "m"(idtr));
}

void idt_set_gate(unsigned char number, unsigned int addr) {
    // Install a 32-bit interrupt gate.
    idt[number].base_low = addr & 0xFFFF;
    idt[number].selector = 0x08;
    idt[number].zero = 0;
    idt[number].type_attributes = 0x8E;
    idt[number].base_high = (addr >> 16) & 0xFFFF;
}