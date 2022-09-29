
/* unixperf - an x11perf-style Unix performance benchmark */

/* test_cpu.c tests raw CPU performance. */

#include "unixperf.h"
#include "test_cpu.h"
#include "random.h"

#include <bstring.h>
#include <math.h>

#if (_MIPS_SZINT == 64)
#define S32BIT __int32_t
#endif

#ifndef S32BIT
#define S32BIT int /* XXX int should be a signed 32-bit type */
#endif

static S32BIT *buffer, *buffer2;
static unsigned int stashedSize;
static unsigned int memsize;

Bool
InitQsort(Version version, unsigned int size)
{
   int i;

   stashedSize = size;
   memsize = size * sizeof(S32BIT);
   buffer = (S32BIT*) malloc(memsize);
   MEMERROR(buffer);
   buffer2 = (S32BIT*) malloc(memsize);
   MEMERROR(buffer2);
   OSIsrandom(23123);
   for(i=0;i<size;i++) {
       buffer[i] = OSIrandom();
   }
   return TRUE;
}

Bool
InitQsort400(Version version)
{
    return InitQsort(version, 400);
}

Bool
InitQsort16K(Version version)
{
    return InitQsort(version, 16000);
}

Bool
InitQsort64K(Version version)
{
    return InitQsort(version, 64000);
}

static int
compare(const void *a, const void *b)
{
   S32BIT *x, *y;

   x = (S32BIT*) a;
   y = (S32BIT*) b;
   return *x - *y;
}

unsigned int
DoQsort(void)
{
   bcopy(buffer, buffer2, memsize);
   qsort(buffer2, stashedSize, sizeof(S32BIT), compare);
   return 1;
}

void
CleanupQsort(void)
{
    free(buffer);
    free(buffer2);
}

unsigned int
DoBcopy256(void)
{
#define SIZE 256
    volatile char buf1[SIZE], buf2[SIZE];
    int i;

    for(i=100;i>0;i--) {
       bcopy((void*)buf1, (void*)buf2, SIZE);
       bcopy((void*)buf2, (void*)buf1, SIZE);
       bcopy((void*)buf1, (void*)buf2, SIZE);
       bcopy((void*)buf2, (void*)buf1, SIZE);
    }
    return 400;
#undef SIZE
}

unsigned int
DoBcopy16K(void)
{
#define SIZE 16*1024
    volatile char buf1[SIZE], buf2[SIZE];
    int i;

    for(i=10;i>0;i--) {
       bcopy((void*)buf1, (void*)buf2, SIZE);
       bcopy((void*)buf2, (void*)buf1, SIZE);
       bcopy((void*)buf1, (void*)buf2, SIZE);
       bcopy((void*)buf2, (void*)buf1, SIZE);
    }
    return 40;
#undef SIZE
}

unsigned int
DoBcopy100K(void)
{
#define SIZE 100*1024
    volatile char buf1[SIZE], buf2[SIZE];

    bcopy((void*)buf1, (void*)buf2, SIZE);
    bcopy((void*)buf2, (void*)buf1, SIZE);
    bcopy((void*)buf1, (void*)buf2, SIZE);
    bcopy((void*)buf2, (void*)buf1, SIZE);
    return 4;
#undef SIZE
}

unsigned int
DoBcopyMeg(void)
{
#define SIZE 1024*1024
    volatile char buf1[SIZE], buf2[SIZE];

    bcopy((void*)buf1, (void*)buf2, SIZE);
    bcopy((void*)buf2, (void*)buf1, SIZE);
    bcopy((void*)buf1, (void*)buf2, SIZE);
    bcopy((void*)buf2, (void*)buf1, SIZE);
    return 4;
#undef SIZE
}

unsigned int
DoBcopy10Meg(void)
{
#define SIZE 10*1024*1024
    volatile char buf1[SIZE], buf2[SIZE];

    bcopy((void*)buf1, (void*)buf2, SIZE);
    bcopy((void*)buf2, (void*)buf1, SIZE);
    bcopy((void*)buf1, (void*)buf2, SIZE);
    bcopy((void*)buf2, (void*)buf1, SIZE);
    return 4;
#undef SIZE
}

unsigned int
DoBcopy10Mega(void)
{
#define SIZE 10*1024*1024
    volatile char buf11[SIZE+128], buf22[SIZE+128], *buf1, *buf2;

    buf1 = (char *)((long)(buf11 + 127) &~127);
    buf2 = (char *)((long)(buf22 + 127) &~127);
    bcopy((void*)buf1, (void*)buf2, SIZE);
    bcopy((void*)buf2, (void*)buf1, SIZE);
    bcopy((void*)buf1, (void*)buf2, SIZE);
    bcopy((void*)buf2, (void*)buf1, SIZE);
    return 4;
#undef SIZE
}

