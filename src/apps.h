#ifndef APPS_H
#define APPS_H

#define SCREEN_W 80
#define SCREEN_H 25

#define COLOR_TITLE   VGA_ATTR(VGA_WHITE, VGA_BLUE)
#define COLOR_BORDER  VGA_ATTR(VGA_LIGHT_GREY, VGA_BLACK)
#define COLOR_STATUS  VGA_ATTR(VGA_BLACK, VGA_LIGHT_GREY)
#define COLOR_ERROR   VGA_ATTR(VGA_LIGHT_RED, VGA_BLACK)
#define COLOR_HIGHLIGHT VGA_ATTR(VGA_WHITE, VGA_BLUE)
#define COLOR_NORMAL  VGA_ATTR(VGA_LIGHT_GREY, VGA_BLACK)
#define COLOR_DIM     VGA_ATTR(VGA_DARK_GREY, VGA_BLACK)
#define COLOR_GREEN   VGA_ATTR(VGA_LIGHT_GREEN, VGA_BLACK)
#define COLOR_CYAN    VGA_ATTR(VGA_LIGHT_CYAN, VGA_BLACK)
#define COLOR_YELLOW  VGA_ATTR(VGA_YELLOW, VGA_BLACK)
#define COLOR_WHITE   VGA_ATTR(VGA_WHITE, VGA_BLACK)

/* Key event codes (scancode | 0x100 for extended keys) */
#define KEY_UP        (0x48 | 0x100)
#define KEY_DOWN      (0x50 | 0x100)
#define KEY_LEFT      (0x4B | 0x100)
#define KEY_RIGHT     (0x4D | 0x100)
#define KEY_HOME      (0x47 | 0x100)
#define KEY_END       (0x4F | 0x100)
#define KEY_PAGEUP    (0x49 | 0x100)
#define KEY_PAGEDOWN  (0x51 | 0x100)
#define KEY_DELETE    (0x53 | 0x100)

/* TUI drawing primitives */
void ui_clear(void);
void ui_put(int x, int y, char c, unsigned char attr);
void ui_put_str(int x, int y, const char *s, unsigned char attr);
void ui_fill(int x, int y, int w, int h, char c, unsigned char attr);
void ui_box(int x, int y, int w, int h, unsigned char attr);
void ui_double_box(int x, int y, int w, int h, unsigned char attr);
void ui_hline(int x, int y, int w, unsigned char attr);
void ui_status_bar(int y, const char *left, const char *right, unsigned char attr);
void ui_title_bar(int x, int y, int w, const char *title, unsigned char attr);

/* Read a key: returns character for normal keys, KEY_* for special keys */
int ui_read_key(void);

/* Read a key with timeout (ticks), returns -1 on timeout */
int ui_read_key_timeout(int ticks);

/* App launcher functions */
void app_nano(const char *filename);
void app_hexview(const char *filename);
void app_sysinfo(void);
void app_filemanager(void);
void app_htop(void);
void app_calc_tui(void);

#endif /* APPS_H */
