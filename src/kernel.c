#include "console.h"
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

void kmain(unsigned long mbi_addr)
{
	console_clear();

	console_print("================================\n");
	console_print("        CrisOS Kernel\n");
	console_print("================================\n\n");

	console_print("[ OK ] Console initialized\n");

	gdt_init();

	idt_init();

	pic_init();
	console_print("[ OK ] PIC initialized (IRQs 0-15 remapped)\n");

	pic_mask(0xFD, 0xFF); /* enable only IRQ1 (keyboard) -- polling mode */

	timer_init(100);
	console_print("[ OK ] PIT timer initialized (100 Hz)\n");

	keyboard_init();
	console_print("[ OK ] Keyboard initialized\n");

	mouse_init();
	pic_mask(0xFC, 0xEF); /* enable IRQ1 + IRQ12 */

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

	while (1)
		asm volatile("hlt");
}
