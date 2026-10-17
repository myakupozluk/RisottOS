#include "shell.h"
#include "../drivers/vga.h"
#include "../drivers/gfx/gfx.h"
#include "../drivers/keyboard.h"
#include "../drivers/rtc.h"
#include "../drivers/ata.h"
#include "io.h"
#include "../drivers/timer.h"
#include "../mem/mem.h"
#include "../rfs/rfs.h"

// Simple string equality (no libc)
static int str_eq(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

typedef struct {
    const char *name;
    void (*handler)(const char *args);
} ShellCommand;

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
// refuses them. Raise it if the kernel grows past 101 sectors (50.5 KB).
#define ATA_WRITE_MIN_LBA 150u
// os.img is padded to 64 MB = 131072 sectors, so the last valid LBA is 131071.
#define ATA_MAX_LBA       131071u

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
                 "  ata write <lba> <text> - write text to a sector (LBA 150-131071)\n"
                 "  ata read <lba>         - show a sector as text and hex (LBA 0-131071)\n"
                 "  ata test               - automatic write/read-back check\n", 0x0F);
}
static void cmd_ata_disc_space(void){
    float disc_space = ata_disc_space();
    print_string("Disc Space:", 0x0F);
    print_uint(disc_space/2048);
    print_string("MBs\n", 0x0F);
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

    unsigned short *my_buffer = (unsigned short *)r_malloc(512);
    if (my_buffer == 0) {
        print_string("Out of memory!\n", 0x0F);
        return;
    }
    unsigned char *bytes = (unsigned char *)my_buffer;


    for (i = 0; i < 512; i++) bytes[i] = 0;   // so a failed read can't show stale data
    if (ata_read_sector(lba, my_buffer) == -1) {
        print_string("ATA read failed\n", 0x0F);
        r_free(my_buffer);
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
    r_free(my_buffer);
}

// ---------------------------------------------------------------------------
// Modular Shell Command Implementation Functions
// ---------------------------------------------------------------------------

// Handles the 'clear' command
static void sh_clear(const char *args) {
    (void)args;
    clear_screen();
}

// Handles the 'help' command
static void sh_help(const char *args) {
    (void)args;
    print_string("Commands:\n  clear - clear screen\n  help  - show help page\n  echo <msg> - print text\n  rtc - show local time and other things\n  sleep - wait for the number of seconds you specified\n  ata test - write and read back a disk sector\n  ata write <lba> <text> - write text to a sector\n  ata read <lba> - show a sector as text and hex\n  rfs info - show filesystem info\n  rfs init - validate filesystem\n  rfs format - format filesystem (destructive)\n  cd [path] - change directory (cd alone goes to /)\n  pwd - show the current directory\n  ls [path] - list a directory (default: current)\n  mkdir <path> - create a directory\n  rmdir <path> - remove an empty directory\n  touch <path> - create an empty file\n  write <path> <text> - write text to a file\n  cat <path> - show a file\n  rm <path> - delete a file\n  df - show free disk space\n  ui demo|colors - text-mode graphics demos\n  gfx demo|ball - pixel graphics demos\n", 0x0F);
}

// Handles the 'echo' command
static void sh_echo(const char *args) {
    print_string(args, 0x0F);
    print_char('\n', 0x0F);
}

// Handles the 'rtc' subcommands
static void sh_rtc(const char *args) {
    if (args[0] == '\0' || (args[0] == ' ' && args[1] == '\0')) {
        print_string("Commands:\n  rtc get-time - show full time\n  rtc get-seconds/minutes/hours - show seconds/minutes/hours\n", 0x0F);
    } else {
        if (str_eq(args, "help")) {
            print_string("Commands:\n  rtc get-time - show full time\n  rtc get-seconds/minutes/hours - show seconds/minutes/hours\n", 0x0F);
        } else if (str_eq(args, "get-time")) {
            print_string(rtc_get_time(), 0x0F);
            print_char('\n', 0x0F);
        } else if (str_eq(args, "get-seconds")) {
            print_string(rtc_get_seconds(), 0x0F);
            print_char('\n', 0x0F);
        } else if (str_eq(args, "get-minutes")) {
            print_string(rtc_get_minutes(), 0x0F);
            print_char('\n', 0x0F);
        } else if (str_eq(args, "get-hours")) {
            print_string(rtc_get_hours(), 0x0F);
            print_char('\n', 0x0F);
        } else {
            print_string("rtc ", 0x0F);
            print_string(args, 0x0F);
            print_string(": Command not found.\n", 0x0F);
        }
    }
}

// Handles the 'mem test' command
static void sh_mem(const char *args) {
    if (str_eq(args, "test")) {
        print_string("Allocating 100 bytes...\n", 0x0F);
        char *ptr = (char *)r_malloc(100);

        if (ptr != 0) {
            print_string("Malloc OK! Writing data into memory...\n", 0x0F);
            ptr[0] = 'R';
            ptr[1] = 'i';
            ptr[2] = 's';
            ptr[3] = 'o';
            ptr[4] = 't';
            ptr[5] = 't';
            ptr[6] = 'o';
            ptr[7] = '\0';

            print_string("Data read from allocated memory: ", 0x0F);
            print_string(ptr, 0x0F);
            print_string("\nFreeing memory...\n", 0x0F);
            r_free(ptr);
            print_string("Mem test successfully completed!\n", 0x0F);
        } else {
            print_string("Malloc FAILED: Out of memory!\n", 0x0F);
        }
    } else {
        print_string("Usage: mem test\n", 0x0F);
    }
}

// Handles the 'sleep' command
static void sh_sleep(const char *args) {
    if (str_eq(args, "help")) {
        print_string("Command:\n  sleep time(1,3,6...) - wait for the number of seconds you specified", 0x0F);
    } else {
        int seconds = simple_atoi(args);
        if (seconds < 0) {
            print_string("Error: Invalid time format. Use numbers only.\n", 0x0F);
        } else {
            sleep_ticks(seconds * 100);
            print_string("Done sleeping\n", 0x0F);
        }
    }
}

// Handles the multiple 'ata' routing requests
static void sh_ata(const char *args) {
    if (starts_with(args, "write")) {
        cmd_ata_write(args + 5); // skip "write"
    } else if (starts_with(args, "read")) {
        cmd_ata_read(args + 4);  // skip "read"
    } else if (args[0] == '\0' || str_eq(args, "write") || str_eq(args, "read")) {
        ata_usage();
    } else if (str_eq(args, "test")) {
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
    } else if(str_eq(args, "avail")){
        cmd_ata_disc_space();
    } else {
        ata_usage();
    }
}


// Handles the 'rfs' filesystem commands.
static void sh_rfs(const char *args) {
    while (*args == ' ') args++;

    if (str_eq(args, "info")) {
        if (rfs_info() != 0) {
            print_string("RFS info failed. Is the filesystem formatted?\n", 0x0F);
        }
    } else if (str_eq(args, "init")) {
        if (rfs_init() == 0) {
            print_string("RFS initialized successfully.\n", 0x0F);
        } else {
            print_string("RFS init failed: invalid or unformatted filesystem.\n", 0x0F);
        }
    } else if (str_eq(args, "format")) {
        print_string("WARNING: rfs format erases existing RFS metadata!\n", 0x0F);
        print_string("Run this only when you intentionally want to format the filesystem.\n", 0x0F);
        if (rfs_format() == 0) {
            print_string("RFS formatted successfully.\n", 0x0F);
        } else {
            print_string("RFS format failed.\n", 0x0F);
        }
    } else {
        print_string("RFS commands:\n"
                     "  rfs info   - show filesystem information\n"
                     "  rfs init   - validate the filesystem\n"
                     "  rfs format - create/reset filesystem metadata (destructive)\n", 0x0F);
    }
}

// ---------------------------------------------------------------------------
// File and directory commands (cd, pwd, ls, mkdir, rmdir, touch, write, cat, rm, df)
// Paths can be absolute (/docs/a.txt) or relative to the current directory
// (a.txt, docs/a.txt, ../a.txt).
// ---------------------------------------------------------------------------

#define PATH_MAX_LEN 128

// The current directory: always an absolute path with no trailing slash
// ("/" for the root).
static char cwd[PATH_MAX_LEN] = "/";

// Builds a clean absolute path in out from "in", which may be absolute or
// relative to the current directory. "." and ".." and repeated slashes are
// handled here, so RFS only ever sees plain absolute paths.
// out must hold PATH_MAX_LEN bytes. Returns 0, or -1 if the result is too long.
static int resolve_path(const char *in, char *out) {
    unsigned int len = 0;

    if (in[0] != '/') {                          // relative: start from the cwd
        while (cwd[len] != '\0') {
            out[len] = cwd[len];
            len++;
        }
        if (len == 1) len = 0;                   // while building, the root is ""
    }

    while (*in != '\0') {
        unsigned int clen = 0;

        while (*in == '/') in++;
        if (*in == '\0') break;
        while (in[clen] != '\0' && in[clen] != '/') clen++;

        if (clen == 1 && in[0] == '.') {
            // "." means: stay where we are
        } else if (clen == 2 && in[0] == '.' && in[1] == '.') {
            // ".." means: drop the last component (the root has no parent)
            while (len > 0 && out[len - 1] != '/') len--;
            if (len > 0) len--;
        } else {
            if (len + 1 + clen > PATH_MAX_LEN - 1) return -1;
            out[len++] = '/';
            for (unsigned int i = 0; i < clen; i++) out[len++] = in[i];
        }
        in += clen;
    }

    if (len == 0) {
        out[0] = '/';
        out[1] = '\0';
    } else {
        out[len] = '\0';
    }
    return 0;
}

// "RisottOS> " in the root directory, "RisottOS:/docs> " anywhere else.
static void print_prompt(void) {
    print_string("RisottOS", 0x0F);
    if (cwd[1] != '\0') {
        print_char(':', 0x0F);
        print_string(cwd, 0x0F);
    }
    print_string("> ", 0x0F);
}

static void print_rfs_error(int code) {
    const char *msg;
    switch (code) {
        case RFS_E_IO:       msg = "disk error"; break;
        case RFS_E_NOTFOUND: msg = "no such file or directory"; break;
        case RFS_E_EXISTS:   msg = "already exists"; break;
        case RFS_E_FULL:     msg = "file table is full"; break;
        case RFS_E_NOSPACE:  msg = "not enough contiguous free space"; break;
        case RFS_E_NOTEMPTY: msg = "directory is not empty"; break;
        case RFS_E_NOTDIR:   msg = "not a directory"; break;
        case RFS_E_ISDIR:    msg = "is a directory"; break;
        case RFS_E_INVAL:    msg = "invalid path or name"; break;
        case RFS_E_NOFS:     msg = "filesystem not mounted (run: rfs init or rfs format)"; break;
        default:             msg = "unknown error"; break;
    }
    print_string("Error: ", 0x0F);
    print_string(msg, 0x0F);
    print_char('\n', 0x0F);
}

// Takes the first word of args as a path and writes it to path as a clean
// absolute path (relative paths start from the current directory).
// *rest points at whatever follows (one separating space is skipped).
// Returns 0 on success, -1 if there is no word or the path is too long.
static int take_path(const char *args, char *path, const char **rest) {
    char raw[PATH_MAX_LEN];
    unsigned int n = 0;

    while (*args == ' ') args++;
    if (*args == '\0') return -1;

    while (*args != '\0' && *args != ' ') {
        if (n >= PATH_MAX_LEN - 1) return -1;
        raw[n++] = *args++;
    }
    raw[n] = '\0';

    if (*args == ' ') args++;
    *rest = args;
    return resolve_path(raw, path);
}

// Runs a path-only command: mkdir, rmdir, touch, rm.
static void path_cmd(const char *args, const char *usage, int (*op)(const char *)) {
    char path[PATH_MAX_LEN];
    const char *rest;

    if (take_path(args, path, &rest) != 0) {
        print_string(usage, 0x0F);
        return;
    }
    int r = op(path);
    if (r < 0) print_rfs_error(r);
}

static void sh_ls(const char *args) {
    char path[PATH_MAX_LEN];
    const char *rest;

    while (*args == ' ') args++;
    if (*args == '\0') {                         // no path: list the current directory
        unsigned int i = 0;
        while ((path[i] = cwd[i]) != '\0') i++;
    } else if (take_path(args, path, &rest) != 0) {
        print_rfs_error(RFS_E_INVAL);
        return;
    }
    int r = rfs_list(path);
    if (r < 0) {
        print_rfs_error(r);
    } else if (r == 0) {
        print_string("(empty)\n", 0x0F);
    }
}

static void sh_mkdir(const char *args) { path_cmd(args, "Usage: mkdir <path>\n", rfs_mkdir); }
static void sh_rmdir(const char *args) {
    char path[PATH_MAX_LEN];
    const char *rest;

    if (take_path(args, path, &rest) != 0) {
        print_string("Usage: rmdir <path>\n", 0x0F);
        return;
    }
    if (str_eq(path, cwd)) {                     // would leave the shell inside a deleted folder
        print_string("Error: cannot remove the current directory\n", 0x0F);
        return;
    }
    int r = rfs_deldir(path);
    if (r < 0) print_rfs_error(r);
}
static void sh_touch(const char *args) { path_cmd(args, "Usage: touch <path>\n", rfs_create); }
static void sh_rm(const char *args)    { path_cmd(args, "Usage: rm <path>\n",    rfs_delete); }

// write <path> <text>: creates the file or replaces its contents
static void sh_write(const char *args) {
    char path[PATH_MAX_LEN];
    const char *text;
    unsigned int len = 0;

    if (take_path(args, path, &text) != 0) {
        print_string("Usage: write <path> <text>\n", 0x0F);
        return;
    }
    while (text[len] != '\0') len++;

    int r = rfs_write(path, text, len);
    if (r < 0) {
        print_rfs_error(r);
        return;
    }
    print_string("Wrote ", 0x0F);
    print_uint(len);
    print_string(" byte(s)\n", 0x0F);
}

// cat <path>: prints a file (non-printable bytes are shown as '.')
static void sh_cat(const char *args) {
    char path[PATH_MAX_LEN];
    const char *rest;

    if (take_path(args, path, &rest) != 0) {
        print_string("Usage: cat <path>\n", 0x0F);
        return;
    }

    int size = rfs_size(path);
    if (size < 0) {
        print_rfs_error(size);
        return;
    }
    if (size == 0) {
        print_string("(empty file)\n", 0x0F);
        return;
    }

    unsigned char *data = (unsigned char *)r_malloc((unsigned int)size);
    if (data == 0) {
        print_string("Out of memory!\n", 0x0F);
        return;
    }

    int got = rfs_read(path, data, (unsigned int)size);
    if (got < 0) {
        print_rfs_error(got);
        r_free(data);
        return;
    }

    for (int i = 0; i < got; i++) {
        unsigned char c = data[i];
        if (c == '\n' || (c >= 32 && c <= 126)) print_char((char)c, 0x0F);
        else print_char('.', 0x0F);
    }
    print_char('\n', 0x0F);
    r_free(data);
}

// df: free space on the RFS disk area
static void sh_df(const char *args) {
    (void)args;
    unsigned int sectors = rfs_free_sectors();

    print_string("Free: ", 0x0F);
    print_uint(sectors);
    print_string(" sectors (", 0x0F);
    print_uint(sectors / 2u);
    print_string(" KB)\n", 0x0F);
}

// cd [path]: change the current directory ("cd" alone goes to the root)
static void sh_cd(const char *args) {
    char path[PATH_MAX_LEN];
    const char *rest;

    while (*args == ' ') args++;
    if (*args == '\0') {
        cwd[0] = '/';
        cwd[1] = '\0';
        return;
    }
    if (take_path(args, path, &rest) != 0) {
        print_rfs_error(RFS_E_INVAL);
        return;
    }

    int r = rfs_isdir(path);
    if (r < 0) {
        print_rfs_error(r);
    } else if (r == 0) {
        print_rfs_error(RFS_E_NOTDIR);
    } else {
        unsigned int i = 0;
        while ((cwd[i] = path[i]) != '\0') i++;
    }
}

// pwd: show the current directory
static void sh_pwd(const char *args) {
    (void)args;
    print_string(cwd, 0x0F);
    print_char('\n', 0x0F);
}

// ---------------------------------------------------------------------------
// Graphics demos: "ui ..." (text-mode widgets) and "gfx ..." (pixel mode)
// ---------------------------------------------------------------------------

// Block until a key is pressed.
static void wait_for_key(void) {
    while (keyboard_read() == 0) {
        halt_cpu();
    }
}

// Print a number at a screen position (text mode).
static void ui_put_uint(int x, int y, unsigned int n, unsigned char color) {
    char tmp[11];
    int len = 0;

    if (n == 0) {
        vga_putc_at(x, y, '0', color);
        return;
    }
    while (n > 0) {
        tmp[len++] = (char)('0' + (n % 10));
        n /= 10;
    }
    for (int i = 0; len > 0; i++) {
        vga_putc_at(x + i, y, tmp[--len], color);
    }
}

// ui demo: a window with a box, a progress bar and a message
static void ui_demo(void) {
    unsigned char win  = VGA_COLOR(VGA_WHITE, VGA_BLUE);
    unsigned char edge = VGA_COLOR(VGA_YELLOW, VGA_BLUE);
    unsigned char hi   = VGA_COLOR(VGA_LIGHT_CYAN, VGA_BLUE);

    vga_save_screen();
    vga_hide_cursor();

    vga_draw_window(15, 5, 50, 13, " RisottOS UI demo ", edge, win);
    vga_puts_at(18, 7, "Windows, boxes and progress bars", win);
    vga_draw_box(18, 9, 44, 3, hi, VGA_BOX_SINGLE);
    vga_puts_at(20, 10, "Loading, please wait...", win);

    for (unsigned int p = 0; p <= 100; p += 2) {
        vga_progress_bar(18, 13, 44, p,
                         VGA_COLOR(VGA_LIGHT_GREEN, VGA_BLUE),
                         VGA_COLOR(VGA_DARK_GRAY, VGA_BLUE));
        ui_put_uint(18, 14, p, win);
        vga_puts_at(18 + (p >= 100 ? 3 : (p >= 10 ? 2 : 1)), 14, "%  ", win);
        sleep_ticks(3);
    }

    vga_puts_at(18, 16, "Done! Press any key...", edge);
    wait_for_key();

    vga_restore_screen();
    vga_show_cursor();
}

// ui colors: all 16 x 16 foreground/background combinations
static void ui_colors(void) {
    vga_save_screen();
    vga_hide_cursor();
    vga_set_blink(0);                  // so backgrounds 8-15 don't blink

    clear_screen();
    vga_puts_at(2, 1, "All 16 x 16 text colors (rows = background, columns = foreground)", 0x0F);
    for (int bg = 0; bg < 16; bg++) {
        for (int fg = 0; fg < 16; fg++) {
            vga_putc_at(10 + fg * 4,     4 + bg, 'A', VGA_COLOR(fg, bg));
            vga_putc_at(10 + fg * 4 + 1, 4 + bg, 'a', VGA_COLOR(fg, bg));
        }
    }
    vga_puts_at(2, 22, "Press any key...", 0x0F);
    wait_for_key();

    vga_set_blink(1);
    vga_restore_screen();
    vga_show_cursor();
}

static void sh_ui(const char *args) {
    while (*args == ' ') args++;

    if (str_eq(args, "demo")) {
        ui_demo();
    } else if (str_eq(args, "colors")) {
        ui_colors();
    } else {
        print_string("UI commands:\n"
                     "  ui demo   - window with a progress bar\n"
                     "  ui colors - show every color combination\n", 0x0F);
    }
}

// gfx demo: a still scene in 320x200 pixel mode
static void gfx_demo(void) {
    gfx_enter();

    // sky: 20 bands from dark to light blue
    for (int i = 0; i < 20; i++) {
        gfx_fill_rect(0, i * 8, GFX_W, 8, GFX_RGB(0, i / 8, 2 + i * 3 / 20));
    }
    // ground
    gfx_fill_rect(0, 150, GFX_W, 50, GFX_RGB(0, 3, 0));

    // sun
    gfx_fill_circle(250, 45, 22, GFX_YELLOW);
    gfx_circle(250, 45, 26, GFX_BROWN);

    // mountains
    gfx_line(0, 150, 70, 90, GFX_DARK_GRAY);
    gfx_line(70, 90, 140, 150, GFX_DARK_GRAY);
    gfx_line(110, 150, 180, 100, GFX_LIGHT_GRAY);
    gfx_line(180, 100, 250, 150, GFX_LIGHT_GRAY);

    // a house
    gfx_fill_rect(20, 160, 50, 30, GFX_RGB(4, 2, 0));
    gfx_line(15, 160, 45, 140, GFX_RED);
    gfx_line(45, 140, 75, 160, GFX_RED);
    gfx_fill_rect(40, 172, 10, 18, GFX_RGB(2, 1, 0));
    gfx_rect(54, 166, 10, 10, GFX_WHITE);

    // gray ramp and color cube strips
    for (int i = 0; i < 16; i++) gfx_fill_rect(100 + i * 12, 160, 12, 8, GFX_GRAY(i));
    for (int i = 0; i < 36; i++) gfx_fill_rect(100 + i * 5, 172, 5, 8, (unsigned char)(32 + i * 6));

    // text on a title bar
    gfx_fill_rect(0, 0, GFX_W, 20, GFX_BLACK);
    gfx_text(8, 2, "RisottOS 320x200x256", GFX_WHITE, -1);
    gfx_text(8, 182, "Press any key to return", GFX_YELLOW, -1);

    wait_for_key();
    gfx_exit();
}

// gfx ball: a bouncing ball, runs until a key is pressed
// gfx ball: a bouncing ball, runs until a key is pressed
static void gfx_ball(void) {
    int x = 60, y = 50, dx = 1, dy = 1;     // pixels per frame (was 3 and 2)
    const int r = 10;

    gfx_enter();
    gfx_fill_rect(0, 0, GFX_W, GFX_H, GFX_GRAY(1));
    gfx_rect(0, 0, GFX_W, GFX_H, GFX_WHITE);
    gfx_text(8, 4, "Bouncing ball - press any key", GFX_WHITE, -1);

    while (keyboard_read() == 0) {
        // erase a square slightly larger than the ball so the outline can't leave dots
        gfx_fill_rect(x - r - 1, y - r - 1, 2 * r + 3, 2 * r + 3, GFX_GRAY(1));

        x += dx;
        y += dy;
        if (x < r + 1)          { x = r + 1;          dx = -dx; }
        if (x > GFX_W - r - 2)  { x = GFX_W - r - 2;  dx = -dx; }
        if (y < r + 22)         { y = r + 22;         dy = -dy; }
        if (y > GFX_H - r - 2)  { y = GFX_H - r - 2;  dy = -dy; }

        gfx_fill_circle(x, y, r, GFX_LIGHT_RED);
        gfx_circle(x, y, r, GFX_WHITE);
        gfx_wait_vsync();
    }
    gfx_exit();
}

static void sh_gfx(const char *args) {
    while (*args == ' ') args++;

    if (str_eq(args, "demo")) {
        gfx_demo();
    } else if (str_eq(args, "ball")) {
        gfx_ball();
    } else {
        print_string("Pixel graphics commands:\n"
                     "  gfx demo - a still scene (press a key to leave)\n"
                     "  gfx ball - bouncing ball animation\n", 0x0F);
    }
}

// ---------------------------------------------------------------------------
// Shell Command Table Registry Matrix
// ---------------------------------------------------------------------------
static const ShellCommand cmd_table[] = {
    {"clear", sh_clear},
    {"help",  sh_help},
    {"echo",  sh_echo},
    {"rtc",   sh_rtc},
    {"mem",   sh_mem},
    {"sleep", sh_sleep},
    {"ata",   sh_ata},
    {"rfs",   sh_rfs},
    {"ls",    sh_ls},
    {"mkdir", sh_mkdir},
    {"rmdir", sh_rmdir},
    {"touch", sh_touch},
    {"write", sh_write},
    {"cat",   sh_cat},
    {"rm",    sh_rm},
    {"df",    sh_df},
    {"cd",    sh_cd},
    {"pwd",   sh_pwd},
    {"ui",    sh_ui},
    {"gfx",   sh_gfx}
};

// Calculate the safe number of items dynamically inside the array loop
#define CMD_COUNT (sizeof(cmd_table) / sizeof(ShellCommand))

void start_shell() {
    rfs_init();   // mount RFS if the disk has one (otherwise run "rfs format")
    print_char('\n', 0x0F);
    print_prompt();

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

                // COMMAND PROCESSING USING MATRIX REGISTRY TABLE
                if (buffer[0] == '\0') {
                    // empty line, do nothing
                } else {
                    unsigned int i;
                    int cmd_executed = 0;

                    // Iterate through the structured array lookup table
                    for (i = 0; i < CMD_COUNT; i++) {
                        unsigned int cmd_len = 0;
                        while (cmd_table[i].name[cmd_len] != '\0') {
                            cmd_len++;
                        }

                        // Match full direct matching string keywords or spaced inputs
                        if (str_eq(buffer, cmd_table[i].name)) {
                            cmd_table[i].handler(""); // Call function with empty arguments
                            cmd_executed = 1;
                            break;
                        }
                        else if (starts_with(buffer, cmd_table[i].name) && buffer[cmd_len] == ' ') {
                            cmd_table[i].handler(buffer + cmd_len + 1); // Pass sanitized tail parameters
                            cmd_executed = 1;
                            break;
                        }
                    }

                    if (!cmd_executed) {
                        print_string("Command not found: ", 0x0F);
                        print_string(buffer, 0x0F);
                        print_char('\n', 0x0F);
                    }
                }

                index = 0;
                print_prompt();
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