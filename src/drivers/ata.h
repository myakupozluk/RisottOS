#ifndef ATA_H
#define ATA_H
#define SECTOR_SIZE 512

// Defined Constant Port Numbers

#define ATA_DATA 0x1F0
#define ATA_SECTOR_COUNT 0x1F2
#define ATA_LBA_LOW 0x1F3
#define ATA_LBA_MED 0x1F4
#define ATA_LBA_HIGH 0x1F5
#define ATA_DRIVE_SELECT 0x1F6
#define ATA_COMMAND 0x1F7
#define ATA_CONTROL 0x3F6

// Status Bit Constants
#define ATA_ERR 0x01
#define ATA_DRQ 0x08
#define ATA_DF 0x20
#define ATA_BSY 0x80

// Command Constants
#define ATA_READ 0x20
#define ATA_WRITE 0x30
#define ATA_FLUSH 0xE7
#define ATA_IDENTIFY 0xEC


void ata_delay(void);
int ata_wait_ready(void);
int ata_wait_drq(void);
int ata_wait_irq(void);
void ata_read_words(unsigned short port, unsigned short *buffer, unsigned int words);
void ata_write_words(unsigned short port, const unsigned short *buffer, unsigned int words);
void ata_irq_handler(void);
void ata_init(void);
int ata_read_sector(unsigned int lba, unsigned short *buffer);
int ata_write_sector(unsigned int lba, const unsigned short *buffer);
unsigned int ata_disc_space(void);

#endif