/* -----------------------------------------------------------------
 *
 *			subr.c
 *
 * Common subroutines used by the programs in these subdirectories.
 */

/* #pragma ident "@(#)subr.c   1.6     94/02/10 SMI" */
/*
 *  (c) 1992  Sun Microsystems, Inc
 */

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/mount.h>
#include <sys/statvfs.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <wait.h>
#include <ctype.h>
#include <fcntl.h>
#include <mntent.h>
#include <sys/mman.h>
#include <cachefs/cachefs_fs.h>
#include <syslog.h>
#include <dirent.h>
#include "subr.h"

#define XMAXPATH (PATH_MAX + MAXNAMELEN + 2)

extern int errno;
extern char *Progname;

/* -----------------------------------------------------------------
 *
 *			cachefs_dir_lock
 *
 * Description:
 *	Gets a lock on the cache directory.
 *	To release the lock, call cachefs_dir_unlock
 *	with the returned value.
 * Arguments:
 *	cachedirp	name of the cache directory
 *	shared		1 if shared, 0 if not
 * Returns:
 *	Returns -1 if the lock cannot be obtained immediatly.
 *	If the lock is obtained, returns a value >= 0.
 * Preconditions:
 *	precond(cachedirp)
 */

int
cachefs_dir_lock(char *cachedirp, int shared)
{
	int fd;
	int xx;
	char buf[MAXPATHLEN];
	char *lockp = CACHELABEL_NAME;
	struct flock fl;

	/* see if path name is too long */
	xx = strlen(cachedirp) + strlen(lockp) + 3;
	if (xx >= MAXPATHLEN) {
		pr_err(gettext("Cache directory name %s is too long"),
		    cachedirp);
		return (-1);
	}

	/* make a path to the cache directory lock file */
	sprintf(buf, "%s/%s", cachedirp, lockp);

	/* create and open the cache directory lock file */
	fd = open(buf, O_RDWR | O_CREAT, 0700);
	if (fd == -1) {
		pr_err(gettext("Cannot open lock file %s"), buf);
		return (-1);
	}

	/* try to set the lock */
	fl.l_type = (shared == 1) ? F_RDLCK : F_WRLCK;
	fl.l_whence = 0;
	fl.l_start = 0;
	fl.l_len = 0;
	fl.l_sysid = 0;
	fl.l_pid = 0;
	xx = fcntl(fd, F_SETLKW, &fl);
	if (xx == -1) {
		if (errno == EAGAIN) {
			pr_err(gettext("Cannot gain access to the "
			    "cache directory %s."), cachedirp);
		} else {
			pr_err(gettext("Unexpected failure on lock file %s %s"),
			    buf, strerror(errno));
		}
		close(fd);
		return (-1);
	}

	/* return the file descriptor which can be used to release the lock */
	return (fd);
}

/* -----------------------------------------------------------------
 *
 *			cachefs_dir_unlock
 *
 * Description:
 *	Releases an advisory lock on the cache directory.
 * Arguments:
 *	fd	cookie returned by cachefs_dir_lock
 * Returns:
 *	Returns -1 if the lock cannot be released or 0 for success.
 * Preconditions:
 */

int
cachefs_dir_unlock(int fd)
{
	struct flock fl;
	int error = 0;
	int xx;

	/* release the lock */
	fl.l_type = F_UNLCK;
	fl.l_whence = 0;
	fl.l_start = 0;
	fl.l_len = 0;
	fl.l_sysid = 0;
	fl.l_pid = 0;
	xx = fcntl(fd, F_SETLK, &fl);
	if (xx == -1) {
		pr_err(gettext("Unexpected failure releasing lock file %s"),
			strerror(errno));
		error = -1;
	}

	/* close the lock file */
	close(fd);

	return (error);
}

/* -----------------------------------------------------------------
 *
 *			cachefs_label_file_get
 *
 * Description:
 *	Gets the contents of a cache label file.
 *	Performs error checking on the file.
 * Arguments:
 *	filep	name of the cache label file
 *	clabelp	where to put the file contents
 * Returns:
 *	Returns 0 for success or -1 if an error occurs.
 * Preconditions:
 *	precond(filep)
 *	precond(clabelp)
 */