unsigned int
DoBcopy10Megu(void)
{
#define SIZE 10*1024*1024
    volatile char buf11[SIZE+128], buf22[SIZE+128], *buf1, *buf2;

    buf1 = (char *)(((long)(buf11 + 127) &~127) + 1);
    buf2 = (char *)(((long)(buf22 + 127) &~127) + 7);;
    bcopy((void*)buf1, (void*)buf2, SIZE);
    bcopy((void*)buf2, (void*)buf1, SIZE);
    bcopy((void*)buf1, (void*)buf2, SIZE);
    bcopy((void*)buf2, (void*)buf1, SIZE);
    return 4;
#undef SIZE
}

unsigned int
DoBcopy10Megudw(void)
{
#define SIZE 10*1024*1024
    volatile char buf11[SIZE+128], buf22[SIZE+128], *buf1, *buf2;

    buf1 = (char *)(((long)(buf11 + 127) &~127) + 0);
    buf2 = (char *)(((long)(buf22 + 127) &~127) + 4);;
    bcopy((void*)buf1, (void*)buf2, SIZE);
    bcopy((void*)buf2, (void*)buf1, SIZE);
    bcopy((void*)buf1, (void*)buf2, SIZE);
    bcopy((void*)buf2, (void*)buf1, SIZE);
    return 4;
#undef SIZE
}

unsigned int
DoBzero256(void)
{
#define SIZE 256
    volatile char buf1[SIZE], buf2[SIZE];
    int i;

    for(i=100;i>0;i--) {
       bzero((void*)buf1, SIZE);
       bzero((void*)buf2, SIZE);
       bzero((void*)buf1, SIZE);
       bzero((void*)buf2, SIZE);
    }
    return 400;
#undef SIZE
}

unsigned int
DoBzero16K(void)
{
#define SIZE 16*1024
    volatile char buf1[SIZE], buf2[SIZE];
    int i;

    for(i=10;i>0;i--) {
       bzero((void*)buf1, SIZE);
       bzero((void*)buf2, SIZE);
       bzero((void*)buf1, SIZE);
       bzero((void*)buf2, SIZE);
    }
    return 40;
#undef SIZE
}

unsigned int
DoBzero100K(void)
{
#define SIZE 100*1024
    volatile char buf1[SIZE], buf2[SIZE];

    bzero((void*)buf1, SIZE);
    bzero((void*)buf2, SIZE);
    bzero((void*)buf1, SIZE);
    bzero((void*)buf2, SIZE);
    return 4;
#undef SIZE
}

unsigned int
DoBzeroMeg(void)
{
#define SIZE 1024*1024
    volatile char buf1[SIZE], buf2[SIZE];

    bzero((void*)buf1, SIZE);
    bzero((void*)buf2, SIZE);
    bzero((void*)buf1, SIZE);
    bzero((void*)buf2, SIZE);
    return 4;
#undef SIZE
}

unsigned int
DoBzero10Meg(void)
{
#define SIZE 10*1024*1024
    volatile char buf1[SIZE], buf2[SIZE];

    bzero((void*)buf1, SIZE);
    bzero((void*)buf2, SIZE);
    bzero((void*)buf1, SIZE);
    bzero((void*)buf2, SIZE);
    return 4;
#undef SIZE
}

unsigned int
DoBzero10Mega(void)
{
#define SIZE 10*1024*1024
    volatile char buf11[SIZE+128], buf22[SIZE+128], *buf1, *buf2;

    buf1 = (char *)((long)(buf11 + 127) &~127);
    buf2 = (char *)((long)(buf22 + 127) &~127);
    bzero((void*)buf1, SIZE);
    bzero((void*)buf2, SIZE);
    bzero((void*)buf1, SIZE);
    bzero((void*)buf2, SIZE);
    return 4;
#undef SIZE
}

unsigned int
DoBzero10Megu(void)
{
#define SIZE 10*1024*1024
    volatile char buf11[SIZE+128], buf22[SIZE+128], *buf1, *buf2;

    buf1 = (char *)(((long)(buf11 + 127) &~127) + 1);
    buf2 = (char *)(((long)(buf22 + 127) &~127) + 7);;
    bzero((void*)buf1, SIZE);
    bzero((void*)buf2, SIZE);
    bzero((void*)buf1, SIZE);
    bzero((void*)buf2, SIZE);
    return 4;
#undef SIZE
}

