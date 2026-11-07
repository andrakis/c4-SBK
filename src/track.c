// track.c -- upstream's track.cpp, with what it uses of 32blit's TileMap (MIT:
// LICENSE.32blit): TileMap::draw and texture_span, called with the mode 7
// scanline transform. Drawn as upstream draws it, but fog is a table: each
// scanline's alpha over the plain background has one colour for each of the
// tile sheet's, worked out the first time that alpha is used (fog_row).
// Sprites below the track (a kart that has fallen off) show through upstream's
// transparent pixels, so the scanlines with any of those take span_any, which
// reads what is on the screen as upstream does.

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "sbk.h"

extern float sbk_deg, sbk_inf;

enum { TEXEL_EMPTY = 255 };
// under this step in texels * 65536, a run is worth working out
enum { RUN_STEP = 20000 };

// a blend over a known background (RGBA_RGB's pen path)
static int *fog_row(Track *t, int alpha) {
	int *row, *pal;
	int i, a, p, bg, r, g, b, br, bgg, bb;
	if (t->fog_rows[alpha]) return t->fog_rows[alpha];
	row = (int *) malloc(256 * 4);
	pal = t->tiles->pal;
	bg = t->info->bg & 0xFFFFFF;
	br = (bg >> 16) & 255; bgg = (bg >> 8) & 255; bb = bg & 255;
	for (i = 0; i < t->tiles->ncol; i++) {
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
	row[TEXEL_EMPTY] = bg;
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
// alpha in row. The game's hot loops, which is why the whole map is one texture of
// bytes (track_world): what texture_span works out for each tile is then one load,
// and EMPTY says there is nothing to draw. Upstream checks whether each pixel has
// left the last one's texel; span_runs works out how many pixels until it does,
// and is for when that is more than one or two. A pixel of either is the texel
// under it, which is what upstream's comes to.

// TileMap::offset's clamp at the map's edge, then the texel in that tile
static int world_clamp(TrackInfo *info, int x, int y) {
	int w, cx, cy;
	w = info->w;
	cx = x >> 3;
	cy = y >> 3;
	if (cx != (cx & (w - 1))) cx = cx < 0 ? 0 : w - 1;
	if (cy != (cy & (w - 1))) cy = cy < 0 ? 0 : w - 1;
	return info->world[(cx << 3) + (x & 7) + (((cy << 3) + (y & 7)) << info->wshift)];
}

// some of it off the map, where the map's edge goes on for ever
static void span_edge(TrackInfo *info, int *row, int *d, int c, int wx, int wy, int dx, int dy) {
	unsigned char *world;
	int wshift, notmask, x, y;
	world = info->world;
	wshift = info->wshift;
	notmask = ~((info->w << 3) - 1);
	do {
		x = wx >> 16;
		y = wy >> 16;
		if ((x | y) & notmask) {
			if (!info->edge_empty) *d = row[world_clamp(info, x, y)];
		} else
			*d = row[world[x + (y << wshift)]];
		d++;
		wx += dx;
		wy += dy;
	} while (--c);
}

// all of it on the map: no pixel needs checking, and d and c needn't change at each
static void span_pixels(unsigned char *world, int wshift, int *row, int *d, int c, int wx, int wy, int dx, int dy) {
	while (c >= 4) {
		d[0] = row[world[(wx >> 16) + ((wy >> 16) << wshift)]];
		wx += dx; wy += dy;
		d[1] = row[world[(wx >> 16) + ((wy >> 16) << wshift)]];
		wx += dx; wy += dy;
		d[2] = row[world[(wx >> 16) + ((wy >> 16) << wshift)]];
		wx += dx; wy += dy;
		d[3] = row[world[(wx >> 16) + ((wy >> 16) << wshift)]];
		wx += dx; wy += dy;
		d += 4;
		c -= 4;
	}
	while (c) {
		*d++ = row[world[(wx >> 16) + ((wy >> 16) << wshift)]];
		wx += dx; wy += dy;
		c--;
	}
}

// dx and dy small enough, and wx and wy far enough from wrapping round, for the
// arithmetic here to be plain
static void span_runs(TrackInfo *info, int *row, int *d, int c, int wx, int wy, int dx, int dy) {
	unsigned char *world;
	int wshift, notmask, x, y, v, kx, ky, n, i, col;
	world = info->world;
	wshift = info->wshift;
	notmask = ~((info->w << 3) - 1);
	x = wx >> 16;
	y = wy >> 16;
	if (dx > 0) kx = (((x + 1) << 16) - wx + dx - 1) / dx;
	else if (dx < 0) kx = (wx - (x << 16)) / (-dx) + 1;
	else kx = 0x7FFFFFFF;
	if (dy > 0) ky = (((y + 1) << 16) - wy + dy - 1) / dy;
	else if (dy < 0) ky = (wy - (y << 16)) / (-dy) + 1;
	else ky = 0x7FFFFFFF;
	i = 0;
	while (1) {
		if ((x | y) & notmask) v = world_clamp(info, x, y);
		else v = world[x + (y << wshift)];
		n = kx < ky ? kx : ky;
		if (n > c) n = c;
		c -= n;
		i += n;
		kx -= n;
		ky -= n;
		col = row[v];
		if (v != TEXEL_EMPTY) {
			while (n >= 4) {
				d[0] = col;
				d[1] = col;
				d[2] = col;
				d[3] = col;
				d += 4;
				n -= 4;
			}
			while (n) {
				*d++ = col;
				n--;
			}
		} else
			d += n;
		if (!c) return;
		if (!kx) {
			n = wx + i * dx;
			x = n >> 16;
			if (dx > 0) kx = (((x + 1) << 16) - n + dx - 1) / dx;
			else kx = (n - (x << 16)) / (-dx) + 1;
		}
		if (!ky) {
			n = wy + i * dy;
			y = n >> 16;
			if (dy > 0) ky = (((y + 1) << 16) - n + dy - 1) / dy;
			else ky = (n - (y << 16)) / (-dy) + 1;
		}
	}
}

// the map as one texture of the sheet's colour indices, with each tile's flips
// applied: once for each track, kept. Rows of 8 go as two words: the sheet and the
// texture are both whole multiples of 8 from where malloc put them.
static void track_world(TrackInfo *info) {
	int W, wshift, tx, ty, tile, tr, u, v, su, sv, tmp, edge, w, off;
	unsigned char *world, *line, *sheet, *src;
	int *base;
	if (info->world) return;
	w = info->w;
	W = w << 3;
	wshift = info->shift + 3;
	world = (unsigned char *) malloc(W * W);
	if (!world) return;
	memset(world, TEXEL_EMPTY, W * W);
	sheet = info->sheet->data;
	base = info->tile_base;
	edge = 1;
	off = 0;
	for (ty = 0; ty < info->h; ty++)
		for (tx = 0; tx < w; tx++) {
			tile = info->tiles[off];
			tr = info->trans ? info->trans[off] : 0;
			off++;
			if (!tile) continue;
			if (tx == 0 || ty == 0 || tx == w - 1 || ty == info->h - 1) edge = 0;
			line = world + (tx << 3) + ((ty << 3) << wshift);
			if (!tr) {
				src = sheet + base[tile];
				for (v = 0; v < 8; v++) {
					((int *) line)[0] = ((int *) src)[0];
					((int *) line)[1] = ((int *) src)[1];
					line += 1 << wshift;
					src += 128;
				}
			} else {
				for (v = 0; v < 8; v++) {
					for (u = 0; u < 8; u++) {
						sv = (tr & 2) ? 7 - v : v;
						su = (tr & 4) ? 7 - u : u;
						if (tr & 1) { tmp = su; su = sv; sv = tmp; }
						line[u] = sheet[base[tile] + su + (sv << 7)];
					}
					line += 1 << wshift;
				}
			}
		}
	info->wshift = wshift;
	info->edge_empty = edge;
	info->world = world;
}

// Track::render: TileMap::draw through mode7_scanline_transform
void track_render(Track *t, Camera *cam, int any_y0, int any_y1) {
	int horizon, vy, vw, vh, y, y1, a, doff, c, wx, wy, dx, dy;
	float top, left, pitch_c, pitch_s, focal, fog, cy, yb, zb, l, syc, sys, tx, ty, ex, ey, fc, fvw, posy, rx, rz, v;
	int *row;
	TrackInfo *info;
	unsigned char *world;
	int wshift, notmask, c1, lo, hi, n, edge, lim;

	info = t->info;
	// no texture, no room for one: every line the slow way
	if (!info->world) { any_y0 = 0; any_y1 = SCREEN_H; }
	edge = info->edge_empty;
	lim = info->w << 19;
	world = info->world;
	wshift = info->wshift;
	notmask = ~((info->w << 3) - 1);
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
		c1 = c - 1;
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

			if (y >= any_y0 && y <= any_y1) {
				scr_alpha = a;
				span_any(t, doff, c, wx, wy, dx, dy);
			} else {
				row = fog_row(t, a);
				if (dx < 0x400000 && dx > -0x400000 && dy < 0x400000 && dy > -0x400000 &&
				    wx < 0x40000000 && wx > -0x40000000 && wy < 0x40000000 && wy > -0x40000000) {
					// the pixels on the map; off it, where its edge has nothing, the
					// background stays. Upstream looks at each to find out.
					lo = 0;
					hi = c1;
					if (edge) {
						if (dx > 0) {
							if (wx < 0) { n = (dx - 1 - wx) / dx; if (n > lo) lo = n; }
							n = wx < lim ? (lim - 1 - wx) / dx : -1;
							if (n < hi) hi = n;
						} else if (dx < 0) {
							if (wx >= lim) { n = (wx - lim - dx) / -dx; if (n > lo) lo = n; }
							n = wx >= 0 ? wx / -dx : -1;
							if (n < hi) hi = n;
						} else if (wx < 0 || wx >= lim) hi = -1;
						if (dy > 0) {
							if (wy < 0) { n = (dy - 1 - wy) / dy; if (n > lo) lo = n; }
							n = wy < lim ? (lim - 1 - wy) / dy : -1;
							if (n < hi) hi = n;
						} else if (dy < 0) {
							if (wy >= lim) { n = (wy - lim - dy) / -dy; if (n > lo) lo = n; }
							n = wy >= 0 ? wy / -dy : -1;
							if (n < hi) hi = n;
						} else if (wy < 0 || wy >= lim) hi = -1;
					}
					if (lo <= hi) {
						if (!edge && !(((wx | wy | (wx + c1 * dx) | (wy + c1 * dy)) >> 16) & notmask)) edge = 2;
						if (dx < RUN_STEP && dx > -RUN_STEP && dy < RUN_STEP && dy > -RUN_STEP)
							span_runs(info, row, fb + doff + lo, hi - lo + 1, wx + lo * dx, wy + lo * dy, dx, dy);
						else if (edge)
							span_pixels(world, wshift, row, fb + doff + lo, hi - lo + 1, wx + lo * dx, wy + lo * dy, dx, dy);
						else
							span_edge(info, row, fb + doff, c, wx, wy, dx, dy);
						if (edge == 2) edge = 0;
					}
				} else
					span_edge(info, row, fb + doff, c, wx, wy, dx, dy);
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
	track_world(info);
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

// what a kart's check against this object can skip in whole numbers: the object's
// square of reach, and where it is, unless it is too far out to trust
void obj_bounds(TrackObject *ob) {
	float sr;
	sr = (float) ob->sprite.size_w * 4.0f * ob->sprite.scale;
	ob->sr = sr;
	ob->lim = (kart_radius_f + sr) * (kart_radius_f + sr);
	ob->irad = (int) (kart_radius_f + sr) + 2;
	obj_moved(ob);
}

void obj_moved(TrackObject *ob) {
	int x, y;
	x = (int) ob->sprite.world_pos.x;
	y = (int) ob->sprite.world_pos.z;
	if (x > 30000 || x < -30000 || y > 30000 || y < -30000) {
		x = 0;
		y = 0;
		ob->irad = 0x3FFFFFFF;
	}
	ob->ix = x;
	ob->iy = y;
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
		obj_bounds(ob);
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

// Track::find_closest_route_segment. The same answer, for a fraction of the work.
// Last time's segment gives a square no smaller than the best, in round numbers;
// a segment whose bounding box is further away than that can't be the best, or tie
// with it, and takes no floats at all. And a square no smaller than the best so
// far can't have a smaller root.
int track_closest_segment(Track *t, float px, float py, float *segment_t, int hint) {
	int i, n, ret, ub, ix, iy, dx, dy, use;
	float tt, qx, qy, d2, dist, min_dist, min_d2;
	Seg *s;
	ret = 0;
	min_dist = sbk_inf;
	min_d2 = min_dist;
	n = t->info->route_len - 1;

	s = t->info->segs + hint;
	tt = ((px - s->ax) * s->dy - (py - s->ay) * s->dx) / s->rxs;
	if (tt < 0.0f) { qx = s->ax; qy = s->ay; }
	else if (tt > 1.0f) { qx = s->bx; qy = s->by; }
	else { qx = s->ax + s->rx * tt; qy = s->ay + s->ry * tt; }
	qx = px - qx;
	qy = py - qy;
	d2 = qx * qx + qy * qy;
	use = d2 < 16000000.0f;
	if (use) {
		ub = (int) d2;
		ub = ub + (ub >> 8) + 16;
		ix = (int) px;
		iy = (int) py;
	}

	s = t->info->segs;
	for (i = 0; i < n; i++, s++) {
		if (use) {
			dx = s->minx - ix;
			if (dx < 0) dx = ix - s->maxx;
			dy = s->miny - iy;
			if (dy < 0) dy = iy - s->maxy;
			dx -= 2;
			dy -= 2;
			if (dx < 0) dx = 0;
			if (dy < 0) dy = 0;
			if (dx * dx + dy * dy > ub) continue;
		}
		tt = ((px - s->ax) * s->dy - (py - s->ay) * s->dx) / s->rxs;
		if (tt < 0.0f) { qx = s->ax; qy = s->ay; }
		else if (tt > 1.0f) { qx = s->bx; qy = s->by; }
		else { qx = s->ax + s->rx * tt; qy = s->ay + s->ry * tt; }
		qx = px - qx;
		qy = py - qy;
		d2 = qx * qx + qy * qy;
		if (d2 < min_d2) {
			dist = sbk_sqrtf(d2);
			if (dist < min_dist) {
				ret = i;
				*segment_t = tt;
				min_dist = dist;
				min_d2 = d2;
			}
		}
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
	a = degrees * sbk_deg;
	x = v->x; y = v->y; z = v->z;
	if (a == 0.0f) {
		v->x = x + 0.0f;
		v->y = y + 0.0f;
		v->z = z + 0.0f;
		return;
	}
	c = sbk_cosf(a);
	s = sbk_sinf(a);
	t = 1.0f - c;
	v->x = c * x + s * z + 0.0f;
	v->y = (t + c) * y + 0.0f;
	v->z = (0.0f - s) * x + c * z + 0.0f;
}
