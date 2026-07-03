// Simple VGA text console implementation
#include "console.h"
#include "asm.h"
#include "keyboard.h"

static volatile unsigned short* const VGA = (unsigned short*)0xB8000;
static int cursor_x = 0, cursor_y = 0;

#define VGA_COLS 80
#define VGA_ROWS 25

static inline void scroll_if_needed(void) {
    if (cursor_y >= VGA_ROWS) {
        for (int y = 1; y < VGA_ROWS; ++y)
            for (int x = 0; x < VGA_COLS; ++x)
                VGA[(y-1)*VGA_COLS + x] = VGA[y*VGA_COLS + x];
        cursor_y = VGA_ROWS - 1;
        cursor_x = 0;
        unsigned short blank = (unsigned short)' ' | (VGA_DEFAULT_ATTR << 8);
        for (int x = 0; x < VGA_COLS; ++x)
            VGA[cursor_y * VGA_COLS + x] = blank;
    }
}

void console_putchar(char c) {
    console_putchar_color(c, VGA_DEFAULT_ATTR);
}

void console_putchar_color(char c, unsigned char attr) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\b') {
        if (cursor_x > 0) cursor_x--;
        VGA[cursor_y * VGA_COLS + cursor_x] = (unsigned short)' ' | ((unsigned short)attr << 8);
    } else if ((unsigned char)c >= ' ') {
        if (cursor_x >= VGA_COLS) { cursor_x = 0; cursor_y++; }
        if (cursor_y >= VGA_ROWS) scroll_if_needed();
        VGA[cursor_y * VGA_COLS + cursor_x] = (unsigned short)(unsigned char)c | ((unsigned short)attr << 8);
        cursor_x++;
    }
    if (cursor_y >= VGA_ROWS) scroll_if_needed();
}

void console_print(const char* s) {
    while (*s) console_putchar(*s++);
}

void console_print_color(const char *s, unsigned char attr) {
    while (*s) console_putchar_color(*s++, attr);
}

void console_print_at(int x, int y, const char *s, unsigned char attr) {
    while (*s && x < VGA_COLS) {
        if (*s == '\n') { x = 0; y++; }
        else { VGA[y * VGA_COLS + x] = (unsigned char)*s | ((unsigned short)attr << 8); x++; }
        s++;
    }
}

void kernel_panic(const char* message) {
    console_clear_color(VGA_ATTR(VGA_WHITE, VGA_RED));
    console_print_color("KERNEL PANIC\n\n", VGA_ATTR(VGA_WHITE, VGA_RED));
    if (message && message[0]) {
        console_print_color(message, VGA_ATTR(VGA_WHITE, VGA_RED));
        console_print("\n\n");
    }
    console_print_color("El sistema se ha detenido. Presiona una tecla para bloquear.", VGA_ATTR(VGA_YELLOW, VGA_RED));
    keyboard_read_char();
    halt_cpu();
}

void console_clear_color(unsigned char attr) {
    unsigned short value = (unsigned short) ' ' | ((unsigned short)attr << 8);
    memsetw_asm((void*)VGA, value, 80 * 25);
    cursor_x = 0;
    cursor_y = 0;
}

void console_clear(void) {
    console_clear_color(VGA_DEFAULT_ATTR);
}
