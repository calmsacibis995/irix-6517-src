/*
 * ckpt_pathname.c
 *
 *      Support routines for generating pathnames for files and memory\
 *	mappings.
 *
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Revision: 1.26 $"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/syssgi.h>
#include <sys/stat.h>
#include <sys/procfs.h>
#include <sys/ckpt_procfs.h>
#include <sys/ckpt_sys.h>
#include <sys/vfs.h>
#include "ckpt.h"
#include "ckpt_internal.h"

#define	AUTOMOUNT	"/tmp_mnt"

/*
 * set/restore effective uid
 */
static int 
change_ids(ckpt_obj_t *co, int set, uid_t *was_ruid, uid_t *was_uid)
{
	int retv = 0;
	uid_t nuid = co->co_psinfo->pr_uid;

	if(set) {
		*was_ruid = getuid();
		*was_uid = geteuid();

		IFDEB1(cdebug("change_ids: ruid= %d, euid %d to %d \n",
				*was_ruid, *was_uid, nuid));
		if(setruid(0) != 0) {
			cerror("can't setruid(0)\n");
			return -1;
		}
		if(seteuid(nuid) != 0) {
			cerror("can't set euid %d\n", nuid);
			setruid(*was_ruid);
			return -1;
		}
	} else { /* restore ids back to original */
		IFDEB1(cdebug("change_ids restore: ruid euid %d %d\n", 
				*was_ruid, *was_uid));
		if(seteuid(*was_uid) != 0) {
			cerror("can't set euid %d\n", *was_uid);
			retv = -1;
		}
		if(setruid(*was_ruid)) {
			cerror("can't set ruid back to %d\n", *was_ruid);
			retv = -1;
		}
	}
	return retv;
}



/*
 * get_pathname
 *
 * Get the pathname for file descriptor fd in the process opened 
 * through /proc on file descriptor procfd
 */
static char *
get_pathname2(ckpt_obj_t *co, int dirfd, ino_t ino, int last_try)
{
	DIR *dirp;
	char *pathname;
	struct dirent *dp;


	pathname = ckpt_get_dirname(co, dirfd);
	if (pathname == NULL)
		return (NULL);
	dirp = opendir(pathname);
	if (dirp == NULL) {
		if(last_try) {
			cerror("Failed to opendir (%s)\n", STRERR);
		}
		return (NULL);
	}
	setoserror(0);

	while (dp = readdir(dirp)) {
		if (dp->d_ino == ino)
			break;
	}
	if (dp == NULL) {
		IFDEB1(cdebug("cannot find ino %d in %s\n", ino, pathname));
		free(pathname);
		closedir(dirp);
		setoserror(ENOENT);
		return (NULL);
	}
	if ((strlen(pathname) + dp->d_reclen + 1) > CPATHLEN) {
		free(pathname);
		closedir(dirp);
		setoserror(ENAMETOOLONG);
		return (NULL);
	}
	pathname = (char *)realloc(pathname, strlen(pathname) + dp->d_reclen + 1);
	if (pathname == NULL) {
		cerror("Failed to realloc mem (%s)\n", STRERR);
		closedir(dirp);
		return(NULL);
	}
	(void) strcat(pathname, "/");
	(void) strcat(pathname, dp->d_name);

	closedir(dirp);

	return(pathname);
}

static char *
get_pathname(ckpt_obj_t *co, int dirfd, ino_t ino)
{
	char *pathname;
	uid_t was_ruid, was_uid;

	if((pathname = get_pathname2(co, dirfd, ino, 0)) == NULL) {
		IFDEB1(cdebug("try to change euid to the process's\n"));
		if(change_ids(co, 1, &was_ruid, &was_uid) <0)
			return NULL;
		/* do it again */
		pathname = get_pathname2(co, dirfd, ino, 1);
		/* restore uid */
		if(change_ids(co, 0, &was_ruid, &was_uid) <0)
			return NULL;

	}
	return pathname;
}

