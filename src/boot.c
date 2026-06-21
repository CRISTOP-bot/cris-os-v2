#include "boot.h"
#include "console.h"
#include "fs.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define MAX_BOOT_UNITS 16
#define MAX_BOOT_NAME 32
#define MAX_BOOT_DESC 64
#define MAX_BOOT_CMD 128

struct boot_unit {
	char name[MAX_BOOT_NAME];
	char description[MAX_BOOT_DESC];
	char exec_start[MAX_BOOT_CMD];
	bool wants_boot;
	bool active;
};

typedef struct boot_unit boot_unit_t;

static boot_unit_t units[MAX_BOOT_UNITS];
static size_t unit_count;

static const char *bt_trim(const char *s)
{
	while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
		++s;
	return s;
}

static void bt_strncpy(char *dest, const char *src, size_t max_len)
{
	size_t i = 0;
	while (i + 1 < max_len && src[i]) {
		dest[i] = src[i];
		++i;
	}
	dest[i] = '\0';
}

static bool bt_starts_with(const char *s, const char *prefix)
{
	while (*prefix) {
		if (*s != *prefix)
			return false;
		++s;
		++prefix;
	}
	return true;
}

static const char *bt_get_basename(const char *path)
{
	const char *last = path;
	const char *p = path;
	while (*p) {
		if (*p == '/' || *p == '\\')
			last = p + 1;
		++p;
	}
	return last;
}

static void bt_parse_unit(const struct fs_file *file)
{
	if (!file || unit_count >= MAX_BOOT_UNITS)
		return;
	const char *name = bt_get_basename(file->name);
	const char *ext = name;
	while (*ext && *ext != '.')
		++ext;
	if (!bt_starts_with(ext, ".boot"))
		return;

	boot_unit_t *unit = &units[unit_count];
	unit->name[0] = '\0';
	unit->description[0] = '\0';
	unit->exec_start[0] = '\0';
	unit->wants_boot = false;
	unit->active = false;

	bt_strncpy(unit->name, name, MAX_BOOT_NAME);
	if (unit->name[0] == '\0')
		return;

	const char *ptr = (const char *)file->data;
	const char *end = ptr + file->size;
	char line[128];

	while (ptr < end) {
		const char *line_start = ptr;
		while (ptr < end && *ptr != '\n')
			++ptr;
		size_t len = (size_t)(ptr - line_start);
		if (len >= sizeof(line))
			len = sizeof(line) - 1;
		for (size_t i = 0; i < len; ++i)
			line[i] = line_start[i];
		line[len] = '\0';
		if (ptr < end && *ptr == '\n')
			++ptr;

		char *text = line;
		while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n')
			++text;
		size_t start = (size_t)(text - line);
		size_t tlen = len > start ? len - start : 0;
		while (tlen > 0 && (text[tlen - 1] == ' ' || text[tlen - 1] == '\t' ||
		       text[tlen - 1] == '\r' || text[tlen - 1] == '\n'))
			text[--tlen] = '\0';
		if (text[0] == '\0')
			continue;
		if (text[0] == '[')
			continue;
		if (bt_starts_with(text, "Description=")) {
			const char *value = bt_trim(text + 12);
			bt_strncpy(unit->description, value, MAX_BOOT_DESC);
		} else if (bt_starts_with(text, "ExecStart=")) {
			const char *value = bt_trim(text + 10);
			bt_strncpy(unit->exec_start, value, MAX_BOOT_CMD);
		} else if (bt_starts_with(text, "WantedBy=")) {
			const char *value = bt_trim(text + 9);
			if (bt_starts_with(value, "boot.target"))
				unit->wants_boot = true;
		}
	}
	unit_count += 1;
}

static void bt_print(const char *s)
{
	console_print(s);
}

static void bt_print_unit(const boot_unit_t *u)
{
	bt_print(u->name);
	bt_print(" [");
	bt_print(u->active ? "active" : "inactive");
	bt_print("]");
	if (u->description[0]) {
		bt_print(" - ");
		bt_print(u->description);
	}
}

static size_t bt_strlen(const char *s)
{
	size_t len = 0;
	while (s[len])
		++len;
	return len;
}

static int bt_strncmp(const char *a, const char *b, size_t n)
{
	for (size_t i = 0; i < n; ++i) {
		if (a[i] != b[i])
			return (unsigned char)a[i] - (unsigned char)b[i];
		if (!a[i])
			return 0;
	}
	return 0;
}

