// race.c -- upstream's race.cpp and minimap.cpp. A race is one kart for the player
// and seven for the machine, or a time trial against a ghost.
//
// Two things happen later here than upstream, at the next frame drawn rather
// than in every update, because nothing can tell and they are what costs most:
// culling and sorting the sprites (build_lists), and working out the minimap
// (mm_build). Each draws from the same numbers at the same moment.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "sbk.h"
#include "plat.h"

enum { MENU_CONTINUE = 0, MENU_RESTART, MENU_QUIT };
enum { ITEM_CONTAINER = 48, MM_SIZE = 96 };

static struct {
	int player_kart, track_index, mode;
	RaceState state;
	unsigned start_time;
	int num_finished;
	int finish_kart[NUM_KARTS];
	unsigned finish_time[NUM_KARTS];
	Camera cam;
	Sprite3D **list, **below;
	int nlist, nbelow, cap_list, lists_dirty;
	// minimap
	unsigned char *mm_data;
	int mm_x, mm_y, mm_w, mm_h, mm_bx, mm_by, mm_built, mm_any;
	Menu pause_menu, end_menu;
	int paused, show_end_menu, num_karts;
	TimeTrialSave *time_trial_data;
	int worst_slot;
	unsigned worst_time, best_lap_time;
} r;

static GameState race_game_state;

static int mm_palette[4];
static Surf mm_surf;
static int num_font_pen[2];

RaceState *race_state_now(void) { return &r.state; }
int race_end_menu_up(void) { return r.show_end_menu; }

// ---- the minimap ----

// Minimap::update: where it looks, for this frame or a later one
static void mm_update(int cx, int cy) {
	int minx, miny, maxx, maxy, w, h;
	minx = cx - MM_SIZE / 2;
	miny = cy - MM_SIZE / 2;
	maxx = cx + MM_SIZE / 2;
	maxy = cy + MM_SIZE / 2;
	if (minx < 0) { maxx += -minx; minx = 0; }
	if (miny < 0) { maxy += -miny; miny = 0; }
	w = r.state.track->info->w;
	h = r.state.track->info->h;
	if (maxx > w) { minx -= maxx - w; maxx = w; }
	if (maxy > h) { miny -= maxy - h; maxy = h; }
	r.mm_x = minx;
	r.mm_y = miny;
	r.mm_w = maxx - minx;
	r.mm_h = maxy - miny;
	r.mm_any = 1;
}

// what Minimap::update fills in, from the tiles in view, if the view has moved
static void mm_build(void) {
	int x, y, w, line;
	unsigned char *out, *tiles, *cls, *row;
	TrackInfo *info;
	if (!r.mm_any || (r.mm_built && r.mm_bx == r.mm_x && r.mm_by == r.mm_y)) return;
	info = r.state.track->info;
	w = info->w;
	tiles = info->tiles;
	cls = info->mm_class;
	out = r.mm_data;
	line = r.mm_y * w + r.mm_x;
	for (y = 0; y < MM_SIZE; y++) {
		row = tiles + line;
		for (x = 0; x < MM_SIZE; x++) out[x] = cls[row[x]];
		out += MM_SIZE;
		line += w;
	}
	r.mm_built = 1;
	r.mm_bx = r.mm_x;
	r.mm_by = r.mm_y;
}

// ---- the sprites to draw ----

// a forward_list built by push_front and sorted, which is stable: latest first
// among equals
static void sort_list(Sprite3D **l, int n) {
	int i, j;
	Sprite3D *s;
	float z;
	for (i = 1; i < n; i++) {
		s = l[i];
		z = s->z;
		for (j = i; j > 0 && z > l[j - 1]->z; j--) l[j] = l[j - 1];
		l[j] = s;
	}
}

static void reverse_list(Sprite3D **l, int n) {
	int i, j;
	Sprite3D *s;
	i = 0;
	j = n - 1;
	while (i < j) {
		s = l[i]; l[i] = l[j]; l[j] = s;
		i++; j--;
	}
}

