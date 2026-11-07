// plat-c4ix.c -- the game on C4IX: the display is the desktop's, which gives it
// way until the game ends (include/display.h), or else the Terminal window the
// game was started from (window.h). gui.h for its declarations only: video.c
// carries its functions.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define C4IX_SHARED 1
#include "gui.h"
#undef C4IX_SHARED
#include "display.h"
#include "window.h"
#include "plat.h"

int  plat_take_display(void) { return udisplay_take(0); }
void plat_give_display(void) { udisplay_give(); }

// The desktop scales what it is given to the window, whose shape is not the
// game's, so the frame goes in the middle of a wider one that is: 384 x 240 is
// 640 x 400, and the sides stay palette 0, black. The desktop reads the frame
// when it next draws and says so (WIN_FRAME); until then no more go.
enum { WF_W = 384, WF_H = 240, WF_X = 32 };
static int waiting, closed;
static char *wframe;

int plat_win_open(char *title, int w, int h) {
	(void) w; (void) h;
	wframe = (char *) calloc(WF_W * WF_H, 1);
	if (!wframe) return 0;
	return win_open(title);
}

int plat_win_ready(void) { return !closed && !waiting; }

void plat_win_frame(char *pixels, int w, int h, int pitch, int *pal) {
	int y;
	for (y = 0; y < h; y++)
		memcpy(wframe + y * WF_W + WF_X, pixels + y * pitch, w);
	win_fb(wframe, WF_W, WF_H, WF_W, pal);
	win_present();
	waiting = 1;
}

int plat_win_poll(int *ev) {
	int w[3], k;
	if (closed) return -1;
	while (1) {
		k = win_poll(w);
		if (k == WIN_NONE) return 0;
		if (k < 0) { closed = 1; return -1; }
		if (k == WIN_FRAME) { waiting = 0; continue; }
		if (k == WIN_KEY || k == WIN_KEYUP) {
			ev[0] = k == WIN_KEY ? GUI_EV_KEYDOWN : GUI_EV_KEYUP;
			ev[1] = w[0]; ev[2] = w[1]; ev[3] = w[2];
			return 1;
		}
	}
}

void plat_win_close(void) { if (!closed) { win_close(); closed = 1; } }

int  plat_ms(void) { return __time(); }
void plat_sleep(int ms) { if (ms > 0) __c4_usleep(ms * 1000); }
void plat_print(char *s) { fputs(s, stdout); fflush(stdout); }
unsigned plat_cycles(void) { return (unsigned) __c4_cycles(); }
void plat_shot(char *file, int *fb) { (void) file; (void) fb; }
