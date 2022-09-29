/*
 * Bitmap routines
 */
#ident "$Revision: 1.3 $"

#ifdef __STDC__
	#pragma weak bset = _bset
	#pragma weak bclr = _bclr
	#pragma weak btst = _btst
	#pragma weak bfset = _bfset
	#pragma weak bfclr = _bfclr
	#pragma weak bftstset = _bftstset
	#pragma weak bftstclr = _bftstclr
#endif
#include "synonyms.h"
#include "unistd.h"

/*
 * Set bit b in bitmap bp
 */
void
bset(char *bp, bitnum_t b)
{
	*(bp + (b >> 3)) |= 1 << (b & 7);
}

/*
 * Clear bit b in bitmap bp
 */
void
bclr(char *bp, bitnum_t b)
{
	*(bp + (b >> 3)) &= ~(1 << (b & 7));
}

/*
 * Test bit b in bitmap bp
 */
int
btst(char *bp, bitnum_t b)
{
	return *(bp + (b >> 3)) & (1 << (b & 7));
}

/*
 * Set a bit field of length len in bitmap bp starting at b
 */
void
bfset(register char *bp, bitnum_t b, bitlen_t len)
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
bfclr(register char *bp, bitnum_t b, bitlen_t len)
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
bftstset(char *bp, bitnum_t b, bitlen_t len)
{
	register bitlen_t count;
	register int dolong, doshort;

	count = 0;
	if (((__psunsigned_t)bp & 0x1) == 0)
		doshort = 1;
	if (((__psunsigned_t)bp & 0x2) == 0)
		dolong = 1;
	while (len) {
	    if ((b & 7) == 0) {			/* byte aligned */
		if (doshort && (b & 15) == 0) {		/* short aligned */
		    if (dolong && (b & 31) == 0) {	/* 32bit aligned */
			/* count as many 32bit's as possible */
			while (len >= 32) {
			    if (*(__uint32_t *)(bp + (b >> 3)) == 0xFFFFFFFF) {
				b += 32;
				len -= 32;
				count += 32;
			    } else
				break;
			}
		    }
		    /* try to get bit number long aligned */
		    if ((len >= 16) &&
			(*(unsigned short *)(bp + (b >> 3)) == 0xFFFF)) {
			b += 16;
			len -= 16;
			count += 16;
			/* start loop at the top */
			continue;
		    }
		}
		/* try to get bit number short aligned */
		if ((len >= 8) &&
		    (*(unsigned char *)(bp + (b >> 3)) == 0xFF)) {
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
	return (count);
}

/*
 * Test a bit field of length len in bitmap bp starting at b to be all clear.
 * Return a count of the number of clear bits found
 */
bitlen_t
bftstclr(char *bp, bitnum_t b, bitlen_t len)
{
	register bitlen_t count;
	register int dolong, doshort;

	if (((__psunsigned_t)bp & 0x1) == 0)
		doshort = 1;
	if (((__psunsigned_t)bp & 0x2) == 0)
		dolong = 1;

	count = 0;
	while (len) {
	    if ((b & 7) == 0) {			/* byte aligned */
		if (doshort && (b & 15) == 0) {		/* short aligned */
		    if (dolong && (b & 31) == 0) {	/* 32bit aligned */
			/* count as many 32bit's as possible */
			while (len >= 32) {
			    if (*(__uint32_t *)(bp + (b >> 3)) == 0x00000000) {
				b += 32;
				len -= 32;
				count += 32;
			    } else
				break;
			}
		    }
		    /* try to get bit number long aligned */
		    if ((len >= 16) &&
			(*(unsigned short *)(bp + (b >> 3)) == 0x0000)) {
			b += 16;
			len -= 16;
			count += 16;
			/* start loop at the top */
			continue;
		    }
		}
		/* try to get bit number short aligned */
		if ((len >= 8) &&
		    (*(unsigned char *)(bp + (b >> 3)) == 0x00)) {
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
	return (count);
}
