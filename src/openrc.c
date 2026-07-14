#include "openrc.h"
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

struct rc_unit {
	char name[MAX_UNIT_NAME];
	char description[MAX_UNIT_DESC];
	char exec_start[MAX_UNIT_CMD];
	char wanted_by[MAX_UNIT_WANTED];
	bool active;
};

typedef struct rc_unit rc_unit_t;

static rc_unit_t units[MAX_UNITS];
static size_t unit_count;

static const char *rc_trim(const char *s)
{
	while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
		++s;
	return s;
}

static void rc_strncpy(char *dest, const char *src, size_t max_len)
{
	size_t i = 0;
	while (i + 1 < max_len && src[i]) {
		dest[i] = src[i];
		++i;
	}
	dest[i] = '\0';
}

static bool rc_starts_with(const char *s, const char *prefix)
{
	while (*prefix) {
		if (*s != *prefix)
			return false;
		++s;
		++prefix;
	}
	return true;
}

static void rc_print(const char *s)
{
	console_print(s);
}

static void rc_print_unit(const rc_unit_t *u)
{
	rc_print(u->name);
	rc_print(" [");
	rc_print(u->active ? "started" : "stopped");
	rc_print("]");
}

static const char *rc_get_basename(const char *path)
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

static void rc_parse_unit(const struct fs_file *file)
{
	if (!file || unit_count >= MAX_UNITS)
		return;
	const char *name = rc_get_basename(file->name);
	const char *ext = name;
	while (*ext) {
		if (*ext == '.')
			break;
		++ext;
	}
	if (!rc_starts_with(ext, ".service"))
		return;

	rc_unit_t *unit = &units[unit_count];
	unit->name[0] = '\0';
	unit->description[0] = '\0';
	unit->exec_start[0] = '\0';
	unit->wanted_by[0] = '\0';
	unit->active = false;

	rc_strncpy(unit->name, name, MAX_UNIT_NAME);
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
		if (rc_starts_with(text, "Description=")) {
			const char *value = rc_trim(text + 12);
			rc_strncpy(unit->description, value, MAX_UNIT_DESC);
		} else if (rc_starts_with(text, "ExecStart=")) {
			const char *value = rc_trim(text + 10);
			rc_strncpy(unit->exec_start, value, MAX_UNIT_CMD);
		} else if (rc_starts_with(text, "WantedBy=")) {
			const char *value = rc_trim(text + 9);
			rc_strncpy(unit->wanted_by, value, MAX_UNIT_WANTED);
		}
	}
	unit_count += 1;
}

static rc_unit_t *rc_find_unit(const char *name)
{
	size_t name_len = kstrlen(name);
	for (size_t i = 0; i < unit_count; ++i) {
		if (kstrncmp(units[i].name, name, name_len) == 0 &&
		    units[i].name[name_len] == '\0')
			return &units[i];
	}
	return 0;
}

static void rc_start_unit(rc_unit_t *unit)
{
	if (!unit)
		return;
	if (unit->active) {
		rc_print(" * ");
		rc_print(unit->name);
		rc_print(" already started\n");
		return;
	}
	unit->active = true;
	rc_print(" * ");
	rc_print(unit->name);
	rc_print(": starting...\n");
	if (unit->exec_start[0]) {
		rc_print(" * ");
		rc_print(unit->name);
		rc_print(": ");
		rc_print(unit->exec_start);
		rc_print("\n");
	}
	rc_print(" * ");
	rc_print(unit->name);
	rc_print(": started\n");
}

static void rc_stop_unit(rc_unit_t *unit)
{
	if (!unit)
		return;
	if (!unit->active) {
		rc_print(" * ");
		rc_print(unit->name);
		rc_print(": not started\n");
		return;
	}
	unit->active = false;
	rc_print(" * ");
	rc_print(unit->name);
	rc_print(": stopping...\n");
	rc_print(" * ");
	rc_print(unit->name);
	rc_print(": stopped\n");
}

static void rc_start_default_target(void)
{
	for (size_t i = 0; i < unit_count; ++i) {
		if (kstrncmp(units[i].wanted_by, "default.target", 14) == 0)
			rc_start_unit(&units[i]);
	}
}

bool openrc_init(void)
{
	unit_count = 0;
	size_t count = fs_file_count();
	for (size_t i = 0; i < count; ++i) {
		const struct fs_file *file = fs_file_at(i);
		if (file && rc_starts_with(file->name, "openrc/"))
			rc_parse_unit(file);
	}
	rc_start_default_target();
	return true;
}

static void rc_print_help(void)
{
	rc_print("OpenRC commands:\n");
	rc_print("  openrc list\n");
	rc_print("  openrc status <service>\n");
	rc_print("  openrc start <service>\n");
	rc_print("  openrc stop <service>\n");
	rc_print("  openrc restart <service>\n");
}

static void rc_parse_args(const char *args, char *cmd, char *target)
{
	const char *p = rc_trim(args);
	size_t pos = 0;
	while (*p && *p != ' ' && pos + 1 < 32) {
		cmd[pos++] = *p++;
	}
	cmd[pos] = '\0';
	p = rc_trim(p);
	pos = 0;
	while (*p && pos + 1 < MAX_UNIT_NAME)
		target[pos++] = *p++;
	target[pos] = '\0';
}

int openrc_handle_command(const char *args)
{
	char command[32];
	char target[MAX_UNIT_NAME];
	rc_parse_args(args, command, target);
	if (command[0] == '\0' || kstreq(command, "help")) {
		rc_print_help();
		return 0;
	}
	if (kstreq(command, "list")) {
		for (size_t i = 0; i < unit_count; ++i) {
			rc_print_unit(&units[i]);
			rc_print("\n");
		}
		return 0;
	}
	if (kstreq(command, "status")) {
		if (target[0] == '\0') {
			rc_print("status requires a service name\n");
			return 0;
		}
		rc_unit_t *unit = rc_find_unit(target);
		if (!unit) {
			rc_print("Service not found\n");
			return 0;
		}
		rc_print_unit(unit);
		rc_print("\n");
		rc_print("  Description: ");
		rc_print(unit->description[0] ? unit->description : "(none)");
		rc_print("\n");
		rc_print("  ExecStart: ");
		rc_print(unit->exec_start[0] ? unit->exec_start : "(none)");
		rc_print("\n");
		return 0;
	}
	if (kstreq(command, "start")) {
		if (target[0] == '\0') {
			rc_print("start requires a service name\n");
			return 0;
		}
		rc_unit_t *unit = rc_find_unit(target);
		if (!unit) {
			rc_print("Service not found\n");
			return 0;
		}
		rc_start_unit(unit);
		return 0;
	}
	if (kstreq(command, "stop")) {
		if (target[0] == '\0') {
			rc_print("stop requires a service name\n");
			return 0;
		}
		rc_unit_t *unit = rc_find_unit(target);
		if (!unit) {
			rc_print("Service not found\n");
			return 0;
		}
		rc_stop_unit(unit);
		return 0;
	}
	if (kstreq(command, "restart")) {
		if (target[0] == '\0') {
			rc_print("restart requires a service name\n");
			return 0;
		}
		rc_unit_t *unit = rc_find_unit(target);
		if (!unit) {
			rc_print("Service not found\n");
			return 0;
		}
		rc_stop_unit(unit);
		rc_start_unit(unit);
		return 0;
	}
	rc_print("Unknown openrc command. Type 'openrc help' for commands.\n");
	return 0;
}
