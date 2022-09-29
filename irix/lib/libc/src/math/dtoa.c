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
 *
 * Description: Convert a double precision binary number to decimal.
 */

#ident "$Id: dtoa.c,v 1.16 1997/07/08 20:57:54 danc Exp $"

#include <sgidefs.h>
#include <values.h>
#include "math_extern.h"

#define _IEEE 1
#include "fpparts.h"
#undef 	_IEEE

#define min(x,y) ((x)<(y)? (x): (y))
#define max(x,y) ((x)>(y)? (x): (y))

#ifdef DTOA_DEBUG
#include <stdio.h>
#endif

extern  double  fabs(double);
#pragma intrinsic (fabs)

extern	__int32_t _mixed_dtoa(char *, __int32_t, double, __int32_t, __int32_t,
__uint64_t, __int32_t, __int32_t);

extern __int32_t __dtoa( char *, __int32_t, double, __int32_t, __int32_t );

/* powers of 10 from 0 to 24 */

const dblu _tenpow[] =
{
0x3ff00000, 0x00000000,
0x40240000, 0x00000000,
0x40590000, 0x00000000,
0x408f4000, 0x00000000,
0x40c38800, 0x00000000,
0x40f86a00, 0x00000000,
0x412e8480, 0x00000000,
0x416312d0, 0x00000000,
0x4197d784, 0x00000000,
0x41cdcd65, 0x00000000,
0x4202a05f, 0x20000000,
0x42374876, 0xe8000000,
0x426d1a94, 0xa2000000,
0x42a2309c, 0xe5400000,
0x42d6bcc4, 0x1e900000,
0x430c6bf5, 0x26340000,
0x4341c379, 0x37e08000,
0x43763457, 0x85d8a000,
0x43abc16d, 0x674ec800,
0x43e158e4, 0x60913d00,
0x4415af1d, 0x78b58c40,
0x444b1ae4, 0xd6e2ef50,
0x4480f0cf, 0x064dd592,
0x44b52d02, 0xc7e14af6,
0x44ea7843, 0x79d99db4,
};

/*
	Numbers such as 1.5 were not being rounded correctly using %.0e format,
	for example.  The problem is that 1.5 is first scaled to .15 before
	extracting digits.  However, .15 is not exactly representable in base 2,
	and a fraction of a bit is dropped in the process.  This error is fatal,
	and we can never calculate the correctly rounded value after it.
	
	Consequently, the algorithm has been modified to extract the digits
	of the integral part of x first.  This works as long as fabs(x) < 2**52
	or fabs(x) < 2**76 and the number of digits required is < 15.
	For clarity, the algorithm for arguments described above has been
	separated into subroutine _mixed_dtoa.

	This change is going into the 6.2 release of the compiler.
*/

/* ====================================================================
 *
 * FunctionName: _dtoa_f77
 *
 * Description: Fortran interface to __dtoa
 *
 * ====================================================================
 */

__int32_t			/* returns decimal exponent */
_dtoa_f77(buffer, ndigit, x, fflag) 
char *buffer;			/* Destination for ascii string */ 
__int32_t ndigit;		/* number of digits to convert.
  				 * 1 <= ndigit <= 17
				 * Log2(1e17) = 56.5 bits, so the
 				 * 53 significant bits can be con-
 				 * verted to decimal with room left
 				 * over.
				 */ 
double x;			/* number to convert to ascii */ 
__int32_t fflag;		/* 0 for e format, 1 for f format */ 
{
	return ( __dtoa(buffer, ndigit, x, fflag, _ROUNDUP_RM) );
}

/* ====================================================================
 *
 * FunctionName: _dtoa
 *
 * Description: Interface to __dtoa; calls __dtoa with rounding_mode =
 *		_IEEE_RM
 *
 * ====================================================================
 */

__int32_t			/* returns decimal exponent */
_dtoa(buffer, ndigit, x, fflag) 
char *buffer;			/* Destination for ascii string */ 
__int32_t ndigit;		/* number of digits to convert.
  				 * 1 <= ndigit <= 17
				 * Log2(1e17) = 56.5 bits, so the
 				 * 53 significant bits can be con-
 				 * verted to decimal with room left
 				 * over.
				 */ 
