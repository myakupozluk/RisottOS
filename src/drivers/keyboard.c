#include "keyboard.h"
#include "../arch/io.h"

// Set 1 keyboard scancodes mapped to ASCII characters.
static const char scancode_to_ascii[] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '
};

char get_key(void) {
    while (1) {
        // Port 0x64 reports whether keyboard data is available.
        while (!(inb(0x64) & 1)) {
        }

        unsigned char scancode = inb(0x60);

        // Ignore key-release scancodes.
        if (scancode & 0x80) {
            continue;
        }

        if (scancode >= sizeof(scancode_to_ascii)) {
            continue;
        }

        // Return the mapped character for a key press.
        return scancode_to_ascii[scancode];
    }
}
