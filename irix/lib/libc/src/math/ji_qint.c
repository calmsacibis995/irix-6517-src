#include <stdio.h>
#include <inttypes.h>
#include "quad.h"
#include <sgidefs.h>

extern	long double	__qmodf(long double, long double *);

extern	double	fabs(double);
#pragma intrinsic (fabs)

static const	du	infinity =
{0x7ff00000,	0x00000000};

	/* intrinsic JIQINT */

	/* by value only */

	/* converts a long double to a int32_t */

int32_t
__ji_qint( uhi, ulo )
double	uhi, ulo;
{
ldquad	x, xi;

#include "msg.h"

	x.q.hi = uhi;
	x.q.lo = ulo;

	if ( x.q.hi != x.q.hi )
	{
		return ( (int32_t)x.q.hi );
	}

	if ( fabs(uhi) == infinity.d )
		return ( (int32_t)uhi );

	(void) __qmodf(x.ld, &xi.ld);

	return ( (int32_t)xi.q.hi );
}

