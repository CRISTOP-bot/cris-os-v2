#ifndef NUCLEOS_KEYBOARD_H
#define NUCLEOS_KEYBOARD_H

#define KB_LAYOUT_US 0
#define KB_LAYOUT_ES 1
#define KB_LAYOUT_DE 2

void keyboard_init(void);
char keyboard_read_char(void);
int keyboard_data_available(void);
int keyboard_readline(char *buf, int maxlen);
int keyboard_set_layout(int layout);
int keyboard_get_layout(void);
void keyboard_flush(void);
int keyboard_read_scancode(void);
char keyboard_scancode_to_char(int scancode);
void keyboard_irq_handler(void);

#define SC_BACKSPACE 0x0E
#define SC_TAB       0x0F
#define SC_ESCAPE    0x01
#define SC_ENTER     0x1C
#define SC_SPACE     0x39
#define SC_DELETE    0x53
#define SC_HOME      0x47
#define SC_END       0x4F
#define SC_PAGEUP    0x49
#define SC_PAGEDOWN  0x51
#define SC_UP        0x48
#define SC_DOWN      0x50
#define SC_LEFT      0x4B
#define SC_RIGHT     0x4D

#define SC_0      0x0B
#define SC_1      0x02
#define SC_2      0x03
#define SC_3      0x04
#define SC_4      0x05
#define SC_5      0x06
#define SC_6      0x07
#define SC_7      0x08
#define SC_8      0x09
#define SC_9      0x0A

#define SC_A      0x1E
#define SC_B      0x30
#define SC_C      0x2E
#define SC_D      0x20
#define SC_E      0x12
#define SC_F      0x21
#define SC_G      0x22
#define SC_H      0x23
#define SC_I      0x17
#define SC_J      0x24
#define SC_K      0x25
#define SC_L      0x26
#define SC_M      0x32
#define SC_N      0x31
#define SC_O      0x18
#define SC_P      0x19
#define SC_Q      0x10
#define SC_R      0x13
#define SC_S      0x1F
#define SC_T      0x14
#define SC_U      0x16
#define SC_V      0x2F
#define SC_W      0x11
#define SC_X      0x2D
#define SC_Y      0x15
#define SC_Z      0x2C

#endif
