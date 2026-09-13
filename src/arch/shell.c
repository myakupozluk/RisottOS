#include "shell.h"
#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include "../drivers/rtc.h"
#include "io.h"
#include "../drivers/timer.h"

// Simple string equality (no libc)
static int str_eq(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

// Check if str starts with prefix
static int starts_with(const char *str, const char *prefix) {
    while (*prefix && *str == *prefix) {
        str++;
        prefix++;
    }
    return *prefix == '\0';
}

static int is_digit(char c) {
    return c >= '0' && c <= '9';
}

static int simple_atoi(const char *str) {
    int res = 0;
    while (*str) {
        if (!is_digit(*str)) return -1; // Wrong character
        res = res * 10 + (*str - '0');
        str++;
    }
    return res;
}

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

                // COMMAND PROCESSİNG
                // Basic command parsing using helpers
                if (buffer[0] == '\0') {
                    // empty line, do nothing
                } else if (str_eq(buffer, "clear")) {
                    clear_screen();
                } else if (str_eq(buffer, "help")) {
                    print_string("Commands:\n  clear - clear screen\n  help  - show help page\n  echo <msg> - print text\n  rtc - show local time and other things\n  sleep - wait for the number of seconds you specified\n", 0x0F);
                } else if (str_eq(buffer, "rtc") || str_eq(buffer, "rtc ")) {
                    print_string("Commands:\n  rtc get-time - show full time\n  rtc get-seconds/minutes/hours - show seconds/minutes/hours\n", 0x0F);
                } else if (starts_with(buffer, "rtc ")) {
                    char *msg = buffer + 4; // skip "rtc "
                    if (str_eq(msg, "help")) {
                        print_string("Commands:\n  rtc get-time - show full time\n  rtc get-seconds/minutes/hours - show seconds/minutes/hours\n", 0x0F);
                    } else if (str_eq(msg, "get-time")) {
                        print_string(rtc_get_time(), 0x0F);
                        print_char('\n', 0x0F);
                    } else if (str_eq(msg, "get-seconds")) {
                        print_string(rtc_get_seconds(), 0x0F);
                        print_char('\n', 0x0F);
                    } else if (str_eq(msg, "get-minutes")) {
                        print_string(rtc_get_minutes(), 0x0F);
                        print_char('\n', 0x0F);
                    } else if (str_eq(msg, "get-hours")) {
                        print_string(rtc_get_hours(), 0x0F);
                        print_char('\n', 0x0F);
                    } else {
                        print_string("rtc ", 0x0F);
                        print_string(msg, 0x0F);
                        print_string(": Command not found.\n", 0x0F);
                    }
                } else if (starts_with(buffer, "sleep ")) {
                    char *msg = buffer + 6; // skip "sleep "
                    if (str_eq(msg, "help")) {
                        print_string("Command:\n  sleep time(1,3,6...) - wait for the number of seconds you specified", 0x0F);
                    } else {
                        int seconds = simple_atoi(msg);
                        if (seconds < 0) {
                            print_string("Error: Invalid time format. Use numbers only.\n", 0x0F);
                        } else {
                            sleep_ticks(seconds * 100);
                            print_string("Done sleeping\n", 0x0F);
                        }
                    }
                } else if (starts_with(buffer, "echo ")) {
                    char *msg = buffer + 5; // skip "echo "
                    print_string(msg, 0x0F);
                    print_char('\n', 0x0F);
                }  else {
                    print_string("Command not found: ", 0x0F);
                    print_string(buffer, 0x0F);
                    print_char('\n', 0x0F);
                }
                
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