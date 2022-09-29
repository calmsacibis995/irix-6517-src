/*
 * New Bitmap routines
 *
 * $Source: /proj/irix6.5.7m/isms/irix/kern/sgi/RCS/bit.c,v $
 * $Revision: 3.9 $
 * $Date: 1996/09/10 20:53:20 $
 */
#include "sys/debug.h"
#include "sys/types.h"
#include "sys/atomic_ops.h"

/*
 * Define SLOWBIT if these routines are not coded in assembler somewhere
 * else.
 */
#define	SLOWBIT

#ifdef	SLOWBIT

#if (_MIPS_SZINT == 32)
#define BITSPERWORD	32
#define WORDMASK	31
#define BITOFWORD(x)	(1 << (x & WORDMASK))
#define BITSTOWORDS(x)	(x >> 5)
#endif
#if (_MIPS_SZINT == 64)
#define BITSPERWORD	64
#define WORDMASK	63
#define BITOFWORD(x)	(1 << (x & WORDMASK))
#define BITSTOWORDS(x)	(x >> 6)
#endif

/*
 * Set bit b in bitmap bp
 */
void
bset(char *bp, bitnum_t b)
{
	/* insure that bitmap pointer is on a int boundary */
	ASSERT(((__psint_t)bp & 3) == 0);

	*((uint *)bp + BITSTOWORDS(b)) |= BITOFWORD(b);
}

/*
 * Clear bit b in bitmap bp
 */
void
bclr(char *bp, bitnum_t b)
{
	/* insure that bitmap pointer is on a int boundary */
	ASSERT(((__psint_t)bp & 3) == 0);

	*((uint *)bp + BITSTOWORDS(b)) &= ~BITOFWORD(b);
}

/*
 * Test bit b in bitmap bp
 */
int
btst(char *bp, bitnum_t b)
{
	/* insure that bitmap pointer is on a int boundary */
	ASSERT(((__psint_t)bp & 3) == 0);

	return (*((uint *)bp + BITSTOWORDS(b)) & BITOFWORD(b));
}

/*
 * Set a bit field of length len in bitmap bp starting at b
 */
void
bfset(char *bp, register bitnum_t b, register bitlen_t len)
{
	register uint mask, i, j, *wp;

	/* insure that bitmap pointer is on a int boundary */
	ASSERT(((__psint_t)bp & 3) == 0);

	wp = (uint *)bp + BITSTOWORDS(b);
	while (len) {
		i = b & WORDMASK;
		mask = -1 << i;
		if (len < (j = BITSPERWORD - i)) {
			mask &= mask >> (j - len);
			*wp |= mask;
			return;
		}
		else {
			len -= (BITSPERWORD - i);
			*wp |= mask;
			wp++;
			b = 0;
		}
	}
}

/*
 * Clear a bit field of length len in bitmap bp starting at b
 */
void
bfclr(register char *bp, register bitnum_t b, register bitlen_t len)
{
	register uint mask, i, j, *wp;

	/* insure that bitmap pointer is on a int boundary */
	ASSERT(((__psint_t)bp & 3) == 0);

	wp = (uint *)bp + BITSTOWORDS(b);
	while (len) {
		i = b & WORDMASK;
		mask = -1 << i;
		if (len < (j = BITSPERWORD - i)) {
			mask &= mask >> (j - len);
			*wp &= ~mask;
			return;
		}
		else {
			len -= (BITSPERWORD - i);
			*wp &= ~mask;
			wp++;
			b = 0;
		}
	}
}

/*
 * Test a bit field of length len in bitmap bp starting at b to be all set.
 * Return a count of the number of set bits found.
 */
