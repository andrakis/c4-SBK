// blit.c -- what the game draws with from Pimoroni's 32blit SDK (MIT:
// LICENSE.32blit): graphics/blend.cpp, surface.cpp, sprite.cpp, primitive.cpp,
// text.cpp and color.cpp, for a screen of RGB and sprites of 8 bits through a
// palette, which is all the game has. Blending rounds as upstream's does, so
// the frames are the same to the bit.

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "sbk.h"

int *fb;
int clip_x, clip_y, clip_w, clip_h;
int scr_alpha, scr_pen;
int drawn_y0, drawn_y1;

void scr_init(void) {
	fb = (int *) malloc(SCREEN_W * SCREEN_H * 4);
	memset(fb, 0, SCREEN_W * SCREEN_H * 4);
	clip_x = 0; clip_y = 0; clip_w = SCREEN_W; clip_h = SCREEN_H;
	scr_alpha = 255;
	scr_pen = 0;
}

void scr_clip(int x, int y, int w, int h) { clip_x = x; clip_y = y; clip_w = w; clip_h = h; }

int pen_alpha(int a1, int a2) { return ((a1 + 1) * (a2 + 1)) >> 8; }

// RGBA_RGB(const Pen *, ...): c of the pen
void pen_span(int pen, int off, int c) {
	int a, v, n, sr, sg, sb, dr, dg, db, was, out;
	int *d;
	a = (pen >> 24) & 255;
	if (!a) return;
	a = ((a + 1) * (scr_alpha + 1)) >> 8;
	d = fb + off;
	if (a >= 255) {
		v = pen & 0xFFFFFF;
		// memcpy is one of the VM's opcodes, as cheap for a screen as for a word:
		// a lot of pixels are some done by hand, then what is done, doubled
		if (c >= 64) {
			n = 16;
			for (a = 0; a < n; a++) d[a] = v;
			c -= n;
			while (c > 0) {
				a = n < c ? n : c;
				memcpy((char *) (d + n), (char *) d, a * 4);
				n += a;
				c -= a;
			}
			return;
		}
		while (c-- > 0) *d++ = v;
		return;
	}
	sr = (pen >> 16) & 255;
	sg = (pen >> 8) & 255;
	sb = pen & 255;
	// what is under it is often the colour just done
	was = -1;
	out = 0;
	while (c-- > 0) {
		v = *d;
		if (v != was) {
			was = v;
			dr = (v >> 16) & 255;
			dg = (v >> 8) & 255;
			db = v & 255;
			out = ((dr + ((a * (sr - dr) + 127) >> 8)) << 16) |
			      ((dg + ((a * (sg - dg) + 127) >> 8)) << 8) |
			      (db + ((a * (sb - db) + 127) >> 8));
		}
		*d++ = out;
	}
}

// RGBA_RGB(const Surface *, ...): a row of the source. The same colours over the
// same colours come out the same, and in a row they often are.
static void row_blit(Surf *src, int soff, int doff, int cnt) {
	unsigned char *s;
	int *pal, *d;
	int p, a, v, dr, dg, db, alpha, was_p, was_v, out;
	s = src->data + soff;
	pal = src->pal;
	d = fb + doff;
	alpha = scr_alpha + 1;
	was_p = 0;
	was_v = -1;
	out = 0;
	do {
		p = pal[*s++];
		a = (((p >> 24) & 255) + 1) * alpha >> 8;
		if (a >= 255) {
			*d = p & 0xFFFFFF;
		} else if (a > 1) {
			v = *d;
			if (v != was_v || p != was_p) {
				was_v = v;
				was_p = p;
				dr = (v >> 16) & 255;
				dg = (v >> 8) & 255;
				db = v & 255;
				out = ((dr + ((a * (((p >> 16) & 255) - dr) + 127) >> 8)) << 16) |
				      ((dg + ((a * (((p >> 8) & 255) - dg) + 127) >> 8)) << 8) |
				      (db + ((a * ((p & 255) - db) + 127) >> 8));
			}
			*d = out;
		}
		d++;
	} while (--cnt);
}

