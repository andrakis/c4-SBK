// acceltest.c -- c4's rewrite: Super Blit Kart's accelerators (c4/accel.mjs)
// against the loops they stand for, as track.c, blit.c and game.c have
// them (their globals as arguments): the same pixels, on made-up maps,
// tile sheets, palettes, alphas and steps, and the same hashes. test-sbk
// runs it on c4's libjs.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "c4accel.h"

int sbk_accel = -1;
int sbk_accel_on = 1;
int sbk_accel_base(void) { return sbk_accel = accel_find("sbk"); }

unsigned seed = 12345;
int rnd(int n) { seed = seed * 1103515245 + 12345; return (int)((seed >> 8) % (unsigned)n); }
int rndi(void) { return (int)(((unsigned)rnd(1 << 16) << 16) | (unsigned)rnd(1 << 16)); }

enum { SW = 320, SH = 240, N = SW * SH, MW = 32, WSHIFT = 8 };   // a 32x32 map of 8x8 tiles: 256 texels a row
int *fbc, *fba, *base, *row, *pal, *rgb;
unsigned char *world, *tiles, *trans, *sheet, *src;
int fails, cases;

void check(char *what, int k) {
    cases++;
    if (memcmp(fbc, fba, N * 4) != 0) {
        fails++;
        if (fails < 10) printf("accel: %s, case %d, differs\n", what, k);
    }
}
void clear() { int i; for (i = 0; i < 16; i++) base[rnd(N)] = rndi() & 0xFFFFFF; memcpy(fbc, base, N * 4); memcpy(fba, base, N * 4); }

