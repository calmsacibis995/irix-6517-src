/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/mkfifo.c	1.4"
/*
 * mkfifo(3c) - create a named pipe (FIFO). This code provides
 * a POSIX mkfifo function.
 *
 */

#ifdef __STDC__
	#pragma weak mkfifo = _mkfifo
#endif
#include "synonyms.h"
#include <sys/types.h>
#include <sys/stat.h>

int
mkfifo(const char *path, mode_t mode)
{
	mode &= 0777;		/* only allow file access permissions */
	mode |= S_IFIFO;	/* creating a FIFO 		      */
	return(mknod(path,mode,0));
}
