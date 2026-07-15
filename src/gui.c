#include "gui.h"
#include "console.h"
#include "keyboard.h"
#include "timer.h"
#include "kstring.h"
#include "asm.h"
#include "mouse.h"
#include "vfs.h"
#include <stdbool.h>

#define PANEL_ROW 23
#define DESKTOP_ROWS 23
#define VGA_COLS 80

#define PANEL_BG   VGA_ATTR(VGA_WHITE, VGA_DARK_GREY)
#define PANEL_ALT  VGA_ATTR(VGA_WHITE, VGA_BLUE)
#define PANEL_CLK  VGA_ATTR(VGA_LIGHT_GREY, VGA_DARK_GREY)
#define DESKTOP_BG VGA_ATTR(VGA_BLUE, VGA_BLUE)
#define DESKTOP_FG VGA_ATTR(VGA_WHITE, VGA_BLUE)
#define ICON_SEL   VGA_ATTR(VGA_BLACK, VGA_LIGHT_GREY)
#define WIN_TITLE  VGA_ATTR(VGA_WHITE, VGA_BLUE)
#define WIN_BODY   VGA_ATTR(VGA_WHITE, VGA_BLACK)
#define MENU_SEL   VGA_ATTR(VGA_WHITE, VGA_BLUE)

#define NUM_ICONS 5
#define NUM_MENU_ITEMS 7

static int icon_selected;
static int menu_idx;
static bool start_open;

static void safe_putxy(int x, int y, char c, unsigned char attr)
{
	if (x >= 0 && x < VGA_COLS && y >= 0 && y < 25)
		console_putxy(x, y, c, attr);
}

static void fill_area_safe(int x, int y, int w, int h, unsigned char attr)
{
	for (int row = y; row < y + h && row < 25; ++row)
		for (int col = x; col < x + w && col < VGA_COLS; ++col)
			if (col >= 0 && row >= 0)
				safe_putxy(col, row, ' ', attr);
}

static void draw_frame_safe(int x, int y, int w, int h, unsigned char attr, int dbl)
{
	unsigned char hl = dbl ? 0xCD : 0xC4;
	unsigned char vl = dbl ? 0xBA : 0xB3;
	unsigned char tl = dbl ? 0xC9 : 0xDA;
	unsigned char tr = dbl ? 0xBB : 0xBF;
	unsigned char bl = dbl ? 0xC8 : 0xC0;
	unsigned char br = dbl ? 0xBC : 0xD9;
	for (int col = x + 1; col < x + w - 1; ++col) {
		safe_putxy(col, y, hl, attr);
		safe_putxy(col, y + h - 1, hl, attr);
	}
	for (int row = y + 1; row < y + h - 1; ++row) {
		safe_putxy(x, row, vl, attr);
		safe_putxy(x + w - 1, row, vl, attr);
	}
	safe_putxy(x, y, tl, attr);
	safe_putxy(x + w - 1, y, tr, attr);
	safe_putxy(x, y + h - 1, bl, attr);
	safe_putxy(x + w - 1, y + h - 1, br, attr);
}

static void draw_text_safe(int x, int y, const char *s, unsigned char attr)
{
	while (*s && x < VGA_COLS && y < 25) {
		if (x >= 0 && y >= 0)
			safe_putxy(x, y, *s, attr);
		x++;
		s++;
	}
}

static void draw_separator(void)
{
	for (int col = 0; col < 80; ++col)
		safe_putxy(col, PANEL_ROW - 1, 0xC4, VGA_ATTR(VGA_LIGHT_BLUE, VGA_BLUE));
}

static void draw_top_bar(void)
{
	for (int col = 0; col < 80; ++col)
		safe_putxy(col, 0, 0xDF, VGA_ATTR(VGA_LIGHT_BLUE, VGA_BLUE));
}

