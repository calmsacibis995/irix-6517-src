#ident "$Revision: 1.9 $"
/*
 * BSD compatibility routine
 *
 * Use 'fcntl' to implement 'dup2'
 */

#ifdef __STDC__
#ifdef LIBBSD
	#pragma weak dup2 = _lbsd_dup2
#else
	#pragma weak BSDdup2 = _BSDdup2
#endif
#endif
#include "synonyms.h"

#include <fcntl.h>		/* prototype for fcntl() */
#include <errno.h>
#include <unistd.h>		/* prototype for close() */

/*
 * dup2 returns a new descriptor with value 'newd' that is
 *	a dup of 'oldd'.
 */
int					/* return 0 or <0 */
#ifdef LIBBSD
dup2(oldd, newd)
#else
BSDdup2(oldd, newd)
#endif
int oldd, newd;
{
	register int dupd;

	if (oldd == newd)		/* quit if nothing to do */
		return(0);

	/*
	 * Close the desired descriptor first to ensure that it
	 * is available (it may already be available, so errors
	 * are ignored).
	 */
	close(newd);

	/*
	 * fcntl F_DUPFD clones 'oldd' and uses the first available file
	 * descriptor greater than or equal to the third argument.
	 */
	if ((dupd = fcntl(oldd, F_DUPFD, newd)) < 0)
		return(dupd);

	/*
	 * Make sure that we got the right one
	 */
	if (dupd != newd) {
		close(dupd);
		_setoserror(EINVAL);
		return(-1);
	}

	return(0);
}