static void check_sprite(Sprite3D *s) {
	if (s->z >= r.cam.near_z && s->z <= r.cam.far_z) {
		if (s->world_pos.y < 0.0f) r.below[r.nbelow++] = s;
		else r.list[r.nlist++] = s;
	}
}

// the end of Race::update
static void build_lists(void) {
	int i, n;
	Track *t;
	TrackObject *ob;
	if (!r.lists_dirty) return;
	r.lists_dirty = 0;
	t = r.state.track;
	n = r.num_karts + t->num_objects;
	if (n > r.cap_list) {
		r.cap_list = n * 2;
		if (r.list) free((char *) r.list);
		if (r.below) free((char *) r.below);
		r.list = (Sprite3D **) malloc(r.cap_list * sizeof(Sprite3D *));
		r.below = (Sprite3D **) malloc(r.cap_list * sizeof(Sprite3D *));
	}
	r.nlist = 0;
	r.nbelow = 0;
	for (i = 0; i < r.num_karts; i++) {
		sprite_update(&r.state.karts[i].sprite, &r.cam);
		check_sprite(&r.state.karts[i].sprite);
	}
	n = t->num_objects;
	ob = t->objects;
	for (i = 0; i < n; i++, ob++) {
		if (ob->type == OBJ_REMOVED || ob->respawn_timer != 0) continue;
		sprite_update(&ob->sprite, &r.cam);
		check_sprite(&ob->sprite);
	}
	reverse_list(r.list, r.nlist);
	reverse_list(r.below, r.nbelow);
	sort_list(r.list, r.nlist);
	sort_list(r.below, r.nbelow);
}

// ---- setting up ----