static bool bt_streq(const char *a, const char *b)
{
	while (*a && *b) {
		if (*a != *b)
			return false;
		++a;
		++b;
	}
	return *a == *b;
}

static boot_unit_t *bt_find_unit(const char *name)
{
	size_t name_len = bt_strlen(name);
	for (size_t i = 0; i < unit_count; ++i) {
		if (bt_strncmp(units[i].name, name, name_len) == 0 &&
		    units[i].name[name_len] == '\0')
			return &units[i];
	}
	return 0;
}

static void bt_start_unit(boot_unit_t *unit)
{
	if (!unit)
		return;
	if (unit->active) {
		bt_print("Unit already active.\n");
		return;
	}
	unit->active = true;
	bt_print("Boot: starting ");
	bt_print(unit->name);
	bt_print("\n");
	if (unit->exec_start[0]) {
		bt_print("  ExecStart: ");
		bt_print(unit->exec_start);
		bt_print("\n");
	}
}

static void bt_stop_unit(boot_unit_t *unit)
{
	if (!unit)
		return;
	if (!unit->active) {
		bt_print("Unit not active.\n");
		return;
	}
	unit->active = false;
	bt_print("Boot: stopped ");
	bt_print(unit->name);
	bt_print("\n");
}

static void bt_start_default_target(void)
{
	bool started = false;
	for (size_t i = 0; i < unit_count; ++i) {
		if (units[i].wants_boot) {
			bt_start_unit(&units[i]);
			started = true;
		}
	}
	if (!started) {
		for (size_t i = 0; i < unit_count; ++i)
			bt_start_unit(&units[i]);
	}
}

bool boot_init(void)
{
	unit_count = 0;
	size_t count = fs_file_count();
	for (size_t i = 0; i < count; ++i) {
		const struct fs_file *file = fs_file_at(i);
		if (file && bt_starts_with(file->name, "boot/"))
			bt_parse_unit(file);
	}
	if (unit_count == 0) {
		bt_print("Boot manager: no boot units found\n");
		return true;
	}
	bt_print("Boot manager: launching boot target\n");
	bt_start_default_target();
	return true;
}

static void bt_print_help(void)
{
	bt_print("bootctl commands:\n");
	bt_print("  bootctl list-units\n");
	bt_print("  bootctl status <unit>\n");
	bt_print("  bootctl start <unit>\n");
	bt_print("  bootctl stop <unit>\n");
}

static void bt_parse_args(const char *args, char *cmd, char *target)
{
	const char *p = bt_trim(args);
	size_t pos = 0;
	while (*p && *p != ' ' && pos + 1 < 32) {
		cmd[pos++] = *p++;
	}
	cmd[pos] = '\0';
	p = bt_trim(p);
	pos = 0;
	while (*p && pos + 1 < MAX_BOOT_NAME)
		target[pos++] = *p++;
	target[pos] = '\0';
}

int boot_handle_command(const char *args)
{
	char command[32];
	char target[MAX_BOOT_NAME];
	bt_parse_args(args, command, target);
	if (command[0] == '\0' || bt_streq(command, "help")) {
		bt_print_help();
		return 0;
	}
	if (bt_streq(command, "list-units")) {
		for (size_t i = 0; i < unit_count; ++i) {
			bt_print_unit(&units[i]);
			bt_print("\n");
		}
		return 0;
	}
	if (bt_streq(command, "status")) {
		if (target[0] == '\0') {
			bt_print("status requires a unit name\n");
			return 0;
		}
		boot_unit_t *unit = bt_find_unit(target);
		if (!unit) {
			bt_print("Unit not found\n");
			return 0;
		}
		bt_print_unit(unit);
		bt_print("\n");
		return 0;
	}
	if (bt_streq(command, "start")) {
		if (target[0] == '\0') {
			bt_print("start requires a unit name\n");
			return 0;
		}
		boot_unit_t *unit = bt_find_unit(target);
		if (!unit) {
			bt_print("Unit not found\n");
			return 0;
		}
		bt_start_unit(unit);
		return 0;
	}
	if (bt_streq(command, "stop")) {
		if (target[0] == '\0') {
			bt_print("stop requires a unit name\n");
			return 0;
		}
		boot_unit_t *unit = bt_find_unit(target);
		if (!unit) {
			bt_print("Unit not found\n");
			return 0;
		}
		bt_stop_unit(unit);
		return 0;
	}
	bt_print("Unknown bootctl command\n");
	return 0;
}
