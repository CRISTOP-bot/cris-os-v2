#include "gui.h"
#include "console.h"
#include "keyboard.h"
#include "timer.h"
#include "kstring.h"
#include "asm.h"
#include "mouse.h"
#include <stdbool.h>

#define PANEL_ROW 23
#define PANEL_BG   VGA_ATTR(VGA_WHITE, VGA_DARK_GREY)
#define PANEL_ALT  VGA_ATTR(VGA_WHITE, VGA_BLUE)
#define PANEL_CLOCK VGA_ATTR(VGA_LIGHT_GREY, VGA_DARK_GREY)
#define DESKTOP_BG VGA_ATTR(VGA_BLUE, VGA_BLUE)
#define DESKTOP_FG VGA_ATTR(VGA_WHITE, VGA_BLUE)
#define ICON_LABEL VGA_ATTR(VGA_WHITE, VGA_BLUE)
#define ICON_SEL   VGA_ATTR(VGA_BLACK, VGA_LIGHT_GREY)
#define WIN_TITLE  VGA_ATTR(VGA_WHITE, VGA_BLUE)
#define WIN_BORDER VGA_ATTR(VGA_LIGHT_CYAN, VGA_BLUE)
#define WIN_BODY   VGA_ATTR(VGA_WHITE, VGA_BLACK)
#define MENU_SEL   VGA_ATTR(VGA_WHITE, VGA_BLUE)

static unsigned char hour = 0, minute = 0;
static int icon_selected = 0;

static void fill_area(int x, int y, int w, int h, unsigned char attr)
{
	for (int row = y; row < y + h; ++row)
		for (int col = x; col < x + w; ++col)
			console_putxy(col, row, ' ', attr);
}

static void draw_frame(int x, int y, int w, int h, unsigned char attr,
		       int double_lines)
{
	unsigned char hline = double_lines ? 0xCD : 0xC4;
	unsigned char vline = double_lines ? 0xBA : 0xB3;
	unsigned char tl = double_lines ? 0xC9 : 0xDA;
	unsigned char tr = double_lines ? 0xBB : 0xBF;
	unsigned char bl = double_lines ? 0xC8 : 0xC0;
	unsigned char br = double_lines ? 0xBC : 0xD9;
	for (int col = x + 1; col < x + w - 1; ++col) {
		console_putxy(col, y, hline, attr);
		console_putxy(col, y + h - 1, hline, attr);
	}
	for (int row = y + 1; row < y + h - 1; ++row) {
		console_putxy(x, row, vline, attr);
		console_putxy(x + w - 1, row, vline, attr);
	}
	console_putxy(x, y, tl, attr);
	console_putxy(x + w - 1, y, tr, attr);
	console_putxy(x, y + h - 1, bl, attr);
	console_putxy(x + w - 1, y + h - 1, br, attr);
}

static void draw_text(int x, int y, const char *s, unsigned char attr)
{
	while (*s) {
		console_putxy(x, y, *s, attr);
		x++;
		s++;
	}
}

static void draw_desktop(void)
{
	fill_area(0, 0, 80, PANEL_ROW, DESKTOP_BG);
	/* subtle top accent line */
	for (int col = 0; col < 80; ++col)
		console_putxy(col, 0, 0xDF, VGA_ATTR(VGA_LIGHT_BLUE, VGA_BLUE));

	const char *icons[] = {"Home", "Files", "Term", "Settings", "Trash"};
	int ix[] = {3, 21, 39, 57, 3};
	int iy[] = {2, 2, 2, 2, 9};
	for (int i = 0; i < 5; ++i) {
		int x = ix[i], y = iy[i];
		unsigned char attr = (i == icon_selected) ? ICON_SEL : DESKTOP_FG;
		/* folder-like icon */
		console_putxy(x, y, 0xDA, attr);
		console_putxy(x + 1, y, 0xC4, attr);
		console_putxy(x + 2, y, 0xBF, attr);
		console_putxy(x + 1, y + 1, 0xB3, attr);
		for (int col = 0; col < 5; ++col) {
			console_putxy(x - 1 + col, y + 2, 0xC4, attr);
			console_putxy(x - 1 + col, y + 4, 0xC4, attr);
		}
		console_putxy(x - 1, y + 3, 0xB3, attr);
		console_putxy(x + 3, y + 3, 0xB3, attr);
		console_putxy(x - 1, y + 2, 0xC0, attr);
		console_putxy(x + 3, y + 2, 0xD9, attr);
		console_putxy(x - 1, y + 4, 0xC0, attr);
		console_putxy(x + 3, y + 4, 0xD9, attr);
		/* label */
		int lx = x - 1 + (6 - kstrlen(icons[i])) / 2;
		draw_text(lx, y + 6, icons[i], DESKTOP_FG);
	}
}

