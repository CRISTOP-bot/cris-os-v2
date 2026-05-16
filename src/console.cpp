// Simple VGA text console implementation
#include "console.h"

volatile unsigned short* VGA = (unsigned short*)0xB8000;
static int cursor_x = 0, cursor_y = 0;

static inline void scroll_if_needed() {
    if (cursor_y >= 25) {
        for (int y = 1; y < 25; ++y)
            for (int x = 0; x < 80; ++x)
                VGA[(y-1)*80 + x] = VGA[y*80 + x];
        cursor_y = 24; cursor_x = 0;
        for (int x = 0; x < 80; ++x) VGA[cursor_y*80 + x] = (unsigned short) ' ' | (0x07 << 8);
    }
}

void console_putchar(char c) {
    const unsigned short attr = (unsigned short)0x07;
    if (c == '\n') {
        cursor_x = 0; cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\b') {
        if (cursor_x > 0) cursor_x--;
        VGA[cursor_y * 80 + cursor_x] = (unsigned short)(' ') | (attr << 8);
    } else {
        VGA[cursor_y * 80 + cursor_x] = (unsigned short)c | (attr << 8);
        cursor_x++;
        if (cursor_x >= 80) { cursor_x = 0; cursor_y++; }
    }
    scroll_if_needed();
}

void console_print(const char* s) {
    while (*s) console_putchar(*s++);
}

void console_clear() {
    const unsigned short attr = (unsigned short)0x07;
    for (int y=0;y<25;y++) for (int x=0;x<80;x++) VGA[y*80 + x] = (unsigned short) ' ' | (attr << 8);
    cursor_x = 0; cursor_y = 0;
}
