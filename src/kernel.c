#include "drivers/keyboard.h"
#include "drivers/vga.h"

// Kernel entry point
void kmain() {
    // Initialize the display before accepting input.
    clear_screen();
    print_string("RisottOS booted succesfully!\n", 0x0F);

    while (1) {
        // Read and display one keyboard character at a time.
        char c = get_key();
        if (c) {
            print_char(c, 0x0F);
        }
    }
}