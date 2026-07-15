#include "apps.h"
#include "console.h"
#include "keyboard.h"
#include "timer.h"
#include "pci.h"
#include "kstring.h"
#include "memory.h"
#include <stdbool.h>
#include <stdint.h>

extern uint32_t sys_mem_lower;
extern uint32_t sys_mem_upper;

static int pci_scroll;

static void hex16(unsigned short val, char *buf, size_t len)
{
	char tmp[8];
	kxtoa(val, tmp, sizeof(tmp));
	int pad = 4 - kstrlen(tmp);
	int i = 0;
	while (pad-- > 0 && i + 1 < (int)len)
		buf[i++] = '0';
	kstrcpy(buf + i, tmp, len - i);
}

static void hex8(unsigned char val, char *buf, size_t len)
{
	char tmp[4];
	kxtoa(val, tmp, sizeof(tmp));
	if (kstrlen(tmp) < 2) {
		buf[0] = '0';
		kstrcpy(buf + 1, tmp, len - 1);
	} else {
		kstrcpy(buf, tmp, len);
	}
}

static void format_memory(char *buf, size_t len, unsigned long kb)
{
	unsigned long mb = kb / 1024;
	if (mb >= 1024) {
		unsigned long whole = mb / 1024;
		unsigned long frac = (mb % 1024) * 10 / 1024;
		buf[0] = '0' + whole;
		buf[1] = '.';
		buf[2] = '0' + frac;
		buf[3] = ' ';
		buf[4] = 'G';
		buf[5] = 'B';
		buf[6] = '\0';
	} else {
		kutoa(mb, buf, len);
		kstrcat(buf, " MB", len);
	}
}

static void format_uptime(char *buf, unsigned long seconds)
{
	int h = seconds / 3600;
	int m = (seconds % 3600) / 60;
	int s = seconds % 60;
	buf[0] = '0' + h / 10;
	buf[1] = '0' + h % 10;
	buf[2] = ':';
	buf[3] = '0' + m / 10;
	buf[4] = '0' + m % 10;
	buf[5] = ':';
	buf[6] = '0' + s / 10;
	buf[7] = '0' + s % 10;
	buf[8] = '\0';
}

static void draw_panel_title(int x, int y, int w, const char *title)
{
	ui_double_box(x, y, w, 1, COLOR_CYAN);
	ui_title_bar(x, y, w, title, COLOR_CYAN);
}

static void draw_panel(int x, int y, int w, int h)
{
	ui_box(x, y, w, h, COLOR_BORDER);
	ui_fill(x + 1, y + 1, w - 2, h - 2, ' ', COLOR_NORMAL);
}

static void draw_label_value(int x, int y, const char *label, const char *value)
{
	ui_put_str(x, y, label, COLOR_CYAN);
	ui_put_str(x + kstrlen(label), y, value, COLOR_WHITE);
}

static const char *pci_class_name(uint8_t cc)
{
	switch (cc) {
	case 0x00: return "Legacy Device";
	case 0x01: return "Mass Storage";
	case 0x02: return "Network";
	case 0x03: return "Display";
	case 0x04: return "Multimedia";
	case 0x05: return "Memory";
	case 0x06: return "Bridge";
	case 0x07: return "Comm";
	case 0x08: return "System";
	case 0x09: return "Input";
	case 0x0C: return "Serial Bus";
	default:   return "Other";
	}
}

static void draw_system_panel(void)
{
	draw_panel(1, 1, 38, 5);
	draw_panel_title(1, 1, 38, " System ");
	draw_label_value(3, 2, "OS Name:  ", "NucleOS");
	draw_label_value(3, 3, "Kernel:   ", "3.0 (i386)");
	draw_label_value(3, 4, "Arch:     ", "x86_64");
}

static void draw_memory_panel(void)
{
	char buf[32];
	char kb_buf[32];
	unsigned long total_kb = sys_mem_lower + sys_mem_upper;

	draw_panel(41, 1, 38, 5);
	draw_panel_title(41, 1, 38, " Memory ");

	format_memory(buf, sizeof(buf), total_kb);
	draw_label_value(43, 2, "Total RAM: ", buf);
	draw_label_value(43, 3, "Heap:      ", "N/A");

	kutoa(sys_mem_lower, kb_buf, sizeof(kb_buf));
	kstrcat(kb_buf, " KB", sizeof(kb_buf));
	draw_label_value(43, 4, "Lower:     ", kb_buf);
}

