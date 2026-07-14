#include "apps.h"
#include "console.h"
#include "keyboard.h"
#include "kstring.h"
#include "calc.h"
#include "timer.h"
#include <stdbool.h>
#include <stdint.h>

#define EXPR_MAX 127
#define HIST_MAX 5
#define GRID_COLS 4
#define GRID_ROWS 5
#define GRID_X 29
#define GRID_Y 5

static const char btn_chars[GRID_ROWS][GRID_COLS] = {
    {'7', '8', '9', '/'},
    {'4', '5', '6', '*'},
    {'1', '2', '3', '-'},
    {'0', '(', ')', '+'},
    { 0,  '=', 0,  'C'},
};

static int btn_y(int row)
{
    return row < 4 ? GRID_Y + row : 10;
}

static int btn_x(int col)
{
    return GRID_X + col * 6;
}

static void draw_button(int row, int col, bool sel)
{
    char ch = btn_chars[row][col];
    if (!ch)
        return;
    int x = btn_x(col);
    int y = btn_y(row);
    unsigned char attr = sel ? COLOR_HIGHLIGHT : COLOR_BORDER;
    ui_put(x, y, '[', attr);
    ui_put(x + 1, y, ' ', attr);
    ui_put(x + 2, y, ch, sel ? COLOR_WHITE : COLOR_GREEN);
    ui_put(x + 3, y, ' ', attr);
    ui_put(x + 4, y, ']', attr);
}

static void draw_grid(int sr, int sc)
{
    for (int r = 0; r < GRID_ROWS; r++)
        for (int c = 0; c < GRID_COLS; c++)
            draw_button(r, c, r == sr && c == sc);
}

static int snap_col(int row, int col)
{
    if (btn_chars[row][col])
        return col;
    for (int d = 1; d < GRID_COLS; d++) {
        if (col - d >= 0 && btn_chars[row][col - d])
            return col - d;
        if (col + d < GRID_COLS && btn_chars[row][col + d])
            return col + d;
    }
    return -1;
}

static void draw_display(const char *expr)
{
    ui_fill(0, 1, SCREEN_W, 2, ' ', COLOR_NORMAL);
    int len = kstrlen(expr);
    int vis = SCREEN_W - 4;
    const char *show = expr;
    if (len > vis)
        show = expr + len - vis;
    int slen = kstrlen(show);
    int x = SCREEN_W - slen - 2;
    if (x < 1)
        x = 1;
    ui_put_str(x, 1, show, COLOR_WHITE);
}

static void draw_result(long res, bool show)
{
    ui_fill(0, 3, SCREEN_W, 1, ' ', COLOR_NORMAL);
    if (!show)
        return;
    char buf[32];
    kitoa(res, buf, sizeof(buf));
    int len = kstrlen(buf);
    int x = SCREEN_W - len - 2;
    if (x < 3)
        x = 3;
    ui_put(x - 2, 3, '=', COLOR_YELLOW);
    ui_put(x - 1, 3, ' ', COLOR_NORMAL);
    ui_put_str(x, 3, buf, COLOR_GREEN);
}

static void draw_hist(char hist[][128], int cnt)
{
    ui_double_box(0, 14, SCREEN_W, 9, COLOR_CYAN);
    ui_title_bar(0, 14, SCREEN_W, " History ", COLOR_CYAN);
    for (int i = 0; i < 5; i++) {
        int y = 15 + i;
        ui_fill(2, y, SCREEN_W - 4, 1, ' ', COLOR_NORMAL);
        if (i < cnt) {
            int idx = cnt - 1 - i;
            ui_put_str(2, y, hist[idx], COLOR_CYAN);
        }
    }
}

static void add_history(char hist[][128], int *cnt, const char *entry)
{
    if (*cnt < HIST_MAX) {
        kstrcpy(hist[*cnt], entry, 128);
        (*cnt)++;
    } else {
        for (int i = 0; i < HIST_MAX - 1; i++)
            kstrcpy(hist[i], hist[i + 1], 128);
        kstrcpy(hist[HIST_MAX - 1], entry, 128);
    }
}

