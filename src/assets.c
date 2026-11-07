// assets.c -- the art, from the array c4/mkassets.py generates (.sbk/gen/sbk-assets.c),
// unpacked once when the game starts. Upstream's Surface::load and
// TileMap::load_tmx, and what its extract_track_info.py compiles in, all as data
// here; the art itself is not in this repository (data/README.md).

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "sbk.h"

extern char *sbk_blob;

TrackInfo track_info[MAX_TRACKS];
int num_tracks = MAX_TRACKS;
Surf a_tileset, a_track, a_kart, a_kart_alt, a_logo, a_menu_sprites, a_kart_icons;
Font tall_font, big_number_font, menu_font;

static unsigned char *p;

static int rd32(void) {
	int v;
	v = p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
	p += 4;
	return v;
}

// PackBits, as c4/mkassets.py writes it
static void unrle(unsigned char *out, int n) {
	unsigned char *end;
	int c;
	end = p + n;
	while (p < end) {
		c = *p++;
		if (c < 128) {
			c = c + 1;
			memcpy(out, p, c);
			p += c;
			out += c;
		} else {
			c = 257 - c;
			memset(out, *p++, c);
			out += c;
		}
	}
}

// a palette of 256 pens (new Pen[256]: those past the image's own are nothing)
static int *rd_palette(int n) {
	int *pal;
	int i;
	pal = (int *) malloc(256 * 4);
	memset(pal, 0, 256 * 4);
	for (i = 0; i < n; i++) {
		pal[i] = PEN(p[0], p[1], p[2], p[3]);
		p += 4;
	}
	return pal;
}

static void rd_image(Surf *s) {
	int n, np;
	s->w = rd32();
	s->h = rd32();
	np = rd32();
	s->ncol = np;
	s->pal = rd_palette(np);
	s->data = (unsigned char *) malloc(s->w * s->h);
	n = rd32();
	unrle(s->data, n);
}

static void rd_font(Font *f) {
	int num, full, n;
	num = rd32();
	f->char_w = rd32();
	f->char_h = rd32();
	f->spacing = rd32();
	f->widths = (unsigned char *) malloc(num);
	memcpy(f->widths, p, num);
	p += num;
	full = rd32();
	n = rd32();
	f->data = (unsigned char *) malloc(full);
	unrle(f->data, n);
}

