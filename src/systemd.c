#include "systemd.h"
#include "console.h"
#include "fs.h"
#include "kstring.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define MAX_UNITS 16
#define MAX_UNIT_NAME 32
#define MAX_UNIT_DESC 64
#define MAX_UNIT_CMD 128
#define MAX_UNIT_WANTED 64

struct sd_unit {
	char name[MAX_UNIT_NAME];
	char description[MAX_UNIT_DESC];
	char exec_start[MAX_UNIT_CMD];
	char wanted_by[MAX_UNIT_WANTED];
	bool active;
};

typedef struct sd_unit sd_unit_t;

static sd_unit_t units[MAX_UNITS];
static size_t unit_count;

static const char *sd_trim(const char *s)
{
	while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
		++s;
	return s;
}

static void sd_strncpy(char *dest, const char *src, size_t max_len)
{
	size_t i = 0;
	while (i + 1 < max_len && src[i]) {
		dest[i] = src[i];
		++i;
	}
	dest[i] = '\0';
}

static bool sd_starts_with(const char *s, const char *prefix)
{
	while (*prefix) {
		if (*s != *prefix)
			return false;
		++s;
		++prefix;
	}
	return true;
}

static void sd_print(const char *s)
{
	console_print(s);
}

static void sd_print_unit(const sd_unit_t *u)
{
	sd_print(u->name);
	sd_print(" [");
	sd_print(u->active ? "active" : "inactive");
	sd_print("]");
}

static const char *sd_get_basename(const char *path)
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

static void sd_parse_unit(const struct fs_file *file)
{
	if (!file || unit_count >= MAX_UNITS)
		return;
	const char *name = sd_get_basename(file->name);
	const char *ext = name;
	while (*ext) {
		if (*ext == '.')
			break;
		++ext;
	}
	if (!sd_starts_with(ext, ".service"))
		return;

	sd_unit_t *unit = &units[unit_count];
	unit->name[0] = '\0';
	unit->description[0] = '\0';
	unit->exec_start[0] = '\0';
	unit->wanted_by[0] = '\0';
	unit->active = false;

	sd_strncpy(unit->name, name, MAX_UNIT_NAME);
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
		if (sd_starts_with(text, "Description=")) {
			const char *value = sd_trim(text + 12);
			sd_strncpy(unit->description, value, MAX_UNIT_DESC);
		} else if (sd_starts_with(text, "ExecStart=")) {
			const char *value = sd_trim(text + 10);
			sd_strncpy(unit->exec_start, value, MAX_UNIT_CMD);
		} else if (sd_starts_with(text, "WantedBy=")) {
			const char *value = sd_trim(text + 9);
			sd_strncpy(unit->wanted_by, value, MAX_UNIT_WANTED);
		}
	}
	unit_count += 1;
}

static sd_unit_t *sd_find_unit(const char *name)
{
	size_t name_len = kstrlen(name);
	for (size_t i = 0; i < unit_count; ++i) {
		if (kstrncmp(units[i].name, name, name_len) == 0 &&
		    units[i].name[name_len] == '\0')
			return &units[i];
	}
	return 0;
}

static void sd_start_unit(sd_unit_t *unit)
{
	if (!unit)
		return;
	if (unit->active) {
		sd_print("Unit already active.");
		return;
	}
	unit->active = true;
	sd_print("Starting ");
	sd_print(unit->name);
	sd_print("\n");
	if (unit->exec_start[0]) {
		sd_print("ExecStart: ");
		sd_print(unit->exec_start);
		sd_print("\n");
	}
}

static void sd_stop_unit(sd_unit_t *unit)
{
	if (!unit)
		return;
	if (!unit->active) {
		sd_print("Unit not active.");
		return;
	}
	unit->active = false;
	sd_print("Stopped ");
	sd_print(unit->name);
	sd_print("\n");
}

static void sd_start_default_target(void)
{
	for (size_t i = 0; i < unit_count; ++i) {
		if (kstrncmp(units[i].wanted_by, "default.target", 14) == 0)
			sd_start_unit(&units[i]);
	}
}

bool systemd_init(void)
{
	unit_count = 0;
	size_t count = fs_file_count();
	for (size_t i = 0; i < count; ++i) {
		const struct fs_file *file = fs_file_at(i);
		if (file && sd_starts_with(file->name, "systemd/"))
			sd_parse_unit(file);
	}
	sd_start_default_target();
	return true;
}

static void sd_print_help(void)
{
	sd_print("systemctl commands:\n");
	sd_print("  systemctl list-units\n");
	sd_print("  systemctl status <unit>\n");
	sd_print("  systemctl start <unit>\n");
	sd_print("  systemctl stop <unit>\n");
}

static void sd_parse_args(const char *args, char *cmd, char *target)
{
	const char *p = sd_trim(args);
	size_t pos = 0;
	while (*p && *p != ' ' && pos + 1 < 32) {
		cmd[pos++] = *p++;
	}
	cmd[pos] = '\0';
	p = sd_trim(p);
	pos = 0;
	while (*p && pos + 1 < MAX_UNIT_NAME)
		target[pos++] = *p++;
	target[pos] = '\0';
}

int systemd_handle_command(const char *args)
{
	char command[32];
	char target[MAX_UNIT_NAME];
	sd_parse_args(args, command, target);
	if (command[0] == '\0' || kstreq(command, "help")) {
		sd_print_help();
		return 0;
	}
	if (kstreq(command, "list-units")) {
		for (size_t i = 0; i < unit_count; ++i) {
			sd_print_unit(&units[i]);
			sd_print("\n");
		}
		return 0;
	}
	if (kstreq(command, "status")) {
		if (target[0] == '\0') {
			sd_print("status requires a unit name\n");
			return 0;
		}
		sd_unit_t *unit = sd_find_unit(target);
		if (!unit) {
			sd_print("Unit not found\n");
			return 0;
		}
		sd_print_unit(unit);
		sd_print("\n");
		sd_print("Description: ");
		sd_print(unit->description[0] ? unit->description : "(none)");
		sd_print("\n");
		sd_print("ExecStart: ");
		sd_print(unit->exec_start[0] ? unit->exec_start : "(none)");
		sd_print("\n");
		return 0;
	}
	if (kstreq(command, "start")) {
		if (target[0] == '\0') {
			sd_print("start requires a unit name\n");
			return 0;
		}
		sd_unit_t *unit = sd_find_unit(target);
		if (!unit) {
			sd_print("Unit not found\n");
			return 0;
		}
		sd_start_unit(unit);
		return 0;
	}
	if (kstreq(command, "stop")) {
		if (target[0] == '\0') {
			sd_print("stop requires a unit name\n");
			return 0;
		}
		sd_unit_t *unit = sd_find_unit(target);
		if (!unit) {
			sd_print("Unit not found\n");
			return 0;
		}
		sd_stop_unit(unit);
		return 0;
	}
	sd_print("Unknown systemctl command\n");
	return 0;
}
