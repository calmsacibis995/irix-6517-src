#ident "$Revision: 1.1 $"

/* These are the ARCS functions that MUST call back to the real 
 * firmware vector to execute correctly.  They are in a separate
 * file from the other ARCS functions so programs that link with
 * libsk can get these prom entrypoints, but retain their own
 * ARCS function entrypoints instead of going through the firmware
 * vector.
 *
 * EnterInteractiveMode is seperate from restart.c as some systems
 * override this call for various reasons (like IP30 MP issues).
 */

#include <sys/types.h>
#include <arcs/spb.h>
#include <arcs/restart.h>

void
EnterInteractiveMode(void)
{
	__TV->EnterInteractiveMode();
}
