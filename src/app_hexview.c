#include "apps.h"
#include "console.h"
#include "keyboard.h"
#include "vfs.h"
#include "kstring.h"
#include "timer.h"
#include <stdbool.h>
#include <stdint.h>

#define HX_DATA_ROWS 21
#define HX_COLS 16
#define HX_HEX_X 8
#define HX_HEX2_X 33
#define HX_ASCII_X 58

static void fmt_hex2(unsigned char v, char *out)
{
	const char *h = "0123456789ABCDEF";
	out[0] = h[v >> 4];
	out[1] = h[v & 0xF];
	out[2] = '\0';
}

static void fmt_hex8(unsigned long v, char *out)
{
	const char *h = "0123456789ABCDEF";
	for (int i = 0; i < 8; i++)
		out[i] = h[(v >> (28 - i * 4)) & 0xF];
	out[8] = '\0';
}

static void clamp_cursor(unsigned long *off, int *row, int *col, size_t size)
{
	if (size == 0) {
		*row = 0;
		*col = 0;
		return;
	}
	unsigned long pos = *off + *row * HX_COLS + *col;
	if (pos >= size) {
		unsigned long last = size - 1;
		unsigned long base = (last / HX_COLS) * HX_COLS;
		*off = base;
		*row = (int)((last - base) / HX_COLS);
		*col = (int)((last - base) % HX_COLS);
	}
}

void app_hexview(const char *filename)
{
	if (!filename || !vfs_exists(filename)) {
		ui_clear();
		ui_put_str(0, 0, "File not found: ", COLOR_ERROR);
		if (filename)
			ui_put_str(16, 0, filename, COLOR_ERROR);
		ui_read_key();
		return;
	}

	const unsigned char *data = (const unsigned char *)vfs_read(filename);
	size_t size = vfs_get_size(filename);
	if (!data || size == 0) {
		ui_clear();
		ui_put_str(0, 0, "Cannot read file or file is empty", COLOR_ERROR);
		ui_read_key();
		return;
	}

	unsigned long offset = 0;
	int crow = 0;
	int ccol = 0;
	bool running = true;
	char buf[80];
	char tmp[16];
	char hex[4];

	while (running) {
		ui_clear();

		kstrcpy(buf, "Hex Viewer - ", sizeof(buf));
		kstrcat(buf, filename, sizeof(buf));
		ui_title_bar(0, 0, SCREEN_W, buf, COLOR_TITLE);

		ui_put_str(0, 1,
			"Offset  00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F  ASCII",
			COLOR_CYAN);

		for (int r = 0; r < HX_DATA_ROWS; r++) {
			unsigned long roff = offset + r * HX_COLS;
			if (roff >= size)
				break;

			fmt_hex8(roff, buf);
			ui_put_str(0, r + 2, buf, COLOR_DIM);

			for (int c = 0; c < HX_COLS; c++) {
				unsigned long idx = roff + c;
				int hx = (c < 8)
					? (HX_HEX_X + c * 3)
					: (HX_HEX2_X + (c - 8) * 3);
				int ax = HX_ASCII_X + c;
				int ry = r + 2;

				if (idx < size) {
					unsigned char b = data[idx];
					bool cur = (r == crow && c == ccol);
					unsigned char ha = cur ? COLOR_HIGHLIGHT : COLOR_WHITE;
					unsigned char aa = cur ? COLOR_HIGHLIGHT : COLOR_GREEN;

					fmt_hex2(b, hex);
					ui_put_str(hx, ry, hex, ha);

					char ch = (b >= 0x20 && b < 0x7F)
						? (char)b : '.';
					ui_put(ax, ry, ch, aa);
				} else {
					ui_put(hx, ry, ' ', COLOR_NORMAL);
					ui_put(hx + 1, ry, ' ', COLOR_NORMAL);
					ui_put(ax, ry, ' ', COLOR_NORMAL);
				}
			}
		}

		unsigned long cur_pos = offset + crow * HX_COLS + ccol;
		kstrcpy(buf, "Offset: ", sizeof(buf));
		fmt_hex8(cur_pos, tmp);
		kstrcat(buf, tmp, sizeof(buf));
		kstrcat(buf, " / ", sizeof(buf));
		kutoa(size, tmp, sizeof(tmp));
		kstrcat(buf, tmp, sizeof(buf));

		char rbuf[32];
		kstrcpy(rbuf, "Size: ", sizeof(rbuf));
		kutoa(size, tmp, sizeof(tmp));
		kstrcat(rbuf, tmp, sizeof(rbuf));

		ui_status_bar(23, buf, rbuf, COLOR_STATUS);

		ui_fill(0, 24, SCREEN_W, 1, ' ', COLOR_NORMAL);
		ui_put_str(1, 24,
			"Up/Down:Row  PgUp/PgDn:Page  Home/End:Start/End  q:Quit",
			COLOR_DIM);

		int key = ui_read_key();

		if (key == 'q' || key == 27) {
			running = false;
		} else if (key == KEY_UP) {
			if (crow > 0) crow--;
		} else if (key == KEY_DOWN) {
			if (crow < HX_DATA_ROWS - 1) {
				unsigned long next = offset
					+ (crow + 1) * HX_COLS + ccol;
				if (next < size) crow++;
			}
		} else if (key == KEY_LEFT) {
			if (ccol > 0) {
				ccol--;
			} else if (crow > 0) {
				crow--;
				ccol = HX_COLS - 1;
			}
		} else if (key == KEY_RIGHT) {
			if (ccol < HX_COLS - 1) {
				if (offset + crow * HX_COLS + ccol + 1 < size)
					ccol++;
			} else if (crow < HX_DATA_ROWS - 1) {
				if (offset + (crow + 1) * HX_COLS < size) {
					crow++;
					ccol = 0;
				}
			}
		} else if (key == KEY_PAGEUP) {
			unsigned long pg = HX_DATA_ROWS * HX_COLS;
			if (offset >= pg)
				offset -= pg;
			else
				offset = 0;
			clamp_cursor(&offset, &crow, &ccol, size);
		} else if (key == KEY_PAGEDOWN) {
			unsigned long pg = HX_DATA_ROWS * HX_COLS;
			unsigned long max_off = (size > 0)
				? ((size - 1) / HX_COLS) * HX_COLS : 0;
			if (offset + pg <= max_off)
				offset += pg;
			else
				offset = max_off;
			clamp_cursor(&offset, &crow, &ccol, size);
		} else if (key == KEY_HOME) {
			offset = 0;
			crow = 0;
			ccol = 0;
		} else if (key == KEY_END) {
			if (size > 0) {
				unsigned long last = size - 1;
				offset = (last / HX_COLS) * HX_COLS;
				crow = (int)((last - offset) / HX_COLS);
				ccol = (int)((last - offset) % HX_COLS);
			}
		}
	}

	ui_clear();
}
