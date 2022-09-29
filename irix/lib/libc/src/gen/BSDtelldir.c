/*
 * $Revision: 1.15 $
 */

/* Copyright (c) 1982 Regents of the University of California */

#ifdef __STDC__
	#pragma weak BSDtelldir = _BSDtelldir
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

#include <sys/param.h>
#include <sys/dir.h>
#include <sys/dirent.h>
#include <sys/types.h>		/* for prototyping */
#include <unistd.h>		/* for prototyping */
#include "mplib.h"

/*
 * return a pointer into a directory
 */
long
BSDtelldir(dirp)
	register DIR *dirp;
{
	struct dirent *dp;
	LOCKDECLINIT(l, LOCKDIR);


	if (lseek(dirp->dd_fd, 0, 1) == 0) {	/* if at beginning of dir */
		UNLOCKDIR(l);
		return(0);			/* return 0 */
	}
	dp = (struct dirent *)&dirp->dd_buf[dirp->dd_loc];
	UNLOCKDIR(l);
	return((long)dp->d_off);
}
