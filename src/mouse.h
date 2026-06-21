#ifndef CRISOS_MOUSE_H
#define CRISOS_MOUSE_H

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

#endif
