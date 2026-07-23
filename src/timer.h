#ifndef TIMER_H
#define TIMER_H

void timer_init(unsigned int frequency);
unsigned long timer_get_ticks(void);
void timer_handler(void);
void timer_sleep(unsigned long ticks);

#endif
