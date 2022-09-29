#include <stdio.h>
#include <inttypes.h>
#include "quad.h"
#include <sgidefs.h>

	/* uint64_t to long double */

long double
__q_flotku( n )
uint64_t	n;
{
uint64_t m;
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

