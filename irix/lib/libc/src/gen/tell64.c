/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/tell.c	1.9"
/*	3.0 SID #	1.2	*/
/*LINTLIBRARY*/
/*
 * return offset in file.
 */
#ifdef __STDC__
	#pragma weak tell64 = _tell64
#endif
#include "synonyms.h"
#include <sys/types.h>		/* for prototyping */
#include <unistd.h>		/* for prototyping */

off64_t
tell64(f)
int	f;
{
	return(lseek64(f, 0LL, SEEK_CUR));
}
