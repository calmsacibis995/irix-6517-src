/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/munlockall.c	1.3"
#ifdef __STDC__
	#pragma weak munlockall = _munlockall
#endif
#include "synonyms.h"
#include "errno.h"
#include <unistd.h>
#include <sys/lock.h>
#include <sys/mman.h>
#include "proc_extern.h"

/*
 * Function to unlock address space from memory.
 */

int
munlockall(void)
{
	return (pagelock(0, 0, UNLOCKALL));
}
