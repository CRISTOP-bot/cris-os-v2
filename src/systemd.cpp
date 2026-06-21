#include "systemd.h"
#include "console.h"
#include "fs.h"
#include <stddef.h>
#include <stdint.h>

static const size_t MAX_UNITS = 16;
static const size_t MAX_UNIT_NAME = 32;
static const size_t MAX_UNIT_DESC = 64;
static const size_t MAX_UNIT_CMD = 128;
static const size_t MAX_UNIT_WANTED = 64;

struct SystemdUnit {
    char name[MAX_UNIT_NAME];
    char description[MAX_UNIT_DESC];
    char exec_start[MAX_UNIT_CMD];
    char wanted_by[MAX_UNIT_WANTED];
    bool active;
};

static SystemdUnit units[MAX_UNITS];
static size_t unit_count = 0;

static const char* sd_trim(const char* s) {
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') {
        ++s;
    }
    return s;
}

static void sd_strncpy(char* dest, const char* src, size_t max_len) {
    size_t i = 0;
    while (i + 1 < max_len && src[i]) {
        dest[i] = src[i];
        ++i;
    }
    dest[i] = '\0';
}

static bool sd_starts_with(const char* s, const char* prefix) {
    while (*prefix) {
        if (*s != *prefix) return false;
        ++s;
        ++prefix;
    }
    return true;
}

static void sd_print(const char* s) {
    console_print(s);
}

static void sd_print_unit(const SystemdUnit* u) {
    sd_print(u->name);
    sd_print(" [");
    sd_print(u->active ? "active" : "inactive");
    sd_print("]");
}

static const char* sd_get_basename(const char* path) {
    const char* last = path;
    const char* p = path;
    while (*p) {
        if (*p == '/' || *p == '\\') {
            last = p + 1;
        }
        ++p;
    }
    return last;
}

static void sd_parse_unit(const FSFile* file) {
    if (!file || unit_count >= MAX_UNITS) return;
    const char* name = sd_get_basename(file->name);
    const char* ext = name;
    while (*ext) {
        if (*ext == '.') break;
        ++ext;
    }
    if (!sd_starts_with(ext, ".service")) return;

    SystemdUnit* unit = &units[unit_count];
    unit->name[0] = '\0';
    unit->description[0] = '\0';
    unit->exec_start[0] = '\0';
    unit->wanted_by[0] = '\0';
    unit->active = false;

    sd_strncpy(unit->name, name, MAX_UNIT_NAME);
    if (unit->name[0] == '\0') return;

    const char* ptr = (const char*)file->data;
    const char* end = ptr + file->size;
    char line[128];

    while (ptr < end) {
        const char* line_start = ptr;
        while (ptr < end && *ptr != '\n') {
            ++ptr;
        }
        size_t len = (size_t)(ptr - line_start);
        if (len >= sizeof(line)) len = sizeof(line) - 1;
        for (size_t i = 0; i < len; ++i) {
            line[i] = line_start[i];
        }
        line[len] = '\0';
        if (ptr < end && *ptr == '\n') {
            ++ptr;
        }
        char* text = line;
        while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') {
            ++text;
        }
        size_t start = (size_t)(text - line);
        size_t tlen = len > start ? len - start : 0;
        while (tlen > 0 && (text[tlen - 1] == ' ' || text[tlen - 1] == '\t' || text[tlen - 1] == '\r' || text[tlen - 1] == '\n')) {
            text[--tlen] = '\0';
        }
        if (text[0] == '\0') continue;
        if (text[0] == '[') {
            continue;
        }
        if (sd_starts_with(text, "Description=")) {
            const char* value = sd_trim(text + 12);
            sd_strncpy(unit->description, value, MAX_UNIT_DESC);
        } else if (sd_starts_with(text, "ExecStart=")) {
            const char* value = sd_trim(text + 10);
            sd_strncpy(unit->exec_start, value, MAX_UNIT_CMD);
        } else if (sd_starts_with(text, "WantedBy=")) {
            const char* value = sd_trim(text + 9);
            sd_strncpy(unit->wanted_by, value, MAX_UNIT_WANTED);
        }
    }

    unit_count += 1;
}

static size_t sd_strlen(const char* s) {
    size_t len = 0;
    while (s[len]) ++len;
    return len;
}

static int sd_strncmp(const char* a, const char* b, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        if (a[i] != b[i]) return (unsigned char)a[i] - (unsigned char)b[i];
        if (!a[i]) return 0;
    }
    return 0;
}