static void setup_race(void) {
	TrackInfo *info;
	Track *trk;
	Kart *k;
	int i, kart_index, best_slot, worst_slot, slot;
	unsigned best_time, worst_time, this_time, t;
	float dirx, diry, flx, fly, w, spx, spy, inc, d;
	TimeTrialSave *tt;

	// what a frame not yet drawn would have shown, from before this
	build_lists();

	// max two karts for time trial
	if (r.mode == MODE_TIME_TRIAL) r.num_karts = 2;

	r.state.countdown = 3000;

	trk = r.state.track;
	info = trk->info;

	// Track::get_starting_dir
	dirx = info->segs[0].rx;
	diry = info->segs[0].ry;
	d = sqrtf(dirx * dirx + diry * diry);
	dirx = dirx / d;
	diry = diry / d;

	flx = (float) (info->fin[2] - info->fin[0]);
	fly = (float) (info->fin[3] - info->fin[1]);
	w = sqrtf(flx * flx + fly * fly);
	flx = flx / w;
	fly = fly / w;

	d = 3.0f / 20.0f;
	spx = (float) info->fin[0] + flx * w * d;
	spy = (float) info->fin[1] + fly * w * d;

	inc = (w / 5.0f) * 3.0f / 3.0f;

	for (i = 0; i < r.num_karts; i++) {
		k = r.state.karts + i;
		kart_reset(k);
		k->race_state = &r.state;
		k->sprite.scale = 0.75f;

		if (i == 0) kart_index = r.player_kart;
		else kart_index = (int) (sbk_random() % kart_info_count);   // random kart info

		k->sprite.sheet = kart_info_surf[kart_index];
		k->kart_index = kart_index;

		k->sprite.look_dir.x = dirx;
		k->sprite.look_dir.y = 0.0f;
		k->sprite.look_dir.z = diry;
		k->sprite.world_pos.x = spx - k->sprite.look_dir.x * 48.0f;
		k->sprite.world_pos.y = 0.0f - k->sprite.look_dir.y * 48.0f;
		k->sprite.world_pos.z = spy - k->sprite.look_dir.z * 48.0f;

		if (i + 1 == 4) {
			// start_pos -= finish_line * inc * 2.5f
			spx = spx - flx * inc * 2.5f;
			spy = spy - fly * inc * 2.5f;
		} else {
			spx = spx + flx * inc;
			spy = spy + fly * inc;
		}
		spx = spx - dirx * 8.0f;
		spy = spy - diry * 8.0f;
	}

	// player kart
	r.state.karts[0].is_player = 1;
	r.cam.look_at = r.state.karts[0].sprite.world_pos;
	r.cam.pos.x = r.cam.look_at.x - r.state.karts[0].sprite.look_dir.x * 64.0f + 0.0f;
	r.cam.pos.y = r.cam.look_at.y - r.state.karts[0].sprite.look_dir.y * 64.0f + 16.0f;
	r.cam.pos.z = r.cam.look_at.z - r.state.karts[0].sprite.look_dir.z * 64.0f + 0.0f;
	r.cam.vx = 0;
	r.cam.vy = 0;
	r.cam.vw = SCREEN_W;
	r.cam.vh = SCREEN_H;
	cam_update(&r.cam);

	// reset track objects
	track_reset_objects(trk);

	// time trial setup
	if (r.mode == MODE_TIME_TRIAL) {
		if (r.time_trial_data) free((char *) r.time_trial_data);
		tt = (TimeTrialSave *) malloc(sizeof(TimeTrialSave));
		memset((char *) tt, 0, sizeof(TimeTrialSave));
		tt->save_version = 1;
		r.time_trial_data = tt;

		best_time = ~0u;
		worst_time = 0;
		best_slot = 0;
		worst_slot = 0;

		for (i = 0; i < 10; i++) {
			this_time = ~0u;
			slot = SAVE_SLOT(SAVE_TIME_TRIAL, r.track_index, i);
			if (read_save((void *) tt, sizeof(TimeTrialSave), slot) && tt->save_version == 1) {
				this_time = tt->lap_time[0];
				t = tt->lap_time[1];
				if (t < this_time) this_time = t;
				t = tt->lap_time[2];
				if (t < this_time) this_time = t;
			}
			if (this_time < best_time) {
				best_time = this_time;
				best_slot = i;
			} else if (this_time > worst_time) {
				worst_time = this_time;
				worst_slot = i;
			}
		}

		// this is where we'll save to
		r.worst_slot = worst_slot;
		r.worst_time = worst_time;

		r.state.karts[0].time_trial_data = tt;

		// attempt to load the "best" result as a ghost
		if (read_save((void *) tt, sizeof(TimeTrialSave), SAVE_SLOT(SAVE_TIME_TRIAL, r.track_index, best_slot))) {
			// move any existing data to the end of the buffer
			// (the new data will be recorded at the start)
			i = GHOST_MAX - tt->ghost_data_used;
			memmove((char *) (tt->ghost_data + i), (char *) tt->ghost_data, tt->ghost_data_used * sizeof(GhostEntry));

			k = r.state.karts + 1;
			k->time_trial_data = tt;
			k->ghost_offset = i;
			k->ghost_len = tt->ghost_data_used;
			k->sprite.alpha = 0.5f;
			k->sprite.sheet = kart_info_surf[tt->kart];
			k->kart_index = tt->kart;

			r.num_karts = 2;
			r.best_lap_time = best_time;
		} else
			r.num_karts = 1;

		// reset data used/version
		tt->save_version = 1;
		tt->ghost_data_used = 0;

		// disable items
		for (i = 0; i < trk->num_objects; i++)
			if (trk->objects[i].type == OBJ_ITEM) trk->objects[i].type = OBJ_REMOVED;
	}
}

// ---- menus ----