// the minimap: a blit of row_blit's, of a few pens at the same alpha (and with
// the screen's alpha at 255, which it is then), where index 0 draws nothing
void scr_blit_tinted(unsigned char *src, int w, int h, int x0, int y0, int *rgb, int a) {
	int *d;
	int x, y, c, v, was, wc, out, p, dr, dg, db;
	unsigned char *s;
	s = src;
	for (y = 0; y < h; y++) {
		d = fb + x0 + (y0 + y) * SCREEN_W;
		was = -1;
		wc = 0;
		out = 0;
		x = w;
		do {
			c = *s++;
			if (c) {
				v = *d;
				if (v != was || c != wc) {
					was = v;
					wc = c;
					p = rgb[c];
					dr = (v >> 16) & 255;
					dg = (v >> 8) & 255;
					db = v & 255;
					out = ((dr + ((a * (((p >> 16) & 255) - dr) + 127) >> 8)) << 16) |
					      ((dg + ((a * (((p >> 8) & 255) - dg) + 127) >> 8)) << 8) |
					      (db + ((a * ((p & 255) - db) + 127) >> 8));
				}
				*d = out;
			}
			d++;
		} while (--x);
	}
}

// Surface::clear, rectangle
void scr_clear(void) { scr_rect(clip_x, clip_y, clip_w, clip_h); }

void scr_rect(int x, int y, int w, int h) {
	int cx, cy, cw, ch, o;
	cx = clip_x > x ? clip_x : x;
	cy = clip_y > y ? clip_y : y;
	cw = (clip_x + clip_w < x + w ? clip_x + clip_w : x + w) - cx;
	ch = (clip_y + clip_h < y + h ? clip_y + clip_h : y + h) - cy;
	if (cw <= 0 || ch <= 0) return;
	o = cx + cy * SCREEN_W;
	if (cx == 0 && cw == SCREEN_W) {
		pen_span(scr_pen, o, cw * ch);
		return;
	}
	for (y = cy; y < cy + ch; y++) {
		pen_span(scr_pen, o, cw);
		o += SCREEN_W;
	}
}

void scr_vspan(int x, int y, int c) {
	if (x < 0 || x >= SCREEN_W) return;
	if (y < 0) { c -= -y; y = 0; }
	if (y + c > SCREEN_H) c -= y + c - SCREEN_H;
	while (c > 0) {
		pen_span(scr_pen, x + y * SCREEN_W, 1);
		y++;
		c--;
	}
}

void scr_hspan(int x, int y, int c) {
	if (y < 0 || y >= SCREEN_H) return;
	if (x < 0) { c -= -x; x = 0; }
	if (x + c > SCREEN_W) c -= x + c - SCREEN_W;
	if (c > 0) pen_span(scr_pen, x + y * SCREEN_W, c);
}

static int orient2d(int x1, int y1, int x2, int y2, int x3, int y3) {
	return (x2 - x1) * (y3 - y1) - (y2 - y1) * (x3 - x1);
}
static int top_left(int x1, int y1, int x2, int y2) {
	return (y1 == y2 && x1 > x2) || y1 < y2;
}

void scr_triangle(int x1, int y1, int x2, int y2, int x3, int y3) {
	int bx0, by0, bx1, by1, cx, cy, cw, ch, t, x, y;
	int bias0, bias1, bias2, a01, b01, a12, b12, a20, b20, w0row, w1row, w2row, w0, w1, w2;
	bx0 = x1 < x2 ? x1 : x2; if (x3 < bx0) bx0 = x3;
	by0 = y1 < y2 ? y1 : y2; if (y3 < by0) by0 = y3;
	bx1 = x1 > x2 ? x1 : x2; if (x3 > bx1) bx1 = x3;
	by1 = y1 > y2 ? y1 : y2; if (y3 > by1) by1 = y3;
	// mclip: the clip a pixel smaller
	cx = clip_x > bx0 ? clip_x : bx0;
	cy = clip_y > by0 ? clip_y : by0;
	cw = (clip_x + clip_w - 1 < bx1 ? clip_x + clip_w - 1 : bx1) - cx;
	ch = (clip_y + clip_h - 1 < by1 ? clip_y + clip_h - 1 : by1) - cy;
	if (cw <= 0 || ch <= 0) return;
	if (orient2d(x1, y1, x2, y2, x3, y3) < 0) {
		t = x1; x1 = x3; x3 = t;
		t = y1; y1 = y3; y3 = t;
	}
	bias0 = top_left(x2, y2, x3, y3) ? 0 : -1;
	bias1 = top_left(x3, y3, x1, y1) ? 0 : -1;
	bias2 = top_left(x1, y1, x2, y2) ? 0 : -1;
	a01 = y1 - y2; b01 = x2 - x1;
	a12 = y2 - y3; b12 = x3 - x2;
	a20 = y3 - y1; b20 = x1 - x3;
	w0row = orient2d(x2, y2, x3, y3, cx, cy) + bias0;
	w1row = orient2d(x3, y3, x1, y1, cx, cy) + bias1;
	w2row = orient2d(x1, y1, x2, y2, cx, cy) + bias2;
	for (y = cy; y <= cy + ch; y++) {
		w0 = w0row; w1 = w1row; w2 = w2row;
		for (x = cx; x <= cx + cw; x++) {
			if ((w0 | w1 | w2) >= 0) pen_span(scr_pen, x + y * SCREEN_W, 1);
			w0 += a12; w1 += a20; w2 += a01;
		}
		w0row += b12; w1row += b20; w2row += b01;
	}
}

