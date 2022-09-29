/*
 * $Revision: 1.14 $
 */

/* Copyright (c) 1982 Regents of the University of California */

#ifdef __STDC__
	#pragma weak BSDopendir = _BSDopendir
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

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <fcntl.h>		/* for prototyping */
#include <unistd.h>		/* for prototyping */
#include <stdlib.h>		/* for prototyping */

/*
 * open a directory.
 */
DIR *
BSDopendir(const char *name)
{
	register DIR *dirp;
	register int fd;	/* file descriptor fo read */
	struct stat sbuf;

	/*
	 * The BSD kernel namei routine treats "" as "."
	 */
	if ( (fd = open(*name ? name : ".", 0) ) < 0 )
		return NULL;

	if ( (fstat( fd, &sbuf ) < 0)
	  || ((sbuf.st_mode & S_IFMT) != S_IFDIR)
	  || ((dirp = (DIR *)malloc( sizeof(DIR) )) == NULL)
	   )	{
		if ((sbuf.st_mode & S_IFMT) != S_IFDIR)
			_setoserror(ENOTDIR);
		(void)close( fd );
		return NULL;		/* bad luck today */
		}

	dirp->dd_fd = fd;
	dirp->dd_loc = dirp->dd_size = 0;	/* refill needed */

	return dirp;
}
