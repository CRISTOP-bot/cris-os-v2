// Simple VGA text console implementation
#include "console.h"
#include "asm.h"
#include "keyboard.h"
#include "kstring.h"

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
        if (cursor_x > 0) {
            cursor_x--;
        } else if (cursor_y > 0) {
            cursor_y--;
            cursor_x = VGA_COLS - 1;
        }
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
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    while (*s && x < VGA_COLS && y < VGA_ROWS) {
        if (*s == '\n') { x = 0; y++; }
        else { VGA[y * VGA_COLS + x] = (unsigned char)*s | ((unsigned short)attr << 8); x++; }
        s++;
    }
}

static unsigned long get_cr2(void)
{
	unsigned long val;
	__asm__ volatile("mov %%cr2, %0" : "=r"(val));
	return val;
}

void kernel_panic(const char* message) {
    kernel_panic_ex(message, 0, 0, 0);
}

void kernel_panic_ex(const char* message, unsigned int exception_num,
		     unsigned int error_code, unsigned int *regs) {
    console_clear_color(VGA_ATTR(VGA_WHITE, VGA_RED));

    console_print_color("    _   _       _                        ___  ____  \n", VGA_ATTR(VGA_WHITE, VGA_RED));
    console_print_color("   | \\ | | ___ | |_ ___  ___  _ __     / _ \\/ ___| \n", VGA_ATTR(VGA_WHITE, VGA_RED));
    console_print_color("   |  \\| |/ _ \\| __/ _ \\/ __|| '_ \\   | | | \\___ \\ \n", VGA_ATTR(VGA_WHITE, VGA_RED));
    console_print_color("   | |\\  | (_) | || (_) \\__ \\| | | |  | |_| |___) |\n", VGA_ATTR(VGA_WHITE, VGA_RED));
    console_print_color("   |_| \\_|\\___/ \\__\\___/|___/|_| |_|   \\___/|____/ \n", VGA_ATTR(VGA_WHITE, VGA_RED));
    console_print_color("              !! KERNEL PANIC !!                 \n", VGA_ATTR(VGA_YELLOW, VGA_RED));

    if (message && message[0]) {
        console_print_color("  ", VGA_ATTR(VGA_WHITE, VGA_RED));
        console_print_color(message, VGA_ATTR(VGA_YELLOW, VGA_RED));
        console_print_color("\n", VGA_ATTR(VGA_WHITE, VGA_RED));
    }

    if (exception_num > 0) {
        console_print_color("\n", VGA_ATTR(VGA_WHITE, VGA_RED));
        console_print_color("  Exception: ", VGA_ATTR(VGA_LIGHT_CYAN, VGA_RED));
        char num[16];
        kitoa(exception_num, num, sizeof(num));
        console_print_color(num, VGA_ATTR(VGA_WHITE, VGA_RED));
        if (error_code != 0 || exception_num == 14) {
            console_print_color("  Error Code: 0x", VGA_ATTR(VGA_LIGHT_CYAN, VGA_RED));
            kxtoa(error_code, num, sizeof(num));
            console_print_color(num, VGA_ATTR(VGA_WHITE, VGA_RED));
        }
        console_print_color("\n", VGA_ATTR(VGA_WHITE, VGA_RED));

        if (exception_num == 14) {
            console_print_color("  Fault Addr: 0x", VGA_ATTR(VGA_LIGHT_CYAN, VGA_RED));
            kxtoa(get_cr2(), num, sizeof(num));
            console_print_color(num, VGA_ATTR(VGA_WHITE, VGA_RED));
            console_print_color("\n", VGA_ATTR(VGA_WHITE, VGA_RED));
            console_print_color("  ", VGA_ATTR(VGA_WHITE, VGA_RED));
            if (error_code & 1) console_print_color("PRESENT ", VGA_ATTR(VGA_YELLOW, VGA_RED));
            else console_print_color("NOT-PRESENT ", VGA_ATTR(VGA_YELLOW, VGA_RED));
            if (error_code & 2) console_print_color("WRITE ", VGA_ATTR(VGA_YELLOW, VGA_RED));
            else console_print_color("READ ", VGA_ATTR(VGA_YELLOW, VGA_RED));
            if (error_code & 4) console_print_color("USER ", VGA_ATTR(VGA_YELLOW, VGA_RED));
            else console_print_color("SUPERVISOR ", VGA_ATTR(VGA_YELLOW, VGA_RED));
            if (error_code & 8) console_print_color("RESERVED-BIT ", VGA_ATTR(VGA_YELLOW, VGA_RED));
            if (error_code & 16) console_print_color("INSTR-FETCH ", VGA_ATTR(VGA_YELLOW, VGA_RED));
            console_print_color("\n", VGA_ATTR(VGA_WHITE, VGA_RED));
        }
    }

    if (regs) {
        console_print_color("\n  Register Dump:\n", VGA_ATTR(VGA_LIGHT_CYAN, VGA_RED));
        char buf[32];

        console_print_color("    EAX=0x", VGA_ATTR(VGA_LIGHT_GREY, VGA_RED));
        kxtoa(regs[14], buf, sizeof(buf)); console_print_color(buf, VGA_ATTR(VGA_WHITE, VGA_RED));
        console_print_color("  EBX=0x", VGA_ATTR(VGA_LIGHT_GREY, VGA_RED));
        kxtoa(regs[10], buf, sizeof(buf)); console_print_color(buf, VGA_ATTR(VGA_WHITE, VGA_RED));
        console_print_color("  ECX=0x", VGA_ATTR(VGA_LIGHT_GREY, VGA_RED));
        kxtoa(regs[11], buf, sizeof(buf)); console_print_color(buf, VGA_ATTR(VGA_WHITE, VGA_RED));
        console_print_color("\n", VGA_ATTR(VGA_WHITE, VGA_RED));

        console_print_color("    EDX=0x", VGA_ATTR(VGA_LIGHT_GREY, VGA_RED));
        kxtoa(regs[12], buf, sizeof(buf)); console_print_color(buf, VGA_ATTR(VGA_WHITE, VGA_RED));
        console_print_color("  ESI=0x", VGA_ATTR(VGA_LIGHT_GREY, VGA_RED));
        kxtoa(regs[9], buf, sizeof(buf)); console_print_color(buf, VGA_ATTR(VGA_WHITE, VGA_RED));
        console_print_color("  EDI=0x", VGA_ATTR(VGA_LIGHT_GREY, VGA_RED));
        kxtoa(regs[8], buf, sizeof(buf)); console_print_color(buf, VGA_ATTR(VGA_WHITE, VGA_RED));
        console_print_color("\n", VGA_ATTR(VGA_WHITE, VGA_RED));

        console_print_color("    EBP=0x", VGA_ATTR(VGA_LIGHT_GREY, VGA_RED));
        kxtoa(regs[7], buf, sizeof(buf)); console_print_color(buf, VGA_ATTR(VGA_WHITE, VGA_RED));
        console_print_color("  ESP=0x", VGA_ATTR(VGA_LIGHT_GREY, VGA_RED));
        kxtoa(regs[6], buf, sizeof(buf)); console_print_color(buf, VGA_ATTR(VGA_WHITE, VGA_RED));
        console_print_color("\n", VGA_ATTR(VGA_WHITE, VGA_RED));
    }

    console_print_color("\n  System halted.\n", VGA_ATTR(VGA_YELLOW, VGA_RED));
    console_print_color("  Press any key to block...\n", VGA_ATTR(VGA_LIGHT_GREY, VGA_RED));
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
