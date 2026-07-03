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

unsigned long sys_mem_lower;
unsigned long sys_mem_upper;

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

static void boot_status(const char *msg)
{
	console_print_color("[  OK  ] ", VGA_ATTR(VGA_GREEN, VGA_BLACK));
	console_print_color(msg, VGA_DEFAULT_ATTR);
	console_print("\n");
}

static void boot_failed(const char *msg)
{
	console_print_color("[FAILED] ", VGA_ATTR(VGA_RED, VGA_BLACK));
	console_print_color(msg, VGA_DEFAULT_ATTR);
	console_print("\n");
}

static void boot_info(const char *msg)
{
	console_print_color("[ INFO ] ", VGA_ATTR(VGA_CYAN, VGA_BLACK));
	console_print_color(msg, VGA_DEFAULT_ATTR);
	console_print("\n");
}

static void boot_delay(void)
{
	volatile int i;
	for (i = 0; i < 3000000; i++);
}

static void print_banner(void)
{
	console_print_color("\n", VGA_DEFAULT_ATTR);
	console_print_color("          ██████  ██████  ██ ███████  ██████  ███████ \n", VGA_ATTR(VGA_WHITE, VGA_BLACK));
	console_print_color("         ██      ██    ██ ██ ██      ██    ██ ██      \n", VGA_ATTR(VGA_WHITE, VGA_BLACK));
	console_print_color("         ██      ██    ██ ██ ███████ ██    ██ ███████ \n", VGA_ATTR(VGA_WHITE, VGA_BLACK));
	console_print_color("         ██      ██    ██ ██      ██ ██    ██      ██ \n", VGA_ATTR(VGA_WHITE, VGA_BLACK));
	console_print_color("          ██████  ██████  ██ ███████  ██████  ███████ \n", VGA_ATTR(VGA_WHITE, VGA_BLACK));
	console_print_color("                     Operating System v3\n", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
	console_print("\n");
	boot_info("Booting CrisOS v3 i386...\n");
	boot_delay();
}

void kmain(unsigned long mbi_addr)
{
	console_clear();
	print_banner();

	boot_status("Started Console");
	boot_delay();
	gdt_init();
	boot_status("Loaded GDT");
	boot_delay();

	idt_init();
	boot_status("Loaded IDT");
	boot_delay();

	pic_init();
	boot_status("Initialized PIC");
	boot_delay();
	pic_mask(0xFD, 0xFF);

	timer_init(100);
	boot_status("Started PIT");
	boot_delay();

	keyboard_init();
	boot_status("Initialized Keyboard");
	boot_delay();

	mouse_init();
	pic_mask(0xFA, 0xEF);
	boot_status("Initialized Mouse");

	bool rootfs_loaded = false;
	boot_delay();

	if (mbi_addr) {
		struct multiboot_info *mbi = (struct multiboot_info *)mbi_addr;

		if (mbi->flags & 0x1) {
			sys_mem_lower = mbi->mem_lower;
			sys_mem_upper = mbi->mem_upper;
		}

		if (mbi->flags & 0x8) {
			boot_info("Detected Multiboot modules\n");
			boot_delay();

			struct multiboot_module *mods =
			    (struct multiboot_module *)mbi->mods_addr;

			for (unsigned long i = 0; i < mbi->mods_count; ++i) {
				const char *name = (const char *)mods[i].cmdline;
				if (name && kstrstr(name, "rootfs")) {
					boot_info("Mounting rootfs...\n");
					boot_delay();
					if (fs_init((const void *)mods[i].mod_start,
						    mods[i].mod_end - mods[i].mod_start)) {
						boot_status("Mounted rootfs");
						boot_delay();
						if (vfs_init()) {
							boot_status("Initialized VFS");
							rootfs_loaded = true;
						} else {
							boot_failed("VFS initialization");
						}
					} else {
						boot_failed("Rootfs mount");
					}
					break;
				}
			}

			if (!rootfs_loaded)
				boot_failed("Rootfs module not found");
		}
	}

	boot_delay();
	boot_init();
	boot_status("Started Boot Manager");
	boot_delay();

	systemd_init();
	boot_status("Started Service Manager");
	boot_delay();

	lcp_init();
	boot_status("Started LCP Package Manager");
	boot_delay();

	console_print("\n");
	boot_status("Reached target Multi-User System");
	boot_delay();
	console_print("\n");

	sti();
	shell_run();
}
