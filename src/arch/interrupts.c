#include "../drivers/keyboard.h"
#include "pic.h"

unsigned int tick_count = 0;

void keyboard_handler(void) {
	// Forward the IRQ to the keyboard driver.
	keyboard_irq_handler();
}

void timer_handler(void) {
    tick_count++;

    pic_send_eoi(0);
}