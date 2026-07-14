#include "apps.h"
#include "console.h"
#include "keyboard.h"
#include "kstring.h"
#include "timer.h"
#include <stdbool.h>

void ui_clear(void)
{
    console_clear();
}

void ui_put(int x, int y, char c, unsigned char attr)
{
    if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H)
        console_putxy(x, y, c, attr);
}

void ui_put_str(int x, int y, const char *s, unsigned char attr)
{
    while (*s && x < SCREEN_W) {
        ui_put(x++, y, *s++, attr);
    }
}

void ui_fill(int x, int y, int w, int h, char c, unsigned char attr)
{
    for (int row = y; row < y + h && row < SCREEN_H; row++)
        for (int col = x; col < x + w && col < SCREEN_W; col++)
            ui_put(col, row, c, attr);
}

void ui_box(int x, int y, int w, int h, unsigned char attr)
{
    /* corners */
    ui_put(x, y, 0xDA, attr);
    ui_put(x + w - 1, y, 0xBF, attr);
    ui_put(x, y + h - 1, 0xC0, attr);
    ui_put(x + w - 1, y + h - 1, 0xD9, attr);
    /* horizontal */
    for (int i = x + 1; i < x + w - 1; i++) {
        ui_put(i, y, 0xC4, attr);
        ui_put(i, y + h - 1, 0xC4, attr);
    }
    /* vertical */
    for (int i = y + 1; i < y + h - 1; i++) {
        ui_put(x, i, 0xB3, attr);
        ui_put(x + w - 1, i, 0xB3, attr);
    }
}

void ui_double_box(int x, int y, int w, int h, unsigned char attr)
{
    ui_put(x, y, 0xC9, attr);
    ui_put(x + w - 1, y, 0xBB, attr);
    ui_put(x, y + h - 1, 0xC8, attr);
    ui_put(x + w - 1, y + h - 1, 0xBC, attr);
    for (int i = x + 1; i < x + w - 1; i++) {
        ui_put(i, y, 0xCD, attr);
        ui_put(i, y + h - 1, 0xCD, attr);
    }
    for (int i = y + 1; i < y + h - 1; i++) {
        ui_put(x, i, 0xBA, attr);
        ui_put(x + w - 1, i, 0xBA, attr);
    }
}

void ui_hline(int x, int y, int w, unsigned char attr)
{
    for (int i = 0; i < w; i++)
        ui_put(x + i, y, 0xC4, attr);
}

void ui_status_bar(int y, const char *left, const char *right, unsigned char attr)
{
    ui_fill(0, y, SCREEN_W, 1, ' ', attr);
    if (left) ui_put_str(1, y, left, attr);
    if (right) {
        int rlen = kstrlen(right);
        ui_put_str(SCREEN_W - rlen - 1, y, right, attr);
    }
}

void ui_title_bar(int x, int y, int w, const char *title, unsigned char attr)
{
    ui_fill(x + 1, y, w - 2, 1, ' ', attr);
    int tlen = kstrlen(title);
    int tx = x + (w - tlen) / 2;
    if (tx < x + 1) tx = x + 1;
    ui_put_str(tx, y, title, attr);
}

int ui_read_key(void)
{
    while (1) {
        int sc = keyboard_read_scancode();
        if (sc < 0) continue;
        if (sc & 0x100) return sc;
        if (sc == 0x1C) return '\n';
        if (sc == 0x0E) return '\b';
        if (sc == 0x39) return ' ';
        if (sc == 0x01) return 27;
        if (sc == 0x0F) return '\t';
        if (!(sc & 0x80)) {
            char c = keyboard_read_char();
            if (c) return c;
        }
    }
}

int ui_read_key_timeout(int ticks)
{
    unsigned long end = timer_get_ticks() + ticks;
    keyboard_flush();
    while (timer_get_ticks() < end) {
        if (keyboard_data_available()) {
            return ui_read_key();
        }
    }
    return -1;
}