int
cachefs_label_file_get(char *filep, struct cache_label *clabelp)
{
	int xx;
	int fd;
	struct stat statinfo;

	/* get info on the file */
	xx = lstat(filep, &statinfo);
	if (xx == -1) {
		if (errno != ENOENT) {
			pr_err(gettext("Cannot stat file %s: %s"),
			    filep, strerror(errno));
		} else {
			pr_err(gettext("File %s does not exist."), filep);
		}

		return (-1);
	}

	/* if the file is the wrong type */
	if (!S_ISREG(statinfo.st_mode)) {
		pr_err(gettext("Cache label file %s corrupted"), filep);
		return (-1);
	}

	/* if the file is the wrong size */
	if (statinfo.st_size != sizeof (struct cache_label)) {
		pr_err(gettext("Cache label file %s wrong size"), filep);
		return (-1);
	}

	/* open the cache label file */
	fd = open(filep, O_RDONLY);
	if (fd == -1) {
		pr_err(gettext("Error opening %s: %s"), filep,
		    strerror(errno));
		return (-1);
	}

	/* read the current set of parameters */
	xx = read(fd, clabelp, sizeof (struct cache_label));
	if (xx != sizeof (struct cache_label)) {
		pr_err(gettext("Reading %s failed: %s\n"), filep,
		    strerror(errno));
		close(fd);
		return (-1);
	}
	close(fd);

	/* check for an invalid version number */
	if (clabelp->cl_cfsversion != CFSVERSION) {
		pr_err(gettext("Invalid cache version"), filep);
		return (-1);
	}

	/* return success */
	return (0);
}

/* -----------------------------------------------------------------
 *
 *			cachefs_label_file_put
 *
 * Description:
 *	Outputs the contents of a cache label object to a file.
 * Arguments:
 *	filep	name of the cache label file
 *	clabelp	where to get the file contents
 * Returns:
 *	Returns 0 for success or -1 if an error occurs.
 * Preconditions:
 *	precond(filep)
 *	precond(clabelp)
 */

int
cachefs_label_file_put(char *filep, struct cache_label *clabelp)
{
	int xx;
	int fd;
	struct dioattr frontdio;

	/* check for an invalid version number */
	if (clabelp->cl_cfsversion != CFSVERSION) {
		pr_err(gettext("Invalid cache version"), filep);
		return (-1);
	}

	/* get rid of the file if it already exists */
	xx = unlink(filep);
	if ((xx == -1) && (errno != ENOENT)) {
		pr_err(gettext("Could not remove %s: %s"), filep, strerror(errno));
		return (-1);
	}

	/* open the cache label file */
	/*
	 * create the label file and open it for direct I/O first
	 * open for direct I/O so that F_DIOINFO will work
	 */
	fd = open(filep, O_CREAT | O_RDWR | O_DIRECT, 0600);
	if (fd == -1) {
		pr_err(gettext("Error creating %s: %s"), filep, strerror(errno));
		return (-1);
	}
	/*
	 * get the direct I/O information for the front file system
	 */
	if (fcntl(fd, F_DIOINFO, &frontdio) == -1) {
		pr_err(gettext("Error getting direct I/O info for %s: %s"), filep,
		    strerror(errno));
		close(fd);
		return (-1);
	}
	/*
	 * set the minimum I/O transfer size
	 */
	clabelp->cl_minio = frontdio.d_miniosz;
	clabelp->cl_align = frontdio.d_mem;

	/*
	 * close and reopen the label file for normal I/O
	 */
	close(fd);
	fd = open(filep, O_RDWR);
	if (fd == -1) {
		pr_err(gettext("Error reopening %s: %s"), filep, strerror(errno));
		return (-1);
	}

	/* write out the cache label object */
	xx = write(fd, clabelp, sizeof (struct cache_label));
	if (xx != sizeof (struct cache_label)) {
		pr_err(gettext("Writing %s failed: %s"), filep, strerror(errno));
		close(fd);
		return (-1);
	}

	/* make sure the contents get to disk */
	if (fsync(fd) != 0) {
		pr_err(gettext("Writing %s failed on sync: %s"), filep,
		    strerror(errno));
		close(fd);
		return (-1);
	}

	close(fd);

	/* return success */
	return (0);
}

