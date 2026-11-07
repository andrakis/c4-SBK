// states.c -- the menus: upstream's main-menu.cpp, kart-select.cpp and
// track-select.cpp, each a state of the game (game.c)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sbk.h"

// kart-info.hpp: the same sprites, each with a palette
int kart_info_count = 2;
char *kart_info_name[2] = { "Race-A-Tron 3000", "Race-A-Tron 2000" };
Surf *kart_info_surf[2] = { &a_kart_alt, &a_kart };

// ---- main-menu.cpp ----

enum { MENU_TARGET_Y = 96, LOGO_TARGET_Y = 8 };
enum { MM_RACE = 0, MM_TIME_TRIAL, MM_MULTIPLAYER };

typedef struct MKart { int x, y, kart, speed; } MKart;

static struct {
	int display_menu, intro_done, logo_y, fade;
	MKart karts[4];
	Menu menu;
	char *message;
} mm;

static GameState mm_state;

static void mm_on_item(int id) {
	switch (id) {
	case MM_RACE: game_change_state(kart_select_new(MODE_RACE)); break;
	case MM_TIME_TRIAL: game_change_state(kart_select_new(MODE_TIME_TRIAL)); break;
	case MM_MULTIPLAYER: mm.message = "Maybe later : )"; break;
	}
}

static void mm_update(unsigned time) {
	int i, j;
	unsigned r;
	MKart k;
	MKart *kp;

	if (mm.fade) mm.fade--;

	if (!mm.intro_done) {
		for (i = 0; i < 4; i++) {
			kp = mm.karts + i;
			if (kp->x < -32 || kp->x > SCREEN_W) {
				// maybe reset
				r = sbk_random();
				if (r & 0x1FF) continue;
				if (((r >> 10) & 0x3F) == 0) {
					// go backwards for fun
					kp->x = SCREEN_W;
					kp->speed = -1;
				} else {
					kp->x = -32;
					kp->speed = (int) ((r >> 16) & 3) + 1;
				}
				kp->y = SCREEN_H - 44 + (int) ((r >> 18) & 0xF);
				kp->kart = (int) ((r >> 22) & 1);
			}
			kp->x += kp->speed;
		}
		// sort by y: std::sort on so few is an insertion sort
		for (i = 1; i < 4; i++) {
			k = mm.karts[i];
			for (j = i; j > 0 && k.y < mm.karts[j - 1].y; j--) mm.karts[j] = mm.karts[j - 1];
			mm.karts[j] = k;
		}
	}

	if (!mm.display_menu) {
		if (btn_released) mm.display_menu = 1;
		return;
	}

	// transition
	if (mm.logo_y > LOGO_TARGET_Y) {
		mm.logo_y--;
		return;
	}

	if (mm.menu.dy > MENU_TARGET_Y) mm.menu.dy--;
	else mm.intro_done = 1;

	// Esc again on the main menu: the game gives the machine back
	if (mm.intro_done && (btn_released & BTN_MENU)) sbk_quit = 1;

	menu_update(&mm.menu, time);
}

static void mm_render(void) {
	int i;

	scr_pen = RGB(0x63, 0x9b, 0xff);      // "sky" colour
	scr_clear();

	scr_blit(&a_logo, 0, 0, a_logo.w, a_logo.h, (SCREEN_W - a_logo.w) / 2, mm.logo_y);

	// road
	scr_pen = RGB(0x59, 0x56, 0x52);
	scr_rect(0, SCREEN_H - 24, SCREEN_W, 24);

	if (!mm.display_menu && sbk_now % 1000 < 500) {
		scr_pen = RGB(255, 255, 255);
		scr_textat("Press a button!", &tall_font, SCREEN_W / 2, SCREEN_H - 40, ALIGN_CENTER);
	}

	// karts: display until menu has finished scrolling
	if (!mm.intro_done)
		for (i = 0; i < 4; i++)
			scr_sprite(&a_menu_sprites, mm.karts[i].kart * 4, 0, 4, 4, mm.karts[i].x, mm.karts[i].y);

	menu_render(&mm.menu);

	if (mm.message)
		scr_textat(mm.message, &tall_font, SCREEN_W / 2, SCREEN_H - 20, ALIGN_CENTER);

	if (mm.fade) {
		scr_pen = PEN(0, 0, 0, mm.fade);
		scr_clear();
	}
}

static void mm_destroy(void) {}

