/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.5 $"


#include <sys/stat.h>
#include <sys/syssgi.h>
#include <sys/dmi_kern.h>
#include <sys/systeminfo.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


/* Originally this routine called SGI_OPEN_BY_HANDLE on the target object, did
   a fstat on it, then searched for the matching inode number in the directory
   pointed to by dirhanp.  There were a couple of problems with this.
   1. dm_handle_to_path is supposed to work for symlink target objects, but
      didn't because SGI_OPEN_BY_HANDLE only works for files and directories.
   2. The wrong pathname was sometimes returned if dirhanp and targhanp pointed
      to the same directory (a common request) because ".", ".." and/or various
      subdirectories could all be mount points and therefore all have the same
      inode number of 128.
   3. dm_handle_to_path wouldn't work if targhanp was a mount point because
      routine getcomp() sees only the mounted-on directory, not the mount point.

   This rework of dm_handle_to_path fixes all these problems, but at a price,
   because routine getcomp() must make one system call per directory entry.
   Someday these two methods should be combined.  If an SGI_OPEN_BY_HANDLE of
   targhanp works and if both dirhanp and targhanp have the same dev_t, then
   use the old method, otherwise use the current method.  This will remove
   the system call overhead in nearly all cases.
*/

static int getcomp(int dirfd, void *targhanp, size_t targhlen,
			char *bufp, size_t buflen, size_t *rlenp);


extern int
dm_handle_to_path(
	void		*dirhanp,	/* parent directory handle and length */
	size_t		dirhlen,
	void		*targhanp,	/* target object handle and length */
	size_t		targhlen,
	size_t		buflen,		/* length of pathbufp */
	char		*pathbufp,	/* buffer in which name is returned */
	size_t		*rlenp)		/* length of resultant pathname */
{
	int		dirfd = -1;	/* fd for parent directory */
	int		origfd = -1;	/* fd for current working directory */
	int		err;		/* a place to save errno */

	if (buflen == 0) {
		errno = EINVAL;
		return -1;
	}
	if (pathbufp == NULL || rlenp == NULL) {
		errno = EFAULT;
		return -1;
	}
	if ((origfd = open(".", O_RDONLY)) < 0)
		return -1;	/* leave errno set from open */

	dirfd = (int)syssgi(SGI_OPEN_BY_HANDLE, dirhanp, dirhlen, O_RDONLY);
	if (dirfd < 0) {
		err = errno;
	} else if (fchdir(dirfd)) {
		err = errno;
	} else {
		/* From here on the fchdir must always be undone! */

		if (!getcwd(pathbufp, buflen)) {
			if ((err = errno) == ERANGE)	/* buffer too small */
				err = E2BIG;
		} else {
			err = getcomp(dirfd, targhanp, targhlen, pathbufp,
					buflen, rlenp);
		}
		(void) fchdir(origfd);	/* can't do anything about a failure */
	}

	if (origfd >= 0)
		(void)close(origfd);
	if (dirfd >= 0)
		(void)close(dirfd);
	if (!err)
		return(0);

	if (err == E2BIG)
		*rlenp = 2 * buflen;	/* guess since we don't know */
	errno = err;
	return(-1);
}


/* Append the basename of the open file referenced by targfd found in the
   directory dirfd to dirfd's pathname in bufp.  The length of the entire
   path (including the NULL) is returned in *rlenp.

   Returns zero if successful, an appropriate errno if not.
*/

#define READDIRSZ	16384	/* 6.x kernels use 16k buffer */

static int
getcomp(
	int		dirfd,
	void		*targhanp,
	size_t		targhlen,
	char		*bufp,
	size_t		buflen,
	size_t		*rlenp)
{
	char		buf[READDIRSZ];	/* directory entry data buffer */
	int		loc = 0;	/* byte offset of entry in the buffer */
	int		size = 0;	/* number of bytes of data in buffer */
	int		eof = 0;	/* did last ngetdents exhaust dir.? */
	dirent64_t	*dp;		/* pointer to directory entry */
	char		hbuf[DM_MAX_HANDLE_SIZE];
	size_t		hlen;
	size_t		dirlen;		/* length of dirfd's pathname */
	size_t		totlen;		/* length of targfd's pathname */
	dm_ino_t	ino;		/* target object's inode # */

	if (dm_handle_to_ino(targhanp, targhlen, &ino))
		return -1;      /* leave errno set from dm_handle_to_ino */

	/* Append a "/" to the directory name unless the directory is root. */

	dirlen = strlen(bufp);
	if (dirlen > 1) {
		if (buflen < dirlen + 1 + 1)
			return(E2BIG);
		bufp[dirlen++] = '/';
	}

	/* Examine each entry in the directory looking for one with a
	   matching target handle.
	*/

	for(;;) {
		if (size > 0) {
			dp = (dirent64_t *)&buf[loc];
			loc += dp->d_reclen;
		}
		if (loc >= size) {
			if (eof) {
				return(ENOENT);
			}
			loc = size = 0;
		}
		if (size == 0) {	/* refill buffer */
			size = ngetdents64(dirfd, (dirent64_t *)buf,
				    sizeof(buf), &eof);
			if (size == 0)	{	/* This also means EOF */
				return(ENOENT);
			}
			if (size < 0) {		/* error */
				return(errno);
			}
		}
		dp = (dirent64_t *)&buf[loc];

		if (dp->d_ino != ino)
			continue;	/* wrong inode; try again */
		totlen = dirlen + strlen(dp->d_name) + 1;
		if (buflen < totlen)
			return(E2BIG);
		(void)strcpy(bufp + dirlen, dp->d_name);

		if (dmi(DM_PATH_TO_HANDLE, bufp, hbuf, &hlen))
			continue;	/* must have been removed/renamed */
		if (!dm_handle_cmp(targhanp, targhlen, hbuf, hlen))
			break;
	}

	/* We have a match based upon the target handle.  Clean up the end
	   cases before returning the path to the caller.
	*/

	if (!strcmp(dp->d_name, ".")) {		/* the directory itself */
		if (dirlen > 1)
			dirlen--;
		bufp[dirlen] = '\0';		/* trim the trailing "/." */
		*rlenp = dirlen + 1;
		return(0);
	}
	if (!strcmp(dp->d_name, "..")) {	/* the parent directory */
		char	*slash;

		if (dirlen > 1)
			dirlen--;
		bufp[dirlen] = '\0';
		if ((slash = strrchr(bufp, '/')) == NULL)
			return(ENXIO);		/* getcwd screwed up */
		if (slash == bufp)		/* don't whack "/" */
			slash++;
		*slash = '\0';			/* remove the last component */
		*rlenp = strlen(bufp) + 1;
		return(0);
	}

	*rlenp = totlen;		/* success! */
	return(0);
}
