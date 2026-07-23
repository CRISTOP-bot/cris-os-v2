#include "timer.h"
#include "asm.h"
#include "process.h"

#define TIMER_CMD  0x43
#define TIMER_DATA 0x40

static volatile unsigned long timer_ticks;

static void timer_set_freq(unsigned int frequency)
{
	if (frequency == 0)
		frequency = 100;
	unsigned int divisor = 1193180 / frequency;
	outb(TIMER_CMD, 0x36);
	outb(TIMER_DATA, (unsigned char)(divisor & 0xFF));
	outb(TIMER_DATA, (unsigned char)((divisor >> 8) & 0xFF));
}

void timer_init(unsigned int frequency)
{
	timer_ticks = 0;
	timer_set_freq(frequency);
}

unsigned long timer_get_ticks(void)
{
	return timer_ticks;
}

void timer_handler(void)
{
	++timer_ticks;
	if (timer_ticks % 10 == 0) {
		process_schedule();
	}
}
