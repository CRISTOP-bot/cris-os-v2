#include "shell.h"
#include "console.h"
#include "keyboard.h"
#include "vfs.h"
#include "lcp.h"
#include "openrc.h"
#include "boot.h"
#include "gui.h"
#include "asm.h"
#include "calc_app.h"
#include "mouse.h"
#include "kstring.h"
#include "timer.h"
#include "fs.h"
#include "pci.h"
#include "games.h"
#include "pmm.h"
#include "vmm.h"
#include "apps.h"
#include <stdbool.h>

extern unsigned long sys_mem_lower;
extern unsigned long sys_mem_upper;

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
		int digit = *s - '0';
		if (value > 214748364)
			return sign > 0 ? 2147483647 : (-2147483647 - 1);
		if (value == 214748364 && digit > 7)
			return sign > 0 ? 2147483647 : (-2147483647 - 1);
		value = value * 10 + digit;
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
	target[0] = '\0';
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

static void print_prompt(void)
{
	console_print_color("nucleos", VGA_ATTR(VGA_GREEN, VGA_BLACK));
	console_print_color("@", VGA_DEFAULT_ATTR);
	console_print_color("nucleos", VGA_ATTR(VGA_CYAN, VGA_BLACK));
	console_print_color(":", VGA_DEFAULT_ATTR);
	console_print_color(vfs_pwd(), VGA_ATTR(VGA_LIGHT_BLUE, VGA_BLACK));
	console_print_color("$ ", VGA_ATTR(VGA_WHITE, VGA_BLACK));
}

