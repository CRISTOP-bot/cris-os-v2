#ifndef CRISOS_KEYBOARD_H
#define CRISOS_KEYBOARD_H

#define KB_LAYOUT_US 0
#define KB_LAYOUT_ES 1
#define KB_LAYOUT_DE 2

void keyboard_init(void);
char keyboard_read_char(void);
int keyboard_readline(char *buf, int maxlen);
int keyboard_set_layout(int layout);
int keyboard_get_layout(void);
void keyboard_flush(void);

#endif
