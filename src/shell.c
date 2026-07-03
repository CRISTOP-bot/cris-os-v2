#include "shell.h"
#include "console.h"
#include "keyboard.h"
#include "vfs.h"
#include "lcp.h"
#include "systemd.h"
#include "boot.h"
#include "gui.h"
#include "asm.h"
#include "calc_app.h"
#include "mouse.h"
#include "kstring.h"
#include <stdbool.h>

static const char *parse_token(const char *s, char *out, int maxlen)
{
	s = kskip_spaces(s);
	int pos = 0;
	while (*s && *s != ' ' && *s != '\t' && pos + 1 < maxlen)
		out[pos++] = *s++;
	out[pos] = '\0';
	return s;
}

static void copy_rest(const char *s, char *out, int maxlen)
{
	s = kskip_spaces(s);
	int pos = 0;
	while (*s && pos + 1 < maxlen)
		out[pos++] = *s++;
	out[pos] = '\0';
}

static int parse_int(const char *s)
{
	int value = 0;
	int sign = 1;
	if (*s == '-') {
		sign = -1;
		++s;
	}
	while (*s >= '0' && *s <= '9') {
		value = value * 10 + (*s - '0');
		++s;
	}
	return value * sign;
}

static bool grep_file(const char *path, const char *pattern)
{
	const void *data = vfs_read(path);
	size_t size = vfs_get_size(path);
	if (!data || size == 0)
		return false;
	const char *text = (const char *)data;
	int line = 1;
	const char *p = text;
	while ((size_t)(p - text) < size) {
		const char *line_start = p;
		while ((size_t)(p - text) < size && *p != '\n')
			++p;
		int line_len = (int)(p - line_start);
		size_t pattern_len = kstrlen(pattern);
		for (int i = 0; i + (int)pattern_len <= line_len; ++i) {
			bool ok = true;
			for (size_t j = 0; j < pattern_len; ++j) {
				if (line_start[i + j] != pattern[j]) {
					ok = false;
					break;
				}
			}
			if (ok) {
				char num[16];
				kitoa(line, num, sizeof(num));
				console_print(num);
				console_print(": ");
				for (int k = 0; k < line_len; ++k)
					console_putchar(line_start[k]);
				console_print("\n");
				break;
			}
		}
		if ((size_t)(p - text) < size && *p == '\n')
			++p;
		++line;
	}
	return true;
}

static void handle_asm_command(const char *args)
{
	char op[16];
	char first[32];
	char second[32];
	const char *p = args;
	p = parse_token(p, op, sizeof(op));
	p = parse_token(p, first, sizeof(first));
	p = parse_token(p, second, sizeof(second));
	if (op[0] == '\0' || first[0] == '\0' || second[0] == '\0') {
		console_print("Usage: asm add|sub|mul|div <a> <b>\n");
		return;
	}
	int a = parse_int(first);
	int b = parse_int(second);
	int result = 0;
	if (kstreq(op, "add"))
		result = add_asm(a, b);
	else if (kstreq(op, "sub"))
		result = sub_asm(a, b);
	else if (kstreq(op, "mul"))
		result = mul_asm(a, b);
	else if (kstreq(op, "div"))
		result = div_asm(a, b);
	else {
		console_print("Unknown asm op. Use add, sub, mul, div\n");
		return;
	}
	char out[32];
	kitoa(result, out, sizeof(out));
	console_print("= ");
	console_print(out);
	console_print("\n");
}

static void shell_echo(const char *args)
{
	char target[128];
	char value[192];
	const char *p = args;
	int vi = 0;
	while (*p && *p != '>' && vi + 1 < (int)sizeof(value))
		value[vi++] = *p++;
	value[vi] = '\0';
	if (*p == '>') {
		++p;
		if (*p == '>')
			++p;
		p = kskip_spaces(p);
		int ti = 0;
		while (*p && *p != ' ' && ti + 1 < (int)sizeof(target))
			target[ti++] = *p++;
		target[ti] = '\0';
	}
	if (target[0])
		vfs_write(target, value, kstrlen(value));
	else {
		console_print(value);
		console_print("\n");
	}
}

static void show_mouse_state(void)
{
	struct mouse_state ms;
	mouse_get_state(&ms);
	console_print("Mouse: x=");
	char buf[16];
	kitoa(ms.x, buf, sizeof(buf));
	console_print(buf);
	console_print(" y=");
	kitoa(ms.y, buf, sizeof(buf));
	console_print(buf);
	console_print(" buttons=");
	kitoa(ms.buttons, buf, sizeof(buf));
	console_print(buf);
	console_print("\n");
}

static const char *layout_name(int id)
{
	switch (id) {
	case KB_LAYOUT_US: return "us (English)";
	case KB_LAYOUT_ES: return "es (Spanish)";
	case KB_LAYOUT_DE: return "de (German)";
	default:           return "unknown";
	}
}