unsigned int
DoBzero10Megudw(void)
{
#define SIZE 10*1024*1024
    volatile char buf11[SIZE+128], buf22[SIZE+128], *buf1, *buf2;

    buf1 = (char *)(((long)(buf11 + 127) &~127) + 0);
    buf2 = (char *)(((long)(buf22 + 127) &~127) + 4);;
    bzero((void*)buf1, SIZE);
    bzero((void*)buf2, SIZE);
    bzero((void*)buf1, SIZE);
    bzero((void*)buf2, SIZE);
    return 4;
#undef SIZE
}

static void
recurse(unsigned int depth)
{
    if(depth > 1) {
        recurse(depth-1);
    } else {
        return;
    }
}

unsigned int
DoDeepRecursion(void)
{
    int i;

    for(i=500;i>0;i--) {
        recurse(250);
    }
    return 500;
}

/*
 * Compute r = a * b, where r can equal b.
 */
static void
matrixMultFloat(FloatMatrixPtr r, FloatMatrixPtr a, FloatMatrixPtr b)
{
    float b00, b01, b02, b03;
    float b10, b11, b12, b13;
    float b20, b21, b22, b23;
    float b30, b31, b32, b33;
    int i;

    b00 = b->matrix[0][0]; b01 = b->matrix[0][1];
        b02 = b->matrix[0][2]; b03 = b->matrix[0][3];
    b10 = b->matrix[1][0]; b11 = b->matrix[1][1];
        b12 = b->matrix[1][2]; b13 = b->matrix[1][3];
    b20 = b->matrix[2][0]; b21 = b->matrix[2][1];
        b22 = b->matrix[2][2]; b23 = b->matrix[2][3];
    b30 = b->matrix[3][0]; b31 = b->matrix[3][1];
        b32 = b->matrix[3][2]; b33 = b->matrix[3][3];

    for(i=0;i<4;i++) {
        r->matrix[i][0] = a->matrix[i][0]*b00 + a->matrix[i][1]*b10
            + a->matrix[i][2]*b20 + a->matrix[i][3]*b30;
        r->matrix[i][1] = a->matrix[i][0]*b01 + a->matrix[i][1]*b11
            + a->matrix[i][2]*b21 + a->matrix[i][3]*b31;
        r->matrix[i][2] = a->matrix[i][0]*b02 + a->matrix[i][1]*b12
            + a->matrix[i][2]*b22 + a->matrix[i][3]*b32;
        r->matrix[i][3] = a->matrix[i][0]*b03 + a->matrix[i][1]*b13
            + a->matrix[i][2]*b23 + a->matrix[i][3]*b33;
    }
}

/*
 * Compute r = a * b, where r can equal b.
 */
static void
matrixMultDouble(DoubleMatrixPtr r, DoubleMatrixPtr a, DoubleMatrixPtr b)
{
    double b00, b01, b02, b03;
    double b10, b11, b12, b13;
    double b20, b21, b22, b23;
    double b30, b31, b32, b33;
    int i;

    b00 = b->matrix[0][0]; b01 = b->matrix[0][1];
        b02 = b->matrix[0][2]; b03 = b->matrix[0][3];
    b10 = b->matrix[1][0]; b11 = b->matrix[1][1];
        b12 = b->matrix[1][2]; b13 = b->matrix[1][3];
    b20 = b->matrix[2][0]; b21 = b->matrix[2][1];
        b22 = b->matrix[2][2]; b23 = b->matrix[2][3];
    b30 = b->matrix[3][0]; b31 = b->matrix[3][1];
        b32 = b->matrix[3][2]; b33 = b->matrix[3][3];

    for(i=0;i<4;i++) {
        r->matrix[i][0] = a->matrix[i][0]*b00 + a->matrix[i][1]*b10
            + a->matrix[i][2]*b20 + a->matrix[i][3]*b30;
        r->matrix[i][1] = a->matrix[i][0]*b01 + a->matrix[i][1]*b11
            + a->matrix[i][2]*b21 + a->matrix[i][3]*b31;
        r->matrix[i][2] = a->matrix[i][0]*b02 + a->matrix[i][1]*b12
            + a->matrix[i][2]*b22 + a->matrix[i][3]*b32;
        r->matrix[i][3] = a->matrix[i][0]*b03 + a->matrix[i][1]*b13
            + a->matrix[i][2]*b23 + a->matrix[i][3]*b33;
    }
}

