/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/_getcwd.c	1.1"


/*LINTLIBRARY*/

#include 	"synonyms.h"
#include 	"shlib.h"
#include        <sys/types.h>
#include        <sys/stat.h>
#include        <stdio.h>
#include        <errno.h>
#include        <dirent.h>
#include        <string.h>
#include        <stdlib.h>
#include        <sys/mac.h>
#include	<sys/mac_label.h>
#include        <unistd.h>
#include	<sys/mount.h>
#include	<signal.h>
#include	<sys/systeminfo.h>
#include	"priv_extern.h"

#define MAX_PATH 1024
#define MAX_NAME 512
#define BUF_SIZE 1536 /* 3/2 of MAX_PATH for /a/a/a... case */

/* 
 * This algorithm does not use chdir.  Instead, it opens a 
 * successive strings for parent directories, i.e. .., ../..,
 * ../../.., and so forth.
 * 
 * Both getcwd() and getwd() call this common routine.  This saves
 * on memory and code maintenance.
 */

static const char err_stat_dot[] = "getwd: can't stat .";
static const char err_open_dotdot[] = "getwd: can't open ..";
static const char err_read_dotdot[] = "getwd: read error in ..";
static short	use_getmountid = 1;

char *
__getcwd(char *str, size_t size, int version)
{
	char dotdots[BUF_SIZE+MAX_NAME];
	struct stat64		cdir;	/* current directory status */
	struct stat64		tdir;
	struct stat64		pdir;	/* parent directory status */
	DIR			*pdfd;	/* parent directory stream */

	dirent64_t *dir;
	char *dotdot = dotdots + BUF_SIZE - 3;
	char *dotend = dotdots + BUF_SIZE - 1; 
	int alloc, saverr, ret;
	size_t maxpwd;
	size_t strsize;
	mac_t label;
        int moldy_process = 0;
	int mac_enabled = 0;
	ino64_t p_entino;
	mountid_t cmountid, tmountid;
	const char *error_string;

	/*
	 * If MAC is not enabled, skip the mac_enabled processing
	 */
	
	if (mac_enabled = ((int)sysconf(_SC_MAC) > 0)) {
        /*
         * Moldy notes:
         * a) the boolean 'moldy_process' indicates whether the process
         *    should see moldy directories.
         * b) if .. is moldy, and 'moldy_process' is FALSE, don't print .'s
         *    name.
         */
		label = mac_get_proc ();
		if (label == NULL) {
			perror("pwd");
                	exit(1);
		}
        	if (mac_is_moldy(label)) {
                	moldy_process = 1;
        	}
		mac_free(label);
	}
	
	/*
	 * Note that POSIX says size is an unsigned quantity - though
	 * some existing systems let one pass in a negative size, meaning
	 * to allocate a buffer of appropriate size. We now support that,
	 * but if they pass in a negative number AND a buffer, can
	 * we check for that? Not sure this is necessary or correct.
	 */
	if (size == 0) {
		setoserror(EINVAL);
		return NULL;
	}

	error_string = NULL;
	alloc = 0;
	
	if(stat64(".", &pdir) < 0) {
		error_string = err_stat_dot;
		goto out;
	}
	
	if(str == NULL)  {
		/*
		 * As an extension (from GNU?), if size is < 0 AND buf is NULL,
		 * allocate appropriate size
		 */
		if ((ssize_t)size < 0) {
			/* XXX should use pathconf but on what? */
			size = MAX_PATH;
		}
		if((str = (char *)malloc((unsigned)size)) == NULL) {
			setoserror(ENOMEM);
			return NULL;
		}
		alloc = 1;
	}
	
	*dotdot = '.';
	*(dotdot+1) = '.';
	*(dotdot+2) = '\0';
	maxpwd = size--;
	str[size] = 0;

	for(;;)	{
		/* update current directory */
		cdir = pdir;

		/* open parent directory */
		if ((pdfd = opendir(dotdot)) == 0) {
			error_string = err_open_dotdot;
			break;
		}

		if(fstat64(pdfd->dd_fd, &pdir) < 0) {
			/*
			 * getwd never even checked for this error,
			 * so I won't bother with the memory for an
			 * error string.
			 */
			goto error;
		}

		/*
		 * find subdirectory of parent that matches current 
		 * directory
		 */
		if(cdir.st_dev == pdir.st_dev) {
			if(cdir.st_ino == pdir.st_ino) {
				/* at root */
				(void)closedir(pdfd);
				if (size == (maxpwd - 1))
					/* pwd is '/' */
					str[--size] = '/';

				/*
				 * For getwd() we have to do a copy in
				 * our caller, so just return a pointer
				 * to where the path starts.
				 */
				if (version == GETCWD) {
					strcpy(str, &str[size]);
					return str;
				} else {
					return &str[size];
				}
			}
			do {
				if ((dir = readdir64(pdfd)) == 0)	{
					error_string = err_read_dotdot;
					goto error;
				}
				if (mac_enabled) {
					*dotend = '/';
					strcpy(dotend + 1, dir->d_name);
					ret = lstat64(dotdot, &tdir);
					p_entino = tdir.st_ino;
				} else {
					p_entino = dir->d_ino;
					ret = 0;
				}
			} while (ret == -1 || p_entino != cdir.st_ino);
		} else {
			/*
			 * Decide whether or not the new getmountid() system
			 * call exists in this system.  If it does not, then
			 * use the old algorithm which can get hung up on
			 * NFS mount points.  Otherwise use the new system
			 * call to avoid such hangs.
			 */
			*dotend = '/';
			if (!use_getmountid) {
				/*
				 * Use lstat of each entry in the directory
				 * looking for the one that corresponds to
				 * the mount point of the file system we
				 * just came from.
				 */
 use_lstat:
				*(dotend+1) = '\0';
				do {
					if ((dir = readdir64(pdfd)) == 0) {
						error_string =
							err_read_dotdot;
						goto out;
					}
					strcpy(dotend + 1, dir->d_name);
					/*
					 * skip over non-stat'able
					 * entries
					 */
					ret = lstat64(dotdot, &tdir);
					
				} while(ret == -1 ||
					tdir.st_ino != cdir.st_ino ||
					tdir.st_dev != cdir.st_dev);
					
			} else {
				/*
				 * Must determine filenames of subdirectories
				 * and do getmountid() calls.  First munge
				 * around with the pathname to get the
				 * current directory (the child of the one we
				 * have open).
				 *
				 * Do the munging by appending a "/." to the
				 * pathname and stripping one set of "../"
				 * from the front. We add the "/." to make
				 * sure that the case of the current string
				 * ".." works.
				 */

				*(dotend+1) = '.';
				*(dotend+2) = '\0';
				dotdot += 3;
				ret = getmountid(dotdot, &cmountid);
				if (ret != 0) {
					goto out;
				}
				dotdot -= 3;
				*(dotend+1) = '\0';
				/*
				 * Check the current parent directory's
				 * . entry and compare it to its child
				 * that we just looked at.  If they are
				 * the same, then this is most likely
				 * a nohide NFS mount.  We have to resort
				 * to the old method to deal with that.
				 */
				ret = getmountid(dotdot, &tmountid);
				if (ret != 0) {
					goto out;
				}
				if (cmountid.val[0] == tmountid.val[0] &&
				    cmountid.val[1] == tmountid.val[1] &&
				    cmountid.val[2] == tmountid.val[2] &&
				    cmountid.val[3] == tmountid.val[3]) {
					goto use_lstat;
				}

				do {
					if ((dir = readdir64(pdfd)) == 0) {
						error_string =
							err_read_dotdot;
						goto error;
					}
					strcpy(dotend + 1, dir->d_name);

                                       /*
                                         * getmountid info. is not unique for
                                         * autofs mounted directories always,
                                         * therefore, the rdev is also used,
                                         * lstat() is used not stat() so that
                                         * if it's a link and the server is
                                         * down we don't wait for the NFS 
                                         * timeout before we get the error
					 * Only call lstat if getmountid
					 * gave us a match -- avoid blocking
					 * on a down NFS server when possible.
                                         */
					ret = getmountid(dotdot, &tmountid);

					/* avoid calling lstat if possible */

					if (ret == -1 ||
					    cmountid.val[0] != tmountid.val[0] ||
					    cmountid.val[1] != tmountid.val[1] ||
					    cmountid.val[2] != tmountid.val[2] ||
					    cmountid.val[3] != tmountid.val[3])
					{
						ret = -1;
						continue;
					}
                                        ret = lstat64(dotdot, &tdir);
				
				/*
				 * if lstat64 or getmountid returned -1 it 
				 * indicates that the file is non-stat'able 
				 * (ie. like a link to a file that does not
				 * exist) so, just skip the file
				 */
				} while (ret == -1 || cdir.st_rdev != tdir.st_rdev);
			}
		}

		strsize = strlen(dir->d_name);

		if ((size==0) || (strsize > size - 1)) {
			_setoserror(ERANGE);
			(void)closedir(pdfd);
			break;
		} else {
			/*
			 * copy the name of the current directory into the
			 * pathname 
			 */
			if (mac_enabled) {
				label = mac_get_file(dotdot);
				if (label == NULL) {
       		                 	perror("pwd");
       		                 	exit(1);
				}
                		if (moldy_process || ! mac_is_moldy(label)) {
					size -= strsize;
					strncpy(&str[size], dir->d_name, strsize);
					str[--size] = '/';
					(void)closedir(pdfd);
				}
				mac_free(label);
			} else {
				size -= strsize;
				strncpy(&str[size], dir->d_name, strsize);
				str[--size] = '/';
				(void)closedir(pdfd);
			}
		}
		if (dotdot - 3 < dotdots) 
			break;
		/* update dotdot to parent directory */
		*--dotdot = '/';
		*--dotdot = '.';
		*--dotdot = '.';
		*dotend = '\0';
	}
 out:
	if (alloc)
		free(str);

	if ((version == GETWD) && (error_string	!= NULL)) {
		/*
		 * Note that in the GETWD case, 'str' is 
		 * never dynamically allocated and is always
		 * MAX_PATH long.  So this strcpy is ok, even
		 * though it looks scary, given the "free(str)"
		 * above.
		 */
		strcpy(str, error_string);
	}
	return NULL;

 error:
	saverr = _oserror();
	(void)closedir(pdfd);
	_setoserror(saverr);
	goto out;
}