static void draw_home_icon(int x, int y, unsigned char attr)
{
	safe_putxy(x, y, ' ', attr);
	safe_putxy(x + 1, y, 0x1F, attr);
	safe_putxy(x + 2, y, ' ', attr);
	safe_putxy(x - 1, y + 1, 0x1F, attr);
	safe_putxy(x, y + 1, 0x1F, attr);
	safe_putxy(x + 1, y + 1, 0x1F, attr);
	safe_putxy(x + 2, y + 1, 0x1F, attr);
	safe_putxy(x + 3, y + 1, 0x1F, attr);
	safe_putxy(x - 1, y + 2, 0xB3, attr);
	safe_putxy(x, y + 2, ' ', attr);
	safe_putxy(x + 1, y + 2, 0xDB, attr);
	safe_putxy(x + 2, y + 2, ' ', attr);
	safe_putxy(x + 3, y + 2, 0xB3, attr);
	safe_putxy(x - 1, y + 3, 0xC0, attr);
	safe_putxy(x, y + 3, 0xC4, attr);
	safe_putxy(x + 1, y + 3, 0xC4, attr);
	safe_putxy(x + 2, y + 3, 0xC4, attr);
	safe_putxy(x + 3, y + 3, 0xD9, attr);
}

static void draw_folder_icon(int x, int y, unsigned char attr)
{
	safe_putxy(x - 1, y, 0xDA, attr);
	safe_putxy(x, y, 0xC4, attr);
	safe_putxy(x + 1, y, 0xC4, attr);
	safe_putxy(x + 2, y, 0xC4, attr);
	safe_putxy(x + 3, y, 0xBF, attr);
	safe_putxy(x - 1, y + 1, 0xB3, attr);
	safe_putxy(x + 3, y + 1, 0xB3, attr);
	safe_putxy(x - 1, y + 2, 0xC0, attr);
	safe_putxy(x, y + 2, 0xC4, attr);
	safe_putxy(x + 1, y + 2, 0xC4, attr);
	safe_putxy(x + 2, y + 2, 0xC4, attr);
	safe_putxy(x + 3, y + 2, 0xD9, attr);
}

static void draw_terminal_icon(int x, int y, unsigned char attr)
{
	safe_putxy(x - 1, y, 0xDA, attr);
	safe_putxy(x, y, 0xC4, attr);
	safe_putxy(x + 1, y, 0xC4, attr);
	safe_putxy(x + 2, y, 0xC4, attr);
	safe_putxy(x + 3, y, 0xBF, attr);
	safe_putxy(x - 1, y + 1, 0xB3, attr);
	safe_putxy(x + 3, y + 1, 0xB3, attr);
	safe_putxy(x, y + 1, '>', attr);
	safe_putxy(x + 1, y + 1, '_', attr);
	safe_putxy(x - 1, y + 2, 0xB3, attr);
	safe_putxy(x + 3, y + 2, 0xB3, attr);
	safe_putxy(x - 1, y + 3, 0xC0, attr);
	safe_putxy(x, y + 3, 0xC4, attr);
	safe_putxy(x + 1, y + 3, 0xC4, attr);
	safe_putxy(x + 2, y + 3, 0xC4, attr);
	safe_putxy(x + 3, y + 3, 0xD9, attr);
}

static void draw_gear_icon(int x, int y, unsigned char attr)
{
	safe_putxy(x, y, 'O', attr);
	safe_putxy(x - 1, y + 1, '/', attr);
	safe_putxy(x + 1, y + 1, '\\', attr);
	safe_putxy(x - 1, y + 2, '\\', attr);
	safe_putxy(x + 1, y + 2, '/', attr);
	safe_putxy(x, y + 3, 'O', attr);
}

static void draw_trash_icon(int x, int y, unsigned char attr)
{
	safe_putxy(x, y, '_', attr);
	safe_putxy(x + 1, y, '_', attr);
	safe_putxy(x + 2, y, '_', attr);
	safe_putxy(x - 1, y + 1, 0xDA, attr);
	safe_putxy(x, y + 1, 0xC4, attr);
	safe_putxy(x + 1, y + 1, 0xC4, attr);
	safe_putxy(x + 2, y + 1, 0xC4, attr);
	safe_putxy(x + 3, y + 1, 0xBF, attr);
	safe_putxy(x - 1, y + 2, 0xB3, attr);
	safe_putxy(x, y + 2, '!', attr);
	safe_putxy(x + 1, y + 2, ' ', attr);
	safe_putxy(x + 2, y + 2, '!', attr);
	safe_putxy(x + 3, y + 2, 0xB3, attr);
	safe_putxy(x - 1, y + 3, 0xC0, attr);
	safe_putxy(x, y + 3, 0xC4, attr);
	safe_putxy(x + 1, y + 3, 0xC4, attr);
	safe_putxy(x + 2, y + 3, 0xC4, attr);
	safe_putxy(x + 3, y + 3, 0xD9, attr);
}

