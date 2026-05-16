// Minimal console interface for text mode VGA
#pragma once

typedef unsigned short u16;

void console_clear();
void console_putchar(char c);
void console_print(const char* s);
