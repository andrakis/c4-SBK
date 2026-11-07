// host-math.c -- for gcc's builds, C4IX libc's math.c in place of glibc's, so that
// the only difference left between the builds make test-sbk compares is the
// compiler. As Puzzles found, both are within an ulp of the truth but not the
// same one. The arithmetic here is SSE's, and on the machine, soft float's.
#define errno c4_errno
int c4_errno;
static unsigned long long __c4d_sqrt(unsigned long long b)
{
    double d = *(double *)&b;
    __asm__("sqrtsd %0, %0" : "+x" (d));
    return *(unsigned long long *)&d;
}
#include "src/c4ix/libc/math.c"

// gcc makes one call of cosf and sinf of the same angle
void sincosf(float x, float *s, float *c) { *s = sinf(x); *c = cosf(x); }