static bool sd_streq(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) return false;
        ++a; ++b;
    }
    return *a == *b;
}

static SystemdUnit* sd_find_unit(const char* name) {
    size_t name_len = sd_strlen(name);
    for (size_t i = 0; i < unit_count; ++i) {
        if (sd_strncmp(units[i].name, name, name_len) == 0 && units[i].name[name_len] == '\0') {
            return &units[i];
        }
    }
    return 0;
}

static void sd_start_unit(SystemdUnit* unit) {
    if (!unit) return;
    if (unit->active) {
        sd_print("Unit already active.");
        return;
    }
    unit->active = true;
    sd_print("Starting ");
    sd_print(unit->name);
    sd_print("\n");
    if (unit->exec_start[0]) {
        sd_print("ExecStart: ");
        sd_print(unit->exec_start);
        sd_print("\n");
    }
}

static void sd_stop_unit(SystemdUnit* unit) {
    if (!unit) return;
    if (!unit->active) {
        sd_print("Unit not active.");
        return;
    }
    unit->active = false;
    sd_print("Stopped ");
    sd_print(unit->name);
    sd_print("\n");
}

static void sd_start_default_target() {
    for (size_t i = 0; i < unit_count; ++i) {
        if (sd_strncmp(units[i].wanted_by, "default.target", 14) == 0) {
            sd_start_unit(&units[i]);
        }
    }
}

bool systemd_init() {
    unit_count = 0;
    size_t count = fs_file_count();
    for (size_t i = 0; i < count; ++i) {
        const FSFile* file = fs_file_at(i);
        if (file && sd_starts_with(file->name, "systemd/")) {
            sd_parse_unit(file);
        }
    }
    sd_start_default_target();
    return true;
}

static void sd_print_help() {
    sd_print("systemctl commands:\n");
    sd_print("  systemctl list-units\n");
    sd_print("  systemctl status <unit>\n");
    sd_print("  systemctl start <unit>\n");
    sd_print("  systemctl stop <unit>\n");
}

static void sd_parse_args(const char* args, char* cmd, char* target) {
    const char* p = sd_trim(args);
    size_t pos = 0;
    while (*p && *p != ' ' && pos + 1 < 32) {
        cmd[pos++] = *p++;
    }
    cmd[pos] = '\0';
    p = sd_trim(p);
    pos = 0;
    while (*p && pos + 1 < MAX_UNIT_NAME) {
        target[pos++] = *p++;
    }
    target[pos] = '\0';
}

int systemd_handle_command(const char* args) {
    char command[32];
    char target[MAX_UNIT_NAME];
    sd_parse_args(args, command, target);
    if (command[0] == '\0' || sd_streq(command, "help")) {
        sd_print_help();
        return 0;
    }
    if (sd_streq(command, "list-units")) {
        for (size_t i = 0; i < unit_count; ++i) {
            sd_print_unit(&units[i]);
            sd_print("\n");
        }
        return 0;
    }
    if (sd_streq(command, "status")) {
        if (target[0] == '\0') {
            sd_print("status requires a unit name\n");
            return 0;
        }
        SystemdUnit* unit = sd_find_unit(target);
        if (!unit) {
            sd_print("Unit not found\n");
            return 0;
        }
        sd_print_unit(unit);
        sd_print("\n");
        sd_print("Description: ");
        sd_print(unit->description[0] ? unit->description : "(none)");
        sd_print("\n");
        sd_print("ExecStart: ");
        sd_print(unit->exec_start[0] ? unit->exec_start : "(none)");
        sd_print("\n");
        return 0;
    }
    if (sd_streq(command, "start")) {
        if (target[0] == '\0') {
            sd_print("start requires a unit name\n");
            return 0;
        }
        SystemdUnit* unit = sd_find_unit(target);
        if (!unit) {
            sd_print("Unit not found\n");
            return 0;
        }
        sd_start_unit(unit);
        return 0;
    }
    if (sd_streq(command, "stop")) {
        if (target[0] == '\0') {
            sd_print("stop requires a unit name\n");
            return 0;
        }
        SystemdUnit* unit = sd_find_unit(target);
        if (!unit) {
            sd_print("Unit not found\n");
            return 0;
        }
        sd_stop_unit(unit);
        return 0;
    }
    sd_print("Unknown systemctl command\n");
    return 0;
}
