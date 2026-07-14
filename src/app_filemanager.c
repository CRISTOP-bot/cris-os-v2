#include "apps.h"
#include "console.h"
#include "keyboard.h"
#include "kstring.h"
#include "vfs.h"
#include <stdbool.h>

#define FM_MAX_ENTRIES 64
#define FM_VISIBLE     22

static const char *fm_names[FM_MAX_ENTRIES];
static bool fm_isdir[FM_MAX_ENTRIES];
static int fm_count;
static int fm_selected;
static int fm_scroll;

static void fm_build_path(const char *dir, const char *name, char *out, int maxlen)
{
	kstrcpy(out, dir, maxlen);
	if (!kstreq(dir, "/"))
		kstrcat(out, "/", maxlen);
	kstrcat(out, name, maxlen);
}

static void fm_reload(void)
{
	const char *pwd = vfs_pwd();
	fm_count = vfs_get_children(pwd, fm_names, FM_MAX_ENTRIES);
	for (int i = 0; i < fm_count; ++i) {
		char full[128];
		fm_build_path(pwd, fm_names[i], full, sizeof(full));
		fm_isdir[i] = vfs_is_dir(full);
	}
	fm_selected = 0;
	fm_scroll = 0;
}

static void fm_draw(void)
{
	ui_clear();
	const char *pwd = vfs_pwd();
	ui_title_bar(0, 0, SCREEN_W, pwd, COLOR_TITLE);

	int max_scroll = fm_count - FM_VISIBLE;
	if (max_scroll < 0) max_scroll = 0;
	if (fm_scroll > max_scroll) fm_scroll = max_scroll;
	if (fm_scroll < 0) fm_scroll = 0;

	for (int row = 0; row < FM_VISIBLE; ++row) {
		int idx = fm_scroll + row;
		int y = 1 + row;
		unsigned char attr;
		if (idx < fm_count) {
			if (idx == fm_selected)
				attr = COLOR_HIGHLIGHT;
			else if (fm_isdir[idx])
				attr = COLOR_CYAN;
			else
				attr = COLOR_NORMAL;
		} else {
			attr = COLOR_NORMAL;
		}
		ui_fill(0, y, SCREEN_W, 1, ' ', idx == fm_selected ? COLOR_HIGHLIGHT : COLOR_NORMAL);
		if (idx < fm_count) {
			char display[SCREEN_W];
			if (fm_isdir[idx]) {
				display[0] = '[';
				int dpos = 1;
				int nlen = kstrlen(fm_names[idx]);
				for (int c = 0; c < nlen && dpos + 1 < SCREEN_W; ++c)
					display[dpos++] = fm_names[idx][c];
				display[dpos++] = ']';
				display[dpos] = '\0';
			} else {
				kstrcpy(display, fm_names[idx], sizeof(display));
			}
			ui_put_str(2, y, display, attr);
		}
	}

	char count_buf[64];
	char num[16];
	kitoa(fm_count, num, sizeof(num));
	kstrcpy(count_buf, "Items: ", sizeof(count_buf));
	kstrcat(count_buf, num, sizeof(count_buf));
	kstrcat(count_buf, "  Sel: ", sizeof(count_buf));
	kitoa(fm_selected + 1, num, sizeof(num));
	kstrcat(count_buf, num, sizeof(count_buf));
	ui_status_bar(23, count_buf, 0, COLOR_STATUS);
	ui_put_str(0, 24, "Enter:Open BS:Up n:NewFile d:NewDir x:Delete Q:Quit", COLOR_DIM);
}

static void fm_input_line(const char *prompt, char *buf, int maxlen)
{
	ui_fill(0, 24, SCREEN_W, 1, ' ', COLOR_STATUS);
	ui_put_str(1, 24, prompt, COLOR_STATUS);
	int pos = kstrlen(prompt);
	int saved_scroll = fm_scroll;
	int saved_sel = fm_selected;
	fm_scroll = 0;
	fm_selected = -1;
	fm_draw();
	ui_fill(0, 24, SCREEN_W, 1, ' ', COLOR_STATUS);
	ui_put_str(1, 24, prompt, COLOR_STATUS);
	buf[0] = '\0';
	while (1) {
		int key = ui_read_key();
		if (key == '\n')
			break;
		if (key == 27) {
			buf[0] = '\0';
			break;
		}
		if (key == '\b') {
			if (pos > (int)kstrlen(prompt)) {
				--pos;
				buf[pos - kstrlen(prompt)] = '\0';
				ui_put(pos, 24, ' ', COLOR_STATUS);
			}
			continue;
		}
		if (key >= 32 && key < 127) {
			int blen = kstrlen(buf);
			if (blen + 1 < maxlen) {
				buf[blen] = (char)key;
				buf[blen + 1] = '\0';
				ui_put(pos, 24, (char)key, COLOR_STATUS);
				++pos;
			}
		}
	}
	fm_selected = saved_sel;
	fm_scroll = saved_scroll;
}

void app_filemanager(void)
{
	fm_reload();
	while (1) {
		fm_draw();
		int key = ui_read_key();
		if (key == 'q' || key == 'Q')
			break;
		if (key == KEY_UP) {
			if (fm_selected > 0)
				--fm_selected;
			if (fm_selected < fm_scroll)
				fm_scroll = fm_selected;
		} else if (key == KEY_DOWN) {
			if (fm_selected < fm_count - 1)
				++fm_selected;
			if (fm_selected >= fm_scroll + FM_VISIBLE)
				fm_scroll = fm_selected - FM_VISIBLE + 1;
		} else if (key == '\n') {
			if (fm_count > 0 && fm_selected >= 0 && fm_selected < fm_count) {
				if (fm_isdir[fm_selected]) {
					char full[128];
					fm_build_path(vfs_pwd(), fm_names[fm_selected], full, sizeof(full));
					if (vfs_cd(full))
						fm_reload();
				}
			}
		} else if (key == '\b') {
			const char *pwd = vfs_pwd();
			if (!kstreq(pwd, "/")) {
				char parent[128];
				kstrcpy(parent, pwd, sizeof(parent));
				int len = kstrlen(parent);
				if (len > 1 && parent[len - 1] == '/')
					parent[--len] = '\0';
				while (len > 1 && parent[len - 1] != '/')
					parent[--len] = '\0';
				if (len > 1)
					parent[--len] = '\0';
				if (len == 0) {
					parent[0] = '/';
					parent[1] = '\0';
				}
				if (vfs_cd(parent))
					fm_reload();
			}
		} else if (key == 'n') {
			char name_buf[64];
			fm_input_line("New file: ", name_buf, sizeof(name_buf));
			if (name_buf[0]) {
				char full[128];
				fm_build_path(vfs_pwd(), name_buf, full, sizeof(full));
				vfs_touch(full);
				fm_reload();
			}
		} else if (key == 'd') {
			char name_buf[64];
			fm_input_line("New dir: ", name_buf, sizeof(name_buf));
			if (name_buf[0]) {
				char full[128];
				fm_build_path(vfs_pwd(), name_buf, full, sizeof(full));
				vfs_mkdir(full);
				fm_reload();
			}
		} else if (key == 'x') {
			if (fm_count > 0 && fm_selected >= 0 && fm_selected < fm_count) {
				char full[128];
				fm_build_path(vfs_pwd(), fm_names[fm_selected], full, sizeof(full));
				if (fm_isdir[fm_selected])
					vfs_rmdir(full);
				else
					vfs_remove(full);
				fm_reload();
			}
		}
	}
	ui_clear();
}
