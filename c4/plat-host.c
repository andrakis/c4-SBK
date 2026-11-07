// plat-host.c -- gcc's build (make sbk-host), the reference test-sbk holds c4lc's
// to: no display and no window, so only -demo runs.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include "plat.h"

int plat_ms(void) {
	struct timeval tv;
	gettimeofday(&tv, 0);
	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
void plat_sleep(int ms) { usleep(ms * 1000); }
void plat_print(char *s) { fputs(s, stdout); }
unsigned plat_cycles(void) { return 0; }

// a frame to look at, without a browser
void plat_shot(char *file, int *fb) {
	FILE *f;
	int i;
	f = fopen(file, "wb");
	if (!f) return;
	fprintf(f, "P6\n320 240\n255\n");
	for (i = 0; i < 320 * 240; i++) {
		fputc((fb[i] >> 16) & 255, f);
		fputc((fb[i] >> 8) & 255, f);
		fputc(fb[i] & 255, f);
	}
	fclose(f);
}

int  plat_save_read(int slot, char *data, int len) { (void) slot; (void) data; (void) len; return 0; }
void plat_save_write(int slot, char *data, int len) { (void) slot; (void) data; (void) len; }
int  video_open(int window) { (void) window; return 0; }
int  video_poll(void) { return 0; }
void video_show(void) {}
void video_close(void) {}