unsigned int
DoMatrixMultFloat(void)
{
    static FloatMatrix A = { { { 0.1, 0.1, 0.1, -0.4 },
                               { -0.3, 0.0, 0.2, 0.6 },
                               { 0.3, -0.1, 0.2, 1.0 },
                               { 0.1, 0.4, 0.8, 0.2 } } };
    static FloatMatrix B = { { { 0.1, 0.1, 0.6, 1.0 },
                               { 0.7, -0.8, 1.0, 0.1 },
                               { 0.5, -0.1, 0.4, 0.0 },
                               { -0.6, 0.6, 0.2, 0.2 } } };
    FloatMatrix a, b;
    int i;

    a = A;
    b = B;
    for(i=3;i>0;i--) {
        matrixMultFloat(&a, &b, &a);
        matrixMultFloat(&b, &a, &b);
        matrixMultFloat(&a, &A, &a);
        matrixMultFloat(&b, &a, &B);
    }
    return 12;
}

unsigned int
DoMatrixMultDouble(void)
{
    static DoubleMatrix A = { { { 0.1, 0.1, 0.1, -0.4 },
                                { -0.3, 0.0, 0.2, 0.6 },
                                { 0.3, -0.1, 0.2, 1.0 },
                                { 0.1, 0.4, 0.8, 0.2 } } };
    static DoubleMatrix B = { { { 0.1, 0.1, 0.6, 1.0 },
                                { 0.7, -0.8, 1.0, 0.1 },
                                { 0.5, -0.1, 0.4, 0.0 },
                                { -0.6, 0.6, 0.2, 0.2 } } };
    DoubleMatrix a, b;
    int i;

    a = A;
    b = B;
    for(i=3;i>0;i--) {
        matrixMultDouble(&a, &b, &a);
        matrixMultDouble(&b, &a, &b);
        matrixMultDouble(&a, &A, &a);
        matrixMultDouble(&b, &a, &B);
    }
    return 12;
}

double dresult[100]; /* purposefully not static */

unsigned int
DoTrigFuncsDouble(void)
{
    double a, b, c, d, e, f, g, h;
    int i;

    /*
     * eight cos calls
     * eight sin calls
     * nine tan calls
     */
    for(i=100;i>0;i--) {
       a = 3.0 + i;
       b = 4.3232 - i;
       c = -45.3490 + i;
       d = 0 + i;
       d = cos(d) + sin(a);
       e = tan(b);
       f = sin(d) + sin(e);
       g = sin(c);
       h = cos(a) + cos(b + 23);
       d = tan(h) + tan(d + f) + cos(g + f);
       a += tan(h + c) + sin(b);
       b += g + tan(d) + cos(c + 1);
       c -= sin(d) - tan(g) + sin(h + 4.5);
       a = sin(a) + cos(b) + tan(c) + cos(e);
       dresult[i] = tan(a + 5) + cos(d) + tan(e + g);
    }
    return 2500;
}

float fresult[100]; /* purposefully not static */

unsigned int
DoTrigFuncsFloat(void)
{
    float a, b, c, d, e, f, g, h;
    int i;

    /*
     * eight cosf calls
     * eight sinf calls
     * nine tanf calls
     */
    for(i=100;i>0;i--) {
       a = 3.0 + i;
       b = 4.3232 - i;
       c = -45.3490 + i;
       d = 0 + i;
       d = cosf(d) + sinf(a);
       e = tanf(b);
       f = sinf(d) + sinf(e);
       g = sinf(c);
       h = cosf(a) + cosf(b + 23);
       d = tanf(h) + tanf(d + f) + cosf(g + f);
       a += tanf(h + c) + sinf(b);
       b += g + tanf(d) + cosf(c + 1);
       c -= sinf(d) - tanf(g) + sinf(h + 4.5);
       a = sinf(a) + cosf(b) + tanf(c) + cosf(e);
       fresult[i] = tanf(a + 5) + cosf(d) + tanf(e + g);
    }
    return 2500;
}

#define other(i,j) (6-(i+j))

static int num[4];

static void
mov(n,f,t)
{
    int o;

    if(n == 1) {
        num[f]--;
        num[t]++;
        return;
    }
    o = other(f,t);
    mov(n-1,f,o);
    mov(1,f,t);
    mov(n-1,o,t);
    return;
}

unsigned int
DoHanoi10(void)
{
  int disk = 10; /* default number of disks */
  int i;

  for(i=100;i>0;i--) {
    mov(disk,1,3);
    mov(disk,1,3);
    mov(disk,1,3);
  }
  return 300;
}

unsigned int
DoHanoi15(void)
{
  int disk = 15; /* default number of disks */
  int i;

  for(i=100;i>0;i--) {
    mov(disk,1,3);
    mov(disk,1,3);
    mov(disk,1,3);
  }
  return 300;
}

