#ifndef R_RFS_H
#define R_RFS_H

#define RFS_START_LBA   100        // where the filesystem begins on the disk
#define RFS_MAGIC       "RFS1"
#define RFS_NAME_MAX    32         // 31 characters + terminator
#define RFS_TABLE_SECTORS 16       // 16 * 8 = 128 entries (files + directories)
#define RFS_SECTOR_SIZE 512

// Entry flags
#define RFS_FLAG_FREE   0
#define RFS_FLAG_FILE   1
#define RFS_FLAG_DIR    2

// "parent" value for entries that live in the root directory ("/")
#define RFS_ROOT        0xFFFFFFFFu

// Return codes (0 or a positive value = success)
#define RFS_E_IO        (-1)       // disk read/write failed
#define RFS_E_NOTFOUND  (-2)       // path does not exist
#define RFS_E_EXISTS    (-3)       // name already used in that directory
#define RFS_E_FULL      (-4)       // file table is full (128 entries)
#define RFS_E_NOSPACE   (-5)       // no contiguous free sectors big enough
#define RFS_E_NOTEMPTY  (-6)       // directory still has entries inside
#define RFS_E_NOTDIR    (-7)       // a path component is not a directory
#define RFS_E_ISDIR     (-8)       // expected a file, found a directory
#define RFS_E_INVAL     (-9)       // bad path / bad name / bad argument
#define RFS_E_NOFS      (-10)      // rfs_init()/rfs_format() not done yet

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
    unsigned int file_count;       // files + directories
    unsigned char padding[512 - 4 - 9 * 4];
} __attribute__((packed)) RfsSuper;

typedef struct {
    char name[RFS_NAME_MAX];
    unsigned int size;             // bytes (0 for directories)
    unsigned int start_sector;     // first data sector, relative to the FS start
    unsigned int flags;            // RFS_FLAG_FREE / FILE / DIR
    unsigned int timestamp;
    unsigned int parent;           // table index of parent dir, or RFS_ROOT
    unsigned char padding[12];
} __attribute__((packed)) RfsEntry;

// If a struct is the wrong size these arrays get size -1, and the build fails.
typedef char rfs_super_size_ok[(sizeof(RfsSuper) == 512) ? 1 : -1];
typedef char rfs_entry_size_ok[(sizeof(RfsEntry) == 64) ? 1 : -1];

// ---- low level / setup -------------------------------------------------
int  rfs_read_block(unsigned int rel, void *buf);
int  rfs_write_block(unsigned int rel, const void *buf);
int  rfs_format(void);             // wipes the FS area, leaves it mounted
int  rfs_init(void);               // validate + mount an existing FS
int  rfs_info(void);               // print superblock info

// ---- directories -------------------------------------------------------
int  rfs_mkdir(const char *path);  // "/docs", "/docs/sub"
int  rfs_deldir(const char *path); // only works on an empty directory
int  rfs_list(const char *path);   // prints entries, returns their count

// ---- files -------------------------------------------------------------
int  rfs_create(const char *path);                              // empty file
int  rfs_write(const char *path, const void *data, unsigned int size); // create or overwrite
int  rfs_read(const char *path, void *buf, unsigned int max);   // returns bytes read
int  rfs_size(const char *path);                                // file size in bytes
int  rfs_isdir(const char *path);                               // 1 = directory ("/" counts), 0 = file, <0 = error
int  rfs_delete(const char *path);                              // delete a file

// ---- disk space --------------------------------------------------------
unsigned int rfs_free_sectors(void);   // free 512-byte sectors

#endif