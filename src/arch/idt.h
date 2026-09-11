#ifndef R_IDT_H
#define R_IDT_H

void idt_init(void);
void idt_set_gate(unsigned char number, unsigned int addr);

#endif