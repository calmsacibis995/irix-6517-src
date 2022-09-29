#include <stdio.h>
#include <inttypes.h>
#include "quad.h"
#include <sgidefs.h>

	/* computes the unary minus of a long double */

long double
__q_neg( xhi, xlo )
double xhi, xlo;
{
ldquad	result;

#include "msg.h"

	result.q.hi = -(xhi);
	result.q.lo = -(xlo);

	return ( result.ld );
}

