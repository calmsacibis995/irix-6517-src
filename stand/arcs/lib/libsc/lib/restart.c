#ident "$Revision: 1.8 $"

/* These are the ARCS functions that MUST call back to the real 
 * firmware vector to execute correctly.  They are in a separate
 * file from the other ARCS functions so programs that link with
 * libsk can get these prom entrypoints, but retain their own
 * ARCS function entrypoints instead of going through the firmware
 * vector.
 */

#include <sys/types.h>
#include <arcs/spb.h>
#include <arcs/restart.h>

extern void	__close_fds(void);

/*  This file will not be included in the prom, but will be included by
 * other libsk programs since Halt, etc are defined in IPXXprom/csu.s.
 * For the prom, _prom is baked in .data as 1.
 */
int _prom;			/* .BSS -> 0 value */

void
Halt(void)
{
	__TV->Halt();
}

void
PowerDown(void)
{
	__TV->PowerDown();
}

void
Restart(void)
{
	__TV->Restart();
}

void
Reboot(void)
{
	__TV->Reboot();
}
