#include "shell.h"
#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include "../drivers/rtc.h"
#include "../drivers/ata.h"
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

// Sector buffers for the "ata test" command (256 words = 512 bytes each)
static unsigned short ata_out[256];
static unsigned short ata_in[256];

// ---------------------------------------------------------------------------
// Helpers for the "ata read" / "ata write" commands
// ---------------------------------------------------------------------------

// Sectors below this hold the boot sector and the kernel image, so "ata write"
// refuses them. Raise it if the kernel grows past 100 sectors (50 KB).
#define ATA_WRITE_MIN_LBA 100u
// os.img is padded to 1.44 MB = 2880 sectors, so the last valid LBA is 2879.
#define ATA_MAX_LBA       2879u

static void print_uint(unsigned int n) {
    char tmp[11];
    int len = 0;

    if (n == 0) {
        print_char('0', 0x0F);
        return;
    }
    while (n > 0) {
        tmp[len++] = '0' + (n % 10);
        n /= 10;
    }
    while (len > 0) {
        print_char(tmp[--len], 0x0F);
    }
}

static void print_hex_byte(unsigned char b) {
    const char *digits = "0123456789ABCDEF";
    print_char(digits[b >> 4], 0x0F);
    print_char(digits[b & 0x0F], 0x0F);
}

// Parses an unsigned decimal number at *s, skipping leading spaces.
// Returns 0 on success: the value goes in *out and *s moves past the digits.
// Returns -1 if there are no digits, the number overflows, or digits are
// followed by something other than a space or the end of the string.
static int parse_uint(const char **s, unsigned int *out) {
    const char *p = *s;
    unsigned int value = 0;
    int digits = 0;

    while (*p == ' ') p++;
    while (is_digit(*p)) {
        unsigned int d = (unsigned int)(*p - '0');
        if (value > (0xFFFFFFFFu - d) / 10u) return -1;   // would overflow
        value = value * 10u + d;
        digits++;
        p++;
    }
    if (digits == 0) return -1;
    if (*p != '\0' && *p != ' ') return -1;                // e.g. "12abc"

    *out = value;
    *s = p;
    return 0;
}

static void ata_usage(void) {
    print_string("Usage:\n"
                 "  ata write <lba> <text> - write text to a sector (LBA 100-2879)\n"
                 "  ata read <lba>         - show a sector as text and hex (LBA 0-2879)\n"
                 "  ata test               - automatic write/read-back check\n", 0x0F);
}

// ata write <lba> <text>
static void cmd_ata_write(const char *args) {
    unsigned int lba;
    unsigned int i;
    unsigned char *bytes = (unsigned char *)ata_out;

    if (parse_uint(&args, &lba) != 0) {
        ata_usage();
        return;
    }
    if (lba < ATA_WRITE_MIN_LBA) {
        print_string("Refused: sectors below ", 0x0F);
        print_uint(ATA_WRITE_MIN_LBA);
        print_string(" hold the bootloader and kernel.\n", 0x0F);
        return;
    }
    if (lba > ATA_MAX_LBA) {
        print_string("LBA out of range (max ", 0x0F);
        print_uint(ATA_MAX_LBA);
        print_string(").\n", 0x0F);
        return;
    }

    if (*args == ' ') args++;          // the text starts after one separating space

    // the rest of the sector is zero-filled, so "ata write <lba>" erases it
    for (i = 0; i < 512; i++) bytes[i] = 0;
    for (i = 0; args[i] != '\0' && i < 511; i++) bytes[i] = (unsigned char)args[i];

    if (ata_write_sector(lba, ata_out) == -1) {
        print_string("ATA write failed\n", 0x0F);
        return;
    }

    print_string("Wrote ", 0x0F);
    print_uint(i);
    print_string(" byte(s) to LBA ", 0x0F);
    print_uint(lba);
    print_char('\n', 0x0F);
}

// ata read <lba>
static void cmd_ata_read(const char *args) {
    unsigned int lba;
    unsigned int i;
    unsigned char *bytes = (unsigned char *)ata_in;

    if (parse_uint(&args, &lba) != 0) {
        ata_usage();
        return;
    }
    while (*args == ' ') args++;
    if (*args != '\0') {               // extra arguments after the LBA
        ata_usage();
        return;
    }
    if (lba > ATA_MAX_LBA) {
        print_string("LBA out of range (max ", 0x0F);
        print_uint(ATA_MAX_LBA);
        print_string(").\n", 0x0F);
        return;
    }

    for (i = 0; i < 512; i++) bytes[i] = 0;   // so a failed read can't show stale data
    if (ata_read_sector(lba, ata_in) == -1) {
        print_string("ATA read failed\n", 0x0F);
        return;
    }

    print_string("LBA ", 0x0F);
    print_uint(lba);
    print_string(":\n", 0x0F);

    // text view: printable characters up to the first zero byte (max 128)
    print_string("text: ", 0x0F);
    if (bytes[0] == 0) {
        print_string("(empty)", 0x0F);
    } else {
        for (i = 0; i < 512 && bytes[i] != 0 && i < 128; i++) {
            char c = (char)bytes[i];
            print_char((c >= 32 && c <= 126) ? c : '.', 0x0F);
        }
        if (i == 128 && bytes[128] != 0) print_string("...", 0x0F);
    }
    print_char('\n', 0x0F);

    // hex view: the first 32 bytes
    print_string("hex : ", 0x0F);
    for (i = 0; i < 32; i++) {
        print_hex_byte(bytes[i]);
        print_char(' ', 0x0F);
    }
    print_char('\n', 0x0F);
}

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

                // COMMAND PROCESSING
                // Basic command parsing using helpers
                if (buffer[0] == '\0') {
                    // empty line, do nothing
                } else if (str_eq(buffer, "clear")) {
                    clear_screen();
                } else if (str_eq(buffer, "help")) {
                    print_string("Commands:\n  clear - clear screen\n  help  - show help page\n  echo <msg> - print text\n  rtc - show local time and other things\n  sleep - wait for the number of seconds you specified\n  ata test - write and read back a disk sector\n  ata write <lba> <text> - write text to a sector\n  ata read <lba> - show a sector as text and hex\n", 0x0F);
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
                } else if (starts_with(buffer, "ata write ")) {
                    cmd_ata_write(buffer + 10);   // skip "ata write "
                } else if (starts_with(buffer, "ata read ")) {
                    cmd_ata_read(buffer + 9);     // skip "ata read "
                } else if (str_eq(buffer, "ata") || str_eq(buffer, "ata write") || str_eq(buffer, "ata read")) {
                    ata_usage();
                } else if (str_eq(buffer, "ata test")) {
                    unsigned int i;
                    int ok = 1;

                    for (i = 0; i < 256; i++) {
                        ata_out[i] = 0xA000 + i;   // every entry is different
                        ata_in[i] = 0;             // so a failed read can't match by luck
                    }

                    if (ata_write_sector(2000, ata_out) == -1) {
                        print_string("ATA write failed\n", 0x0F);
                    } else if (ata_read_sector(2000, ata_in) == -1) {
                        print_string("ATA read failed\n", 0x0F);
                    } else {
                        for (i = 0; i < 256; i++) {
                            if (ata_in[i] != ata_out[i]) {
                                ok = 0;
                                break;
                            }
                        }
                        if (ok) {
                            print_string("ATA test OK\n", 0x0F);
                        } else {
                            print_string("ATA test FAIL: data mismatch\n", 0x0F);
                        }
                    }
                } else {
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
