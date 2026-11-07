// track.c -- upstream's track.cpp, with what it uses of 32blit's TileMap (MIT:
// LICENSE.32blit): TileMap::draw and texture_span, called with the mode 7
// scanline transform. Drawn as upstream draws it, but fog is a table: each
// scanline's alpha over the plain background has one colour for each of the
// tile sheet's, worked out the first time that alpha is used (fog_row).
// Sprites below the track (a kart that has fallen off) show through upstream's
// transparent pixels, so a frame with any of those takes span_any, which reads
// what is on the screen as upstream does.

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "sbk.h"

extern float sbk_pi, sbk_inf;

// a blend over a known background (RGBA_RGB's pen path)
static int *fog_row(Track *t, int alpha) {
	int *row, *pal;
	int i, a, p, bg, r, g, b, br, bgg, bb;
	if (t->fog_rows[alpha]) return t->fog_rows[alpha];
	row = (int *) malloc(256 * 4);
	pal = t->tiles->pal;
	bg = t->info->bg & 0xFFFFFF;
	br = (bg >> 16) & 255; bgg = (bg >> 8) & 255; bb = bg & 255;
	for (i = 0; i < 256; i++) {
		p = pal[i];
		a = (p >> 24) & 255;
		if (!a) { row[i] = bg; continue; }
		a = pen_alpha(a, alpha);
		if (a >= 255) { row[i] = p & 0xFFFFFF; continue; }
		r = br + ((a * (((p >> 16) & 255) - br) + 127) >> 8);
		g = bgg + ((a * (((p >> 8) & 255) - bgg) + 127) >> 8);
		b = bb + ((a * ((p & 255) - bb) + 127) >> 8);
		row[i] = (r << 16) | (g << 8) | b;
	}
	t->fog_rows[alpha] = row;
	return row;
}

// TileMap::offset, for CLAMP_TO_EDGE and a map that is square
static int map_clamp(int x, int y, int w) {
	int cx, cy;
	cx = x & (w - 1);
	cy = y & (w - 1);
	if (x != cx) cx = x < 0 ? 0 : w - 1;
	if (y != cy) cy = y < 0 ? 0 : w - 1;
	return cx + cy * w;
}

// TileMap::texture_span, as it is, for when the screen under the track is not
// only the background
static void span_any(Track *t, int doff, int c, int wx, int wy, int dx, int dy) {
	TrackInfo *info;
	int w, wcx, wcy, toff, tile, tr, u, v, tmp, count, pen;
	int *pal;
	unsigned char *sheet;
	info = t->info;
	w = info->w;
	pal = t->tiles->pal;
	sheet = t->tiles->data;
	do {
		wcx = wx >> 16;
		wcy = wy >> 16;
		toff = map_clamp(wcx >> 3, wcy >> 3, w);
		tile = info->tiles[toff];
		if (tile != 0) {
			tr = info->trans ? info->trans[toff] : 0;
			u = wcx & 7;
			v = wcy & 7;
			if (tr) {
				v = (tr & 2) ? 7 - v : v;
				u = (tr & 4) ? 7 - u : u;
				if (tr & 1) { tmp = u; u = v; v = tmp; }
			}
			u += (tile & 15) * 8;
			v += (tile >> 4) * 8;
			count = 0;
			do {
				wx += dx;
				wy += dy;
				c--;
				count++;
			} while (c && (wx >> 16) == wcx && (wy >> 16) == wcy);
			pen = pal[sheet[u + v * 128]];
			pen_span(pen, doff, count);
			doff += count;
			continue;
		}
		do {
			wx += dx;
			wy += dy;
			doff++;
			c--;
		} while (c && (wx >> 19) == wcx >> 3 && (wy >> 19) == wcy >> 3);
	} while (c);
}

