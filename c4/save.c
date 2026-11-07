// save.c -- blit::read_save and write_save: a file in the current directory for
// each slot, named so that C4DOS could hold it too. A disk that can't take one
// costs the save when the game ends (src/game.c keeps them till then), not the race.

#include <stdio.h>
#include "plat.h"

static void save_name(char *buf, int slot) { sprintf(buf, "S%05d.SAV", slot); }

int plat_save_read(int slot, char *data, int len) {
	char name[16];
	FILE *f;
	int n;
	save_name(name, slot);
	f = fopen(name, "rb");
	if (!f) return 0;
	n = fread(data, 1, len, f);
	fclose(f);
	return n == len;
}

void plat_save_write(int slot, char *data, int len) {
	char name[16];
	FILE *f;
	save_name(name, slot);
	f = fopen(name, "wb");
	if (!f) return;
	fwrite(data, 1, len, f);
	fclose(f);
}