static void on_menu_activated(int id) {
	RaceSave save, old_save;
	TimeTrialSave *tt;
	int slot, have_old, i;
	unsigned best_lap, t;

	switch (id) {
	case MENU_RESTART:
		setup_race();
		r.show_end_menu = 0;
		break;
	case MENU_QUIT:
		game_change_state(main_menu_new(0));
		break;
	}

	// write save on continue/exit. After a restart this is the new race's
	// times, all nothing, over the old: upstream's way, kept.
	if (r.num_finished == r.num_karts) {
		if (r.mode == MODE_RACE) {
			save.save_version = 1;
			for (i = 0; i < NUM_KARTS; i++)
				if (r.finish_kart[i] == 0) break;
			save.place = i;
			save.time = kart_race_time(r.state.karts) / 10;

			slot = SAVE_SLOT(SAVE_RACE, r.track_index, 0);
			// overwrite if improved
			have_old = read_save((void *) &old_save, sizeof(RaceSave), slot) && old_save.save_version == 1 && old_save.time > 0;
			if (!have_old || save.place < old_save.place || (save.place == old_save.place && save.time < old_save.time))
				write_save((void *) &save, sizeof(RaceSave), slot);
		} else if (r.mode == MODE_TIME_TRIAL) {
			slot = SAVE_SLOT(SAVE_TIME_TRIAL, r.track_index, r.worst_slot);
			tt = r.time_trial_data;
			tt->kart = r.player_kart;
			for (i = 0; i < 3; i++)
				tt->lap_time[i] = kart_lap_time(r.state.karts, i) / 10;
			best_lap = tt->lap_time[0];
			t = tt->lap_time[1];
			if (t < best_lap) best_lap = t;
			t = tt->lap_time[2];
			if (t < best_lap) best_lap = t;
			// make sure this is actually an improvement
			if (best_lap < r.worst_time)
				write_save((void *) tt, sizeof(TimeTrialSave), slot);
		}
	}

	r.paused = 0;
}

// ---- Race::render ----

// big text helper
static void stretch_text(char *text, Font *font, int x, int y, float scale) {
	int w, h, dw, dh;
	unsigned char *buf;
	Surf s;
	w = measure_w(text, font);
	h = measure_h(font);
	buf = (unsigned char *) malloc(w * h);
	memset(buf, 0, w * h);
	text_bytes(text, font, buf, w, h);
	num_font_pen[0] = 0;
	num_font_pen[1] = scr_pen;
	s.data = buf;
	s.w = w;
	s.h = h;
	s.pal = num_font_pen;
	dw = (int) ((float) w * scale);
	dh = (int) ((float) h * scale);
	// dest.x -= bounds.w * scale / 2, in floats
	x = (int) ((float) x - (float) w * scale / 2.0f);
	y = (int) ((float) y - (float) h * scale / 2.0f);
	scr_stretch(&s, 0, 0, w, h, x, y, dw, dh, 0);
	free((char *) buf);
}

static void render_result(void) {
	char buf[40];
	int i, y, kart_idx, time, item_height;

	scr_pen = PEN(0, 0, 0, 150);
	scr_clear();
	scr_pen = RGB(255, 255, 255);

	item_height = tall_font.char_h + 1;
	y = (SCREEN_H - 8 * item_height) / 2;

	for (i = 0; i < NUM_KARTS; i++) {
		if (r.finish_time[i] == ~0u || i == r.num_karts) break;
		kart_idx = r.finish_kart[i];
		time = (int) (r.state.karts[kart_idx].finish_time - r.start_time);
		sprintf(buf, "%d - %s - %02d:%02d.%02d", i + 1, kart_idx == 0 ? "You" : "CPU",
			time / 60000, (time / 1000) % 60, (time % 1000) / 10);
		scr_text(buf, &tall_font, SCREEN_W / 2, y, 0, 0, ALIGN_TOP_CENTER);
		y += item_height;
	}

	if (r.show_end_menu) menu_render(&r.end_menu);
}

