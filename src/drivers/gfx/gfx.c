#include "gfx.h"
#include "../vga.h"
#include "../../arch/io.h"

// Video memory window at 0xA0000. (Overridable so the code can be tested on a PC.)
#ifndef GFX_FB
#define GFX_FB ((volatile unsigned char *)0xA0000)
#endif

static int g_active = 0;

// The BIOS text font lives in VGA plane 2 (256 glyphs x 32 bytes). Mode 13h
// overwrites it, so we copy it out before switching, restore it when leaving,
// and also use it to draw text in graphics mode.
static unsigned char g_font[8192];
static int g_font_saved = 0;

// ======================================================================
//  VGA register sets
// ======================================================================

typedef struct {
    unsigned char misc;
    unsigned char seq[5];
    unsigned char crtc[25];
    unsigned char gc[9];
    unsigned char ac[21];
} VgaRegs;

static const VgaRegs mode_13h = {
    0x63,
    { 0x03, 0x01, 0x0F, 0x00, 0x0E },
    { 0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0xBF, 0x1F, 0x00, 0x41, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x9C, 0x0E, 0x8F, 0x28, 0x40, 0x96, 0xB9, 0xA3, 0xFF },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x05, 0x0F, 0xFF },
    { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
      0x0D, 0x0E, 0x0F, 0x41, 0x00, 0x0F, 0x00, 0x00 }
};

static const VgaRegs mode_text = {
    0x67,
    { 0x03, 0x00, 0x03, 0x00, 0x02 },
    { 0x5F, 0x4F, 0x50, 0x82, 0x55, 0x81, 0xBF, 0x1F, 0x00, 0x4F, 0x0D, 0x0E, 0x00,
      0x00, 0x00, 0x50, 0x9C, 0x0E, 0x8F, 0x28, 0x1F, 0x96, 0xB9, 0xA3, 0xFF },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0E, 0x00, 0xFF },
    { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07, 0x38, 0x39, 0x3A, 0x3B, 0x3C,
      0x3D, 0x3E, 0x3F, 0x0C, 0x00, 0x0F, 0x08, 0x00 }
};

static void write_regs(const VgaRegs *r) {
    unsigned char crtc[25];
    int i;

    for (i = 0; i < 25; i++) crtc[i] = r->crtc[i];

    outb(0x3C2, r->misc);

    for (i = 0; i < 5; i++) {
        outb(0x3C4, (unsigned char)i);
        outb(0x3C5, r->seq[i]);
    }

    // Unlock CRTC registers 0-7 before writing them.
    crtc[0x03] |= 0x80;
    crtc[0x11] &= (unsigned char)~0x80;

    for (i = 0; i < 25; i++) {
        outb(0x3D4, (unsigned char)i);
        outb(0x3D5, crtc[i]);
    }

    for (i = 0; i < 9; i++) {
        outb(0x3CE, (unsigned char)i);
        outb(0x3CF, r->gc[i]);
    }

    for (i = 0; i < 21; i++) {
        (void)inb(0x3DA);                  // reset the attribute flip-flop
        outb(0x3C0, (unsigned char)i);
        outb(0x3C0, r->ac[i]);
    }

    (void)inb(0x3DA);
    outb(0x3C0, 0x20);                     // turn video output back on
}

// ======================================================================
//  Font (plane 2) access
// ======================================================================

// Map plane 2 at 0xA0000 as plain sequential memory.
static void font_access_begin(void) {
    outb(0x3C4, 0x02); outb(0x3C5, 0x04);  // write only plane 2
    outb(0x3C4, 0x04); outb(0x3C5, 0x06);  // sequential, no chain-4
    outb(0x3CE, 0x04); outb(0x3CF, 0x02);  // read plane 2
    outb(0x3CE, 0x05); outb(0x3CF, 0x00);  // no odd/even
    outb(0x3CE, 0x06); outb(0x3CF, 0x04);  // map 0xA0000, 64 KB
}

// Put the text-mode memory layout back.
static void font_access_end(void) {
    outb(0x3C4, 0x02); outb(0x3C5, 0x03);
    outb(0x3C4, 0x04); outb(0x3C5, 0x02);
    outb(0x3CE, 0x04); outb(0x3CF, 0x00);
    outb(0x3CE, 0x05); outb(0x3CF, 0x10);
    outb(0x3CE, 0x06); outb(0x3CF, 0x0E);
}

