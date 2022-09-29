#ifndef OPENUTIL_H
#define OPENUTIL_H

#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/common/RCS/openutil.h,v 1.1 1994/09/13 19:29:23 cbullis Exp $"

/* openutil.[hc] - useful functions for opening tmp and housekeeping
 * files.
 */


/* allocate and fill a character sting buffer with a pathname generated
 * by catenating the dir and base args. if pid is non-zero, the decimal
 * representation of the pid will be appended to the pathname, beginning
 * with a '.'.
 */
extern char *open_pathalloc( char *dirname, char *basename, pid_t pid );

/* create the specified file, creating or truncating as necessary,
 * with read and write permissions, given a directory and base.
 * return the file descriptor, or -1 with errno set. uses mlog( MLOG_NORMAL...
 * if the creation fails.
 */
extern intgen_t open_trwdb( char *dirname, char *basename, pid_t pid );
extern intgen_t open_trwp( char *pathname );


/* open the specified file, with read and write permissions, given a
 * directory and base.* return the file descriptor, or -1 with errno set.
 * uses mlog( MLOG_NORMAL... if the open fails.
 */
extern intgen_t open_rwdb( char *dirname, char *basename, pid_t pid );
extern intgen_t open_rwp( char *pathname );


/* create and open the specified file, failing if already exists
 */
extern intgen_t open_erwp( char *pathname );
extern intgen_t open_erwdb( char *dirname, char *basename, pid_t pid );


/* create the specified directory, guaranteed to be initially empty. returns
 * 0 on success, -1 if trouble. uses mlog( MLOG_NORMAL... if the creation fails.
 */
extern intgen_t mkdir_tp( char *pathname );


#endif /* UTIL_H */