static void draw_desktop(void)
{
	fill_area_safe(0, 0, 80, DESKTOP_ROWS, DESKTOP_BG);
	draw_top_bar();

	const char *names[] = {"Home", "Files", "Term", "Settings", "Trash"};
	int ix[] = {3, 21, 39, 57, 3};
	int iy[] = {2, 2, 2, 2, 9};
	void (*icon_funcs[])(int, int, unsigned char) = {
		draw_home_icon, draw_folder_icon, draw_terminal_icon,
		draw_gear_icon, draw_trash_icon
	};

	for (int i = 0; i < NUM_ICONS; ++i) {
		int x = ix[i], y = iy[i];
		unsigned char attr = (i == icon_selected) ? ICON_SEL : DESKTOP_FG;
		icon_funcs[i](x, y, attr);
		int lx = x - 1 + (6 - kstrlen(names[i])) / 2;
		draw_text_safe(lx, y + 5, names[i], DESKTOP_FG);
	}
}

static void draw_panel(void)
{
	fill_area_safe(0, PANEL_ROW, 80, 1, PANEL_BG);
	draw_separator();

	fill_area_safe(0, PANEL_ROW, 8, 1, PANEL_ALT);
	draw_text_safe(0, PANEL_ROW, " *NIX  ", PANEL_ALT);
	safe_putxy(8, PANEL_ROW, 0xB3, PANEL_BG);
	draw_text_safe(10, PANEL_ROW, "  NucleOS Desktop  ", VGA_ATTR(VGA_LIGHT_GREY, VGA_DARK_GREY));

	unsigned long ticks = timer_get_ticks();
	int minute = (ticks / 100 / 60) % 60;
	int hour = (ticks / 100 / 3600) % 24;
	char timebuf[6];
	timebuf[0] = '0' + hour / 10;
	timebuf[1] = '0' + hour % 10;
	timebuf[2] = ':';
	timebuf[3] = '0' + minute / 10;
	timebuf[4] = '0' + minute % 10;
	timebuf[5] = '\0';

	int clock_x = 71;
	draw_text_safe(clock_x, PANEL_ROW, timebuf, PANEL_CLK);
	safe_putxy(clock_x - 1, PANEL_ROW, 0xB3, PANEL_BG);
	draw_text_safe(clock_x - 7, PANEL_ROW, "  us  ", PANEL_CLK);
}

