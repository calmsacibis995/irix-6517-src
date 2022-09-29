/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/seekdir.c	1.9"
/*
	seekdir -- C library extension routine

*/

#ifdef __STDC__
	#pragma weak seekdir64 = _seekdir64
#endif
#include	"synonyms.h"
#include	"mplib.h"
#include	<sys/types.h>
#include	<unistd.h>		/* for prototyping */
#include	<dirent.h>

/* parameter "dirp" is a stream from opendir() */
/* parameter "loc" is the position from telldir(), 0 if rewinddir() */

void
seekdir64(register DIR *dirp, off64_t loc)
{

	LOCKDECLINIT(l, LOCKDIR);
	 
	/**
	 ** This is no optimization - telldir itself does
	 ** an lseek. Also buggy, since telldir assumes that
	 ** dirp->dd_fd and data in dirp->dd_buf are in sync -
	 ** not neccessarily so in POSIX-conforming applications
	 ** using rewinddir()
	if (telldir(dirp) == loc)
		return;
	 **/
	lseek64(dirp->dd_fd, loc, 0);
	dirp->dd_loc = 0;
	dirp->dd_size = 0;
	dirp->__dd_flags = 0;	/* clear eof as well as 32/64 mode */
	UNLOCKDIR(l);
}
