/*
 * local locking operations:
 *
 *		fcntl
 *		flock
 *		lockf
 */
#include <sys/param.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <string.h>
#if !SVR4
#include <strings.h>
#endif /* !SVR4 */
#include <stdlib.h>
#include <assert.h>
#include <rpc/rpc.h>
#include "lk_test.h"
#include "print.h"
#include "util.h"
#include "fileops.h"

extern int errno;
extern char *Progname;
extern int Verbose;

/*
 * Perform an fcntl(2) on a file on a local file system.
 */
/* ARGSUSED */
int
local_fcntl(struct fcntlargs *fap)
{
	int status = 0;
	struct flock fl;
	int fd;

	if (Verbose) {
		printf("%s: local_fcntl: %s start %d len %d type %s\n",
			Progname, lockcmd_to_str(fap->fa_cmd), (int)fap->fa_lock.ld_offset,
			(int)fap->fa_lock.ld_len, locktype_to_str(fap->fa_lock.ld_type));
	}
	switch (fap->fa_cmd) {
		case F_GETLK:
		case F_SETLK:
		case F_SETLKW:
			if ( (fd = open_file(fap->fa_name, OPEN_FLAGS, DEFAULT_MODE))
				== -1 ) {
					(void)fprintf( stderr,
						"%s: local_fcntl: unable to open %s\n",
						Progname, fap->fa_name );
					status = errno ? errno : -1;
			} else {
				fl.l_whence = SEEK_SET;
				fl.l_start = fap->fa_lock.ld_offset;
				fl.l_len = fap->fa_lock.ld_len;
				fl.l_pid = getpid();
				fl.l_type = fap->fa_lock.ld_type;
				if ( fcntl( fd, fap->fa_cmd, &fl ) ) {
					status = errno;
				} else if (fap->fa_cmd == F_GETLK) {
					fap->fa_lock.ld_offset = fl.l_start;
					fap->fa_lock.ld_len = fl.l_len;
					fap->fa_lock.ld_pid = fl.l_pid;
					fap->fa_lock.ld_sysid = fl.l_sysid;
					fap->fa_lock.ld_type = fl.l_type;
				}
			}
			break;
		default:
			status = EINVAL;
	}
	return(status);
}

/*
 * Perform an flock(3B) on a file on a local file system.
 */
/* ARGSUSED */
int
local_flock(struct flockargs *fap)
{
	int status = 0;
	int fd;

	if ( (fd = open_file(fap->fa_name, OPEN_FLAGS, DEFAULT_MODE)) == -1 ) {
		(void)fprintf( stderr, "%s: local_flock: unable to open %s\n",
			Progname, fap->fa_name );
		status = errno ? errno : -1;
	} else {
		if ( flock(fd, fap->fa_op) ) {
			status = errno;
		}
	}
	return(status);
}

/*
 * Perform an lockf(3C) on a file on a local file system.
 */
/* ARGSUSED */
int
local_lockf(struct lockfargs *lap)
{
	int status = 0;
	int fd;
	off_t offset;

	if ( (fd = open_file(lap->la_name, OPEN_FLAGS, DEFAULT_MODE)) == -1 ) {
		status = errno ? errno : -1;
		(void)fprintf( stderr, "%s: local_lockf: unable to open %s\n",
			Progname, lap->la_name );
	} else if ((offset = lseek(fd, lap->la_offset, SEEK_SET)) == -1) {
		status = errno;
		(void)fprintf( stderr, "%s: local_lockf: unable to lseek %s: %s\n",
			Progname, lap->la_name, strerror(status) );
	} else if (offset != lap->la_offset) {
		status = -1;
		(void)fprintf( stderr,
			"%s: local_lockf: bad lseek for %s: returned %d expected %d\n",
			Progname, lap->la_name, offset, lap->la_offset );
	} else if ( lockf( fd, lap->la_func, lap->la_size ) ) {
		status = errno;
		(void)fprintf( stderr, "%s: local_lockf: unable to lock %s: %s\n",
			Progname, lap->la_name, strerror(status) );
	}
	return(status);
}
