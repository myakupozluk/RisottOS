
#include "rfs.h"
#include "../drivers/ata.h"
#include "../drivers/vga.h"
static unsigned char buffer[512] __attribute__((aligned(2)));

// Everything in RFS goes through these two functions.
int rfs_read_block(unsigned int rel, void *buf) {
    return ata_read_sector(
        RFS_START_LBA + rel,
        (unsigned short *)buf
    );
}

int rfs_write_block(unsigned int rel, const void *buf) {
    return ata_write_sector(
        RFS_START_LBA + rel,
        (const unsigned short *)buf
    );
}

// Clear the reusable 512-byte buffer.
static void rfs_clear_buffer(void) {
    unsigned int i = 0;

    while (i < 512) {
        buffer[i] = 0;
        i++;
    }
}

// Format the disk area reserved for RFS.
// WARNING: This destroys existing RFS metadata and file entries.
int rfs_format(void) {
    unsigned int disc_size = ata_disc_space();

    if (disc_size <= RFS_START_LBA) {
        return -1;
    }

    unsigned int total = disc_size - RFS_START_LBA;

    unsigned int bitmap_start = 1;
    unsigned int bitmap_sectors = (total + 4095u) / 4096u;

    unsigned int table_start = bitmap_start + bitmap_sectors;
    unsigned int table_sectors = RFS_TABLE_SECTORS;

    unsigned int data_start = table_start + table_sectors;

    if (total <= data_start) {
        return -1;
    }

    unsigned int free_sectors = total - data_start;

    // 1. Build the bitmap, one 512-byte sector at a time.
    unsigned int block = 0;

    while (block < bitmap_sectors) {
        rfs_clear_buffer();

        unsigned int sector = 0;

        while (sector < 4096u) {
            // Position of this bit across the entire bitmap.
            unsigned int bitmap_index = block * 4096u + sector;

            // Reserve metadata sectors and nonexistent sectors.
            if (bitmap_index < data_start ||
                bitmap_index >= total) {

                unsigned int byte_index = sector / 8u;
                unsigned int bit_index = sector % 8u;

                buffer[byte_index] |=
                    (unsigned char)(1u << bit_index);
            }

            sector++;
        }

        if (rfs_write_block(bitmap_start + block, buffer) != 0) {
            return -1;
        }

        block++;
    }

    // 2. Clear the file table.
    unsigned int table_block = 0;

    rfs_clear_buffer();

    while (table_block < table_sectors) {
        if (rfs_write_block(table_start + table_block, buffer) != 0) {
            return -1;
        }

        table_block++;
    }

    // 3. Prepare the superblock.
    RfsSuper sb;

    // Clear the entire structure, including padding.
    unsigned char *sb_bytes = (unsigned char *)&sb;
    unsigned int i = 0;

    while (i < sizeof(RfsSuper)) {
        sb_bytes[i] = 0;
        i++;
    }

    sb.magic[0] = 'R';
    sb.magic[1] = 'F';
    sb.magic[2] = 'S';
    sb.magic[3] = '1';

    sb.version = 1;
    sb.total_sectors = total;
    sb.bitmap_start = bitmap_start;
    sb.bitmap_sectors = bitmap_sectors;
    sb.table_start = table_start;
    sb.table_sectors = table_sectors;
    sb.data_start = data_start;
    sb.free_sectors = free_sectors;
    sb.file_count = 0;

    // Write the superblock LAST, so an interrupted format
    // doesn't leave a valid-looking superblock prematurely.
    if (rfs_write_block(0, &sb) != 0) {
        return -1;
    }

    return 0;
}

// Load and validate an existing filesystem.
int rfs_init(void) {
    unsigned int disc_size = ata_disc_space();

    if (disc_size <= RFS_START_LBA) {
        return -1;
    }

    RfsSuper sb;

    if (rfs_read_block(0, &sb) != 0) {
        return -1;
    }

    if (sb.magic[0] != 'R' ||
        sb.magic[1] != 'F' ||
        sb.magic[2] != 'S' ||
        sb.magic[3] != '1') {
        return -1;
    }

    if (sb.version != 1) {
        return -1;
    }

    unsigned int available = disc_size - RFS_START_LBA;

    if (sb.total_sectors > available ||
        sb.total_sectors == 0) {
        return -1;
    }

    // Check that the stored layout is internally consistent.
    unsigned int expected_bitmap =
        (sb.total_sectors + 4095u) / 4096u;

    if (sb.bitmap_start != 1 ||
        sb.bitmap_sectors != expected_bitmap ||
        sb.table_start != sb.bitmap_start + sb.bitmap_sectors ||
        sb.table_sectors != RFS_TABLE_SECTORS ||
        sb.data_start != sb.table_start + sb.table_sectors ||
        sb.data_start >= sb.total_sectors ||
        sb.free_sectors != sb.total_sectors - sb.data_start) {
        return -1;
    }

    return 0;
}

/* Print an unsigned integer using the VGA library. */
static void rfs_print_uint(unsigned int value, unsigned char color)
{
    char text[11];
    int i = 0;

    if (value == 0) {
        print_string("0", color);
        return;
    }

    while (value > 0) {
        text[i++] = '0' + (value % 10);
        value /= 10;
    }

    /* Reverse the digits into a printable string. */
    while (i > 0) {
        i--;
        print_char(text[i], color);
    }
}

static void rfs_print_line(const char *label,
                           unsigned int value,
                           unsigned char color)
{
    print_string(label, color);
    rfs_print_uint(value, color);
    print_char('\n', color);
}

int rfs_info(void)
{
    RfsSuper sb;
    unsigned char color = 0x0F; /* White text on black. */

    if (rfs_read_block(0, &sb) != 0) {
        print_string("RFS: Failed to read superblock\n", color);
        return -1;
    }

    if (sb.magic[0] != 'R' ||
        sb.magic[1] != 'F' ||
        sb.magic[2] != 'S' ||
        sb.magic[3] != '1') {
        print_string("RFS: Not formatted\n", color);
        return -1;
    }

    print_string("=== RFS INFO ===\n", color);
    rfs_print_line("Version: ", sb.version, color);
    rfs_print_line("Total sectors: ", sb.total_sectors, color);
    rfs_print_line("Bitmap start: ", sb.bitmap_start, color);
    rfs_print_line("Bitmap sectors: ", sb.bitmap_sectors, color);
    rfs_print_line("Table start: ", sb.table_start, color);
    rfs_print_line("Table sectors: ", sb.table_sectors, color);
    rfs_print_line("Data start: ", sb.data_start, color);
    rfs_print_line("Free sectors: ", sb.free_sectors, color);
    rfs_print_line("File count: ", sb.file_count, color);

    return 0;
}
