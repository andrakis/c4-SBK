// plat-bare.c -- the game on the bare machine and under C4DOS: the display is the
// game's to take. Under grin, C4DOS's graphical shell, the display is grin's,
// and grin publishes a table in C4DOS's slot 25 while it has it (c4's
// docs/grin-plan.md 3.6): then the game plays in a window of grin's, and grin's
// loop runs in its polls. As games/tyrian/tyr-bare.c does.

#include <stdio.h>
#include <stdlib.h>
#define C4IX_SHARED 1
#include "gui.h"
#undef C4IX_SHARED
#include "plat.h"

// grin's table (c4's src/grin/gkit.h): its first eight words
struct grin_table {
	int magic, version, gen;
	void *env;
	void (*hello)(void);
	int  (*fwin_open)(char *title, int w, int h, int flags);
	void (*fwin_frame)(char *pixels, int w, int h, int pitch, int *pal);
	int  (*fwin_poll)(int *ev);
};
extern int *__c4dos_api;                // bare.c's: C4DOS's table, 0 on the bare machine

static struct grin_table *grin(void) {
	struct grin_table *t;
	if (!__c4dos_api || __c4dos_api[0] != 4404292 || __c4dos_api[5] < 4) return 0;
	t = (struct grin_table *) __c4dos_api[25];
	return t && t->magic == 1196575054 && t->version >= 2 ? t : 0;
}

static struct grin_table *g;

int  plat_take_display(void) { return !grin(); }
void plat_give_display(void) {}
int  plat_win_open(char *title, int w, int h) { return (g = grin()) != 0 && g->fwin_open(title, w, h, 0); }
int  plat_win_ready(void) { return g != 0; }
void plat_win_frame(char *pixels, int w, int h, int pitch, int *pal) {
	if (g) g->fwin_frame(pixels, w, h, pitch, pal);
}
int  plat_win_poll(int *ev) { return g ? g->fwin_poll(ev) : 0; }
void plat_win_close(void) {}

int  plat_ms(void) { return __time(); }
void plat_sleep(int ms) { if (ms > 0) __c4_usleep(ms * 1000); }
void plat_print(char *s) { fputs(s, stdout); fflush(stdout); }
unsigned plat_cycles(void) { return (unsigned) __c4_cycles(); }
void plat_shot(char *file, int *fb) { (void) file; (void) fb; }