/* -----------------------------------------------------------------
 *
 *			cachefs_inuse
 *
 * Description:
 *	Tests whether or not the cache directory is in use by
 *	the cache file system.
 * Arguments:
 *	cachedirp	name of the file system cache directory
 * Returns:
 *	Returns 1 if the cache is in use or an error, 0 if not.
 * Preconditions:
 *	precond(cachedirp)
 */

int
cachefs_inuse(char *cachedirp)
{
	int fd;
	int xx;
	char buf[MAXPATHLEN];
	char *lockp = CACHELABEL_NAME;
	struct flock fl;

	/* see if path name is too long */
	xx = strlen(cachedirp) + strlen(lockp) + 3;
	if (xx >= MAXPATHLEN) {
		pr_err(gettext("Cache directory name %s is too long"),
		    cachedirp);
		return (1);
	}

	/* make a path to the cache directory lock file */
	sprintf(buf, "%s/%s", cachedirp, lockp);

	/* open the kernel in use lock file */
	fd = open(buf, O_RDWR, 0700);
	if (fd == -1) {
		pr_err(gettext("Cannot open lock file %s"), buf);
		return (1);
	}

	/* test the lock status */
	fl.l_type = F_WRLCK;
	fl.l_whence = 0;
	fl.l_start = 0;
	fl.l_len = 1024;
	fl.l_sysid = 0;
	fl.l_pid = 0;
	xx = fcntl(fd, F_GETLK, &fl);
	if (xx == -1) {
		pr_err(gettext("Unexpected failure on lock file %s %s"),
		    buf, strerror(errno));
		close(fd);
		return (1);
	}
	close(fd);

	if (fl.l_type == F_UNLCK)
		xx = 0;
	else
		xx = 1;

	/* return whether or not the cache is in use */
	return (xx);
}

/* -----------------------------------------------------------------
 *
 *			pr_err
 *
 * Description:
 *	Prints an error message to stderr.
 * Arguments:
 *	fmt	printf style format
 *	...	arguments for fmt
 * Returns:
 * Preconditions:
 *	precond(fmt)
 */

void
pr_err(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	(void) fprintf(stderr, gettext("%s: "), Progname);
	(void) vfprintf(stderr, fmt, ap);
	(void) fprintf(stderr, "\n");
	va_end(ap);
}

/* -----------------------------------------------------------------
 *
 *			log_err
 *
 * Description:
 *	Prints an error message to syslog.
 * Arguments:
 *	fmt	printf style format
 *	...	arguments for fmt
 * Returns:
 * Preconditions:
 *	precond(fmt)
 */

void
log_err(char *fmt, ...)
{
	va_list ap;
	char *log_fmt;

	log_fmt = malloc(strlen(Progname) + 2 + strlen(fmt) + 3);
	if (!log_fmt) {
		syslog(LOG_ERR, "log_err: out of memory\n");
		return;
	}
	va_start(ap, fmt);
	(void) sprintf(log_fmt, "%s: %s\n", Progname, fmt);
	(void) vsyslog(LOG_ERR, log_fmt, ap);
	va_end(ap);
}

/* -----------------------------------------------------------------
 *
 *			delete_file
 *
 * Description:
 *	Remove a file or directory; called by nftw().
 * Arguments:
 *	namep	pathname of the file
 *	statp	stat info about the file
 *	flg	info about file
 *	ftwp	depth information
 * Returns:
 *	Returns 0 for success, -1 for failure.
 * Preconditions:
 *	precond(namep)
 */

int
delete_file(const char *namep, const struct stat *statp, int flg,
    struct FTW *ftwp)
{
	/* ignore . and .. */
	if (!strcmp(namep, ".") || !strcmp(namep, ".."))
		return (0);

	switch (flg) {
	case FTW_F:	/* files */
	case FTW_SL:
		if (unlink(namep) == -1) {
			pr_err(gettext("unlink %s failed: %s"),
			    namep, strerror(errno));
			return (-1);
		}
		break;

	case FTW_DP:	/* directories that have their children processed */
		if (rmdir(namep) == -1) {
			pr_err(gettext("rmdir %s failed: %s"),
			    namep, strerror(errno));
			return (-1);
		}
		break;

	case FTW_D:	/* ignore directories if children not processed */
		break;

	default:
		pr_err(gettext("failure on file %s, flg %d."),
		    namep, flg);
		return (-1);
	}

	/* return success */
	return (0);
}

