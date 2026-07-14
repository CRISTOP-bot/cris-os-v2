#include "mouse.h"
#include "console.h"
#include "asm.h"
#include "pic.h"
#include "timer.h"
#include <stdbool.h>

#define MOUSE_CMD   0x64
#define MOUSE_DATA  0x60
#define MOUSE_ACK   0xFA
#define VGA_BUF     ((volatile unsigned short*)0xB8000)

static struct mouse_state mstate;
static unsigned char pkt_buf[4];
static int pkt_idx;
static int packet_size = 3;

static int cursor_old_x = -1, cursor_old_y = -1;
static unsigned short cursor_saved_cell = 0;
static bool cursor_shown = false;

static unsigned char prev_buttons = 0;
static int click_x = -1, click_y = -1;
static bool click_pending = false;
static bool doubleclick_pending = false;

static unsigned long last_click_time = 0;
#define DOUBLECLICK_TICKS 25

static int scroll_delta = 0;

static int accel_lut[] = {
	0, 0, 0, 1, 1, 1, 2, 2, 3, 4, 5, 6, 8, 10, 12, 15
};

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
	mouse_read();
}

static int mouse_read_config(void)
{
	mouse_wait_write();
	outb(MOUSE_CMD, 0x20);
	return mouse_read();
}

static void mouse_write_config(unsigned char cfg)
{
	mouse_wait_write();
	outb(MOUSE_CMD, 0x60);
	mouse_wait_write();
	outb(MOUSE_DATA, cfg);
}

static int abs_val(int v) { return v < 0 ? -v : v; }

static int apply_acceleration(int delta)
{
	int mag = abs_val(delta);
	int sign = delta >= 0 ? 1 : -1;
	if (mag >= 16) mag = 15;
	return delta + (accel_lut[mag] * sign);
}

void mouse_init(void)
{
	mstate.x = 40;
	mstate.y = 12;
	mstate.buttons = 0;
	mstate.delta_x = 0;
	mstate.delta_y = 0;
	pkt_idx = 0;
	scroll_delta = 0;

	/* Enable auxiliary device (mouse) */
	mouse_wait_write();
	outb(MOUSE_CMD, 0xA8);

	/* Enable IRQ12 in the PS/2 controller */
	unsigned char cfg = mouse_read_config();
	cfg |= 2;    /* enable IRQ12 */
	cfg &= ~0x20; /* disable mouse clock scaling */
	mouse_write_config(cfg);

	/* Reset mouse and detect capabilities */
	mouse_write(0xFF);
	mouse_read(); /* consume response */

	/* Set defaults */
	mouse_write(0xF6);
	mouse_read();

	/* Try to enable scroll wheel: send 0xF3 (set sample rate) 3 times */
	mouse_write(0xF3);
	mouse_read();
	mouse_write(200);
	mouse_read();

	mouse_write(0xF3);
	mouse_read();
	mouse_write(100);
	mouse_read();

	mouse_write(0xF3);
	mouse_read();
	mouse_write(80);
	mouse_read();

	/* Read device ID */
	mouse_write(0xF2);
	unsigned char dev_id = mouse_read();
	if (dev_id == 0x03) {
		packet_size = 4;
	}

	/* Enable data reporting */
	mouse_write(0xF4);
	mouse_read();

	console_print("[ OK ] PS/2 mouse initialized");
	if (packet_size == 4)
		console_print(" (scroll wheel)");
	console_print("\n");
}

void mouse_handler(void)
{
	unsigned char data = inb(MOUSE_DATA);
	pkt_buf[pkt_idx++] = data;

	if (pkt_idx < packet_size)
		return;

	pkt_idx = 0;

	mstate.buttons = pkt_buf[0] & 7;

	int dx = (int)(char)pkt_buf[1];
	int dy = -(int)(char)pkt_buf[2];
	scroll_delta = 0;

	if (packet_size == 4) {
		signed char sd = (signed char)(pkt_buf[3] & 0x0F);
		if (sd > 7) sd -= 16;
		scroll_delta = -(int)sd;
	}

	dx = apply_acceleration(dx);
	dy = apply_acceleration(dy);

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

int mouse_get_scroll(void)
{
	int s = scroll_delta;
	scroll_delta = 0;
	return s;
}

void mouse_render(void)
{
	mouse_hide();

	if (mstate.x < 0 || mstate.x >= 80 || mstate.y < 0 || mstate.y >= 25)
		return;

	cursor_saved_cell = VGA_BUF[mstate.y * 80 + mstate.x];

	unsigned char old_attr = (cursor_saved_cell >> 8) & 0xFF;
	unsigned char old_fg = old_attr & 0x0F;
	unsigned char old_bg = (old_attr >> 4) & 0x0F;
	unsigned char inv_attr = (old_bg == old_fg)
		? VGA_ATTR(VGA_WHITE, VGA_BLACK)
		: VGA_ATTR(old_bg, old_fg);
	unsigned short cursor_cell = (cursor_saved_cell & 0xFF)
		| ((unsigned short)inv_attr << 8);
	VGA_BUF[mstate.y * 80 + mstate.x] = cursor_cell;

	cursor_old_x = mstate.x;
	cursor_old_y = mstate.y;
	cursor_shown = true;

	/* detect left click release */
	if ((prev_buttons & 1) && !(mstate.buttons & 1)) {
		unsigned long now = timer_get_ticks();
		if ((now - last_click_time) < DOUBLECLICK_TICKS) {
			doubleclick_pending = true;
		}
		click_x = mstate.x;
		click_y = mstate.y;
		click_pending = true;
		last_click_time = now;
	}
	prev_buttons = mstate.buttons;
}

void mouse_hide(void)
{
	if (cursor_shown) {
		if (cursor_old_x >= 0 && cursor_old_x < 80 &&
		    cursor_old_y >= 0 && cursor_old_y < 25) {
			VGA_BUF[cursor_old_y * 80 + cursor_old_x] = cursor_saved_cell;
		}
		cursor_shown = false;
	}
	cursor_old_x = -1;
	cursor_old_y = -1;
}

bool mouse_clicked(unsigned char btn)
{
	if (btn == 0 && click_pending) {
		click_pending = false;
		return true;
	}
	return false;
}

bool mouse_doubleclicked(void)
{
	if (doubleclick_pending) {
		doubleclick_pending = false;
		return true;
	}
	return false;
}

bool mouse_get_click(int *x, int *y)
{
	if (click_pending) {
		click_pending = false;
		if (x) *x = click_x;
		if (y) *y = click_y;
		return true;
	}
	return false;
}
