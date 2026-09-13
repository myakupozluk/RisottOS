#ifndef R_RTC_H
#define R_RTC_H

unsigned char cmos_read(unsigned char address);
char* rtc_get_seconds(void);
char* rtc_get_minutes(void);
char* rtc_get_hours(void);
char* rtc_get_time(void);

#endif