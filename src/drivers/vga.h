#ifndef R_VGA_H
#define R_VGA_H

void clear_screen(void);
void print_char(char character, unsigned char color);
void print_string(const char *string, unsigned char color);
void scroll_screen(void);
int get_cursor_index(void);

#endif
