#ident	"lib/libsk/lib/errputs.c:  $Revision: 1.8 $"

#include <arcs/io.h>
#include <libsc.h>
#include <libsk.h>
#include <alocks.h>


/*
 * Worst-case output function without formatting
 */
void
_errputs(char *cp)
{
	register char c;

	LOCK_ARCS_UI();
	while (c = *cp++) {
		if (c == '\n')
			_errputc('\r');
		_errputc(c);
	}
	UNLOCK_ARCS_UI();
}

/*
 * Primitive output routine in case all else fails
 * This is the libsk version that tries to do I/O, first
 * through graphics, then through a cpu-dependent errputc
 * function.
 *
 * The libsc version is in libsc/lib/errputc.c.
 */
void
_errputc(char c)
{
	/*
	 * If the graphics port is enabled, then print the character on it.
	 *
	 */
	if (console_is_gfx()) {
                txPrint(&c, 1);
	} else {
		cpu_errputc(c);
	}
}