static void draw_panel(void)
{
	/* panel background */
	fill_area(0, PANEL_ROW, 80, 1, PANEL_BG);
	/* separator line above panel */
	for (int col = 0; col < 80; ++col)
		console_putxy(col, PANEL_ROW - 1, 0xC4, VGA_ATTR(VGA_LIGHT_BLUE, VGA_BLUE));

	/* KDE start button */
	fill_area(0, PANEL_ROW, 7, 1, PANEL_ALT);
	draw_text(0, PANEL_ROW, " \x10 KDE ", PANEL_ALT);

	/* separator */
	console_putxy(7, PANEL_ROW, 0xB3, PANEL_BG);

	/* taskbar middle area */
	draw_text(10, PANEL_ROW, "  CrisOS Plasma  ", VGA_ATTR(VGA_LIGHT_GREY, VGA_DARK_GREY));

	/* clock on right */
	unsigned long ticks = timer_get_ticks();
	minute = (unsigned char)((ticks / 100 / 60) % 60);
	hour = (unsigned char)((ticks / 100 / 3600) % 24);
	char timebuf[6];
	timebuf[0] = '0' + hour / 10;
	timebuf[1] = '0' + hour % 10;
	timebuf[2] = ':';
	timebuf[3] = '0' + minute / 10;
	timebuf[4] = '0' + minute % 10;
	timebuf[5] = '\0';

	int clock_x = 80 - 5 - 4;
	draw_text(clock_x, PANEL_ROW, timebuf, PANEL_CLOCK);
	console_putxy(clock_x - 3, PANEL_ROW, 0xB3, PANEL_BG);
	draw_text(clock_x - 2, PANEL_ROW, "us", PANEL_CLOCK);
}

