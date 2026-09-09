int cursor_index = 0; // Current cursor position

// Send a byte to an I/O port (Assembly wrapper)
void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

// Update hardware cursor position
void update_cursor(int index) {
    outb(0x3D4, 0x0F);
    outb(0x3D5, (unsigned char)(index & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (unsigned char)((index >> 8) & 0xFF));
}

// Print a single character with color
void print_char(char c, unsigned char color) {
    char *video = (char*) 0xB8000; // VGA buffer

    if (c == '\n') {
        cursor_index = (cursor_index / 80 + 1) * 80; // Next line
    } else {
        video[cursor_index * 2] = c;     // Write character
        video[cursor_index * 2 + 1] = color; // Write color
        cursor_index++;                  // Advance cursor
    }
    
    update_cursor(cursor_index); // Move hardware cursor
}

// Print a string
void print_string(const char *str, unsigned char clr) {
    int i = 0;
    while (str[i] != '\0') {
        print_char(str[i], clr);
        i++;
    }
}

// Kernel entry point
void kmain() {
    char *video = (char*) 0xB8000; // VGA buffer
    int i;

    // Clear screen
    for (i = 0; i < 80 * 25 * 2; i += 2) {
        video[i] = ' ';
        video[i + 1] = 0x0F;
    }
    
    cursor_index = 0; // Reset cursor to top-left after clearing
    update_cursor(cursor_index);

    print_string("RisottOS booted succesfully!\n", 0x0F); // Print string & move cursor

    while (1) {
        /* Halt */
    }
}