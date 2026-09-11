#include "keyboard.h"
#include "../arch/io.h"
#include "../arch/pic.h"

#define KEYBOARD_BUFFER_SIZE 128

// Store characters received from keyboard IRQs using a circular buffer.
static volatile char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static volatile unsigned char buffer_head = 0;
static volatile unsigned char buffer_tail = 0;

// Set 1 keyboard scancodes mapped to ASCII characters.
static const char scancode_to_ascii[] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '
};

// Poll keyboard directly for a single key press.
char get_key(void) {
    while (1) {
        // Wait until keyboard buffer has data available on port 0x64.
        while (!(inb(0x64) & 1)) {
        }

        unsigned char scancode = inb(0x60);

        // Ignore key-release scancodes (bit 7 set).
        if (scancode & 0x80) {
            continue;
        }

        // Ignore scancodes exceeding the ASCII map size.
        if (scancode >= sizeof(scancode_to_ascii)) {
            continue;
        }

        // Return the mapped character for a valid key press.
        return scancode_to_ascii[scancode];
    }
}

// Interrupt handler for IRQ1 (keyboard input).
void keyboard_irq_handler(void) {
    // Read the scancode from the data port before acknowledging.
    unsigned char scancode = inb(0x60);

    // Process only key press scancodes within valid range.
    if (!(scancode & 0x80) && scancode < sizeof(scancode_to_ascii)) {
        char character = scancode_to_ascii[scancode];
        unsigned char next_head = (buffer_head + 1) % KEYBOARD_BUFFER_SIZE;

        // Store character in circular buffer if space permits.
        if (character && next_head != buffer_tail) {
            keyboard_buffer[buffer_head] = character;
            buffer_head = next_head;
        }
    }

    // Send End-of-Interrupt signal to PIC for IRQ1.
    pic_send_eoi(1);
}

// Retrieve the next character from the keyboard input buffer.
char keyboard_read(void) {
    char character;

    // Return 0 if the buffer is empty.
    if (buffer_head == buffer_tail) {
        return 0;
    }

    // Fetch character from tail and advance pointer.
    character = keyboard_buffer[buffer_tail];
    buffer_tail = (buffer_tail + 1) % KEYBOARD_BUFFER_SIZE;
    return character;
}