// game.c -- upstream's game.cpp, and what it asks of 32blit's engine: the buttons'
// changes and an update every 10 ms (blit::tick), the clock, random numbers,
// and the saves. main() is here too: playing, or -demo, which plays
// c4/demo.c on a clock of its own and prints a checksum of every frame.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sbk.h"
#include "plat.h"

float sbk_pi = 3.14159265358979323846f;
float sbk_inf, sbk_pi4, sbk_deg;

unsigned sbk_now;
unsigned btn_state, btn_pressed, btn_released;
int sbk_quit;
int sbk_perf;
PerfRace perf_race[MAX_TRACKS];

static GameState *state, *next_state;

void game_change_state(GameState *s) {
	if (next_state) next_state->destroy();
	next_state = s;
	if (!state) {
		state = next_state;
		next_state = 0;
	}
}

void game_update(unsigned time) {
	if (state) state->update(time);
	if (next_state) {
		state->destroy();
		state = next_state;
		next_state = 0;
	}
}

void game_render(void) { if (state) state->render(); }

void game_init(void) {
	game_change_state(main_menu_new(1));
}

// blit::tick
static unsigned pending_update_time, last_tick_time, last_state;

void game_tick(unsigned time) {
	unsigned changed;
	if (last_tick_time == 0) last_tick_time = time;
	pending_update_time += time - last_tick_time;
	while (pending_update_time >= 10) {
		changed = btn_state ^ last_state;
		btn_pressed = changed & btn_state;
		btn_released = changed & last_state;
		last_state = btn_state;
		game_update(time - pending_update_time);
		pending_update_time -= 10;
	}
	last_tick_time = time;
}

// blit::random: anything will do, so long as a seed plays the same
static unsigned rng;
void sbk_seed(unsigned s) { rng = s ? s : 1; }
unsigned sbk_random(void) {
	rng = rng ^ (rng << 13);
	rng = rng ^ (rng >> 17);
	rng = rng ^ (rng << 5);
	return rng;
}

// ---- saves ----
// Each is kept in memory, for the rest of the game, and written to a file. The bare
// machine and C4DOS can't write one here, but a ghost still lasts as long as the
// game does. -demo leaves no trace, so it stops at memory.

typedef struct MemSave { int slot, len; char *data; struct MemSave *next; } MemSave;
static MemSave *mem_saves;
static int saves_in_memory;

int read_save(void *data, int len, int slot) {
	MemSave *m;
	for (m = mem_saves; m; m = m->next)
		if (m->slot == slot) {
			if (m->len != len) return 0;
			memcpy((char *) data, m->data, len);
			return 1;
		}
	if (saves_in_memory) return 0;
	return plat_save_read(slot, (char *) data, len);
}

void write_save(void *data, int len, int slot) {
	MemSave *m;
	for (m = mem_saves; m; m = m->next)
		if (m->slot == slot) break;
	if (!m) {
		m = (MemSave *) malloc(sizeof(MemSave));
		m->slot = slot;
		m->data = (char *) malloc(len);
		m->len = len;
		m->next = mem_saves;
		mem_saves = m;
	}
	if (m->len == len) memcpy(m->data, (char *) data, len);
	if (!saves_in_memory) plat_save_write(slot, (char *) data, len);
}

// ---- -demo ----

#ifndef SBK_RECORD
extern int sbk_demo[];
extern int sbk_demo_len;
#else
static int sbk_demo[1];
static int sbk_demo_len;
#endif

// updates to a frame, under -demo: 25 frames a second, on the clock it keeps
enum { DEMO_UPDATES = 4 };

static unsigned frame_hash(void) {
	unsigned h;
	int *p, *end;
	h = 2166136261u;
	p = fb;
	end = fb + SCREEN_W * SCREEN_H;
	while (p < end) h = (h ^ (unsigned) *p++) * 16777619u;
	return h;
}

static int demo_step_count, demo_step_mask, demo_index;