bitlen_t
bftstset(register char *bp, register bitnum_t b, register bitlen_t len)
{
	register uint mask, count, w, *wp;
	register ushort i, n, flen, sflen;

	/* insure that bitmap pointer is on a int boundary */
	ASSERT(((__psint_t)bp & 3) == 0);

	wp = (uint *)bp + BITSTOWORDS(b);
	i = b & WORDMASK;

	for (count = 0, w = *wp >> i, flen = BITSPERWORD - i;
	     (len > 0) && (w & 1); w = *(++wp), flen = BITSPERWORD) {

		for (mask = -1, i = BITSPERWORD, sflen = flen, n = 0;
		     (len > 0) && (flen > 0) && (i > 0) && (w & 1);
		     i >>= 1, mask >>= i) {

			if ((len >= i) && (flen >= i) &&
			    ((w & mask) == mask)) {
				n += i;
				len -= i;
				flen -= i;
				w >>= i;
			}
		}
		count += n;
		if (n < sflen)
			break;
	}
	return (count);
}

/*
 * Test a bit field of length len in bitmap bp starting at b to be all clear.
 * Return a count of the number of clear bits found
 */
bitlen_t
bftstclr(register char *bp, register bitnum_t b, register bitlen_t len)
{
	register uint mask, count, w, *wp;
	register ushort i, n, flen, sflen;

	/* insure that bitmap pointer is on a int boundary */
	ASSERT(((__psint_t)bp & 3) == 0);

	wp = (uint *)bp + BITSTOWORDS(b);
	i = b & WORDMASK;

	for (count = 0, w = *wp >> i, flen = BITSPERWORD - i;
	     (len > 0) && !(w & 1); w = *(++wp), flen = BITSPERWORD) {

		for (mask = -1, i = BITSPERWORD, sflen = flen, n = 0;
		     (len > 0) && (flen > 0) && (i > 0) && !(w & 1);
		     i >>= 1, mask >>= i) {

			if ((len >= i) && (flen >= i) &&
			    ((w | ~mask) == ~mask)) {
					n += i;
					len -= i;
					flen -= i;
					w >>= i;
			}
		}
		count += n;
		if (n < sflen)
			break;
	}
	return (count);
}

/*
 * Counts the number of bits set in a bit field of length len starting
 * at bitnumber b. It uses the fact for machines which use 2's complement
 * arithmetic n &(n-1) clears one bit in the word n.
 */

bitlen_t
bfcount(register char *bp, register bitnum_t b, register bitlen_t len)
{
	register uint count, w, *wp;
	register uint i, n; 

	/* insure that bitmap pointer is on a int boundary */
	ASSERT(((__psint_t)bp & 3) == 0);

	wp = (uint *)bp + BITSTOWORDS(b);
	count = 0;
	while (len) {
		i = b & WORDMASK;
		w = *wp;
		w >>= i;
		if (len < (BITSPERWORD - i)) {
			w &= (((uint)-1) >> (BITSPERWORD -len));
			len = 0;
		}
		n = 0;
		while (w) {
			n++;
			w= (w & (w-1));
		}

		count += n;
		if (len) {
			len -= (BITSPERWORD - i);
			wp++;
			b = 0;
		} else break;
	}
	return count;
}

/*
 * byte-oriented bit operations:  used by fs/efs/efs_bitmap.c
 */

/*
 * Set bit b in bitmap bp
 */
void
bbset(char *bp, bitnum_t b)
{
	*(bp + (b >> 3)) |= 1 << (b & 7);
}

/*
 * Clear bit b in bitmap bp
 */
void
bbclr(char *bp, bitnum_t b)
{
	*(bp + (b >> 3)) &= ~(1 << (b & 7));
}

/*
 * Test bit b in bitmap bp
 */
int
bbtst(char *bp, bitnum_t b)
{
	return *(bp + (b >> 3)) & (1 << (b & 7));
}

/*
 * Set a bit field of length len in bitmap bp starting at b
 */
void
bbfset(register char *bp, register bitnum_t b, register bitlen_t len)
{
	while (len--) {
		*(bp + (b >> 3)) |= 1 << (b & 7);
		b++;
	}
}

/*
 * Clear a bit field of length len in bitmap bp starting at b
 */
void
bbfclr(register char *bp, register bitnum_t b, register bitlen_t len)
{
	while (len--) {
		*(bp + (b >> 3)) &= ~(1 << (b & 7));
		b++;
	}
}

