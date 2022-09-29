#include <stdio.h>
#include <inttypes.h>
#include "quad.h"
#include <sgidefs.h>

extern	int64_t	__ki_qint(double, double);

extern	double	fabs(double);
#pragma intrinsic (fabs)

static const	du	infinity =
{0x7ff00000,	0x00000000};

	/* intrinsic IIQUINT */

	/* by value only */

	/* converts a long double to a uint16_t */

uint32_t
__ji_quint( uhi, ulo )
double	uhi, ulo;
{
ldquad	x;
int64_t	n;
uint32_t result;

#include "msg.h"

	x.q.hi = uhi;
	x.q.lo = ulo;

	if ( x.q.hi != x.q.hi )
	{
		return ( (uint32_t)x.q.hi );
	}

	if ( fabs(uhi) == infinity.d )
		return ( (uint32_t)uhi );

	n = __ki_qint(uhi, ulo);

	result = (uint32_t)(n & 0xffffffff);

	return ( result );
}

