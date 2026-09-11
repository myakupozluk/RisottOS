#include "drivers/keyboard.h"
#include "drivers/vga.h"
#include "arch/idt.h"
#include "arch/pic.h"
#include "arch/io.h"
#include "arch/shell.h"
#include "drivers/timer.h"

// Kernel entry point
void kmain() {
    // Initialize core hardware components
    clear_screen();
    idt_init();
    pic_remap();
    __asm__ volatile ("sti");
    timer_init(100);

    // Boot message and initial prompt
    print_string("RisottOS booted succesfully!\n", 0x0F);
    sleep_ticks(100);
    print_string("\nRisottOS> ", 0x0F);

    // The shell owns the input loop.
    start_shell();
}