// track.c's world_clamp, span_edge, span_pixels and span_runs
int world_clamp(int x, int y) {
    int w, cx, cy;
    w = MW;
    cx = x >> 3;
    cy = y >> 3;
    if (cx != (cx & (w - 1))) cx = cx < 0 ? 0 : w - 1;
    if (cy != (cy & (w - 1))) cy = cy < 0 ? 0 : w - 1;
    return world[(cx << 3) + (x & 7) + (((cy << 3) + (y & 7)) << WSHIFT)];
}
void span_edge(int empty, int *d, int c, int wx, int wy, int dx, int dy) {
    int notmask, x, y;
    notmask = ~((MW << 3) - 1);
    do {
        x = wx >> 16;
        y = wy >> 16;
        if ((x | y) & notmask) {
            if (!empty) *d = row[world_clamp(x, y)];
        } else
            *d = row[world[x + (y << WSHIFT)]];
        d++;
        wx += dx;
        wy += dy;
    } while (--c);
}
void span_pixels(int *d, int c, int wx, int wy, int dx, int dy) {
    while (c) {
        *d++ = row[world[(wx >> 16) + ((wy >> 16) << WSHIFT)]];
        wx += dx; wy += dy;
        c--;
    }
}
void span_runs(int *d, int c, int wx, int wy, int dx, int dy) {
    int notmask, x, y, v, kx, ky, n, i, col;
    notmask = ~((MW << 3) - 1);
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
        if ((x | y) & notmask) v = world_clamp(x, y);
        else v = world[x + (y << WSHIFT)];
        n = kx < ky ? kx : ky;
        if (n > c) n = c;
        c -= n; i += n; kx -= n; ky -= n;
        col = row[v];
        if (v != 255) { while (n) { *d++ = col; n--; } }
        else d += n;
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
void test_spans() {
    int k, y, x, c, wx, wy, dx, dy, kind, empty, lim;
    lim = MW << 19;
    for (k = 0; k < 1500; k++) {
        clear();
        kind = k % 3; y = rnd(SH); x = rnd(SW); c = 1 + rnd(SW - x); empty = rnd(2);
        if (kind == 0) {        // the runs: small steps, far from wrapping round
            dx = rnd(40000) - 20000; dy = rnd(40000) - 20000;
            wx = rnd(lim + (1 << 22)) - (1 << 21); wy = rnd(lim + (1 << 22)) - (1 << 21);
            span_runs(fbc + y * SW + x, c, wx, wy, dx, dy);
            sbk_runs(world, WSHIFT, MW, row, fba + y * SW + x, c, wx, wy, dx, dy);
        } else if (kind == 1) { // the pixels: all of it on the map
            dx = rnd(1 << 18) - (1 << 17); dy = rnd(1 << 18) - (1 << 17);
            wx = (1 << 20) + rnd(lim - (2 << 20)); wy = (1 << 20) + rnd(lim - (2 << 20));
            if (c * (dx < 0 ? -dx : dx) > (1 << 20)) c = 1 + (1 << 20) / (1 + (dx < 0 ? -dx : dx));
            if (c * (dy < 0 ? -dy : dy) > (1 << 20)) c = 1 + (1 << 20) / (1 + (dy < 0 ? -dy : dy));
            if (x + c > SW) c = SW - x;
            span_pixels(fbc + y * SW + x, c, wx, wy, dx, dy);
            sbk_pixels(world, WSHIFT, row, fba + y * SW + x, c, wx, wy, dx, dy);
        } else {                // the edge: anywhere, any step
            dx = rndi() >> rnd(12); dy = rndi() >> rnd(12);
            wx = rndi() >> rnd(8); wy = rndi() >> rnd(8);
            span_edge(empty, fbc + y * SW + x, c, wx, wy, dx, dy);
            sbk_edge(world, WSHIFT, MW, empty, row, fba + y * SW + x, c, wx, wy, dx, dy);
        }
        check(kind == 0 ? "span_runs" : kind == 1 ? "span_pixels" : "span_edge", k);
    }
}

// blit.c's pen_span (its globals, scr_alpha, as an argument), row_blit,
// scr_blit_tinted and scr_stretch's path no wider than the source
int pen_alpha(int a1, int a2) { return ((a1 + 1) * (a2 + 1)) >> 8; }
void pen_span(int *fb, int pen, int off, int c, int scr_alpha) {
    int a, v, sr, sg, sb, dr, dg, db;
    int *d;
    a = (pen >> 24) & 255;
    if (!a) return;
    a = ((a + 1) * (scr_alpha + 1)) >> 8;
    d = fb + off;
    if (a >= 255) {
        v = pen & 0xFFFFFF;
        while (c-- > 0) *d++ = v;
        return;
    }
    sr = (pen >> 16) & 255;
    sg = (pen >> 8) & 255;
    sb = pen & 255;
    while (c-- > 0) {
        v = *d;
        dr = (v >> 16) & 255;
        dg = (v >> 8) & 255;
        db = v & 255;
        *d++ = ((dr + ((a * (sr - dr) + 127) >> 8)) << 16) |
               ((dg + ((a * (sg - dg) + 127) >> 8)) << 8) |
               (db + ((a * (sb - db) + 127) >> 8));
    }
}
void test_pen() {
    int k, off, c, p, sa, a;
    for (k = 0; k < 800; k++) {
        clear();
        off = rnd(N - SW); c = rnd(SW); p = rndi(); sa = rnd(256);
        if (k & 1) p |= 0xFF000000;
        pen_span(fbc, p, off, c, sa);
        a = (p >> 24) & 255;
        if (a) sbk_pen(p, fba + off, c, ((a + 1) * (sa + 1)) >> 8);
        check("pen_span", k);
    }
}
void row_blit(int *d, unsigned char *s, int cnt, int alpha) {
    int p, a, v, dr, dg, db;
    do {
        p = pal[*s++];
        a = (((p >> 24) & 255) + 1) * alpha >> 8;
        if (a >= 255) {
            *d = p & 0xFFFFFF;
        } else if (a > 1) {
            v = *d;
            dr = (v >> 16) & 255;
            dg = (v >> 8) & 255;
            db = v & 255;
            *d = ((dr + ((a * (((p >> 16) & 255) - dr) + 127) >> 8)) << 16) |
                 ((dg + ((a * (((p >> 8) & 255) - dg) + 127) >> 8)) << 8) |
                 (db + ((a * ((p & 255) - db) + 127) >> 8));
        }
        d++;
    } while (--cnt);
}
void test_row() {
    int k, off, c, sa;
    unsigned char *s;
    for (k = 0; k < 800; k++) {
        clear();
        off = rnd(N - SW); c = 1 + rnd(SW - 1); sa = rnd(256); s = src + rnd(4096);
        row_blit(fbc + off, s, c, sa + 1);
        sbk_row(s, pal, fba + off, c, sa + 1);
        check("row_blit", k);
    }
}
void tinted(unsigned char *s, int w, int h, int x0, int y0, int a) {
    int *d;
    int x, y, c, v, p, dr, dg, db;
    for (y = 0; y < h; y++) {
        d = fbc + x0 + (y0 + y) * SW;
        x = w;
        do {
            c = *s++;
            if (c) {
                v = *d;
                p = rgb[c];
                dr = (v >> 16) & 255;
                dg = (v >> 8) & 255;
                db = v & 255;
                *d = ((dr + ((a * (((p >> 16) & 255) - dr) + 127) >> 8)) << 16) |
                     ((dg + ((a * (((p >> 8) & 255) - dg) + 127) >> 8)) << 8) |
                     (db + ((a * ((p & 255) - db) + 127) >> 8));
            }
            d++;
        } while (--x);
    }
}
void test_tint() {
    int k, w, h, x0, y0, a;
    unsigned char *s;
    for (k = 0; k < 300; k++) {
        clear();
        w = 1 + rnd(64); h = 1 + rnd(48); x0 = rnd(SW - w + 1); y0 = rnd(SH - h + 1); a = rnd(256);
        s = src + rnd(4096);
        tinted(s, w, h, x0, y0, a);
        sbk_tint(s, w, h, fba + x0 + y0 * SW, SW * 4, rgb, a);
        check("scr_blit_tinted", k);
    }
}
void stretch1(unsigned char *row0, int sy, int sw2, int y, int y_step, int left, int x_step, int doff, int dw, int dh, int alpha1) {
    int p, a, v, dr, dg, db, x, x_count, y_count;
    unsigned char *r;
    int *d;
    y_count = dh;
    do {
        r = row0 + (sy + (y >> 16)) * sw2;
        x = left;
        d = fbc + doff;
        x_count = dw;
        do {
            p = pal[r[x >> 16]];
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
        doff += SW;
        y += y_step;
    } while (--y_count);
}
void test_stretch() {
    int k, sw, sh, dw, dh, x, y, xs, ys, flip, left, sa;
    for (k = 0; k < 400; k++) {
        clear();
        sw = 8 + rnd(56); sh = 8 + rnd(56); flip = k & 1; sa = rnd(256);
        dw = 1 + rnd(sw); dh = 1 + rnd(sh);       // no wider than the source
        xs = (sw << 16) / dw; ys = (sh << 16) / dh;
        left = rnd(xs);
        if (flip) { left = (sw << 16) - 1 - left; xs = -xs; }
        x = rnd(SW - dw + 1); y = rnd(SH - dh + 1);
        {
            int y0 = rnd(ys);
            stretch1(src + 8, 3, 64, y0, ys, left, xs, x + y * SW, dw, dh, sa + 1);
            sbk_stretch1(src + 8, 3, 64, pal, y0, ys, left, xs, fba + x + y * SW, dw, dh, sa + 1);
        }
        check("scr_stretch", k);
    }
}

// scr_stretch's path wider than the source, and scr_text from where its
// first character goes, the screen's alpha and clip as arguments
int run_x[SW], run_n[SW], vis_pen[SW], vis_opaque[SW], vis_off[SW], vis_n[SW];
void stretchw(unsigned char *data, int sy, int sw2, int y, int y_step, int left, int x_step, int x_scale_px,
              int doff, int dw, int dh, int alpha1) {
    int k, x_count, x, num, new_x, last, vis, src_y, off, j, p, a, v, c, y_count;
    unsigned char *row;
    int *base, *d;
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
    last = -1;
    vis = 0;
    y_count = dh;
    do {
        src_y = sy + (y >> 16);
        if (src_y != last) {
            last = src_y;
            row = data + src_y * sw2;
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
        base = fbc + doff;
        for (j = 0; j < vis; j++) {
            d = base + vis_off[j];
            if (vis_opaque[j]) {
                v = vis_pen[j];
                c = vis_n[j];
                while (c) { *d++ = v; c--; }
            } else
                pen_span(fbc, vis_pen[j], doff + vis_off[j], vis_n[j], alpha1 - 1);
        }
        doff += SW;
        y += y_step;
    } while (--y_count);
}
void test_stretchw() {
    int k, sw, sh, dw, dh, x, y, xs, ys, flip, left, sa, xpx;
    for (k = 0; k < 400; k++) {
        clear();
        sw = 4 + rnd(28); sh = 4 + rnd(28); flip = k & 1; sa = rnd(256);
        dw = sw + 1 + rnd(3 * sw); dh = 1 + rnd(4 * sh);       // wider than the source
        if (dw > SW) dw = SW;
        if (dh > SH) dh = SH;
        xs = (sw << 16) / dw; ys = (sh << 16) / dh;
        xpx = 65536 / xs;
        if (dw < xpx) xpx = dw;
        if (xpx < 2) continue;
        left = k & 2 ? 0 : rnd(xs);
        if (flip) { left = (sw << 16) - 1 - left; xs = -xs; }
        x = rnd(SW - dw + 1); y = rnd(SH - dh + 1);
        {
            int y0 = rnd(ys);
            stretchw(src + 8, 3, 64, y0, ys, left, xs, xpx, x + y * SW, dw, dh, sa + 1);
            sbk_stretchw(src + 8, 3, 64, pal, y0, ys, left, xs, xpx, fba + x + y * SW, dw, dh, sa + 1);
        }
        check("scr_stretch, wider", k);
    }
}
void text(char *s, unsigned char *data, unsigned char *widths, int cw, int ch, int cx, int cy,
          int clip_x, int clip_y, int clip_w, int clip_h, int plain, int ink, int pen, int alpha) {
    int hb, size, c, x, y, xx, yy, bit, po;
    unsigned char *g;
    hb = (ch + 7) / 8;
    size = cw * hb;
    while (*s) {
        c = *s++ & 0x7F;
        c = c < 32 ? 0 : c - 32;
        g = data + c * size;
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
                            if (plain) fbc[xx + yy * SW] = ink;
                            else pen_span(fbc, pen, xx + yy * SW, 1, alpha);
                        }
                    }
                }
            }
        }
        cx += widths[c];
    }
}
void test_text() {
    int k, i, n, cw, ch, cx, cy, plain, pen, alpha, clx, cly, clw, clh;
    char str[24];
    unsigned char widths[96];
    for (k = 0; k < 400; k++) {
        clear();
        cw = 4 + rnd(8); ch = 6 + rnd(12);
        for (i = 0; i < 96; i++) widths[i] = 2 + rnd(cw);
        n = rnd(20);
        for (i = 0; i < n; i++) str[i] = 1 + rnd(127);
        str[n] = 0;
        clx = rnd(40); cly = rnd(40); clw = SW - clx - rnd(40); clh = SH - cly - rnd(40);
        cx = rnd(SW + 20) - 20; cy = rnd(SH + 20) - 20;
        plain = k & 1; pen = rndi(); alpha = rnd(256);
        text(str, src, widths, cw, ch, cx, cy, clx, cly, clw, clh, plain, pen & 0xFFFFFF, pen, alpha);
        sbk_text(str, src, widths, cw, ch, cx, cy, clx, cly, clw, clh, plain, pen & 0xFFFFFF, pen, alpha, fba);
        check(plain ? "scr_text" : "scr_text, blended", k);
    }
}

