#ident	"lib/libsc/lib/errputs.c:  $Revision: 1.11 $"

#include <arcs/io.h>

void _errputc(char);

/*
 * Worst-case output function without formatting
 */
void
_errputs(cp)
register char *cp;
{
	register char c;

	while (c = *cp++) {
		if (c == '\n')
			_errputc('\r');
		_errputc(c);
	}
}

/*
 * Primitive output routine in case all else fails
 * This is the libsc version that just writes to the prom.
 *
 * The libsk version is in libsk/lib/errputc.c.
 */
void
_errputc(char c)
{
    ULONG dummy;

    (void)Write(StandardOut, &c, 1, &dummy);
}
