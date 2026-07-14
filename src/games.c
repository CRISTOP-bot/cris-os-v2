#include "games.h"
#include "console.h"
#include "keyboard.h"
#include "timer.h"
#include "kstring.h"
#include "asm.h"

static unsigned int rng_state;

static unsigned int rng_next(void)
{
	rng_state = rng_state * 1103515245 + 12345;
	return (rng_state >> 16) & 0x7FFF;
}

static void rng_seed(void)
{
	rng_state = (unsigned int)timer_get_ticks();
	if (rng_state == 0)
		rng_state = 0xDEADBEEF;
}

static int scancode_to_digit(int sc)
{
	switch (sc) {
	case SC_1: return 1;
	case SC_2: return 2;
	case SC_3: return 3;
	case SC_4: return 4;
	case SC_5: return 5;
	case SC_6: return 6;
	case SC_7: return 7;
	case SC_8: return 8;
	case SC_9: return 9;
	default:  return -1;
	}
}

static void wait_frame(int ticks)
{
	unsigned long end = timer_get_ticks() + ticks;
	while (timer_get_ticks() < end) {
		if (keyboard_data_available())
			break;
	}
}

/* ===================== SNAKE ===================== */

#define SNAKE_W 40
#define SNAKE_H 20
#define SNAKE_MAX 128

static int snake_field[SNAKE_H][SNAKE_W];
static int snake_x[SNAKE_MAX], snake_y[SNAKE_MAX];
static int snake_len;
static int snake_dir;
static int snake_food_x, snake_food_y;
static int snake_score;
static int snake_speed;

enum { DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT };

static void snake_place_food(void)
{
	while (1) {
		snake_food_x = rng_next() % (SNAKE_W - 2) + 1;
		snake_food_y = rng_next() % (SNAKE_H - 2) + 1;
		if (snake_field[snake_food_y][snake_food_x] == 0)
			break;
	}
}