// the buttons held at the next update, or -1 when the stream has run out
int demo_next(void) {
	while (demo_step_count == 0) {
		if (demo_index >= sbk_demo_len) return -1;
		demo_step_count = sbk_demo[demo_index];
		demo_step_mask = sbk_demo[demo_index + 1];
		demo_index += 2;
	}
	demo_step_count--;
	return demo_step_mask;
}

#ifdef SBK_RECORD
int record_buttons(void);          // c4/record.c
void record_step(int mask);
void record_done(void);
void record_open(char *file);
static int record_mode;
#endif

static int demo_run(int perf, int frames, int shot_frame, char *shot_file) {
	unsigned t, u0, u1, r0, r1, sum_u, sum_r, max_u, max_r, c;
	int frame, n, mask, nu, want;
	char line[64];
	sbk_perf = perf;
	want = 0;
	frame = 0;
	n = 0;
	t = 1000;
	sbk_now = t;
	game_init();
	game_tick(t);
	sum_u = 0; sum_r = 0; max_u = 0; max_r = 0;
	while (1) {
#ifdef SBK_RECORD
		if (record_mode) {
			mask = record_buttons();
			if (mask >= 0) record_step(mask);
		} else
#endif
		mask = demo_next();
		if (mask < 0 || sbk_quit) break;
		btn_state = mask;
		t += 10;
		sbk_now = t;
		u0 = plat_cycles();
		game_tick(t);
		u1 = plat_cycles();
		sum_u += (u1 - u0) >> 8;
		if (u1 - u0 > max_u) max_u = u1 - u0;
		if (++n == DEMO_UPDATES) {
			n = 0;
			r0 = plat_cycles();
			game_render();
			r1 = plat_cycles();
			c = r1 - r0;
			sum_r += c >> 8;
			if (c > max_r) { max_r = c; want = frame; }
			if (!perf) {
				sprintf(line, "sbk %d %08x\n", frame, frame_hash());
				plat_print(line);
			}
			if (frame == shot_frame && shot_file) plat_shot(shot_file, fb);
			frame++;
			if (frames && frame >= frames) break;
		}
	}
	if (perf) {
		sprintf(line, "sbkperf frames %d\n", frame);
		plat_print(line);
		sprintf(line, "sbkperf update avg %u max %u\n", (sum_u / (frame * DEMO_UPDATES)) << 8, max_u);
		plat_print(line);
		sprintf(line, "sbkperf render avg %u max %u at frame %d\n", (sum_r / frame) << 8, max_r, want);
		plat_print(line);
		for (nu = 0; nu < MAX_TRACKS; nu++) {
			if (!perf_race[nu].frames) continue;
			c = perf_race[nu].frames;
			sprintf(line, "sbkperf %s frames %u:", track_info[nu].name, c);
			plat_print(line);
			sprintf(line, " update %u", perf_race[nu].updates ? (perf_race[nu].update / perf_race[nu].updates) << 8 : 0);
			plat_print(line);
			sprintf(line, " lists %u", (perf_race[nu].lists / c) << 8);
			plat_print(line);
			sprintf(line, " mode7 %u", (perf_race[nu].mode7 / c) << 8);
			plat_print(line);
			sprintf(line, " sprites %u", (perf_race[nu].sprites / c) << 8);
			plat_print(line);
			sprintf(line, " hud %u\n", (perf_race[nu].hud / c) << 8);
			plat_print(line);
			sprintf(line, "  worst: mode7 %u sprites %u", perf_race[nu].max_mode7, perf_race[nu].max_sprites);
			plat_print(line);
			sprintf(line, " hud %u update %u\n", perf_race[nu].max_hud, perf_race[nu].max_update);
			plat_print(line);
		}
	}
#ifdef SBK_RECORD
	if (record_mode) record_done();
#endif
	sprintf(line, "sbk end %d %08x\n", frame, frame_hash());
	plat_print(line);
	return 0;
}

