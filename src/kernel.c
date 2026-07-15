#include "console.h"
#include "asm.h"
#include "fs.h"
#include "vfs.h"
#include "shell.h"
#include "keyboard.h"
#include "lcp.h"
#include "openrc.h"
#include "boot.h"
#include "idt.h"
#include "gdt.h"
#include "pic.h"
#include "timer.h"
#include "mouse.h"
#include "kstring.h"
#include "pci.h"
#include "pmm.h"
#include "vmm.h"
#include "memory.h"
#include <stdbool.h>
#include <stdint.h>

uint32_t sys_mem_lower;
uint32_t sys_mem_upper;

struct multiboot_info {
	uint32_t flags;
	uint32_t mem_lower;
	uint32_t mem_upper;
	uint32_t boot_device;
	uint32_t cmdline;
	uint32_t mods_count;
	uint32_t mods_addr;
};

struct multiboot_module {
	uint32_t mod_start;
	uint32_t mod_end;
	uint32_t cmdline;
	uint32_t reserved;
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
	console_print_color("========================================\n", VGA_ATTR(VGA_CYAN, VGA_BLACK));
	console_print_color("   _   _            _   _  ____         \n", VGA_ATTR(VGA_CYAN, VGA_BLACK));
	console_print_color("  | \\ | | ___  _ __| | | |/ ___|       \n", VGA_ATTR(VGA_CYAN, VGA_BLACK));
	console_print_color("  |  \\| |/ _ \\| '__| | | | |  _        \n", VGA_ATTR(VGA_CYAN, VGA_BLACK));
	console_print_color("  | |\\  | (_) | |  | |_| | |_| |       \n", VGA_ATTR(VGA_CYAN, VGA_BLACK));
	console_print_color("  |_| \\_|\\___/|_|   \\___/ \\____|       \n", VGA_ATTR(VGA_CYAN, VGA_BLACK));
	console_print_color("========================================\n", VGA_ATTR(VGA_CYAN, VGA_BLACK));
	console_print_color("      Open Source Operating System\n", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
	console_print("\n");
	boot_info("Booting NucleOS v3 x86_64...\n");
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
	pic_mask(0xFC, 0xFF);

	if (mbi_addr) {
		struct multiboot_info *mbi = (struct multiboot_info *)mbi_addr;
		if (mbi->flags & 0x1) {
			sys_mem_lower = mbi->mem_lower;
			sys_mem_upper = mbi->mem_upper;
		}
	}

	pmm_init(sys_mem_lower, sys_mem_upper);
	boot_status("Initialized Physical Memory Manager");
	boot_delay();

	vmm_init(sys_mem_lower, sys_mem_upper);
	boot_status("Initialized Virtual Memory Manager");
	boot_delay();

	memory_init();
	boot_status("Initialized Heap Allocator");
	boot_delay();

	pci_init();
	boot_delay();

	timer_init(100);
	boot_status("Started PIT");
	boot_delay();

	keyboard_init();
	boot_status("Initialized Keyboard");
	boot_delay();

	mouse_init();
	pic_mask(0xFC, 0xEF);
	boot_status("Initialized Mouse");

	bool rootfs_loaded = false;
	boot_delay();

	if (mbi_addr) {
		struct multiboot_info *mbi = (struct multiboot_info *)mbi_addr;

		if (mbi->flags & 0x8) {
			boot_info("Detected Multiboot modules\n");
			boot_delay();

			struct multiboot_module *mods =
			    (struct multiboot_module *)(uintptr_t)mbi->mods_addr;

			for (unsigned long i = 0; i < mbi->mods_count; ++i) {
				const char *name = (const char *)(uintptr_t)mods[i].cmdline;
				if (name && kstrstr(name, "rootfs")) {
					boot_info("Mounting rootfs...\n");
					boot_delay();
					if (fs_init((const void *)(uintptr_t)mods[i].mod_start,
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

	openrc_init();
	boot_status("Started Service Manager (OpenRC)");
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
