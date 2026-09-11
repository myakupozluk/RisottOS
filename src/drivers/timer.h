#ifndef R_TIMER_H
#define R_TIMER_H

void timer_init (unsigned int freq);
void sleep_ticks(unsigned int ticks);
extern volatile unsigned int tick_count;

#endif
