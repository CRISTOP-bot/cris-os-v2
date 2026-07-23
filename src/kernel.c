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
#include "tss.h"
#include "syscall.h"
#include "process.h"
#include "serial.h"
#include "rust_ffi.h"
#include "persist.h"
#include "net.h"
#include "ata.h"
#include "part.h"
#include "ext2.h"
#include <stdbool.h>
#include <stdint.h>

#define MULTIBOOT_MAGIC 0x2BADB002

uint32_t sys_mem_lower;
uint32_t sys_mem_upper;
extern uint32_t multiboot_magic;

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

static void enable_a20(void)
{
	outb(0x64, 0xAD);
	outb(0x64, 0xD0);
	int t = 10000;
	while (t-- && !(inb(0x64) & 1));
	uint8_t val = inb(0x60);
	outb(0x64, 0xD1);
	t = 10000;
	while (t-- && (inb(0x64) & 2));
	outb(0x60, val | 2);
	outb(0x64, 0xAE);
}

void kmain(unsigned long mbi_addr)
{
	console_clear();
	print_banner();

	if (multiboot_magic != MULTIBOOT_MAGIC) {
		boot_failed("Invalid multiboot magic");
		halt_cpu();
	}

	if (mbi_addr) {
		struct multiboot_info *mbi = (struct multiboot_info *)mbi_addr;
		if ((mbi->flags & 0x1) == 0) {
			boot_failed("No memory info from bootloader");
		}
	}

	enable_a20();
	boot_status("A20 gate enabled");
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

	if (!sys_mem_upper) {
		boot_failed("No usable memory detected");
		halt_cpu();
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

	tss_init();
	boot_status("Initialized TSS");
	boot_delay();

	serial_init();
	boot_status("Initialized Serial Port");
	boot_delay();

	rust_serial_init();
	rust_info();
	boot_status("Loaded Rust kernel module");
	boot_delay();

	pci_init();
	boot_delay();

	timer_init(100);
	boot_status("Started PIT (100 Hz)");
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
					boot_info("Mounting rootfs from module...\n");
					boot_delay();
					if (fs_init((const void *)(uintptr_t)mods[i].mod_start,
						    mods[i].mod_end - mods[i].mod_start)) {
						boot_status("Mounted rootfs (module)");
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

	if (!rootfs_loaded) {
		boot_info("No module rootfs found, trying disk...\n");
		boot_delay();

		boot_info("ATA scan...\n");
		ata_init();

		struct part_entry part;
		if (part_find_by_type(0, PART_TYPE_LINUX, &part) > 0) {
			boot_info("Found Linux partition on disk\n");
			boot_delay();

			if (ext2_mount(0, part.lba_start)) {
				boot_status("EXT2 filesystem mounted");

				uint32_t rootfs_size = ext2_get_file_size("/boot/rootfs.bin");
				if (rootfs_size > 0) {
					boot_info("Loading rootfs.bin from disk...\n");
					boot_delay();

					void *rootfs_buf = pmm_alloc_page();
					uint32_t alloc_size = PAGE_SIZE;

					while (alloc_size < rootfs_size) {
						void *extra = pmm_alloc_page();
						if (!extra) break;
						alloc_size += PAGE_SIZE;
					}

					if (rootfs_buf && ext2_read_file("/boot/rootfs.bin",
					    rootfs_buf, &rootfs_size)) {
						if (fs_init(rootfs_buf, rootfs_size)) {
							boot_status("Mounted rootfs (disk)");
							boot_delay();
							if (vfs_init()) {
								boot_status("Initialized VFS");
								rootfs_loaded = true;
							} else {
								boot_failed("VFS init");
							}
						} else {
							boot_failed("Rootfs mount (disk)");
						}
					} else {
						boot_failed("Failed to read rootfs.bin");
					}
				} else {
					boot_failed("rootfs.bin not found on disk");
				}
			} else {
				boot_failed("Failed to mount EXT2");
			}
		} else {
			boot_failed("No Linux partition found");
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

	process_init();
	boot_status("Initialized Process Manager");
	boot_delay();

	syscall_init();
	boot_status("Initialized Syscalls (INT 0x80)");
	boot_delay();

	persist_init();
	boot_status("Initialized Persistent Storage");
	boot_delay();

	net_init();
	boot_status("Initialized Network Stack");
	boot_delay();

	console_print("\n");
	boot_status("Reached target Multi-User System");
	boot_delay();
	console_print("\n");

	sti();
	shell_run();
}
