/*
 *  IP28 memory test interrupt handler
 *
 *  make sure that the memory subsystem is running in the appropriate
 *  mode if the user interrupts a memory test that is in progress.
 *
 *  ide subsystem invokes the command "trap" if it is defined, and we
 *  have that command point here.
 */

#ident "$Id: trap.c,v 1.1 1996/05/20 20:39:00 poist Exp $"

#include <sys/cpu.h>
#include "uif.h"

/*ARGSUSED*/
int
trap_memtests(int argc, char *argv[] )
{
#ifdef	IP28
	ip28_ecc_correct();
	ip28_enable_eccerr();
#endif	/* IP28 */

	return 0;
}
