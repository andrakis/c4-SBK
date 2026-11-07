// record.c -- how c4/demo.c was made: sbk-host-record -record drives the game
// through a fixed course of menus and races, steering by the state of the game
// as a player would watch it, and writes the buttons it held as C. Only gcc's
// recording build has this. What test-sbk replays is the stream alone, so it
// holds every build to the same keys, and the original C++ game too.
//
//   make sbk-record

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "sbk.h"

// the course: what to do, and for how long or until what. It lets go of A
// before it pauses: letting go in the same update as Esc would choose the pause
// menu's Continue at once, on upstream's game as here.
enum { S_WAIT, S_TAP, S_DOWN_TAP, S_DRIVE_FINISH, S_DRIVE_TIME, S_WAIT_END, S_PAUSE_TAP, S_ITEM_DRIVE, S_DONE };
typedef struct Step { int kind, a; } Step;

static Step course[] = {
	// the intro, with its karts going by; a button; the logo rising
	{ S_WAIT, 500 },
	{ S_TAP, BTN_A },
	{ S_WAIT, 350 },
	// Multiplayer says maybe later
	{ S_TAP, BTN_DOWN }, { S_WAIT, 30 }, { S_TAP, BTN_DOWN }, { S_WAIT, 30 }, { S_TAP, BTN_A }, { S_WAIT, 60 },
	{ S_TAP, BTN_UP }, { S_WAIT, 30 }, { S_TAP, BTN_UP }, { S_WAIT, 30 },
	// Race: kart select, each kart a while; back; on
	{ S_TAP, BTN_A }, { S_WAIT, 120 }, { S_TAP, BTN_DOWN }, { S_WAIT, 200 }, { S_TAP, BTN_B }, { S_WAIT, 40 },
	{ S_TAP, BTN_A }, { S_WAIT, 40 }, { S_TAP, BTN_DOWN }, { S_WAIT, 40 }, { S_TAP, BTN_A }, { S_WAIT, 100 },
	// track select: look at both, back to rainbow, and go
	{ S_TAP, BTN_DOWN }, { S_WAIT, 120 }, { S_TAP, BTN_DOWN }, { S_WAIT, 60 }, { S_TAP, BTN_A }, { S_WAIT, 120 },
	// a race on rainbow: pause and go on, three laps with the items it finds, to the end
	{ S_ITEM_DRIVE, 700 }, { S_WAIT, 10 }, { S_PAUSE_TAP, 0 }, { S_WAIT, 60 }, { S_TAP, BTN_A }, { S_WAIT, 10 }, { S_DRIVE_FINISH, 0 },
	{ S_WAIT_END, 0 }, { S_WAIT, 50 },
	// Quit: the result is saved
	{ S_TAP, BTN_DOWN }, { S_WAIT, 30 }, { S_TAP, BTN_A }, { S_WAIT, 60 },

	// Time Trial on rainbow, three laps; quit, which saves the ghost
	{ S_TAP, BTN_DOWN }, { S_WAIT, 30 }, { S_TAP, BTN_A }, { S_WAIT, 40 }, { S_TAP, BTN_A }, { S_WAIT, 100 },
	{ S_TAP, BTN_A }, { S_WAIT, 40 },
	{ S_DRIVE_FINISH, 0 }, { S_WAIT_END, 0 }, { S_WAIT, 50 },
	{ S_TAP, BTN_DOWN }, { S_WAIT, 30 }, { S_TAP, BTN_A }, { S_WAIT, 60 },

	// again, against the ghost, for a while; pause, restart, a little more; pause, quit
	{ S_TAP, BTN_DOWN }, { S_WAIT, 30 }, { S_TAP, BTN_A }, { S_WAIT, 40 }, { S_TAP, BTN_A }, { S_WAIT, 100 },
	{ S_TAP, BTN_A }, { S_WAIT, 40 },
	{ S_DRIVE_TIME, 1200 },
	{ S_WAIT, 10 }, { S_PAUSE_TAP, 0 }, { S_WAIT, 30 }, { S_TAP, BTN_DOWN }, { S_WAIT, 30 }, { S_TAP, BTN_A }, { S_WAIT, 40 },
	{ S_DRIVE_TIME, 500 },
	{ S_WAIT, 10 }, { S_PAUSE_TAP, 0 }, { S_WAIT, 30 }, { S_TAP, BTN_DOWN }, { S_WAIT, 30 },
	{ S_TAP, BTN_A }, { S_WAIT, 60 },

	// a race on the desert, for a while: boost strips, cacti, walls, items; pause, quit
	{ S_TAP, BTN_A }, { S_WAIT, 40 }, { S_TAP, BTN_A }, { S_WAIT, 60 },
	{ S_TAP, BTN_DOWN }, { S_WAIT, 150 }, { S_TAP, BTN_A }, { S_WAIT, 120 },
	{ S_ITEM_DRIVE, 3000 }, { S_DRIVE_TIME, 1500 },
	{ S_WAIT, 10 }, { S_PAUSE_TAP, 0 }, { S_WAIT, 30 }, { S_TAP, BTN_DOWN }, { S_WAIT, 30 }, { S_TAP, BTN_DOWN }, { S_WAIT, 30 },
	{ S_TAP, BTN_A }, { S_WAIT, 60 },
	{ S_DONE, 0 }
};