static void draw_uptime_panel(void)
{
	char buf[32];
	unsigned long secs = timer_get_ticks() / 100;

	draw_panel(1, 6, 38, 4);
	draw_panel_title(1, 6, 38, " Uptime ");

	format_uptime(buf, secs);
	draw_label_value(3, 7, "Elapsed:   ", buf);

	kutoa(secs, buf, sizeof(buf));
	kstrcat(buf, "s", sizeof(buf));
	draw_label_value(3, 8, "Raw ticks: ", buf);
}

static void draw_keyboard_panel(void)
{
	int lay = keyboard_get_layout();
	const char *name;

	if (lay == KB_LAYOUT_ES)
		name = "Spanish (ES)";
	else if (lay == KB_LAYOUT_DE)
		name = "German (DE)";
	else
		name = "English (US)";

	draw_panel(41, 6, 38, 4);
	draw_panel_title(41, 6, 38, " Keyboard ");
	draw_label_value(43, 7, "Layout:    ", name);
}

static void draw_pci_panel(void)
{
	int count = pci_get_device_count();
	int max_visible = 11;

	draw_panel(1, 10, 78, 13);
	draw_panel_title(1, 10, 78, " PCI Devices ");

	if (count == 0) {
		ui_put_str(3, 11, "No PCI devices detected.", COLOR_DIM);
		return;
	}

	if (pci_scroll > count - max_visible)
		pci_scroll = count - max_visible;
	if (pci_scroll < 0)
		pci_scroll = 0;

	for (int i = 0; i < max_visible && (i + pci_scroll) < count; i++) {
		const struct pci_device *dev = pci_get_device(i + pci_scroll);
		if (!dev) continue;

		char line[64];
		char hv[8];

		hex16(dev->vendor_id, hv, sizeof(hv));
		kstrcpy(line, hv, sizeof(line));
		kstrcat(line, ":", sizeof(line));
		hex16(dev->device_id, hv, sizeof(hv));
		kstrcat(line, hv, sizeof(line));
		kstrcat(line, "  ", sizeof(line));
		kstrcat(line, pci_class_name(dev->class_code), sizeof(line));
		kstrcat(line, "  ", sizeof(line));
		hex8(dev->class_code, hv, sizeof(hv));
		kstrcat(line, hv, sizeof(line));
		kstrcat(line, ".", sizeof(line));
		hex8(dev->subclass, hv, sizeof(hv));
		kstrcat(line, hv, sizeof(line));

		ui_put_str(3, 11 + i, line, COLOR_NORMAL);
	}

	if (count > max_visible) {
		char info[32];
		char tmp[8];
		int end = pci_scroll + max_visible;
		if (end > count) end = count;

		kitoa(pci_scroll + 1, info, sizeof(info));
		kstrcat(info, "-", sizeof(info));
		kitoa(end, tmp, sizeof(tmp));
		kstrcat(info, tmp, sizeof(info));
		kstrcat(info, "/", sizeof(info));
		kitoa(count, tmp, sizeof(tmp));
		kstrcat(info, tmp, sizeof(info));
		ui_put_str(62, 22, info, COLOR_DIM);
		ui_put_str(3, 22, "\x18\x19 Scroll", COLOR_DIM);
	} else {
		char info[32];
		kitoa(count, info, sizeof(info));
		kstrcat(info, " device(s)", sizeof(info));
		ui_put_str(62, 22, info, COLOR_DIM);
	}
}

static void draw_all(void)
{
	ui_clear();
	ui_double_box(0, 0, SCREEN_W, SCREEN_H, COLOR_CYAN);
	ui_title_bar(0, 0, SCREEN_W, " NucleOS System Information ", COLOR_TITLE);
	ui_status_bar(23, " NucleOS SysInfo ", "v3.0", COLOR_STATUS);
	ui_fill(0, 24, SCREEN_W, 1, ' ', COLOR_STATUS);
	ui_put_str(SCREEN_W - 12, 24, "Q: Return", COLOR_STATUS);

	draw_system_panel();
	draw_memory_panel();
	draw_uptime_panel();
	draw_keyboard_panel();
	draw_pci_panel();
}

void app_sysinfo(void)
{
	pci_scroll = 0;

	while (1) {
		draw_all();

		int key = ui_read_key_timeout(50);
		if (key == 'q' || key == 'Q' || key == 27) {
			ui_clear();
			return;
		}
		if (key == KEY_UP) {
			if (pci_scroll > 0) pci_scroll--;
		} else if (key == KEY_DOWN) {
			pci_scroll++;
		} else if (key == KEY_PAGEUP) {
			pci_scroll -= 10;
			if (pci_scroll < 0) pci_scroll = 0;
		} else if (key == KEY_PAGEDOWN) {
			pci_scroll += 10;
		}
	}
}