static void snake_draw(void)
{
	for (int y = 0; y < SNAKE_H; ++y) {
		for (int x = 0; x < SNAKE_W; ++x) {
			char c;
			unsigned char attr;
			if (y == 0 || y == SNAKE_H - 1 || x == 0 || x == SNAKE_W - 1) {
				c = '#';
				attr = VGA_ATTR(VGA_DARK_GREY, VGA_BLACK);
			} else if (x == snake_food_x && y == snake_food_y) {
				c = '*';
				attr = VGA_ATTR(VGA_LIGHT_RED, VGA_BLACK);
			} else if (snake_field[y][x] > 0) {
				c = '@';
				attr = VGA_ATTR(VGA_LIGHT_GREEN, VGA_BLACK);
			} else {
				c = ' ';
				attr = VGA_DEFAULT_ATTR;
			}
			console_putxy(x + 1, y + 1, c, attr);
		}
	}
	char score_buf[32];
	char num[16];
	kstrcpy(score_buf, "Score: ", sizeof(score_buf));
	kitoa(snake_score, num, sizeof(num));
	kstrcat(score_buf, num, sizeof(score_buf));
	console_print_at(1, 0, score_buf, VGA_ATTR(VGA_YELLOW, VGA_BLACK));
	console_print_at(SNAKE_W + 3, 1, "WASD=Move Q=Quit", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
}

static void snake_init(void)
{
	snake_len = 4;
	snake_dir = DIR_RIGHT;
	snake_score = 0;
	snake_speed = 8;
	kmemset(snake_field, 0, sizeof(snake_field));
	for (int i = 0; i < snake_len; ++i) {
		snake_x[i] = SNAKE_W / 2 - i;
		snake_y[i] = SNAKE_H / 2;
		snake_field[snake_y[i]][snake_x[i]] = 1;
	}
	snake_place_food();
}

static void game_snake(void)
{
	console_clear();
	rng_seed();
	snake_init();
	int alive = 1;
	while (alive) {
		snake_draw();
		wait_frame(snake_speed);
		if (keyboard_data_available()) {
			int sc = keyboard_read_scancode();
			if (sc == SC_Q || sc == SC_ESCAPE) break;
			if (sc == SC_W && snake_dir != DIR_DOWN)  snake_dir = DIR_UP;
			if (sc == SC_S && snake_dir != DIR_UP)    snake_dir = DIR_DOWN;
			if (sc == SC_A && snake_dir != DIR_RIGHT) snake_dir = DIR_LEFT;
			if (sc == SC_D && snake_dir != DIR_LEFT)  snake_dir = DIR_RIGHT;
			if (sc == SC_P) snake_speed = (snake_speed > 3) ? snake_speed - 2 : 2;
		}
		int nx = snake_x[0], ny = snake_y[0];
		if (snake_dir == DIR_UP)    ny--;
		if (snake_dir == DIR_DOWN)  ny++;
		if (snake_dir == DIR_LEFT)  nx--;
		if (snake_dir == DIR_RIGHT) nx++;
		if (nx <= 0 || nx >= SNAKE_W - 1 || ny <= 0 || ny >= SNAKE_H - 1) {
			alive = 0;
			break;
		}
		if (snake_field[ny][nx] > 0) {
			alive = 0;
			break;
		}
		if (nx == snake_food_x && ny == snake_food_y) {
			snake_score += 10;
			if (snake_len < SNAKE_MAX) {
				snake_x[snake_len] = snake_x[snake_len - 1];
				snake_y[snake_len] = snake_y[snake_len - 1];
				snake_len++;
			}
			snake_place_food();
			if (snake_speed > 3) snake_speed--;
		}
		for (int i = snake_len - 1; i > 0; --i) {
			snake_x[i] = snake_x[i - 1];
			snake_y[i] = snake_y[i - 1];
		}
		snake_x[0] = nx;
		snake_y[0] = ny;
		kmemset(snake_field, 0, sizeof(snake_field));
		for (int i = 0; i < snake_len; ++i)
			snake_field[snake_y[i]][snake_x[i]] = 1;
	}
	snake_draw();
	char msg[64];
	char num[16];
	kstrcpy(msg, "GAME OVER! Score: ", sizeof(msg));
	kitoa(snake_score, num, sizeof(num));
	kstrcat(msg, num, sizeof(msg));
	console_print_at(10, SNAKE_H / 2 + 1, msg, VGA_ATTR(VGA_LIGHT_RED, VGA_BLACK));
	console_print_at(10, SNAKE_H / 2 + 2, "Press any key...", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
	keyboard_read_char();
}

/* ===================== TETRIS ===================== */

#define TET_W 12
#define TET_H 22
#define TET_PIECES 7

static unsigned char tetris_field[TET_H][TET_W];
static int tet_piece, tet_rot, tet_px, tet_py;
static int tet_next;
static int tet_score, tet_lines;

static const unsigned char tetrominoes[TET_PIECES][4][4] = {
	{{1,1,1,1},{0,0,0,0},{0,0,0,0},{0,0,0,0}},
	{{1,0,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
	{{0,0,1,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
	{{1,1,0,0},{1,1,0,0},{0,0,0,0},{0,0,0,0}},
	{{0,1,1,0},{1,1,0,0},{0,0,0,0},{0,0,0,0}},
	{{0,1,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
	{{1,1,0,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}},
};

static int tet_size(void)
{
	if (tet_piece == 3) return 2;
	return (tet_piece == 0) ? 4 : 3;
}

static int tet_collides(int piece, int rot, int px, int py)
{
	int sz = (piece == 3) ? 2 : ((piece == 0) ? 4 : 3);
	int r = rot % 4;
	for (int i = 0; i < sz; ++i)
		for (int j = 0; j < sz; ++j) {
			int rx = j, ry = i;
			for (int k = 0; k < r; ++k) {
				int tmp = rx;
				rx = sz - 1 - ry;
				ry = tmp;
			}
			if (!tetrominoes[piece][i][j])
				continue;
			int fx = px + rx, fy = py + ry;
			if (fx < 0 || fx >= TET_W || fy >= TET_H)
				return 1;
			if (fy >= 0 && tetris_field[fy][fx])
				return 1;
		}
	return 0;
}

static void tet_lock(void)
{
	int sz = tet_size();
	int r = tet_rot % 4;
	for (int i = 0; i < sz; ++i)
		for (int j = 0; j < sz; ++j) {
			int rx = j, ry = i;
			for (int k = 0; k < r; ++k) {
				int tmp = rx;
				rx = sz - 1 - ry;
				ry = tmp;
			}
			if (!tetrominoes[tet_piece][i][j])
				continue;
			int fx = tet_px + rx, fy = tet_py + ry;
			if (fy >= 0 && fy < TET_H && fx >= 0 && fx < TET_W)
				tetris_field[fy][fx] = 1;
		}
}

static void tet_clear_lines(void)
{
	int cleared = 0;
	for (int y = TET_H - 1; y >= 0; --y) {
		int full = 1;
		for (int x = 0; x < TET_W; ++x)
			if (!tetris_field[y][x]) { full = 0; break; }
		if (full) {
			cleared++;
			for (int yy = y; yy > 0; --yy)
				for (int x = 0; x < TET_W; ++x)
					tetris_field[yy][x] = tetris_field[yy - 1][x];
			for (int x = 0; x < TET_W; ++x)
				tetris_field[0][x] = 0;
			y++;
		}
	}
	if (cleared == 1) tet_score += 100;
	else if (cleared == 2) tet_score += 300;
	else if (cleared == 3) tet_score += 500;
	else if (cleared >= 4) tet_score += 800;
	tet_lines += cleared;
}

static void tet_spawn(void)
{
	tet_piece = tet_next;
	tet_next = rng_next() % TET_PIECES;
	tet_rot = 0;
	tet_px = TET_W / 2 - tet_size() / 2;
	tet_py = 0;
	if (tet_collides(tet_piece, tet_rot, tet_px, tet_py))
		tet_py = -1;
}

static void tetris_draw(void)
{
	for (int y = 0; y < TET_H; ++y) {
		for (int x = 0; x < TET_W; ++x) {
			char c = ' ';
			unsigned char attr = VGA_DEFAULT_ATTR;
			if (y == 0 || y == TET_H - 1 || x == 0 || x == TET_W - 1) {
				c = '#';
				attr = VGA_ATTR(VGA_DARK_GREY, VGA_BLACK);
			} else if (tetris_field[y][x]) {
				c = 0xDB;
				attr = VGA_ATTR(VGA_LIGHT_CYAN, VGA_BLACK);
			}
			console_putxy(x, y, c, attr);
		}
	}
	int sz = tet_size();
	int r = tet_rot % 4;
	for (int i = 0; i < sz; ++i)
		for (int j = 0; j < sz; ++j) {
			int rx = j, ry = i;
			for (int k = 0; k < r; ++k) {
				int tmp = rx;
				rx = sz - 1 - ry;
				ry = tmp;
			}
			if (!tetrominoes[tet_piece][i][j])
				continue;
			int dx = tet_px + rx, dy = tet_py + ry;
			if (dx > 0 && dx < TET_W && dy >= 0 && dy < TET_H)
				console_putxy(dx, dy, 0xDB, VGA_ATTR(VGA_LIGHT_GREEN, VGA_BLACK));
		}
	char info[32];
	char num[16];
	kstrcpy(info, "Score: ", sizeof(info));
	kitoa(tet_score, num, sizeof(num));
	kstrcat(info, num, sizeof(info));
	console_print_at(TET_W + 2, 1, info, VGA_ATTR(VGA_YELLOW, VGA_BLACK));
	kstrcpy(info, "Lines: ", sizeof(info));
	kitoa(tet_lines, num, sizeof(num));
	kstrcat(info, num, sizeof(info));
	console_print_at(TET_W + 2, 2, info, VGA_ATTR(VGA_YELLOW, VGA_BLACK));
	console_print_at(TET_W + 2, 4, "Controls:", VGA_ATTR(VGA_WHITE, VGA_BLACK));
	console_print_at(TET_W + 2, 5, "A/D=Move", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
	console_print_at(TET_W + 2, 6, "W=Rotate", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
	console_print_at(TET_W + 2, 7, "S=SoftDrop", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
	console_print_at(TET_W + 2, 8, "Space=Drop", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
	console_print_at(TET_W + 2, 9, "Q=Quit", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
}

static void game_tetris(void)
{
	console_clear();
	rng_seed();
	kmemset(tetris_field, 0, sizeof(tetris_field));
	tet_score = 0;
	tet_lines = 0;
	tet_next = rng_next() % TET_PIECES;
	tet_spawn();
	int running = 1;
	int drop_timer = 0;
	int drop_speed = 15;
	while (running) {
		tetris_draw();
		drop_timer++;
		if (drop_timer >= drop_speed) {
			drop_timer = 0;
			if (!tet_collides(tet_piece, tet_rot, tet_px, tet_py + 1)) {
				tet_py++;
			} else {
				tet_lock();
				tet_clear_lines();
				tet_spawn();
				if (tet_collides(tet_piece, tet_rot, tet_px, tet_py)) {
					running = 0;
				}
			}
		}
		wait_frame(2);
		if (keyboard_data_available()) {
			int sc = keyboard_read_scancode();
			if (sc == SC_Q || sc == SC_ESCAPE) break;
			if (sc == SC_A) {
				if (!tet_collides(tet_piece, tet_rot, tet_px - 1, tet_py))
					tet_px--;
			}
			if (sc == SC_D) {
				if (!tet_collides(tet_piece, tet_rot, tet_px + 1, tet_py))
					tet_px++;
			}
			if (sc == SC_W) {
				if (!tet_collides(tet_piece, tet_rot + 1, tet_px, tet_py))
					tet_rot++;
			}
			if (sc == SC_S) {
				if (!tet_collides(tet_piece, tet_rot, tet_px, tet_py + 1))
					tet_py++;
			}
			if (sc == SC_SPACE) {
				while (!tet_collides(tet_piece, tet_rot, tet_px, tet_py + 1))
					tet_py++;
				tet_lock();
				tet_clear_lines();
				tet_spawn();
				if (tet_collides(tet_piece, tet_rot, tet_px, tet_py))
					running = 0;
			}
			if (sc == SC_P)
				drop_speed = (drop_speed > 5) ? drop_speed - 2 : 3;
		}
	}
	console_print_at(TET_W / 2 - 4, TET_H / 2, "GAME OVER!", VGA_ATTR(VGA_LIGHT_RED, VGA_BLACK));
	console_print_at(TET_W / 2 - 7, TET_H / 2 + 1, "Press any key...", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
	keyboard_read_char();
}

/* ===================== PONG ===================== */

#define PONG_W 40
#define PONG_H 20
#define PADDLE_H 4

static int pong_py, pong_ay;
static int pong_bx, pong_by;
static int pong_bdx, pong_bdy;
static int pong_ps, pong_as;

static void pong_reset(void)
{
	pong_py = PONG_H / 2 - PADDLE_H / 2;
	pong_ay = PONG_H / 2 - PADDLE_H / 2;
	pong_bx = PONG_W / 2;
	pong_by = PONG_H / 2;
	pong_bdx = 1;
	pong_bdy = 1;
}

static void pong_draw(void)
{
	for (int y = 0; y <= PONG_H; ++y) {
		for (int x = 0; x <= PONG_W; ++x) {
			char c = ' ';
			unsigned char attr = VGA_DEFAULT_ATTR;
			if (x == 0 || x == PONG_W) {
				c = '|';
				attr = VGA_ATTR(VGA_DARK_GREY, VGA_BLACK);
			} else if (y == 0 || y == PONG_H) {
				c = '-';
				attr = VGA_ATTR(VGA_DARK_GREY, VGA_BLACK);
			} else if (x == 1 && y >= pong_py && y < pong_py + PADDLE_H) {
				c = 0xDB;
				attr = VGA_ATTR(VGA_LIGHT_BLUE, VGA_BLACK);
			} else if (x == PONG_W - 1 && y >= pong_ay && y < pong_ay + PADDLE_H) {
				c = 0xDB;
				attr = VGA_ATTR(VGA_LIGHT_RED, VGA_BLACK);
			} else if (x == pong_bx && y == pong_by) {
				c = 'O';
				attr = VGA_ATTR(VGA_WHITE, VGA_BLACK);
			}
			console_putxy(x + 20, y + 2, c, attr);
		}
	}
	char score_l[32], score_r[32];
	char num1[8], num2[8];
	kitoa(pong_ps, num1, sizeof(num1));
	kitoa(pong_as, num2, sizeof(num2));
	kstrcpy(score_l, "Player: ", sizeof(score_l));
	kstrcat(score_l, num1, sizeof(score_l));
	kstrcpy(score_r, "CPU: ", sizeof(score_r));
	kstrcat(score_r, num2, sizeof(score_r));
	console_print_at(22, 1, score_l, VGA_ATTR(VGA_LIGHT_BLUE, VGA_BLACK));
	console_print_at(52, 1, score_r, VGA_ATTR(VGA_LIGHT_RED, VGA_BLACK));
	console_print_at(0, 0, "PONG  W/S=Move Q=Quit", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
}

static void game_pong(void)
{
	console_clear();
	pong_ps = 0;
	pong_as = 0;
	pong_reset();
	int running = 1;
	while (running) {
		pong_draw();
		wait_frame(4);
		if (keyboard_data_available()) {
			int sc = keyboard_read_scancode();
			if (sc == SC_Q || sc == SC_ESCAPE) break;
			if (sc == SC_W && pong_py > 1) pong_py--;
			if (sc == SC_S && pong_py < PONG_H - PADDLE_H - 1) pong_py++;
		}
		if (pong_ay + PADDLE_H / 2 < pong_by)
			pong_ay++;
		else if (pong_ay + PADDLE_H / 2 > pong_by)
			pong_ay--;
		if (pong_ay < 1) pong_ay = 1;
		if (pong_ay > PONG_H - PADDLE_H - 1) pong_ay = PONG_H - PADDLE_H - 1;
		pong_bx += pong_bdx;
		pong_by += pong_bdy;
		if (pong_by <= 1 || pong_by >= PONG_H - 1)
			pong_bdy = -pong_bdy;
		if (pong_bx == 2 && pong_by >= pong_py && pong_by < pong_py + PADDLE_H) {
			pong_bdx = 1;
			pong_bdy = (pong_by - pong_py - PADDLE_H / 2);
		}
		if (pong_bx == PONG_W - 2 && pong_by >= pong_ay && pong_by < pong_ay + PADDLE_H) {
			pong_bdx = -1;
			pong_bdy = (pong_by - pong_ay - PADDLE_H / 2);
		}
		if (pong_bx <= 0) {
			pong_as++;
			pong_reset();
		}
		if (pong_bx >= PONG_W) {
			pong_ps++;
			pong_reset();
		}
		if (pong_ps >= 10 || pong_as >= 10)
			running = 0;
	}
	pong_draw();
	const char *winner = (pong_ps >= 10) ? "PLAYER WINS!" : "CPU WINS!";
	console_print_at(30, 12, winner, VGA_ATTR(VGA_YELLOW, VGA_BLACK));
	console_print_at(28, 13, "Press any key...", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
	keyboard_read_char();
}

/* ===================== 2048 ===================== */

static int game2048_grid[4][4];
static int game2048_score;
static int game2048_best;

static void game2048_spawn(void)
{
	int empty[16];
	int count = 0;
	for (int y = 0; y < 4; ++y)
		for (int x = 0; x < 4; ++x)
			if (game2048_grid[y][x] == 0)
				empty[count++] = y * 4 + x;
	if (count == 0) return;
	int idx = rng_next() % count;
	int v = (rng_next() % 10 < 9) ? 2 : 4;
	game2048_grid[empty[idx] / 4][empty[idx] % 4] = v;
}

static void game2048_draw(void)
{
	console_print_at(0, 0, "2048  Arrows=Move R=Restart Q=Quit", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
	char score_buf[32];
	char num[16];
	kstrcpy(score_buf, "Score: ", sizeof(score_buf));
	kitoa(game2048_score, num, sizeof(num));
	kstrcat(score_buf, num, sizeof(score_buf));
	console_print_at(0, 1, score_buf, VGA_ATTR(VGA_YELLOW, VGA_BLACK));
	kstrcpy(score_buf, "Best:  ", sizeof(score_buf));
	kitoa(game2048_best, num, sizeof(num));
	kstrcat(score_buf, num, sizeof(score_buf));
	console_print_at(25, 1, score_buf, VGA_ATTR(VGA_CYAN, VGA_BLACK));
	for (int y = 0; y < 4; ++y) {
		for (int x = 0; x < 4; ++x) {
			int bx = x * 6 + 1;
			int by = y * 2 + 3;
			console_print_at(bx, by, "+----+", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
			console_print_at(bx, by + 1, "|    |", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
			int val = game2048_grid[y][x];
			if (val > 0) {
				kitoa(val, num, sizeof(num));
				int nlen = kstrlen(num);
				int pad = (4 - nlen) / 2;
				char cell[6];
				int ci = 0;
				for (int p = 0; p < pad; ++p) cell[ci++] = ' ';
				for (int p = 0; num[p]; ++p) cell[ci++] = num[p];
				while (ci < 4) cell[ci++] = ' ';
				cell[ci] = '\0';
				unsigned char attr = VGA_ATTR(VGA_WHITE, VGA_BLACK);
				if (val == 2)    attr = VGA_ATTR(VGA_LIGHT_GREY, VGA_BLACK);
				else if (val == 4)    attr = VGA_ATTR(VGA_YELLOW, VGA_BLACK);
				else if (val == 8)    attr = VGA_ATTR(VGA_LIGHT_RED, VGA_BLACK);
				else if (val == 16)   attr = VGA_ATTR(VGA_RED, VGA_BLACK);
				else if (val == 32)   attr = VGA_ATTR(VGA_LIGHT_MAGENTA, VGA_BLACK);
				else if (val == 64)   attr = VGA_ATTR(VGA_MAGENTA, VGA_BLACK);
				else if (val >= 128)  attr = VGA_ATTR(VGA_LIGHT_CYAN, VGA_BLACK);
				console_print_at(bx + 1, by + 1, cell, attr);
			}
		}
	}
}

static int game2048_slide_row(int row[4], int *score_add)
{
	int merged[4] = {0, 0, 0, 0};
	int dst = 0;
	int moved = 0;
	for (int src = 0; src < 4; ++src) {
		if (row[src] == 0) continue;
		if (dst > 0 && row[dst - 1] == row[src] && !merged[dst - 1]) {
			row[dst - 1] *= 2;
			*score_add += row[dst - 1];
			merged[dst - 1] = 1;
		} else {
			row[dst] = row[src];
			dst++;
		}
	}
	for (int i = dst; i < 4; ++i)
		row[i] = 0;
	for (int i = 0; i < 4; ++i)
		if (row[i] != 0) moved = 1;
	return moved;
}

static int game2048_move(int dir)
{
	int moved = 0;
	int score_add = 0;
	if (dir == 0) {
		for (int y = 0; y < 4; ++y)
			moved |= game2048_slide_row(game2048_grid[y], &score_add);
	} else if (dir == 2) {
		for (int y = 0; y < 4; ++y) {
			int row[4] = {game2048_grid[y][3], game2048_grid[y][2],
			              game2048_grid[y][1], game2048_grid[y][0]};
			moved |= game2048_slide_row(row, &score_add);
			game2048_grid[y][0] = row[3];
			game2048_grid[y][1] = row[2];
			game2048_grid[y][2] = row[1];
			game2048_grid[y][3] = row[0];
		}
	} else if (dir == 1) {
		for (int x = 0; x < 4; ++x) {
			int row[4] = {game2048_grid[0][x], game2048_grid[1][x],
			              game2048_grid[2][x], game2048_grid[3][x]};
			moved |= game2048_slide_row(row, &score_add);
			for (int y = 0; y < 4; ++y)
				game2048_grid[y][x] = row[y];
		}
	} else if (dir == 3) {
		for (int x = 0; x < 4; ++x) {
			int row[4] = {game2048_grid[3][x], game2048_grid[2][x],
			              game2048_grid[1][x], game2048_grid[0][x]};
			moved |= game2048_slide_row(row, &score_add);
			game2048_grid[0][x] = row[3];
			game2048_grid[1][x] = row[2];
			game2048_grid[2][x] = row[1];
			game2048_grid[3][x] = row[0];
		}
	}
	game2048_score += score_add;
	return moved;
}

static int game2048_has_moves(void)
{
	for (int y = 0; y < 4; ++y)
		for (int x = 0; x < 4; ++x) {
			if (game2048_grid[y][x] == 0) return 1;
			if (x < 3 && game2048_grid[y][x] == game2048_grid[y][x + 1]) return 1;
			if (y < 3 && game2048_grid[y][x] == game2048_grid[y + 1][x]) return 1;
		}
	return 0;
}

static void game2048_init(void)
{
	kmemset(game2048_grid, 0, sizeof(game2048_grid));
	game2048_score = 0;
	game2048_spawn();
	game2048_spawn();
}

static void game_2048(void)
{
	console_clear();
	rng_seed();
	game2048_best = 0;
	game2048_init();
	int running = 1;
	while (running) {
		game2048_draw();
		wait_frame(0);
		if (keyboard_data_available()) {
			int sc = keyboard_read_scancode();
			if (sc == SC_Q || sc == SC_ESCAPE) break;
			if (sc == SC_R) { game2048_init(); continue; }
			int dir = -1;
			if (sc == SC_UP)    dir = 1;
			if (sc == SC_DOWN)  dir = 3;
			if (sc == SC_LEFT)  dir = 0;
			if (sc == SC_RIGHT) dir = 2;
			if (dir >= 0) {
				if (game2048_move(dir)) {
					game2048_spawn();
					if (game2048_score > game2048_best)
						game2048_best = game2048_score;
				}
				if (!game2048_has_moves())
					running = 0;
			}
		}
	}
	game2048_draw();
	console_print_at(15, 12, "GAME OVER!", VGA_ATTR(VGA_LIGHT_RED, VGA_BLACK));
	console_print_at(13, 13, "Press any key...", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
	keyboard_read_char();
}

/* ===================== TIC TAC TOE ===================== */

static int ttt_board[3][3];
static int ttt_turn;

static int ttt_check(void)
{
	for (int i = 0; i < 3; ++i) {
		if (ttt_board[i][0] && ttt_board[i][0] == ttt_board[i][1] && ttt_board[i][0] == ttt_board[i][2])
			return ttt_board[i][0];
		if (ttt_board[0][i] && ttt_board[0][i] == ttt_board[1][i] && ttt_board[0][i] == ttt_board[2][i])
			return ttt_board[0][i];
	}
	if (ttt_board[0][0] && ttt_board[0][0] == ttt_board[1][1] && ttt_board[0][0] == ttt_board[2][2])
		return ttt_board[0][0];
	if (ttt_board[0][2] && ttt_board[0][2] == ttt_board[1][1] && ttt_board[0][2] == ttt_board[2][0])
		return ttt_board[0][2];
	return 0;
}

static int ttt_minimax(int depth, int is_max)
{
	int winner = ttt_check();
	if (winner == 2) return 10 - depth;
	if (winner == 1) return depth - 10;
	int full = 1;
	for (int y = 0; y < 3; ++y)
		for (int x = 0; x < 3; ++x)
			if (!ttt_board[y][x]) full = 0;
	if (full) return 0;
	if (is_max) {
		int best = -11;
		for (int y = 0; y < 3; ++y)
			for (int x = 0; x < 3; ++x) {
				if (!ttt_board[y][x]) {
					ttt_board[y][x] = 2;
					int v = ttt_minimax(depth + 1, 0);
					if (v > best) best = v;
					ttt_board[y][x] = 0;
				}
			}
		return best;
	} else {
		int best = 11;
		for (int y = 0; y < 3; ++y)
			for (int x = 0; x < 3; ++x) {
				if (!ttt_board[y][x]) {
					ttt_board[y][x] = 1;
					int v = ttt_minimax(depth + 1, 1);
					if (v < best) best = v;
					ttt_board[y][x] = 0;
				}
			}
		return best;
	}
}

static void ttt_cpu_move(void)
{
	int best_score = -11;
	int best_x = -1, best_y = -1;
	for (int y = 0; y < 3; ++y)
		for (int x = 0; x < 3; ++x) {
			if (!ttt_board[y][x]) {
				ttt_board[y][x] = 2;
				int score = ttt_minimax(0, 0);
				ttt_board[y][x] = 0;
				if (score > best_score) {
					best_score = score;
					best_x = x;
					best_y = y;
				}
			}
		}
	if (best_x >= 0)
		ttt_board[best_y][best_x] = 2;
}

static void ttt_draw(void)
{
	console_print_at(0, 0, "TIC TAC TOE  Numbers=Play Q=Quit", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
	console_print_at(10, 2, "You=X  CPU=O", VGA_ATTR(VGA_WHITE, VGA_BLACK));
	for (int y = 0; y < 3; ++y) {
		int by = y * 2 + 4;
		console_print_at(14, by,   "   |   |   ", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
		console_print_at(14, by + 1, "---+---+---", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
		for (int x = 0; x < 3; ++x) {
			int bx = x * 4 + 14;
			char c = ' ';
			unsigned char attr = VGA_DEFAULT_ATTR;
			int cell = y * 3 + x + 1;
			char num[2] = {'0' + cell, '\0'};
			if (ttt_board[y][x] == 1) {
				c = 'X';
				attr = VGA_ATTR(VGA_LIGHT_GREEN, VGA_BLACK);
			} else if (ttt_board[y][x] == 2) {
				c = 'O';
				attr = VGA_ATTR(VGA_LIGHT_RED, VGA_BLACK);
			} else {
				console_print_at(bx + 1, by, num, VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
				continue;
			}
			console_putxy(bx + 1, by, c, attr);
		}
	}
	console_print_at(14, 11, "1-9=Place X", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
}

static void game_tictactoe(void)
{
	console_clear();
	kmemset(ttt_board, 0, sizeof(ttt_board));
	ttt_turn = 0;
	int running = 1;
	while (running) {
		ttt_draw();
		if (ttt_check() || ttt_turn >= 9) {
			running = 0;
			break;
		}
		wait_frame(0);
		if (keyboard_data_available()) {
			int sc = keyboard_read_scancode();
			if (sc == SC_Q || sc == SC_ESCAPE) break;
			int digit = scancode_to_digit(sc);
			if (digit >= 1 && digit <= 9) {
				int y = (digit - 1) / 3;
				int x = (digit - 1) % 3;
				if (ttt_board[y][x] == 0) {
					ttt_board[y][x] = 1;
					ttt_turn++;
					if (!ttt_check() && ttt_turn < 9)
						ttt_cpu_move();
					ttt_turn++;
				}
			}
		}
	}
	ttt_draw();
	int winner = ttt_check();
	const char *msg;
	unsigned char attr;
	if (winner == 1) { msg = "YOU WIN!"; attr = VGA_ATTR(VGA_LIGHT_GREEN, VGA_BLACK); }
	else if (winner == 2) { msg = "CPU WINS!"; attr = VGA_ATTR(VGA_LIGHT_RED, VGA_BLACK); }
	else { msg = "DRAW!"; attr = VGA_ATTR(VGA_YELLOW, VGA_BLACK); }
	console_print_at(16, 13, msg, attr);
	console_print_at(13, 14, "Press any key...", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
	keyboard_read_char();
}

/* ===================== MINESWEEPER ===================== */

#define MINE_W 9
#define MINE_H 9
#define MINE_COUNT 10

static int mine_field[MINE_H][MINE_W];
static int mine_revealed[MINE_H][MINE_W];
static int mine_flagged[MINE_H][MINE_W];
static int mine_cx, mine_cy;
static int mine_flags_placed;
static int mine_game_over;
static int mine_win;

static void mine_place_mines(void)
{
	int placed = 0;
	while (placed < MINE_COUNT) {
		int x = rng_next() % MINE_W;
		int y = rng_next() % MINE_H;
		if (mine_field[y][x] != -1) {
			mine_field[y][x] = -1;
			placed++;
		}
	}
	for (int y = 0; y < MINE_H; ++y)
		for (int x = 0; x < MINE_W; ++x) {
			if (mine_field[y][x] == -1) continue;
			int count = 0;
			for (int dy = -1; dy <= 1; ++dy)
				for (int dx = -1; dx <= 1; ++dx) {
					int nx = x + dx, ny = y + dy;
					if (nx >= 0 && nx < MINE_W && ny >= 0 && ny < MINE_H && mine_field[ny][nx] == -1)
						count++;
				}
			mine_field[y][x] = count;
		}
}

static void mine_init(void)
{
	kmemset(mine_field, 0, sizeof(mine_field));
	kmemset(mine_revealed, 0, sizeof(mine_revealed));
	kmemset(mine_flagged, 0, sizeof(mine_flagged));
	mine_cx = MINE_W / 2;
	mine_cy = MINE_H / 2;
	mine_flags_placed = 0;
	mine_game_over = 0;
	mine_win = 0;
	mine_place_mines();
}

static void mine_reveal(int x, int y)
{
	if (x < 0 || x >= MINE_W || y < 0 || y >= MINE_H) return;
	if (mine_revealed[y][x] || mine_flagged[y][x]) return;
	mine_revealed[y][x] = 1;
	if (mine_field[y][x] == 0) {
		for (int dy = -1; dy <= 1; ++dy)
			for (int dx = -1; dx <= 1; ++dx)
				mine_reveal(x + dx, y + dy);
	}
}

static void mine_check_win(void)
{
	int unrevealed = 0;
	for (int y = 0; y < MINE_H; ++y)
		for (int x = 0; x < MINE_W; ++x)
			if (!mine_revealed[y][x]) unrevealed++;
	if (unrevealed == MINE_COUNT)
		mine_win = 1;
}

static void mine_draw(void)
{
	console_print_at(0, 0, "MINESWEEPER  Arrows=Move Space=Reveal F=Flag Q=Quit", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
	char info[32];
	char num[8];
	kstrcpy(info, "Mines: ", sizeof(info));
	kitoa(MINE_COUNT - mine_flags_placed, num, sizeof(num));
	kstrcat(info, num, sizeof(info));
	console_print_at(0, 1, info, VGA_ATTR(VGA_YELLOW, VGA_BLACK));
	for (int y = 0; y < MINE_H; ++y) {
		for (int x = 0; x < MINE_W; ++x) {
			int px = x * 2 + 2;
			int py = y + 3;
			unsigned char attr = VGA_DEFAULT_ATTR;
			char c = ' ';
			if (mine_revealed[y][x]) {
				if (mine_field[y][x] == -1) {
					c = '*';
					attr = VGA_ATTR(VGA_LIGHT_RED, VGA_BLACK);
				} else if (mine_field[y][x] == 0) {
					c = '.';
					attr = VGA_ATTR(VGA_DARK_GREY, VGA_BLACK);
				} else {
					c = '0' + mine_field[y][x];
					if (mine_field[y][x] <= 2) attr = VGA_ATTR(VGA_LIGHT_GREEN, VGA_BLACK);
					else if (mine_field[y][x] <= 4) attr = VGA_ATTR(VGA_YELLOW, VGA_BLACK);
					else if (mine_field[y][x] <= 6) attr = VGA_ATTR(VGA_LIGHT_RED, VGA_BLACK);
					else attr = VGA_ATTR(VGA_RED, VGA_BLACK);
				}
			} else if (mine_flagged[y][x]) {
				c = 'F';
				attr = VGA_ATTR(VGA_LIGHT_RED, VGA_BLACK);
			}
			if (x == mine_cx && y == mine_cy && !mine_game_over)
				attr = (attr & 0x0F) | ((attr & 0xF0) >> 4) | 0x80;
			console_putxy(px, py, c, attr);
			console_putxy(px + 1, py, ' ', (attr & 0xF0) >> 4);
		}
	}
}

static void game_minesweeper(void)
{
	console_clear();
	rng_seed();
	mine_init();
	int running = 1;
	while (running) {
		mine_draw();
		wait_frame(0);
		if (mine_game_over || mine_win) {
			if (mine_win)
				console_print_at(20, 14, "YOU WIN! All mines cleared!", VGA_ATTR(VGA_LIGHT_GREEN, VGA_BLACK));
			else
				console_print_at(20, 14, "BOOM! You hit a mine!", VGA_ATTR(VGA_LIGHT_RED, VGA_BLACK));
			console_print_at(20, 15, "Press any key...", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
			keyboard_read_char();
			break;
		}
		if (keyboard_data_available()) {
			int sc = keyboard_read_scancode();
			if (sc == SC_Q || sc == SC_ESCAPE) break;
			if (sc == SC_LEFT  && mine_cx > 0) mine_cx--;
			if (sc == SC_RIGHT && mine_cx < MINE_W - 1) mine_cx++;
			if (sc == SC_UP    && mine_cy > 0) mine_cy--;
			if (sc == SC_DOWN  && mine_cy < MINE_H - 1) mine_cy++;
			if (sc == SC_SPACE) {
				if (mine_flagged[mine_cy][mine_cx]) continue;
				if (mine_field[mine_cy][mine_cx] == -1) {
					mine_revealed[mine_cy][mine_cx] = 1;
					mine_game_over = 1;
				} else {
					mine_reveal(mine_cx, mine_cy);
					mine_check_win();
				}
			}
			if (sc == SC_F) {
				if (!mine_revealed[mine_cy][mine_cx]) {
					if (mine_flagged[mine_cy][mine_cx]) {
						mine_flagged[mine_cy][mine_cx] = 0;
						mine_flags_placed--;
					} else {
						mine_flagged[mine_cy][mine_cx] = 1;
						mine_flags_placed++;
					}
				}
			}
		}
	}
}

/* ===================== BREAKOUT ===================== */

#define BK_W 38
#define BK_H 22
#define BK_PADDLE_W 6
#define BK_BRICK_ROWS 5
#define BK_BRICK_COLS 8

static int bk_paddle_x;
static int bk_bx, bk_by;
static int bk_bdx, bk_bdy;
static int bk_bricks[BK_BRICK_ROWS][BK_BRICK_COLS];
static int bk_score;
static int bk_lives;
static unsigned char bk_brick_colors[BK_BRICK_ROWS];

static void bk_init(void)
{
	bk_paddle_x = BK_W / 2 - BK_PADDLE_W / 2;
	bk_bx = BK_W / 2;
	bk_by = BK_H - 3;
	bk_bdx = 1;
	bk_bdy = -1;
	bk_score = 0;
	bk_lives = 3;
	unsigned char colors[] = {
		VGA_RED, VGA_LIGHT_RED, VGA_YELLOW, VGA_LIGHT_GREEN, VGA_CYAN
	};
	for (int y = 0; y < BK_BRICK_ROWS; ++y) {
		bk_brick_colors[y] = colors[y];
		for (int x = 0; x < BK_BRICK_COLS; ++x)
			bk_bricks[y][x] = 1;
	}
}

static void bk_draw(void)
{
	for (int y = 0; y <= BK_H; ++y) {
		for (int x = 0; x <= BK_W; ++x) {
			char c = ' ';
			unsigned char attr = VGA_DEFAULT_ATTR;
			if (y == 0 || x == 0 || x == BK_W) {
				c = '#';
				attr = VGA_ATTR(VGA_DARK_GREY, VGA_BLACK);
			} else if (y == BK_H) {
				c = '#';
				attr = VGA_ATTR(VGA_DARK_GREY, VGA_BLACK);
			} else if (y >= 1 && y <= BK_BRICK_ROWS && x >= 1 && x <= BK_W - 1) {
				int brick_x = (x - 1) * BK_BRICK_COLS / (BK_W - 1);
				int brick_y = y - 1;
				if (brick_x >= BK_BRICK_COLS) brick_x = BK_BRICK_COLS - 1;
				if (brick_y < BK_BRICK_ROWS && bk_bricks[brick_y][brick_x]) {
					c = 0xDB;
					attr = VGA_ATTR(bk_brick_colors[brick_y], VGA_BLACK);
				}
			}
			if (y == BK_H - 2 && x >= bk_paddle_x && x < bk_paddle_x + BK_PADDLE_W) {
				c = 0xDB;
				attr = VGA_ATTR(VGA_LIGHT_BLUE, VGA_BLACK);
			}
			if (x == bk_bx && y == bk_by && y < BK_H - 1) {
				c = 'O';
				attr = VGA_ATTR(VGA_WHITE, VGA_BLACK);
			}
			console_putxy(x + 20, y + 1, c, attr);
		}
	}
	char info[32];
	char num[8];
	kstrcpy(info, "Score: ", sizeof(info));
	kitoa(bk_score, num, sizeof(num));
	kstrcat(info, num, sizeof(info));
	console_print_at(22, 0, info, VGA_ATTR(VGA_YELLOW, VGA_BLACK));
	kstrcpy(info, "Lives: ", sizeof(info));
	kitoa(bk_lives, num, sizeof(num));
	kstrcat(info, num, sizeof(info));
	console_print_at(40, 0, info, VGA_ATTR(VGA_LIGHT_RED, VGA_BLACK));
	console_print_at(0, 0, "BREAKOUT  A/D=Move Q=Quit", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
}

static void game_breakout(void)
{
	console_clear();
	bk_init();
	int running = 1;
	int speed = 2;
	int frame = 0;
	while (running) {
		bk_draw();
		frame++;
		if (frame >= speed) {
			frame = 0;
			bk_bx += bk_bdx;
			bk_by += bk_bdy;
			if (bk_bx <= 1) { bk_bdx = 1; bk_bx = 1; }
			if (bk_bx >= BK_W - 1) { bk_bdx = -1; bk_bx = BK_W - 1; }
			if (bk_by <= 1) { bk_bdy = 1; bk_by = 1; }
			if (bk_by == BK_H - 3 && bk_bx >= bk_paddle_x && bk_bx < bk_paddle_x + BK_PADDLE_W) {
				bk_bdy = -1;
				int rel = bk_bx - bk_paddle_x - BK_PADDLE_W / 2;
				bk_bdx = rel < -1 ? -1 : (rel > 1 ? 1 : rel);
				if (bk_bdx == 0) bk_bdx = (rng_next() % 2) ? 1 : -1;
			}
			if (bk_by >= BK_H) {
				bk_lives--;
				if (bk_lives <= 0) { running = 0; break; }
				bk_bx = BK_W / 2;
				bk_by = BK_H - 3;
				bk_bdx = 1;
				bk_bdy = -1;
			}
			int bx_clamped = bk_bx >= BK_W ? BK_W - 1 : bk_bx;
			int by_clamped = bk_by >= BK_H ? BK_H - 1 : bk_by;
			int brick_x = (bx_clamped - 1) * BK_BRICK_COLS / (BK_W - 1);
			int brick_y = by_clamped - 1;
			if (brick_x >= BK_BRICK_COLS) brick_x = BK_BRICK_COLS - 1;
			if (brick_y >= 0 && brick_y < BK_BRICK_ROWS && brick_x >= 0 && brick_x < BK_BRICK_COLS) {
				if (bk_bricks[brick_y][brick_x]) {
					bk_bricks[brick_y][brick_x] = 0;
					bk_bdy = -bk_bdy;
					bk_score += 10 * (BK_BRICK_ROWS - brick_y);
					int remaining = 0;
					for (int y = 0; y < BK_BRICK_ROWS; ++y)
						for (int x = 0; x < BK_BRICK_COLS; ++x)
							if (bk_bricks[y][x]) remaining++;
					if (remaining == 0) { running = 0; break; }
				}
			}
		}
		wait_frame(1);
		if (keyboard_data_available()) {
			int sc = keyboard_read_scancode();
			if (sc == SC_Q || sc == SC_ESCAPE) break;
			if (sc == SC_A && bk_paddle_x > 1) bk_paddle_x--;
			if (sc == SC_D && bk_paddle_x < BK_W - BK_PADDLE_W) bk_paddle_x++;
		}
	}
	const char *msg = (bk_lives > 0) ? "YOU WIN! All bricks cleared!" : "GAME OVER! No lives left.";
	unsigned char mattr = (bk_lives > 0) ? VGA_ATTR(VGA_LIGHT_GREEN, VGA_BLACK) : VGA_ATTR(VGA_LIGHT_RED, VGA_BLACK);
	bk_draw();
	console_print_at(22, 12, msg, mattr);
	console_print_at(22, 13, "Press any key...", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
	keyboard_read_char();
}

/* ===================== MEMORY (CARD MATCH) ===================== */

#define MEM_GRID 4
#define MEM_PAIRS 8

static int mem_cards[MEM_GRID][MEM_GRID];
static int mem_revealed[MEM_GRID][MEM_GRID];
static int mem_matched[MEM_GRID][MEM_GRID];
static int mem_cx, mem_cy;
static int mem_flip1_x, mem_flip1_y;
static int mem_flip2_x, mem_flip2_y;
static int mem_flipped;
static int mem_pairs_found;
static int mem_moves;

static void mem_init(void)
{
	int vals[MEM_GRID * MEM_GRID];
	for (int i = 0; i < MEM_PAIRS; ++i) {
		vals[i * 2] = i + 1;
		vals[i * 2 + 1] = i + 1;
	}
	for (int i = MEM_GRID * MEM_GRID - 1; i > 0; --i) {
		int j = rng_next() % (i + 1);
		int tmp = vals[i];
		vals[i] = vals[j];
		vals[j] = tmp;
	}
	for (int y = 0; y < MEM_GRID; ++y)
		for (int x = 0; x < MEM_GRID; ++x)
			mem_cards[y][x] = vals[y * MEM_GRID + x];
	kmemset(mem_revealed, 0, sizeof(mem_revealed));
	kmemset(mem_matched, 0, sizeof(mem_matched));
	mem_cx = 0;
	mem_cy = 0;
	mem_flip1_x = mem_flip1_y = -1;
	mem_flip2_x = mem_flip2_y = -1;
	mem_flipped = 0;
	mem_pairs_found = 0;
	mem_moves = 0;
}

static const char *mem_symbols[] = {
	"", "*", "#", "@", "$", "%", "&", "~", "+"
};

static void mem_draw(void)
{
	console_print_at(0, 0, "MEMORY  Arrows=Move Space=Flip Q=Quit", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
	char info[32];
	char num[8];
	kstrcpy(info, "Pairs: ", sizeof(info));
	kitoa(mem_pairs_found, num, sizeof(num));
	kstrcat(info, num, sizeof(info));
	kstrcat(info, "/", sizeof(info));
	kitoa(MEM_PAIRS, num, sizeof(num));
	kstrcat(info, num, sizeof(info));
	console_print_at(0, 1, info, VGA_ATTR(VGA_YELLOW, VGA_BLACK));
	kstrcpy(info, "Moves: ", sizeof(info));
	kitoa(mem_moves, num, sizeof(num));
	kstrcat(info, num, sizeof(info));
	console_print_at(25, 1, info, VGA_ATTR(VGA_CYAN, VGA_BLACK));
	for (int y = 0; y < MEM_GRID; ++y) {
		for (int x = 0; x < MEM_GRID; ++x) {
			int px = x * 6 + 5;
			int py = y * 2 + 4;
			unsigned char attr;
			char cell[4];
			if (mem_matched[y][x] || mem_revealed[y][x]) {
				int val = mem_cards[y][x];
				kstrcpy(cell, mem_symbols[val], sizeof(cell));
				attr = mem_matched[y][x] ? VGA_ATTR(VGA_LIGHT_GREEN, VGA_BLACK) : VGA_ATTR(VGA_LIGHT_CYAN, VGA_BLACK);
			} else {
				kstrcpy(cell, "?", sizeof(cell));
				attr = VGA_ATTR(VGA_DARK_GREY, VGA_BLACK);
			}
			if (x == mem_cx && y == mem_cy && !mem_matched[y][x])
				attr = (attr & 0x0F) | 0x80;
			console_print_at(px, py, "+----+", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
			char row[8];
			kstrcpy(row, "| ", sizeof(row));
			kstrcat(row, cell, sizeof(row));
			int clen = kstrlen(cell);
			for (int p = clen; p < 2; ++p) kstrcat(row, " ", sizeof(row));
			while (kstrlen(row) < 5) kstrcat(row, " ", sizeof(row));
			kstrcat(row, "|", sizeof(row));
			console_print_at(px, py + 1, row, attr);
			console_print_at(px, py + 2, "+----+", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
		}
	}
}

static void game_memory(void)
{
	console_clear();
	rng_seed();
	mem_init();
	int running = 1;
	int wait_frames = 0;
	while (running) {
		mem_draw();
		if (wait_frames > 0) {
			wait_frame(8);
			wait_frames--;
			if (wait_frames == 0) {
				if (mem_flip1_x >= 0 && mem_flip2_x >= 0) {
					if (mem_cards[mem_flip1_y][mem_flip1_x] == mem_cards[mem_flip2_y][mem_flip2_x]) {
						mem_matched[mem_flip1_y][mem_flip1_x] = 1;
						mem_matched[mem_flip2_y][mem_flip2_x] = 1;
						mem_pairs_found++;
					}
					mem_revealed[mem_flip1_y][mem_flip1_x] = 0;
					mem_revealed[mem_flip2_y][mem_flip2_x] = 0;
					mem_flip1_x = mem_flip1_y = -1;
					mem_flip2_x = mem_flip2_y = -1;
				}
				continue;
			}
			continue;
		}
		if (mem_pairs_found == MEM_PAIRS) {
			mem_draw();
			console_print_at(12, 13, "YOU WIN! All pairs matched!", VGA_ATTR(VGA_LIGHT_GREEN, VGA_BLACK));
			char info[32];
			char num[8];
			kstrcpy(info, "Completed in ", sizeof(info));
			kitoa(mem_moves, num, sizeof(num));
			kstrcat(info, num, sizeof(info));
			kstrcat(info, " moves", sizeof(info));
			console_print_at(12, 14, info, VGA_ATTR(VGA_YELLOW, VGA_BLACK));
			console_print_at(12, 15, "Press any key...", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
			keyboard_read_char();
			break;
		}
		wait_frame(0);
		if (keyboard_data_available()) {
			int sc = keyboard_read_scancode();
			if (sc == SC_Q || sc == SC_ESCAPE) break;
			if (sc == SC_LEFT  && mem_cx > 0) mem_cx--;
			if (sc == SC_RIGHT && mem_cx < MEM_GRID - 1) mem_cx++;
			if (sc == SC_UP    && mem_cy > 0) mem_cy--;
			if (sc == SC_DOWN  && mem_cy < MEM_GRID - 1) mem_cy++;
			if (sc == SC_SPACE) {
				if (mem_revealed[mem_cy][mem_cx] || mem_matched[mem_cy][mem_cx])
					continue;
				if (mem_flip1_x < 0) {
					mem_flip1_x = mem_cx;
					mem_flip1_y = mem_cy;
					mem_revealed[mem_cy][mem_cx] = 1;
				} else if (mem_flip2_x < 0 && (mem_cx != mem_flip1_x || mem_cy != mem_flip1_y)) {
					mem_flip2_x = mem_cx;
					mem_flip2_y = mem_cy;
					mem_revealed[mem_cy][mem_cx] = 1;
					mem_moves++;
					wait_frames = 2;
				}
			}
		}
	}
}

/* ===================== MENU ===================== */

void games_menu(void)
{
	console_clear();
	int running = 1;
	while (running) {
		console_clear();
		console_print_at(0, 0,  "    _   _       _                        ___  ____  ", VGA_ATTR(VGA_CYAN, VGA_BLACK));
		console_print_at(0, 1,  "   | \\ | | ___ | |_ ___  ___  _ __     / _ \\/ ___| ", VGA_ATTR(VGA_CYAN, VGA_BLACK));
		console_print_at(0, 2,  "   |  \\| |/ _ \\| __/ _ \\/ __|| '_ \\   | | | \\___ \\ ", VGA_ATTR(VGA_CYAN, VGA_BLACK));
		console_print_at(0, 3,  "   | |\\  | (_) | || (_) \\__ \\| | | |  | |_| |___) |", VGA_ATTR(VGA_CYAN, VGA_BLACK));
		console_print_at(0, 4,  "   |_| \\_|\\___/ \\__\\___/|___/|_| |_|   \\___/|____/ ", VGA_ATTR(VGA_CYAN, VGA_BLACK));
		console_print_at(0, 5,  "                 G A M E S                          ", VGA_ATTR(VGA_WHITE, VGA_BLACK));
		console_print_at(0, 7,  "  1. Snake", VGA_ATTR(VGA_WHITE, VGA_BLACK));
		console_print_at(0, 8,  "  2. Tetris", VGA_ATTR(VGA_WHITE, VGA_BLACK));
		console_print_at(0, 9,  "  3. Pong", VGA_ATTR(VGA_WHITE, VGA_BLACK));
		console_print_at(0, 10, "  4. 2048", VGA_ATTR(VGA_WHITE, VGA_BLACK));
		console_print_at(0, 11, "  5. Tic-Tac-Toe", VGA_ATTR(VGA_WHITE, VGA_BLACK));
		console_print_at(0, 12, "  6. Minesweeper", VGA_ATTR(VGA_WHITE, VGA_BLACK));
		console_print_at(0, 13, "  7. Breakout", VGA_ATTR(VGA_WHITE, VGA_BLACK));
		console_print_at(0, 14, "  8. Memory", VGA_ATTR(VGA_WHITE, VGA_BLACK));
		console_print_at(0, 16, "  Q. Back to shell", VGA_ATTR(VGA_DARK_GREY, VGA_BLACK));
		console_print_at(0, 18, "Select: ", VGA_DEFAULT_ATTR);
		while (1) {
			if (keyboard_data_available()) {
				int sc = keyboard_read_scancode();
				if (sc == SC_Q || sc == SC_ESCAPE) { running = 0; break; }
				if (sc == SC_1) { game_snake(); break; }
				if (sc == SC_2) { game_tetris(); break; }
				if (sc == SC_3) { game_pong(); break; }
				if (sc == SC_4) { game_2048(); break; }
				if (sc == SC_5) { game_tictactoe(); break; }
				if (sc == SC_6) { game_minesweeper(); break; }
				if (sc == SC_7) { game_breakout(); break; }
				if (sc == SC_8) { game_memory(); break; }
			}
		}
	}
	console_clear();
}

void game_launch_snake(void) { game_snake(); console_clear(); }
void game_launch_tetris(void) { game_tetris(); console_clear(); }
void game_launch_pong(void) { game_pong(); console_clear(); }
void game_launch_2048(void) { game_2048(); console_clear(); }
void game_launch_ttt(void) { game_tictactoe(); console_clear(); }
void game_launch_minesweeper(void) { game_minesweeper(); console_clear(); }
void game_launch_breakout(void) { game_breakout(); console_clear(); }
void game_launch_memory(void) { game_memory(); console_clear(); }
