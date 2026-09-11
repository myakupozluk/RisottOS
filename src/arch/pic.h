#ifndef R_PIC_H
#define R_PIC_H

void pic_remap(void);
void pic_send_eoi(unsigned char irq);

#endif