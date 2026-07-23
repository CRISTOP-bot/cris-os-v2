#include "serial.h"
#include "asm.h"

#define SERIAL_PORT 0x3F8

void serial_init(void)
{
	outb(SERIAL_PORT + 1, 0x00);
	outb(SERIAL_PORT + 3, 0x80);
	outb(SERIAL_PORT + 0, 0x03);
	outb(SERIAL_PORT + 1, 0x00);
	outb(SERIAL_PORT + 3, 0x03);
	outb(SERIAL_PORT + 2, 0xC7);
	outb(SERIAL_PORT + 4, 0x0B);
}

void serial_putchar(char c)
{
	while (!(inb(SERIAL_PORT + 5) & 0x20));
	outb(SERIAL_PORT, c);
}

void serial_print(const char *s)
{
	while (*s)
		serial_putchar(*s++);
}

char serial_getchar(void)
{
	while (!(inb(SERIAL_PORT + 5) & 0x01));
	return inb(SERIAL_PORT);
}

int serial_available(void)
{
	return (inb(SERIAL_PORT + 5) & 0x01) != 0;
}
