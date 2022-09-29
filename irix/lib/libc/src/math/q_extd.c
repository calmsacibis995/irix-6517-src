#include <stdio.h>
#include <inttypes.h>
#include "quad.h"
#include <sgidefs.h>

	/* intrinsic QEXTD */

	/* by value only */

	/* converts double to long double */

long double
__q_extd( x )
double	x;
{
ldquad	result;

#include "msg.h"

        result.q.hi = x;
	result.q.lo = 0.0;

	return ( result.ld );
}

