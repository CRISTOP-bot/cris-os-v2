#include "console.h"
#include "fs.h"
#include "shell.h"

extern "C" int add_asm(int a, int b);

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

static const char* kstrstr(const char* haystack, const char* needle) {
    if (!*needle) return haystack;
    while (*haystack) {
        const char* h = haystack;
        const char* n = needle;
        while (*h && *n && *h == *n) { ++h; ++n; }
        if (!*n) return haystack;
        ++haystack;
    }
    return 0;
}

extern "C" void kmain(unsigned long mbi_addr) {
    console_clear();
    console_print("MiniOS - modularizado\n");
    console_print("Type 'help' for commands.\n\n");

    if (mbi_addr) {
        multiboot_info* mbi = (multiboot_info*)mbi_addr;
        if (mbi->flags & 0x8) {
            multiboot_module* mods = (multiboot_module*)mbi->mods_addr;
            for (unsigned long i = 0; i < mbi->mods_count; ++i) {
                const char* name = (const char*)mods[i].cmdline;
                if (name && kstrstr(name, "rootfs")) {
                    if (fs_init((const void*)mods[i].mod_start, mods[i].mod_end - mods[i].mod_start)) {
                        console_print("Rootfs mounted successfully\n");
                    } else {
                        console_print("Rootfs failed to mount\n");
                    }
                    break;
                }
            }
        } else {
            console_print("No multiboot modules found\n");
        }
    }

    int s = add_asm(7,5);
    char buf[32]; int p = 0; int sx = s; if (sx == 0) buf[p++] = '0'; int neg = 0; if (sx < 0) { neg = 1; sx = -sx; }
    char rev[32]; int rp = 0; while (sx > 0) { rev[rp++] = '0' + (sx % 10); sx /= 10; }
    if (neg) buf[p++] = '-'; for (int i = rp-1; i >= 0; --i) buf[p++] = rev[i]; buf[p] = 0;
    console_print("[asm add 7+5= "); console_print(buf); console_print("]\n\n");

    shell_run();
}
