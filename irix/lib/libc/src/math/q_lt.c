#include <stdio.h>
#include <inttypes.h>
#include "quad.h"
#include <sgidefs.h>

	/* implements the relational operator < for long doubles */

int32_t
__q_lt( xhi, xlo, yhi, ylo)
double xhi, xlo, yhi, ylo;
{

#include "msg.h"

	if ( xhi < yhi )
		return ( 1 );

	if ( (xhi == yhi) && (xlo < ylo) )
		return ( 1 );

	return ( 0 );
}

