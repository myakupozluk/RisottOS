#ifndef R_GFX_H
#define R_GFX_H

// Pixel graphics: VGA mode 13h, 320x200, 256 colors, one byte per pixel.
// Entering this mode takes over the screen; call gfx_exit() to get the text
// shell back.

#define GFX_W 320
#define GFX_H 200

// Palette (loaded by gfx_enter):
//    0-15   the 16 standard text colors (same order as VGA_BLACK ... VGA_WHITE)
//   16-31   16 shades of gray, black to white
//   32-247  6x6x6 color cube
#define GFX_BLACK        0
#define GFX_BLUE         1
#define GFX_GREEN        2
#define GFX_CYAN         3
#define GFX_RED          4
#define GFX_MAGENTA      5
#define GFX_BROWN        6
#define GFX_LIGHT_GRAY   7
#define GFX_DARK_GRAY    8
#define GFX_LIGHT_BLUE   9
#define GFX_LIGHT_GREEN  10
#define GFX_LIGHT_CYAN   11
#define GFX_LIGHT_RED    12
#define GFX_LIGHT_MAGENTA 13
#define GFX_YELLOW       14
#define GFX_WHITE        15

#define GFX_GRAY(n)      ((unsigned char)(16 + (n)))                       // n = 0..15
#define GFX_RGB(r, g, b) ((unsigned char)(32 + 36 * (r) + 6 * (g) + (b)))  // r,g,b = 0..5

// ---- mode control ---------------------------------------------------------
int  gfx_enter(void);          // switch to 320x200x256; returns 0
void gfx_exit(void);           // back to 80x25 text (screen is cleared)
int  gfx_active(void);         // 1 while in graphics mode
void gfx_set_color(unsigned char index, unsigned char r, unsigned char g, unsigned char b); // r,g,b = 0..63

// ---- drawing (everything is clipped to the screen) ---------------------------
void gfx_clear(unsigned char color);
void gfx_putpixel(int x, int y, unsigned char color);
unsigned char gfx_getpixel(int x, int y);
void gfx_hline(int x1, int x2, int y, unsigned char color);
void gfx_vline(int x, int y1, int y2, unsigned char color);
void gfx_line(int x1, int y1, int x2, int y2, unsigned char color);
void gfx_rect(int x, int y, int w, int h, unsigned char color);        // outline
void gfx_fill_rect(int x, int y, int w, int h, unsigned char color);
void gfx_circle(int cx, int cy, int r, unsigned char color);           // outline
void gfx_fill_circle(int cx, int cy, int r, unsigned char color);

// ---- text (8x16 glyphs taken from the VGA text font; 40 x 12 characters) ----
// bg < 0 means a transparent background.
void gfx_char(int x, int y, char ch, unsigned char fg, int bg);
void gfx_text(int x, int y, const char *s, unsigned char fg, int bg);  // '\n' starts a new line

// ---- timing ----------------------------------------------------------------
void gfx_wait_vsync(void);     // wait for the start of the next frame (~60 per second)

#endif