static void race_render(void) {
	static char *suffix[5] = { "", "st", "nd", "rd", "th" };
	char buf[20];
	char place_char[2];
	Kart *k0;
	Track *trk;
	TrackObject *ob;
	int i, num, place, bw, bh, col, cr, cg, cb, lap, cur, ix, iy, item, cx, cy, off_x, off_y, oldx, oldy, oldw, oldh, sx, mx, my;
	float scale;
	unsigned c0, c1;
	PerfRace *pf;

	pf = perf_race + r.track_index;
	c0 = sbk_perf ? plat_cycles() : 0;
	build_lists();
	mm_build();
	if (sbk_perf) { c1 = plat_cycles(); pf->frames++; pf->lists += c1 - c0; c0 = c1; }

	trk = r.state.track;
	k0 = r.state.karts;

	scr_pen = trk->info->bg;
	scr_clear();

	for (i = 0; i < r.nbelow; i++) sprite_render(r.below[i], &r.cam);

	if (sbk_perf) { c1 = plat_cycles(); pf->sprites += c1 - c0; c0 = c1; }
	track_render(trk, &r.cam, r.nbelow == 0);
	if (sbk_perf) { c1 = plat_cycles(); pf->mode7 += c1 - c0; c0 = c1; }

	for (i = 0; i < r.nlist; i++) sprite_render(r.list[i], &r.cam);
	if (sbk_perf) { c1 = plat_cycles(); pf->sprites += c1 - c0; c0 = c1; }

	scr_pen = RGB(255, 255, 255);

	if (r.state.countdown) {
		num = (int) ceilf((float) r.state.countdown / 1000.0f);
		scale = 1.0f + (1.0f - (float) (r.state.countdown % 1000) / 1000.0f);
		sprintf(buf, "%d", num);
		stretch_text(buf, &big_number_font, SCREEN_W / 2, SCREEN_H / 4, scale);
	}

	// place
	place = k0->current_place;
	place_char[0] = '0' + place;
	place_char[1] = 0;
	bw = measure_w(place_char, &big_number_font);
	bh = measure_h(&big_number_font);

	col = hsv_to_pen((float) (place - 1) / 8.0f, 1.0f, 0.7f);
	cr = (col >> 16) & 255;
	cg = (col >> 8) & 255;
	cb = col & 255;

	// "shadow"
	scr_pen = RGB(cr / 2, cg / 2, cb / 2);
	scr_text(place_char, &big_number_font, 9, 9, 0, 0, ALIGN_TOP_LEFT);
	scr_text(suffix[place < 4 ? place : 4], &tall_font, bw + 11, bh + 5, 0, 0, ALIGN_BOTTOM_LEFT);

	scr_pen = col;
	scr_text(place_char, &big_number_font, 8, 8, 0, 0, ALIGN_TOP_LEFT);
	scr_text(suffix[place < 4 ? place : 4], &tall_font, bw + 10, bh + 4, 0, 0, ALIGN_BOTTOM_LEFT);

	// lap count
	lap = (k0->current_lap > 0 ? k0->current_lap : 0) + 1;

	scr_pen = RGB(255, 255, 255);

	if (lap < 4) {
		sprintf(buf, "Lap %d/3", lap);
		scr_text(buf, &tall_font, 8, bh + 8, 0, 0, ALIGN_TOP_LEFT);
	}

	// current/best time
	if (r.mode == MODE_TIME_TRIAL) {
		cur = kart_lap_time(k0, k0->current_lap);
		sprintf(buf, "Time: %02d:%02d.%02d", cur / 60000, (cur / 1000) % 60, (cur % 1000) / 10);
		scr_text(buf, &tall_font, 8, bh + 20, 0, 0, ALIGN_TOP_LEFT);
		if (r.best_lap_time != ~0u) {
			sprintf(buf, "Best: %02d:%02d.%02d", (int) (r.best_lap_time / 6000), (int) ((r.best_lap_time / 100) % 60), (int) (r.best_lap_time % 100));
			scr_text(buf, &tall_font, 8, bh + 32, 0, 0, ALIGN_TOP_LEFT);
		}
	}

	// item
	scr_pen = RGB(0xFF, 0xFF, 0xFF);
	ix = SCREEN_W - ITEM_CONTAINER - 8;
	iy = 8;
	scr_hspan(ix, iy, ITEM_CONTAINER);
	scr_hspan(ix, iy + ITEM_CONTAINER - 1, ITEM_CONTAINER);
	scr_vspan(ix, iy, ITEM_CONTAINER);
	scr_vspan(ix + ITEM_CONTAINER - 1, iy, ITEM_CONTAINER);

	item = k0->current_item;
	if (item != ITEM_NONE) {
		cx = ix + ITEM_CONTAINER / 2;
		cy = iy + ITEM_CONTAINER / 2;
		scr_sprite_scaled(trk->tiles, item_sprite_x[item], 15, 1, 1, cx, cy, 4, 4, 2.0f, 0);
	}

	// minimap
	scr_blit(&mm_surf, 0, 0, MM_SIZE, MM_SIZE, SCREEN_W - MM_SIZE, SCREEN_H - MM_SIZE);

	// kart locations on minimap
	mx = SCREEN_W - r.mm_w;
	my = SCREEN_H - r.mm_h;
	oldx = clip_x; oldy = clip_y; oldw = clip_w; oldh = clip_h;
	scr_clip(mx, my, r.mm_w, r.mm_h);
	off_x = mx - r.mm_x;
	off_y = my - r.mm_y;
	for (i = 0; i < r.num_karts; i++) {
		sx = off_x + (int) (r.state.karts[i].sprite.world_pos.x / 8.0f) - 4;
		cy = off_y + (int) (r.state.karts[i].sprite.world_pos.z / 8.0f) - 4;
		// this will probably be a sprite eventually
		scr_pen = hsv_to_pen((float) i / 8.0f, 1.0f, 1.0f);
		scr_rect(sx, cy, 8, 8);
		num = r.state.karts[i].kart_index;
		scr_sprite(&a_kart_icons, num % 16, num / 16, 1, 1, sx, cy);
	}
	scr_clip(oldx, oldy, oldw, oldh);

	// we've finished - display the result
	if (kart_finished(k0)) render_result();
	else if (r.paused) {
		scr_pen = PEN(0, 0, 0, 150);
		scr_clear();
		menu_render(&r.pause_menu);
	}
	if (sbk_perf) pf->hud += plat_cycles() - c0;
}

