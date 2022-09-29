/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/mlock.c	1.3"
#ifdef __STDC__
	#pragma weak mlock = _mlock
#endif
#include "synonyms.h"
#include "errno.h"
#include <unistd.h>
#include <sys/lock.h>
#include <sys/mman.h>
#include "proc_extern.h"

/*
 * Function to lock address range in memory.
 */

int
mlock(const void *addr, size_t len)
{
	if (pagelock((void *) addr, len, PGLOCK) < 0) {
		/* This error check is need for Posix.1b compliance */
		if (oserror() == EINVAL)
			setoserror(ENOMEM);
		return -1;
	}  

	return 0;
}
