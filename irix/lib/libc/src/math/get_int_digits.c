#include <sgidefs.h>
#include <values.h>
#include "synonyms.h"

#define _IEEE 1
#include "fpparts.h"
#undef 	_IEEE

static const dblu tenth = {0x3fb99999, 0x9999999a}; /* .1 */

static const dblu magic = {0x43300000, 0x00000000};

/* ====================================================================
 *
 * FunctionName: _get_int_digits
 *
 * Description: calculates the digits of argument dm right to left; the
 *		successive digits of dm are computed as dm%10 followed
 *		by dm /= 10.  Floating point arithmetic is used for speed.
 *		Note that dm must be an integer, i.e. fractional part = 0,
 *		and 0 < dm < 0x433
 *
 * ====================================================================
 */

void
_get_int_digits( double dm, __int32_t k, char *buffer )
{
__int32_t	i;
double	d;


	buffer[k] = '\0';

	for ( i=1; i<=k; i++ )
	{
		d = dm*tenth.d; /* compute dm/10.0 approximately */

		d += magic.d; /* truncate fractional part of d */
		d -= magic.d;

		buffer[k-i] = ((__int32_t)(dm - d*10.0)) + '0'; /* dm%10 */
		dm = d;
	}
}

