#ifndef NUCLEOS_TIMER_H
#define NUCLEOS_TIMER_H

void timer_init(unsigned int frequency);
unsigned long timer_get_ticks(void);
void timer_sleep(unsigned long ticks);
void timer_handler(void);

#endif