char *
ckpt_get_dirname(ckpt_obj_t *co, int dirfd)
{
	char *cwd, *pathname = NULL;
	uid_t was_ruid, was_uid;
	int res;

	if ((cwd = getcwd(NULL, CPATHLEN + 1)) == NULL) {
		IFDEB1(cdebug("try to change euid to the process's\n"));
		if (change_ids(co, 1, &was_ruid, &was_uid) < 0) {
			cerror("Failed to getcwd (%s)\n", STRERR);
			return NULL;
		} else {
			cwd = getcwd(NULL, CPATHLEN + 1);
			if ((change_ids(co, 0, &was_ruid, &was_uid) < 0) ||
			    (cwd == NULL)) {
				cerror("Failed to getcwd (%s)\n", STRERR);
				return NULL;
			}
		}
	}

	if (fchdir(dirfd) < 0) {
		IFDEB1(cdebug("try to change euid to the process's\n"));
		if (change_ids(co, 1, &was_ruid, &was_uid) < 0) {
			cerror("Failed to fchdir (%s)\n", STRERR);
			goto errout;
		} else {
			res = fchdir(dirfd);
			if ((change_ids(co, 0, &was_ruid, &was_uid) < 0) ||
			    (res < 0 )) {
				cerror("Failed to fchdir (%s)\n", STRERR);
				goto errout;
			}
		}
	}

	if ((pathname = getcwd(NULL, CPATHLEN + 1)) == NULL) {
		IFDEB1(cdebug("try to change euid to the process's\n"));
		if (change_ids(co, 1, &was_ruid, &was_uid) < 0) {
			cerror("Failed to getcwd again (%s)\n", STRERR);
			goto errout;
		} else {
			pathname = getcwd(NULL, CPATHLEN + 1);
			if ((change_ids(co, 0, &was_ruid, &was_uid) < 0) ||
			    (pathname == NULL)) {
				cerror("Failed to getcwd again (%s)\n", STRERR);
				pathname = NULL;
				goto errout;
			}
		}
	}

	(void)chdir(cwd);

errout:
	free(cwd);
	return (pathname);
}

/*
 * ckpt_pathname
 *
 * Returns pathname for open file desriptor.  If the file descriptor
 * references an unlinked file, attempts to return pathname of directory
 * file was found in.
 *
 * returns -1 on error, 1 if unlinked file, 0 otherwise
 */
int 
ckpt_pathname(ckpt_obj_t *co, int fd, char **path)
{
	struct stat statbuf;
	int dirfd, link_dir = 1;
	int rval = 0;

	*path = NULL;

	if (fstat(fd, &statbuf) < 0) {
		cerror("ckpt_pathname: fstat %s)\n", STRERR);
		return (-1);
	}
	IFDEB1(cdebug("ckpt_pathname: fd %d st_mode %x st_nlink %d st_ino %d\n",
		fd, statbuf.st_mode, statbuf.st_nlink, statbuf.st_ino));

	switch (statbuf.st_mode & S_IFMT) {
	case S_IFDIR:
		*path = ckpt_get_dirname(co, fd);
		break;
	case S_IFREG:
	case S_IFIFO:
	case S_IFCHR:
	case S_IFBLK:
		if ((dirfd = (int)syssgi(SGI_CKPT_SYS, CKPT_OPENDIR, fd, 
					O_RDONLY)) < 0) {
			if(statbuf.st_nlink == 0) {
				link_dir = 0;
			} else {
				cerror("ckpt_pathname: CKPT_OPENDIR (%s)\n", 
									STRERR);
				return (-1);
			}
		}
		if((link_dir == 0) && (statbuf.st_nlink == 0)) {
			/* the directory of the unlinked file has been
			 * removed. we use /tmp instead.
			 */
			if((*path = malloc(10)) == NULL) {
				cerror("CKPT_OPENDIR: malloc");
				return -1;
			}
			strcpy(*path, "/tmp");
			IFDEB1(cdebug("dir of unlinked file (fd = %d) deleted, "
				"use %s as dir instead\n", fd, *path));
		} else if (statbuf.st_nlink == 0)
			*path = ckpt_get_dirname(co, dirfd);
		else
			*path = get_pathname(co, dirfd, statbuf.st_ino);
		(void)close(dirfd);
		break;
	default:
		cerror("unknown file mode 0x%x on fd %d\n",
			statbuf.st_mode & S_IFMT, fd);
		return (-1);
	}
	if (*path == NULL) {
		IFDEB1(cdebug("Warning: fd %d (dirfd %d) has no path name\n",
			fd, dirfd));
		return (-1);
	}
	/*
	 * If no links at all, or inode not found in dirctory used to open
	 * the file, mark as unlinked.
	 */
	if (statbuf.st_nlink == 0 || oserror() == ENOENT)
		rval = 1;
	/*
	 * Hack to strip off automount hard path
	 */
	if (strncmp(*path, AUTOMOUNT, strlen(AUTOMOUNT)) == 0) {
		char *src, *dst;

		src = *path + strlen(AUTOMOUNT);
		dst = malloc(strlen(*path) - strlen(AUTOMOUNT) + 1);
		
		strcpy(dst, src);
		free(*path);
		*path = dst;
	}
	return (rval);
}