/* -----------------------------------------------------------------
 *
 *			convert_cl2uv
 *
 * Description:
 *	Copies the contents of a cache_label object into a
 *	user_values object, performing the necessary conversions.
 * Arguments:
 *	clp	cache_label to copy from
 *	uvp	user_values to copy into
 *	dirp	cache directory
 * Returns:
 *	Returns 0 for success, -1 for an error.
 * Preconditions:
 *	precond(clp)
 *	precond(uvp)
 *	precond(dirp)
 */

int
convert_cl2uv(const struct cache_label *clp, struct user_values *uvp,
    const char *dirp)
{
	struct statvfs fs;
	int xx;
	long long ltmp;

	/* get file system information */
	xx = statvfs(dirp, &fs);
	if (xx == -1) {
		pr_err(gettext("statvfs %s failed: %s"), dirp,
		    strerror(errno));
		return (-1);
	}

#define	BOUND(yy) \
	yy = (yy < 0) ? 0 : yy; \
	yy = (yy > 100) ? 100 : yy;

	xx = ((double)clp->cl_maxblks / (double)fs.f_blocks) * 100. + .5;
	BOUND(xx);
	uvp->uv_maxblocks = xx;

	xx = ((double)clp->cl_maxfiles / (double)fs.f_files) * 100. + .5;
	BOUND(xx);
	uvp->uv_maxfiles = xx;

	xx = ((double)clp->cl_blkhiwat / (double)fs.f_blocks) * 100. + .5;
	BOUND(xx);
	uvp->uv_hiblocks = xx;

	xx = ((double)clp->cl_blklowat / (double)fs.f_blocks) * 100. + .5;
	BOUND(xx);
	uvp->uv_lowblocks = xx;

	xx = ((double)clp->cl_filehiwat / (double)fs.f_files) * 100. + .5;
	BOUND(xx);
	uvp->uv_hifiles = xx;

	xx = ((double)clp->cl_filelowat / (double)fs.f_files) * 100. + .5;
	BOUND(xx);
	uvp->uv_lowfiles = xx;

	/* return success */
	return (0);
}

/* -----------------------------------------------------------------
 *
 *			convert_uv2cl
 *
 * Description:
 *	Copies the contents of a user_values object into a
 *	cache_label object, performing the necessary conversions.
 * Arguments:
 *	uvp	user_values to copy from
 *	clp	cache_label to copy into
 *	dirp	cache directory
 * Returns:
 *	Returns 0 for success, -1 for an error.
 * Preconditions:
 *	precond(uvp)
 *	precond(clp)
 *	precond(dirp)
 */

int
convert_uv2cl(const struct user_values *uvp, struct cache_label *clp,
    const char *dirp)
{
	struct statvfs fs;
	int xx;

	/* get file system information */
	xx = statvfs(dirp, &fs);
	if (xx == -1) {
		pr_err(gettext("statvfs %s failed: %s"), dirp,
		    strerror(errno));
		return (-1);
	}

	clp->cl_maxblks = uvp->uv_maxblocks / 100.0 * fs.f_blocks + .5;
	clp->cl_maxfiles = uvp->uv_maxfiles / 100.0 * fs.f_files + .5;

	clp->cl_blkhiwat = uvp->uv_hiblocks / 100.0 * fs.f_blocks + .5;
	clp->cl_blklowat = uvp->uv_lowblocks / 100.0 * fs.f_blocks + .5;

	clp->cl_filehiwat = uvp->uv_hifiles / 100.0 * fs.f_files + .5;
	clp->cl_filelowat = uvp->uv_lowfiles / 100.0 * fs.f_files + .5;

	/* return success */
	return (0);
}

/* -----------------------------------------------------------------
 *
 *			create_cache
 *
 * Description:
 *	Creates the specified cache directory and populates it as
 *	needed by CFS.
 * Arguments:
 *	dirp		the name of the cache directory
 *	uvp			user values
 * Returns:
 *	Returns 0 for success or:
 *		-1 for an error
 *		-2 for an error and cache directory partially created
 * Preconditions:
 *	precond(dirp)
 *	precond(uvp)
 */

