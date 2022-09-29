#include <sgidefs.h>
#include <values.h>
#include "synonyms.h"


#ifdef DTOA_DEBUG
#include <stdio.h>
#endif

#define _IEEE 1
#include "fpparts.h"
#undef 	_IEEE

typedef  union {
	__uint32_t	i[2];
	__uint64_t	ll;
} _ullval;

extern	dblu	_tenpow[];

#if (_MIPS_SIM == _MIPS_SIM_ABI32)

void _gen_digits( char **, __uint64_t *, __int32_t * );

#endif

static __int32_t roundbuff( char *, __int32_t, __int32_t, __int32_t );

extern double modf( double, double * );
extern __int32_t get_fpc_csr( void );
extern __int32_t set_fpc_csr( __int32_t );
extern void _get_int_digits( double, __int32_t, char * );

#ifdef DTOA_DEBUG
char *buffer0;
#endif

/*
   nd is the number of digits of output needed.  In this algorithm,
   argument x is assumed to be between 1.0 and 2**52, or 2**52 <= x <
   2**76 and nd < 15.

   This algorithm calculates one extra guard digit, used in rounding.
   If the integral part of x has exactly the right number of digits,
   i.e. nd+1, then the digits of x are determined right to left, using
   division by 10 and the remainder mod 10.  If nd + 1 exceeds the
   number of digits in the integral part of x, then the remaining
   digits are determined left to right, multiplying by 10 at each stage,
   as in the original dtoa algorithm.
 
   Finally, if the integral part of x has more than nd + 1 digits, x is
   scaled to a number with nd + 1 digits by dividing by an appropriate 
   power of 10.

   Note that the subroutine _get_int_digits uses floating point operations
   to do division by 10 and to compute the remainder mod 10.  This is done
   for speed, since integer division is slow.  This routine must be run
   in round to zero mode, so that fractional parts of multiplication by
   .1 are chopped instead of rounded.

   Two rounding modes are supported, IEEE (round to nearest) and round-up
   (round up if guard digit >= 5).  For backward compatibility, there 
   are some interface routines, and the main dtoa routine is now called
   __dtoa.

   If scaling is done, it must be done in round to zero mode, so that
   we can determine the sticky bit correctly.
*/

/* ====================================================================
 *
 * FunctionName: _mixed_dtoa
 *
 * Description: Convert a double precision binary number to decimal;
 *		assumes 1.0 <= x < 2**76.  If x >= 2**52, nd must be
 *		< 15.
 *
 * ====================================================================
 */

