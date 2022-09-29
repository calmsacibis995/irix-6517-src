/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, Silicon Graphics Computer Systems.  All | */
/* | Rights Reserved.  This software contains proprietary and	    | */
/* | confidential information of MIPS and its suppliers.  Use,	    | */
/* | disclosure or reproduction is prohibited without the prior	    | */
/* | express written consent of Silicon Graphics Computer Systems.  | */
/* ------------------------------------------------------------------ */

/*LINTLIBRARY*/

#ifdef __STDC__
	#pragma weak memcntl = _memcntl
#endif
#include "synonyms.h"
#include <errno.h>
#include <sys/types.h>
#include <sys/mman.h>

/*
 * The function memcntl(2) is not supported, but according to the ABI
 * the entry point for this function must be present in the library.
 * This stub sets the global variable errno to the value ENOSYS and
 * returns -1 when called.
 */

/* ARGSUSED */
int
memcntl(const void *addr, size_t len, int cmd, void *arg, int attr, int mask)
{
	setoserror(ENOSYS);
	return (-1);
}