int
create_cache(char *dirp, struct user_values *uvp)
{
	struct cache_label clabel;
	int xx;
	char path[XMAXPATH];
	int fd;
	void *bufp;
	int cnt;
	int lockid;

	/* verify options are reasonable */
	xx = check_user_values_for_sanity(uvp);
	if (xx)
		return (-1);

	/* make sure cache dir name is not too long */
	if (strlen(dirp) > (size_t)PATH_MAX) {
		pr_err(gettext("path name %s is too long."), dirp);
		return (-1);
	}

	/* make the directory */
	if (mkdir(dirp, 0) == -1) {
		switch (errno) {
		case EEXIST:
			if (chmod(dirp, (mode_t)0) == -1) {
				pr_err(gettext("Unable to change %s to mode 0."), dirp);
				return(-1);
			}
			break;

		default:
			pr_err(gettext("mkdir %s failed: %s"),
			    dirp, strerror(errno));
			return (-1);
		}
	}

	/* convert user values to a cache_label */
	xx = convert_uv2cl(uvp, &clabel, dirp);
	if (xx)
		return (-1);

	/*
	 * set the label version
	 */
	clabel.cl_cfsversion = CFSVERSION;
	clabel.cl_cfslongsize = 0;
	clabel.cl_cfsmetasize = 0;

	/* lock the cache directory non-shared */
	lockid = cachefs_dir_lock(dirp, 0);
	if (lockid == -1) {
		/* quit if could not get the lock */
		return (-2);
	}

	/* make the directory for the back file system mount points */
	/* note: we do not count this directory in the resources */
	sprintf(path, "%s/%s", dirp, BACKMNT_NAME);
	if (mkdir(path, 0700) == -1) {
		if (errno != EEXIST) {
			pr_err(gettext("mkdir %s failed: %s"), path,
			    strerror(errno));
			cachefs_dir_unlock(lockid);
			return (-2);
		} else {
			cachefs_dir_unlock(lockid);
			return(0);
		}
	}

	/* create the cache label file */
	sprintf(path, "%s/%s", dirp, CACHELABEL_NAME);
	xx = cachefs_label_file_put(path, &clabel);
	if (xx == -1) {
		pr_err(gettext("creating %s failed."), path);
		cachefs_dir_unlock(lockid);
		return (-2);
	}

	/* create the cache label duplicate file */
	sprintf(path, "%s/%s.dup", dirp, CACHELABEL_NAME);
	xx = cachefs_label_file_put(path, &clabel);
	if (xx == -1) {
		pr_err(gettext("creating %s failed."), path);
		cachefs_dir_unlock(lockid);
		return (-2);
	}

	/* release the lock on the cache */
	cachefs_dir_unlock(lockid);

	/* return success */
	return (0);
}

/* -----------------------------------------------------------------
 *
 *			check_user_values_for_sanity
 *
 * Description:
 *	Check the user_values for sanity.
 * Arguments:
 *	uvp	user_values object to check
 * Returns:
 *	Returns 0 if okay, -1 if not.
 * Preconditions:
 *	precond(uvp)
 */

int
check_user_values_for_sanity(const struct user_values *uvp)
{
	int ret;

	ret = 0;

	if (uvp->uv_lowblocks >= uvp->uv_hiblocks) {
		pr_err(gettext("lowblocks can't be >= hiblocks."));
		ret = -1;
	}
	if (uvp->uv_lowfiles >= uvp->uv_hifiles) {
		pr_err(gettext("lowfiles can't be >= hifiles."));
		ret = -1;
	}

	/* XXX more conditions to check here? */

	/* XXX make sure thresh values are between min and max values */

	/* return status */
	return (ret);
}

/* -----------------------------------------------------------------
 *
 *			user_values_defaults
 *
 * Description:
 *	Sets default values in the user_values object.
 * Arguments:
 *	uvp	user_values object to set values for
 * Returns:
 * Preconditions:
 *	precond(uvp)
 */

void
user_values_defaults(struct user_values *uvp)
{
	uvp->uv_maxblocks = 90;
	uvp->uv_maxfiles = 90;
	uvp->uv_hiblocks = 85;
	uvp->uv_lowblocks = 75;
	uvp->uv_hifiles = 85;
	uvp->uv_lowfiles = 75;
}

/* -----------------------------------------------------------------
 *
 *			delete_cache
 *
 * Description:
 *	Deletes the specified file system cache.
 * Arguments:
 *	dirp	cache directory name
 *	namep	file system cache directory to delete
 * Returns:
 *	Returns 0 for success, -1 for failure.
 * Preconditions:
 *	precond(dirp)
 *	precond(namep)
 */

