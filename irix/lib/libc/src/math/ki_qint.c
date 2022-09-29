#include <stdio.h>
#include <inttypes.h>
#include "quad.h"
#include <sgidefs.h>

extern	long double	__qmodf(long double, long double *);

extern	double	fabs(double);
#pragma intrinsic (fabs)

	/* intrinsic KIQINT */

	/* by value only */

	/* converts a long double to a int64_t */

static const	ldu	m_twop63 =
{
{0xc3e00000,	0x00000000,
 0x00000000,	0x00000000},
};

static const	ldu	twop63 =
{
{0x43e00000,	0x00000000,
 0x00000000,	0x00000000},
};

static const	du	twop62 =
{0x43d00000,	0x00000000};

static const	du	n_twop63 =
{0xc3e00000,	0x00000000};

static const	du	infinity =
{0x7ff00000,	0x00000000};

int64_t
__ki_qint( uhi, ulo )
double	uhi, ulo;
{
ldquad	x, xi;
int64_t	t;

#include "msg.h"

	x.q.hi = uhi;
	x.q.lo = ulo;

	if ( x.q.hi != x.q.hi )
	{
		return ( (int64_t)x.q.hi );
	}

	if ( fabs(uhi) == infinity.d )
		return ( (int64_t)uhi );

	(void) __qmodf(x.ld, &xi.ld);

	if ( (xi.ld < m_twop63.ld) || (twop63.ld <= xi.ld) )
	{
		/* we get overflow here, so just let the hardware do it */

		if ( xi.q.hi == n_twop63.d )
		{
			xi.q.hi *= 2.0;
		}

		return ( (int64_t)xi.q.hi );
	}

	if ( fabs(xi.q.hi) > twop62.d )
	{
		/* add things up very carefully to avoid overflow */

		t = (int64_t)(0.5*xi.q.hi);
		return ( ((int64_t)xi.q.lo) + t + t );
	}

	/* just do it */

	return ( (int64_t)xi.q.hi + (int64_t)xi.q.lo );
}

