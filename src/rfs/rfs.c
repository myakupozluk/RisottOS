#include "rfs.h"
#include "../drivers/ata.h"
#include "../drivers/vga.h"

// Stops GCC from turning our byte loops into calls to memcpy/memset,
// which a freestanding kernel usually doesn't have.
#define RFS_NOLIB __attribute__((optimize("no-tree-loop-distribute-patterns")))

static unsigned char buffer[512] __attribute__((aligned(2)));  // used by rfs_format

// Scratch sectors. RFS is not re-entrant: call it from one place at a time.
static unsigned char g_sec[512] __attribute__((aligned(2)));   // file table sector
static unsigned char g_bm[512]  __attribute__((aligned(2)));   // bitmap sector
static unsigned char g_io[512]  __attribute__((aligned(2)));   // file data sector

// Cached superblock of the mounted filesystem.
static RfsSuper g_sb __attribute__((aligned(2)));
static int g_mounted = 0;

#define NEED_FS() do { if (!g_mounted) return RFS_E_NOFS; } while (0)

// ======================================================================
//  Small helpers (no libc)
// ======================================================================

RFS_NOLIB static void rfs_copy(void *dst, const void *src, unsigned int n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    unsigned int i = 0;
    while (i < n) { d[i] = s[i]; i++; }
}

RFS_NOLIB static void rfs_zero(void *dst, unsigned int n) {
    unsigned char *d = (unsigned char *)dst;
    unsigned int i = 0;
    while (i < n) { d[i] = 0; i++; }
}

static int rfs_streq(const char *a, const char *b) {
    unsigned int i = 0;
    while (i < RFS_NAME_MAX) {
        if (a[i] != b[i]) return 0;
        if (a[i] == 0) return 1;
        i++;
    }
    return 1;
}

static unsigned int sectors_for(unsigned int bytes) {
    return bytes / RFS_SECTOR_SIZE + ((bytes % RFS_SECTOR_SIZE) ? 1u : 0u);
}

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

// ======================================================================
//  Format / init / info
// ======================================================================

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

    // The fresh filesystem is now the mounted one.
    rfs_copy(&g_sb, &sb, sizeof(RfsSuper));
    g_mounted = 1;

    return 0;
}

