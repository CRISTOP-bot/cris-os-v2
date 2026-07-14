#ifndef NUCLEOS_MOUSE_H
#define NUCLEOS_MOUSE_H

#include <stdbool.h>

struct mouse_state {
	int x;
	int y;
	unsigned char buttons;
	int delta_x;
	int delta_y;
};

void mouse_init(void);
void mouse_handler(void);
void mouse_get_state(struct mouse_state *state);
void mouse_render(void);
void mouse_hide(void);
bool mouse_clicked(unsigned char btn);
bool mouse_doubleclicked(void);
bool mouse_get_click(int *x, int *y);
int mouse_get_scroll(void);

#endif
