#ifndef CONSOLE_H
#define CONSOLE_H

void console_clear(void);
void console_clear_color(unsigned char attr);
void console_putchar(char c);
void console_putxy(int x, int y, char c, unsigned char attr);
void console_print(const char *s);
void kernel_panic(const char *message);

#endif /* CONSOLE_H */
