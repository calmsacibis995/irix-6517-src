/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:stdio/fpos.c	1.2"
/*LINTLIBRARY*/
#ifdef __STDC__
	#pragma weak fgetpos64 = _fgetpos64
	#pragma weak fsetpos64 = _fsetpos64
#endif

#include "synonyms.h"
#include <sys/types.h>
#include <stdio.h>

int
fgetpos64(FILE *stream, fpos64_t *pos)
{
	if ((*pos = (fpos64_t)ftell64(stream)) == -1LL)
		return(-1);
	return(0);
}

int
fsetpos64(FILE *stream, const fpos64_t *pos)
{
	if (fseek64(stream, (off64_t)*pos, SEEK_SET) != 0)
		return(-1);
	return(0);
}
