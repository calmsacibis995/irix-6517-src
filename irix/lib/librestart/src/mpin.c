/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, Silicon Graphics Computer Systems.  All | */
/* | Rights Reserved.  This software contains proprietary and	    | */
/* | confidential information of MIPS and its suppliers.  Use,	    | */
/* | disclosure or reproduction is prohibited without the prior	    | */
/* | express written consent of Silicon Graphics Computer Systems.  | */
/* ------------------------------------------------------------------ */

/*LINTLIBRARY*/

#ifdef __STDC__
	#pragma weak mpin = _mpin
	#pragma weak munpin = _munpin
#endif
#include "synonyms.h"
#include <unistd.h>
#include <sys/lock.h>
#include "proc_extern.h"

/*
 * Lock pages referenced by (addr, len) into memory.
 * Returns -1 on error, 0 otherwise.
 */
int
mpin(void *addr, size_t len)
{
	return (pagelock(addr, len, PGLOCK));
}


/*
 * Unlocks pages referenced by (addr, len).
 * Returns -1 on error, 0 otherwise.
 */
int
munpin(void *addr, size_t len)
{
	return (pagelock(addr, len, UNLOCK));
}
