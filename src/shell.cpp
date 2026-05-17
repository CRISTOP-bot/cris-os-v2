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

static int kstrcmp(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) return *a - *b;
        ++a; ++b;
    }
    return *a - *b;
}

static const char* skip_spaces(const char* s) {
    while (*s == ' ' || *s == '\t') ++s;
    return s;
}

static const char* parse_token(const char* s, char* out, int maxlen) {
    s = skip_spaces(s);
    int pos = 0;
    while (*s && *s != ' ' && *s != '\t' && pos + 1 < maxlen) {
        out[pos++] = *s++;
    }
    out[pos] = '\0';
    return s;
}

static void copy_rest(const char* s, char* out, int maxlen) {
    s = skip_spaces(s);
    int pos = 0;
    while (*s && pos + 1 < maxlen) {
        out[pos++] = *s++;
    }
    out[pos] = '\0';
}

static int parse_int(const char* s) {
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

static int kstrlen(const char* s) {
    int len = 0;
    while (s[len]) ++len;
    return len;
}

static bool grep_file(const char* path, const char* pattern) {
    const void* data = vfs_read(path);
    size_t size = vfs_get_size(path);
    if (!data || size == 0) return false;
    const char* text = (const char*)data;
    int line = 1;
    const char* p = text;
    while ((size_t)(p - text) < size) {
        const char* line_start = p;
        while ((size_t)(p - text) < size && *p != '\n') {
            ++p;
        }
        int line_len = (int)(p - line_start);
        for (int i = 0; i + (int)kstrlen(pattern) <= line_len; ++i) {
            bool ok = true;
            for (int j = 0; pattern[j]; ++j) {
                if (line_start[i + j] != pattern[j]) {
                    ok = false;
                    break;
                }
            }
            if (ok) {
                char number[16];
                int np = 0;
                int value = line;
                if (value == 0) {
                    number[np++] = '0';
                } else {
                    char rev[16];
                    int rp = 0;
                    while (value > 0) {
                        rev[rp++] = '0' + (value % 10);
                        value /= 10;
                    }
                    for (int k = rp - 1; k >= 0; --k) {
                        number[np++] = rev[k];
                    }
                }
                number[np] = '\0';
                console_print(number);
                console_print(": ");
                for (int k = 0; k < line_len; ++k) {
                    console_putchar(line_start[k]);
                }
                console_print("\n");
                break;
            }
        }
        if ((size_t)(p - text) < size && *p == '\n') ++p;
        ++line;
    }
    return true;
}

static void handle_asm_command(const char* args) {
    char op[16] = {0};
    char first[32] = {0};
    char second[32] = {0};
    const char* p = args;
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
    if (kstrcmp(op, "add") == 0) {
        result = add_asm(a, b);
    } else if (kstrcmp(op, "sub") == 0) {
        result = sub_asm(a, b);
    } else if (kstrcmp(op, "mul") == 0) {
        result = mul_asm(a, b);
    } else if (kstrcmp(op, "div") == 0) {
        result = div_asm(a, b);
    } else {
        console_print("Unknown asm op. Use add, sub, mul, div\n");
        return;
    }
    char out[32];
    int pnum = 0;
    long x = result;
    if (x == 0) {
        out[pnum++] = '0';
    }
    int neg = 0;
    if (x < 0) {
        neg = 1;
        x = -x;
    }
    char rev[32];
    int rp = 0;
    while (x > 0) {
        rev[rp++] = '0' + (x % 10);
        x /= 10;
    }
    if (neg) out[pnum++] = '-';
    for (int i = rp - 1; i >= 0; --i) out[pnum++] = rev[i];
    out[pnum] = '\0';
    console_print("= ");
    console_print(out);
    console_print("\n");
}

static void shell_echo(const char* args) {
    char target[128] = {0};
    char value[192] = {0};
    const char* p = args;
    int vi = 0;
    while (*p && !(*p == '>' && p[1] != '>') && vi + 1 < (int)sizeof(value)) {
        value[vi++] = *p++;
    }
    value[vi] = '\0';
    p = skip_spaces(p);
    if (*p == '>') {
        ++p;
        p = skip_spaces(p);
        int ti = 0;
        while (*p && *p != ' ' && ti + 1 < (int)sizeof(target)) {
            target[ti++] = *p++;
        }
        target[ti] = '\0';
    }
    if (target[0]) {
        vfs_write(target, value, kstrlen(value));
    } else {
        console_print(value);
        console_print("\n");
    }
}

void shell_run() {
    char buf[256];
    while (1) {
        console_print("> ");
        int n = keyboard_readline(buf, sizeof(buf));
        if (n == 0) continue;
        for (int i = 0; buf[i]; ++i) if (buf[i] == '=') buf[i] = '+';

        char cmd[32] = {0};
        char arg1[128] = {0};
        char arg2[128] = {0};
        char rest[192] = {0};
        const char* p = buf;
        p = parse_token(p, cmd, sizeof(cmd));
        const char* tail = p;
        p = parse_token(p, arg1, sizeof(arg1));
        p = parse_token(p, arg2, sizeof(arg2));
        copy_rest(tail, rest, sizeof(rest));

        if (kstrcmp(cmd, "help") == 0) {
            console_print("help: help clear ls pwd cd mkdir rmdir rm touch cp mv cat grep echo uname whoami df stat panic reboot lcp systemctl bootctl gui asm calc\n");
            continue;
        }
        if (kstrcmp(cmd, "clear") == 0) {
            console_clear();
            continue;
        }
        if (kstrcmp(cmd, "pwd") == 0) {
            console_print(vfs_pwd());
            console_print("\n");
            continue;
        }
        if (kstrcmp(cmd, "cd") == 0) {
            if (arg1[0] == '\0') {
                console_print("Usage: cd <path>\n");
            } else if (!vfs_cd(arg1)) {
                console_print("Directory not found\n");
            }
            continue;
        }
        if (kstrcmp(cmd, "ls") == 0) {
            const char* path = arg1[0] ? arg1 : 0;
            if (!vfs_list(path)) {
                console_print("Invalid path\n");
            }
            continue;
        }
        if (kstrcmp(cmd, "mkdir") == 0) {
            if (arg1[0] == '\0') {
                console_print("Usage: mkdir <path>\n");
            } else if (!vfs_mkdir(arg1)) {
                console_print("Cannot create directory\n");
            }
            continue;
        }
        if (kstrcmp(cmd, "rmdir") == 0) {
            if (arg1[0] == '\0') {
                console_print("Usage: rmdir <path>\n");
            } else if (!vfs_rmdir(arg1)) {
                console_print("Cannot remove directory\n");
            }
            continue;
        }
        if (kstrcmp(cmd, "rm") == 0) {
            if (arg1[0] == '\0') {
                console_print("Usage: rm <path>\n");
            } else if (!vfs_remove(arg1)) {
                console_print("Cannot remove file\n");
            }
            continue;
        }
        if (kstrcmp(cmd, "touch") == 0) {
            if (arg1[0] == '\0') {
                console_print("Usage: touch <path>\n");
            } else if (!vfs_touch(arg1)) {
                console_print("Cannot touch file\n");
            }
            continue;
        }
        if (kstrcmp(cmd, "cp") == 0) {
            if (arg1[0] == '\0' || arg2[0] == '\0') {
                console_print("Usage: cp <src> <dst>\n");
            } else if (!vfs_cp(arg1, arg2)) {
                console_print("Copy failed\n");
            }
            continue;
        }
        if (kstrcmp(cmd, "mv") == 0) {
            if (arg1[0] == '\0' || arg2[0] == '\0') {
                console_print("Usage: mv <src> <dst>\n");
            } else if (!vfs_mv(arg1, arg2)) {
                console_print("Move failed\n");
            }
            continue;
        }
        if (kstrcmp(cmd, "cat") == 0) {
            if (arg1[0] == '\0') {
                console_print("Usage: cat <file>\n");
            } else if (!vfs_cat(arg1)) {
                console_print("File not found or is a directory\n");
            }
            continue;
        }
        if (kstrcmp(cmd, "grep") == 0) {
            if (arg1[0] == '\0' || arg2[0] == '\0') {
                console_print("Usage: grep <pattern> <file>\n");
            } else if (!grep_file(arg2, arg1)) {
                console_print("Pattern search failed\n");
            }
            continue;
        }
        if (kstrcmp(cmd, "echo") == 0) {
            shell_echo(rest);
            continue;
        }
        if (kstrcmp(cmd, "uname") == 0) {
            console_print("Linux on CrisOS\n");
            continue;
        }
        if (kstrcmp(cmd, "whoami") == 0) {
            console_print("cris\n");
            continue;
        }
        if (kstrcmp(cmd, "df") == 0) {
            console_print("Filesystem 64K-blocks Used Avail Mounted on\n");
            console_print("rootfs      64      8    56    /\n");
            continue;
        }
        if (kstrcmp(cmd, "stat") == 0) {
            if (arg1[0] == '\0') {
                console_print("Usage: stat <file>\n");
            } else if (!vfs_exists(arg1)) {
                console_print("No such file or directory\n");
            } else {
                size_t size = vfs_get_size(arg1);
                console_print(arg1);
                console_print(" - size: ");
                char num[32];
                int np = 0;
                int value = (int)size;
                if (value == 0) {
                    num[np++] = '0';
                } else {
                    char rev[32];
                    int rp = 0;
                    while (value > 0) {
                        rev[rp++] = '0' + (value % 10);
                        value /= 10;
                    }
                    for (int i = rp - 1; i >= 0; --i) num[np++] = rev[i];
                }
                num[np] = '\0';
                console_print(num);
                console_print(" bytes\n");
            }
            continue;
        }
        if (kstrcmp(cmd, "reboot") == 0) {
            console_print("Rebooting...\n");
            halt_cpu();
        }
        if (kstrcmp(cmd, "panic") == 0) {
            if (rest[0] == '\0') {
                kernel_panic("Kernel panic triggered from shell.");
            } else {
                kernel_panic(rest);
            }
        }
        if (kstrcmp(cmd, "lcp") == 0) {
            lcp_handle_command("");
            continue;
        }
        if (kstrcmp(cmd, "systemctl") == 0) {
            systemd_handle_command(arg1);
            continue;
        }
        if (kstrcmp(cmd, "bootctl") == 0) {
            boot_handle_command(arg1);
            continue;
        }
        if (kstrcmp(cmd, "gui") == 0) {
            gui_show();
            continue;
        }
        if (kstrcmp(cmd, "asm") == 0) {
            handle_asm_command(rest);
            continue;
        }
        if (kstrcmp(cmd, "calc") == 0) {
            calc_app(rest);
            continue;
        }
        console_print("Unknown command. Type 'help' for a list of commands.\n");
    }
}