// ---- Race::update ----

static void race_update(unsigned time) {
	Kart *k, *k0;
	Track *trk;
	int kart_idx[NUM_KARTS];
	float progress[NUM_KARTS];
	unsigned ftime;
	float p, z, tx, ty, tz, px, py, pz, lx, ly, lz;
	int i, j, new_num_finished, route_len;
	unsigned c0;

	k0 = r.state.karts;
	c0 = sbk_perf ? plat_cycles() : 0;

	// toggle pause menu, but not after the race ends
	if ((btn_pressed & BTN_MENU) && !kart_finished(k0)) r.paused = !r.paused;

	if (r.paused) {
		menu_update(&r.pause_menu, time);
		return;
	}

	if (r.state.countdown) {
		r.state.countdown -= 10;
		if (!r.state.countdown) r.start_time = sbk_now;
	}

	trk = r.state.track;
	route_len = trk->info->route_len;
	new_num_finished = 0;

	for (i = 0; i < r.num_karts; i++) {
		k = r.state.karts + i;
		kart_update(k);
		if (kart_finished(k)) new_num_finished++;
		// approx progress through race: stable by insertion, as std::sort of so few
		p = (float) (k->current_lap * route_len) + ((float) k->current_route_segment + k->current_route_frac);
		for (j = i; j > 0 && p > progress[j - 1]; j--) {
			progress[j] = progress[j - 1];
			kart_idx[j] = kart_idx[j - 1];
		}
		progress[j] = p;
		kart_idx[j] = i;
	}

	// objects: TrackObject::update
	{
		TrackObject *ob;
		TrackInfo *info;
		RectF *rect;
		int n, m;
		float radius, vx, vy, ox, oy, dist, pen, dot;
		info = trk->info;
		n = trk->num_objects;
		ob = trk->objects;
		for (i = 0; i < n; i++, ob++) {
			if (ob->respawn_timer) ob->respawn_timer -= 10;
			if (ob->type != OBJ_PROJECTILE) continue;
			ob->sprite.world_pos.x = ob->sprite.world_pos.x + ob->vel.x * 0.01f;
			ob->sprite.world_pos.y = ob->sprite.world_pos.y + ob->vel.y * 0.01f;
			ob->sprite.world_pos.z = ob->sprite.world_pos.z + ob->vel.z * 0.01f;
			// collide with track obstacles
			rect = info->rects;
			for (m = 0; m < info->num_rects; m++, rect++) {
				if (rect->w <= 0 || rect->h <= 0) continue;
				px = ob->sprite.world_pos.x;
				py = ob->sprite.world_pos.z;
				radius = (float) ob->sprite.size_w * 4.0f;
				ox = px;
				oy = py;
				if (px < rect->fx) ox = rect->fx;
				else if (px >= rect->fx2) ox = rect->fx2;
				if (py < rect->fy) oy = rect->fy;
				else if (py >= rect->fy2) oy = rect->fy2;
				vx = px - ox;
				vy = py - oy;
				dist = sqrtf(vx * vx + vy * vy);
				if (dist > radius) continue;
				vx = vx / dist;
				vy = vy / dist;
				pen = radius - dist;
				ob->sprite.world_pos.x = ob->sprite.world_pos.x + vx * pen;
				ob->sprite.world_pos.y = ob->sprite.world_pos.y + 0.0f * pen;
				ob->sprite.world_pos.z = ob->sprite.world_pos.z + vy * pen;
				// boing
				dot = vx * ob->vel.x + 0.0f * ob->vel.y + vy * ob->vel.z;
				ob->vel.x = ob->vel.x - vx * dot * 2.0f;
				ob->vel.y = ob->vel.y - 0.0f * dot * 2.0f;
				ob->vel.z = ob->vel.z - vy * dot * 2.0f;
			}
		}
	}

	// sorted by progress as they went
	for (i = 0; i < r.num_karts; i++) r.state.karts[kart_idx[i]].current_place = i + 1;

	// update the results when someone finishes
	if (new_num_finished != r.num_finished) {
		for (i = 0; i < NUM_KARTS; i++) {
			k = r.state.karts + i;
			ftime = kart_finished(k) ? k->finish_time : ~0u;
			for (j = i; j > 0 && ftime < r.finish_time[j - 1]; j--) {
				r.finish_time[j] = r.finish_time[j - 1];
				r.finish_kart[j] = r.finish_kart[j - 1];
			}
			r.finish_time[j] = ftime;
			r.finish_kart[j] = i;
		}
	}

	r.num_finished = new_num_finished;

	// end of race restart/quit menu
	if (r.show_end_menu) menu_update(&r.end_menu, time);

	// don't show the menu if A is still held to avoid accidentally restarting
	if (r.num_finished == r.num_karts && !(btn_state & BTN_A)) r.show_end_menu = 1;

	// update camera
	tx = k0->sprite.world_pos.x;
	ty = k0->sprite.world_pos.y;
	tz = k0->sprite.world_pos.z;
	lx = k0->sprite.look_dir.x;
	ly = k0->sprite.look_dir.y;
	lz = k0->sprite.look_dir.z;
	px = tx - lx * 64.0f + 0.0f;
	py = ty - ly * 64.0f + 16.0f;
	pz = tz - lz * 64.0f + 0.0f;

	r.cam.look_at.x = r.cam.look_at.x + (tx - r.cam.look_at.x) * 0.1f;
	r.cam.look_at.y = r.cam.look_at.y + (ty - r.cam.look_at.y) * 0.1f;
	r.cam.look_at.z = r.cam.look_at.z + (tz - r.cam.look_at.z) * 0.1f;

	if (kart_finished(k0)) {
		// spin camera around after race finished
		r.cam.pos.x = r.cam.pos.x + (tx - r.cam.look_at.x) * 0.1f;   // follow
		r.cam.pos.y = r.cam.pos.y + (ty - r.cam.look_at.y) * 0.1f;
		r.cam.pos.z = r.cam.pos.z + (tz - r.cam.look_at.z) * 0.1f;

		r.cam.pos.x = r.cam.pos.x - r.cam.look_at.x;
		r.cam.pos.y = r.cam.pos.y - r.cam.look_at.y;
		r.cam.pos.z = r.cam.pos.z - r.cam.look_at.z;
		rot_y(&r.cam.pos, 0.2f);
		r.cam.pos.x = r.cam.pos.x + r.cam.look_at.x;
		r.cam.pos.y = r.cam.pos.y + r.cam.look_at.y;
		r.cam.pos.z = r.cam.pos.z + r.cam.look_at.z;
	} else {
		r.cam.pos.x = r.cam.pos.x + (px - r.cam.pos.x) * 0.03f;
		r.cam.pos.y = r.cam.pos.y + (py - r.cam.pos.y) * 0.03f;
		r.cam.pos.z = r.cam.pos.z + (pz - r.cam.pos.z) * 0.03f;
	}

	// prevent the camera going below the track
	r.cam.pos.y = 1.0f < r.cam.pos.y ? r.cam.pos.y : 1.0f;
	cam_update(&r.cam);

	// cull/sort: done when a frame is drawn
	r.lists_dirty = 1;

	// minimap: the same
	mm_update((int) (tx / 8.0f), (int) (tz / 8.0f));

	if (sbk_perf) {
		perf_race[r.track_index].updates++;
		perf_race[r.track_index].update += plat_cycles() - c0;
	}
}

