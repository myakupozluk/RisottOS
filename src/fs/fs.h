#ifndef R_FS_H
#define R_FS_H

typedef struct TarHeader {
    char name[100]; // Full name of file
    char mode[8]; // File permissions
    char uid[8]; // File user's ID
    char gid[8]; // File group's ID
    char size[12]; // File size in bytes
    char mtime[12]; // File's last modification date
    char chksum[8]; // checksum number
    char typeflag; // File's type (0 = normal file, 5 = folder)
    char linkname[100]; // Target file (if it's a shortcut)
    char magic[6]; // The magic word that proves type is TAR ("ustar")
    char version[2]; // Version of TAR type (usually "00")
    char uname[32]; // File user's username
    char gname[32]; // File group's group name
    char devmajor[8]; // Hardware device numbers
    char devminor[8];
    char prefix[155]; // If file name exceeds 100 characters, the remainder of the name is written here
    char padding[12]; // Padding to ensure the total comes to exactly 512 bytes (1 sector)
} __attribute__((packed)) TarHeader;

void fs_list_files(void);
void fs_cat_file(const char *filename);

#endif