// Surface::blit(src, r, p), which clips with uint8_t offsets, as here
void scr_blit(Surf *src, int sx, int sy, int sw, int sh, int px, int py) {
	int dx, dy, dw, dh, l, t, soff, doff, y;
	dx = clip_x > px ? clip_x : px;
	dy = clip_y > py ? clip_y : py;
	dw = (clip_x + clip_w < px + sw ? clip_x + clip_w : px + sw) - dx;
	dh = (clip_y + clip_h < py + sh ? clip_y + clip_h : py + sh) - dy;
	if (dw <= 0 || dh <= 0) return;
	l = (dx - px) & 255;
	t = (dy - py) & 255;
	sx += l;
	sy += t;
	soff = sx + sy * src->w;
	doff = dx + dy * SCREEN_W;
	for (y = py; y < py + dh; y++) {
		row_blit(src, soff, doff, dw);
		soff += src->w;
		doff += SCREEN_W;
	}
}

// Surface::sprite(rect in tiles, position): Surface::blit(src, sprite, p, 0)
void scr_sprite(Surf *src, int sx, int sy, int sw, int sh, int px, int py) {
	int dx, dy, dw, dh, left, top, y, count, doff;
	sx *= 8; sy *= 8; sw *= 8; sh *= 8;
	dx = clip_x > px ? clip_x : px;
	dy = clip_y > py ? clip_y : py;
	dw = (clip_x + clip_w < px + sw ? clip_x + clip_w : px + sw) - dx;
	dh = (clip_y + clip_h < py + sh ? clip_y + clip_h : py + sh) - dy;
	if (dw <= 0 || dh <= 0) return;
	left = dx - px;
	top = dy - py;
	doff = dx + dy * SCREEN_W;
	count = dh;
	y = top;
	do {
		row_blit(src, sx + left + (sy + y) * src->w, doff, dw);
		doff += SCREEN_W;
		y++;
	} while (--count);
}

static int run_x[SCREEN_W], run_n[SCREEN_W];
static int vis_pen[SCREEN_W], vis_opaque[SCREEN_W], vis_off[SCREEN_W], vis_n[SCREEN_W];

