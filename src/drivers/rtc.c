#include "rtc.h"
#include "../arch/io.h"

unsigned char cmos_read(unsigned char address) {
    outb(0x70, address);
    return inb(0x71);
}

static unsigned char bcd_to_decimal(unsigned char bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

char* rtc_get_seconds(void) {
    static char buf[3];
    unsigned char bcd = cmos_read(0x00);
    unsigned int val = bcd_to_decimal(bcd);
    buf[0] = '0' + (val / 10);
    buf[1] = '0' + (val % 10);
    buf[2] = '\0';
    return buf;
}

char* rtc_get_minutes(void) {
    static char buf[3];
    unsigned char bcd = cmos_read(0x02);
    unsigned int val = bcd_to_decimal(bcd);
    buf[0] = '0' + (val / 10);
    buf[1] = '0' + (val % 10);
    buf[2] = '\0';
    return buf;
}

char* rtc_get_hours(void) {
    static char buf[3];
    unsigned char bcd = cmos_read(0x04);
    bcd = bcd & 0x3F;
    unsigned int val = bcd_to_decimal(bcd);
    buf[0] = '0' + (val / 10);
    buf[1] = '0' + (val % 10);
    buf[2] = '\0';
    return buf;
}

char* rtc_get_time(void) {
    static char buf[9];
    char* h = rtc_get_hours();
    char* m = rtc_get_minutes();
    char* s = rtc_get_seconds();

    buf[0] = h[0];
    buf[1] = h[1];
    buf[2] = ':';
    buf[3] = m[0];
    buf[4] = m[1];
    buf[5] = ':';
    buf[6] = s[0];
    buf[7] = s[1];
    buf[8] = '\0';

    return buf;
}