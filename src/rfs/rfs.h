#ifndef R_RFS_H
#define R_RFS_H

#define RFS_START_LBA   100        // where the filesystem begins on the disk
#define RFS_MAGIC       "RFS1"
#define RFS_NAME_MAX    32
#define RFS_TABLE_SECTORS 16       // 16 * 8 = 128 files

typedef struct {
    char magic[4];                 // "RFS1", no terminator stored
    unsigned int version;
    unsigned int total_sectors;
    unsigned int bitmap_start;
    unsigned int bitmap_sectors;
    unsigned int table_start;
    unsigned int table_sectors;
    unsigned int data_start;
    unsigned int free_sectors;
    unsigned int file_count;
    unsigned char padding[512 - 4 - 9 * 4];
} __attribute__((packed)) RfsSuper;

typedef struct {
    char name[RFS_NAME_MAX];
    unsigned int size;
    unsigned int start_sector;
    unsigned int flags;            // 0 = unused, 1 = file
    unsigned int timestamp;
    unsigned char padding[16];
} __attribute__((packed)) RfsEntry;

// If a struct is the wrong size these arrays get size -1, and the build fails.
typedef char rfs_super_size_ok[(sizeof(RfsSuper) == 512) ? 1 : -1];
typedef char rfs_entry_size_ok[(sizeof(RfsEntry) == 64) ? 1 : -1];

int  rfs_read_block(unsigned int rel, void *buf);
int  rfs_write_block(unsigned int rel, const void *buf);
int  rfs_format(void);
int  rfs_info(void);
int rfs_init(void);
#endif