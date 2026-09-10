#include "vga.h"
#include "../arch/io.h"

static int cursor_index = 0;

// Update the hardware cursor position.
static void update_cursor(int index) {
    outb(0x3D4, 0x0F);
    outb(0x3D5, (unsigned char)(index & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (unsigned char)((index >> 8) & 0xFF));
}

void clear_screen(void) {
    // VGA text mode stores each cell as character plus color.
    char *video = (char*)0xB8000;

    for (int index = 0; index < 80 * 25 * 2; index += 2) {
        video[index] = ' ';
        video[index + 1] = 0x0F;
    }

    cursor_index = 0;
    update_cursor(cursor_index);
}

int get_cursor_index (void) {
    return cursor_index;
}

void print_char(char character, unsigned char color) {
    char *video = (char*)0xB8000;

    if (character == '\n') {
        cursor_index = (cursor_index / 80 + 1) * 80;
    } else if (character == '\b') {
        // Move back and clear the previous screen cell.
        if (cursor_index > 0) {
            cursor_index--;
            video[cursor_index * 2] = ' ';
            video[cursor_index * 2 + 1] = color;
        }
    } else {
        video[cursor_index * 2] = character;
        video[cursor_index * 2 + 1] = color;
        cursor_index++;
    }

    update_cursor(cursor_index);
}

void print_string(const char *string, unsigned char color) {
    // Print until the null terminator.
    while (*string != '\0') {
        print_char(*string, color);
        string++;
    }
}
