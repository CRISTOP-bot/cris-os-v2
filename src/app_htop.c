#include "apps.h"
#include "console.h"
#include "keyboard.h"
#include "timer.h"
#include "pmm.h"
#include "kstring.h"
#include <stdbool.h>
#include <stdint.h>

extern unsigned long sys_mem_lower;
extern unsigned long sys_mem_upper;

#define BAR_WIDTH 30
#define NUM_TASKS 12
#define BAR_FILLED 0xDB
#define BAR_EMPTY  0xB0

static const char *task_names[NUM_TASKS] = {
	"idle", "timer", "keyboard", "mouse", "shell",
	"vfs", "pmm", "vmm", "pci", "pic", "console", "lcp"
};

static void draw_bar(int x, int y, unsigned long used, unsigned long total)
{
	unsigned char attr_bar = COLOR_GREEN;
	unsigned char attr_empty = COLOR_DIM;
	int filled;

	if (total == 0)
		filled = 0;
	else
		filled = (int)((used * (unsigned long)BAR_WIDTH) / total);
	if (filled > BAR_WIDTH)
		filled = BAR_WIDTH;

	for (int i = 0; i < BAR_WIDTH; i++) {
		if (i < filled)
			ui_put(x + i, y, BAR_FILLED, attr_bar);
		else
			ui_put(x + i, y, BAR_EMPTY, attr_empty);
	}
}

static void draw_number(int x, int y, unsigned long val, unsigned char attr)
{
	char buf[32];
	kutoa(val, buf, sizeof(buf));
	ui_put_str(x, y, buf, attr);
}

void app_htop(void)
{
	char buf[64];

	while (1) {
		unsigned long ticks = timer_get_ticks();
		unsigned long total_pages = pmm_get_total_pages();
		unsigned long used_pages = pmm_get_used_pages();
		unsigned long free_pages = pmm_get_free_pages();
		unsigned long total_kb = sys_mem_lower + sys_mem_upper;
		unsigned long used_kb = (used_pages * 4);
		unsigned long free_kb = (free_pages * 4);
		unsigned long uptime_sec = ticks / 100;

		ui_clear();

		ui_fill(0, 0, SCREEN_W, 1, ' ', COLOR_TITLE);
		ui_title_bar(0, 0, SCREEN_W, " NucleOS System Monitor ", COLOR_TITLE);

		ui_put_str(1, 1, "Memory Total: ", COLOR_CYAN);
		draw_number(15, 1, total_kb, COLOR_WHITE);
		ui_put_str(21, 1, " KB  Pages: ", COLOR_DIM);
		draw_number(33, 1, total_pages, COLOR_WHITE);

		ui_put_str(1, 2, "Used: ", COLOR_CYAN);
		draw_number(7, 2, used_kb, COLOR_GREEN);
		ui_put_str(13, 2, " KB  Free: ", COLOR_DIM);
		draw_number(24, 2, free_kb, COLOR_YELLOW);
		ui_put_str(30, 2, " KB", COLOR_DIM);

		draw_bar(1, 3, used_pages, total_pages);
		ui_put_str(BAR_WIDTH + 3, 3, "[ ", COLOR_DIM);
		draw_number(BAR_WIDTH + 5, 3, (used_pages * 100) / (total_pages ? total_pages : 1), COLOR_GREEN);
		ui_put_str(BAR_WIDTH + 9, 3, "% used ]", COLOR_DIM);

		ui_put_str(1, 5, "Uptime: ", COLOR_CYAN);
		draw_number(9, 5, uptime_sec / 3600, COLOR_WHITE);
		ui_put(11, 5, 'h', COLOR_WHITE);
		draw_number(13, 5, (uptime_sec % 3600) / 60, COLOR_WHITE);
		ui_put(15, 5, 'm', COLOR_WHITE);
		draw_number(17, 5, uptime_sec % 60, COLOR_WHITE);
		ui_put(19, 5, 's', COLOR_WHITE);

		ui_put_str(30, 5, "Ticks: ", COLOR_CYAN);
		draw_number(37, 5, ticks, COLOR_WHITE);
		ui_put_str(46, 5, "  Rate: ", COLOR_DIM);
		ui_put_str(54, 5, "100 Hz", COLOR_WHITE);

		ui_hline(0, 6, SCREEN_W, COLOR_BORDER);

		ui_put_str(2, 7, "PID", COLOR_TITLE);
		ui_put_str(8, 7, "STATE", COLOR_TITLE);
		ui_put_str(18, 7, "CMD", COLOR_TITLE);

		ui_hline(0, 8, SCREEN_W, COLOR_BORDER);

		for (int i = 0; i < NUM_TASKS; i++) {
			int row = 9 + i;
			unsigned char attr = COLOR_NORMAL;

			if (i == 0)
				attr = COLOR_GREEN;

			kitoa(i + 1, buf, sizeof(buf));
			int len = kstrlen(buf);
			for (int p = 0; p < 3 - len; p++)
				ui_put(2 + p, row, ' ', attr);
			ui_put_str(2 + (3 - len), row, buf, attr);

			ui_put_str(8, row, "running", COLOR_GREEN);

			ui_put_str(18, row, task_names[i], attr);
		}

		ui_hline(0, 21, SCREEN_W, COLOR_BORDER);

		ui_put_str(1, 22, "Tasks: ", COLOR_CYAN);
		draw_number(8, 22, NUM_TASKS, COLOR_WHITE);
		ui_put_str(13, 22, " running: ", COLOR_DIM);
		draw_number(23, 22, NUM_TASKS, COLOR_GREEN);

		ui_status_bar(23, "NucleOS htop", "PID 1-12", COLOR_STATUS);

		ui_status_bar(24, "Q: Return  F5: Refresh", 0, COLOR_BORDER);

		int key = ui_read_key_timeout(25);
		if (key == 'q' || key == 'Q')
			return;
	}
}