static void do_eval(char *expr, int *expr_len, long *result, bool *has_result,
                    char hist[][128], int *hist_count)
{
    if (*expr_len == 0)
        return;
    *result = calc_eval(expr);
    *has_result = true;
    draw_result(*result, true);
    char entry[128];
    char rbuf[32];
    kstrcpy(entry, expr, sizeof(entry));
    kstrcat(entry, " = ", sizeof(entry));
    kitoa(*result, rbuf, sizeof(rbuf));
    kstrcat(entry, rbuf, sizeof(entry));
    add_history(hist, hist_count, entry);
    draw_hist(hist, *hist_count);
}

void app_calc_tui(void)
{
    char expr[EXPR_MAX + 1];
    int expr_len = 0;
    expr[0] = '\0';

    char history[HIST_MAX][128];
    int history_count = 0;

    int sel_row = 0, sel_col = 0;
    long result = 0;
    bool has_result = false;

    ui_clear();
    ui_title_bar(0, 0, SCREEN_W, " NucleOS Calculator ", COLOR_TITLE);
    ui_hline(0, 4, SCREEN_W, COLOR_BORDER);
    draw_grid(sel_row, sel_col);
    ui_hline(0, 12, SCREEN_W, COLOR_BORDER);
    ui_hline(0, 13, SCREEN_W, COLOR_BORDER);
    draw_hist(history, history_count);
    ui_status_bar(23, "NucleOS Calculator", "INT MODE", COLOR_STATUS);
    ui_fill(0, 24, SCREEN_W, 1, ' ', COLOR_STATUS);
    ui_put_str(1, 24, "Type expression Enter:= Bksp:Del C:Clear Q:Quit",
               COLOR_STATUS);
    draw_display(expr);

    while (1) {
        int key = ui_read_key();
        bool eval = false;

        if (key == 'q' || key == 'Q')
            break;

        if (key == '\n') {
            char bc = btn_chars[sel_row][sel_col];
            if (bc == 'C') {
                expr_len = 0;
                expr[0] = '\0';
                has_result = false;
                draw_display(expr);
                draw_result(0, false);
                continue;
            }
            if (bc && bc != '=') {
                if (expr_len < EXPR_MAX) {
                    expr[expr_len++] = bc;
                    expr[expr_len] = '\0';
                    draw_display(expr);
                    has_result = false;
                }
                continue;
            }
            eval = true;
        } else if (key == '=') {
            eval = true;
        } else if (key == '\b') {
            if (expr_len > 0) {
                expr[--expr_len] = '\0';
                draw_display(expr);
                has_result = false;
                draw_result(0, false);
            }
            continue;
        } else if (key == 'c' || key == 'C') {
            expr_len = 0;
            expr[0] = '\0';
            has_result = false;
            draw_display(expr);
            draw_result(0, false);
            continue;
        } else if (key == KEY_UP) {
            int nr = sel_row - 1;
            if (nr >= 0) {
                int nc = snap_col(nr, sel_col);
                if (nc >= 0) {
                    sel_row = nr;
                    sel_col = nc;
                }
            }
            draw_grid(sel_row, sel_col);
            continue;
        } else if (key == KEY_DOWN) {
            int nr = sel_row + 1;
            if (nr < GRID_ROWS) {
                int nc = snap_col(nr, sel_col);
                if (nc >= 0) {
                    sel_row = nr;
                    sel_col = nc;
                }
            }
            draw_grid(sel_row, sel_col);
            continue;
        } else if (key == KEY_LEFT) {
            int nc = sel_col - 1;
            while (nc >= 0 && !btn_chars[sel_row][nc])
                nc--;
            if (nc >= 0)
                sel_col = nc;
            draw_grid(sel_row, sel_col);
            continue;
        } else if (key == KEY_RIGHT) {
            int nc = sel_col + 1;
            while (nc < GRID_COLS && !btn_chars[sel_row][nc])
                nc++;
            if (nc < GRID_COLS)
                sel_col = nc;
            draw_grid(sel_row, sel_col);
            continue;
        } else {
            if (expr_len < EXPR_MAX) {
                char c = (char)key;
                if ((c >= '0' && c <= '9') || c == '+' || c == '-' ||
                    c == '*' || c == '/' || c == '(' || c == ')') {
                    expr[expr_len++] = c;
                    expr[expr_len] = '\0';
                    draw_display(expr);
                    has_result = false;
                }
            }
            continue;
        }

        if (eval)
            do_eval(expr, &expr_len, &result, &has_result,
                    history, &history_count);
    }

    ui_clear();
}
