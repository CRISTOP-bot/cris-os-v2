#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

void serial_init(void);
void serial_putchar(char c);
void serial_print(const char *s);
char serial_getchar(void);
int serial_available(void);

#endif
