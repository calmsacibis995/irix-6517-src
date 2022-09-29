/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/fattach.c	1.4"
/*
 * Attach a STREAMS-based file descriptor to an object in the file 
 * system name space.
 */
#ifdef __STDC__
	#pragma weak fattach = _fattach
#endif
#include "synonyms.h"
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <namefs/namenode.h>
#include <sys/mount.h>
#include "gen_extern.h"

int
fattach(int fildes, char *path)
{
	struct namefd  namefdp;

	switch ( isastream( fildes ) ) {
	case 0:
		setoserror(EINVAL);
		return( -1 );
	case 1:
		namefdp.fd = fildes;
		return (mount((char *)NULL, path, MS_DATA,(const char *)"namefs", 
			(char *)&namefdp, sizeof(struct namefd)));
	default:
		return( -1 );
	}
}
