#include "apps.h"
#include "console.h"
#include "keyboard.h"
#include "vfs.h"
#include "kstring.h"
#include "timer.h"
#include <stdbool.h>
#include <stdint.h>

#define BUF_SIZE 4096
#define CONTENT_ROWS 23
#define LINE_NUM_W 4

static char buf[BUF_SIZE];
static int buf_len;
static int cursor_pos;
static int scroll_y;
static bool modified;
static const char *g_filename;

static int count_lines(void)
{
	int n = 1;
	if (buf_len == 0)
		return 1;
	for (int i = 0; i < buf_len; i++)
		if (buf[i] == '\n')
			n++;
	return n;
}

static int find_line_start(int pos)
{
	if (pos <= 0)
		return 0;
	for (int i = pos - 1; i >= 0; i--)
		if (buf[i] == '\n')
			return i + 1;
	return 0;
}

static int find_line_end(int pos)
{
	for (int i = pos; i < buf_len; i++)
		if (buf[i] == '\n')
			return i;
	return buf_len;
}

static int pos_to_line(int pos)
{
	int line = 0;
	for (int i = 0; i < pos && i < buf_len; i++)
		if (buf[i] == '\n')
			line++;
	return line;
}

static int line_to_start(int num)
{
	if (num <= 0)
		return 0;
	int count = 0;
	for (int i = 0; i < buf_len; i++) {
		if (buf[i] == '\n') {
			count++;
			if (count == num)
				return i + 1;
		}
	}
	return buf_len;
}

static void do_insert(char c)
{
	if (buf_len >= BUF_SIZE)
		return;
	for (int i = buf_len; i > cursor_pos; i--)
		buf[i] = buf[i - 1];
	buf[cursor_pos] = c;
	buf_len++;
	cursor_pos++;
	modified = true;
}

static void do_delete(void)
{
	if (cursor_pos >= buf_len)
		return;
	for (int i = cursor_pos; i < buf_len - 1; i++)
		buf[i] = buf[i + 1];
	buf_len--;
	modified = true;
}

static void do_backspace(void)
{
	if (cursor_pos <= 0)
		return;
	for (int i = cursor_pos - 1; i < buf_len - 1; i++)
		buf[i] = buf[i + 1];
	buf_len--;
	cursor_pos--;
	modified = true;
}

static void adjust_scroll(void)
{
	int cl = pos_to_line(cursor_pos);
	if (cl < scroll_y)
		scroll_y = cl;
	else if (cl >= scroll_y + CONTENT_ROWS)
		scroll_y = cl - CONTENT_ROWS + 1;
}

static void draw(void)
{
	ui_clear();

	char sl[64];
	char sr[32];
	const char *fname = g_filename ? g_filename : "(new)";

	if (modified)
		kstrcpy(sl, " * ", sizeof(sl));
	else
		kstrcpy(sl, "   ", sizeof(sl));
	kstrcat(sl, fname, sizeof(sl));

	int cl = pos_to_line(cursor_pos);
	int cc = cursor_pos - find_line_start(cursor_pos);

	kitoa(cl + 1, sr, sizeof(sr));
	kstrcat(sr, ":", sizeof(sr));
	char tmp[16];
	kitoa(cc + 1, tmp, sizeof(tmp));
	kstrcat(sr, tmp, sizeof(sr));

	ui_status_bar(0, sl, sr, COLOR_TITLE);

	int total = count_lines();
	for (int row = 0; row < CONTENT_ROWS; row++) {
		int ln = scroll_y + row;
		ui_fill(0, row + 1, SCREEN_W, 1, ' ', COLOR_NORMAL);

		if (ln < total) {
			char nb[8];
			kitoa(ln + 1, nb, sizeof(nb));
			int nlen = kstrlen(nb);
			int pad = LINE_NUM_W - 1 - nlen;
			for (int p = 0; p < pad; p++)
				ui_put(p, row + 1, ' ', COLOR_DIM);
			ui_put_str(pad > 0 ? pad : 0, row + 1, nb, COLOR_DIM);
			ui_put(LINE_NUM_W - 1, row + 1, ' ', COLOR_DIM);

			int ls = line_to_start(ln);
			int le = find_line_end(ls);
			int cw = SCREEN_W - LINE_NUM_W;
			for (int col = 0; col < cw; col++) {
				int bp = ls + col;
				unsigned char attr = COLOR_NORMAL;
				if (bp == cursor_pos)
					attr = COLOR_HIGHLIGHT;
				if (bp < le) {
					char ch = buf[bp];
					if (ch == '\t')
						ch = ' ';
					ui_put(LINE_NUM_W + col, row + 1, ch, attr);
				} else {
					ui_put(LINE_NUM_W + col, row + 1, ' ', attr);
				}
			}
		} else {
			ui_put(0, row + 1, '~', COLOR_CYAN);
		}
	}

	ui_fill(0, SCREEN_H - 1, SCREEN_W, 1, ' ', COLOR_STATUS);
	ui_put_str(1, SCREEN_H - 1, "^S:Save ^X:Exit  arrows:Nav",
		   COLOR_STATUS);
}

