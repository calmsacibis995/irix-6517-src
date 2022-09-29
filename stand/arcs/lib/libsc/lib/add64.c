/* 64 bit add.
*/

#ident "$Revision: 1.3 $"

#ifndef ADD64_TEST
#include <arcs/types.h>
#else
typedef long LONG;
typedef unsigned long ULONG;
#endif

#define pos(x)	((x)>=0)

void
add64(	LONG ah, ULONG al,
	LONG bh, ULONG bl,
	LONG *rh, ULONG *rl)
{
	int cary;
	int sign=0;

	switch((pos(ah)<<4)|pos(bh)) {
	case 0x00:	/* neg(a), neg(b) */
		ah = ~ah; al = ~al;		/* 2's compliment */
		add64(ah,al,0,1,rh,rl);
		ah = *rh; al = *rl;

		bh = ~bh; bl = ~bl;		/* 2's compliment */
		add64(bh,bl,0,1,rh,rl);
		bh = *rh; bl = *rl;

		add64(ah,al,bh,bl,rh,rl);	/* add them */
		ah = *rh; al = *rl;

		ah = ~ah; al = ~al;
		add64(ah,al,0,1,rh,rl);	/* 2's compliment back */
		break;

	case 0x01:	/* neg(a), pos(b) */
		add64(bh,bl,ah,al,rh,rl);	/* switch to case 0x10 */
		break;

	case 0x10:	/* pos(a), neg(b) */
		bh = ~bh; bl = ~bl;		/* 2's compliment */
		add64(bh,bl,0,1,rh,rl);
		bh = *rh; bl = *rl;		/* now pos - pos */

		if (al >= bl) {
			/* no borrowing -- easy! */
			*rl = al - bl;
		}
		else {
			/* try to borrow */
			*rl = bl - al;		/* delta */
			if (ah) {
				ah--;			/* borrow */
				*rl = (unsigned long)0xffffffff - *rl + 1;
			}
			else
				sign = 1;
		}

		*rh = ah - bh;

		if (sign) {
			if (*rh >= 0)		/* neg whole range */
				*rh = ~*rh;
			*rl = ~*rl;
			/* can't recurse here, but handle +1 on negatives */
			if (!++(*rl))
				*rh = 0;
		}
		break;

	case 0x11:	/* pos(a), pos(b) */
		/* Add with masking possible carry, and handle carry
		 * ourselves.
		 */
		*rl = (al&0x7fffffff) + (bl&0x7fffffff);
		*rh = ah + bh;
		cary = ((al&0x80000000)>31) + ((bl&0x80000000)>31) +
			((*rl&0x80000000)>31);
		*rl &= 0x7fffffff;
		*rl |= ((cary&0x1)<<31);
		*rh += ((cary&0x2)>>1);
		break;
	}
	return;
}

#ifdef ADD64_TEST
void
test(	LONG ah, ULONG al,
	LONG bh, ULONG bl,
	LONG rh, ULONG rl)
{
	LONG rrh;
	ULONG rrl;

	add64(ah,al,bh,bl,&rrh,&rrl);

	if (rrh != rh || rrl != rl) {
		printf("FAILED: %x %x + %x %x = %x %x not %x %x\n",
			ah, al, bh, bl, rrh, rrl, rh, rl);
		exit(1);
	}
}
#define tester main		/* fool cvstatic */
tester()
{
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

	exit(0);
}
#endif