GameState *main_menu_new(int initial) {
	int i, menu_y;
	memset((char *) &mm, 0, sizeof(mm));
	if (initial) {
		mm.logo_y = ((SCREEN_H - 16) - a_logo.h) / 2;
		menu_y = SCREEN_H;
		mm.fade = 0xFF;
	} else {
		mm.logo_y = LOGO_TARGET_Y;
		menu_y = MENU_TARGET_Y;
		mm.display_menu = 1;
		mm.intro_done = 1;
		mm.fade = 0;
	}
	for (i = 0; i < 4; i++) mm.karts[i].x = -64;
	menu_init(&mm.menu, "", &menu_font);
	menu_add(&mm.menu, MM_RACE, "Race");
	menu_add(&mm.menu, MM_TIME_TRIAL, "Time Trial");
	menu_add(&mm.menu, MM_MULTIPLAYER, "Multiplayer");
	menu_set_rect(&mm.menu, 0, menu_y, SCREEN_W, SCREEN_H - MENU_TARGET_Y);
	mm.menu.on_item = mm_on_item;
	mm_state.update = mm_update;
	mm_state.render = mm_render;
	mm_state.destroy = mm_destroy;
	return &mm_state;
}

// ---- kart-select.cpp ----

static struct {
	int mode, preview_index;
	Menu kart_menu;
	Surf *kart_sprites;
} ks;

static GameState ks_state;

static void ks_on_item(int id) {
	game_change_state(track_select_new(id, ks.mode));
}

static void ks_update(unsigned time) {
	int item;
	menu_update(&ks.kart_menu, time);
	if (btn_released & BTN_B) {
		game_change_state(main_menu_new(0));
		return;
	}
	item = ks.kart_menu.current_item;
	if (item != ks.preview_index) {
		ks.preview_index = item;
		ks.kart_sprites = kart_info_surf[item];
	}
}

static void ks_render(void) {
	int frame, transform;
	scr_pen = RGB(127, 127, 127);
	scr_clear();
	if (ks.kart_sprites) {
		frame = (int) ((sbk_now / 100) % 30);
		transform = 0;
		if (frame >= 16) {
			frame = (16 - 2) - (frame - 16);
			transform = XFORM_HORIZONTAL;
		}
		scr_sprite_scaled(ks.kart_sprites, frame * 4, 0, 4, 4, SCREEN_W / 2 - 60, SCREEN_H / 2, 16, 16, 3.0f, transform);
	}
	menu_render(&ks.kart_menu);
}

static void ks_destroy(void) {}

GameState *kart_select_new(int mode) {
	int i;
	memset((char *) &ks, 0, sizeof(ks));
	ks.mode = mode;
	ks.preview_index = -1;
	menu_init(&ks.kart_menu, "", &tall_font);
	menu_set_rect(&ks.kart_menu, SCREEN_W - 120, 0, 120, SCREEN_H);
	for (i = 0; i < kart_info_count; i++) menu_add(&ks.kart_menu, i, kart_info_name[i]);
	ks.kart_menu.on_item = ks_on_item;
	ks_state.update = ks_update;
	ks_state.render = ks_render;
	ks_state.destroy = ks_destroy;
	return &ks_state;
}

// ---- track-select.cpp ----

static struct {
	int player_kart, mode, preview_index;
	Menu track_menu;
	Camera cam;
	Track *track;
	RaceSave race_save;
	int lap_times[3];
	int race_save_loaded;
} tsel;

static GameState ts_state;

static void ts_on_item(int id) {
	game_change_state(race_new(tsel.player_kart, id, tsel.mode));
}

static void ts_load_time_trial_data(void) {
	int i, track_index;
	unsigned best_time, this_time, t;
	TimeTrialSave *save;
	track_index = tsel.track_menu.current_item;
	best_time = ~0u;
	save = (TimeTrialSave *) malloc(sizeof(TimeTrialSave));
	for (i = 0; i < 10; i++) {
		this_time = ~0u;
		if (read_save((void *) save, sizeof(TimeTrialSave), SAVE_SLOT(SAVE_TIME_TRIAL, track_index, i)) && save->save_version == 1) {
			this_time = save->lap_time[0];
			t = save->lap_time[1];
			if (t < this_time) this_time = t;
			t = save->lap_time[2];
			if (t < this_time) this_time = t;
		}
		if (this_time < best_time) {
			best_time = this_time;
			tsel.lap_times[0] = save->lap_time[0];
			tsel.lap_times[1] = save->lap_time[1];
			tsel.lap_times[2] = save->lap_time[2];
		}
	}
	free((char *) save);
	tsel.race_save_loaded = best_time != ~0u;
}