// -savetest: that a file written here comes back, the way a save's would. -demo
// keeps its saves in memory, so this is what tries the files.
static int save_test(void) {
	char *a, *b;
	int i, ok;
	a = (char *) malloc(1000);
	b = (char *) malloc(1000);
	for (i = 0; i < 1000; i++) a[i] = (char) (i * 7);
	plat_save_write(4095, a, 1000);
	ok = plat_save_read(4095, b, 1000);
	for (i = 0; ok && i < 1000; i++) if (a[i] != b[i]) ok = 0;
	plat_print(ok ? "sbk savetest OK\n" : "sbk savetest FAILED\n");
	return !ok;
}

// ---- playing ----

static int play(int window, int stats) {
	unsigned t, t0, last_frame, c0, c1, frames;
	int ms, start;
	char line[80];
	frames = 0;
	if (!video_open(window)) {
		plat_print("sbk: no display and no window to play in\n");
		return 1;
	}
	sbk_seed((unsigned) plat_ms());
	t0 = (unsigned) plat_ms();
	t = 1000;
	sbk_now = t;
	game_init();
	game_tick(t);
	start = plat_ms();
	c0 = plat_cycles();
	while (!sbk_quit) {
		if (!video_poll()) break;     // the buttons, or the window has closed
		ms = plat_ms();
		// a machine that stopped for a while doesn't race to catch up
		if ((unsigned) ms - t0 > t - 1000 + 200) t0 = (unsigned) ms - (t - 1000) - 100;
		t = 1000 + ((unsigned) ms - t0);
		sbk_now = t;
		game_tick(t);
		if (sbk_quit) break;
		game_render();
		video_show();
		frames++;
		// -stats: every five seconds, frames and instructions (shown when the game ends)
		if (stats && ms - start >= 5000) {
			c1 = plat_cycles();
			sprintf(line, "sbk: %u frames a second, %u K instructions each\n", frames * 1000 / (ms - start), (c1 - c0) / frames / 1000);
			plat_print(line);
			start = ms;
			c0 = c1;
			frames = 0;
		}
		// about 60 frames a second, as upstream's on a display
		last_frame = (unsigned) plat_ms() - (unsigned) ms;
		if (last_frame < 16) plat_sleep(16 - (int) last_frame);
	}
	video_close();
	return 0;
}

int main(int argc, char **argv) {
	int i, demo, perf, window, frames, shot_frame, stats;
	char *shot_file;
	demo = 0; perf = 0; window = 0; frames = 0; shot_frame = -1; stats = 0;
	shot_file = 0;
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-demo")) demo = 1;
		else if (!strcmp(argv[i], "-perf")) perf = 1;
		else if (!strcmp(argv[i], "-window")) window = 1;
		else if (!strcmp(argv[i], "-stats")) stats = 1;
		else if (!strcmp(argv[i], "-savetest")) return save_test();
		else if (!strcmp(argv[i], "-frames") && i + 1 < argc) frames = atoi(argv[++i]);
		else if (!strcmp(argv[i], "-shot") && i + 2 < argc) { shot_frame = atoi(argv[++i]); shot_file = argv[++i]; }
#ifdef SBK_RECORD
		else if (!strcmp(argv[i], "-record") && i + 1 < argc) { demo = 1; record_mode = 1; record_open(argv[++i]); }
#endif
		else if (!strcmp(argv[i], "-seed") && i + 1 < argc) sbk_seed((unsigned) atoi(argv[++i]));
		else {
			plat_print("usage: sbk [-window] [-stats] [-demo [-perf] [-frames N] [-shot N FILE]]\n");
			return 1;
		}
	}
	sbk_inf = 1.0e30f * 1.0e30f;
	sbk_pi4 = sbk_pi / 4.0f;
	sbk_deg = sbk_pi / 180.0f;
	scr_init();
	assets_load();
	if (demo) {
		saves_in_memory = 1;
		sbk_seed(12345);
		return demo_run(perf, frames, shot_frame, shot_file);
	}
	return play(window, stats);
}
