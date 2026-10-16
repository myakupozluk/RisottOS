#include "../arch/pic.h"
#include "../arch/io.h"
#include "ata.h"
static volatile int irq_status = 0;
static volatile int irq_flag = 0;
void ata_delay(void){
    int i = 0;
    while(i < 4){
        inb(ATA_CONTROL);
        i++;
    }
}
int ata_wait_ready(void){
    int counter = 100000;
    while(counter > 0){
        int status = inb(ATA_COMMAND);
        if((status & ATA_BSY) == 0){
            return 0;
        }
        counter--;
    }
    return -1;
}
int ata_wait_drq(void){
    int counter = 100000;
    while(counter > 0){
        int status = inb(ATA_COMMAND);
        if(((ATA_ERR | ATA_DF) & status)){
            return -1;
        }
        if(((status & ATA_BSY) == 0) && ((status & ATA_DRQ) > 0)){
            return 0;
        }
        counter--;
    }
    return -1;
}
int ata_wait_irq(void){
    int counter = 10000000;
    while(counter > 0){
        if(irq_flag == 1){
            return 0;
        }
        counter--;
    }
    return -1;
}
void ata_irq_handler(void){
    irq_status = inb(ATA_COMMAND);
    irq_flag = 1;


}
void ata_read_words(unsigned short port, unsigned short *buffer, unsigned int words){
    unsigned int counter = 0;
    while(counter < words){
        buffer[counter] = inw(port);
        counter++;
    }
}
void ata_write_words(unsigned short port, const unsigned short *buffer, unsigned int words){
    unsigned int counter = 0;
    while(counter < words){
        outw(port,buffer[counter]);
        counter++;
    }
}
void ata_init(void){
    outb(ATA_CONTROL,0);

}
int ata_write_sector(unsigned int lba, const unsigned short *buffer){
    if(ata_wait_ready() == -1){
        return -1;
    }

    outb(ATA_DRIVE_SELECT, 0xE0 | ((lba >> 24) & 0x0F));
    ata_delay();

    outb(ATA_SECTOR_COUNT, 1);
    outb(ATA_LBA_LOW,  lba & 0xFF);
    outb(ATA_LBA_MED,  (lba >> 8) & 0xFF);
    outb(ATA_LBA_HIGH, (lba >> 16) & 0xFF);

    outb(ATA_COMMAND, ATA_WRITE);

    // no IRQ before the first sector on writes, so poll for DRQ
    if(ata_wait_drq() == -1){
        return -1;
    }

    irq_flag = 0;
    ata_write_words(ATA_DATA, buffer, 256);

    // the drive raises an IRQ once it has accepted the data
    if(ata_wait_irq() == -1){
        return -1;
    }
    if((ATA_ERR | ATA_DF) & irq_status){
        return -1;
    }

    // flush the drive's write cache to the disk
    outb(ATA_COMMAND, ATA_FLUSH);
    if(ata_wait_ready() == -1){
        return -1;
    }

    return 0;
}
int ata_read_sector(unsigned int lba, unsigned short *buffer){
    if(ata_wait_ready() == -1){
        return -1;
    }

    outb(ATA_DRIVE_SELECT, 0xE0 | ((lba >> 24) & 0x0F));
    ata_delay();

    outb(ATA_SECTOR_COUNT, 1);
    outb(ATA_LBA_LOW,  lba & 0xFF);
    outb(ATA_LBA_MED,  (lba >> 8) & 0xFF);
    outb(ATA_LBA_HIGH, (lba >> 16) & 0xFF);

    irq_flag = 0;
    outb(ATA_COMMAND, ATA_READ);

    if(ata_wait_irq() == -1){
        return -1;
    }

    if((ATA_ERR | ATA_DF) & irq_status){
        return -1;
    }
    if(!(irq_status & ATA_DRQ)){
        return -1;
    }

    ata_read_words(ATA_DATA, buffer, 256);
    return 0;
}

// Returns the total number of 512-byte sectors on the master drive,
// or 0 if there is no ATA drive / the command failed.
unsigned int ata_disc_space(void){
    static unsigned short identify[256];

    if(ata_wait_ready() == -1){
        return 0;
    }

    outb(ATA_DRIVE_SELECT, 0xA0);   // master, IDENTIFY does not use LBA mode
    ata_delay();

    outb(ATA_SECTOR_COUNT, 0);
    outb(ATA_LBA_LOW, 0);
    outb(ATA_LBA_MED, 0);
    outb(ATA_LBA_HIGH, 0);

    outb(ATA_COMMAND, ATA_IDENTIFY);
    ata_delay();

    // status 0 means there is no drive on this channel
    if(inb(ATA_COMMAND) == 0){
        return 0;
    }

    if(ata_wait_ready() == -1){
        return 0;
    }

    // non-zero here means it is not a plain ATA drive (ATAPI/SATA)
    if(inb(ATA_LBA_MED) != 0 || inb(ATA_LBA_HIGH) != 0){
        return 0;
    }

    if(ata_wait_drq() == -1){
        return 0;
    }

    ata_read_words(ATA_DATA, identify, 256);

    // words 60 (low) and 61 (high) = total user-addressable sectors
    return ((unsigned int)identify[61] << 16) | identify[60];
}