static void race_destroy(void) {
	track_free(r.state.track);
	if (r.time_trial_data) free((char *) r.time_trial_data);
	r.time_trial_data = 0;
	if (r.list) free((char *) r.list);
	if (r.below) free((char *) r.below);
	r.list = 0;
	r.below = 0;
	free((char *) r.mm_data);
}

GameState *race_new(int player_kart, int track_index, int mode) {
	Menu *m;
	int h;
	memset((char *) &r, 0, sizeof(r));
	r.player_kart = player_kart;
	r.track_index = track_index;
	r.mode = mode;
	r.num_karts = NUM_KARTS;
	r.worst_time = ~0u;
	cam_init(&r.cam);
	r.state.track = track_new(track_info + track_index);
	r.state.countdown = 3000;

	r.mm_data = (unsigned char *) malloc(MM_SIZE * MM_SIZE);
	memset(r.mm_data, 0, MM_SIZE * MM_SIZE);
	mm_surf.data = r.mm_data;
	mm_surf.w = MM_SIZE;
	mm_surf.h = MM_SIZE;
	mm_surf.pal = mm_palette;
	mm_palette[0] = PEN(0, 0, 0, 0);
	mm_palette[1] = PEN(0, 0, 0, 128);
	mm_palette[2] = PEN(128, 128, 128, 128);
	mm_palette[3] = PEN(255, 255, 255, 128);

	setup_race();

	m = &r.pause_menu;
	menu_init(m, "Paused", &tall_font);
	menu_add(m, MENU_CONTINUE, "Continue");
	menu_add(m, MENU_RESTART, "Restart");
	menu_add(m, MENU_QUIT, "Quit");
	menu_set_rect(m, (SCREEN_W - 90) / 2, (SCREEN_H - 90) / 2, 90, 90);
	m->on_item = on_menu_activated;

	m = &r.end_menu;
	menu_init(m, "", &tall_font);
	menu_add(m, MENU_RESTART, "Restart");
	menu_add(m, MENU_QUIT, "Quit");
	h = (tall_font.char_h + 10) * 2;
	menu_set_rect(m, (SCREEN_W - 90) / 2, SCREEN_H - (h + 16), 90, h);
	m->on_item = on_menu_activated;

	race_game_state.update = race_update;
	race_game_state.render = race_render;
	race_game_state.destroy = race_destroy;
	return &race_game_state;
}
