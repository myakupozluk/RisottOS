#include "shell.h"
#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include "io.h"

// Command input buffer and tracking index
static char buffer[256];
static int index = 0;

void start_shell() {
    print_string("\nRisottOS> ", 0x0F);

    while (1) {
        char c = keyboard_read();
        
        if (c != 0) {
            // Handle backspace key
            if (c == '\b') {
                if (index > 0) {
                    index--;
                    buffer[index] = '\0';
                    print_char(c, 0x0F);
                }
            }
            // Handle enter key for command execution
            else if (c == '\n' || c == '\r') {
                buffer[index] = '\0';
                print_char('\n', 0x0F);
                
                // TODO: Command parsing logic will go here
                
                index = 0;
                print_string("RisottOS> ", 0x0F);
            }
            // Handle printable characters with buffer overflow protection
            else {
                if (index < 255) {
                    buffer[index++] = c;
                    print_char(c, 0x0F);
                }
            }
        } else {
            // Halt CPU to save power when no input is ready
            halt_cpu();
        }
    }
}