// Minimal console interface for text mode VGA
#pragma once

typedef unsigned short u16;

#ifdef __cplusplus
extern "C" {
#endif

void console_clear();
void console_clear_color(unsigned char attr);
void console_putchar(char c);
void console_putxy(int x, int y, char c, unsigned char attr);
void console_print(const char* s);
void kernel_panic(const char* message);

#ifdef __cplusplus
}
#endif
