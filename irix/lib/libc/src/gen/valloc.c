/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/valloc.c	1.2"

#ifdef __STDC__
	#pragma weak valloc = _valloc
#endif

#include "synonyms.h"
#include <stdlib.h>
#include <unistd.h>

/* move out of function scope so we get a global symbol for use with data cording */
static size_t pagesize;

VOID *
valloc(size)
	size_t size;
{
	if (!pagesize)
		pagesize = (size_t)sysconf(_SC_PAGESIZE);
	return memalign(pagesize, size);
}
