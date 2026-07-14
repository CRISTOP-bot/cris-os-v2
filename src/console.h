#ifndef CONSOLE_H
#define CONSOLE_H

/* VGA color constants */
#define VGA_BLACK        0x0
#define VGA_BLUE         0x1
#define VGA_GREEN        0x2
#define VGA_CYAN         0x3
#define VGA_RED          0x4
#define VGA_MAGENTA      0x5
#define VGA_BROWN        0x6
#define VGA_LIGHT_GREY   0x7
#define VGA_DARK_GREY    0x8
#define VGA_LIGHT_BLUE   0x9
#define VGA_LIGHT_GREEN  0xA
#define VGA_LIGHT_CYAN   0xB
#define VGA_LIGHT_RED    0xC
#define VGA_LIGHT_MAGENTA 0xD
#define VGA_YELLOW       0xE
#define VGA_WHITE        0xF

#define VGA_ATTR(fg, bg) ((unsigned char)(((bg) << 4) | (fg)))
#define VGA_DEFAULT_ATTR VGA_ATTR(VGA_LIGHT_GREY, VGA_BLACK)

void console_clear(void);
void console_clear_color(unsigned char attr);
void console_putchar(char c);
void console_putchar_color(char c, unsigned char attr);
void console_putxy(int x, int y, char c, unsigned char attr);
void console_print(const char *s);
void console_print_color(const char *s, unsigned char attr);
void console_print_at(int x, int y, const char *s, unsigned char attr);
void kernel_panic(const char *message);
void kernel_panic_ex(const char *message, unsigned int exception_num,
		     unsigned int error_code, unsigned int *regs);

#endif /* CONSOLE_H */
