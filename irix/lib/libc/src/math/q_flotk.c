#include <stdio.h>
#include <inttypes.h>
#include "quad.h"
#include <sgidefs.h>

	/* intrinsic QFLOTK */

	/* by value only */

	/* converts int64_t to long double */

long double
__q_flotk( n )
int64_t	n;
{
int64_t m;
double	w, ww;
ldquad	result;

#include "msg.h"

	m = (n >> 11);
	m <<= 11;

	w = (double)m;

	ww = (double)(n - m);

	/* normalize result */

	result.q.hi = w + ww;
	result.q.lo = w - result.q.hi + ww;

	return ( result.ld );
}

