#ifndef R_IO_H
#define R_IO_H

void outb(unsigned short port, unsigned char value);
unsigned char inb(unsigned short port);
void halt_cpu(void);

#endif
