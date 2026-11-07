// sbkmath.c -- sines and arctangents in float. C4IX's libm works in double, on a
// machine where every double op takes two words of helpers: atan2f was 4,900
// instructions, and the machine's karts need it twice an update each. These are
// about 500, within an ulp or so. They are fdlibm's float kernels (s_atanf.c,
// k_sinf.c, k_cosf.c): "Copyright (C) 1993 by Sun Microsystems, Inc. All rights
// reserved. Developed at SunPro, a Sun Microsystems, Inc. business. Permission
// to use, copy, modify, and distribute this software is freely granted,
// provided that this notice is preserved."
//
// Every build uses these, the original C++ under make test-sbk-ref too, so
// the frames compare; only a game on another libm would differ, and by that ulp.

#include "sbk.h"

// the bits of a float, and back
#define F_BITS(f) (*(int *) &(f))

static float atan_hi[4] = { 4.6364760399e-01f, 7.8539812565e-01f, 9.8279368877e-01f, 1.5707962513e+00f };
static float atan_lo[4] = { 5.0121582440e-09f, 3.7748947079e-08f, 3.4473217170e-08f, 7.5497894159e-08f };

static float atan_kernel(float x) {
	int ix, id, neg;
	float z, w, s1, s2;
	ix = F_BITS(x);
	neg = ix < 0;
	ix = ix & 0x7FFFFFFF;
	if (ix >= 0x4C800000) {          // past 2^26
		z = atan_hi[3] + atan_lo[3];
		return neg ? -z : z;
	}
	if (ix < 0x3EE00000) {           // below 0.4375
		if (ix < 0x39800000) return x;
		id = -1;
	} else {
		if (neg) x = -x;
		if (ix < 0x3F980000) {       // below 1.1875
			if (ix < 0x3F300000) { id = 0; x = (2.0f * x - 1.0f) / (2.0f + x); }
			else { id = 1; x = (x - 1.0f) / (x + 1.0f); }
		} else if (ix < 0x401C0000) { id = 2; x = (x - 1.5f) / (1.0f + 1.5f * x); }
		else { id = 3; x = -1.0f / x; }
	}
	z = x * x;
	w = z * z;
	s1 = z * (3.3333328366e-01f + w * (1.4253635705e-01f + w * 6.1687607318e-02f));
	s2 = w * (-1.9999158382e-01f + w * -1.0648017377e-01f);
	if (id < 0) return x - x * (s1 + s2);
	z = atan_hi[id] - ((x * (s1 + s2) - atan_lo[id]) - x);
	return neg ? -z : z;
}

// fdlibm's e_atan2f.c, for what a kart can ask
float sbk_atan2f(float y, float x) {
	int ix, iy, hx, hy, k;
	float z;
	hx = F_BITS(x);
	hy = F_BITS(y);
	ix = hx & 0x7FFFFFFF;
	iy = hy & 0x7FFFFFFF;
	if (ix > 0x7F800000 || iy > 0x7F800000) return x + y;     // NaN
	if (hx == 0x3F800000) return atan_kernel(y);              // x = 1
	if (iy == 0) {
		if (hx >= 0) return y;                                // +x, or +0
		return hy < 0 ? -3.1415927410e+00f : 3.1415927410e+00f;
	}
	if (ix == 0) return hy < 0 ? -1.5707963705e+00f : 1.5707963705e+00f;
	if (ix == 0x7F800000) {
		if (iy == 0x7F800000)
			z = hx >= 0 ? 7.8539818525e-01f : 2.3561945200e+00f;
		else
			z = hx >= 0 ? 0.0f : 3.1415927410e+00f;
		return hy < 0 ? -z : z;
	}
	if (iy == 0x7F800000) return hy < 0 ? -1.5707963705e+00f : 1.5707963705e+00f;
	k = (iy - ix) >> 23;
	if (k > 26) {                    // |y/x| past 2^26
		z = 1.5707963705e+00f + 0.5f * -8.7422776573e-08f;
	} else if (k < -26 && hx < 0) {
		z = 0.0f;                    // |y|/x below -2^-26
	} else {
		z = (y < 0.0f ? -y : y) / (x < 0.0f ? -x : x);
		z = atan_kernel(z);
	}
	if (hx >= 0) return hy < 0 ? -z : z;
	z = 3.1415927410e+00f - (z - -8.7422776573e-08f);
	return hy < 0 ? -z : z;
}

