#ident	"lib/libsc/lib/btoa.c:  $Revision: 1.4 $"
#include "libsc.h"

/*
 * btoa.c - convert binary to ascii string
 */

char * 
btoa (int val)
{
    static char buff[16];
    char *cp;
    int minus = 0;
    int rem;

    if (val < 0) {
	minus = 1;
	val = -val;
    }

    cp = &buff[15];
    *cp-- = '\0';

    do {
	rem = val % 10;
	*cp-- = (char)('0' + rem);
	val /= 10;
    } while (val);

    if (minus)
	*cp-- = '-';

    return ++cp;
}