double x;			/* number to convert to ascii */ 
__int32_t fflag;		/* 0 for e format, 1 for f format */ 
{
	return ( __dtoa(buffer, ndigit, x, fflag, _IEEE_RM) );
}


#if (_MIPS_SIM == _MIPS_SIM_ABI32)

typedef  union {
	__uint32_t	i[2];
	__uint64_t	ll;
} _ullval;

extern	void _get_first_digit( __uint32_t *, __uint32_t *, __int32_t * );

extern	void _get_digits( char **,__uint32_t *, __uint32_t *, __int32_t * );

/* ====================================================================
 *
 * FunctionName: __dtoa
 *
 * Description: Convert a double precision binary number to decimal.
 *
 * ====================================================================
 */

__int32_t			/* returns decimal exponent */
__dtoa(buffer, ndigit, x, fflag, round_mode) 
char *buffer;			/* Destination for ascii string */ 
__int32_t ndigit;		/* number of digits to convert.
  				 * 1 <= ndigit <= 17
				 * Log2(1e17) = 56.5 bits, so the
 				 * 53 significant bits can be con-
 				 * verted to decimal with room left
 				 * over.
				 */ 
double x;			/* number to convert to ascii */ 
__int32_t fflag;		/* 0 for e format, 1 for f format */ 
__int32_t round_mode;		/* currently, _IEEE_RM or _ROUNDUP_RM */
{
_dval value;			/* the value manipulated */ 

__int32_t sign;			/* sign of number */ 
__int32_t exp;			/* exponents of number */ 
_ullval	frac;			/* fraction of number */
__int32_t dexp;			/* decimal exponent */ 
__int32_t bexp;			/* binary exponent */ 
__int32_t nd;			/* number of digits output */ 
__int32_t guard;		/* guard digit */
__int32_t round;		/* non-zero if round */
char	*head;			/* pointer to head of buffer */
char	*b;
double	y;

#ifdef DTOA_DEBUG 
char *buffer0 = buffer;		/* initial value of buffer */
#endif


	head = buffer;		/* save in case we have to zap sign later */

	value = * ((_dval *) &x);

#ifdef DTOA_DEBUG 
	printf("initially: value=0x%016llx\n", * ((__uint64_t *) &value)); 
#endif

	sign = SIGNBIT(value); 
	exp = EXPONENT(value);
	frac.i[0] = value.fparts.hi;
	frac.i[1] = value.fparts.lo;
	*buffer++ = (char)(sign? '-': ' ');

#ifdef DTOA_DEBUG 
	printf("after field extraction: frac=0x%016llx\n",  frac.ll); 
	printf("                        exp=0x%0x\n", exp); 
	printf("                        sign=%d\n", sign); 
	*buffer = '\0'; 
	printf("                  buffer=%s\n",buffer0);
#endif

	if( exp == 0x7ff )
	{	/* infinity or NaN */ 
	char *s;
	char *infinity = "inf"; 
	char *nan = "nan";

		s = (frac.i[0] || frac.i[1]) ? nan : infinity;

		do 
	  		*buffer++ = *s; 
		while ( *s++ );

#ifdef DTOA_DEBUG 
	printf("NaN or Infinity: buffer=%s\n",buffer0); 
#endif

		return(0); 
	}

	if ( (0x3ff <= exp) && (exp < 0x44b) )
	{	/* 1.0 <= x < 2**76 */

		y = fabs(x);

		/* compute log10(y) exactly to determine number of digits 
		 * in the integral part of y
		 */
	
		dexp = (exp-1023)*(0x4D104D42 >> 21);
	
		/* truncate to -infinity */
	
		dexp >>= 11;
		dexp ++;
	
		if ( y < _tenpow[dexp].d ) /* adjust value if necessary */
			dexp--;
	
		/* the integral part of y has dexp + 1 digits */

#ifdef DTOA_DEBUG
	printf("log10(y) = %d\n", dexp);
#endif

		/* adjust no. of digits if fflag is set */

		if ( fflag )
			nd = min(17, dexp+1+ndigit);
		else
			nd = ndigit;

#ifdef DTOA_DEBUG
	printf("nd = %d\n", nd);
#endif

		if ( (exp < 0x433) || (nd < 15) )
			return ( _mixed_dtoa(buffer, nd, y, dexp, exp, frac.ll, fflag, round_mode) );
	}

	if( exp == 0 && frac.i[0] == 0 && frac.i[1] == 0 )
	{	/* true zero */ 

		b = buffer + ndigit; 

		while ( buffer < b ) 
			*buffer++ = '0';

		*head = ' ';	/* zap sign */

		*buffer++ = '\0';

#ifdef DTOA_DEBUG 
	printf("frac and exp were zero: frac=0x%016llx\n", frac.ll); 
	printf("                        exp=%d\n",exp);
	*buffer = '\0'; 
	printf("                        buffer=%s\n",buffer0);
#endif

		/*
		 *  For XPG4: for a passed value of true zero the number in
		 *  ndigit either allows one digit of zero to the left of the 
		 *  decimal point or no digits to the left of the decimal 
		 *  point.  A return(0) below will eventually become one when 
		 *  one is added to the return value from this routine and 
		 *  this value will be the position of the decimal point.
		 *  [see cvt() and fcvt() for the addition of one to the 
		 *  return from this routine.]
		 *
		 *  For example, a value of zero passed to this routine and 
		 *  ndigit=6 will result in (7) zeroes returned with a 
		 *  decimal position of '1', if a return(0) is done from
		 *  __dtoa().  This would represent: the value of 0.000000.
		 *
		 *  For example, a value of zero passed to this routine and 
		 *  ndigit=6 will result in (6) zeroes returned with a 
		 *  decimal position of '0', if a return(-1) is done from 
		 *  __dtoa().  This would represent: the value of 000000.
		 *
		 *  SGI chooses to return(-1) from __dtoa() for a passed in
		 *  value of true zero.
		 */

		return(-1);
	}

#ifdef DTOA_DEBUG 
	printf("value non-zero: frac=0x%016llx\n", frac.ll);
	printf("                exp=%d\n",exp); 
#endif

	/* processing a number. create 64b fraction */ 
	/* frac is non-zero, or the following while loop wouldn't terminate */ 

	if( exp == 0 )
	{	/* denorm */ 

		exp++;

		while( !(frac.i[0] & (1<<20)) )
		{ /* normalize - need not be fast */ 

			frac.i[0] <<=1; 
			frac.i[0] |= (frac.i[1] >> 31);
			frac.i[1] <<= 1;
			exp--;

#ifdef DTOA_DEBUG 
	printf("denorm: frac=0x%016llx\n", frac.ll); 
	printf("exp=%d\n",exp); 
#endif

		}
	} 
	else
	{	/* value is normal number */ 

		frac.i[0] |= 1<<20;	/* expose hidden bit */

#ifdef DTOA_DEBUG 
	printf("normal: frac=0x%016llx\n", frac.ll); 
	printf("exp=%d\n",exp); 
#endif

	}

	frac.i[0] <<=11;		/* left align */
	frac.i[0] |= (frac.i[1] >> 21);
	frac.i[1] <<= 11;

#ifdef DTOA_DEBUG 
	printf("frac aligned: frac=0x%016llx\n", frac.ll); 
#endif

	/* At this point we have a 64b binary fraction frac, the low 11
	 * bits of which are zero, and a biased binary exponent exp.
	 *
	 * Generate the decimal exponent by multiplying the binary 
	 * exponent by log10(2). 
	 */

	/* exp is at most 9 bits plus sign. Shifts avoid overflow
	 * and recover integer part.
	 */

	dexp = (exp-1023)*(0x4D104D42 >> 21);

	/* truncate to -infinity */

	dexp >>= 11;
	dexp++;

#ifdef DTOA_DEBUG
	printf("before scaling: dexp=%d\n",dexp);
#endif

	/* Scale fraction by decimal exponent. */

	if ( dexp )	/* value *= 10 ** -dexp */
    		__tenscale(&frac.ll, -dexp, &bexp); 
	else
		bexp=0;

#ifdef DTOA_DEBUG
	printf("after scaling: frac=0x%016llx\n",frac.ll);
	printf("               bexp=%d\n",bexp);
#endif

	exp += bexp;

#ifdef DTOA_DEBUG
	printf("adjusted exp: exp=%d\n",exp);
#endif

	/* unbias exp */

	exp -= 1022;

	/* Remove binary exponent by denormalizing */
	/* Shift an extra 4 bits to allow for later digit generation */

	round =  ( 1<<(3-exp)      & frac.i[1] ) &&     /* round bit set and */
		 ( ( 1<<(4-exp)    & frac.i[1] ) ||     /*     odd */
		   ((1<<(3-exp))-1 & frac.i[1] )        /*     or not borderline */
		 );

#ifdef DTOA_DEBUG
	printf("rounding: round=%d\n",round);
#endif

	/* shift frac right (4 - exp), leaving room for digit generation */

	/* frac.ll >>= (4 - exp); */

	frac.i[1] >>= (4-exp);
	frac.i[1] |= (frac.i[0] << (28 + exp));
	frac.i[0] >>= (4-exp);
	frac.ll += round;

#ifdef DTOA_DEBUG
	printf("denormalized: frac=0x%016llx\n",frac.ll);
#endif

	/* find first non-zero digit (use assembly language for speed) */

/*
	while( !(frac.i[0] >> 28) )
	{
		frac.ll = frac.ll * 10;
		dexp--;
	}
*/

	_get_first_digit(&frac.i[0], &frac.i[1], &dexp);

#ifdef DTOA_DEBUG
	printf("finding first digit: frac=0x%016llx\n",frac.ll);
	printf("                     dexp=%d\n",dexp);
#endif
	/* produce digits */

	if( fflag )
	{			/* f format */
		nd = min(17, ndigit + dexp + 1); /* need 1 integer digit */
	}
	else
	{
		nd = ndigit;		/* e format */
	}

	if ( fflag && (ndigit + dexp < 0) )
	{
		if ( ndigit + dexp == -1 )
		{
			/* result is zero, rounded using successive digits */

			*buffer++ = '0';
			dexp++;

#ifdef DTOA_DEBUG 
	printf("one significant digit: ndigit=%d\n", ndigit); 
	printf("                        dexp=%d\n",dexp);
	*buffer = '\0'; 
	printf("                        buffer=%s\n",buffer0);
#endif

			guard = (frac.i[0]>>28);

#ifdef DTOA_DEBUG
	printf("before round: guard = %d\n", guard);
	printf("             *(buffer-1) = %x\n", *(buffer-1));
	printf("             frac & ((1ULL<<60)-1) = 0x%016llx\n", frac.ll & ((1ULL<<60)-1));
#endif

			if ( (round_mode == _IEEE_RM) && (guard >= 5) )
			{
				if ( (guard > 5) ||
				     ( (frac.i[0] & ((1<<28)-1)) || frac.i[1] )
				   )
				{ /* rest non-zero */
					*(buffer-1) = '1';
					*buffer = '\0';

#ifdef DTOA_DEBUG
	printf("done: buffer=%s\n",buffer0);
	printf("      dexp=%d\n", dexp);
#endif

					return dexp;
				}
			}
			else if (guard >= 5)
			{	/* round_mode == _ROUNDUP_RM */

				*(buffer-1) = '1';
				*buffer = '\0';

#ifdef DTOA_DEBUG
	printf("done: buffer=%s\n",buffer0);
	printf("      dexp=%d\n", dexp);
#endif
				return dexp;
			}

			buffer--;
		}

		/* result is zero */

		b = buffer + ndigit; 

		while ( buffer < b ) 
			*buffer++ = '0';

		*head = ' ';	/* zap sign */

		*buffer++ = '\0';

#ifdef DTOA_DEBUG 
	printf("no significant digits: ndigit=%d\n", ndigit); 
	printf("                        buffer=%s\n",buffer0);
#endif

		return(0);
	}

#ifdef DTOA_DEBUG
	printf("number of digits: nd=%d\n",nd);
	printf("                  ndigit=%d\n",ndigit);
	printf("                  dexp=%d\n",dexp);
#endif

	/* produce digits - always at least one (use assembly
	 * language for speed)
	*/

/*
	do
	{
		*buffer++ = (char)('0' + (frac.i[0]>>28));
		frac.i[0] &= (1<<28) - 1;
		frac.ll = frac.ll * 10;

	} while ( --nd > 0 );
*/

	_get_digits(&buffer, &frac.i[0], &frac.i[1], &nd);

#ifdef DTOA_DEBUG
	printf("after calculating digits:\n");
	*buffer = '\0';
	printf("               buffer=%s\n",buffer0);
#endif

	/* decimal round */

	/*	*buffer	guard	rest	Action
	 * 	
	 * 	dc	0-4	dc	none
	 *	dc	6-9	dc	round
	 * 	odd	5	dc	round
	 * 	even	5	0	none
	 * 	even	5	!=0	round
	 */


	guard = (frac.i[0]>>28);

#ifdef DTOA_DEBUG
	printf("before round: guard = %d\n", guard);
	printf("             *(buffer-1) = %x\n", *(buffer-1));
	printf("             frac & ((1ULL<<60)-1) = 0x%016llx\n", frac.ll & ((1ULL<<60)-1));
#endif

	round = 0;

	if( (guard >= 5) && (round_mode == _IEEE_RM) )
	{
		if( (guard > 5) ||
		    (*(buffer-1)&1) ||		/* buffer odd */
		    ( (frac.i[0] & ((1<<28)-1)) || frac.i[1] ) /* rest non-zero */
		  )
		{
			round = 1;
		}
	}
	else if ( guard >= 5 )
		round = 1;

	if ( round )
	{
		/* must round */

#ifdef DTOA_DEBUG
	printf("must round\n");
#endif

		for ( b=buffer-1; ; b-- )
		{
			if ( *b < '0' || '9' < *b )
			{ /* attempt to carry into sign */

				*(b+1) = '1';
				dexp++;

				if( fflag && nd != min(17, ndigit + dexp + 1) )
				{ 
					*buffer++ = '0'; /* need one more digit */

#ifdef DTOA_DEBUG
	printf("added a zero: fflag=%d\n",fflag);
	printf("              ndigit=%d\n",ndigit);
	printf("              nd=%d\n",nd);
#endif

				}

#ifdef DTOA_DEBUG
	printf("carry into sign: dexp=%d\n",dexp);
	printf("                 buffer=%s\n",buffer0);
#endif

				break;
			}

			(*b)++;

#ifdef DTOA_DEBUG
	*buffer = '\0';
	printf("round: buffer=%s\n",buffer0);
#endif

			if( *b > '9' )
			{		/* carry */
				*b = '0';

#ifdef DTOA_DEBUG
	printf("carry: buffer=%s\n",buffer0);
#endif

			}
			else
				break;		/* no carry */
		}
	}

#if REALLYNEEDED

	if ( fflag && (ndigit+dexp < 0) )
	{ /* no significant digits */

		dexp--;		/* This is a lie. However, that's the
				 * way dtoa.s behaves (sometimes!), so 
				 * make dtoa.c work the same way.
				*/
	}
#endif

	*buffer = '\0';

#ifdef DTOA_DEBUG
	printf("done: buffer=%s\n",buffer0);
	printf("      dexp=%d\n", dexp);
#endif

	return dexp;

}

