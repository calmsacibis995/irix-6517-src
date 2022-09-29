#include <stdio.h>
#include <inttypes.h>
#include "quad.h"
#include <sgidefs.h>

	/* uint32_t to long double */

long double
__q_flotju( n )
uint32_t	n;
{
ldquad	result;

#include "msg.h"

	result.q.hi = n;
	result.q.lo = 0.0;
	return ( result.ld );
}

