/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1993 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*
 * Description: Convert a quad precision binary number to decimal.
 */
#ident "$Id: qtoa.c,v 1.8 1994/10/04 19:39:11 jwag Exp $"

#define _IEEE 1
#include "fpparts.h"
#undef 	_IEEE
#include "math_extern.h"

#define min(x,y) ((x)<(y)? (x): (y))
#define max(x,y) ((x)>(y)? (x): (y))

#if QUAD_DEBUG
#include <stdio.h>
#endif

/* ====================================================================
 *
 * FunctionName: _qtoa
 *
 * Description: Convert a quad precision binary number to decimal.
 *
 * ====================================================================
 */

int				/* returns decimal exponent */
_qtoa(
char *buffer,		/* Destination for ascii string */ 
int ndigit,		/* number of digits to convert.
			 * 1 <= ndigit <= 34
			 * Log2(1e34) = 112.9 bits, so the
 			 * 107 significant bits can be con-
 			 * verted to decimal with room left
 			 * over.
			 */ 
long double x,		/* number to convert to ascii */ 
int fflag)		/* 0 for e format, 1 for f format */ 
{
_ldblval value;		/* the value manipulated */ 
#define HI (value.fwords.hi)	/* top 64b of 128b fraction */ 
#define LO (value.fwords.lo)	/* bottom 64b of 128b fraction */

int signhi, signlo;		/* sign of HI = sign of number */ 
int exphi, explo;		/* exponents of HI and LO */ 
int dexp;			/* decimal exponent */ 
int bexp;			/* binary exponent */ 
unsigned int lohi;		/* top 4 bits of lo */ 
unsigned long long lolo;	/* bottom 60 bits of lo */ 
int nd;				/* number of digits output */ 
int guard;			/* guard digit */

#if QUAD_DEBUG 
char *buffer0 = buffer;		/* initial value of buffer */
#endif

	/* note that ndigit could be zero if fflag == 1;
	 * this is adjusted later.
	 */

	ndigit = min(ndigit, 34);
	ndigit = max(ndigit, 0);

	HI = *((unsigned long long *) &x);
	LO = *(((unsigned long long *) &x) + 1);

#if QUAD_DEBUG 
	printf("initially: HI=0x%016llx\n",HI); 
	printf("LO=0x%016llx\n",LO); 
#endif

	signhi = SIGNBIT(HI); 
	HI = ((HI << 1) >> 1);		/* discard sign bit */ 
	*buffer++ = signhi ? '-' : ' ';

#if QUAD_DEBUG 
	printf("after de-signing hi: HI=0x%016llx\n",HI); 
	printf("signhi=%d\n",signhi); 
  	*buffer = '\0'; 
	printf(" buffer=%s\n",buffer0);
#endif

	if ( HI == 0 )
	{		/* true zero */ 
	char *b;	/* fencepost for buffer */

		 b = buffer + ndigit; 

		 while ( buffer < b ) 
			*buffer++ = '0';

		 *buffer++ = '\0';	/* null terminate buffer */

#if QUAD_DEBUG 
	printf("HI was zero: HI=0x%016llx\n",HI); 
	printf("signhi=%d\n",signhi); 
	*buffer = '\0'; 
	printf(" buffer=%s\n",buffer0);
#endif

		return(0);
	}

	exphi = EXPONENT(HI); 
	EXPONENT(HI) = 0;

#if QUAD_DEBUG 
	printf("HI non-zero: HI=0x%016llx\n",HI); 
	printf("exphi=%d\n",exphi); 
#endif

	if ( exphi == 0x7ff )
	{		/* infinity or NaN */ 
	char *s;	/* infinity or nan string */ 
	char *infinity = "inf"; 
	char *nan = "nan";

		s = ( HI == 0 ) ? infinity : nan;

		do 
			*buffer++ = *s; 
		while ( *s++ );

#if QUAD_DEBUG 
	printf("NaN or Infinity: buffer=%s\n",buffer0); 
#endif

		return(0); 
	}

	/* processing a number. create 128b fraction */ 
	/* hi is non-zero, or the following while loop wouldn't terminate */ 

	if ( exphi == 0 )
	{	/* HI denorm */ 

		exphi++;

		while ( !(HI & (1ULL << 52)) )
		{
			/* normalize - need not be fast */ 

			HI <<=1; 
			exphi--;
#if QUAD_DEBUG 
	printf("HI denorm: HI=0x%016llx\n",HI); 
	printf("exphi=%d\n",exphi); 
#endif

		}
	} 
	else
	{	/* HI is normal number */ 

		HI |= (1ULL << 52);		/* expose hidden bit */

#if QUAD_DEBUG 
	printf("HI normal: HI=0x%016llx\n",HI); 
	printf("exphi=%d\n",exphi); 
#endif

	}

	/* handle LO */

	signlo = SIGNBIT(LO) ^ signhi; /* HI,LO signs differ? */
	SIGNBIT(LO) = 0;	/* discard sign */

#if QUAD_DEBUG
	printf("handle LO: LO=0x%016llx\n",LO);
	printf("           signlo=%d\n",signlo);
#endif

	if ( LO )
	{	/* lo is non-zero */
	int	shift_count;

		explo = EXPONENT(LO);
		EXPONENT(LO) = 0;

#if QUAD_DEBUG
	printf("non-zero LO: LO=0x%016llx\n",LO);
	printf("             explo=%d\n",explo);
#endif

		if ( explo == 0 )
		{	/* LO denorm */ 

			explo++;

			while ( !(LO & (1ULL << 52)) )
			{
				/* normalize - need not be fast */ 

				LO <<= 1; 
				explo--;
#if QUAD_DEBUG 
	printf("LO denorm: LO=0x%016llx\n",LO); 
	printf("explo=%d\n",explo); 
#endif

			}
		} 
		else
		{		/* LO normal number */

			LO |= (1ULL << 52);	/* expose hidden bit */
      
#if QUAD_DEBUG
      printf("expose hidden bit: LO=0x%016llx\n",LO);
#endif
		}

		LO <<= 11;	/* move msb of LO to bit 63 */

		/* LO should be no more than 1/2 ulp of HI, so calculate
		 * the shift count and adjust in case of error.
		 */

		shift_count = (exphi - explo) - 53;

		if ( shift_count < 0 )
		{
			LO = 0;
			goto combine;
		}

		shift_count = min(shift_count, 55);	/* HI + LO should have
							 * no more than 107 bits
							 */

		LO >>= shift_count;	/* line up fractional
					 * bits of LO as
					 * appropriate
					 */

#if QUAD_DEBUG
	printf("normalize LO to HI: LO=0x%016llx\n",LO);
#endif

		if ( signlo )
		{		/* LO is negative */

			LO = (~LO) + 1ULL;
			HI -= 1ULL;

#if QUAD_DEBUG
	printf("LO negative: HI=0x%016llx\n",HI);
	printf("             LO=0x%016llx\n",LO);
#endif

		}

	}

combine:

	HI = ((HI << 11) | (LO >> 53));
	LO <<= 11;

	/* we may have to shift one more bit, depending
	 * on the sizes of HI and LO
	 */

	if ( (HI >> 63) == 0ULL )
	{
		HI = ((HI << 1) | (LO >> 63));
		LO <<= 1;
		exphi--;
	}

	/* mask the low 21 bits of LO, in case of error;
	 * HI + LO should have at most 107 bits
	 */

	LO &= ~0x1fffffULL;

#if QUAD_DEBUG
	printf("LO&HI combined: HI=0x%016llx\n",HI);
	printf("                LO=0x%016llx\n",LO);
#endif

	/* At this point we have a 128b binary fraction HILO, the low 21
	 * bits of which are zero, and a biased binary exponent exphi.
	 *
	 * Generate the decimal exponent by multiplying the binary 
	 * exponent by log10(2). 
	 */

	/* exphi is at most 9 bits plus sign. Shifts avoid overflow
	 * and recover integer part.
	 */

	dexp = (int)( ((long)(exphi-1023))*(0x4D104D42L >> 21) );

	/* truncate to -infinity */

	dexp >>= 11;
	dexp++;

#if QUAD_DEBUG
	printf("before scaling: dexp=%d\n",dexp);
#endif

	/* Scale fraction by decimal exponent. */

	if ( dexp )	/* value *= 10 ** -dexp */
		__qtenscale(&value, -dexp, &bexp); 
	else
		bexp = 0;

#if QUAD_DEBUG
	printf("after scaling: HI=0x%016llx\n",HI);
	printf("               LO=0x%016llx\n",LO);
	printf("               bexp=%d\n",bexp);
#endif

	exphi += bexp;

#if QUAD_DEBUG
	printf("adjusted exphi: exphi=%d\n",exphi);
#endif

	/* unbias exphi */

	exphi -= 1022;

	/* Remove binary exponent by denormalizing */
	/* Shift an extra 4 bits to allow for later digit generation */

	LO = (LO >> (4 - exphi)) | (HI << (60 + exphi));
	HI >>= (4 - exphi);

#if QUAD_DEBUG
	printf("denormalized: HI=0x%016llx\n",HI);
	printf("              LO=0x%016llx\n",LO);
#endif

  /* Split LO inpreparation to producing digits */

	lohi = LO >> 60;
	lolo = LO & ((1ULL << 60) - 1);

#if QUAD_DEBUG
	printf("split LO: lohi=0x%02x\n",lohi);
	printf("          lolo=0x%016llx\n",lolo);
#endif

	/* find first non-zero digit */

	while ( !(HI >> 60) )
	{
		lolo *= 10;
		lohi = lohi * 10 + (lolo >> 60);
		lolo &= (1ULL << 60) - 1;
		HI = HI * 10 + (lohi >> 4);
		lohi &= 0xF;
		dexp--;

#if QUAD_DEBUG
	printf("finding first digit: HI=0x%016llx\n",HI);
	printf("                     lohi=0x%02x\n",lohi);
	printf("                     lolo=0x%016llx\n",lolo);
	printf("                     dexp=%d\n",dexp);
#endif

	}

	/* produce digits */

	if ( fflag )
	{		/* f format */

		/* need 1 integer digit */

		nd = min(34, ndigit + dexp + 1);
	}
	else
	{
		nd = ndigit;	/* e format */
	}

#if QUAD_DEBUG
	printf("number of digits: nd=%d\n",nd);
	printf("                  ndigit=%d\n",ndigit);
	printf("                  dexp=%d\n",dexp);
#endif

	/* produce digits - always at least one */

	do
	{
		*buffer++ = '0' + (HI >> 60);
		HI &= (1ULL << 60) - 1;
		lolo *= 10;
		lohi = lohi * 10 + (lolo >> 60);
		lolo &= (1ULL << 60) - 1;
		HI = HI * 10 + (lohi >> 4);
		lohi &= 0xF;

#if QUAD_DEBUG
	printf("another digit: HI=0x%016llx\n",HI);
	printf("               lohi=0x%02x\n",lohi);
	printf("               lolo=0x%016llx\n",lolo);
	*buffer = '\0';
	printf("               buffer=%s\n",buffer0);
#endif

	}
	while ( --nd > 0 );

  /* decimal round */

	/*	*buffer	guard	rest	Action
	 * 	
	 * 	dc	0-4	dc	none
	 *	dc	6-9	dc	round
	 * 	odd	5	dc	round
	 * 	even	5	0	none
	 * 	even	5	!=0	round
	 */

	guard = HI>>60;

	if ( guard >= 5 )
	{
		if ( guard > 5 ||
		     (*buffer-1) & 1 ||	/* buffer odd */
		     HI & ((1ULL << 60) - 1) ||
		     lohi || lolo	/* rest non-zero */
		   )
		{

	char *b = buffer;	/* buffer pointer */

		/* must round */

#if QUAD_DEBUG
	printf("must round\n");
#endif
	for ( b=buffer-1; ; b-- )
	{
		if ( (*b < '0') || ('9' < *b) )
		{
			/* attempt to carry into sign */

			*(b + 1) = '1';
			dexp++;

			if ( fflag && (nd != min(34, ndigit + dexp + 1)) )
			{ 
				*buffer++ = '0'; /* need one more digit */
#if QUAD_DEBUG
	printf("added a zero: fflag=%d\n",fflag);
	printf("              ndigit=%d\n",ndigit);
	printf("              nd=%d\n",nd);
#endif
		}

#if QUAD_DEBUG
	printf("carry into sign: dexp=%d\n",dexp);
	printf("                 buffer=%s\n",buffer0);
#endif

		break;
	}

	(*b)++;

#if QUAD_DEBUG
	*buffer = '\0';
	printf("round: buffer=%s\n",buffer0);
#endif

	if ( *b > '9' )
	{	/* carry */

		*b = '0';

#if QUAD_DEBUG
	printf("carry: buffer=%s\n",buffer0);
#endif

	}
	else
	  break;		/* no carry */
	}
      
    }
  }

#if REALLYNEEDED
	if ( fflag && (ndigit + dexp < 0) )
	{
		/* no significant digits */

		dexp--;	/* This is a lie. However, that's the
			 * way dtoa behaves (sometimes!), so 
			 * make qtoa work the same way.
			 */
	}
#endif

	*buffer = '\0';

#if QUAD_DEBUG
	printf("done: buffer=%s\n",buffer0);
	printf("      dexp=%d\n", dexp);
#endif

	return dexp;
}