int
delete_cache(char *dirp, char *namep)
{
	char command[XMAXPATH + 8];
	int status;

	/* make sure cache dir name is not too long */
	if (strlen(dirp) > (size_t)PATH_MAX) {
		pr_err(gettext("path name %s is too long."),
		    dirp);
		return (-1);
	}

	/* construct the path name of the file system cache directory */
	sprintf(command, "rm -rf %s/%s", dirp, namep);

	if ((status = system(command)) == -1) {
		pr_err("Could not remove %s/%s: %s\n", dirp, namep, strerror(errno));
		return(-1);
	} else if (WIFEXITED(status)) {
		if (WEXITSTATUS(status)) {
			pr_err("Could not remove %s/%s: exit status %d\n", dirp, namep,
				WEXITSTATUS(status));
			return(-1);
		}
	} else if (WIFSIGNALED(status)) {
		pr_err("Could not remove %s/%s: signal %d\n", dirp, namep,
			WTERMSIG(status));
		return(-1);
	} else {
		pr_err("Could not remove %s/%s: status 0x%x\n", dirp, namep, status);
		return(-1);
	}

	/* return success */
	return (0);
}

/* -----------------------------------------------------------------
 *
 *			delete_all_cache
 *
 * Description:
 *	Delete all caches in cache directory.
 * Arguments:
 *	dirp	the pathname of of the cache directory to delete
 * Returns:
 *	Returns 0 for success or -1 for an error.
 * Preconditions:
 *	precond(dirp)
 */

int
delete_all_cache(char *dirp)
{
	int status;
	DIR *dp;
	struct dirent *dep;
	char command[XMAXPATH + 8];
	struct stat statinfo;

	/* make sure cache dir name is not too long */
	if (strlen(dirp) > (size_t)PATH_MAX) {
		pr_err(gettext("path name %s is too long."),
		    dirp);
		return (-1);
	}

	/* delete the back FS mount point directory if it exists */
	sprintf(command, "rm -rf %s/%s", dirp, BACKMNT_NAME);
	if ((status = system(command)) == -1) {
		pr_err(gettext("Could not remove %s/%s: %s"), dirp, BACKMNT_NAME,
		    strerror(errno));
		return (-1);
	} else if (WIFEXITED(status)) {
		if (WEXITSTATUS(status)) {
			pr_err("Could not remove %s/%s: exit status %d\n", dirp,
				BACKMNT_NAME, WEXITSTATUS(status));
			return(-1);
		}
	} else if (WIFSIGNALED(status)) {
		pr_err("Could not remove %s/%s: signal %d\n", dirp, BACKMNT_NAME,
			WTERMSIG(status));
		return(-1);
	} else {
		pr_err("Could not remove %s/%s: status 0x%x\n", dirp, BACKMNT_NAME,
			status);
		return(-1);
	}

	/* open the cache directory specified */
	if ((dp = opendir(dirp)) == NULL) {
		pr_err(gettext("cannot open cache directory %s: %s"),
		    dirp, strerror(errno));
		return (-1);
	}

	/*
	 * read the file names in the cache directory
	 * remove only directories with names beginning with CACHEID_PREFIX
	 * all cache ID directory names begin with CACHEID_PREFIX
	 */
	while ((dep = readdir(dp)) != NULL) {
		if (strncmp(dep->d_name, CACHEID_PREFIX, CACHEID_PREFIX_LEN) == 0) {
			sprintf(command, "%s/%s", dirp, dep->d_name);
			if (lstat(command, &statinfo) == -1) {
				pr_err("Could not lstat %s: %s", command, strerror(errno));
			} else if (S_ISDIR(statinfo.st_mode)) {
				/* delete the file system cache directory */
				if (delete_cache(dirp, dep->d_name)) {
					closedir(dp);
					return (-1);
				}
			}
		}
	}
	closedir(dp);

	/* remove the cache label file */
	sprintf(command, "%s/%s", dirp, CACHELABEL_NAME);
	if ((unlink(command) == -1) && (errno != ENOENT)) {
		pr_err(gettext("unlink %s failed: %s"), command,
		    strerror(errno));
		return (-1);
	}

	/* remove the cache label duplicate file */
	sprintf(command, "%s/%s.dup", dirp, CACHELABEL_NAME);
	if ((unlink(command) == -1) && (errno != ENOENT)) {
		pr_err(gettext("unlink %s failed: %s"), command,
		    strerror(errno));
		return (-1);
	}

	/* remove the directory */
	if (rmdir(dirp) == -1) {
		pr_err(gettext("rmdir %s failed: %s"), dirp,
		    strerror(errno));
		return (-1);
	}

	/* return success */
	return (0);
}