__int32_t
_mixed_dtoa(buffer, nd, x, dexp, exp, frac0, fflag, round_mode)
char		*buffer;
__int32_t	nd;		/* number of digits output */ 
double		x;
__int32_t	dexp;		/* decimal exponent */ 
__int32_t	exp;
__uint64_t	frac0;
__int32_t	fflag;
__int32_t	round_mode;	/* currently, _IEEE_RM or _ROUNDUP_RM */
{
_ullval		frac;
double		d, r;
double		w, dm;
__int32_t	i, k;
__int32_t	csr;
__int32_t	old_fpc_csr;
__int32_t	new_fpc_csr;
__int32_t	inexact;
__int32_t	carry;
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
__int32_t	count;
#endif

#ifdef DTOA_DEBUG 
char *buffer0 = buffer;		/* initial value of buffer */
#endif

#ifdef DTOA_DEBUG
	printf("entering _mixed_dtoa\n");
	printf("nd = %d\n", nd);
#endif

	frac.ll = frac0;

	/* run with rounding mode set to round-to-zero */

	new_fpc_csr = old_fpc_csr = get_fpc_csr();
	new_fpc_csr &= ~0x7; /* clear rounding bits and
			      * inexact flag
			      */
	new_fpc_csr |= 0x1; /* set round to zero */

	/* call set_fpc_csr before calling _get_int_digits */

	/* 1.0 <= x < 2**76 */

	/* try to determine nd + 1 digits of x (required number +
	   1 guard digit for rounding)
	 */

	if ( dexp < nd )
	{	/* integral part of x doesn't have enough digits */

		r = modf(x, &dm);

		/* determine first dexp + 1 digits of x from dm */

		set_fpc_csr( new_fpc_csr );

		_get_int_digits(dm, dexp+1, buffer);

		buffer += dexp + 1;

#ifdef DTOA_DEBUG
	buffer[0] = '\0';
	printf("number of digits = %d\n", dexp+1);
	printf("buffer = %s\n", buffer0);
#endif
		if ( r == 0.0 )
		{
			/* nothing left, so padd buffer with zeroes */

			for ( i=0; i<nd-dexp-1; i++ )
				buffer[i] = '0';

			buffer[nd-dexp-1] = '\0'; /* seal it up */

			set_fpc_csr( old_fpc_csr ); /* restore fpc csr */

			return ( dexp );
		}

		/* determine mantissa of r by shifting off integer
		   part of x
		 */

#if (_MIPS_SIM == _MIPS_SIM_ABI32)

		count = 8 + (exp - 0x3ff);

		if ( count < 32 )
		{
			frac.i[0] <<= count;
			frac.i[0] |= (frac.i[1] >> (32 - count));
			frac.i[1] <<= count;
		}
		else
		{
			count -= 32;
			frac.i[0] = (frac.i[1] << count);
			frac.i[1] = 0;
		}

		frac.i[0] &= 0xfffffff; /* make room for digit generation */

#else /* (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32) */

		frac.ll <<= 12 + (exp - 0x3ff);

		frac.ll >>= 4; /* make room for digit generation */
#endif

#ifdef DTOA_DEBUG
	printf("denormalized: frac = 0x%016llx\n", frac.ll);
#endif

		k = nd - dexp; /* number of digits left to compute, 
				* including guard digit
				*/

#if (_MIPS_SIM == _MIPS_SIM_ABI32)

		/* use assembly language for speed */

 		_gen_digits(&buffer, &frac.ll, &k);

#else /* (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32) */

		do
		{
			if ( frac.ll == 0L )
				break;

			frac.ll = frac.ll * 10; /* generate digit */
			*buffer++ = (char)('0' + (frac.ll>>60)); /* store it */
			frac.ll &= (1ULL<<60) - 1; /* zero first 4 bits */

		} while ( --k > 0 );
#endif

		if ( k > 0 ) /* nothing left, so padd buffer with zeroes */
			for ( i=0; i<k; i++ )
				*buffer++ = '0';

#ifdef DTOA_DEBUG
	*buffer = '\0';
	printf("buffer after digit generation = %s\n", buffer0);
#endif

		carry = roundbuff(--buffer, frac.ll!=0L, fflag, round_mode);



#ifdef DTOA_DEBUG
	printf("buffer after rounding = %s\n", buffer0);
#endif

		set_fpc_csr( old_fpc_csr ); /* restore fpc csr */

		return ( dexp+carry );

	}

	if ( dexp == nd )
	{
		r = modf(x, &dm);

		/* determine first nd + 1 digits of x from dm */

		set_fpc_csr( new_fpc_csr );

		_get_int_digits(dm, nd+1, buffer);

		buffer += nd + 1;

		/* r = sticky */

#ifdef DTOA_DEBUG
	printf("number of digits = %d\n", nd);
	buffer[0] = '\0';
	printf("buffer before rounding = %s\n", buffer0);
#endif

		carry = roundbuff(--buffer, r != 0.0, fflag, round_mode);



#ifdef DTOA_DEBUG
	printf("buffer after rounding = %s\n", buffer0);
#endif

		set_fpc_csr( old_fpc_csr ); /* restore fpc csr */

		return ( dexp+carry );
	}

	/* scale x down; the number of digits in the
	   integral part of x exceeds nd + 1
	 */


	w = _tenpow[dexp-nd].d;

	set_fpc_csr( new_fpc_csr );

	d = x/w;
	csr = get_fpc_csr();
	inexact = (csr >> 2) & 1;

#ifdef DTOA_DEBUG
	printf("csr after divide = %08x\n", csr);
	printf("inexact = %d\n", inexact);
#endif

#ifdef DTOA_DEBUG
	printf("scaled value of x=%08x%08x\n", *(int *)&d, *((int *)&d + 1));
#endif

	r = modf(d, &dm);

#ifdef DTOA_DEBUG
	printf("integral part of d=%08x%08x\n", *(int *)&dm, *((int *)&dm + 1));
	printf("fractional part of d=%08x%08x\n", *(int *)&r, *((int *)&r + 1));
#endif

	_get_int_digits( dm, nd+1, buffer );

	buffer += nd + 1;

#ifdef DTOA_DEBUG
	printf("number of digits = %d\n", nd);
	buffer[0] = '\0';
	printf("buffer = %s\n", buffer0);
#endif

	/* use r and inexact flag to determine rounding */

	carry = roundbuff(--buffer, (r != 0.0) || inexact, fflag, round_mode);



#ifdef DTOA_DEBUG
	printf("buffer after rounding = %s\n", buffer0);
#endif

	set_fpc_csr( old_fpc_csr ); /* restore fpc csr */

	return ( dexp+carry );
}

/* ====================================================================
 *
 * FunctionName: roundbuff
 *
 * Description: rounds ascii buffer.  Assume buffer contains round and
 *		guard digits.  Sticky flag is passed as a parameter.
 *		Buffer is preceded by a non-digit.  On entry, buffer
 *		is pointing to the last digit, i.e. the guard digit.
 *		Returns overflow, i.e. flag indicating carry into the
 *		non-digit position.  Round_mode indicates rounding mode,
 *		either _IEEE_RM, or _ROUNDUP_RM.  This routine must
 *		terminate the buffer with a null character.
 *
 * ====================================================================
 */

static
__int32_t
roundbuff( char *buffer, __int32_t sticky, __int32_t fflag, __int32_t round_mode )
{
char	round, guard;
__int32_t	carry;
__int32_t	overflow;
char	*b;

#ifdef DTOA_DEBUG
	printf("roundbuff:  sticky = %08x, fflag = %d, round_mode = %d\n",
		sticky, fflag, round_mode);
#endif

	guard = *buffer--;
	round = *buffer;

	if ( round_mode == _IEEE_RM )
		carry = (guard > '5') || ( (guard == '5') && sticky ) ||
			( (guard == '5') && ((round - '0')%2 == 1) );
	else
		carry = (guard >= '5');

	if ( !carry )
	{
		*++buffer = '\0';

		return ( 0 );
	}

	overflow = 0;

	for ( b=buffer; ; b-- )
	{
		if ( *b < '0' || '9' < *b )
		{ /* attempt to carry into sign */

			*(b+1) = '1';
			overflow = 1;

			if ( fflag )
			{
				*++buffer = '0';
			}

#ifdef DTOA_DEBUG
	printf("roundbuff:  carry into sign\n");
#endif

			break;
		}

		(*b)++;

		if( *b > '9' )
		{		/* carry */
			*b = '0';

		}
		else
			break;		/* no carry */
	}

	 *++buffer = '\0';

#ifdef DTOA_DEBUG
	printf("roundbuff returning overflow=%d\n", overflow);
#endif

	return ( overflow );
}