static int at, timer, phase;
static int run_count, run_mask = -1;
static int total, count;

static FILE *out;

void record_open(char *file) {
	out = fopen(file, "w");
	fprintf(out, "// demo.c -- the stream of buttons -demo plays: updates held, buttons. Made by\n"
	             "// make sbk-record (c4/record.c), not by hand.\n"
	             "int sbk_demo[] = {\n");
}

static void emit(int c, int m) { fprintf(out, "%d,%d,\n", c, m); total++; }

void record_step(int mask) {
	if (mask == run_mask) { run_count++; return; }
	if (run_count) emit(run_count, run_mask);
	run_mask = mask;
	run_count = 1;
}

void record_done(void) {
	if (run_count) emit(run_count, run_mask);
	fprintf(out, "};\nint sbk_demo_len = %d;\n", total * 2);
	fclose(out);
}

// a player's eye: the buttons that do what the machine's karts decide to
// (kart_auto_drive), who can turn by any amount where these turn fully or not
static int steer(int use_items) {
	Kart *k;
	Track *t;
	TrackInfo *info;
	int seg, next, mask, off;
	float lx, lz, px, py, tcx, tcy, dist, ang, turn, recenter;
	mask = BTN_A;
	k = race_state_now()->karts;
	t = race_state_now()->track;
	info = t->info;
	seg = k->current_route_segment;
	lx = k->sprite.look_dir.x;
	lz = k->sprite.look_dir.z;
	px = k->sprite.world_pos.x;
	py = k->sprite.world_pos.z;
	tcx = info->segs[seg].ax + info->segs[seg].rx * k->current_route_frac - px;
	tcy = info->segs[seg].ay + info->segs[seg].ry * k->current_route_frac - py;
	dist = sqrtf(tcx * tcx + tcy * tcy);
	off = track_friction(t, px, py) > 1.0f;
	recenter = 0.0f;
	if (dist > 40.0f || off) {
		ang = atan2f((tcx / dist) * lz - (tcy / dist) * lx, (tcx / dist) * lx + (tcy / dist) * lz);
		if (fabsf(ang) >= 0.785f) recenter = ang < 0.0f ? -0.5f : 0.5f;
	}
	next = seg + 1;
	if (next + 1 == info->route_len) next = 0;
	ang = atan2f(info->segs[next].nx * lz - info->segs[next].ny * lx, info->segs[next].nx * lx + info->segs[next].ny * lz);
	turn = ang * 0.8f;
	if (off || fabsf(recenter) > fabsf(turn)) turn = recenter;
	if (turn > 0.1f) mask |= BTN_LEFT;
	else if (turn < -0.1f) mask |= BTN_RIGHT;
	if (use_items && k->current_item != ITEM_NONE && (timer % 200) == 0) mask |= BTN_X;
	return mask;
}

// the buttons for the next update, or -1: the course is over
int record_buttons(void) {
	Step *s;
	if (++count > 100000) {
		fprintf(stderr, "sbk: the course is stuck at step %d", at);
		return -1;
	}
	while (1) {
		s = course + at;
		switch (s->kind) {
		case S_WAIT:
			if (timer++ < s->a) return 0;
			break;
		case S_TAP:
			if (timer++ < 6) return s->a;
			break;
		case S_PAUSE_TAP:
			if (timer++ < 6) return BTN_MENU;
			break;
		case S_ITEM_DRIVE:
			if (timer++ < s->a) return steer(1);
			break;
		case S_DRIVE_TIME:
			if (timer++ < s->a) return steer(0);
			break;
		case S_DRIVE_FINISH:
			if (!kart_finished(race_state_now()->karts)) { timer++; return steer(1); }
			break;
		case S_WAIT_END:
			if (!race_end_menu_up()) return 0;
			break;
		case S_DONE:
			return -1;
		}
		at++;
		timer = 0;
	}
}
