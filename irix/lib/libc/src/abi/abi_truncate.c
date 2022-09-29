/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*LINTLIBRARY*/
/*********************************************************
 * ftruncate() and truncate() set a file to a specified
 * length using fcntl(F_FREESP) system call. If the file
 * was previously longer than length, the bytes past the
 * length will no longer be accessible. If it was shorter,
 * bytes not written will be zero filled.
 */
#ident	"$Id: abi_truncate.c,v 1.2 1994/10/11 21:23:08 jwag Exp $"

#ifdef __STDC__
	#pragma weak ftruncate = _ftruncate
	#pragma weak truncate = _truncate
#endif
#include "synonyms.h"
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

int
ftruncate(int fildes, off_t len)
{
	struct flock lck;

	lck.l_whence = 0;	/* offset l_start from beginning of file */
	lck.l_start = len;
	lck.l_type = F_WRLCK;	/* setting a write lock */
	lck.l_len = 0L;		/* until the end of the file address space */

	if(fcntl(fildes, F_FREESP, &lck) == -1){
		return(-1);
	}
	else
		return(0);
}

int
truncate(const char *path, off_t len)
{

	register fd;


	if((fd = open(path, O_WRONLY)) == -1){
		return(-1);
	}

	if(ftruncate(fd, len) == -1){
		close(fd);
		return(-1);
	}

	close(fd);
	return(0);
}
