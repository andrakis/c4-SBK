// video.c -- the game's framebuffer to libjs's display, or to a window: C4IX's,
// the Terminal it was started from, or one of grin's under C4DOS. The display
// takes the frame as it is (gui_fb, 0x00RRGGBB), so that costs the game nothing.
// A window takes 8 bits through a palette, so a frame in one goes through a
// table to the nearest of 252 colours. The keys are 32blit's SDL build's,
// with Enter, Space and Esc added.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sbk.h"
#include "plat.h"
#include "gui.h"

enum { RING = 65536, LUT_BITS = 15 };

static int on, win;
static int *ring;
static unsigned char *lut, *frame8;
static int *pal8;

static void make_palette(void) {
	int i, r, g, b, x, y, z;
	pal8 = (int *) malloc(256 * 4);
	memset((char *) pal8, 0, 256 * 4);
	for (r = 0; r < 6; r++)
		for (g = 0; g < 7; g++)
			for (b = 0; b < 6; b++)
				pal8[(r * 7 + g) * 6 + b] = ((r * 255 / 5) << 16) | ((g * 255 / 6) << 8) | (b * 255 / 5);
	lut = (unsigned char *) malloc(1 << LUT_BITS);
	for (i = 0; i < (1 << LUT_BITS); i++) {
		// the middle of each of 32 steps, to the nearest of 6 or 7
		x = ((i >> 10) & 31) * 8 + 4;
		y = ((i >> 5) & 31) * 8 + 4;
		z = (i & 31) * 8 + 4;
		r = (x * 5 + 127) / 255;
		g = (y * 6 + 127) / 255;
		b = (z * 5 + 127) / 255;
		lut[i] = (r * 7 + g) * 6 + b;
	}
	frame8 = (unsigned char *) malloc(SCREEN_W * SCREEN_H);
}

int video_open(int window) {
	if (!window && gui_present() && plat_take_display()) {
		ring = (int *) malloc(RING);
		if (!ring || !gui_attach(ring, RING, 640, 480)) {
			plat_give_display();
			return 0;
		}
		gui_events(GUI_M_KEYS);
		gui_cursor(GUI_PTR_NONE);
		gui_fb(fb, SCREEN_W * 4);
		on = 1;
		return 1;
	}
	make_palette();
	if (plat_win_open("Super Blit Kart", SCREEN_W, SCREEN_H)) {
		win = 1;
		return 1;
	}
	return 0;
}

static int button_of(int key) {
	switch (key) {
	case 37: case 65: return BTN_LEFT;     // arrows, WASD
	case 39: case 68: return BTN_RIGHT;
	case 38: case 87: return BTN_UP;
	case 40: case 83: return BTN_DOWN;
	case 90: case 85: case 13: return BTN_A;     // Z, U, Enter
	case 88: case 73: case 8: return BTN_B;      // X, I, Backspace
	case 67: case 79: case 32: return BTN_X;     // C, O, Space
	case 86: case 80: return BTN_Y;              // V, P
	case 50: case 27: return BTN_MENU;           // 2, Esc
	case 49: return BTN_HOME;
	case 51: return BTN_JOYSTICK;
	}
	return 0;
}

int video_poll(void) {
	int ev[4], k;
	while (1) {
		if (on) k = gui_poll(ev);
		else {
			k = plat_win_poll(ev);
			if (k < 0) return 0;
		}
		if (!k) return 1;
		if (ev[0] == GUI_EV_KEYDOWN) btn_state |= button_of(ev[1]);
		else if (ev[0] == GUI_EV_KEYUP) btn_state &= ~button_of(ev[1]);
	}
}

void video_show(void) {
	int *s, *end;
	int v;
	unsigned char *d;
	if (on) {
		gui_flip(SCREEN_W, SCREEN_H);
		return;
	}
	if (!win || !plat_win_ready()) return;
	s = fb;
	end = fb + SCREEN_W * SCREEN_H;
	d = frame8;
	while (s < end) {
		v = *s++;
		*d++ = lut[((v >> 9) & 0x7C00) | ((v >> 6) & 0x3E0) | ((v >> 3) & 31)];
	}
	plat_win_frame((char *) frame8, SCREEN_W, SCREEN_H, SCREEN_W, pal8);
}

void video_close(void) {
	if (on) {
		gui_cursor(GUI_PTR_ARROW);
		gui_detach();
		plat_give_display();
		on = 0;
	}
	if (win) {
		plat_win_close();
		win = 0;
	}
}