static void draw_start_menu(void)
{
	int mx = 1, my = 3, mw = 28, mh = 12;
	fill_area(mx, my, mw, mh, WIN_BODY);
	draw_frame(mx, my, mw, mh, VGA_ATTR(VGA_CYAN, VGA_BLACK), 0);
	/* title bar */
	fill_area(mx + 1, my + 1, mw - 2, 1, WIN_TITLE);
	draw_text(mx + 2, my + 1, "  Application Launcher", WIN_TITLE);

	const char *items[] = {
		"Terminal", "Files", "System Status",
		"Settings", "Lock Screen", "Leave (Shell)", "Shutdown"
	};
	for (int i = 0; i < 7; ++i) {
		int row = my + 3 + i;
		unsigned char a = (i == 0) ? MENU_SEL : WIN_BODY;
		fill_area(mx + 1, row, mw - 2, 1, a);
		char buf[28];
		int j;
		for (j = 0; items[i][j] && j < 24; ++j)
			buf[j] = items[i][j];
		buf[j] = '\0';
		draw_text(mx + 3, row, buf, a);
	}
	draw_text(mx + 2, my + mh - 2, "  \x18\x19 Navigate  Enter:OK  Q:Close", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
}

static void run_start_menu_item(int idx)
{
	console_clear();
	switch (idx) {
	case 0:
		console_print_color("Launching Terminal...\n\n", VGA_ATTR(VGA_CYAN, VGA_BLACK));
		return;
	case 1:
		console_print_color("Opening Files...\n\n", VGA_ATTR(VGA_CYAN, VGA_BLACK));
		return;
	case 2:
		console_print_color("System Status\n\n", VGA_ATTR(VGA_CYAN, VGA_BLACK));
		console_print("Kernel: CrisOS v3 i386\n");
		unsigned long ticks = timer_get_ticks();
		char buf[16];
		kitoa(ticks / 100, buf, sizeof(buf));
		console_print("Uptime: ");
		console_print(buf);
		console_print(" seconds\n");
		break;
	case 3:
		console_print_color("Settings\n\n", VGA_ATTR(VGA_CYAN, VGA_BLACK));
		console_print("Keyboard layout: ");
		int lay = keyboard_get_layout();
		console_print(lay == 1 ? "es" : lay == 2 ? "de" : "us");
		console_print("\n");
		break;
	case 4:
		console_print_color("Screen locked. Press any key to unlock.\n", VGA_ATTR(VGA_YELLOW, VGA_BLACK));
		keyboard_read_char();
		return;
	case 5:
		return;
	case 6:
		console_print("Shutting down...\n");
		halt_cpu();
		break;
	}
	console_print("\nPress any key to return.\n");
	keyboard_read_char();
}

static int get_region(int mx, int my, bool in_menu)
{
	if (in_menu) {
		/* start menu items: rows 6..12, col 2..27 */
		if (my >= 6 && my <= 12 && mx >= 2 && mx <= 27)
			return 10 + (my - 6);
		return -1;
	}
	/* start button */
	if (my == 23 && mx >= 0 && mx <= 6)
		return 0;
	/* desktop icons */
	if (my >= 1 && my <= 8 && mx >= 1 && mx <= 6)  return 1;  /* Home */
	if (my >= 1 && my <= 8 && mx >= 19 && mx <= 24) return 2;  /* Files */
	if (my >= 1 && my <= 8 && mx >= 37 && mx <= 42) return 3;  /* Term */
	if (my >= 1 && my <= 8 && mx >= 55 && mx <= 60) return 4;  /* Settings */
	if (my >= 8 && my <= 15 && mx >= 1 && mx <= 6)  return 5;  /* Trash */
	return -1;
}

static bool handle_mouse(int mx, int my, bool *start_open, int *menu_idx)
{
	if (*start_open) {
		int r = get_region(mx, my, true);
		if (r >= 10 && r <= 16) {
			int idx = r - 10;
			run_start_menu_item(idx);
			if (idx == 5) {
				console_clear();
				return true;
			}
			if (idx == 0 || idx == 1) {
				console_clear();
				return true;
			}
			*start_open = false;
		} else {
			*start_open = false;
		}
		return false;
	}
	/* check start button */
	if (get_region(mx, my, false) == 0) {
		*start_open = true;
		*menu_idx = 0;
		return false;
	}
	int r = get_region(mx, my, false);
	if (r >= 1 && r <= 5) {
		icon_selected = r - 1;
		int icon_actions[] = {0, 1, 2, 3, 4};
		run_start_menu_item(icon_actions[icon_selected]);
		if (icon_actions[icon_selected] == 0 ||
		    icon_actions[icon_selected] == 1) {
			console_clear();
			return true;
		}
	}
	return false;
}

void gui_show(void)
{
	icon_selected = 0;
	bool start_open = false;
	int menu_idx = 0;

	while (1) {
		draw_desktop();
		draw_panel();

		if (start_open) {
			draw_start_menu();
			console_putxy(2, 3 + 3 + menu_idx, 0x10, MENU_SEL);
		}

		/* render mouse cursor on top of everything */
		mouse_render();

		/* poll both keyboard and mouse */
		char c = 0;
		for (int poll = 0; poll < 2000; poll++) {
			if (keyboard_data_available()) {
				c = keyboard_read_char();
				break;
			}
			int mx, my;
			if (mouse_get_click(&mx, &my)) {
				if (handle_mouse(mx, my, &start_open, &menu_idx))
					return;
				c = 0;
				break;
			}
		}

		if (!c)
			continue;

		if (start_open) {
			switch (c) {
			case 'q': case 'Q':
				start_open = false;
				break;
			case 'w': case 'W':
				if (menu_idx > 0) menu_idx--;
				break;
			case 's': case 'S':
				if (menu_idx < 6) menu_idx++;
				break;
			case '\r':
			case ' ':
				run_start_menu_item(menu_idx);
				if (menu_idx == 5) {
					console_clear();
					return;
				}
				if (menu_idx == 0 || menu_idx == 1) {
					console_clear();
					return;
				}
				start_open = false;
				break;
			}
		} else {
			switch (c) {
			case 'q': case 'Q':
				console_clear();
				return;
			case 'k': case 'K':
			case '\r':
				start_open = true;
				menu_idx = 0;
				break;
			case 'd': case 'D':
				icon_selected = (icon_selected + 1) % 5;
				break;
			case 'a': case 'A':
				icon_selected = (icon_selected - 1 + 5) % 5;
				break;
			case 'w': case 'W':
				icon_selected = (icon_selected - 1 + 5) % 5;
				break;
			case 's': case 'S':
				icon_selected = (icon_selected + 1) % 5;
				break;
			}
		}
	}
}
