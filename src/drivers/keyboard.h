#ifndef R_KEYBOARD_H
#define R_KEYBOARD_H

#define KEY_UP    0x01
#define KEY_DOWN  0x02

char get_key(void);
char keyboard_read(void);
void keyboard_irq_handler(void);

#endif
