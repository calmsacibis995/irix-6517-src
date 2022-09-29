/*
 * $Revision: 1.19 $
 */

/* Copyright (c) 1982 Regents of the University of California */

#ifdef __STDC__
	#pragma weak BSDreaddir = _BSDreaddir
#endif
#include "synonyms.h"

/*
 * The following are redefined in sys/dir.h
 */
#undef opendir
#undef readdir
#undef telldir
#undef seekdir
#undef closedir
#undef scandir
#undef alphasort

#include <sys/types.h>
#include <sys/param.h>
#include <sys/dir.h>
#include <sys/dirent.h>
#include <string.h>		/* for prototyping */
#include "mplib.h"

/*
 * Use 'getdents' system call to read a directory and present it
 * to the caller as if it were a 4.2 style directory.
 */

/*
 * Return next entry in a directory.
 */
struct direct *
BSDreaddir(dirp)
	register DIR *dirp;
{
	register struct dirent *dp;	/* struct returned by getdents */
	register struct direct *retp;   /* BSD4.2 directory struct */
	long saveloc = 0;
	LOCKDECLINIT(l, LOCKDIR);

	if (dirp->dd_size > 0) {
		dp = (struct dirent *)&dirp->dd_buf[dirp->dd_loc];
		saveloc = dirp->dd_loc;   /* save for possible EOF */
		dirp->dd_loc += dp->d_reclen;
	}
	if (dirp->dd_loc >= dirp->dd_size)
		dirp->dd_loc = dirp->dd_size = 0;

	if (dirp->dd_size == 0 	/* refill buffer */
	    && (dirp->dd_size = getdents(dirp->dd_fd,
					(struct dirent *)dirp->dd_buf,
					sizeof dirp->dd_buf)) <= 0) {
		if (dirp->dd_size == 0)	/* This means EOF */
			dirp->dd_loc = saveloc;  /* EOF so save for telldir */
		UNLOCKDIR(l);
		return NULL;	/* error or EOF */
	}

	dp = (struct dirent *)&dirp->dd_buf[dirp->dd_loc];
	/*
	|| copy the data from getdents buffer to the 
	|| BSD struct data
	*/
	retp = &dirp->dd_direct;
	retp->d_ino = (__bsdino)dp->d_ino;

	/*
	|| Names returned by getdents are guaranteed null terminated
	*/
	retp->d_namlen = (u_short) strlen(dp->d_name);
	strcpy(retp->d_name, dp->d_name);
	retp->d_reclen = (u_short) DIRSIZ(retp);
	UNLOCKDIR(l);
	return (retp);
}
