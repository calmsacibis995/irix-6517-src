/* 
 * Auxiliary C library -- dup2 - attempts to allocate a specific descriptor
 * closing the current one if necessary.
 * BERKNET extension
 *
 * error = dup2(oldd,newd)
 * error = -1 if an error occurs
 */

#include "../inc/config.h"
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>

extern errno;

int
xylo_dup2(oldd,newd)

int oldd, newd;
{

	int desc;

	/*
	 * Do a few checks for legit arguments.
	*/

	if ((oldd < 0 || oldd >= _NFILE) || (newd < 0 || newd >= _NFILE)) {
		errno = EBADF;
		return(-1);
	}

	/*
	 * Check special case where newd equals oldd.  Not an error a noop.
	 */

	if (oldd == newd) {
		return(0);
	}
	
	/*
	 * First try closing newd.  Then use fcntl() to match desired one.  
	 * Else something is wrong, just return an error.
	 */

	if (close(newd) == -1) {

		/* 
		 * 	Close failed because it wasn't open.
		 * 	Reset errno to 0 ignoring this one.
		 */

		errno=0;
	}

	if ((desc = fcntl(oldd,F_DUPFD,newd)) == newd) {

		/*
		 *	Success.
		 */

		return(0);
	}

	/*
	 *	Make sure an errno is set.  This could happen if
	 *	the close failed above and we were allocated the
	 *	next higher descriptor instead of the desired one.
	 */

	close(desc);

	if (!errno)
	  errno = EBADF;

	return(-1);
}
