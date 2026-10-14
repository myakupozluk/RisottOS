#include "fs.h"
#include "../drivers/ata.h"
#include "../mem/mem.h"
#include "../drivers/vga.h"

// Helper func to convert TAR's ASCII Octal size string to a real int
static unsigned int tar_octal_to_int(const char *str, int size) {
    unsigned int result = 0;
    int i;

    for (i = 0; i < size; i++) {

        // TAR fields are space or null padded at the end, so stop if we hit them
        if (str[i] == '\0' || str[i] == ' ') {
            break;
        }

        // Standard atoi shift math, but multiplied by 8 because it is octal (base 8)
        result = (result * 8) + (str[i] - '0');
    }

    return result;
}

// Helper function to print an unsigned integer directly to the VGA screen
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

void fs_list_files(void) {
    unsigned int current_lba = 100;
    unsigned short sector_buffer[256];

    print_string("Listing files on RFS\n", 0x07);

    while (1) {
        if (ata_read_sector(current_lba, sector_buffer) == -1) {
            print_string("RFS Error: Disk read failed during list operation.\n", 0x07);
            return;
        }
        TarHeader *header = (TarHeader *)sector_buffer;

        if (header->name[0] == '\0') {
            break;
        }

        unsigned int file_size = tar_octal_to_int(header->size, 12);

        print_string("  - ", 0x0F);
        print_string(header->name, 0x0A);
        print_string("  (", 0x0F);
        print_uint(file_size);
        print_string(" bytes)\n", 0x0F);

        unsigned int sectors_occupied = (file_size + 511) / 512;

        current_lba += 1 + sectors_occupied;
    }
}