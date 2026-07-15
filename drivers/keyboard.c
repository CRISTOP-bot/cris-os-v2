#include "keyboard.h"
#include "console.h"
#include "asm.h"
#include "pic.h"
#include "mouse.h"
#include <stdbool.h>

#define SCANCODE_BUF_SIZE 128

/* ---- Keyboard layouts ---- */
struct kb_layout {
        const char *name;
        unsigned char normal[128];
        unsigned char shifted[128];
};

static const unsigned char us_normal[128] = {
        0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
        ' ', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
        0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
        '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ',
};

static const unsigned char us_shifted[128] = {
        0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
        ' ', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
        0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,
        '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ',
};

static const unsigned char es_normal[128] = {
        0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '\'', '!', '\b',
        ' ', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '`', '+', '\n',
        0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 'n', 0xAC, 0, 0,
        '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '-', 0, '*', 0, ' ',
};

static const unsigned char es_shifted[128] = {
        0, 0, '"', 0x27, '.', '5', 0, '7', '8', '9', '0', '\'', '?', '!', '\b',
        ' ', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '^', '*', '\n',
        0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', 'N', 0xBF, '`', 0,
        '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', ';', ':', '_', 0, '*', 0, ' ',
};

/* German QWERTZ layout */
static const unsigned char de_normal[128] = {
        0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', 0xDF, 0xB4, '\b',
        ' ', 'q', 'w', 'e', 'r', 't', 'z', 'u', 'i', 'o', 'p', 0xFC, '+', '\n',
        0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 0xF6, 0xE4, '^', 0,
        '\\', 'y', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '-', 0, '*', 0, ' ',
};

static const unsigned char de_shifted[128] = {
        0, 0, '!', '"', 0xA7, '$', '%', '&', '/', '(', ')', '=', '?', '`', '\b',
        ' ', 'Q', 'W', 'E', 'R', 'T', 'Z', 'U', 'I', 'O', 'P', 0xDC, '*', '\n',
        0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', 0xD6, 0xC4, 0xB0, 0,
        0xDF, 'Y', 'X', 'C', 'V', 'B', 'N', 'M', ';', ':', '_', 0, '*', 0, ' ',
};

static struct kb_layout layouts[] = {
        { "us", {0}, {0} },
        { "es", {0}, {0} },
        { "de", {0}, {0} },
};

static int current_layout;

/* ---- Keyboard state ---- */
static bool shift_state;
static bool caps_lock;
static bool ctrl_state;
static bool alt_state;
static bool extended_code;

static void layout_populate(void)
{
	for (int i = 0; i < 128; ++i) {
		layouts[0].normal[i]  = us_normal[i];
		layouts[0].shifted[i] = us_shifted[i];
		layouts[1].normal[i]  = es_normal[i];
		layouts[1].shifted[i] = es_shifted[i];
		layouts[2].normal[i]  = de_normal[i];
		layouts[2].shifted[i] = de_shifted[i];
	}
}

static int wait_keyboard_data(void)
{
        int timeout = 50000;
        while (timeout--) {
                unsigned char status = inb(0x64);
                if (status & 1) {
                        if (!(status & 0x20))
                                return 0;
                        (void)inb(0x60);
                }
        }
        return -1;
}

static const char *layout_names[] = { "US", "ES", "DE" };