static void draw_start_menu(void)
{
	int mx = 1, my = 3, mw = 28, mh = 12;
	fill_area_safe(mx, my, mw, mh, WIN_BODY);
	draw_frame_safe(mx, my, mw, mh, VGA_ATTR(VGA_CYAN, VGA_BLACK), 0);
	fill_area_safe(mx + 1, my + 1, mw - 2, 1, WIN_TITLE);
	draw_text_safe(mx + 2, my + 1, "  Application Launcher", WIN_TITLE);

	const char *items[] = {
		"Terminal", "Files", "System Status",
		"Settings", "Lock Screen", "Leave (Shell)", "Shutdown"
	};
	for (int i = 0; i < NUM_MENU_ITEMS; ++i) {
		int row = my + 3 + i;
		if (row >= 25) break;
		unsigned char a = (i == menu_idx) ? MENU_SEL : WIN_BODY;
		fill_area_safe(mx + 1, row, mw - 2, 1, a);
		draw_text_safe(mx + 3, row, items[i], a);
	}
	draw_text_safe(mx + 2, my + mh - 2, "  \x18\x19 Navigate  Enter:OK  Q:Close",
			VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
	safe_putxy(mx + 2, my + 3 + menu_idx, 0x10, MENU_SEL);
}

static void run_start_menu_item(int idx)
{
	if (idx < 0 || idx >= NUM_MENU_ITEMS) return;
	console_clear();
	switch (idx) {
	case 0:
		console_print_color("Terminal\n\n", VGA_ATTR(VGA_CYAN, VGA_BLACK));
		console_print("Type 'help' for commands.\n");
		break;
	case 1:
		console_print_color("Files\n\n", VGA_ATTR(VGA_CYAN, VGA_BLACK));
		if (!vfs_list(0))
			console_print("(empty)\n");
		break;
	case 2: {
		console_print_color("System Status\n\n", VGA_ATTR(VGA_CYAN, VGA_BLACK));
		console_print("Kernel: NucleOS v3 x86_64\n");
		unsigned long t = timer_get_ticks();
		char buf[16];
		kitoa(t / 100, buf, sizeof(buf));
		console_print("Uptime: ");
		console_print(buf);
		console_print("s\n");
		break;
	}
	case 3:
		console_print_color("Settings\n\n", VGA_ATTR(VGA_CYAN, VGA_BLACK));
		console_print("Keyboard: ");
		int lay = keyboard_get_layout();
		console_print(lay == 1 ? "es" : lay == 2 ? "de" : "us");
		console_print("\n");
		break;
	case 4:
		console_print_color("Screen locked. Press any key.\n", VGA_ATTR(VGA_YELLOW, VGA_BLACK));
		keyboard_read_char();
		return;
	case 5:
		return;
	case 6:
		console_print("Shutting down...\n");
		halt_cpu();
		return;
	}
	console_print("\nPress any key to return.\n");
	keyboard_read_char();
}

static int get_menu_item(int mx, int my)
{
	if (my >= 6 && my <= 12 && mx >= 2 && mx <= 27)
		return my - 6;
	return -1;
}

static int get_icon_at(int mx, int my)
{
	if (my >= 1 && my <= 8) {
		if (mx >= 1 && mx <= 6)   return 0;
		if (mx >= 19 && mx <= 24) return 1;
		if (mx >= 37 && mx <= 42) return 2;
		if (mx >= 55 && mx <= 60) return 3;
	}
	if (my >= 9 && my <= 15 && mx >= 1 && mx <= 6) return 4;
	return -1;
}

static bool handle_mouse_click(int mx, int my)
{
	if (start_open) {
		int item = get_menu_item(mx, my);
		if (item >= 0 && item < NUM_MENU_ITEMS) {
			menu_idx = item;
			run_start_menu_item(item);
			start_open = false;
			if (item == 5) return true;
			if (item == 0 || item == 1) return true;
		} else {
			start_open = false;
		}
		return false;
	}
	if (my == PANEL_ROW && mx >= 0 && mx <= 7) {
		start_open = true;
		menu_idx = 0;
		return false;
	}
	int icon = get_icon_at(mx, my);
	if (icon >= 0 && icon < NUM_ICONS) {
		icon_selected = icon;
		run_start_menu_item(icon);
		start_open = false;
		if (icon == 0 || icon == 1) return true;
	}
	return false;
}

static void handle_key(char c)
{
	if (start_open) {
		switch (c) {
		case 'q': case 'Q':
			start_open = false;
			break;
		case 'w': case 'W':
			if (menu_idx > 0) menu_idx--;
			break;
		case 's': case 'S':
			if (menu_idx < NUM_MENU_ITEMS - 1) menu_idx++;
			break;
		case '\r': case ' ':
			run_start_menu_item(menu_idx);
			start_open = false;
			if (menu_idx == 5) break;
			if (menu_idx == 0 || menu_idx == 1) break;
			break;
		}
	} else {
		switch (c) {
		case 'q': case 'Q':
			break;
		case 'k': case 'K': case '\r':
			start_open = true;
			menu_idx = 0;
			break;
		case 'd': case 'D':
			icon_selected = (icon_selected + 1) % NUM_ICONS;
			break;
		case 'a': case 'A':
			icon_selected = (icon_selected + NUM_ICONS - 1) % NUM_ICONS;
			break;
		}
	}
}

void gui_show(void)
{
	icon_selected = 0;
	start_open = false;
	menu_idx = 0;

	while (1) {
		draw_desktop();
		draw_panel();
		if (start_open)
			draw_start_menu();
		mouse_render();

		char c = 0;
		for (int poll = 0; poll < 1500; ++poll) {
			if (keyboard_data_available()) {
				c = keyboard_read_char();
				break;
			}
			int mx, my;
			if (mouse_get_click(&mx, &my)) {
				if (handle_mouse_click(mx, my)) {
					console_clear();
					return;
				}
				break;
			}
		}

		if (c == 'q' && !start_open) {
			console_clear();
			return;
		}
		if (c)
			handle_key(c);
	}
}