void shell_run(void)
{
	char buf[256];
	while (1) {
		console_print("> ");
		int n = keyboard_readline(buf, sizeof(buf));
		if (n == 0)
			continue;
		char cmd[32];
		char arg1[128];
		char arg2[128];
		char rest[192];
		const char *p = buf;
		p = parse_token(p, cmd, sizeof(cmd));
		const char *tail = p;
		p = parse_token(p, arg1, sizeof(arg1));
		p = parse_token(p, arg2, sizeof(arg2));
		copy_rest(tail, rest, sizeof(rest));

		if (kstreq(cmd, "help")) {
			console_print("Commands: help clear ls pwd cd mkdir rmdir rm touch\n");
			console_print(" cp mv cat grep echo uname whoami df stat panic\n");
			console_print(" reboot lcp systemctl bootctl gui asm calc\n");
			console_print(" kblayout mouse\n");
			continue;
		}
		if (kstreq(cmd, "clear")) {
			console_clear();
			continue;
		}
		if (kstreq(cmd, "pwd")) {
			console_print(vfs_pwd());
			console_print("\n");
			continue;
		}
		if (kstreq(cmd, "cd")) {
			if (arg1[0] == '\0')
				console_print("Usage: cd <path>\n");
			else if (!vfs_cd(arg1))
				console_print("Directory not found\n");
			continue;
		}
		if (kstreq(cmd, "ls")) {
			const char *path = arg1[0] ? arg1 : 0;
			if (!vfs_list(path))
				console_print("Invalid path\n");
			continue;
		}
		if (kstreq(cmd, "mkdir")) {
			if (arg1[0] == '\0')
				console_print("Usage: mkdir <path>\n");
			else if (!vfs_mkdir(arg1))
				console_print("Cannot create directory\n");
			continue;
		}
		if (kstreq(cmd, "rmdir")) {
			if (arg1[0] == '\0')
				console_print("Usage: rmdir <path>\n");
			else if (!vfs_rmdir(arg1))
				console_print("Cannot remove directory\n");
			continue;
		}
		if (kstreq(cmd, "rm")) {
			if (arg1[0] == '\0')
				console_print("Usage: rm <path>\n");
			else if (!vfs_remove(arg1))
				console_print("Cannot remove file\n");
			continue;
		}
		if (kstreq(cmd, "touch")) {
			if (arg1[0] == '\0')
				console_print("Usage: touch <path>\n");
			else if (!vfs_touch(arg1))
				console_print("Cannot touch file\n");
			continue;
		}
		if (kstreq(cmd, "cp")) {
			if (arg1[0] == '\0' || arg2[0] == '\0')
				console_print("Usage: cp <src> <dst>\n");
			else if (!vfs_cp(arg1, arg2))
				console_print("Copy failed\n");
			continue;
		}
		if (kstreq(cmd, "mv")) {
			if (arg1[0] == '\0' || arg2[0] == '\0')
				console_print("Usage: mv <src> <dst>\n");
			else if (!vfs_mv(arg1, arg2))
				console_print("Move failed\n");
			continue;
		}
		if (kstreq(cmd, "cat")) {
			if (arg1[0] == '\0')
				console_print("Usage: cat <file>\n");
			else if (!vfs_cat(arg1))
				console_print("File not found or is a directory\n");
			continue;
		}
		if (kstreq(cmd, "grep")) {
			if (arg1[0] == '\0' || arg2[0] == '\0')
				console_print("Usage: grep <pattern> <file>\n");
			else if (!grep_file(arg2, arg1))
				console_print("Pattern search failed\n");
			continue;
		}
		if (kstreq(cmd, "echo")) {
			shell_echo(rest);
			continue;
		}
		if (kstreq(cmd, "uname")) {
			console_print("CrisOS i386\n");
			continue;
		}
		if (kstreq(cmd, "whoami")) {
			console_print("cris\n");
			continue;
		}
		if (kstreq(cmd, "df")) {
			console_print("Filesystem  1K-blocks  Used  Avail  Mounted on\n");
			console_print("rootfs      64        8     56     /\n");
			continue;
		}
		if (kstreq(cmd, "stat")) {
			if (arg1[0] == '\0') {
				console_print("Usage: stat <file>\n");
			} else if (!vfs_exists(arg1)) {
				console_print("No such file or directory\n");
			} else {
				size_t size = vfs_get_size(arg1);
				console_print(arg1);
				console_print(" - size: ");
				char num[32];
				kitoa((long)size, num, sizeof(num));
				console_print(num);
				console_print(" bytes\n");
			}
			continue;
		}
		if (kstreq(cmd, "reboot")) {
			console_print("Rebooting...\n");
			reboot_cpu();
		}
		if (kstreq(cmd, "panic")) {
			kernel_panic(rest[0] ? rest : "Kernel panic triggered from shell.");
		}
		if (kstreq(cmd, "lcp")) {
			lcp_handle_command(rest);
			continue;
		}
		if (kstreq(cmd, "systemctl")) {
			systemd_handle_command(rest);
			continue;
		}
		if (kstreq(cmd, "bootctl")) {
			boot_handle_command(rest);
			continue;
		}
		if (kstreq(cmd, "gui")) {
			gui_show();
			continue;
		}
		if (kstreq(cmd, "asm")) {
			handle_asm_command(rest);
			continue;
		}
		if (kstreq(cmd, "calc")) {
			for (int i = 0; rest[i]; ++i)
				if (rest[i] == '=')
					rest[i] = '+';
			calc_app(rest);
			continue;
		}
		if (kstreq(cmd, "mouse")) {
			show_mouse_state();
			continue;
		}
		if (kstreq(cmd, "kblayout")) {
			if (arg1[0] == '\0') {
				int cur = keyboard_get_layout();
				console_print("Current layout: ");
				console_print(layout_name(cur));
				console_print("\n");
				console_print("Usage: kblayout us|es|de\n");
			} else {
				int id;
				if (kstreq(arg1, "us"))
					id = KB_LAYOUT_US;
				else if (kstreq(arg1, "es"))
					id = KB_LAYOUT_ES;
				else if (kstreq(arg1, "de"))
					id = KB_LAYOUT_DE;
				else {
					console_print("Unknown layout. Use us, es, de\n");
					continue;
				}
				if (keyboard_set_layout(id) == 0) {
					console_print("Layout set to ");
					console_print(layout_name(id));
					console_print("\n");
				}
			}
			continue;
		}
		console_print("Unknown command. Type 'help' for a list of commands.\n");
	}
}