static void print_help(void)
{
	console_print_color("\n-- Filesystem --\n", VGA_ATTR(VGA_YELLOW, VGA_BLACK));
	console_print("  ls [-l|-a]   List directory contents\n");
	console_print("  tree         Display directory tree\n");
	console_print("  pwd          Print working directory\n");
	console_print("  cd           Change directory\n");
	console_print("  mkdir        Create directory\n");
	console_print("  rmdir        Remove directory\n");
	console_print("  touch        Create empty file\n");
	console_print("  rm           Remove file\n");
	console_print("  cp           Copy file\n");
	console_print("  mv           Move/rename file\n");
	console_print("  cat          Display file contents\n");
	console_print("  grep         Search in file\n");
	console_print("  echo         Print text or write to file\n");
	console_print("  stat         Display file information\n");
	console_print("  df           Show filesystem usage\n");
	console_print("  chmod        Change file permissions\n");
	console_print("  hexdump      Hex dump of file contents\n");
	console_print("  wc           Word/line/char count\n");
	console_print("  head         Print first N lines\n");
	console_print("  tail         Print last N lines\n");
	console_print_color("\n-- System --\n", VGA_ATTR(VGA_YELLOW, VGA_BLACK));
	console_print("  uname        Display system information\n");
	console_print("  whoami       Display current user\n");
	console_print("  sudo         Execute as root\n");
	console_print("  meminfo      Display memory information\n");
	console_print("  fastfetch    Show system information\n");
	console_print("  lspci        List PCI devices\n");
	console_print("  clear        Clear screen\n");
	console_print("  reboot       Reboot the system\n");
	console_print("  panic        Trigger kernel panic\n");
	console_print_color("\n-- Services --\n", VGA_ATTR(VGA_YELLOW, VGA_BLACK));
	console_print("  openrc       Service manager (OpenRC)\n");
	console_print("  bootctl      Boot manager\n");
	console_print("  lcp          Package manager\n");
	console_print_color("\n-- Misc --\n", VGA_ATTR(VGA_YELLOW, VGA_BLACK));
	console_print("  asm          Arithmetic via assembly\n");
	console_print("  calc         Expression calculator\n");
	console_print("  games        Game menu (all games)\n");
	console_print("  snake        Launch Snake game\n");
	console_print("  tetris       Launch Tetris game\n");
	console_print("  pong         Launch Pong game\n");
	console_print("  2048         Launch 2048 puzzle game\n");
	console_print("  ttt          Launch Tic-Tac-Toe\n");
	console_print("  minesweeper  Launch Minesweeper\n");
	console_print("  breakout     Launch Breakout\n");
	console_print("  memory       Launch Memory card game\n");
	console_print_color("\n-- Applications --\n", VGA_ATTR(VGA_YELLOW, VGA_BLACK));
	console_print("  nano <file>     Text editor\n");
	console_print("  hexview <file>  Hex viewer\n");
	console_print("  fm              File manager\n");
	console_print("  htop            System monitor\n");
	console_print("  calc-tui        Interactive calculator\n");
	console_print("  kblayout     Change keyboard layout\n");
	console_print("  mouse        Show mouse state\n");
	console_print("  gui          Show GUI (experimental)\n");
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

static void print_cpu_vendor(char *out, int maxlen)
{
	unsigned int res[4];
	out[0] = '\0';
	cpuid(0, res);
	if (res[0] == 0) return;
	unsigned char *vendor = (unsigned char *)&res[1];
	int i;
	for (i = 0; i < 12 && i < maxlen - 1; ++i)
		out[i] = vendor[i];
	out[i] = '\0';
}

static void fastfetch(void)
{
	char buf[32];
	char cpu_vendor[16];

	print_cpu_vendor(cpu_vendor, sizeof(cpu_vendor));

	unsigned char attr_label = VGA_ATTR(VGA_CYAN, VGA_BLACK);
	unsigned char attr_val  = VGA_ATTR(VGA_WHITE, VGA_BLACK);
	unsigned char attr_sep  = VGA_DEFAULT_ATTR;

	console_print_color("\n", attr_sep);
	console_print_color("  ╔══════════════════╗  ", VGA_ATTR(VGA_CYAN, VGA_BLACK));
	console_print_color("OS:      ", attr_label); console_print_color("NucleOS v3 x86_64\n", attr_val);
	console_print_color("  ║   NucleOS v3   ║  ", VGA_ATTR(VGA_CYAN, VGA_BLACK));
	console_print_color("Kernel:  ", attr_label); console_print_color("NucleOS v3\n", attr_val);
	console_print_color("  ╚══════════════════╝  ", VGA_ATTR(VGA_CYAN, VGA_BLACK));
	console_print_color("CPU:     ", attr_label);
	console_print_color(cpu_vendor[0] ? cpu_vendor : "x86_64 (no CPUID)", attr_val);
	console_print("\n");
	console_print_color("                           ", attr_label); console_print_color("Uptime:  ", attr_label);
	unsigned long ticks = timer_get_ticks();
	unsigned long secs = ticks / 100;
	kitoa(secs, buf, sizeof(buf));
	console_print_color(buf, attr_val);
	console_print_color("s\n", attr_val);
	console_print_color("                           ", attr_label); console_print_color("Memory:  ", attr_label);
	if (sys_mem_upper) {
		kitoa((sys_mem_upper + sys_mem_lower) / 1024, buf, sizeof(buf));
		console_print_color(buf, attr_val);
		console_print_color(" MB\n", attr_val);
	} else {
		console_print_color("not detected\n", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
	}
	console_print_color("                           ", attr_label); console_print_color("Shell:   ", attr_label); console_print_color("NucleOS Shell\n", attr_val);
	console_print_color("                           ", attr_label); console_print_color("Term:    ", attr_label); console_print_color("VGA 80x25\n", attr_val);
	console_print_color("                           ", attr_label); console_print_color("User:    ", attr_label); console_print_color("nucleos\n", attr_val);
	console_print_color("                           ", attr_label); console_print_color("Layout:  ", attr_label);
	console_print_color(layout_name(keyboard_get_layout()), attr_val);
	console_print("\n");
	unsigned long nfiles = fs_file_count();
	kitoa(nfiles, buf, sizeof(buf));
	console_print_color("                           ", attr_label); console_print_color("Files:   ", attr_label);
	console_print_color(buf, attr_val);
	console_print_color(" in rootfs\n", attr_val);
	console_print("\n");
}

static void ls_long(const char *path)
{
	const char *target = path && path[0] ? path : vfs_pwd();
	const char *names[32];
	int count = vfs_get_children(target, names, 32);
	if (count == 0) {
		return;
	}
	for (int i = 0; i < count; ++i) {
		char full[128];
		full[0] = '\0';
		kstrcpy(full, target, sizeof(full));
		if (!kstreq(target, "/"))
			kstrcat(full, "/", sizeof(full));
		kstrcat(full, names[i], sizeof(full));

		if (vfs_is_dir(full)) {
			console_print_color("d", VGA_ATTR(VGA_CYAN, VGA_BLACK));
		} else {
			console_print_color("-", VGA_DEFAULT_ATTR);
		}
		console_print("rwxrwxrwx  1 nucleos nucleos  ");

		size_t sz = vfs_get_size(full);
		char num[16];
		kitoa((long)sz, num, sizeof(num));
		int nlen = kstrlen(num);
		for (int p = nlen; p < 8; ++p)
			console_print(" ");
		console_print(num);

		console_print("  ");
		if (vfs_is_dir(full))
			console_print_color(names[i], VGA_ATTR(VGA_CYAN, VGA_BLACK));
		else
			console_print_color(names[i], VGA_ATTR(VGA_WHITE, VGA_BLACK));
		console_print("\n");
	}
}

static void ls_short(const char *path)
{
	const char *target = path && path[0] ? path : vfs_pwd();
	const char *names[32];
	int count = vfs_get_children(target, names, 32);
	for (int i = 0; i < count; ++i) {
		char full[128];
		full[0] = '\0';
		kstrcpy(full, target, sizeof(full));
		if (!kstreq(target, "/"))
			kstrcat(full, "/", sizeof(full));
		kstrcat(full, names[i], sizeof(full));

		if (vfs_is_dir(full))
			console_print_color(names[i], VGA_ATTR(VGA_CYAN, VGA_BLACK));
		else
			console_print_color(names[i], VGA_ATTR(VGA_WHITE, VGA_BLACK));
		console_print("  ");
	}
	if (count > 0)
		console_print("\n");
}

#define TREE_MAX_DEPTH 8

static void tree_print(const char *path, const char *prefix, int is_last, int depth)
{
	if (depth >= TREE_MAX_DEPTH)
		return;
	const char *names[16];
	int count = vfs_get_children(path, names, 16);

	for (int i = 0; i < count; ++i) {
		char full[128];
		full[0] = '\0';
		kstrcpy(full, path, sizeof(full));
		if (!kstreq(path, "/"))
			kstrcat(full, "/", sizeof(full));
		kstrcat(full, names[i], sizeof(full));

		console_print(prefix);
		if (is_last)
			console_print("└── ");
		else
			console_print("├── ");

		if (vfs_is_dir(full))
			console_print_color(names[i], VGA_ATTR(VGA_CYAN, VGA_BLACK));
		else
			console_print_color(names[i], VGA_ATTR(VGA_WHITE, VGA_BLACK));
		console_print("\n");

		if (vfs_is_dir(full)) {
			char new_prefix[128];
			kstrcpy(new_prefix, prefix, sizeof(new_prefix));
			if (is_last)
				kstrcat(new_prefix, "    ", sizeof(new_prefix));
			else
				kstrcat(new_prefix, "│   ", sizeof(new_prefix));
			tree_print(full, new_prefix, i == count - 1, depth + 1);
		}
	}
}

void shell_run(void)
{
	char buf[256];

	console_print_color("╔════════════════════════════════════════════╗\n", VGA_ATTR(VGA_CYAN, VGA_BLACK));
	console_print_color("║        NucleOS v3 Interactive Shell        ║\n", VGA_ATTR(VGA_CYAN, VGA_BLACK));
	console_print_color("║        Type 'help' for commands            ║\n", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
	console_print_color("╚════════════════════════════════════════════╝\n\n", VGA_ATTR(VGA_CYAN, VGA_BLACK));

	while (1) {
		print_prompt();
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
			print_help();
			continue;
		}
		if (kstreq(cmd, "clear")) {
			console_clear();
			continue;
		}
		if (kstreq(cmd, "pwd")) {
			console_print_color(vfs_pwd(), VGA_ATTR(VGA_LIGHT_BLUE, VGA_BLACK));
			console_print("\n");
			continue;
		}
		if (kstreq(cmd, "cd")) {
			if (arg1[0] == '\0')
				console_print("Usage: cd <path>\n");
			else if (!vfs_cd(arg1))
				console_print_color("cd: directory not found\n", VGA_ATTR(VGA_RED, VGA_BLACK));
			continue;
		}
		if (kstreq(cmd, "ls")) {
			bool show_long = false;
			const char *path = 0;
			const char *r = rest;
			char tok[32];
			while (*r) {
				r = parse_token(r, tok, sizeof(tok));
				if (kstreq(tok, "-l"))
					show_long = true;
				else if (tok[0] != '\0')
					path = tok;
			}
			if (show_long)
				ls_long(path);
			else
				ls_short(path);
			continue;
		}
		if (kstreq(cmd, "tree")) {
			const char *target = arg1[0] ? arg1 : "/";
			if (!vfs_is_dir(target)) {
				console_print_color("tree: not a directory\n", VGA_ATTR(VGA_RED, VGA_BLACK));
				continue;
			}
			console_print_color(target, VGA_ATTR(VGA_CYAN, VGA_BLACK));
			console_print("\n");
			tree_print(target, "", 1, 0);
			continue;
		}
		if (kstreq(cmd, "mkdir")) {
			if (arg1[0] == '\0')
				console_print("Usage: mkdir <path>\n");
			else if (!vfs_mkdir(arg1))
				console_print_color("mkdir: cannot create directory\n", VGA_ATTR(VGA_RED, VGA_BLACK));
			continue;
		}
		if (kstreq(cmd, "rmdir")) {
			if (arg1[0] == '\0')
				console_print("Usage: rmdir <path>\n");
			else if (!vfs_rmdir(arg1))
				console_print_color("rmdir: cannot remove directory\n", VGA_ATTR(VGA_RED, VGA_BLACK));
			continue;
		}
		if (kstreq(cmd, "rm")) {
			if (arg1[0] == '\0')
				console_print("Usage: rm <path>\n");
			else if (!vfs_remove(arg1))
				console_print_color("rm: cannot remove file\n", VGA_ATTR(VGA_RED, VGA_BLACK));
			continue;
		}
		if (kstreq(cmd, "touch")) {
			if (arg1[0] == '\0')
				console_print("Usage: touch <path>\n");
			else if (!vfs_touch(arg1))
				console_print_color("touch: cannot create file\n", VGA_ATTR(VGA_RED, VGA_BLACK));
			continue;
		}
		if (kstreq(cmd, "cp")) {
			if (arg1[0] == '\0' || arg2[0] == '\0')
				console_print("Usage: cp <src> <dst>\n");
			else if (!vfs_cp(arg1, arg2))
				console_print_color("cp: copy failed\n", VGA_ATTR(VGA_RED, VGA_BLACK));
			continue;
		}
		if (kstreq(cmd, "mv")) {
			if (arg1[0] == '\0' || arg2[0] == '\0')
				console_print("Usage: mv <src> <dst>\n");
			else if (!vfs_mv(arg1, arg2))
				console_print_color("mv: move failed\n", VGA_ATTR(VGA_RED, VGA_BLACK));
			continue;
		}
		if (kstreq(cmd, "cat")) {
			if (arg1[0] == '\0')
				console_print("Usage: cat <file>\n");
			else if (!vfs_cat(arg1))
				console_print_color("cat: file not found or is a directory\n", VGA_ATTR(VGA_RED, VGA_BLACK));
			continue;
		}
		if (kstreq(cmd, "grep")) {
			if (arg1[0] == '\0' || arg2[0] == '\0')
				console_print("Usage: grep <pattern> <file>\n");
			else if (!grep_file(arg2, arg1))
				console_print_color("grep: pattern not found\n", VGA_ATTR(VGA_RED, VGA_BLACK));
			continue;
		}
		if (kstreq(cmd, "echo")) {
			shell_echo(rest);
			continue;
		}
		if (kstreq(cmd, "uname")) {
			console_print_color("NucleOS", VGA_ATTR(VGA_CYAN, VGA_BLACK));
			console_print_color(" x86_64 ", VGA_DEFAULT_ATTR);
			console_print_color("v3\n", VGA_ATTR(VGA_GREEN, VGA_BLACK));
			continue;
		}
		if (kstreq(cmd, "whoami")) {
			console_print_color("nucleos\n", VGA_ATTR(VGA_GREEN, VGA_BLACK));
			continue;
		}
		if (kstreq(cmd, "meminfo")) {
			console_print_color("\n  Physical Memory\n", VGA_ATTR(VGA_CYAN, VGA_BLACK));
			char buf[32];
			console_print("  Total:     ");
			kitoa(pmm_get_total_bytes() / 1024, buf, sizeof(buf));
			console_print_color(buf, VGA_ATTR(VGA_WHITE, VGA_BLACK));
			console_print(" KB (");
			kitoa(pmm_get_total_pages(), buf, sizeof(buf));
			console_print(buf);
			console_print(" pages)\n");
			console_print("  Free:      ");
			kitoa(pmm_get_free_bytes() / 1024, buf, sizeof(buf));
			console_print_color(buf, VGA_ATTR(VGA_LIGHT_GREEN, VGA_BLACK));
			console_print(" KB (");
			kitoa(pmm_get_free_pages(), buf, sizeof(buf));
			console_print(buf);
			console_print(" pages)\n");
			console_print("  Used:      ");
			unsigned long used = pmm_get_total_bytes() - pmm_get_free_bytes();
			kitoa(used / 1024, buf, sizeof(buf));
			console_print_color(buf, VGA_ATTR(VGA_LIGHT_RED, VGA_BLACK));
			console_print(" KB (");
			kitoa(pmm_get_used_pages(), buf, sizeof(buf));
			console_print(buf);
			console_print(" pages)\n");
			console_print_color("\n  Virtual Memory\n", VGA_ATTR(VGA_CYAN, VGA_BLACK));
			console_print("  Status:    ");
			unsigned long cr0;
			__asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
			if (cr0 & 0x80000000)
				console_print_color("Active (paging enabled)\n", VGA_ATTR(VGA_LIGHT_GREEN, VGA_BLACK));
			else
				console_print_color("Inactive\n", VGA_ATTR(VGA_RED, VGA_BLACK));
			console_print("  Page Size: 4096 bytes\n");
			console_print("  Pages:     ");
			kitoa(pmm_get_total_pages(), buf, sizeof(buf));
			console_print(buf);
			console_print(" mapped (identity)\n");
			console_print("\n");
			continue;
		}
		if (kstreq(cmd, "sudo")) {
			if (rest[0] == '\0') {
				console_print("Usage: sudo <command>\n");
			} else {
				char s_cmd[32];
				char s_arg1[128];
				char s_rest[192];
				const char *sp = rest;
				sp = parse_token(sp, s_cmd, sizeof(s_cmd));
				const char *s_tail = sp;
				sp = parse_token(sp, s_arg1, sizeof(s_arg1));
				copy_rest(s_tail, s_rest, sizeof(s_rest));
				if (kstreq(s_cmd, "ls")) {
					bool show_long = false;
					const char *path = 0;
					const char *r = s_rest;
					char tok[32];
					while (*r) {
						r = parse_token(r, tok, sizeof(tok));
						if (kstreq(tok, "-l"))
							show_long = true;
						else if (tok[0] != '\0')
							path = tok;
					}
					if (show_long)
						ls_long(path);
					else
						ls_short(path);
				} else if (kstreq(s_cmd, "reboot")) {
					console_print("Rebooting (sudo)...\n");
					reboot_cpu();
				} else if (kstreq(s_cmd, "poweroff") || kstreq(s_cmd, "shutdown")) {
					console_print("Shutting down...\n");
					halt_cpu();
				} else {
					console_print_color("[sudo] ", VGA_ATTR(VGA_YELLOW, VGA_BLACK));
					console_print_color(s_cmd, VGA_ATTR(VGA_WHITE, VGA_BLACK));
					console_print(": command executed as root\n");
				}
			}
			continue;
		}
		if (kstreq(cmd, "chmod")) {
			if (arg1[0] == '\0' || arg2[0] == '\0')
				console_print("Usage: chmod <mode> <file>\n");
			else
				console_print("chmod: permissions changed (simulated)\n");
			continue;
		}
		if (kstreq(cmd, "df")) {
			console_print_color("Filesystem  ", VGA_ATTR(VGA_YELLOW, VGA_BLACK));
			console_print_color("1K-blocks  ", VGA_ATTR(VGA_YELLOW, VGA_BLACK));
			console_print_color("Used  ", VGA_ATTR(VGA_YELLOW, VGA_BLACK));
			console_print_color("Avail  ", VGA_ATTR(VGA_YELLOW, VGA_BLACK));
			console_print_color("Mounted on\n", VGA_ATTR(VGA_YELLOW, VGA_BLACK));
			console_print_color("rootfs      64        8     56     /\n", VGA_ATTR(VGA_LIGHT_GREY, VGA_BLACK));
			continue;
		}
		if (kstreq(cmd, "stat")) {
			if (arg1[0] == '\0') {
				console_print("Usage: stat <file>\n");
			} else if (!vfs_exists(arg1)) {
				console_print_color("stat: ", VGA_ATTR(VGA_RED, VGA_BLACK));
				console_print(arg1);
				console_print(": no such file or directory\n");
			} else {
				vfs_stat(arg1);
			}
			continue;
		}
		if (kstreq(cmd, "reboot")) {
			console_print("Rebooting...\n");
			reboot_cpu();
			continue;
		}
		if (kstreq(cmd, "panic")) {
			kernel_panic(rest[0] ? rest : "Kernel panic triggered from shell.");
			continue;
		}
		if (kstreq(cmd, "lcp")) {
			lcp_handle_command(rest);
			continue;
		}
		if (kstreq(cmd, "openrc")) {
			openrc_handle_command(rest);
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
		if (kstreq(cmd, "lspci")) {
			int count = pci_get_device_count();
			if (count == 0) {
				console_print("No PCI devices found.\n");
			} else {
				console_print_color("BDF    Vendor  Device  Class\n", VGA_ATTR(VGA_YELLOW, VGA_BLACK));
				for (int i = 0; i < count; i++) {
					const struct pci_device *dev = pci_get_device(i);
					if (!dev) break;
					char bdf[32];
					char num[8];
					kitoa(dev->bus, num, sizeof(num));
					kstrcpy(bdf, num, sizeof(bdf));
					kstrcat(bdf, ":", sizeof(bdf));
					kitoa(dev->device, num, sizeof(num));
					kstrcat(bdf, num, sizeof(bdf));
					kstrcat(bdf, ".", sizeof(bdf));
					kitoa(dev->function, num, sizeof(num));
					kstrcat(bdf, num, sizeof(bdf));
					int len = kstrlen(bdf);
					while (len < 7) { bdf[len++] = ' '; bdf[len] = '\0'; }
					console_print(bdf);
					char vd[12];
					kitoa(dev->vendor_id, num, sizeof(num));
					kstrcpy(vd, num, sizeof(vd));
					kstrcat(vd, ":", sizeof(vd));
					kitoa(dev->device_id, num, sizeof(num));
					kstrcat(vd, num, sizeof(vd));
					len = kstrlen(vd);
					while (len < 10) { vd[len++] = ' '; vd[len] = '\0'; }
					console_print(vd);
					console_print("0x");
					char cls[8];
					kitoa(dev->class_code, num, sizeof(num));
					kstrcpy(cls, num, sizeof(cls));
					kstrcat(cls, ".", sizeof(cls));
					kitoa(dev->subclass, num, sizeof(num));
					kstrcat(cls, num, sizeof(cls));
					console_print(cls);
					console_print("\n");
				}
			}
			continue;
		}
		if (kstreq(cmd, "asm")) {
			handle_asm_command(rest);
			continue;
		}
		if (kstreq(cmd, "calc")) {
			calc_app(rest);
			continue;
		}
		if (kstreq(cmd, "mouse")) {
			show_mouse_state();
			continue;
		}
		if (kstreq(cmd, "games")) {
			games_menu();
			continue;
		}
		if (kstreq(cmd, "fastfetch") || kstreq(cmd, "neofetch") || kstreq(cmd, "sysinfo")) {
			fastfetch();
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
		if (kstreq(cmd, "hexdump")) {
			if (arg1[0] == '\0')
				console_print("Usage: hexdump <file>\n");
			else {
				const void *data = vfs_read(arg1);
				size_t size = vfs_get_size(arg1);
				if (!data || size == 0)
					console_print_color("hexdump: file not found or empty\n", VGA_ATTR(VGA_RED, VGA_BLACK));
				else {
					const unsigned char *p = (const unsigned char *)data;
					for (size_t i = 0; i < size; i += 16) {
						char addr[16];
						kxtoa(i, addr, sizeof(addr));
						int alen = kstrlen(addr);
						while (alen < 8) { console_print("0"); alen++; }
						console_print(addr);
						console_print("  ");
						char hex[4];
						for (int j = 0; j < 16 && i + j < size; ++j) {
							kxtoa(p[i + j], hex, sizeof(hex));
							if (kstrlen(hex) < 2) console_print("0");
							console_print(hex);
							console_print(" ");
						}
						console_print(" |");
						for (int j = 0; j < 16 && i + j < size; ++j) {
							char c = p[i + j];
							console_putchar((c >= 32 && c < 127) ? c : '.');
						}
						console_print("|\n");
					}
				}
			}
			continue;
		}
		if (kstreq(cmd, "wc")) {
			if (arg1[0] == '\0')
				console_print("Usage: wc <file>\n");
			else {
				const void *data = vfs_read(arg1);
				size_t size = vfs_get_size(arg1);
				if (!data || size == 0)
					console_print_color("wc: file not found\n", VGA_ATTR(VGA_RED, VGA_BLACK));
				else {
					const char *text = (const char *)data;
					size_t lines = 0, words = 0, chars = size;
					int in_word = 0;
					for (size_t i = 0; i < size; ++i) {
						if (text[i] == '\n') lines++;
						if (text[i] == ' ' || text[i] == '\n' || text[i] == '\t') {
							if (in_word) { words++; in_word = 0; }
						} else {
							in_word = 1;
						}
					}
					if (in_word) words++;
					char buf[16];
					kitoa(lines, buf, sizeof(buf));
					console_print(buf);
					console_print(" ");
					kitoa(words, buf, sizeof(buf));
					console_print(buf);
					console_print(" ");
					kitoa(chars, buf, sizeof(buf));
					console_print(buf);
					console_print(" ");
					console_print_color(arg1, VGA_ATTR(VGA_LIGHT_BLUE, VGA_BLACK));
					console_print("\n");
				}
			}
			continue;
		}
		if (kstreq(cmd, "head")) {
			if (arg1[0] == '\0')
				console_print("Usage: head [-n N] <file>\n");
			else {
				int nlines = 10;
				const char *path = arg1;
				if (kstreq(arg1, "-n")) {
					nlines = parse_int(arg2);
					if (nlines <= 0) nlines = 10;
					path = rest;
					char tok[128];
					const char *r = rest;
					r = parse_token(r, tok, sizeof(tok));
					r = parse_token(r, tok, sizeof(tok));
					path = tok;
				}
				const void *data = vfs_read(path);
				size_t size = vfs_get_size(path);
				if (!data || size == 0)
					console_print_color("head: file not found\n", VGA_ATTR(VGA_RED, VGA_BLACK));
				else {
					const char *text = (const char *)data;
					int shown = 0;
					for (size_t i = 0; i < size && shown < nlines; ++i) {
						console_putchar(text[i]);
						if (text[i] == '\n') shown++;
					}
					if (shown == 0) console_print("\n");
				}
			}
			continue;
		}
		if (kstreq(cmd, "tail")) {
			if (arg1[0] == '\0')
				console_print("Usage: tail [-n N] <file>\n");
			else {
				int nlines = 10;
				const char *path = arg1;
				if (kstreq(arg1, "-n")) {
					nlines = parse_int(arg2);
					if (nlines <= 0) nlines = 10;
					char tok[128];
					const char *r = rest;
					r = parse_token(r, tok, sizeof(tok));
					r = parse_token(r, tok, sizeof(tok));
					path = tok;
				}
				const void *data = vfs_read(path);
				size_t size = vfs_get_size(path);
				if (!data || size == 0)
					console_print_color("tail: file not found\n", VGA_ATTR(VGA_RED, VGA_BLACK));
				else {
					const char *text = (const char *)data;
					int total_lines = 0;
					for (size_t i = 0; i < size; ++i)
						if (text[i] == '\n') total_lines++;
					int skip = total_lines - nlines;
					if (skip < 0) skip = 0;
					int line = 0;
					for (size_t i = 0; i < size; ++i) {
						if (line >= skip)
							console_putchar(text[i]);
						if (text[i] == '\n') line++;
					}
					if (size > 0 && text[size - 1] != '\n')
						console_print("\n");
				}
			}
			continue;
		}
		if (kstreq(cmd, "snake")) { game_launch_snake(); continue; }
		if (kstreq(cmd, "tetris")) { game_launch_tetris(); continue; }
		if (kstreq(cmd, "pong")) { game_launch_pong(); continue; }
		if (kstreq(cmd, "2048")) { game_launch_2048(); continue; }
		if (kstreq(cmd, "ttt")) { game_launch_ttt(); continue; }
		if (kstreq(cmd, "minesweeper")) { game_launch_minesweeper(); continue; }
		if (kstreq(cmd, "breakout")) { game_launch_breakout(); continue; }
		if (kstreq(cmd, "memory")) { game_launch_memory(); continue; }
		if (kstreq(cmd, "nano")) {
			app_nano(arg1[0] ? arg1 : 0);
			continue;
		}
		if (kstreq(cmd, "hexview")) {
			app_hexview(arg1[0] ? arg1 : 0);
			continue;
		}
		if (kstreq(cmd, "fm") || kstreq(cmd, "filemanager")) {
			app_filemanager();
			continue;
		}
		if (kstreq(cmd, "htop")) {
			app_htop();
			continue;
		}
		if (kstreq(cmd, "calc-tui")) {
			app_calc_tui();
			continue;
		}
		console_print_color("nucleos: command not found: ", VGA_ATTR(VGA_RED, VGA_BLACK));
		console_print_color(cmd, VGA_ATTR(VGA_WHITE, VGA_BLACK));
		console_print("\n");
	}
}