static void font_save(void) {
    volatile unsigned char *fb = GFX_FB;

    font_access_begin();
    for (int i = 0; i < 8192; i++) g_font[i] = fb[i];
    font_access_end();
    g_font_saved = 1;
}

static void font_restore(void) {
    volatile unsigned char *fb = GFX_FB;

    font_access_begin();
    for (int i = 0; i < 8192; i++) fb[i] = g_font[i];
    font_access_end();
}

// ======================================================================
//  Palette
// ======================================================================

// The 16 standard text colors as 6-bit r,g,b.
static const unsigned char ega16[16][3] = {
    {  0,  0,  0 }, {  0,  0, 42 }, {  0, 42,  0 }, {  0, 42, 42 },
    { 42,  0,  0 }, { 42,  0, 42 }, { 42, 21,  0 }, { 42, 42, 42 },
    { 21, 21, 21 }, { 21, 21, 63 }, { 21, 63, 21 }, { 21, 63, 63 },
    { 63, 21, 21 }, { 63, 21, 63 }, { 63, 63, 21 }, { 63, 63, 63 }
};

// Which DAC entries text mode actually uses for its 16 colors
// (matches the attribute controller table in mode_text).
static const unsigned char text_dac_index[16] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07,
    0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F
};

void gfx_set_color(unsigned char index, unsigned char r, unsigned char g, unsigned char b) {
    outb(0x3C8, index);
    outb(0x3C9, (unsigned char)(r & 63));
    outb(0x3C9, (unsigned char)(g & 63));
    outb(0x3C9, (unsigned char)(b & 63));
}

static void load_gfx_palette(void) {
    outb(0x3C6, 0xFF);                                      // use all 256 entries

    for (int i = 0; i < 16; i++)
        gfx_set_color((unsigned char)i, ega16[i][0], ega16[i][1], ega16[i][2]);

    for (int i = 0; i < 16; i++) {
        unsigned char v = (unsigned char)(i * 63 / 15);
        gfx_set_color((unsigned char)(16 + i), v, v, v);
    }

    for (int r = 0; r < 6; r++)
        for (int g = 0; g < 6; g++)
            for (int b = 0; b < 6; b++)
                gfx_set_color((unsigned char)(32 + 36 * r + 6 * g + b),
                              (unsigned char)(r * 63 / 5),
                              (unsigned char)(g * 63 / 5),
                              (unsigned char)(b * 63 / 5));

    for (int i = 248; i < 256; i++) gfx_set_color((unsigned char)i, 0, 0, 0);
}

// Give text mode back the colors it expects.
static void load_text_palette(void) {
    for (int i = 0; i < 16; i++)
        gfx_set_color(text_dac_index[i], ega16[i][0], ega16[i][1], ega16[i][2]);
}

// ======================================================================
//  Mode switching
// ======================================================================

int gfx_active(void) {
    return g_active;
}

int gfx_enter(void) {
    if (g_active) return 0;

    if (!g_font_saved) font_save();        // must happen while still in text mode

    write_regs(&mode_13h);
    load_gfx_palette();
    g_active = 1;
    gfx_clear(0);
    return 0;
}

void gfx_exit(void) {
    if (!g_active) return;

    write_regs(&mode_text);
    font_restore();
    load_text_palette();
    g_active = 0;
    clear_screen();
}

// ======================================================================
//  Drawing
// ======================================================================

void gfx_clear(unsigned char color) {
    volatile unsigned char *fb = GFX_FB;
    for (int i = 0; i < GFX_W * GFX_H; i++) fb[i] = color;
}

void gfx_putpixel(int x, int y, unsigned char color) {
    if (x < 0 || x >= GFX_W || y < 0 || y >= GFX_H) return;
    GFX_FB[y * GFX_W + x] = color;
}

unsigned char gfx_getpixel(int x, int y) {
    if (x < 0 || x >= GFX_W || y < 0 || y >= GFX_H) return 0;
    return GFX_FB[y * GFX_W + x];
}

