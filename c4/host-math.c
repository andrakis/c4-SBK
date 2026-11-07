// host-math.c -- for gcc's builds, C4IX libc's math.c in place of glibc's, so that
// the only difference left between the builds make test-sbk compares is the
// compiler. As Puzzles found, both are within an ulp of the truth but not the
// same one. The arithmetic here is SSE's, and on the machine, soft float's.
// sinf, cosf and atan2f are the game's own (src/sbkmath.c), whose names the original
// C++ under make test-sbk-ref calls by these.
#define errno c4_errno
#define sinf c4ix_sinf
#define cosf c4ix_cosf
#define atan2f c4ix_atan2f
int c4_errno;
static unsigned long long __c4d_sqrt(unsigned long long b)
{
    double d = *(double *)&b;
    __asm__("sqrtsd %0, %0" : "+x" (d));
    return *(unsigned long long *)&d;
}
#include "src/c4ix/libc/math.c"
#undef sinf
#undef cosf
#undef atan2f

float sbk_sinf(float x);
float sbk_cosf(float x);
float sbk_atan2f(float y, float x);
float sinf(float x) { return sbk_sinf(x); }
float cosf(float x) { return sbk_cosf(x); }
float atan2f(float y, float x) { return sbk_atan2f(y, x); }
void sincosf(float x, float *s, float *c) { *s = sbk_sinf(x); *c = sbk_cosf(x); }
