#include "vga.h"
#include "../arch/io.h"

// Track the current linear text-mode cursor index (0 to 1999)
static int cursor_index = 0;

// Update the hardware cursor position using VGA CRTC control ports.
static void update_cursor(int index) {
    outb(0x3D4, 0x0F);
    outb(0x3D5, (unsigned char)(index & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (unsigned char)((index >> 8) & 0xFF));
}

// Clear the entire VGA text-mode buffer and reset the cursor position.
void clear_screen(void) {
    // VGA text mode stores each cell as character plus attribute byte (color).
    char *video = (char*)0xB8000;

    for (int index = 0; index < 80 * 25 * 2; index += 2) {
        video[index] = ' ';
        video[index + 1] = 0x0F;
    }

    cursor_index = 0;
    update_cursor(cursor_index);
}

// Return the current cursor index.
int get_cursor_index (void) {
    return cursor_index;
}

// Print a single character to the screen, handling special control characters.
void print_char(char character, unsigned char color) {
    char *video = (char*)0xB8000;

    // Scroll screen down if cursor exceeds the 25th row.
    if (cursor_index >= 2000) {
        scroll_screen();
        cursor_index -= 80;
    }

    if (character == '\n') {
        // Move cursor to the start of the next line.
        cursor_index = (cursor_index / 80 + 1) * 80;
    } else if (character == '\b') {
        // Move back and clear the previous screen cell.
        if (cursor_index > 0) {
            cursor_index--;
            video[cursor_index * 2] = ' ';
            video[cursor_index * 2 + 1] = color;
        }
    } else {
        // Write character and color attribute to video memory and advance cursor.
        video[cursor_index * 2] = character;
        video[cursor_index * 2 + 1] = color;
        cursor_index++;
    }

    update_cursor(cursor_index);
}

// Print a null-terminated string to the screen.
void print_string(const char *string, unsigned char color) {
    // Print until the null terminator.
    while (*string != '\0') {
        print_char(*string, color);
        string++;
    }
}

// Scroll the text-mode screen up by one row.
void scroll_screen (void) {
    char *video = (char*)0xB8000;
    int i;

    // Copy each row up by one line (160 bytes per row).
    for (i = 0; i < 3840; i++) {
        video[i] = video[i + 160];
    }

    // Clear the last row one VGA cell at a time.
    for (i = 3840; i < 4000; i += 2) {
        video[i] = ' ';
        video[i + 1] = 0x0F;
    }
}