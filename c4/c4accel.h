/* c4accel.h -- c4's rewrite: Super Blit Kart's accelerators, c4/accel.mjs,
   from the game's side (c4's libjs/guest/accel.h, docs/accel.md). On a
   machine that has them, with the main menu's Accelerated on, the host
   runs these in the game's own JavaScript, one instruction each;
   elsewhere, and in gcc's build, the C runs as before. sbk_accel is the
   first of the game's numbers (0 for none, -1 until asked: game.c);
   SBK_ACCEL() asks once. The numbers are the module's operations in its
   order. */
#ifndef C4ACCEL_H
#define C4ACCEL_H

enum { SA_HASH, SA_PIXELS, SA_RUNS, SA_EDGE, SA_PEN, SA_ROW, SA_TINT, SA_ANY, SA_STRETCH1, SA_STRETCHW, SA_TEXT };

extern int sbk_accel;
int sbk_accel_base(void);
int sbk_accel_avail(void);      /* the machine has them */
void sbk_accel_set(int on);     /* the menu's switch, kept in a save */
extern int sbk_accel_on;        /* Accelerated */
#define SBK_ACCEL() (sbk_accel >= 0 ? sbk_accel : sbk_accel_base())

#ifdef __c4_inttypes__
#include "accel.h"
#define SA_OP(op) (sbk_accel + (op))
/* game.c's frame_hash over n words */
#define sbk_hash(p, n) __c4_opcode((p), (n), SA_OP(SA_HASH))
/* track.c's span_pixels, span_runs and span_edge */
#define sbk_pixels(world, wshift, row, d, c, wx, wy, dx, dy) \
	__c4_opcode((world), (wshift), (row), (d), (c), (wx), (wy), (dx), (dy), SA_OP(SA_PIXELS))
#define sbk_runs(world, wshift, w, row, d, c, wx, wy, dx, dy) \
	__c4_opcode((world), (wshift), (w), (row), (d), (c), (wx), (wy), (dx), (dy), SA_OP(SA_RUNS))
#define sbk_edge(world, wshift, w, empty, row, d, c, wx, wy, dx, dy) \
	__c4_opcode((world), (wshift), (w), (empty), (row), (d), (c), (wx), (wy), (dx), (dy), SA_OP(SA_EDGE))
/* blit.c's pen_span from its blend on, a with the screen's alpha already */
#define sbk_pen(pen, d, c, a) __c4_opcode((pen), (d), (c), (a), SA_OP(SA_PEN))
/* blit.c's row_blit, alpha1 the screen's alpha + 1 */
#define sbk_row(s, pal, d, cnt, alpha1) __c4_opcode((s), (pal), (d), (cnt), (alpha1), SA_OP(SA_ROW))
/* blit.c's scr_blit_tinted, d0 its first row's first pixel */
#define sbk_tint(src, w, h, d0, stride, rgb, a) \
	__c4_opcode((src), (w), (h), (d0), (stride), (rgb), (a), SA_OP(SA_TINT))
/* track.c's span_any at the screen's alpha */
#define sbk_any(tiles, trans, pal, sheet, w, d, c, wx, wy, dx, dy, alpha) \
	__c4_opcode((tiles), (trans), (pal), (sheet), (w), (d), (c), (wx), (wy), (dx), (dy), (alpha), SA_OP(SA_ANY))
/* blit.c's scr_stretch no wider than the source */
#define sbk_stretch1(row0, sy, sw2, pal, y, ystep, left, xstep, d, dw, dh, alpha1) \
	__c4_opcode((row0), (sy), (sw2), (pal), (y), (ystep), (left), (xstep), (d), (dw), (dh), (alpha1), SA_OP(SA_STRETCH1))
/* ... and wider than it, xpx its x_scale_px */
#define sbk_stretchw(row0, sy, sw2, pal, y, ystep, left, xstep, xpx, d, dw, dh, alpha1) \
	__c4_opcode((row0), (sy), (sw2), (pal), (y), (ystep), (left), (xstep), (xpx), (d), (dw), (dh), (alpha1), \
		SA_OP(SA_STRETCHW))
/* blit.c's scr_text from where its first character goes */
#define sbk_text(s, data, widths, cw, ch, cx, cy, clx, cly, clw, clh, plain, ink, pen, alpha, fb) \
	__c4_opcode((s), (data), (widths), (cw), (ch), (cx), (cy), (clx), (cly), (clw), (clh), (plain), (ink), (pen), \
		(alpha), (fb), SA_OP(SA_TEXT))
#else
#define sbk_stretchw(row0, sy, sw2, pal, y, ystep, left, xstep, xpx, d, dw, dh, alpha1) 0
#define sbk_text(s, data, widths, cw, ch, cx, cy, clx, cly, clw, clh, plain, ink, pen, alpha, fb) 0
#define accel_find(name) 0
#define sbk_hash(p, n) 0
#define sbk_pixels(world, wshift, row, d, c, wx, wy, dx, dy) 0
#define sbk_runs(world, wshift, w, row, d, c, wx, wy, dx, dy) 0
#define sbk_edge(world, wshift, w, empty, row, d, c, wx, wy, dx, dy) 0
#define sbk_pen(pen, d, c, a) 0
#define sbk_row(s, pal, d, cnt, alpha1) 0
#define sbk_tint(src, w, h, d0, stride, rgb, a) 0
#define sbk_any(tiles, trans, pal, sheet, w, d, c, wx, wy, dx, dy, alpha) 0
#define sbk_stretch1(row0, sy, sw2, pal, y, ystep, left, xstep, d, dw, dh, alpha1) 0
#endif

#endif
