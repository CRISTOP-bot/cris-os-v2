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
#include "timer.h"
#include "fs.h"
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

static void print_prompt(void)
{
	console_print_color("cris", VGA_ATTR(VGA_GREEN, VGA_BLACK));
	console_print_color("@", VGA_DEFAULT_ATTR);
	console_print_color("crisos", VGA_ATTR(VGA_CYAN, VGA_BLACK));
	console_print_color(":", VGA_DEFAULT_ATTR);
	console_print_color(vfs_pwd(), VGA_ATTR(VGA_LIGHT_BLUE, VGA_BLACK));
	console_print_color("$ ", VGA_ATTR(VGA_WHITE, VGA_BLACK));
}

static void print_help(void)
{
	console_print_color("\n-- Filesystem --\n", VGA_ATTR(VGA_YELLOW, VGA_BLACK));
	console_print("  ls      List directory contents\n");
	console_print("  pwd     Print working directory\n");
	console_print("  cd      Change directory\n");
	console_print("  mkdir   Create directory\n");
	console_print("  rmdir   Remove directory\n");
	console_print("  touch   Create empty file\n");
	console_print("  rm      Remove file\n");
	console_print("  cp      Copy file\n");
	console_print("  mv      Move/rename file\n");
	console_print("  cat     Display file contents\n");
	console_print("  grep    Search in file\n");
	console_print("  echo    Print text or write to file\n");
	console_print("  stat    Display file information\n");
	console_print("  df      Show filesystem usage\n");
	console_print_color("\n-- System --\n", VGA_ATTR(VGA_YELLOW, VGA_BLACK));
	console_print("  uname      Display system information\n");
	console_print("  whoami     Display current user\n");
	console_print("  fastfetch  Show system information (neofetch-like)\n");
	console_print("  clear      Clear screen\n");
	console_print("  reboot     Reboot the system\n");
	console_print("  panic      Trigger kernel panic\n");
	console_print_color("\n-- Services --\n", VGA_ATTR(VGA_YELLOW, VGA_BLACK));
	console_print("  systemctl  Service manager\n");
	console_print("  bootctl    Boot manager\n");
	console_print("  lcp        Package manager\n");
	console_print_color("\n-- Misc --\n", VGA_ATTR(VGA_YELLOW, VGA_BLACK));
	console_print("  asm        Arithmetic via assembly\n");
	console_print("  calc       Expression calculator\n");
	console_print("  kblayout   Change keyboard layout\n");
	console_print("  mouse      Show mouse state\n");
	console_print("  gui        Show GUI (experimental)\n");
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
	console_print_color("             #####\n", VGA_ATTR(VGA_YELLOW, VGA_BLACK));
	console_print_color("            #######           ", attr_label); console_print_color("OS:      ", attr_label); console_print_color("CrisOS v3 i386\n", attr_val);
	console_print_color("            ##               ", attr_label); console_print_color("Kernel:  ", attr_label); console_print_color("CrisOS v3\n", attr_val);
	console_print_color("            ##               ", attr_label);
	if (cpu_vendor[0])
		console_print_color("CPU:     ", attr_label);
	else
		console_print_color("CPU:     ", attr_label);
	console_print_color(cpu_vendor[0] ? cpu_vendor : "i386 (no CPUID)", attr_val);
	console_print("\n");
	console_print_color("            ##               ", attr_label); console_print_color("Uptime:  ", attr_label);
	unsigned long ticks = timer_get_ticks();
	unsigned long secs = ticks / 100;
	kitoa(secs, buf, sizeof(buf));
	console_print_color(buf, attr_val);
	console_print_color("s\n", attr_val);
	console_print_color("            ##               ", attr_label); console_print_color("Memory:  ", attr_label);
	if (sys_mem_upper) {
		kitoa((sys_mem_upper + sys_mem_lower) / 1024, buf, sizeof(buf));
		console_print_color(buf, attr_val);
		console_print_color(" MB\n", attr_val);
	} else {
		console_print_color("not detected\n", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
	}
	console_print_color("            ##               ", attr_label); console_print_color("Shell:   ", attr_label); console_print_color("CrisOS Shell\n", attr_val);
	console_print_color("            ##               ", attr_label); console_print_color("Term:    ", attr_label); console_print_color("VGA 80x25\n", attr_val);
	console_print_color("            ##               ", attr_label); console_print_color("User:    ", attr_label); console_print_color("cris\n", attr_val);
	console_print_color("           ###               ", attr_label); console_print_color("Layout:  ", attr_label);
	console_print_color(layout_name(keyboard_get_layout()), attr_val);
	console_print("\n");
	unsigned long nfiles = fs_file_count();
	kitoa(nfiles, buf, sizeof(buf));
	console_print_color("          ## ##              ", attr_label); console_print_color("Files:   ", attr_label);
	console_print_color(buf, attr_val);
	console_print_color(" in rootfs\n", attr_val);
	console_print_color("         ##   ##      \n", VGA_ATTR(VGA_YELLOW, VGA_BLACK));
	console_print("\n");
}

void shell_run(void)
{
	char buf[256];

	console_print_color("============================================\n", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
	console_print_color("  Welcome to CrisOS v3 Interactive Shell\n", VGA_ATTR(VGA_CYAN, VGA_BLACK));
	console_print_color("  Type 'help' for a list of commands\n", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
	console_print_color("============================================\n\n", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));

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
			const char *path = arg1[0] ? arg1 : 0;
			if (!vfs_list(path))
				console_print_color("ls: invalid path\n", VGA_ATTR(VGA_RED, VGA_BLACK));
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
			console_print_color("CrisOS", VGA_ATTR(VGA_CYAN, VGA_BLACK));
			console_print_color(" i386 ", VGA_DEFAULT_ATTR);
			console_print_color("v3\n", VGA_ATTR(VGA_GREEN, VGA_BLACK));
			continue;
		}
		if (kstreq(cmd, "whoami")) {
			console_print_color("cris\n", VGA_ATTR(VGA_GREEN, VGA_BLACK));
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
				size_t size = vfs_get_size(arg1);
				console_print("  File: ");
				console_print_color(arg1, VGA_ATTR(VGA_WHITE, VGA_BLACK));
				console_print("\n  Size: ");
				char num[32];
				kitoa((long)size, num, sizeof(num));
				console_print_color(num, VGA_ATTR(VGA_CYAN, VGA_BLACK));
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
		console_print_color("crisos: command not found: ", VGA_ATTR(VGA_RED, VGA_BLACK));
		console_print_color(cmd, VGA_ATTR(VGA_WHITE, VGA_BLACK));
		console_print("\n");
	}
}
