#include "keyboard.h"
#include "console.h"
#include "asm.h"
#include "pic.h"
#include <stdbool.h>

#define SCANCODE_BUF_SIZE 128

/* ---- Keyboard layouts ---- */
struct kb_layout {
	const char *name;
	const unsigned char normal[128];
	const unsigned char shifted[128];
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

static const struct kb_layout layouts[] = {
	{ "us", {0}, {0} },  /* populated at runtime */
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

/* Scancode buffer for IRQ mode (not used yet, keep polling) */
static unsigned char scancode_buf[SCANCODE_BUF_SIZE];
static volatile int buf_head, buf_tail;

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

void keyboard_init(void)
{
	shift_state = false;
	caps_lock = false;
	ctrl_state = false;
	alt_state = false;
	extended_code = false;
	current_layout = KB_LAYOUT_US;
	buf_head = buf_tail = 0;
	layout_populate();
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
		while (!(inb(0x64) & 1))
			;
		sc = inb(0x60);
		if (sc == 0xE0) {
			extended_code = true;
			continue;
		}
		break;
	}
	if (sc & 0x80) {
		sc &= 0x7F;
		if (extended_code) {
			extended_code = false;
			return 0;
		}
		if (sc == 0x2A || sc == 0x36) shift_state = false;
		if (sc == 0x1D) ctrl_state = false;
		if (sc == 0x38) alt_state = false;
		return 0;
	}
	if (extended_code) {
		extended_code = false;
		return 0;
	}
	if (sc == 0x2A || sc == 0x36) { shift_state = true; return 0; }
	if (sc == 0x3A) { caps_lock = !caps_lock; return 0; }
	if (sc == 0x1D) { ctrl_state = true; return 0; }
	if (sc == 0x38) { alt_state = true; return 0; }

	const struct kb_layout *lay = &layouts[current_layout];
	if (sc < 128) {
		char c = shift_state ? lay->shifted[sc] : lay->normal[sc];
		return normalize_key(c);
	}
	return 0;
}

int keyboard_readline(char *buf, int maxlen)
{
	int pos = 0;
	while (1) {
		char c = keyboard_read_char();
		if (!c)
			continue;
		if (c == '\n') {
			console_putchar('\n');
			buf[pos] = 0;
			return pos;
		}
		if (c == '\b') {
			if (pos > 0) {
				--pos;
				console_putchar('\b');
			}
			continue;
		}
		if (pos < maxlen - 1) {
			buf[pos++] = c;
			console_putchar(c);
		}
	}
}

int keyboard_set_layout(int layout)
{
	if (layout < 0 || layout >= (int)(sizeof(layouts) / sizeof(layouts[0])))
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
	shift_state = false;
	extended_code = false;
}
