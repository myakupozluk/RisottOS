#include "timer.h"
#include "../arch/io.h"

// External tick counter updated by the timer interrupt handler
extern volatile unsigned int tick_count;

// Initialize the Programmable Interval Timer (PIT) at a specified frequency
void timer_init(unsigned int freq) {
    // PIT base frequency is 1.193180 MHz
    unsigned int divisor = 1193180 / freq;
    
    // Set command byte: channel 0, access mode lobyte/hibyte, mode 3 (square wave)
    outb(0x43, 0x36);
    
    // Send frequency divisor (low byte, then high byte)
    outb(0x40, (unsigned char)(divisor & 0xFF));
    outb(0x40, (unsigned char)((divisor >> 8) & 0xFF));
}

// Pause execution for a given number of timer ticks
void sleep_ticks(unsigned int ticks) {
    unsigned int start_tick = tick_count;
    // Halt CPU to save power until the target tick duration passes
    while (tick_count - start_tick < ticks) {
        halt_cpu(); 
    }
}