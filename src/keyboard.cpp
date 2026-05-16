// PS/2 keyboard handling (scancode set 1) - minimal
#include "keyboard.h"
#include "console.h"

typedef unsigned char u8;
static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    asm volatile ("inb %1, %0" : "=a" (ret) : "Nd" (port));
    return ret;
}

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

static char read_scancode_char() {
    static bool shift = false;
    while (!(inb(0x64) & 1)) ;
    unsigned char sc = inb(0x60);
    if (sc == 0) return 0;
    if (sc == 0x2A || sc == 0x36) { shift = true; return 0; }
    if (sc == 0xAA || sc == 0xB6) { shift = false; return 0; }
    if (sc & 0x80) return 0;
    if (sc < 128) {
        if (shift) return shift_map[sc];
        return scancode_map[sc];
    }
    return 0;
}

char keyboard_read_char() {
    return read_scancode_char();
}

int keyboard_readline(char* buf, int maxlen) {
    int pos = 0;
    while (1) {
        char c = read_scancode_char();
        if (!c) continue;
        if (c == '\n') { console_putchar('\n'); buf[pos]=0; return pos; }
        if (c == '\b') { if (pos>0) { pos--; console_putchar('\b'); } continue; }
        if (pos < maxlen-1) { buf[pos++] = c; console_putchar(c); }
    }
}
