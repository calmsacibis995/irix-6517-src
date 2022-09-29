/*
 * $Revision: 1.13 $
 */

/* Copyright (c) 1982 Regents of the University of California */

#ifdef __STDC__
	#pragma weak BSDseekdir = _BSDseekdir
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
#include <unistd.h>		/* for prototyping */
#include "mplib.h"

/*
 * seek to an entry in a directory.
 * Only values returned by ``telldir'' should be passed to seekdir.
 */
void
BSDseekdir(register DIR *dirp, long loc)
{
	LOCKDECLINIT(l, LOCKDIR);

	/* XXX see seekdir
	if (BSDtelldir(dirp) == loc)
		return;
	*/
	
	/* 
	 * The requested location is not in the current buffer.
	 * Discard the buffer and reposition the file pointer to the
	 * requested location.
	 */
	dirp->dd_loc = 0;
	lseek(dirp->dd_fd, loc, 0);
	dirp->dd_size = 0;
	UNLOCKDIR(l);
}
