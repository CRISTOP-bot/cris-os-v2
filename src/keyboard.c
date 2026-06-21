#include "keyboard.h"
#include "console.h"
#include "asm.h"
#include <stdbool.h>

typedef unsigned char u8;

static const char scancode_map[128] = {
  0,  27,'1','2','3','4','5','6','7','8','9','0','-','=','\b',
 ' ','q','w','e','r','t','y','u','i','o','p','[',']','\n',
 0,'a','s','d','f','g','h','j','k','l',';', '\'', '`', 0,
 '\\','z','x','c','v','b','n','m',',','.','/',0,'*',0,' ',
};

static const char shift_map[128] = {
  0,  0,'!','@','#','$','%','^','&','*','(',')','_','+','\b',
 ' ', 'Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
 0,'A','S','D','F','G','H','J','K','L',':','"','~',0,
 '|','Z','X','C','V','B','N','M','<','>','?',0,'*',0,' ',
};

static bool shift_state;
static bool caps_lock;
static bool ctrl_state;
static bool alt_state;
static bool extended_code;

void keyboard_init(void)
{
	shift_state = false;
	caps_lock = false;
	ctrl_state = false;
	alt_state = false;
	extended_code = false;
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

static char read_scancode_char(void)
{
	while (!(inb(0x64) & 1))
		;
	unsigned char sc = inb(0x60);
	if (sc == 0)
		return 0;
	if (sc == 0xE0) {
		extended_code = true;
		return 0;
	}
	if (sc & 0x80) {
		sc &= 0x7F;
		if (extended_code) {
			extended_code = false;
			return 0;
		}
		if (sc == 0x2A || sc == 0x36)
			shift_state = false;
		if (sc == 0x1D)
			ctrl_state = false;
		if (sc == 0x38)
			alt_state = false;
		return 0;
	}
	if (extended_code) {
		extended_code = false;
		return 0;
	}
	if (sc == 0x2A || sc == 0x36) {
		shift_state = true;
		return 0;
	}
	if (sc == 0x3A) {
		caps_lock = !caps_lock;
		return 0;
	}
	if (sc == 0x1D) {
		ctrl_state = true;
		return 0;
	}
	if (sc == 0x38) {
		alt_state = true;
		return 0;
	}
	if (sc < 128) {
		char c = shift_state ? shift_map[sc] : scancode_map[sc];
		return normalize_key(c);
	}
	return 0;
}

char keyboard_read_char(void)
{
	return read_scancode_char();
}

int keyboard_readline(char *buf, int maxlen)
{
	int pos = 0;
	while (1) {
		char c = read_scancode_char();
		if (!c)
			continue;
		if (c == '\n') {
			console_putchar('\n');
			buf[pos] = 0;
			return pos;
		}
		if (c == '\b') {
			if (pos > 0) {
				pos--;
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