// on |x| up to pi/4; below 0.1, which is nearly all a kart turns by, the terms left
// out are a long way under an ulp
static float k_sin(float x) {
	float z, v;
	int ix;
	ix = F_BITS(x) & 0x7FFFFFFF;
	if (ix < 0x39000000) return x;
	z = x * x;
	v = z * x;
	if (ix < 0x3DCCCCCD) return x + v * (-1.6666667163e-01f + z * 8.3333337680e-03f);
	return x + v * (-1.6666667163e-01f + z * (8.3333337680e-03f + z * (-1.9841270114e-04f + z * (2.7557314297e-06f + z * -2.5050759689e-08f))));
}

static float k_cos(float x) {
	float z, r, h, q;
	int ix;
	ix = F_BITS(x) & 0x7FFFFFFF;
	if (ix < 0x39000000) return 1.0f;
	z = x * x;
	if (ix < 0x3DCCCCCD) {
		r = z * (4.1666667908e-02f + z * -1.3888889225e-03f);
		return 1.0f - (0.5f * z - z * r);
	}
	r = z * (4.1666667908e-02f + z * (-1.3888889225e-03f + z * (2.4801587642e-05f + z * -2.7557314297e-07f)));
	if (ix < 0x3E99999A) return 1.0f - (0.5f * z - z * r);
	// past 0.3: fdlibm's way to keep the last bits
	q = ix > 0x3F480000 ? 0.28125f : 0.0f;
	if (q == 0.0f) *(int *) &q = ix - 0x01000000;
	h = 0.5f * z - q;
	return (1.0f - q) - (h - z * r);
}

// a whole circle or two, which is what a ghost's angles need
static float reduce(float x, int *q) {
	int n;
	n = (int) (x * 6.3661980629e-01f + (x < 0.0f ? -0.5f : 0.5f));
	*q = n & 3;
	return (x - (float) n * 1.5707855225e+00f) - (float) n * 1.0804334124e-05f;
}

float sbk_sinf(float x) {
	int q;
	if ((F_BITS(x) & 0x7FFFFFFF) <= 0x3F490FDB) return k_sin(x);
	x = reduce(x, &q);
	switch (q) {
	case 0: return k_sin(x);
	case 1: return k_cos(x);
	case 2: return -k_sin(x);
	}
	return -k_cos(x);
}

float sbk_cosf(float x) {
	int q;
	if ((F_BITS(x) & 0x7FFFFFFF) <= 0x3F490FDB) return k_cos(x);
	x = reduce(x, &q);
	switch (q) {
	case 0: return k_cos(x);
	case 1: return -k_sin(x);
	case 2: return -k_cos(x);
	}
	return k_sin(x);
}

// roundf, as libm's: halves away from zero. Only what becomes an int here.
float sbk_roundf(float x) {
	float t, d;
	if ((F_BITS(x) & 0x7FFFFFFF) >= 0x4B000000) return x;     // whole already, or not a number
	t = (float) (int) x;
	d = x - t;
	if (x < 0.0f) { if (d <= -0.5f) t = t - 1.0f; }
	else if (d >= 0.5f) t = t + 1.0f;
	return t;
}

// sqrtf, which C4IX's libc does through a double, at ten times the cost
#ifdef SBK_C4
float sbk_sqrtf(float x) {
	unsigned b;
	b = __c4f_sqrt(*(unsigned *) &x);
	return *(float *) &b;
}
#endif