/*
 * Test a bit field of length len in bitmap bp starting at b to be all set.
 * Return a count of the number of set bits found.
 */
bitlen_t
bbftstset(register char *bp, register bitnum_t b, register bitlen_t len)
{
	register bitlen_t count;

	/* insure that bitmap pointer is on a int boundary */
	ASSERT(((__psint_t)bp & 3) == 0);

	count = 0;
	while (len) {
	    if ((b & 7) == 0) {			/* byte aligned */
		if ((b & 15) == 0) {		/* short aligned */
		    if ((b & 31) == 0) {	/* int aligned */
			/* count as many ints as possible */
			while (len >= 32) {
			    if (*(uint *)(bp + (b >> 3)) == 0xFFFFFFFF) {
				b += 32;
				len -= 32;
				count += 32;
			    } else
				break;
			}
		    }
		    /* try to get bit number int aligned */
		    if ((len >= 16) &&
			(*(ushort *)(bp + (b >> 3)) == 0xFFFF)) {
			b += 16;
			len -= 16;
			count += 16;
			/* start loop at the top */
			continue;
		    }
		}
		/* try to get bit number short aligned */
		if ((len >= 8) &&
		    (*(unchar *)(bp + (b >> 3)) == 0xFF)) {
		    b += 8;
		    len -= 8;
		    count += 8;
		    /* start loop at the top */
		    continue;
		}
	    }
	    if (len == 0)
		break;

	    /* count one bit */
	    if ( ! (*(bp + (b >> 3)) & (1 << (b & 7))) )
		break;
	    b++;
	    len--;
	    count++;
	}
	return count;
}

/*
 * Test a bit field of length len in bitmap bp starting at b to be all clear.
 * Return a count of the number of clear bits found
 */
bitlen_t
bbftstclr(register char *bp, register bitnum_t b, register bitlen_t len)
{
	register bitlen_t count;

	/* insure that bitmap pointer is on a int boundary */
	ASSERT(((__psint_t)bp & 3) == 0);

	count = 0;
	while (len) {
	    if ((b & 7) == 0) {			/* byte aligned */
		if ((b & 15) == 0) {		/* short aligned */
		    if ((b & 31) == 0) {	/* int aligned */
			/* count as many ints as possible */
			while (len >= 32) {
			    if (*(uint *)(bp + (b >> 3)) == 0x00000000) {
				b += 32;
				len -= 32;
				count += 32;
			    } else
				break;
			}
		    }
		    /* try to get bit number int aligned */
		    if ((len >= 16) &&
			(*(ushort *)(bp + (b >> 3)) == 0x0000)) {
			b += 16;
			len -= 16;
			count += 16;
			/* start loop at the top */
			continue;
		    }
		}
		/* try to get bit number short aligned */
		if ((len >= 8) &&
		    (*(unchar *)(bp + (b >> 3)) == 0x00)) {
		    b += 8;
		    len -= 8;
		    count += 8;
		    /* start loop at the top */
		    continue;
		}
	    }
	    if (len == 0)
		break;

	    /* count one bit */
	    if ( *(bp + (b >> 3)) & (1 << (b & 7)) )
		break;
	    b++;
	    len--;
	    count++;
	}
	return count;
}

/*
 * Set bit b in bitmap bp
 */
void
bset_atomic(unsigned int *bp, bitnum_t b)
{
	/* insure that bitmap pointer is on a int boundary */
	ASSERT(((__psint_t)bp & 3) == 0);

	atomicSetInt(((int *)bp + BITSTOWORDS(b)),BITOFWORD(b));
}

/*
 * Clear bit b in bitmap bp
 */
void
bclr_atomic(unsigned int *bp, bitnum_t b)
{
	/* insure that bitmap pointer is on a int boundary */
	ASSERT(((__psint_t)bp & 3) == 0);

	atomicClearInt(((int *)bp + BITSTOWORDS(b)),BITOFWORD(b));
}
#endif