// Surface::stretch_blit(src, sprite, r, t), for no flip or a flip across
void scr_stretch(Surf *src, int sx, int sy, int sw, int sh, int rx, int ry, int rw, int rh, int t) {
	int dx, dy, dw, dh, scale_x, scale_y, left, top, x_step, y_step, doff, y_count, y, x_scale_px;
	int x_count, x, num, new_x, sw2, alpha1, p, a, v, c, dr, dg, db, k, j, last, vis, off, src_y;
	int *base;
	unsigned char *data, *row;
	int *pal, *d;
	dx = clip_x > rx ? clip_x : rx;
	dy = clip_y > ry ? clip_y : ry;
	dw = (clip_x + clip_w < rx + rw ? clip_x + clip_w : rx + rw) - dx;
	dh = (clip_y + clip_h < ry + rh ? clip_y + clip_h : ry + rh) - dy;
	if (dw <= 0 || dh <= 0) return;
	if (dy < drawn_y0) drawn_y0 = dy;
	if (dy + dh - 1 > drawn_y1) drawn_y1 = dy + dh - 1;
	scale_x = (sw << 16) / rw;
	scale_y = (sh << 16) / rh;
	left = (dx - rx) * scale_x;
	top = (dy - ry) * scale_y;
	y_step = scale_y;
	x_step = scale_x;
	if (t & XFORM_HORIZONTAL) {
		left = (sw << 16) - 1 - left;
		x_step = -x_step;
	}
	doff = dx + dy * SCREEN_W;
	y_count = dh;
	y = top;
	x_scale_px = 1;
	if (rw > sw) {
		x_scale_px = 65536 / scale_x;
		if (dw < x_scale_px) x_scale_px = dw;
	}
	data = src->data;
	pal = src->pal;
	sw2 = src->w;
	alpha1 = scr_alpha + 1;
	if (x_scale_px == 1) {
		// no wider than the source: whatever else upstream's loop does, each
		// pixel is the source pixel under it
		do {
			row = data + sx + (sy + (y >> 16)) * sw2;
			x = left;
			d = fb + doff;
			x_count = dw;
			do {
				p = pal[row[x >> 16]];
				x += x_step;
				if (p >> 24) {
					a = (((p >> 24) & 255) + 1) * alpha1 >> 8;
					if (a >= 255) *d = p & 0xFFFFFF;
					else {
						v = *d;
						dr = (v >> 16) & 255;
						dg = (v >> 8) & 255;
						db = v & 255;
						*d = ((dr + ((a * (((p >> 16) & 255) - dr) + 127) >> 8)) << 16) |
						     ((dg + ((a * (((p >> 8) & 255) - dg) + 127) >> 8)) << 8) |
						     (db + ((a * ((p & 255) - db) + 127) >> 8));
					}
				}
				d++;
			} while (--x_count);
			doff += SCREEN_W;
			y += y_step;
		} while (--y_count);
		return;
	}
	// wider than the source: every row steps through its source the same way, so
	// upstream's stepping is worked out once, into a table
	k = 0;
	x_count = dw;
	x = left;
	num = (x & 65535) == 0 ? x_scale_px : 1;
	do {
		new_x = x + x_step * num;
		if (x >> 16 == new_x >> 16 && x_count > num) {
			num++;
			new_x += x_step;
		}
		run_x[k] = x >> 16;
		run_n[k] = num;
		k++;
		x = new_x;
		x_count -= num;
		num = x_count < x_scale_px ? x_count : x_scale_px;
	} while (x_count);
	// and many rows show the same row of the source, mostly nothing: its visible runs
	// are worked out when it changes, and the rows that show it draw only those
	last = -1;
	vis = 0;
	do {
		src_y = sy + (y >> 16);
		if (src_y != last) {
			last = src_y;
			row = data + sx + src_y * sw2;
			vis = 0;
			off = 0;
			for (j = 0; j < k; j++) {
				p = pal[row[run_x[j]]];
				if (p >> 24) {
					a = (((p >> 24) & 255) + 1) * alpha1 >> 8;
					vis_pen[vis] = a >= 255 ? p & 0xFFFFFF : p;
					vis_opaque[vis] = a >= 255;
					vis_off[vis] = off;
					vis_n[vis] = run_n[j];
					vis++;
				}
				off += run_n[j];
			}
		}
		base = fb + doff;
		for (j = 0; j < vis; j++) {
			d = base + vis_off[j];
			if (vis_opaque[j]) {
				v = vis_pen[j];
				c = vis_n[j];
				while (c >= 4) {
					d[0] = v; d[1] = v; d[2] = v; d[3] = v;
					d += 4;
					c -= 4;
				}
				while (c) { *d++ = v; c--; }
			} else
				pen_span(vis_pen[j], doff + vis_off[j], vis_n[j]);
		}
		doff += SCREEN_W;
		y += y_step;
	} while (--y_count);
}

// Surface::sprite(rect, position, origin, scale, transform)
void scr_sprite_scaled(Surf *src, int sx, int sy, int sw, int sh, int px, int py, int ox, int oy, float scale, int t) {
	int rx, ry, rw, rh;
	rx = (int) sbk_roundf((float) px - (float) ox * scale);
	ry = (int) sbk_roundf((float) py - (float) oy * scale);
	rw = (int) sbk_roundf((float) sw * 8.0f * scale);
	rh = (int) sbk_roundf((float) sh * 8.0f * scale);
	scr_stretch(src, sx * 8, sy * 8, sw * 8, sh * 8, rx, ry, rw, rh, t);
}

