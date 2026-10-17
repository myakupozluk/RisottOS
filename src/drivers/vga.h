#ifndef R_VGA_H
#define R_VGA_H

#define VGA_COLS 80
#define VGA_ROWS 25

// ---- colors -------------------------------------------------------------
enum {
    VGA_BLACK = 0, VGA_BLUE, VGA_GREEN, VGA_CYAN, VGA_RED, VGA_MAGENTA,
    VGA_BROWN, VGA_LIGHT_GRAY, VGA_DARK_GRAY, VGA_LIGHT_BLUE, VGA_LIGHT_GREEN,
    VGA_LIGHT_CYAN, VGA_LIGHT_RED, VGA_LIGHT_MAGENTA, VGA_YELLOW, VGA_WHITE
};

// Build a color attribute byte: VGA_COLOR(VGA_WHITE, VGA_BLUE)
// Note: backgrounds 8-15 blink unless you call vga_set_blink(0).
#define VGA_COLOR(fg, bg) ((unsigned char)(((bg) << 4) | (fg)))

// Box styles for vga_draw_box
#define VGA_BOX_SINGLE 0
#define VGA_BOX_DOUBLE 1

// ---- basic text output (unchanged) ----------------------------------------
void clear_screen(void);
void print_char(char character, unsigned char color);
void print_string(const char *string, unsigned char color);
void scroll_screen(void);
int  get_cursor_index(void);

// ---- text-mode graphics ---------------------------------------------------
// All coordinates are (column x 0-79, row y 0-24). Everything is clipped to the
// screen, and none of these move the text cursor.
void vga_putc_at(int x, int y, char c, unsigned char color);
void vga_puts_at(int x, int y, const char *s, unsigned char color);
void vga_fill_rect(int x, int y, int w, int h, char ch, unsigned char color);
void vga_draw_box(int x, int y, int w, int h, unsigned char color, int style);
void vga_draw_window(int x, int y, int w, int h, const char *title,
                     unsigned char border_color, unsigned char fill_color);
void vga_progress_bar(int x, int y, int w, unsigned int percent,
                      unsigned char fill_color, unsigned char empty_color);

// ---- cursor / screen state ------------------------------------------------
void vga_set_cursor(int x, int y);
void vga_hide_cursor(void);
void vga_show_cursor(void);
void vga_set_blink(int enabled);   // 0 = 16 background colors, 1 = blinking text (default)
void vga_save_screen(void);        // remember the screen + cursor (one slot)
void vga_restore_screen(void);     // bring it back

#endif