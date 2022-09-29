/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/mlockall.c	1.3"
#ifdef __STDC__
	#pragma weak mlockall = _mlockall
#endif
#include "synonyms.h"
#include "errno.h"
#include <unistd.h>
#include <sys/lock.h>
#include <sys/mman.h>
#include "proc_extern.h"

/*
 * Function to lock address space in memory.
 */

int
mlockall(int flags)
{
	if ((flags == 0) || (flags & ~(MCL_CURRENT | MCL_FUTURE))) {
		setoserror(EINVAL);
		return -1;
	}
  
	if (flags & MCL_CURRENT) {
		if (pagelock(0, 0, PGLOCKALL) != 0)
			return -1;
	}
 
	if (flags & MCL_FUTURE)
		(void) pagelock(0, 0, FUTURELOCK);

	return 0;
}
