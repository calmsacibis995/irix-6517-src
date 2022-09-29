/*
 * $Revision: 1.15 $
 */

/* Copyright (c) 1982 Regents of the University of California */

#ifdef __STDC__
	#pragma weak BSDclosedir = _BSDclosedir
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
#include <sys/dir.h>
#include <unistd.h>	/* for prototyping */
#include <stdlib.h>	/* for prototyping */
#include "mplib.h"

/*
 * close a directory.
 */
void
BSDclosedir(dirp)
	register DIR *dirp;
{
	LOCKDECLINIT(l, LOCKDIR);
	(void)close(dirp->dd_fd);
	free((char *)dirp);
	UNLOCKDIR(l);
}
