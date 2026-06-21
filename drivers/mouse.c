#include "mouse.h"
#include "console.h"
#include "asm.h"
#include "pic.h"
#include <stdbool.h>

#define MOUSE_CMD   0x64
#define MOUSE_DATA  0x60
#define MOUSE_ACK   0xFA

static struct mouse_state mstate;
static unsigned char pkt_buf[3];
static int pkt_idx;

static bool mouse_wait_write(void)
{
	int timeout = 100000;
	while (timeout--) {
		if (!(inb(MOUSE_CMD) & 2))
			return true;
	}
	return false;
}

static bool mouse_wait_read(void)
{
	int timeout = 100000;
	while (timeout--) {
		if (inb(MOUSE_CMD) & 1)
			return true;
	}
	return false;
}

static unsigned char mouse_read(void)
{
	mouse_wait_read();
	return inb(MOUSE_DATA);
}

static void mouse_write(unsigned char val)
{
	mouse_wait_write();
	outb(MOUSE_CMD, 0xD4);
	mouse_wait_write();
	outb(MOUSE_DATA, val);
	mouse_read();  /* ack */
}

void mouse_init(void)
{
	mstate.x = 40;
	mstate.y = 12;
	mstate.buttons = 0;
	mstate.delta_x = 0;
	mstate.delta_y = 0;
	pkt_idx = 0;

	/* Enable PS/2 mouse */
	mouse_wait_write();
	outb(MOUSE_CMD, 0xA8);        /* enable mouse */
	mouse_wait_write();
	outb(MOUSE_CMD, 0x20);        /* get current command byte */
	unsigned char status = mouse_read();
	status |= 2;                   /* enable IRQ12 */
	mouse_wait_write();
	outb(MOUSE_CMD, 0x60);        /* set command byte */
	mouse_wait_write();
	outb(MOUSE_DATA, status);
	mouse_write(0xF6);             /* set default settings */
	mouse_write(0xF4);             /* enable mouse */

	console_print("[ OK ] PS/2 mouse initialized\n");
}

void mouse_handler(void)
{
	unsigned char data = inb(MOUSE_DATA);
	pkt_buf[pkt_idx++] = data;

	if (pkt_idx < 3)
		return;

	pkt_idx = 0;

	mstate.buttons = pkt_buf[0] & 7;

	int dx = (int)(char)pkt_buf[1];
	int dy = -(int)(char)pkt_buf[2];

	mstate.delta_x = dx;
	mstate.delta_y = dy;
	mstate.x += dx;
	mstate.y += dy;

	if (mstate.x < 0) mstate.x = 0;
	if (mstate.x >= 80) mstate.x = 79;
	if (mstate.y < 0) mstate.y = 0;
	if (mstate.y >= 25) mstate.y = 24;
}

void mouse_get_state(struct mouse_state *state)
{
	if (state) {
		state->x = mstate.x;
		state->y = mstate.y;
		state->buttons = mstate.buttons;
		state->delta_x = mstate.delta_x;
		state->delta_y = mstate.delta_y;
	}
}
