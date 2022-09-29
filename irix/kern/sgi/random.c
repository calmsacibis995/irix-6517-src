
#include <sys/types.h>
#include <sys/systm.h>
/*
 *
 *  The "Minimal Standard Random Number Generator" of Park and Miller,
 *  CACM Vol 31, No 10, October 1988, pp 1192-1201.
 *
 *  Caveats:
 *
 *	The global variable RandomValue is not protected by a lock and,
 *      therefore, multiple concurrent calls of Random() may return the
 *	same value, and subsequent calls may skip values in the pseudo-
 *	random sequence.
 *
 *	Random() uses two long multiplies, a long divide, and a long
 *	modulus operation, all of which are rather expensive.  A faster
 *	implementation that avoids the divide and modulus is described in
 *	an article by David Carta in CACM Vol 33, No 1, January 1990,
 *	pp 87-88.  Unfortunately, it requires access to the 64-bit
 *	product of the multiplication of two 32-bit integers, which is
 *	not available on many processors.
 *
 *  	Written by Steve Deering, May 1993.
 *	Modified by Rosen Sharma, Aug 1994.
 *
 *	MULTICAST 1.0
 */


static unsigned long RandomValue = 1;


void srandom(int seed)
{
    /* converts seed into a value between 1 and 2^31 - 2 */

    RandomValue = seed % 2147483646 + 1;
}


unsigned long random( )
{
    /* cycles pseudo-randomly through all values between 1 and 2^31 - 2 */

    register long rv = RandomValue;
    register long lo;
    register long hi;

    hi = rv / 127773;
    lo = rv % 127773;
    rv = 16807 * lo - 2836 * hi;
    if( rv <= 0 ) rv += 2147483647;
    return( RandomValue = rv );
}