void app_nano(const char *filename)
{
	if (!filename || filename[0] == '\0') {
		console_print("Usage: nano <filename>\n");
		return;
	}

	g_filename = filename;
	buf_len = 0;
	cursor_pos = 0;
	scroll_y = 0;
	modified = false;
	kmemset(buf, 0, BUF_SIZE);

	if (vfs_exists(filename)) {
		const void *data = vfs_read(filename);
		size_t sz = vfs_get_size(filename);
		if (data && sz > 0) {
			if (sz > BUF_SIZE - 1)
				sz = BUF_SIZE - 1;
			kmemcpy(buf, data, sz);
			buf_len = (int)sz;
		}
	}

	adjust_scroll();

	while (1) {
		draw();
		int key = ui_read_key();

		if (key == 24) {
			if (modified) {
				ui_fill(0, SCREEN_H - 1, SCREEN_W, 1, ' ',
					COLOR_ERROR);
				ui_put_str(1, SCREEN_H - 1,
					   "Modified! Ctrl+X again to exit",
					   COLOR_ERROR);
				int key2 = ui_read_key();
				if (key2 != 24)
					continue;
			}
			break;
		}

		if (key == 19) {
			if (vfs_write(g_filename, buf, (size_t)buf_len))
				modified = false;
			continue;
		}

		if (key == KEY_UP) {
			int cl = pos_to_line(cursor_pos);
			if (cl > 0) {
				int cc = cursor_pos -
					 find_line_start(cursor_pos);
				int nls = line_to_start(cl - 1);
				int nle = find_line_end(nls);
				int np = nls + cc;
				if (np > nle)
					np = nle;
				cursor_pos = np;
			}
		} else if (key == KEY_DOWN) {
			int cl = pos_to_line(cursor_pos);
			int total = count_lines();
			if (cl < total - 1) {
				int cc = cursor_pos -
					 find_line_start(cursor_pos);
				int nls = line_to_start(cl + 1);
				int nle = find_line_end(nls);
				int np = nls + cc;
				if (np > nle)
					np = nle;
				cursor_pos = np;
			}
		} else if (key == KEY_LEFT) {
			if (cursor_pos > 0)
				cursor_pos--;
		} else if (key == KEY_RIGHT) {
			if (cursor_pos < buf_len)
				cursor_pos++;
		} else if (key == KEY_HOME) {
			cursor_pos = find_line_start(cursor_pos);
		} else if (key == KEY_END) {
			cursor_pos = find_line_end(cursor_pos);
		} else if (key == KEY_PAGEUP) {
			cursor_pos = line_to_start(
				pos_to_line(cursor_pos) - CONTENT_ROWS);
		} else if (key == KEY_PAGEDOWN) {
			int cl = pos_to_line(cursor_pos) + CONTENT_ROWS;
			int total = count_lines();
			if (cl >= total)
				cl = total - 1;
			cursor_pos = line_to_start(cl);
		} else if (key == KEY_DELETE) {
			do_delete();
		} else if (key == '\b') {
			do_backspace();
		} else if (key == '\n') {
			do_insert('\n');
		} else if (key == '\t') {
			for (int t = 0; t < 4 && buf_len < BUF_SIZE; t++)
				do_insert(' ');
		} else if (key >= 32 && key <= 126) {
			do_insert((char)key);
		}

		adjust_scroll();
	}

	ui_clear();
}
