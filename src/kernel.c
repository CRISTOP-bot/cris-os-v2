#include "console.h"
#include "asm.h"
#include "fs.h"
#include "vfs.h"
#include "shell.h"
#include "keyboard.h"
#include "lcp.h"
#include "systemd.h"
#include "boot.h"
#include "idt.h"
#include "gdt.h"
#include "pic.h"
#include "timer.h"
#include "mouse.h"
#include "kstring.h"
#include "asm.h"
#include <stdbool.h>

struct multiboot_info {
	unsigned long flags;
	unsigned long mem_lower;
	unsigned long mem_upper;
	unsigned long boot_device;
	unsigned long cmdline;
	unsigned long mods_count;
	unsigned long mods_addr;
};

struct multiboot_module {
	unsigned long mod_start;
	unsigned long mod_end;
	unsigned long cmdline;
	unsigned long reserved;
};

static void print_tag(const char *tag, const char *msg, unsigned char color)
{
	console_print_color("[ ", VGA_DEFAULT_ATTR);
	console_print_color(tag, color);
	console_print_color(" ] ", VGA_DEFAULT_ATTR);
	console_print(msg);
}

static void print_banner(void)
{
	console_print_color("\n", VGA_DEFAULT_ATTR);
	console_print_color("  ██████  ██████  ██ ███████  ██████  ███████ \n", VGA_ATTR(VGA_CYAN, VGA_BLACK));
	console_print_color(" ██      ██    ██ ██ ██      ██    ██ ██      \n", VGA_ATTR(VGA_CYAN, VGA_BLACK));
	console_print_color(" ██      ██    ██ ██ ███████ ██    ██ ███████ \n", VGA_ATTR(VGA_CYAN, VGA_BLACK));
	console_print_color(" ██      ██    ██ ██      ██ ██    ██      ██ \n", VGA_ATTR(VGA_CYAN, VGA_BLACK));
	console_print_color("  ██████  ██████  ██ ███████  ██████  ███████ \n", VGA_ATTR(VGA_CYAN, VGA_BLACK));
	console_print_color("                     Operating System v2\n", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
	console_print("\n");
}

void kmain(unsigned long mbi_addr)
{
	console_clear();
	print_banner();

	print_tag("OK", "Console initialized\n", VGA_GREEN);
	gdt_init();

	idt_init();

	pic_init();
	print_tag("OK", "PIC initialized (IRQs 0-15 remapped)\n", VGA_GREEN);

	pic_mask(0xFD, 0xFF); /* enable only IRQ1 (keyboard) */

	timer_init(100);
	print_tag("OK", "PIT timer initialized (100 Hz)\n", VGA_GREEN);

	keyboard_init();
	print_tag("OK", "Keyboard initialized\n", VGA_GREEN);

	mouse_init();
	pic_mask(0xFC, 0xEF); /* enable IRQ1 + IRQ12 */

	bool rootfs_loaded = false;

	if (mbi_addr) {
		struct multiboot_info *mbi = (struct multiboot_info *)mbi_addr;

		if (mbi->flags & 0x8) {
			print_tag("OK", "Multiboot modules detected\n", VGA_GREEN);

			struct multiboot_module *mods =
			    (struct multiboot_module *)mbi->mods_addr;

			for (unsigned long i = 0; i < mbi->mods_count; ++i) {
				const char *name = (const char *)mods[i].cmdline;
				if (name && kstrstr(name, "rootfs")) {
					print_tag("...", "Mounting rootfs...\n", VGA_CYAN);
					if (fs_init((const void *)mods[i].mod_start,
						    mods[i].mod_end - mods[i].mod_start)) {
						print_tag("OK", "Rootfs mounted\n", VGA_GREEN);
						if (vfs_init()) {
							print_tag("OK", "VFS initialized\n", VGA_GREEN);
							rootfs_loaded = true;
						} else {
							print_tag("FAIL", "VFS initialization failed\n", VGA_RED);
						}
					} else {
						print_tag("FAIL", "Rootfs mount failed\n", VGA_RED);
					}
					break;
				}
			}

			if (!rootfs_loaded)
				print_tag("WARN", "No rootfs module found or failed to mount\n", VGA_BROWN);
		} else {
			print_tag("FAIL", "No multiboot modules found\n", VGA_RED);
		}
	} else {
		print_tag("FAIL", "Invalid multiboot info\n", VGA_RED);
	}

	boot_init();
	print_tag("OK", "Boot manager initialized\n", VGA_GREEN);

	systemd_init();
	print_tag("OK", "Service manager initialized\n", VGA_GREEN);

	lcp_init();
	print_tag("OK", "LCP package manager initialized\n", VGA_GREEN);

	int sum = add_asm(7, 5);
	char buf[32];
	kitoa(sum, buf, sizeof(buf));
	console_print("\n  ");
	console_print_color("[ASM]", VGA_ATTR(VGA_YELLOW, VGA_BLACK));
	console_print_color(" 7 + 5 = ", VGA_DEFAULT_ATTR);
	console_print_color(buf, VGA_ATTR(VGA_WHITE, VGA_BLACK));
	console_print_color("\n\n", VGA_DEFAULT_ATTR);

	print_tag("OK", "All systems ready. Launching shell...\n", VGA_GREEN);
	console_print("\n");

	shell_run();
}
