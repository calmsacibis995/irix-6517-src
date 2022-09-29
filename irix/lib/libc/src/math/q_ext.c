#include <stdio.h>
#include <inttypes.h>
#include "quad.h"
#include <sgidefs.h>

	/* intrinsic QEXT */

	/* by value only */

	/* converts float to long double */

long double
__q_ext( float x )
{
ldquad	result;

#include "msg.h"

        result.q.hi = x;
	result.q.lo = 0.0;

	return ( result.ld );
}