// Load and validate an existing filesystem.
int rfs_init(void) {
    g_mounted = 0;

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

    // NOTE: free_sectors changes every time a file is written or deleted,
    // so it can only be checked against an upper bound here. (The old check
    // "free_sectors == total - data_start" would reject any FS that has files.)
    if (sb.bitmap_start != 1 ||
        sb.bitmap_sectors != expected_bitmap ||
        sb.table_start != sb.bitmap_start + sb.bitmap_sectors ||
        sb.table_sectors != RFS_TABLE_SECTORS ||
        sb.data_start != sb.table_start + sb.table_sectors ||
        sb.data_start >= sb.total_sectors ||
        sb.free_sectors > sb.total_sectors - sb.data_start ||
        sb.file_count > sb.table_sectors * 8u) {
        return -1;
    }

    rfs_copy(&g_sb, &sb, sizeof(RfsSuper));
    g_mounted = 1;

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

// ======================================================================
//  Superblock + file table access
// ======================================================================

static int sb_flush(void) {
    return (rfs_write_block(0, &g_sb) == 0) ? 0 : RFS_E_IO;
}

static int entry_read(unsigned int idx, RfsEntry *e) {
    if (idx >= g_sb.table_sectors * 8u) return RFS_E_INVAL;
    if (rfs_read_block(g_sb.table_start + idx / 8u, g_sec) != 0) return RFS_E_IO;
    rfs_copy(e, g_sec + (idx % 8u) * sizeof(RfsEntry), sizeof(RfsEntry));
    return 0;
}

static int entry_write(unsigned int idx, const RfsEntry *e) {
    if (idx >= g_sb.table_sectors * 8u) return RFS_E_INVAL;
    if (rfs_read_block(g_sb.table_start + idx / 8u, g_sec) != 0) return RFS_E_IO;
    rfs_copy(g_sec + (idx % 8u) * sizeof(RfsEntry), e, sizeof(RfsEntry));
    if (rfs_write_block(g_sb.table_start + idx / 8u, g_sec) != 0) return RFS_E_IO;
    return 0;
}

// Look for "name" inside directory "parent". Returns the entry index (>= 0),
// RFS_E_NOTFOUND, or RFS_E_IO. If out != 0 the entry is copied there.
static int rfs_find(const char *name, unsigned int parent, RfsEntry *out) {
    RfsEntry *t = (RfsEntry *)g_sec;
    for (unsigned int s = 0; s < g_sb.table_sectors; s++) {
        if (rfs_read_block(g_sb.table_start + s, g_sec) != 0) return RFS_E_IO;
        for (unsigned int i = 0; i < 8; i++) {
            if (t[i].flags != RFS_FLAG_FREE &&
                t[i].parent == parent &&
                rfs_streq(t[i].name, name)) {
                if (out) rfs_copy(out, &t[i], sizeof(RfsEntry));
                return (int)(s * 8u + i);
            }
        }
    }
    return RFS_E_NOTFOUND;
}

// ======================================================================
//  Bitmap: 1 bit per sector, bit set = used. Bit index == relative sector.
// ======================================================================

// Find n contiguous free sectors. On success *start = first sector.
static int bm_find(unsigned int n, unsigned int *start) {
    unsigned int run = 0, run_start = 0;

    if (n == 0 || n > g_sb.free_sectors) return RFS_E_NOSPACE;

    for (unsigned int b = 0; b < g_sb.bitmap_sectors; b++) {
        if (rfs_read_block(g_sb.bitmap_start + b, g_bm) != 0) return RFS_E_IO;

        for (unsigned int k = 0; k < 4096u; k++) {
            unsigned int idx = b * 4096u + k;
            if (idx >= g_sb.total_sectors) break;

            if (g_bm[k >> 3] & (1u << (k & 7u))) {
                run = 0;                       // used: restart the run
            } else {
                if (run == 0) run_start = idx;
                run++;
                if (run == n) { *start = run_start; return 0; }
            }
        }
    }
    return RFS_E_NOSPACE;
}

// Mark sectors [start, start+n) used (value = 1) or free (value = 0).
static int bm_set(unsigned int start, unsigned int n, int value) {
    unsigned int i = start, end = start + n;

    if (end > g_sb.total_sectors || start < g_sb.data_start) return RFS_E_INVAL;

    while (i < end) {
        unsigned int b = i / 4096u;
        unsigned int k = i % 4096u;

        if (rfs_read_block(g_sb.bitmap_start + b, g_bm) != 0) return RFS_E_IO;

        while (k < 4096u && i < end) {
            if (value) g_bm[k >> 3] |=  (unsigned char)(1u << (k & 7u));
            else       g_bm[k >> 3] &= (unsigned char)~(1u << (k & 7u));
            k++;
            i++;
        }

        if (rfs_write_block(g_sb.bitmap_start + b, g_bm) != 0) return RFS_E_IO;
    }
    return 0;
}

// ======================================================================
//  Paths: "/docs/sub/a.txt"  (no "." / ".." support)
// ======================================================================

// Pull the next component out of *p into name.
// Returns 1 = got one, 0 = end of path, <0 = error (name too long).
static int next_comp(const char **p, char *name) {
    const char *s = *p;
    unsigned int len = 0;

    while (*s == '/') s++;
    if (*s == 0) { *p = s; return 0; }

    while (*s && *s != '/') {
        if (len >= RFS_NAME_MAX - 1) return RFS_E_INVAL;
        name[len++] = *s++;
    }
    name[len] = 0;
    *p = s;
    return 1;
}

// Split a path into (parent directory, last component).
// Every directory on the way must exist. "/" alone has no leaf -> RFS_E_INVAL.
static int split_path(const char *path, unsigned int *parent, char *leaf) {
    unsigned int cur = RFS_ROOT;
    const char *p = path;
    char comp[RFS_NAME_MAX];
    char nxt[RFS_NAME_MAX];
    RfsEntry e;

    if (!path) return RFS_E_INVAL;

    int r = next_comp(&p, comp);
    if (r < 0) return r;
    if (r == 0) return RFS_E_INVAL;

    for (;;) {
        r = next_comp(&p, nxt);
        if (r < 0) return r;

        if (r == 0) {                       // comp is the last one
            rfs_copy(leaf, comp, RFS_NAME_MAX);
            *parent = cur;
            return 0;
        }

        int idx = rfs_find(comp, cur, &e);  // comp is a directory to enter
        if (idx < 0) return idx;
        if (e.flags != RFS_FLAG_DIR) return RFS_E_NOTDIR;

        cur = (unsigned int)idx;
        rfs_copy(comp, nxt, RFS_NAME_MAX);
    }
}

// Resolve a whole path. "/" gives *idx = RFS_ROOT.
// If e != 0 it receives the entry (not filled for the root).
static int resolve(const char *path, unsigned int *idx, RfsEntry *e) {
    unsigned int parent;
    char leaf[RFS_NAME_MAX];
    const char *p = path;
    char tmp[RFS_NAME_MAX];

    if (!path) return RFS_E_INVAL;

    int r = next_comp(&p, tmp);
    if (r < 0) return r;
    if (r == 0) { *idx = RFS_ROOT; return 0; }

    r = split_path(path, &parent, leaf);
    if (r < 0) return r;

    int found = rfs_find(leaf, parent, e);
    if (found < 0) return found;
    *idx = (unsigned int)found;
    return 0;
}

// ======================================================================
//  Creating things
// ======================================================================

// Add a new entry (file or dir) called leaf inside "parent".
static int rfs_make(const char *leaf, unsigned int parent,
                    unsigned int flags, unsigned int *out_idx) {
    RfsEntry *t = (RfsEntry *)g_sec;
    int free_idx = -1;
    unsigned int len = 0;

    // "." and ".." are reserved
    if (leaf[0] == '.' && (leaf[1] == 0 || (leaf[1] == '.' && leaf[2] == 0)))
        return RFS_E_INVAL;
    while (leaf[len] && len < RFS_NAME_MAX) len++;
    if (len == 0 || len >= RFS_NAME_MAX) return RFS_E_INVAL;

    // One pass: reject duplicates and remember the first free slot.
    for (unsigned int s = 0; s < g_sb.table_sectors; s++) {
        if (rfs_read_block(g_sb.table_start + s, g_sec) != 0) return RFS_E_IO;
        for (unsigned int i = 0; i < 8; i++) {
            if (t[i].flags == RFS_FLAG_FREE) {
                if (free_idx < 0) free_idx = (int)(s * 8u + i);
            } else if (t[i].parent == parent && rfs_streq(t[i].name, leaf)) {
                return RFS_E_EXISTS;
            }
        }
    }
    if (free_idx < 0) return RFS_E_FULL;

    RfsEntry e;
    rfs_zero(&e, sizeof(RfsEntry));
    rfs_copy(e.name, leaf, len);
    e.flags  = flags;
    e.parent = parent;

    // Entry first, superblock last.
    int r = entry_write((unsigned int)free_idx, &e);
    if (r < 0) return r;

    g_sb.file_count++;
    r = sb_flush();
    if (r < 0) return r;

    if (out_idx) *out_idx = (unsigned int)free_idx;
    return 0;
}

// Throw an entry away again (used to undo a failed rfs_write of a new file).
static void discard_entry(unsigned int idx) {
    RfsEntry e;
    rfs_zero(&e, sizeof(RfsEntry));
    entry_write(idx, &e);
    if (g_sb.file_count > 0) g_sb.file_count--;
    sb_flush();
}

int rfs_mkdir(const char *path) {
    unsigned int parent;
    char leaf[RFS_NAME_MAX];
    NEED_FS();

    int r = split_path(path, &parent, leaf);
    if (r < 0) return r;
    return rfs_make(leaf, parent, RFS_FLAG_DIR, 0);
}

int rfs_create(const char *path) {
    unsigned int parent;
    char leaf[RFS_NAME_MAX];
    NEED_FS();

    int r = split_path(path, &parent, leaf);
    if (r < 0) return r;
    return rfs_make(leaf, parent, RFS_FLAG_FILE, 0);
}

// ======================================================================
//  File contents
// ======================================================================

// Replace the contents of file entry idx with data[0..size).
static int set_contents(unsigned int idx, const void *data, unsigned int size) {
    RfsEntry e;
    int r = entry_read(idx, &e);
    if (r < 0) return r;

    unsigned int old_n = sectors_for(e.size);
    unsigned int need  = sectors_for(size);
    unsigned int new_start = 0;
    int fresh = 0;                       // 1 = we allocated a new extent

    if (need == 0) {
        new_start = 0;
    } else if (old_n > 0 && need <= old_n) {
        new_start = e.start_sector;      // fits where it is: overwrite in place
    } else {
        r = bm_find(need, &new_start);   // grow: need a new contiguous extent
        if (r < 0) return r;
        r = bm_set(new_start, need, 1);
        if (r < 0) return r;
        fresh = 1;
    }

    // Data sectors
    for (unsigned int i = 0; i < need; i++) {
        unsigned int off = i * RFS_SECTOR_SIZE;
        unsigned int chunk = size - off;
        if (chunk > RFS_SECTOR_SIZE) chunk = RFS_SECTOR_SIZE;

        rfs_zero(g_io, RFS_SECTOR_SIZE);
        rfs_copy(g_io, (const unsigned char *)data + off, chunk);

        if (rfs_write_block(new_start + i, g_io) != 0) {
            if (fresh) bm_set(new_start, need, 0);
            return RFS_E_IO;
        }
    }

    // Which sectors does this file give back?
    unsigned int free_start = 0, free_n = 0;
    if (need == 0) {
        free_start = e.start_sector; free_n = old_n;
    } else if (fresh) {
        free_start = e.start_sector; free_n = old_n;          // the old extent
    } else if (old_n > need) {
        free_start = e.start_sector + need; free_n = old_n - need;   // shrunk tail
    }

    // Directory entry next, then release old sectors, superblock last.
    e.size = size;
    e.start_sector = new_start;
    r = entry_write(idx, &e);
    if (r < 0) {
        if (fresh) bm_set(new_start, need, 0);
        return r;
    }

    if (free_n > 0) {
        r = bm_set(free_start, free_n, 0);
        if (r < 0) return r;
    }

    g_sb.free_sectors = g_sb.free_sectors + free_n - (fresh ? need : 0);
    return sb_flush();
}

// Create the file if needed, then replace its contents.
int rfs_write(const char *path, const void *data, unsigned int size) {
    unsigned int parent, idx;
    char leaf[RFS_NAME_MAX];
    RfsEntry e;
    int created = 0;
    NEED_FS();

    if (!data && size > 0) return RFS_E_INVAL;

    int r = split_path(path, &parent, leaf);
    if (r < 0) return r;

    int found = rfs_find(leaf, parent, &e);
    if (found >= 0) {
        if (e.flags != RFS_FLAG_FILE) return RFS_E_ISDIR;
        idx = (unsigned int)found;
    } else if (found == RFS_E_NOTFOUND) {
        r = rfs_make(leaf, parent, RFS_FLAG_FILE, &idx);
        if (r < 0) return r;
        created = 1;
    } else {
        return found;
    }

    r = set_contents(idx, data, size);
    if (r < 0 && created) discard_entry(idx);
    return r;
}

// Returns the number of bytes copied into buf (at most max).
int rfs_read(const char *path, void *buf, unsigned int max) {
    unsigned int idx;
    RfsEntry e;
    NEED_FS();

    if (!buf && max > 0) return RFS_E_INVAL;

    int r = resolve(path, &idx, &e);
    if (r < 0) return r;
    if (idx == RFS_ROOT || e.flags != RFS_FLAG_FILE) return RFS_E_ISDIR;

    unsigned int n = (e.size < max) ? e.size : max;
    unsigned int sectors = sectors_for(n);

    for (unsigned int i = 0; i < sectors; i++) {
        unsigned int off = i * RFS_SECTOR_SIZE;
        unsigned int chunk = n - off;
        if (chunk > RFS_SECTOR_SIZE) chunk = RFS_SECTOR_SIZE;

        if (rfs_read_block(e.start_sector + i, g_io) != 0) return RFS_E_IO;
        rfs_copy((unsigned char *)buf + off, g_io, chunk);
    }
    return (int)n;
}

int rfs_size(const char *path) {
    unsigned int idx;
    RfsEntry e;
    NEED_FS();

    int r = resolve(path, &idx, &e);
    if (r < 0) return r;
    if (idx == RFS_ROOT || e.flags != RFS_FLAG_FILE) return RFS_E_ISDIR;
    return (int)e.size;
}

// 1 = directory (the root counts), 0 = file, negative = error (RFS_E_NOTFOUND ...).
int rfs_isdir(const char *path) {
    unsigned int idx;
    RfsEntry e;
    NEED_FS();

    int r = resolve(path, &idx, &e);
    if (r < 0) return r;
    if (idx == RFS_ROOT) return 1;
    return (e.flags == RFS_FLAG_DIR) ? 1 : 0;
}

// ======================================================================
//  Deleting
// ======================================================================

int rfs_delete(const char *path) {
    unsigned int idx;
    RfsEntry e;
    NEED_FS();

    int r = resolve(path, &idx, &e);
    if (r < 0) return r;
    if (idx == RFS_ROOT || e.flags != RFS_FLAG_FILE) return RFS_E_ISDIR;

    unsigned int n = sectors_for(e.size);
    unsigned int start = e.start_sector;

    // Entry first (so nothing points at freed sectors), then bitmap, then superblock.
    rfs_zero(&e, sizeof(RfsEntry));
    r = entry_write(idx, &e);
    if (r < 0) return r;

    if (n > 0) {
        r = bm_set(start, n, 0);
        if (r < 0) return r;
    }

    g_sb.free_sectors += n;
    if (g_sb.file_count > 0) g_sb.file_count--;
    return sb_flush();
}

int rfs_deldir(const char *path) {
    unsigned int idx;
    RfsEntry e;
    RfsEntry *t = (RfsEntry *)g_sec;
    NEED_FS();

    int r = resolve(path, &idx, &e);
    if (r < 0) return r;
    if (idx == RFS_ROOT) return RFS_E_INVAL;          // can't delete "/"
    if (e.flags != RFS_FLAG_DIR) return RFS_E_NOTDIR;

    // Refuse if anything still lives inside it.
    for (unsigned int s = 0; s < g_sb.table_sectors; s++) {
        if (rfs_read_block(g_sb.table_start + s, g_sec) != 0) return RFS_E_IO;
        for (unsigned int i = 0; i < 8; i++) {
            if (t[i].flags != RFS_FLAG_FREE && t[i].parent == idx)
                return RFS_E_NOTEMPTY;
        }
    }

    rfs_zero(&e, sizeof(RfsEntry));
    r = entry_write(idx, &e);
    if (r < 0) return r;

    if (g_sb.file_count > 0) g_sb.file_count--;
    return sb_flush();
}

// ======================================================================
//  Listing + disk space
// ======================================================================

// Print the contents of a directory ("/" for the root). Returns entry count.
int rfs_list(const char *path) {
    unsigned int dir;
    RfsEntry e;
    RfsEntry *t = (RfsEntry *)g_sec;
    unsigned char color = 0x0F;
    int count = 0;
    NEED_FS();

    int r = resolve(path, &dir, &e);
    if (r < 0) return r;
    if (dir != RFS_ROOT && e.flags != RFS_FLAG_DIR) return RFS_E_NOTDIR;

    for (unsigned int s = 0; s < g_sb.table_sectors; s++) {
        if (rfs_read_block(g_sb.table_start + s, g_sec) != 0) return RFS_E_IO;
        for (unsigned int i = 0; i < 8; i++) {
            if (t[i].flags == RFS_FLAG_FREE || t[i].parent != dir) continue;

            if (t[i].flags == RFS_FLAG_DIR) {
                print_string("[DIR] ", color);
                print_string(t[i].name, color);
            } else {
                print_string("      ", color);
                print_string(t[i].name, color);
                print_string("  ", color);
                rfs_print_uint(t[i].size, color);
                print_string(" bytes", color);
            }
            print_char('\n', color);
            count++;
        }
    }
    return count;
}

unsigned int rfs_free_sectors(void) {
    return g_mounted ? g_sb.free_sectors : 0;
}