void gfx_hline(int x1, int x2, int y, unsigned char color) {
    volatile unsigned char *fb = GFX_FB;

    if (y < 0 || y >= GFX_H) return;
    if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
    if (x2 < 0 || x1 >= GFX_W) return;
    if (x1 < 0) x1 = 0;
    if (x2 >= GFX_W) x2 = GFX_W - 1;

    for (int x = x1; x <= x2; x++) fb[y * GFX_W + x] = color;
}

void gfx_vline(int x, int y1, int y2, unsigned char color) {
    volatile unsigned char *fb = GFX_FB;

    if (x < 0 || x >= GFX_W) return;
    if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }
    if (y2 < 0 || y1 >= GFX_H) return;
    if (y1 < 0) y1 = 0;
    if (y2 >= GFX_H) y2 = GFX_H - 1;

    for (int y = y1; y <= y2; y++) fb[y * GFX_W + x] = color;
}

// Bresenham line.
void gfx_line(int x1, int y1, int x2, int y2, unsigned char color) {
    int dx = (x2 > x1) ? (x2 - x1) : (x1 - x2);
    int dy = (y2 > y1) ? (y2 - y1) : (y1 - y2);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    for (;;) {
        gfx_putpixel(x1, y1, color);
        if (x1 == x2 && y1 == y2) break;

        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 <  dx) { err += dx; y1 += sy; }
    }
}

void gfx_rect(int x, int y, int w, int h, unsigned char color) {
    if (w <= 0 || h <= 0) return;

    gfx_hline(x, x + w - 1, y, color);
    gfx_hline(x, x + w - 1, y + h - 1, color);
    gfx_vline(x, y, y + h - 1, color);
    gfx_vline(x + w - 1, y, y + h - 1, color);
}

void gfx_fill_rect(int x, int y, int w, int h, unsigned char color) {
    if (w <= 0 || h <= 0) return;

    int y_start = (y < 0) ? 0 : y;
    int y_end = (y + h > GFX_H) ? GFX_H : y + h;

    for (int j = y_start; j < y_end; j++) gfx_hline(x, x + w - 1, j, color);
}

// Midpoint circle (outline).
void gfx_circle(int cx, int cy, int r, unsigned char color) {
    int x = r, y = 0, err = 1 - r;

    if (r < 0) return;

    while (x >= y) {
        gfx_putpixel(cx + x, cy + y, color);
        gfx_putpixel(cx - x, cy + y, color);
        gfx_putpixel(cx + x, cy - y, color);
        gfx_putpixel(cx - x, cy - y, color);
        gfx_putpixel(cx + y, cy + x, color);
        gfx_putpixel(cx - y, cy + x, color);
        gfx_putpixel(cx + y, cy - x, color);
        gfx_putpixel(cx - y, cy - x, color);

        y++;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

void gfx_fill_circle(int cx, int cy, int r, unsigned char color) {
    int x = r;

    if (r < 0) return;

    for (int dy = 0; dy <= r; dy++) {
        while (x * x + dy * dy > r * r) x--;
        gfx_hline(cx - x, cx + x, cy + dy, color);
        if (dy != 0) gfx_hline(cx - x, cx + x, cy - dy, color);
    }
}

// ======================================================================
//  Text
// ======================================================================

void gfx_char(int x, int y, char ch, unsigned char fg, int bg) {
    if (!g_font_saved) return;

    const unsigned char *glyph = &g_font[(unsigned char)ch * 32];

    for (int row = 0; row < 16; row++) {
        unsigned char bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            if (bits & (0x80u >> col)) gfx_putpixel(x + col, y + row, fg);
            else if (bg >= 0)          gfx_putpixel(x + col, y + row, (unsigned char)bg);
        }
    }
}

void gfx_text(int x, int y, const char *s, unsigned char fg, int bg) {
    int cx = x;

    while (*s != '\0') {
        if (*s == '\n') {
            cx = x;
            y += 16;
        } else {
            gfx_char(cx, y, *s, fg, bg);
            cx += 8;
        }
        s++;
    }
}

// ======================================================================
//  Timing
// ======================================================================

void gfx_wait_vsync(void) {
    while (inb(0x3DA) & 0x08) { }          // wait for any current retrace to end
    while (!(inb(0x3DA) & 0x08)) { }       // then for the next one to start
}