// The same, over the plain background, with the colours of this scanline's
// alpha in row. This is the game's hot loop.
static void span_plain(Track *t, int *row, int doff, int c, int wx, int wy, int dx, int dy) {
	TrackInfo *info;
	int w, shift, notmask, wcx, wcy, tx, ty, toff, tile, tr, u, v, tmp, col;
	unsigned char *tiles, *trans, *sheet;
	int *base;
	int *d;
	info = t->info;
	w = info->w;
	shift = info->shift;
	notmask = ~(w - 1);
	tiles = info->tiles;
	trans = info->trans;
	sheet = t->tiles->data;
	base = info->tile_base;
	d = fb + doff;
	do {
		wcx = wx >> 16;
		wcy = wy >> 16;
		tx = wcx >> 3;
		ty = wcy >> 3;
		if ((tx | ty) & notmask) toff = map_clamp(tx, ty, w);
		else toff = tx + (ty << shift);
		tile = tiles[toff];
		if (tile) {
			u = wcx & 7;
			v = wcy & 7;
			if (trans) {
				tr = trans[toff];
				if (tr) {
					v = (tr & 2) ? 7 - v : v;
					u = (tr & 4) ? 7 - u : u;
					if (tr & 1) { tmp = u; u = v; v = tmp; }
				}
			}
			col = row[sheet[base[tile] + u + (v << 7)]];
			do {
				wx += dx;
				wy += dy;
				*d++ = col;
				c--;
			} while (c && (wx >> 16) == wcx && (wy >> 16) == wcy);
			continue;
		}
		do {
			wx += dx;
			wy += dy;
			d++;
			c--;
		} while (c && (wx >> 19) == tx && (wy >> 19) == ty);
	} while (c);
}

// Track::render: TileMap::draw through mode7_scanline_transform
void track_render(Track *t, Camera *cam, int plain) {
	int horizon, vy, vw, vh, y, y1, a, doff, c, wx, wy, dx, dy;
	float top, left, pitch_c, pitch_s, focal, fog, cy, yb, zb, l, syc, sys, tx, ty, ex, ey, fc, fvw, posy, rx, rz, v;
	int *row;

	horizon = (int) ((float) (SCREEN_H / 2) -
		(((cam->far_z * -cam->forward.y) - cam->pos.y) * cam->focal) / (cam->far_z * cam->up.y));

	// the viewport, clipped: a camera's is always at x 0
	vy = cam->vy + horizon > clip_y ? cam->vy + horizon : clip_y;
	vw = (clip_x + clip_w < cam->vw ? clip_x + clip_w : cam->vw);
	vh = (clip_y + clip_h < cam->vy + cam->vh ? clip_y + clip_h : cam->vy + cam->vh) - vy;

	top = (float) (cam->vh / 2);
	left = (float) (-cam->vw / 2);
	pitch_c = cam->up.y;
	pitch_s = -cam->forward.y;
	focal = cam->focal;
	fog = t->fog;
	posy = cam->pos.y;
	rx = cam->right.x;
	rz = cam->right.z;

	c = vw;
	if (c > 0) {
		fc = (float) c;
		fvw = fc;
		y1 = vy + vh;
		doff = vy * SCREEN_W;
		for (y = vy; y < y1; y++) {
			cy = (float) y - top;
			yb = cy * pitch_c + focal * pitch_s;
			zb = cy * pitch_s - focal * pitch_c;
			l = posy / yb;
			syc = l * rx;
			sys = l * rz;
			tx = cam->pos.x + syc * left - sys * zb;
			ty = cam->pos.z + sys * left + syc * zb;

			// screen.alpha = 255 - max(0, min(255, (l - 0.2) * fog))
			v = (l - 0.2f) * fog;
			v = v < 255.0f ? v : 255.0f;
			v = 0.0f < v ? v : 0.0f;
			a = ((int) (255.0f - v)) & 255;

			// swc and ewc: (0, y) and (vw, y) through the matrix
			ex = syc * fvw + tx;
			ey = sys * fvw + ty;

			wx = (int) (tx * 65536.0f);
			wy = (int) (ty * 65536.0f);
			dx = (int) (((ex - tx) / fc) * 65536.0f);
			dy = (int) (((ey - ty) / fc) * 65536.0f);

			if (plain) {
				row = fog_row(t, a);
				span_plain(t, row, doff, c, wx, wy, dx, dy);
			} else {
				scr_alpha = a;
				span_any(t, doff, c, wx, wy, dx, dy);
			}
			doff += SCREEN_W;
		}
	}
	scr_alpha = 255;
}

Track *track_new(TrackInfo *info) {
	Track *t;
	t = (Track *) malloc(sizeof(Track));
	t->info = info;
	t->tiles = info->sheet;
	t->fog = 170.0f;
	t->fog_rows = (int **) malloc(256 * sizeof(int *));
	memset(t->fog_rows, 0, 256 * sizeof(int *));
	t->cap_objects = info->num_sprites + 16;
	t->objects = (TrackObject *) malloc(t->cap_objects * sizeof(TrackObject));
	t->num_objects = 0;
	track_reset_objects(t);
	return t;
}

