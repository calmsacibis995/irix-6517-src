#include <stdio.h>
#include <inttypes.h>
#include "quad.h"
#include <sgidefs.h>

	/* intrinsic QFLOTI */

	/* by value only */

	/* converts int16_t to long double */

long double
__q_floti( n )
int16_t	n;
{
ldquad	result;

#include "msg.h"

	result.q.hi = n;
	result.q.lo = 0.0;

	return ( result.ld );
}

