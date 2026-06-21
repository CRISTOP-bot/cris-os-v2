#ifndef KEYBOARD_H
#define KEYBOARD_H

void keyboard_init(void);
char keyboard_read_char(void);
int keyboard_readline(char *buf, int maxlen);

#endif /* KEYBOARD_H */
