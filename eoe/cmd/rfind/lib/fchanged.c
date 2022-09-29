#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "fsdump.h"

/*
 * Determine if a file has changed since the fsdump was taken.
 * Look for the "significant" changes that suggest the data
 * contents of the file changed.  Ignore access time, ...
 */

int fchanged (inod_t *ip, char *pnam, char *mntpt) {
	struct stat64 statbuf;
	char pathname[PATH_MAX];

	if (pnam != NULL) {
		strcpy (pathname, pnam);
	} else {
		pathname[0] = 0;
		if (strcmp (mntpt, "/") != 0)
			strcpy (pathname, mntpt);
		strcat (pathname,  path (PLNK (mh, ip, 0)));
	}
#if 0
printf ("fchanged (%d) <%s> ==> ", ip->i_xino, pathname);
#endif
	if (
	    lstat64 (pathname, &statbuf) == 0
	 &&
	    ip->i_mode != 0
	 &&
	    statbuf.st_mode != 0
	 &&
	    ip->i_mtime == statbuf.st_mtime
	 &&
	    ip->i_ctime == statbuf.st_ctime
	 &&
	    ip->i_size == statbuf.st_size
	 &&
	    ((ip->i_mode) & S_IFMT) == ((statbuf.st_mode) & S_IFMT)
	) {
		switch ((ip->i_mode) & S_IFMT) {
		case S_IFSOCK:
		case S_IFCHR:
		case S_IFBLK:
			if (ip->i_rdev == statbuf.st_dev)
				return 0;		/* [bc]dev not changed */
			break;
		default:
#if 0
printf("FALSE\n");
#endif
			return 0;			/* other file not changed */
		}
	}

#if 0
printf("TRUE\n");
#endif
	return 1;					/* changed */
}
