/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/telldir.c	1.6"
/*
	telldir -- C library extension routine

*/

#ifdef __STDC__
	#pragma weak telldir = _telldir
#endif
#include	"synonyms.h"
#include	"mplib.h"
#include	<sys/types.h>
#include	<sys/errno.h>
#include	<errno.h>
#include 	<stdlib.h>	/* get malloc() prototype */
#include 	<unistd.h>	/* get lseek() prototype */
#include 	<dirent.h>

/* parameter "dirp" is a stream from opendir() */

off_t
telldir(register DIR *dirp)
{
	dirent_t *dp;
	off_t rval;
	off64_t rv64;
	LOCKDECLINIT(l, LOCKDIR);
						/* if at beginning of dir */
	if ((rv64 = lseek64(dirp->dd_fd, (off64_t)0, SEEK_CUR)) == (off64_t)0) {
		UNLOCKDIR(l);
		return(0);			/* return 0 */
	}
	if (dirp->dd_loc == 0 && dirp->dd_size == 0) {
		UNLOCKDIR(l);
		return((off_t)rv64);
	}
#if _MIPS_SIM == _MIPS_SIM_ABI32
	if (dirp->__dd_flags & _DIR_FLAGS_READDIR32) {
		dp = (dirent_t *)&dirp->dd_buf[ dirp->dd_loc ];
		rval = dp->d_off;
	} else if (dirp->__dd_flags & _DIR_FLAGS_READDIR64) {
		setoserror(EINVAL);
		rval = -1;
	} else {
		rval = 0;
	}
#else
	dp = (dirent_t *)&dirp->dd_buf[ dirp->dd_loc ];
	rval = dp->d_off;
#endif
	UNLOCKDIR(l);
	return(rval);
}
