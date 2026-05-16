#include "shell.h"
#include "console.h"
#include "keyboard.h"
#include "fs.h"

extern "C" long calc_eval(const char* s);

static int kstrcmp(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) return *a - *b;
        ++a; ++b;
    }
    return *a - *b;
}

static int kstrncmp(const char* a, const char* b, int n) {
    for (int i = 0; i < n; ++i) {
        if (a[i] != b[i]) return (unsigned char)a[i] - (unsigned char)b[i];
        if (!a[i]) return 0;
    }
    return 0;
}

static void print_file_content(const void* data, unsigned int size) {
    const char* bytes = (const char*)data;
    for (unsigned int i = 0; i < size; ++i) {
        console_putchar(bytes[i] ? bytes[i] : '\n');
    }
}

void shell_run() {
    char buf[256];
    while (1) {
        console_print("> ");
        int n = keyboard_readline(buf, sizeof(buf));
        if (n == 0) continue;
        for (int i=0; buf[i]; ++i) if (buf[i]=='=') buf[i] = '+';
        if (kstrcmp(buf, "help") == 0) {
            console_print("help: help clear ls cat calc reboot\n");
            continue;
        }
        if (kstrcmp(buf, "clear") == 0) {
            console_clear();
            continue;
        }
        if (kstrcmp(buf, "ls") == 0) {
            unsigned int count = fs_file_count();
            for (unsigned int i = 0; i < count; ++i) {
                const FSFile* file = fs_file_at(i);
                if (!file) continue;
                console_print(file->name);
                console_print("\n");
            }
            continue;
        }
        if (kstrncmp(buf, "cat ", 4) == 0) {
            const FSFile* file = fs_find(buf + 4);
            if (!file) {
                console_print("File not found\n");
            } else {
                print_file_content(file->data, file->size);
                console_print("\n");
            }
            continue;
        }
        if (kstrcmp(buf, "reboot") == 0) {
            console_print("Rebooting...\n");
            asm volatile ("cli;hlt");
        }
        long res = calc_eval(buf);
        char out[32]; int p=0; long x = res;
        if (x==0) out[p++]='0';
        int neg=0; if (x<0) { neg=1; x=-x; }
        char rev[32]; int rp=0; while (x>0) { rev[rp++]= '0' + (x%10); x/=10; }
        if (neg) out[p++]='-';
        for (int i=rp-1;i>=0;--i) out[p++]=rev[i]; out[p]=0;
        console_print("= "); console_print(out); console_print("\n");
    }
}