// Surface::measure_text, with variable widths, on one line
int measure_w(char *s, Font *f) {
	int n, c;
	n = 0;
	while (*s) {
		c = *s++ & 0x7F;
		c = c < 32 ? 0 : c - 32;
		n += f->widths[c];
	}
	return n;
}
int measure_h(Font *f) { return f->char_h + f->spacing; }

// Surface::text, variable widths, on one line
void scr_text(char *s, Font *f, int rx, int ry, int rw, int rh, int align) {
	int cx, cy, hb, size, c, y, x, po, xx, yy, bit, ch, cw;
	unsigned char *g;
	int plain, ink;
	plain = pen_alpha((scr_pen >> 24) & 255, scr_alpha) >= 255;
	ink = scr_pen & 0xFFFFFF;
	if (clip_x > rx + rw || clip_x + clip_w < rx || clip_y > ry + rh || clip_y + clip_h < ry) return;
	cx = rx;
	cy = ry;
	if ((align & 3) != 0) {
		if ((align & 3) == ALIGN_BOTTOM) cy += rh - measure_h(f);
		else cy += (rh - measure_h(f)) / 2;
	}
	if ((align & 12) != 0) cx += (rw - measure_w(s, f)) / 2;
	ch = f->char_h;
	cw = f->char_w;
	hb = (ch + 7) / 8;
	size = cw * hb;
	while (*s) {
		c = *s++ & 0x7F;
		c = c < 32 ? 0 : c - 32;
		g = f->data + c * size;
		// down each column, as the font is held, where a byte of no ink is one look
		for (x = 0; x < cw; x++) {
			xx = cx + x;
			if (xx < clip_x || xx >= clip_x + clip_w) { g += hb; continue; }
			for (y = 0; y < ch; y += 8) {
				bit = *g++;
				if (!bit) continue;
				for (po = 0; po < 8; po++) {
					if (bit & (1 << po)) {
						yy = cy + y + po;
						if (yy >= 0 && yy >= clip_y && yy < clip_y + clip_h) {
							if (plain) fb[xx + yy * SCREEN_W] = ink;
							else pen_span(scr_pen, xx + yy * SCREEN_W, 1);
						}
					}
				}
			}
		}
		cx += f->widths[c];
	}
}

void scr_textat(char *s, Font *f, int x, int y, int align) { scr_text(s, f, x, y, 0, 0, align); }

// text on a surface of its own (stretch_text in race.cpp): index 1 where there is ink
void text_bytes(char *s, Font *f, unsigned char *buf, int w, int h) {
	int cx, hb, size, c, y, x, ch, cw, xx;
	unsigned char *g;
	ch = f->char_h;
	cw = f->char_w;
	hb = (ch + 7) / 8;
	size = cw * hb;
	cx = 0;
	while (*s) {
		c = *s++ & 0x7F;
		c = c < 32 ? 0 : c - 32;
		g = f->data + c * size;
		for (y = 0; y < ch && y < h; y++) {
			for (x = 0; x < cw; x++) {
				xx = cx + x;
				if ((g[x * hb + y / 8] & (1 << (y & 7))) && xx < w) buf[xx + y * w] = 1;
			}
		}
		cx += f->widths[c];
	}
}

// hsv_to_rgba. A value out of a byte's range is what a cast of it to uint8_t
// comes to on x86, which hsv_to_rgba reaches before the race starts (place 0).
int hsv_to_pen(float h, float s, float v) {
	int i, p, q, t, bv;
	float f, sv, fsv;
	i = (int) (h * 6.0f);
	f = h * 6.0f - (float) i;
	v = v * 255.0f;
	sv = s * v;
	fsv = f * sv;
	p = ((int) (-sv + v)) & 255;
	q = ((int) (-fsv + v)) & 255;
	t = ((int) (fsv - sv + v)) & 255;
	bv = ((int) v) & 255;
	switch (i % 6) {
	case 1: return RGB(q, bv, p);
	case 2: return RGB(p, bv, t);
	case 3: return RGB(p, q, bv);
	case 4: return RGB(t, p, bv);
	case 5: return RGB(bv, p, q);
	}
	return RGB(bv, t, p);
}
