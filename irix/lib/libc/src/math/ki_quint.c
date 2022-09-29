#include <stdio.h>
#include <inttypes.h>
#include "quad.h"
#include <sgidefs.h>

extern	int64_t	__ki_qint(double, double);

extern	double	fabs(double);
#pragma intrinsic (fabs)

	/* intrinsic KIQUINT */

	/* by value only */

	/* converts a long double to a uint64_t */

static const	ldu	twop64 =
{
{0x43f00000,	0x00000000,
 0x00000000,	0x00000000},
};

static const	du	twop63 =
{0x43e00000,	0x00000000};

static const	du	infinity =
{0x7ff00000,	0x00000000};

uint64_t
__ki_quint( uhi, ulo )
double	uhi, ulo;
{
ldquad	x;
uint64_t result;

#include "msg.h"

	x.q.hi = uhi;
	x.q.lo = ulo;

	if ( x.q.hi != x.q.hi )
	{
		return ( (uint64_t)x.q.hi );
	}

	if ( fabs(uhi) == infinity.d )
		return ( (uint64_t)uhi );

	if ( (uhi < twop63.d) ||
	     ((uhi == twop63.d) && (ulo < 0.0))
	   )
	{
		result = (uint64_t)__ki_qint(uhi, ulo);

		return ( result );
	}

	x.ld -= twop64.ld;

	result = (uint64_t)__ki_qint(x.q.hi, x.q.lo);

	return ( result );
}