#else /* (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32) */

/* ====================================================================
 *
 * FunctionName: __dtoa
 *
 * Description: Convert a double precision binary number to decimal.
 *
 * ====================================================================
 */

__int32_t			/* returns decimal exponent */
__dtoa(buffer, ndigit, x, fflag, round_mode) 
char *buffer;			/* Destination for ascii string */ 
__int32_t ndigit;		/* number of digits to convert.
  				 * 1 <= ndigit <= 17
				 * Log2(1e17) = 56.5 bits, so the
 				 * 53 significant bits can be con-
 				 * verted to decimal with room left
 				 * over.
				 */ 
double x;			/* number to convert to ascii */ 
__int32_t fflag;		/* 0 for e format, 1 for f format */ 
__int32_t round_mode;		/* currently, _IEEE_RM or _ROUNDUP_RM */
{
_dval value;			/* the value manipulated */ 

__int32_t sign;			/* sign of number */ 
__int32_t exp;			/* exponents of number */ 
__uint64_t frac;		/* fraction of number */
__int32_t dexp;			/* decimal exponent */ 
__int32_t bexp;			/* binary exponent */ 
__int32_t nd;			/* number of digits output */ 
__int32_t guard;		/* guard digit */
__int32_t round;		/* non-zero if round */
char	*head;			/* pointer to head of buffer */
char	*b;
double	y;

#ifdef DTOA_DEBUG 
char *buffer0 = buffer;		/* initial value of buffer */
#endif


	head = buffer;		/* save in case we have to zap sign later */

	value = * ((_dval *) &x);

#ifdef DTOA_DEBUG 
	printf("initially: value=0x%016llx\n", * ((__uint64_t *) &value)); 
#endif

	sign = SIGNBIT(value); 
	exp = EXPONENT(value);
	frac= (* ((__uint64_t *) &value)) << 12 >> 12; 
	*buffer++ = (char)(sign? '-': ' ');

#ifdef DTOA_DEBUG 
	printf("after field extraction: frac=0x%016llx\n",  frac); 
	printf("                        exp=0x%0x\n", exp); 
	printf("                        sign=%d\n", sign); 
	*buffer = '\0'; 
	printf("                  buffer=%s\n",buffer0);
#endif

	if( exp == 0x7ff )
	{	/* infinity or NaN */ 
	char *s;			/* infinity or nan string */ 
	char *infinity = "inf"; 
	char *nan = "nan";

		s = frac? nan: infinity;

		do 
	  		*buffer++ = *s; 
		while ( *s++ );

#ifdef DTOA_DEBUG 
	printf("NaN or Infinity: buffer=%s\n",buffer0); 
#endif

		return(0); 
	}

	if ( (0x3ff <= exp) && (exp < 0x44b) )
	{	/* 1.0 <= exp <= 2**76 */

		y = fabs(x);

		/* compute log10(y) exactly to determine number of digits 
		 * in the integral part of y
		 */
	
		dexp = (exp-1023)*(0x4d104d42 >> 21);
	
		/* truncate to -infinity */
	
		dexp >>= 11;
		dexp ++;

#ifdef DTOA_DEBUG
	printf("dexp = %d\n", dexp);
#endif

		if ( y < _tenpow[dexp].d ) /* adjust value if necessary */
			dexp--;
	
		/* the integral part of y has dexp + 1 digits */

#ifdef DTOA_DEBUG
	printf("log10(y) = %d\n", dexp);
#endif

		/* adjust no. of digits if fflag is set */

		if ( fflag )
			nd = min(17, dexp+1+ndigit);
		else
			nd = ndigit;

#ifdef DTOA_DEBUG
	printf("nd = %d\n", nd);
#endif

		if ( (exp < 0x433) || (nd < 15) )
			return ( _mixed_dtoa(buffer, nd, y, dexp, exp, frac, fflag, round_mode) );
	}

	if( exp == 0 && frac == 0 )
	{	/* true zero */ 

		b = buffer + ndigit; 

		while ( buffer < b ) 
			*buffer++ = '0';

		*head = ' ';	/* zap sign */

		*buffer++ = '\0';

#ifdef DTOA_DEBUG 
	printf("frac and exp were zero: frac=0x%016llx\n", frac); 
	printf("                        exp=%d\n",exp);
	*buffer = '\0'; 
	printf("                        buffer=%s\n",buffer0);
#endif

		/*
		 *  For XPG4: for a passed value of true zero the number in
		 *  ndigit either allows one digit of zero to the left of the 
		 *  decimal point or no digits to the left of the decimal 
		 *  point.  A return(0) below will eventually become one when 
		 *  one is added to the return value from this routine and 
		 *  this value will be the position of the decimal point.
		 *  [see cvt() and fcvt() for the addition of one to the 
		 *  return from this routine.]
		 *
		 *  For example, a value of zero passed to this routine and 
		 *  ndigit=6 will result in (7) zeroes returned with a 
		 *  decimal position of '1', if a return(0) is done from
		 *  __dtoa().  This would represent: the value of 0.000000.
		 *
		 *  For example, a value of zero passed to this routine and 
		 *  ndigit=6 will result in (6) zeroes returned with a 
		 *  decimal position of '0', if a return(-1) is done from 
		 *  __dtoa().  This would represent: the value of 000000.
		 *
		 *  SGI chooses to return(-1) from __dtoa() for a passed in
		 *  value of true zero.
		 */

		return(-1);
	}

#ifdef DTOA_DEBUG 
	printf("value non-zero: frac=0x%016llx\n", frac);
	printf("                exp=%d\n",exp); 
#endif

	/* processing a number. create 64b fraction */ 
	/* frac is non-zero, or the following while loop wouldn't terminate */ 

	if( exp == 0 )
	{	/* denorm */ 

		exp++;

		while( !(frac & (1ULL<<52)) )
		{ /* normalize - need not be fast */ 

			frac <<=1; 
			exp--;

#ifdef DTOA_DEBUG 
	printf("denorm: frac=0x%016llx\n", frac); 
	printf("exp=%d\n",exp); 
#endif

		}
	} 
	else
	{	/* value is normal number */ 

		frac |= 1ULL<<52;		/* expose hidden bit */

#ifdef DTOA_DEBUG 
	printf("normal: frac=0x%016llx\n", frac); 
	printf("exp=%d\n",exp); 
#endif

	}

	frac <<=11;			/* left align */

#ifdef DTOA_DEBUG 
	printf("frac aligned: frac=0x%016llx\n", frac); 
#endif

	/* At this point we have a 64b binary fraction frac, the low 11
	 * bits of which are zero, and a biased binary exponent exp.
	 *
	 * Generate the decimal exponent by multiplying the binary 
	 * exponent by log10(2). 
	 */

	/* exp is at most 9 bits plus sign. Shifts avoid overflow
	 * and recover integer part.
	 */

	dexp = (__int32_t)( ( (exp-1023)) * (0x4D104D42 >> 21) );

	/* truncate to -infinity */

	dexp >>= 11;
	dexp++;

#ifdef DTOA_DEBUG
	printf("before scaling: dexp=%d\n",dexp);
#endif

	/* Scale fraction by decimal exponent. */

	if ( dexp )	/* value *= 10 ** -dexp */
    		__tenscale(&frac, -dexp, &bexp); 
	else
		bexp=0;

#ifdef DTOA_DEBUG
	printf("after scaling: frac=0x%016llx\n",frac);
	printf("               bexp=%d\n",bexp);
#endif

	exp += bexp;

#ifdef DTOA_DEBUG
	printf("adjusted exp: exp=%d\n",exp);
#endif

	/* unbias exp */

	exp -= 1022;

	/* Remove binary exponent by denormalizing */
	/* Shift an extra 4 bits to allow for later digit generation */

	round =  ( 1<<(3-exp)      & frac ) &&     /* round bit set and */
		 ( ( 1<<(4-exp)    & frac ) ||     /*     odd */
		   ((1<<(3-exp))-1 & frac )        /*     or not borderline */
		 );

#ifdef DTOA_DEBUG
	printf("rounding: round=%d\n",round);
#endif

	frac = ( frac>>(__uint64_t)(4-exp) ) + round;

#ifdef DTOA_DEBUG
	printf("denormalized: frac=0x%016llx\n",frac);
#endif

	/* find first non-zero digit */

	while( !(frac >> 60) )
	{
		frac = frac * 10;
		dexp--;

#ifdef DTOA_DEBUG
	printf("finding first digit: frac=0x%016llx\n",frac);
	printf("                     dexp=%d\n",dexp);
#endif

	}

	/* produce digits */

	if( fflag )
	{			/* f format */
		nd = min(17, ndigit + dexp + 1); /* need 1 integer digit */
	}
	else
	{
		nd = ndigit;		/* e format */
	}

	if ( fflag && (ndigit + dexp < 0) )
	{
		if ( ndigit + dexp == -1 )
		{
			/* result is zero, rounded using successive digits */

			*buffer++ = '0';
			dexp++;

#ifdef DTOA_DEBUG 
	printf("one significant digit: ndigit=%d\n", ndigit); 
	printf("                        dexp=%d\n",dexp);
	*buffer = '\0'; 
	printf("                        buffer=%s\n",buffer0);
#endif

			guard = (__int32_t)(frac>>60);

#ifdef DTOA_DEBUG
	printf("before round: guard = %d\n", guard);
	printf("             *(buffer-1) = %x\n", *(buffer-1));
	printf("             frac & ((1ULL<<60)-1) = 0x%016llx\n", frac & ((1ULL<<60)-1));
#endif

			if ( (round_mode == _IEEE_RM) && (guard >= 5) )
			{
				if ( guard > 5 || frac & ((1ULL<<60)-1))
				{ /* rest non-zero */
					*(buffer-1) = '1';
					*buffer = '\0';

#ifdef DTOA_DEBUG
	printf("done: buffer=%s\n",buffer0);
	printf("      dexp=%d\n", dexp);
#endif

					return dexp;
				}
			}
			else if (guard >= 5)
			{	/* round_mode == _ROUNDUP_RM */

				*(buffer-1) = '1';
				*buffer = '\0';

#ifdef DTOA_DEBUG
	printf("done: buffer=%s\n",buffer0);
	printf("      dexp=%d\n", dexp);
#endif
				return dexp;
			}


			buffer--;
		}

		/* result is zero */

		b = buffer + ndigit; 

		while ( buffer < b ) 
			*buffer++ = '0';

		*head = ' ';	/* zap sign */

		*buffer++ = '\0';

#ifdef DTOA_DEBUG 
	printf("no significant digits: ndigit=%d\n", ndigit); 
	printf("                        buffer=%s\n",buffer0);
#endif

		return(0);
	}

#ifdef DTOA_DEBUG
	printf("number of digits: nd=%d\n",nd);
	printf("                  ndigit=%d\n",ndigit);
	printf("                  dexp=%d\n",dexp);
#endif

	/* produce digits - always at least one */

	do
	{
		*buffer++ = (char)('0' + (frac>>60));
		frac &= (1ULL<<60) - 1;
		frac = frac * 10;

#ifdef DTOA_DEBUG
	printf("another digit: frac=0x%016llx\n",frac);
	*buffer = '\0';
	printf("               buffer=%s\n",buffer0);
#endif

	} while ( --nd > 0 );

	/* decimal round */

	/*	*buffer	guard	rest	Action
	 * 	
	 * 	dc	0-4	dc	none
	 *	dc	6-9	dc	round
	 * 	odd	5	dc	round
	 * 	even	5	0	none
	 * 	even	5	!=0	round
	 */


	guard = (__int32_t)(frac>>60);

#ifdef DTOA_DEBUG
	printf("before round: guard = %d\n", guard);
	printf("             *(buffer-1) = %x\n", *(buffer-1));
	printf("             frac & ((1ULL<<60)-1) = 0x%016llx\n", frac & ((1ULL<<60)-1));
#endif

	round = 0;

	if( (guard >= 5) && (round_mode == _IEEE_RM) )
	{
		if( (guard > 5) ||
		    (*(buffer-1)&1) ||		/* buffer odd */
		    (frac & ((1ULL<<60)-1))	/* rest non-zero */
		  )
		{
			round = 1;
		}
	}
	else if ( guard >= 5 ) /* round_mode == _ROUNDUP_RM */
		round = 1;

	if ( round )
	{
		/* must round */

#ifdef DTOA_DEBUG
	printf("must round\n");
#endif

		for ( b=buffer-1; ; b-- )
		{
			if ( *b < '0' || '9' < *b )
			{ /* attempt to carry into sign */

				*(b+1) = '1';
				dexp++;

				if( fflag && nd != min(17, ndigit + dexp + 1) )
				{ 
					*buffer++ = '0'; /* need one more digit */

#ifdef DTOA_DEBUG
	printf("added a zero: fflag=%d\n",fflag);
	printf("              ndigit=%d\n",ndigit);
	printf("              nd=%d\n",nd);
#endif

				}

#ifdef DTOA_DEBUG
	printf("carry into sign: dexp=%d\n",dexp);
	printf("                 buffer=%s\n",buffer0);
#endif

				break;
			}

			(*b)++;

#ifdef DTOA_DEBUG
	*buffer = '\0';
	printf("round: buffer=%s\n",buffer0);
#endif

			if( *b > '9' )
			{		/* carry */
				*b = '0';

#ifdef DTOA_DEBUG
	printf("carry: buffer=%s\n",buffer0);
#endif

			}
			else
				break;		/* no carry */
		}
	}

#if REALLYNEEDED

	if ( fflag && (ndigit+dexp < 0) )
	{ /* no significant digits */

		dexp--;		/* This is a lie. However, that's the
				 * way dtoa.s behaves (sometimes!), so 
				 * make dtoa.c work the same way.
				*/
	}
#endif

	*buffer = '\0';

#ifdef DTOA_DEBUG
	printf("done: buffer=%s\n",buffer0);
	printf("      dexp=%d\n", dexp);
#endif

	return dexp;

}
#endif