static int detect_layout(void)
{
	int ch;

	console_print("\n");
	console_print_color("  Keyboard layout selection:\n", VGA_ATTR(VGA_WHITE, VGA_BLACK));
	console_print("\n");
	console_print_color("    [1] ", VGA_ATTR(VGA_LIGHT_CYAN, VGA_BLACK));
	console_print_color("US (English)", VGA_DEFAULT_ATTR);
	console_print_color("  - Standard QWERTY\n", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
	console_print_color("    [2] ", VGA_ATTR(VGA_LIGHT_CYAN, VGA_BLACK));
	console_print_color("ES (Spanish)", VGA_DEFAULT_ATTR);
	console_print_color("  - QWERTY with ñ\n", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
	console_print_color("    [3] ", VGA_ATTR(VGA_LIGHT_CYAN, VGA_BLACK));
	console_print_color("DE (German)", VGA_DEFAULT_ATTR);
	console_print_color("  - QWERTZ\n", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
	console_print("\n");
	console_print_color("  Select [1/2/3]: ", VGA_ATTR(VGA_GREEN, VGA_BLACK));

	current_layout = KB_LAYOUT_US;

	{
		volatile int timeout = 100;
		while (timeout--) {
			if (wait_keyboard_data() == 0) {
				unsigned char sc = inb(0x60);
				if (sc & 0x80)
					continue;
				if (sc == SC_1) { ch = KB_LAYOUT_US; goto layout_done; }
				if (sc == SC_2) { ch = KB_LAYOUT_ES; goto layout_done; }
				if (sc == SC_3) { ch = KB_LAYOUT_DE; goto layout_done; }
			}
		}
	}
	ch = KB_LAYOUT_US;

layout_done:
	current_layout = ch;

	console_print_color(layout_names[ch], VGA_ATTR(VGA_WHITE, VGA_BLACK));
	console_print("\n");

	return ch;
}

void keyboard_init(void)
{
	shift_state = false;
	caps_lock = false;
	ctrl_state = false;
	alt_state = false;
	extended_code = false;
	current_layout = KB_LAYOUT_US;

	/* PS/2 controller initialization for real hardware */
	/* Disable devices during initialization */
	outb(0x64, 0xAD);
	outb(0x64, 0xA7);

	/* Flush output buffer */
	while (inb(0x64) & 1)
		(void)inb(0x60);

	/* Self-test PS/2 controller */
	outb(0x64, 0xAA);
	/* Wait for response with timeout */
	{
		int t = 100000;
		while (t-- && !(inb(0x64) & 1))
			;
		if (t > 0)
			(void)inb(0x60);
	}

	/* Enable keyboard */
	outb(0x64, 0xAE);

	/* Try to set scan code set 1 */
	outb(0x60, 0xF0);
	/* Wait for ACK */
	{
		int t = 100000;
		while (t-- && !(inb(0x64) & 1))
			;
		if (t > 0) {
			unsigned char ack = inb(0x60);
			if (ack == 0xFA) {
				outb(0x60, 0x01);
				t = 100000;
				while (t-- && !(inb(0x64) & 1))
					;
				if (t > 0)
					(void)inb(0x60);
			}
		}
	}

	/* Set typematic rate (fastest) */
	outb(0x60, 0xF3);
	/* Wait for ACK */
	{
		int t = 100000;
		while (t-- && !(inb(0x64) & 1))
			;
		if (t > 0)
			(void)inb(0x60);
	}
	outb(0x60, 0x00);
	/* Wait for ACK */
	{
		int t = 100000;
		while (t-- && !(inb(0x64) & 1))
			;
		if (t > 0)
			(void)inb(0x60);
	}

	/* Enable scanning */
	outb(0x60, 0xF4);
	/* Wait for ACK */
	{
		int t = 100000;
		while (t-- && !(inb(0x64) & 1))
			;
		if (t > 0)
			(void)inb(0x60);
	}

	layout_populate();
	detect_layout();
}

static char normalize_key(char c)
{
        if (c >= 'a' && c <= 'z') {
                if (caps_lock ^ shift_state)
                        return c - 'a' + 'A';
        } else if (c >= 'A' && c <= 'Z') {
                if (caps_lock && !shift_state)
                        return c - 'A' + 'a';
        }
        return c;
}

char keyboard_read_char(void)
{
        unsigned char sc;

        while (1) {
                if (wait_keyboard_data() < 0) {
                        mouse_render();
                        continue;
                }

                sc = inb(0x60);

                if (sc == 0xE0) {
                        extended_code = true;
                        continue;
                }

                if (extended_code) {
                        extended_code = false;
                        continue;
                }

                if (sc == 0x2A || sc == 0x36) { /* shift press */
                        shift_state = true;
                        continue;
                }
                if (sc == 0xAA || sc == 0xB6) { /* shift release */
                        shift_state = false;
                        continue;
                }
                if (sc == 0x1D) { /* ctrl press */
                        ctrl_state = true;
                        continue;
                }
                if (sc == 0x9D) { /* ctrl release */
                        ctrl_state = false;
                        continue;
                }
                if (sc == 0x38) { /* alt press */
                        alt_state = true;
                        continue;
                }
                if (sc == 0xB8) { /* alt release */
                        alt_state = false;
                        continue;
                }
                if (sc == 0x3A) { /* caps lock */
                        caps_lock = !caps_lock;
                        continue;
                }

                if (!(sc & 0x80)) {  /* key press */
                        char c = shift_state ? layouts[current_layout].shifted[sc]
                                             : layouts[current_layout].normal[sc];
                        return normalize_key(c);
                }
        }
}

int keyboard_data_available(void)
{
	return inb(0x64) & 1;
}

int keyboard_readline(char *buf, int maxlen)
{
        int pos = 0;
        while (pos < maxlen - 1) {
                char c = keyboard_read_char();
                if (c == '\n' || c == '\r') {
                        console_putchar('\n');
                        buf[pos] = '\0';
                        return pos;
                } else if (c == '\b') {
                        if (pos > 0) {
                                --pos;
                                console_putchar('\b');
                        }
                } else if (c >= ' ') {
                        buf[pos++] = c;
                        console_putchar(c);
                }
        }
        buf[pos] = '\0';
        return pos;
}

int keyboard_set_layout(int layout)
{
        if (layout < KB_LAYOUT_US || layout > KB_LAYOUT_DE)
                return -1;
        current_layout = layout;
        return 0;
}

int keyboard_get_layout(void)
{
        return current_layout;
}

void keyboard_flush(void)
{
	while (inb(0x64) & 1)
		(void)inb(0x60);
}

int keyboard_read_scancode(void)
{
	while (1) {
		if (wait_keyboard_data() < 0) {
			mouse_render();
			continue;
		}
		unsigned char sc = inb(0x60);
		if (sc == 0xE0) {
			if (wait_keyboard_data() < 0)
				continue;
			unsigned char ext = inb(0x60);
			if (!(ext & 0x80))
				return ext | 0x100;
			continue;
		}
		if (sc == 0x2A || sc == 0x36) { shift_state = true; continue; }
		if (sc == 0xAA || sc == 0xB6) { shift_state = false; continue; }
		if (sc == 0x1D) { ctrl_state = true; continue; }
		if (sc == 0x9D) { ctrl_state = false; continue; }
		if (sc == 0x38) { alt_state = true; continue; }
		if (sc == 0xB8) { alt_state = false; continue; }
		if (sc == 0x3A) { caps_lock = !caps_lock; continue; }
		if (sc & 0x80)
			continue;
		return sc;
	}
}

char keyboard_scancode_to_char(int scancode)
{
	if (scancode & 0x100)
		return 0;
	if (scancode < 0 || scancode >= 128)
		return 0;
	char c = shift_state ? layouts[current_layout].shifted[scancode]
	                     : layouts[current_layout].normal[scancode];
	return normalize_key(c);
}
