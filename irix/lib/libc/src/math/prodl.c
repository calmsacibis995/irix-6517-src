#include <inttypes.h>
#include "quad.h"

/* Computes the long double product of two doubles.
 * Assume args are finite and products do not overflow
 * or underflow.
 */

/* const1 is 1.0 + 2^(53 - 53/2), i.e. 1.0 + 2^27 */

static	const du	const1 =
{0x41a00000,	0x02000000};

long double
_prodl( x, y )
double	x, y;
{
ldquad	u;
double	p, hx, tx;
double	hy, ty;

	p = x*const1.d;

	hx = (x - p) + p;
	tx = x - hx;

	p = y*const1.d;

	hy = (y - p) + p;
	ty = y - hy;

	u.q.hi = x*y;
	u.q.lo = (((hx*hy - u.q.hi) + hx*ty) + hy*tx) + tx*ty;

	return ( u.ld );
}

