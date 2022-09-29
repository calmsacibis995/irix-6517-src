/*
 * large.c - Standalone support for 64 bit integers
 *
 * $Revision: 1.4 $
 */

#include <arcs/large.h>

/* 
 * Note: 64 bit signed adds return 1 on overflow 
 */

void longtolarge(long, LARGE *);
void ulongtolarge(unsigned long, LARGE *);

/*
 * addlarge - add two large ints, a and b, leaving sum in third
 */
int 
addlarge(register LARGE *a, register LARGE *b, register LARGE *s)
{
    unsigned long carryout;
    long ahi = a->hi;		/* make copies so input parameters */
    long bhi = b->hi;		/* can be overwritten by output */

    s->lo = b->lo + a->lo;
    carryout = (unsigned long)((s->lo < a->lo || s->lo < b->lo) ? 1 : 0);
    
    s->hi = (long)((unsigned long)a->hi + (unsigned long)b->hi + carryout);

    /* Check for signed overflow
     */
    if ((ahi < 0 && bhi < 0 && s->hi >= 0) || 
	    (ahi >= 0 && bhi >= 0 && s->hi < 0))
	return 1;

    return 0;
}

#if 0		/* not used currently! */
/*
 * addlongtolarge - add a long to a large int
 */
int 
addlongtolarge(long a, LARGE *b)
{
    LARGE la;

    longtolarge(a, &la);
    return addlarge(&la, b, b);
}
#endif

/*
 * addulongtolarge - add an unsigned long to a large int
 */
int 
addulongtolarge(unsigned long a, LARGE *b)
{
    LARGE la;

    ulongtolarge(a, &la);
    return addlarge(&la, b, b);
}

/*
 * longtolarge - convert long to a large int
 */
void 
longtolarge(long a, LARGE *b)
{
    if (a < 0)		/* sign extend */
	b->hi = -1;
    else
	b->hi = 0;
    b->lo = (unsigned long)a;
}

/*
 * ulongtolarge - convert unsigned long to a large int
 */
void 
ulongtolarge(unsigned long a, LARGE *b)
{
    b->hi = 0;
    b->lo = a;
}

#ifdef ADD64_TEST
/* test 64 bit adds
 */
typedef long LONG;
typedef unsigned long ULONG;

void
test(	LONG ah, ULONG al,
	LONG bh, ULONG bl,
	LONG rh, ULONG rl)
{
	LARGE a, b, r;

	a.hi = ah;
	a.lo = al;
	b.hi = bh;
	b.lo = bl;
	if (addlarge(&a,&b,&r)) {
		printf ("64 bit signed overflow detected\n");
		return;
	}

	if (r.hi != rh || r.lo != rl) {
		printf("FAILED: %x %x + %x %x = %x %x not %x %x\n",
			ah, al, bh, bl, r.hi, r.lo, rh, rl);
		exit(1);
	}
}

#define tester main		/* fool cvstatic */
tester()
{
	LARGE lg;

	test(0,0, 0,0, 0,0);
	test(0,1, 0,0, 0,1);
	test(0,0, 0,1000, 0,1000);
	test(0,10, 0,10, 0,20);

	test(90,0, 0,0, 90,0);
	test(45,0, 45,0, 90,0);

	test(1,1, 1,1, 2,2);
	test(5,5, 1,2, 6,7);

	test(0,0x80000000, 0,0x80000000, 1,0);
	test(0,0x80000000, 0,0x7fffffff, 0,0xffffffff);
	test(1,0x80000000, 0,0x7fffffff, 1,0xffffffff);
	test(0,0x0fffffff, 0,0x0fffffff, 0,0x1ffffffe);

	test(-1,-1, -1,-1, -1,-2);
	test(-1,-100, -1,-9, -1,-109);

	test(0,1, -1,-1, 0,0);
	test(-1,-1, 0,1, 0,0);
	test(-1,-5, 0,5, 0,0);
	test(10,1, -1,-1, 10,0);
	test(10,0, -1,-1, 9,0xffffffff);

	test(0,10, -1,-20, -1,-10);
	test(0,0xffffffff, -1,-1, 0,0xfffffffe);

	test(1000,52, -1000,-60, 0,-8);

	test(24,1, 3,-1, 28,0);

	/* this should generate overflow
	 */
	printf ("Following test should fail with overflow.\n");
	test(0x7fffffff,0xffffffff, 0,1, 0,0);
	printf ("Overflow test complete.\n");

	longtolarge (0xffffffff, &lg);
	if (lg.hi != -1 || lg.lo != 0xffffffff)
		printf ("Error longtolarge (-1)\n");

	ulongtolarge (0xffffffff, &lg);
	if (lg.hi != 0 || lg.lo != 0xffffffff)
		printf ("Error ulongtolarge (0xffffffff)\n");

	lg.hi = -1;
	lg.lo = 0xffffffff;
	if (addlongtolarge(1, &lg))
		printf ("Unexpected overflow in addlongtolarge\n");
	if (lg.hi != 0 || lg.lo != 0)
		printf ("Error addlongtolarge (0)\n");

	lg.hi = (long)0x80000000;
	lg.lo = 0;
	if (!addlongtolarge(-1, &lg))
		printf ("Overflow expected but not found in addlongtolarge\n");

	exit(0);
}
#endif