/* -----------------------------------------------------------------
 *
 * Calculate a 16 bit ones-complement checksum for a single buffer.
 *	This routine always adds even address bytes to the high order 8 bits
 *	of the 16 bit checksum and odd address bytes are added to the low
 *	order 8 bits of the 16 bit checksum.  The caller must swap bytes in
 *	the sum to make this correct.
 *
 * The caller must ensure that the length is not zero or > 32K.
 */
static u_int
cksum(ushort *src,			/* first byte */
	     int len)			/* # of bytes */
{
	u_int ck = 0;

	/* do 64 byte blocks for the 128-byte cache line of the IP17 */
	while (len >= 64) {
		ck += src[0]; ck += src[1]; ck += src[2]; ck += src[3];
		ck += src[4]; ck += src[5]; ck += src[6]; ck += src[7];

		ck += src[8]; ck += src[9]; ck += src[10]; ck += src[11];
		ck += src[12]; ck += src[13]; ck += src[14]; ck += src[15];

		ck += src[16]; ck += src[17]; ck += src[18]; ck += src[19];
		ck += src[20]; ck += src[21]; ck += src[22]; ck += src[23];

		ck += src[24]; ck += src[25]; ck += src[26]; ck += src[27];
		ck += src[28]; ck += src[29]; ck += src[30]; ck += src[31];
		src += 32;
		len -= 64;
	}

	/* we have < 64 bytes remaining */
	if (0 != (len&32)) {
		ck += src[0]; ck += src[1]; ck += src[2]; ck += src[3];
		ck += src[4]; ck += src[5]; ck += src[6]; ck += src[7];
		ck += src[8]; ck += src[9]; ck += src[10]; ck += src[11];
		ck += src[12]; ck += src[13]; ck += src[14]; ck += src[15];
		src += 16;
	}
	if (0 != (len&16)) {
		ck += src[0]; ck += src[1]; ck += src[2]; ck += src[3];
		ck += src[4]; ck += src[5]; ck += src[6]; ck += src[7];
		src += 8;
	}
	if (0 != (len&8)) {
		ck += src[0]; ck += src[1]; ck += src[2]; ck += src[3];
		src += 4;
	}

	if (0 != (len&4)) {
		ck += src[0]; ck += src[1];
		src += 2;
	}

	if (0 != (len&2)) {
		ck += src[0];
		src += 1;
	}

	if (0 != (len&1)) {
#ifdef _MIPSEL
		ck += *(unchar*)src;
#else
		ck += *(unchar*)src << 8;
#endif
	}

	ck = (ck & 0xffff) + (ck >> 16);
	return(0xffff & (ck + (ck >> 16)));
}

/* -----------------------------------------------------------------
 *
 *			read_file_header
 *
 * Description:
 *	Read the file header from a cache file.
 * Arguments:
 *	path		full path for the file
 *	fileheader	place to put the file header
 * Returns:
 *	fileheader	the file header is placed into the location pointed to
 *				by fileheader
 *	The return value of the function is a Boolean indicating whether or
 *	not the file header was successfully read.
 * Preconditions:
 *	precond(path)
 *	precond(fileheader)
 */

int
read_file_header(const char *path, char *fileheader)
{
	u_long header_sum = 0;
	u_long calculated_sum = 0;
	char *bp = fileheader;
	int bytes = 0;
	int resid = FILEHEADER_SIZE;
	int status = 1;
	int fd;
	fileheader_t *fhp = (fileheader_t *)fileheader;

	if ((fd = open(path, O_RDONLY, CACHEFS_FILEMODE)) == -1) {
		return(0);
	}
	do {
		bytes = read(fd, bp, resid);
		switch (bytes) {
			case 0:
				status = 0;
				break;
			case -1:
				status = 0;
				break;
			default:
				resid -= bytes;
				bp += bytes;
		}
	} while (status && (bytes < resid) && (bytes > 0));
	close(fd);
	if (status) {
		if (fhp->fh_metadata.md_checksum == 0) {
			status = 0;
		} else {
			header_sum = (u_long)fhp->fh_metadata.md_checksum;
			fhp->fh_metadata.md_checksum = 0;
			calculated_sum = cksum((ushort *)fileheader,
				sizeof(fileheader_t));
			if (calculated_sum != header_sum) {
				status = 0;
			}
		}
	}
	return(status);
}