static void rd_track(TrackInfo *t) {
	int i, n, r, g, b, set;
	Seg *sg;
	RectF *rc;
	float *fr;
	float fv;
	unsigned char *mm;
	n = rd32();
	t->name = (char *) malloc(n + 1);
	memcpy(t->name, p, n);
	t->name[n] = 0;
	p += n;
	t->w = rd32();
	t->h = rd32();
	t->shift = t->w == 256 ? 8 : 7;
	r = rd32();
	g = rd32();
	b = rd32();
	t->bg = RGB(r, g, b);
	set = rd32();
	t->sheet = set ? &a_track : &a_tileset;
	for (i = 0; i < 4; i++) t->fin[i] = rd32();

	t->route_len = rd32();
	t->route = (int *) malloc(t->route_len * 8);
	for (i = 0; i < t->route_len * 2; i++) t->route[i] = rd32();
	t->segs = (Seg *) malloc(t->route_len * sizeof(Seg));
	for (i = 0; i < t->route_len - 1; i++) {
		sg = t->segs + i;
		sg->ax = (float) t->route[i * 2];
		sg->ay = (float) t->route[i * 2 + 1];
		sg->bx = (float) t->route[i * 2 + 2];
		sg->by = (float) t->route[i * 2 + 3];
		sg->rx = sg->bx - sg->ax;
		sg->ry = sg->by - sg->ay;
		sg->dx = sg->ry;
		sg->dy = -sg->rx;
		sg->rxs = (sg->rx * sg->dy) - (sg->ry * sg->dx);
		sg->nx = sbk_sqrtf(sg->rx * sg->rx + sg->ry * sg->ry);
		sg->ny = sg->ry / sg->nx;
		sg->nx = sg->rx / sg->nx;
		sg->minx = t->route[i * 2] < t->route[i * 2 + 2] ? t->route[i * 2] : t->route[i * 2 + 2];
		sg->maxx = t->route[i * 2] > t->route[i * 2 + 2] ? t->route[i * 2] : t->route[i * 2 + 2];
		sg->miny = t->route[i * 2 + 1] < t->route[i * 2 + 3] ? t->route[i * 2 + 1] : t->route[i * 2 + 3];
		sg->maxy = t->route[i * 2 + 1] > t->route[i * 2 + 3] ? t->route[i * 2 + 1] : t->route[i * 2 + 3];
	}

	t->num_rects = rd32();
	t->rects = (RectF *) malloc(t->num_rects * sizeof(RectF));
	for (i = 0; i < t->num_rects; i++) {
		rc = t->rects + i;
		rc->x = rd32();
		rc->y = rd32();
		rc->w = rd32();
		rc->h = rd32();
		rc->fx = (float) rc->x;
		rc->fy = (float) rc->y;
		rc->fx2 = (float) (rc->x + rc->w);
		rc->fy2 = (float) (rc->y + rc->h);
	}

	t->num_sprites = rd32();
	t->sprites = (ObjInfo *) malloc((t->num_sprites + 1) * sizeof(ObjInfo));
	for (i = 0; i < t->num_sprites; i++) {
		t->sprites[i].type = rd32();
		t->sprites[i].scale = rd32();
		t->sprites[i].x = rd32();
		t->sprites[i].y = rd32();
		t->sprites[i].sx = rd32();
		t->sprites[i].sy = rd32();
		t->sprites[i].sw = rd32();
		t->sprites[i].sh = rd32();
		t->sprites[i].ox = rd32();
		t->sprites[i].oy = rd32();
	}

	// the friction of all 256 tiles: past the list's end, 1.0
	fr = (float *) malloc(256 * sizeof(float));
	mm = (unsigned char *) malloc(256);
	for (i = 0; i < 256; i++) fr[i] = 1.0f;
	n = rd32();
	for (i = 0; i < n; i++) {
		*(int *) &fv = rd32();
		fr[i] = fv;
	}
	for (i = 0; i < 256; i++) {
		if (fr[i] == 0.0f) mm[i] = 1;
		else if (fr[i] > 1.0f) mm[i] = 2;
		else mm[i] = 3;
	}
	t->friction = fr;
	t->mm_class = mm;

	t->tile_base = (int *) malloc(256 * 4);
	for (i = 0; i < 256; i++) t->tile_base[i] = (i & 15) * 8 + ((i >> 4) * 8) * 128;

	t->tiles = (unsigned char *) malloc(t->w * t->h);
	n = rd32();
	unrle(t->tiles, n);
	n = rd32();
	if (n < 0) t->trans = 0;
	else {
		t->trans = (unsigned char *) malloc(t->w * t->h);
		unrle(t->trans, n);
	}
}

void assets_load(void) {
	int i;
	p = (unsigned char *) sbk_blob;
	rd_image(&a_tileset);
	rd_image(&a_track);
	rd_image(&a_kart);
	rd_image(&a_logo);
	rd_image(&a_menu_sprites);
	rd_image(&a_kart_icons);
	// the first kart in kart-info.hpp: the same sprites, another palette
	a_kart_alt.data = a_kart.data;
	a_kart_alt.w = a_kart.w;
	a_kart_alt.h = a_kart.h;
	a_kart_alt.pal = rd_palette(256);
	a_kart_alt.ncol = 256;
	rd_font(&tall_font);
	rd_font(&big_number_font);
	rd_font(&menu_font);
	for (i = 0; i < MAX_TRACKS; i++) rd_track(track_info + i);
}