void track_free(Track *t) {
	int i;
	for (i = 0; i < 256; i++) if (t->fog_rows[i]) free((char *) t->fog_rows[i]);
	free((char *) t->fog_rows);
	free((char *) t->objects);
	free((char *) t);
}

void track_reset_objects(Track *t) {
	int i;
	ObjInfo *o;
	TrackObject *ob;
	t->num_objects = t->info->num_sprites;
	for (i = 0; i < t->info->num_sprites; i++) {
		o = t->info->sprites + i;
		ob = t->objects + i;
		sprite_init(&ob->sprite);
		ob->type = o->type;
		ob->respawn_timer = 0;
		ob->vel.x = 0.0f; ob->vel.y = 0.0f; ob->vel.z = 0.0f;
		ob->sprite.sheet = t->tiles;
		ob->sprite.world_pos.x = (float) o->x;
		ob->sprite.world_pos.z = (float) o->y;
		ob->sprite.scale = (float) o->scale / 16.0f;
		ob->sprite.sheet_x = o->sx;
		ob->sprite.sheet_y = o->sy;
		ob->sprite.size_w = o->sw;
		ob->sprite.size_h = o->sh;
		ob->sprite.origin_x = o->ox;
		ob->sprite.origin_y = o->oy;
	}
}

// Track::add_object: a slot for the caller to fill, a removed one if there is
// one; std::vector's growth otherwise
TrackObject *track_add_object(Track *t) {
	int i;
	for (i = t->info->num_sprites; i < t->num_objects; i++)
		if (t->objects[i].type == OBJ_REMOVED) return t->objects + i;
	if (t->num_objects == t->cap_objects) {
		t->cap_objects = t->cap_objects * 2;
		t->objects = (TrackObject *) realloc((char *) t->objects, t->cap_objects * sizeof(TrackObject));
	}
	i = t->num_objects;
	t->num_objects = i + 1;
	return t->objects + i;
}

// Track::find_closest_route_segment. The square root is only taken where it
// can matter: a square no smaller than the best's has a root no smaller.
int track_closest_segment(Track *t, float px, float py, float *segment_t) {
	int i, n, ret;
	float tt, qx, qy, d2, dist, min_dist, min_d2;
	Seg *s;
	ret = 0;
	min_dist = sbk_inf;
	min_d2 = min_dist;
	n = t->info->route_len - 1;
	s = t->info->segs;
	for (i = 0; i < n; i++) {
		tt = ((px - s->ax) * s->dy - (py - s->ay) * s->dx) / s->rxs;
		if (tt < 0.0f) { qx = s->ax; qy = s->ay; }
		else if (tt > 1.0f) { qx = s->bx; qy = s->by; }
		else { qx = s->ax + s->rx * tt; qy = s->ay + s->ry * tt; }
		qx = px - qx;
		qy = py - qy;
		d2 = qx * qx + qy * qy;
		if (d2 < min_d2) {
			dist = sqrtf(d2);
			if (dist < min_dist) {
				ret = i;
				*segment_t = tt;
				min_dist = dist;
				min_d2 = d2;
			}
		}
		s++;
	}
	return ret;
}

// Track::get_friction
float track_friction(Track *t, float px, float py) {
	int x, y;
	TrackInfo *info;
	info = t->info;
	x = (int) (px / 8.0f);
	y = (int) (py / 8.0f);
	if (x < 0 || y < 0 || x >= info->w || y >= info->h) return 0.0f;
	return info->friction[info->tiles[x + y * info->w]];
}

// Vec3::transform(Mat4::rotation(degrees, up)). An exact zero comes out +0, as it
// does through the terms the whole matrix adds and which are left out here;
// with no angle there is nothing else to do.
void rot_y(Vec3 *v, float degrees) {
	float a, c, s, t, x, y, z;
	a = degrees * (sbk_pi / 180.0f);
	x = v->x; y = v->y; z = v->z;
	if (a == 0.0f) {
		v->x = x + 0.0f;
		v->y = y + 0.0f;
		v->z = z + 0.0f;
		return;
	}
	c = cosf(a);
	s = sinf(a);
	t = 1.0f - c;
	v->x = c * x + s * z + 0.0f;
	v->y = (t + c) * y + 0.0f;
	v->z = (0.0f - s) * x + c * z + 0.0f;
}