static void ts_update(unsigned time) {
	int item, w;
	TrackInfo *info;
	menu_update(&tsel.track_menu, time);
	if (btn_released & BTN_B) {
		game_change_state(kart_select_new(tsel.mode));
		return;
	}
	item = tsel.track_menu.current_item;
	if (item != tsel.preview_index) {
		tsel.preview_index = item;
		if (tsel.track) track_free(tsel.track);
		info = track_info + item;
		tsel.track = track_new(info);
		if (tsel.mode == MODE_RACE)
			tsel.race_save_loaded = read_save((void *) &tsel.race_save, sizeof(RaceSave), SAVE_SLOT(SAVE_RACE, item, 0));
		else
			ts_load_time_trial_data();
		w = info->w;
		tsel.cam.look_at.x = (float) w * 4.0f;
		tsel.cam.look_at.y = 0.0f;
		tsel.cam.look_at.z = (float) info->h * 4.0f;
		tsel.cam.pos.x = tsel.cam.look_at.x + 4.0f * (float) w;
		tsel.cam.pos.y = tsel.cam.look_at.y + 1.0f * (float) w;
		tsel.cam.pos.z = tsel.cam.look_at.z + 4.0f * (float) w;
		tsel.track->fog = 50.0f / (float) (w / 128);    // less fog and even less for desert
	}
}

static void ts_render(void) {
	static char *suffix[4] = { "st", "nd", "rd", "th" };
	char buf[40];
	int place, iy, ih;
	Camera *c;
	if (!tsel.track) return;
	c = &tsel.cam;
	scr_pen = tsel.track->info->bg;
	scr_rect(c->vx, c->vy, c->vw, c->vh);

	c->pos.x = c->pos.x - c->look_at.x;
	c->pos.y = c->pos.y - c->look_at.y;
	c->pos.z = c->pos.z - c->look_at.z;
	rot_y(&c->pos, -0.5f);
	c->pos.x = c->pos.x + c->look_at.x;
	c->pos.y = c->pos.y + c->look_at.y;
	c->pos.z = c->pos.z + c->look_at.z;
	cam_update(c);

	track_render(tsel.track, c, 1);
	menu_render(&tsel.track_menu);

	// some info
	scr_pen = RGB(0, 0, 0);
	iy = c->vh;
	ih = SCREEN_H - c->vh;
	scr_rect(0, iy, c->vw, ih);

	if (tsel.race_save_loaded) {
		scr_pen = RGB(255, 255, 255);
		if (tsel.mode == MODE_RACE) {
			place = tsel.race_save.place;
			sprintf(buf, "Best: %d%s place in %02d:%02d.%02d", place + 1, suffix[place < 3 ? place : 3],
				tsel.race_save.time / 6000, (tsel.race_save.time % 6000) / 100, tsel.race_save.time % 100);
		} else {
			sprintf(buf, "Best: %02d:%02d.%02d %02d:%02d.%02d %02d:%02d.%02d",
				tsel.lap_times[0] / 6000, (tsel.lap_times[0] % 6000) / 100, tsel.lap_times[0] % 100,
				tsel.lap_times[1] / 6000, (tsel.lap_times[1] % 6000) / 100, tsel.lap_times[1] % 100,
				tsel.lap_times[2] / 6000, (tsel.lap_times[2] % 6000) / 100, tsel.lap_times[2] % 100);
		}
		scr_text(buf, &tall_font, 4, iy + 4, c->vw - 8, ih - 8, ALIGN_TOP_LEFT);
	}
}

static void ts_destroy(void) {
	if (tsel.track) track_free(tsel.track);
	tsel.track = 0;
}

GameState *track_select_new(int player_kart, int mode) {
	int i;
	memset((char *) &tsel, 0, sizeof(tsel));
	tsel.player_kart = player_kart;
	tsel.mode = mode;
	tsel.preview_index = -1;
	menu_init(&tsel.track_menu, "", &tall_font);
	menu_set_rect(&tsel.track_menu, SCREEN_W - 80, 0, 80, SCREEN_H);
	cam_init(&tsel.cam);
	tsel.cam.vx = 0;
	tsel.cam.vy = 0;
	tsel.cam.vw = SCREEN_W - 80;
	tsel.cam.vh = SCREEN_H - 40;
	tsel.cam.far_z = 10000.0f;     // basically don't clip
	for (i = 0; i < num_tracks; i++) menu_add(&tsel.track_menu, i, track_info[i].name);
	tsel.track_menu.on_item = ts_on_item;
	ts_state.update = ts_update;
	ts_state.render = ts_render;
	ts_state.destroy = ts_destroy;
	return &ts_state;
}