// track.c's map_clamp and span_any, the screen's alpha an argument
int map_clamp(int x, int y, int w) {
    int cx, cy;
    cx = x & (w - 1);
    cy = y & (w - 1);
    if (x != cx) cx = x < 0 ? 0 : w - 1;
    if (y != cy) cy = y < 0 ? 0 : w - 1;
    return cx + cy * w;
}
void span_any(int *fb, int doff, int c, int wx, int wy, int dx, int dy, int alpha) {
    int w, wcx, wcy, toff, tile, tr, u, v, tmp, count, pen;
    w = MW;
    do {
        wcx = wx >> 16;
        wcy = wy >> 16;
        toff = map_clamp(wcx >> 3, wcy >> 3, w);
        tile = tiles[toff];
        if (tile != 0) {
            tr = trans ? trans[toff] : 0;
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
            pen_span(fb, pen, doff, count, alpha);
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
void test_any() {
    int k, x, y, c, wx, wy, dx, dy, alpha, lim;
    lim = MW << 19;
    for (k = 0; k < 600; k++) {
        clear();
        y = rnd(SH); x = rnd(SW); c = 1 + rnd(SW - x); alpha = rnd(256);
        dx = rnd(1 << 18) - (1 << 17); dy = rnd(1 << 18) - (1 << 17);
        wx = rnd(lim + (1 << 22)) - (1 << 21); wy = rnd(lim + (1 << 22)) - (1 << 21);
        span_any(fbc, x + y * SW, c, wx, wy, dx, dy, alpha);
        sbk_any(tiles, trans, pal, sheet, MW, fba + x + y * SW, c, wx, wy, dx, dy, alpha);
        check("span_any", k);
    }
}

// game.c's frame_hash
unsigned frame_hash(int *fb) {
    unsigned h;
    int *p, *end;
    h = 2166136261u;
    p = fb;
    end = fb + N;
    while (p < end) h = (h ^ (unsigned) *p++) * 16777619u;
    return h;
}
void test_hash() {
    int k, i;
    unsigned a, c;
    for (k = 0; k < 20; k++) {
        for (i = 0; i < 64; i++) fbc[rnd(N)] = rndi();
        c = frame_hash(fbc);
        a = (unsigned) sbk_hash(fbc, N);
        cases++;
        if (a != c) { fails++; if (fails < 10) printf("accel: frame_hash, case %d, %08x, not %08x\n", k, a, c); }
    }
}

int main() {
    int i;
    if (SBK_ACCEL() <= 0) { printf("accel: SBK's are not here\n"); return 0; }
    fbc = (int *) malloc(N * 4); fba = (int *) malloc(N * 4); base = (int *) malloc(N * 4);
    row = (int *) malloc(256 * 4); pal = (int *) malloc(256 * 4); rgb = (int *) malloc(256 * 4);
    world = (unsigned char *) malloc((MW * 8) << WSHIFT);
    src = (unsigned char *) malloc(8192);
    for (i = 0; i < N; i++) base[i] = rndi() & 0xFFFFFF;
    for (i = 0; i < 256; i++) { row[i] = rndi() & 0xFFFFFF; pal[i] = rndi(); rgb[i] = rndi() & 0xFFFFFF; }
    for (i = 0; i < 64; i++) pal[rnd(256)] &= 0xFFFFFF;     // some clear
    for (i = 0; i < 64; i++) pal[rnd(256)] |= 0xFF000000;   // some opaque
    for (i = 0; i < ((MW * 8) << WSHIFT); i++) world[i] = rnd(8) ? rnd(255) : 255;
    for (i = 0; i < 8192; i++) src[i] = rnd(4) ? rnd(256) : 0;
    tiles = (unsigned char *) malloc(MW * MW); trans = (unsigned char *) malloc(MW * MW);
    sheet = (unsigned char *) malloc(128 * 128);
    for (i = 0; i < MW * MW; i++) { tiles[i] = rnd(3) ? rnd(256) : 0; trans[i] = rnd(2) ? rnd(8) : 0; }
    for (i = 0; i < 128 * 128; i++) sheet[i] = rnd(256);
    test_spans(); test_pen(); test_row(); test_tint(); test_stretch(); test_stretchw(); test_text(); test_any(); test_hash();
    if (fails) printf("accel: %d of %d cases differ\n", fails, cases);
    else printf("accel: SBK's, %d cases, each as its loop has it\n", cases);
    return fails != 0;
}
