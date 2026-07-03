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

static void serial_out(char c) {
	while (!(inb(0x3F8 + 5) & 0x20));
	outb(0x3F8, c);
}

static void serial_print(const char *s) {
	while (*s) serial_out(*s++);
}

void kmain(unsigned long mbi_addr)
{
	/* init serial port (115200 8N1) */
	outb(0x3F8 + 1, 0x00);
	outb(0x3F8 + 3, 0x80);
	outb(0x3F8 + 0, 0x01);
	outb(0x3F8 + 1, 0x00);
	outb(0x3F8 + 3, 0x03);
	outb(0x3F8 + 2, 0xC7);
	outb(0x3F8 + 4, 0x0B);
	serial_print("kmain() started\n");

	console_clear();
	serial_print("console_clear done\n");

	for (volatile int i = 0; i < 100000; i++); // tiny delay
	serial_print("starting console_print\n");
	console_print("================================\n");
	serial_print("after first print\n");
	console_print("        CrisOS Kernel\n");
	serial_print("after second print\n");
	console_print("================================\n\n");
	serial_print("after third print\n");

	console_print("[ OK ] Console initialized\n");
	serial_print("before gdt_init\n");

	gdt_init();
	serial_print("after gdt_init\n");

	serial_print("before idt_init\n");
	idt_init();
	serial_print("after idt_init\n");

	serial_print("before pic_init\n");
	pic_init();
	serial_print("after pic_init\n");
	console_print("[ OK ] PIC initialized (IRQs 0-15 remapped)\n");

	serial_print("before pic_mask\n");
	pic_mask(0xFD, 0xFF); /* enable only IRQ1 (keyboard) -- polling mode */
	serial_print("after pic_mask\n");

	serial_print("before timer_init\n");
	timer_init(100);
	serial_print("after timer_init\n");
	console_print("[ OK ] PIT timer initialized (100 Hz)\n");

	serial_print("before keyboard_init\n");
	keyboard_init();
	serial_print("after keyboard_init\n");
	console_print("[ OK ] Keyboard initialized\n");

	serial_print("before mouse_init\n");
	mouse_init();
	serial_print("after mouse_init\n");
	pic_mask(0xFC, 0xEF); /* enable IRQ1 + IRQ12 */
	serial_print("after second pic_mask\n");

	bool rootfs_loaded = false;

	if (mbi_addr) {
		struct multiboot_info *mbi = (struct multiboot_info *)mbi_addr;

		if (mbi->flags & 0x8) {
			console_print("[ OK ] Multiboot modules detected\n");

			struct multiboot_module *mods =
			    (struct multiboot_module *)mbi->mods_addr;

			for (unsigned long i = 0; i < mbi->mods_count; ++i) {
				const char *name = (const char *)mods[i].cmdline;
				if (name && kstrstr(name, "rootfs")) {
					console_print("[ INFO ] Mounting rootfs...\n");
					if (fs_init((const void *)mods[i].mod_start,
						    mods[i].mod_end - mods[i].mod_start)) {
						console_print("[ OK ] Rootfs mounted\n");
						if (vfs_init()) {
							console_print("[ OK ] VFS initialized\n");
							rootfs_loaded = true;
						} else {
							console_print("[FAIL] VFS initialization failed\n");
						}
					} else {
						console_print("[FAIL] Rootfs mount failed\n");
					}
					break;
				}
			}

			if (!rootfs_loaded)
				console_print("[WARN] No rootfs module found or failed to mount\n");
		} else {
			console_print("[FAIL] No multiboot modules found\n");
		}
	} else {
		console_print("[FAIL] Invalid multiboot info\n");
	}

	boot_init();
	console_print("[ OK ] Boot manager initialized\n");

	systemd_init();
	console_print("[ OK ] Service manager initialized\n");

	lcp_init();
	console_print("[ OK ] LCP package manager initialized\n\n");

	int sum = add_asm(7, 5);
	char buf[32];
	kitoa(sum, buf, sizeof(buf));
	console_print("[ASM] 7 + 5 = ");
	console_print(buf);
	console_print("\n\n");

	console_print("Launching shell...\n\n");

	shell_run();
}
