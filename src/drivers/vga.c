#include "vga.h"
#include "../arch/io.h"

// Text-mode video memory. (Overridable so the code can be tested on a PC.)
#ifndef VGA_MEM
#define VGA_MEM ((volatile unsigned char *)0xB8000)
#endif

// Code page 437 characters used for drawing
#define CH_FULL   ((char)0xDB)
#define CH_SHADE  ((char)0xB0)

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
    volatile unsigned char *video = VGA_MEM;

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
    volatile unsigned char *video = VGA_MEM;

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
        video[cursor_index * 2] = (unsigned char)character;
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
    volatile unsigned char *video = VGA_MEM;
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

// ======================================================================
//  Text-mode graphics
// ======================================================================

// Write one cell at (x, y). Anything off-screen is ignored.
void vga_putc_at(int x, int y, char c, unsigned char color) {
    volatile unsigned char *video = VGA_MEM;

    if (x < 0 || x >= VGA_COLS || y < 0 || y >= VGA_ROWS) return;

    int i = (y * VGA_COLS + x) * 2;
    video[i] = (unsigned char)c;
    video[i + 1] = color;
}

// Write a string starting at (x, y). No wrapping: it stops at the screen edge.
void vga_puts_at(int x, int y, const char *s, unsigned char color) {
    while (*s != '\0') {
        vga_putc_at(x, y, *s, color);
        x++;
        s++;
    }
}

// Fill a w x h rectangle with one character and color.
void vga_fill_rect(int x, int y, int w, int h, char ch, unsigned char color) {
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            vga_putc_at(x + i, y + j, ch, color);
        }
    }
}

// Draw a box outline (w and h include the border, so they must be >= 2).
void vga_draw_box(int x, int y, int w, int h, unsigned char color, int style) {
    char tl, tr, bl, br, hz, vt;

    if (w < 2 || h < 2) return;

    if (style == VGA_BOX_DOUBLE) {
        tl = (char)0xC9; tr = (char)0xBB; bl = (char)0xC8; br = (char)0xBC;
        hz = (char)0xCD; vt = (char)0xBA;
    } else {
        tl = (char)0xDA; tr = (char)0xBF; bl = (char)0xC0; br = (char)0xD9;
        hz = (char)0xC4; vt = (char)0xB3;
    }

    for (int i = 1; i < w - 1; i++) {
        vga_putc_at(x + i, y, hz, color);
        vga_putc_at(x + i, y + h - 1, hz, color);
    }
    for (int j = 1; j < h - 1; j++) {
        vga_putc_at(x, y + j, vt, color);
        vga_putc_at(x + w - 1, y + j, vt, color);
    }
    vga_putc_at(x, y, tl, color);
    vga_putc_at(x + w - 1, y, tr, color);
    vga_putc_at(x, y + h - 1, bl, color);
    vga_putc_at(x + w - 1, y + h - 1, br, color);
}

// A window: cleared interior, double-line border, and a title in the top edge.
void vga_draw_window(int x, int y, int w, int h, const char *title,
                     unsigned char border_color, unsigned char fill_color) {
    if (w < 2 || h < 2) return;

    vga_fill_rect(x, y, w, h, ' ', fill_color);
    vga_draw_box(x, y, w, h, border_color, VGA_BOX_DOUBLE);

    if (title != 0) {
        // Centre the title; cut it if it doesn't fit between the corners.
        int len = 0;
        while (title[len] != '\0') len++;
        if (len > w - 2) len = w - 2;

        int start = x + 1 + ((w - 2) - len) / 2;
        for (int i = 0; i < len; i++) {
            vga_putc_at(start + i, y, title[i], border_color);
        }
    }
}

// A horizontal progress bar w cells wide; percent is clamped to 0-100.
void vga_progress_bar(int x, int y, int w, unsigned int percent,
                      unsigned char fill_color, unsigned char empty_color) {
    if (w <= 0) return;
    if (percent > 100) percent = 100;

    int filled = (int)(((unsigned int)w * percent) / 100u);

    for (int i = 0; i < w; i++) {
        if (i < filled) vga_putc_at(x + i, y, CH_FULL, fill_color);
        else            vga_putc_at(x + i, y, CH_SHADE, empty_color);
    }
}

// ======================================================================
//  Cursor / screen state
// ======================================================================

// Move the text cursor (affects where print_char writes next).
void vga_set_cursor(int x, int y) {
    if (x < 0) x = 0;
    if (x >= VGA_COLS) x = VGA_COLS - 1;
    if (y < 0) y = 0;
    if (y >= VGA_ROWS) y = VGA_ROWS - 1;

    cursor_index = y * VGA_COLS + x;
    update_cursor(cursor_index);
}

void vga_hide_cursor(void) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);          // bit 5 = cursor disabled
}

void vga_show_cursor(void) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x0D);          // scanline 13 ...
    outb(0x3D4, 0x0B);
    outb(0x3D5, 0x0E);          // ... to 14 (underline cursor in an 8x16 cell)
}

// By default VGA uses bit 7 of the color byte as "blink". Turning blink off
// gives you all 16 colors as backgrounds.
void vga_set_blink(int enabled) {
    unsigned char mode;

    (void)inb(0x3DA);           // reset the attribute controller flip-flop
    outb(0x3C0, 0x10);          // select the mode control register
    mode = (unsigned char)inb(0x3C1);
    if (enabled) mode |= 0x08;
    else         mode &= (unsigned char)~0x08;
    outb(0x3C0, mode);
    (void)inb(0x3DA);
    outb(0x3C0, 0x20);          // re-enable video output
}

// One save slot for the whole text screen, so a window can pop up and vanish.
static unsigned char saved_screen[VGA_COLS * VGA_ROWS * 2];
static int saved_cursor = 0;

void vga_save_screen(void) {
    volatile unsigned char *video = VGA_MEM;
    for (int i = 0; i < VGA_COLS * VGA_ROWS * 2; i++) saved_screen[i] = video[i];
    saved_cursor = cursor_index;
}

void vga_restore_screen(void) {
    volatile unsigned char *video = VGA_MEM;
    for (int i = 0; i < VGA_COLS * VGA_ROWS * 2; i++) video[i] = saved_screen[i];
    cursor_index = saved_cursor;
    update_cursor(cursor_index);
}