char *
make_filename(char *path)
{
	char *name = strdup(path);
	char *cp;

	if (name) {
		cp = name;
		while ((cp = strchr(cp, '/')) != NULL)
			*cp = '_';
	}
	return(name);
}

/*
 *
 *			get_cacheid
 *
 * Description:
 *	Determines an identifier for the front file system cache.
 *	The length of the returned string is < PATH_MAX.
 *	The cache ID is of the form CACHEID_PREFIX<fsid>:<mntp> where fsid
 *	is the back file system FS name and mntp is the mount point name.
 * Arguments:
 *	fsidp	back file system id
 * Returns:
 *	Returns a pointer to the string identifier, or NULL if the
 *	identifier was overflowed.
 * Preconditions:
 *	precond(fsidp)
 */

char *
get_cacheid(char *fsidp, char *mntp)
{
	char *c1;
	char *buf;
	int len = strlen(mntp) + strlen(fsidp) + strlen(CACHEID_PREFIX) + 1;

	if (len >= (size_t) PATH_MAX)
		return (NULL);

	buf = malloc(len + 1);
	if (!buf)
		return(NULL);
	strcpy(buf, CACHEID_PREFIX);
	strcat(buf, fsidp);
	strcat(buf, ":");
	strcat(buf, mntp);
	c1 = buf;
	while ((c1 = strchr(c1, '/')) != NULL)
		*c1 = '_';
	return (buf);
}

/*
 *
 *			get_mount_point
 *
 * Description:
 *	Makes a suitable mount point for the back file system.
 *	The name of the mount point created is stored in a malloced
 *	buffer in pathpp
 * Arguments:
 *	cachedirp	the name of the cache directory
 *	cacheid		the cache ID for the file system
 *	pathpp		where to store the mount point
 * Returns:
 *	Returns 0 for success, -1 for an error, -2 for busy.
 * Preconditions:
 *	precond(cachedirp)
 *	precond(cacheid)
 *	precond(pathpp)
 */

int
get_mount_point(char *cachedirp, char *cacheid, char **pathpp, int validate)
{
	char *strp;
	char *namep;
	struct stat stat1;
	struct stat stat2;
	int xx;

	/* get some space for the path name */
	strp = malloc(MAXPATHLEN);
	if (strp == NULL) {
		pr_err(gettext("out of memory"));
		return (-1);
	}

	/* see if the mount directory is valid */
	sprintf(strp, "%s/%s", cachedirp, BACKMNT_NAME);
	xx = stat(strp, &stat1);
	if ((xx == -1) || !S_ISDIR(stat1.st_mode)) {
		pr_err(gettext("%s is not a valid cache."), strp);
		return (-1);
	}

	/* find a directory name we can use */
	/* construct a directory name to consider */
	namep = strp + strlen(strp);
	sprintf(namep, "/%s", cacheid);

	if (validate) {
		/* stat the directory */
		xx = stat(strp, &stat2);

		/* if the stat failed */
		if (xx == -1) {
			/* if the dir does not exist we can use it */
			if (errno == ENOENT) {
				/* create the directory */
				if (mkdir(strp, 0755) == -1) {
					pr_err(gettext("could not make directory %s"), strp);
					return (-1);
				}

				/* return success */
				*pathpp = strp;
				return (0);
			}

			/* any other error we are hosed */
			pr_err(gettext("could not stat %s %s"),
			    strp, strerror(errno));
			return (-1);
		}

		/* if it is a dir that is not being used, then we can use it */
		if (S_ISDIR(stat2.st_mode) && (stat1.st_dev == stat2.st_dev)) {
			*pathpp = strp;
			return (0);
		}
	}

	/* the mount point is in use */
	*pathpp = strp;
	return (-2);
}
