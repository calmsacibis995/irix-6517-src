/* 
 * This is a test case for lockd sysid management.
 *
 * Problem:
 *		If one application acquires a lock in a file and never releases it,
 *		none of the sysids for remote applications which might at some time
 *		have locked a portion of the same file are ever released.
 *
 *		The cause of this behavior is the way in which sysids and the file
 *		descriptor table are managed in lockd.  sysid release was linked to
 *		file descriptor release.  File descriptor release was linked to
 *		there being no locks held in the file, either through the
 *		underlying local file system or from NFS mounts.
 *
 * Solution:
 *		Decouple sysid and file descriptor release by keeping track of how
 *		many active locks a sysid has.  An active lock is one for which
 *		no corresponding unlock request has been received.  There may not
 *		be as many unlock requests as lock requests.  An example of this is
 *		a process which acquires locks but does not explicitly release them.
 *		It releases them by default by closing the file.
 *
 * Demonstration of the problem:
 *		The problem may be easily demonstrated as follows:
 *
 *			1. construct a file with two regions to be locked
 *			2. one process acquires a read lock on one region
 *			3. repeatedly fork a process to lock and unlock the other
 *			   region, continuing until the forked process exits
 *			   with a non-zero status
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/select.h>							/* to get FD_SETSIZE */
#include <sys/signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <assert.h>
#if _SUNOS
#include <rpc/rpc.h>
#endif /* _SUNOS */
#include "util.h"

#define OPTSTR					"nvr:d:f:p:"

#define DEFAULT_REPEAT			2000
#define DEFAULT_FILENAME		"sysid.testfile"
#define DEFAULT_DIR				NULL
#define DEFAULT_NOTIFY_COUNT	100

#define TEMPSIZE				256

#define MAX_VERBOSE_LEVEL		2

#define REGION_ONE				1
#define REGION_TWO				2
#define REGION_LEN				1024
#define REGION_ONE_START		0
#define REGION_TWO_START		REGION_ONE_START + REGION_LEN
#define REGIONS					2

#define BUFLEN					REGIONS * REGION_LEN

#define ERROREXIT				-1

#define OPEN_FLAGS				O_RDWR
#define CREATE_FLAGS			(OPEN_FLAGS | O_CREAT | O_TRUNC)
#define OPEN_MODE \
	(S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)
#define CREATE_MODE				OPEN_MODE

int Sigint = 0;
int Sigchld = 0;

char *Progname;

int Verbose = 0;

extern char *optarg;
extern int errno;

void
signal_handler( int sig, int code, struct sigcontext *sc )
{
	switch ( sig ) {
		case SIGINT:
			Sigint = 1;
			(void)sigignore( SIGINT );
			break;
		case SIGCHLD:
			Sigchld = 1;
			(void)sigignore( SIGCHLD );
			break;
	}
}

int
closeall( pid_t pid )
{
	int fd;
	int status = 0;

	for ( fd = 3; fd < FD_SETSIZE; fd++ ) {
		if ( (status = close( fd )) == -1 ) {
			if ( errno == EBADF ) {
				status = 0;
			} else {
				fprintf( stderr, "%s[%d]: closeall: ", Progname, pid );
				perror( "closeall" );
				status = ERROREXIT;
				break;
			}
		}
	}
	return( status );
}

int
lock_region( pid_t pid, int fd, int region, int locktype )
{
	int status = 0;
	struct flock fl;
	off_t offset;
	long len;

	switch ( region ) {
		case REGION_ONE:
			offset = REGION_ONE_START;
			break;
		case REGION_TWO:
			offset = REGION_TWO_START;
			break;
		default:
			fprintf( stderr, "%s[%d] lock_region: bad region: %d\n", Progname,
				pid, (int)region );
			errno = EINVAL;
			return( -1 );
	}
	len = REGION_LEN;
	if ( Verbose >= MAX_VERBOSE_LEVEL ) {
		printf( "%s[%d] lock_region: region %d, len %d, start %d\n", Progname,
			pid, region, len, (int)offset );
	}
	fl.l_whence = SEEK_SET;
	fl.l_start = offset;
	fl.l_len = len;
	fl.l_pid = pid;
	fl.l_type = locktype;
	if ( fcntl( fd, F_SETLKW, &fl ) == -1 ) {
		if ( errno != EINTR ) {
			fprintf( stderr, "%s[%d] lock_region: ", Progname, pid );
			perror( "fcntl" );
			status = ERROREXIT;
		}
	}
	return( status );
}

int
unlock_region( pid_t pid, int fd, int region )
{
	int status = 0;
	struct flock fl;
	off_t offset;
	long len;

	switch ( region ) {
		case REGION_ONE:
			offset = REGION_ONE_START;
			break;
		case REGION_TWO:
			offset = REGION_TWO_START;
			break;
		default:
			fprintf( stderr, "%s[%d] unlock_region: bad region: %d\n", Progname,
				pid, (int)region );
			errno = EINVAL;
			return( ERROREXIT );
	}
	len = REGION_LEN;
	if ( Verbose >= MAX_VERBOSE_LEVEL ) {
		printf( "%s[%d] unlock_region: region %d, len %d, start %d\n", Progname,
			pid, region, len, (int)offset );
	}
	fl.l_whence = SEEK_SET;
	fl.l_start = offset;
	fl.l_len = len;
	fl.l_pid = pid;
	fl.l_type = F_UNLCK;
	if ( fcntl( fd, F_SETLK, &fl ) == -1 ) {
		if ( errno != EINTR ) {
			status = errno;
			fprintf( stderr, "%s[%d] unlock_region: ", Progname, pid );
			perror( "fcntl" );
			status = ERROREXIT;
		}
	}
	return( status );
}

int
test_region( pid_t pid, int fd, int region, struct flock *fl )
{
	int status = 0;
	off_t offset;
	long len;

	assert(valid_addresses((caddr_t)fl, sizeof(*fl)));
	switch ( region ) {
		case REGION_ONE:
			offset = REGION_ONE_START;
			break;
		case REGION_TWO:
			offset = REGION_TWO_START;
			break;
		default:
			fprintf( stderr, "%s[%d] test_region: bad region: %d\n", Progname,
				pid, (int)region );
			errno = EINVAL;
			return( ERROREXIT );
	}
	len = REGION_LEN;
	if ( Verbose >= MAX_VERBOSE_LEVEL ) {
		printf( "%s[%d] test_region: region %d, len %d, start %d\n", Progname,
			pid, region, len, (int)offset );
	}
	fl->l_whence = SEEK_SET;
	fl->l_start = offset;
	fl->l_len = len;
	fl->l_pid = pid;
	fl->l_type = F_WRLCK;
	if ( fcntl( fd, F_GETLK, fl ) == -1 ) {
		if ( errno != EINTR ) {
			fprintf( stderr, "%s[%d] test_region: ", Progname, pid );
			perror( "fcntl" );
			status = ERROREXIT;
		}
	}
	return( status );
}

int
await_lock( int fd, int region )
{
	int status = 0;
	struct flock fl;

	fl.l_type = F_UNLCK;
	while ( (fl.l_type == F_UNLCK) && !status && !Sigint ) {
		status = test_region( -1, fd, region, &fl );
	}
	return( status );
}

void
lock_process1( char *filename )
{
	int status = 0;
	int fd;
	pid_t pid;

	assert(valid_addresses((caddr_t)filename, sizeof(*filename)));
	if ( signal( SIGINT, signal_handler ) == SIG_ERR ) {
		status = errno;
		fprintf( stderr, "%s[%d] lock_process1: ", Progname, pid );
		perror( "signal(SIGINT,signal_handler)" );
	} else if ( (pid = getpid()) == (pid_t)-1 ) {
		status = errno;
		fprintf( stderr, "%s[%d] lock_process1: ", Progname, pid );
		perror( "getpid" );
	} else if ( closeall( pid ) == ERROREXIT ) {
		status = errno;
	} else if ( (fd = open( filename, OPEN_FLAGS, OPEN_MODE )) == -1 ) {
		status = errno;
		fprintf( stderr, "%s[%d] lock_process1: %s: ", Progname, pid,
			filename );
		perror( "open" );
	} else if ( lock_region( pid, fd, REGION_TWO, F_RDLCK ) == ERROREXIT ) {
		status = errno;
	} else if ( unlock_region( pid, fd, REGION_TWO ) == ERROREXIT ) {
		status = errno;
	} else if ( close( fd ) == -1 ) {
		status = errno;
		fprintf( stderr, "%s[%d] lock_process1: %s: ", Progname, pid,
			filename );
		perror( "close" );
	}
	exit( status );
}

void
lock_process2( char *filename )
{
	int status = 0;
	int fd;
	pid_t pid;

	assert(valid_addresses((caddr_t)filename, sizeof(*filename)));
	if ( signal( SIGINT, signal_handler ) == SIG_ERR ) {
		status = errno;
		fprintf( stderr, "%s[%d] lock_process2: ", Progname, pid );
		perror( "signal(SIGINT,signal_handler)" );
	} else if ( (pid = getpid()) == (pid_t)-1 ) {
		status = errno;
		fprintf( stderr, "%s[%d] lock_process2: ", Progname, pid );
		perror( "getpid" );
	} else if ( closeall( pid ) == ERROREXIT ) {
		status = errno;
	} else if ( (fd = open( filename, OPEN_FLAGS, OPEN_MODE )) == -1 ) {
		status = errno;
		fprintf( stderr, "%s[%d] lock_process2: %s: ", Progname, pid,
			filename );
		perror( "open" );
	} else if ( lock_region( pid, fd, REGION_TWO, F_RDLCK ) == ERROREXIT ) {
		status = errno;
	} else if ( pause() == -1 ) {
		if ( unlock_region( pid, fd, REGION_TWO ) == ERROREXIT ) {
			status = errno;
		} else if ( close( fd ) == -1 ) {
			status = errno;
			fprintf( stderr, "%s[%d] lock_process2: %s: ", Progname, pid,
				filename );
			perror( "close" );
		}
	}
	exit( status );
}

void
lock_process3( char *filename )
{
	int status = 0;
	int fd;
	pid_t pid;
	struct flock fl1;
	struct flock fl2;

	assert(valid_addresses((caddr_t)filename, sizeof(*filename)));
	if ( signal( SIGINT, signal_handler ) == SIG_ERR ) {
		status = errno;
		fprintf( stderr, "%s[%d] lock_process3: ", Progname, pid );
		perror( "signal(SIGINT,signal_handler)" );
	} else if ( (pid = getpid()) == (pid_t)-1 ) {
		status = errno;
		fprintf( stderr, "%s[%d] lock_process3: ", Progname, pid );
		perror( "getpid" );
	} else if ( closeall( pid ) == ERROREXIT ) {
		status = errno;
	} else if ( (fd = open( filename, OPEN_FLAGS, OPEN_MODE )) == -1 ) {
		status = errno;
		fprintf( stderr, "%s[%d] lock_process3: %s: ", Progname, pid,
			filename );
		perror( "open" );
	} else if ( test_region( pid, fd, REGION_ONE, &fl1 ) == ERROREXIT ) {
		status = errno;
	} else if ( test_region( pid, fd, REGION_TWO, &fl2 ) == ERROREXIT ) {
		status = errno;
	} else if ( close( fd ) == -1 ) {
		status = errno;
		fprintf( stderr, "%s[%d] lock_process3: %s: ", Progname, pid,
			filename );
		perror( "close" );
	} else {
		if ( fl1.l_type != F_RDLCK ) {
			printf( "%s[%d] lock_process3: region 1 test failure, result is %s (expected F_RDLCK)\n",
				Progname, pid, locktype_to_str( fl1.l_type ) );
			status = ERROREXIT;
		}
		if ( fl2.l_type != F_UNLCK ) {
			printf( "%s[%d] lock_process3: region 2 test failure, result is %s (expected F_UNLCK)\n",
				Progname, pid, locktype_to_str( fl2.l_type ) );
			printf( "%s[%d] lock_process3: region 2 lock offset %d, len %d, pid %d, sysid %d\n",
				Progname, pid, fl2.l_start, fl2.l_len, fl2.l_pid,
				fl2.l_sysid );
			status = ERROREXIT;
		}
	}
	exit( status );
}

void
lock_process4( char *filename )
{
	int status = 0;
	int fd;
	pid_t pid;
	int i;
	struct flock flk;

	assert(valid_addresses((caddr_t)filename, sizeof(*filename)));
	if ( signal( SIGINT, signal_handler ) == SIG_ERR ) {
		status = ERROREXIT;
		fprintf( stderr, "%s[%d] lock_process4: ", Progname, pid );
		perror( "signal(SIGINT,signal_handler)" );
	} else if ( (pid = getpid()) == (pid_t)-1 ) {
		switch ( errno ) {
			case EINTR:
				break;
			default:
				status = ERROREXIT;
				fprintf( stderr, "%s[%d] lock_process4: ", Progname, pid );
				perror( "getpid" );
		}
	} else if ( closeall( pid ) == ERROREXIT ) {
		status = ERROREXIT;
	} else if ( (fd = open ( filename , O_RDWR, 0666 )) == -1 ) {
		switch ( errno ) {
			case EINTR:
				break;
			default:
				fprintf( stderr, "%s[%d] lock_process4: %s: ", Progname, pid,
					filename );
				perror( "open" );
				status = ERROREXIT;
		}
	} else {
		/*
		 * read lock from offset 2 to the end of the file
		 */
		flk.l_whence = 0;
		flk.l_start = 2;
		flk.l_len = 0;
		flk.l_type = F_RDLCK;
		if (fcntl (fd, F_SETLK, &flk) == -1) {
			switch ( errno ) {
				case EINTR:		/* ignore EINTR */
					break;
				default:
					fprintf ( stderr,
						"%s[%d] lock_process4: offset %d, len %d ", Progname,
						pid, (int)flk.l_start, (int)flk.l_len );
					perror("fcntl F_RDLCK 2 0");
					status = ERROREXIT;
			}
		} else {
			/*
			 * write lock byte 0 of the file
			 */
			flk.l_whence = 0;
			flk.l_start = 0;
			flk.l_len = 1;
			flk.l_type = F_WRLCK;
			if (fcntl (fd, F_SETLK, &flk) == -1) {
				switch ( errno ) {
					case EINTR:		/* ignore EINTR */
						break;
					default:
						status = ERROREXIT;
						fprintf ( stderr,
							"%s[%d] lock_process4: offset %d, len %d ",
							Progname, pid, (int)flk.l_start, (int)flk.l_len );
						perror("fcntl F_WRLCK");
				}
			} else {
				/*
				 * read lock byte 1
				 */
				flk.l_whence = 0;
				flk.l_start = 1;
				flk.l_len = 1;
				flk.l_type = F_RDLCK;
				if (fcntl (fd, F_SETLK, &flk) == -1) {
					switch ( errno ) {
						case EINTR:		/* ignore EINTR */
							break;
						default:
							status = ERROREXIT;
							fprintf ( stderr,
								"%s[%d] lock_process4: offset %d, len %d ",
								Progname, pid, (int)flk.l_start,
								(int)flk.l_len );
							perror("fcntl F_RDLCK");
					}
				} else {
					/*
					 * unlock byte 0
					 */
					flk.l_whence = 0;
					flk.l_start = 0;
					flk.l_len = 1;
					flk.l_type = F_UNLCK;
					if (fcntl (fd, F_SETLK, &flk) == -1) {
						switch ( errno ) {
							case EINTR:		/* ignore EINTR */
								break;
							default:
								status = ERROREXIT;
								fprintf ( stderr,
									"%s[%d] lock_process4: offset %d, len %d ",
									Progname, pid, (int)flk.l_start,
									(int)flk.l_len );
								perror("fcntl F_UNLCK");
						}
					}
				}
			}
		}
		if (close (fd) == -1) {
			switch ( errno ) {
				case EINTR:
					break;
				default:
					fprintf ( stderr, "%s[%d] lock_process4: ", Progname, pid );
					perror("close");
					status = ERROREXIT;
			}
		}
	}
	exit( status );
}

void
lock_process5( char *filename )
{
	int status = 0;
	int fd;
	pid_t pid;
	off_t len;
	struct flock flk;

	assert(valid_addresses((caddr_t)filename, sizeof(*filename)));
	if ( signal( SIGINT, signal_handler ) == SIG_ERR ) {
		status = ERROREXIT;
		fprintf( stderr, "%s[%d] lock_process5: ", Progname, pid );
		perror( "signal(SIGINT,signal_handler)" );
	} else if ( (pid = getpid()) == (pid_t)-1 ) {
		switch ( errno ) {
			case EINTR:
				break;
			default:
				status = ERROREXIT;
				fprintf( stderr, "%s[%d] lock_process5: ", Progname, pid );
				perror( "getpid" );
		}
	} else if ( closeall( pid ) == ERROREXIT ) {
		status = ERROREXIT;
	} else {
		for ( len = 3; len >= 0; len -- ) {
			if ( (fd = open(filename, O_RDWR, 0666)) == -1 ) {
				switch ( errno ) {
					case EINTR:
						break;
					default:
						status = ERROREXIT;
						fprintf( stderr, "%s[%d] lock_process5: %s, len %d: ",
							Progname, pid, filename, len );
						perror( "open" );
				}
			} else if ( ftruncate( fd, len ) == -1 ) {
				switch ( errno ) {
					case EINTR:
						break;
					default:
						status = ERROREXIT;
						fprintf( stderr, "%s[%d] lock_process5: %s, len %d: ",
							Progname, pid, filename, len );
						perror( "ftruncate" );
				}
			} else {
				/*
				 * read lock from offset 2 to the end of the file
				 */
				flk.l_whence = 0;
				flk.l_start = 2;
				flk.l_len = 0;
				flk.l_type = F_RDLCK;
				if (fcntl (fd, F_SETLK, &flk) == -1) {
					switch ( errno ) {
						case EINTR:
							break;
						default:
							fprintf ( stderr, "%s[%d] lock_process5: file len "
								"%d, lock offset %d, len %d ", Progname, pid,
								len, (int)flk.l_start, (int)flk.l_len );
							perror("fcntl F_RDLCK");
							status = ERROREXIT;
					}
				} else {
					/*
					 * write lock byte 0 of the file
					 */
					flk.l_whence = 0;
					flk.l_start = 0;
					flk.l_len = 1;
					flk.l_type = F_WRLCK;
					if (fcntl (fd, F_SETLK, &flk) == -1) {
						switch ( errno ) {
							case EINTR:
								break;
							default:
								fprintf ( stderr, "%s[%d] lock_process5: file "
									"len %d, lock offset %d, len %d ",
									Progname, pid, len, (int)flk.l_start,
									(int)flk.l_len );
								perror("fcntl F_WRLCK");
								status = ERROREXIT;
						}
					} else {
						/*
						 * read lock byte 1
						 */
						flk.l_whence = 0;
						flk.l_start = 1;
						flk.l_len = 1;
						flk.l_type = F_RDLCK;
						if (fcntl (fd, F_SETLK, &flk) == -1) {
							switch ( errno ) {
								case EINTR:
									break;
								default:
									fprintf ( stderr, "%s[%d] lock_process5: "
										"file len %d, lock offset %d, len %d ",
										Progname, pid, len, (int)flk.l_start,
										(int)flk.l_len );
									perror("fcntl F_RDLCK");
									status = ERROREXIT;
							}
						} else {
							/*
							 * unlock byte 0
							 */
							flk.l_whence = 0;
							flk.l_start = 0;
							flk.l_len = 1;
							flk.l_type = F_UNLCK;
							if (fcntl (fd, F_SETLK, &flk) == -1) {
								switch ( errno ) {
									case EINTR:
										break;
									default:
										fprintf ( stderr,
											"%s[%d] lock_process5: file len "
											"%d, lock offset %d, len %d ",
											Progname, pid, len,
											(int)flk.l_start, (int)flk.l_len );
										perror("fcntl F_UNLCK 0 1");
										status = ERROREXIT;
								}
							}
						}
					}
				}
				if (close (fd) == -1) {
					switch ( errno ) {
						case EINTR:
							break;
						default:
							fprintf ( stderr, "%s[%d] lock_process5: file len "
								"%d ", Progname, pid, len );
							perror("close");
							status = ERROREXIT;
					}
				}
			}
		}
	}
	exit( status );
}

void
usage(void)
{
	char *op = OPTSTR;

	fprintf( stderr, "usage %s", Progname );
	while ( *op ) {
		if ( *(op + 1) == ':' ) {
			fprintf( stderr, " [-%c arg]", *op );
			op++;
		} else {
			fprintf( stderr, " [-%c]", *op );
		}
		op++;
	}
	fprintf( stderr, "\n" );
}

int
fork_proc( void (*func)(char *), char *filename, pid_t *pid )
{
	int status = 0;

	assert(valid_addresses((caddr_t)filename, sizeof(*filename)));
	assert(valid_addresses((caddr_t)pid, sizeof(*pid)));
	switch ( *pid = fork() ) {
		case 0:		/* child process */
			(*func)( filename );
			fprintf( stderr, "%s[child] fork_proc: ", Progname );
			printf( "%s[%d] fork_one_proc: exiting with status %d\n",
				Progname, *pid, ERROREXIT );
			exit( ERROREXIT );
			break;
		case -1:	/* fork error */
			if ( errno != EINTR ) {
				fprintf( stderr, "%s fork_proc: ", Progname );
				perror( "fork" );
				status = errno;
			}
			break;
	}
	return( status );
}

build_file( char *filename, int size )
{
	int i;
	int status = 0;
	int len;
	int fd = -1;
	char *mbp;
	int n;
	struct stat statbuf;
	char filebuf[BUFLEN];

	assert(valid_addresses((caddr_t)filename, sizeof(*filename)));
	if ( (status = stat( filename, &statbuf )) ||
		(statbuf.st_size < size) ) {
			if ( Verbose >= MAX_VERBOSE_LEVEL ) {
				(void)printf( "%s: build_file: file %s, size %d\n", Progname,
					filename, size );
				(void)fflush( stdout );
			}
			/*
			 * if the file exists, we need to change its mode
			 */
			if ( status == 0 ) {
				if ( chmod( filename, CREATE_MODE ) == -1 ) {
					fprintf( stderr, "%s build_file: %s: ", Progname,
						filename );
					perror( "chmod" );
					status = errno;
					return( status );
				}
			}
			status = 0;
			if ( (fd = open( filename, CREATE_FLAGS, CREATE_MODE )) == -1 ) {
				fprintf( stderr, "%s build_file: %s: ", Progname, filename );
				perror( "open" );
				status = errno;
			} else {
				/*
				 * fill in the file buffer
				 */
				for ( i = 0; i < BUFLEN; i++ ) {
					filebuf[i] = (char)(i % 128);
				}
				while ( size && !status ) {
					/*
					 * set the write length
					 * this will be BUFLEN or size
					 * size indicates the remaining length to
					 * write
					 */
					if ( size >= BUFLEN ) {
						len = BUFLEN;
					} else {
						len = size;
					}
					/*
					 * decrement the remaining size
					 */
					size -= len;
					/*
					 * set the buffer pointer
					 */
					mbp = filebuf;
					/*
					 * keep writing until we hit a zero-length write
					 * or a write error
					 */
					do {
						n = write( fd, mbp, len );
						if ( n == -1 ) {
							fprintf( stderr, "%s build_file: %s: ", Progname,
								filename );
							perror( "write" );
							status = errno;
							break;
						} else if ( n == 0 ) {
							fprintf( stderr,
								"%s build_file: %s: zero-byte write\n",
								Progname, filename );
							status = ERROREXIT;
							break;
						} else {
							len -= n;
							mbp += n;
						}
					} while ( len );
				}
				if ( close( fd ) == -1 ) {
					fprintf( stderr, "%s build_file: %s: ", Progname,
						filename );
					perror( "close" );
					status = errno;
				} else if ( chmod( filename, CREATE_MODE ) == -1 ) {
					fprintf( stderr, "%s build_file: %s: ", Progname,
						filename );
					perror( "chmod" );
					status = errno;
				}
			}
	} else if ( ((statbuf.st_mode & CREATE_MODE) != CREATE_MODE) &&
		(chmod( filename, CREATE_MODE ) == -1) ) {
			fprintf( stderr, "%s build_file: %s: ", Progname, filename );
			perror( "chmod" );
			status = errno;
	}
	return( status );
}

int
reap_process( int expected_sig )
{
	int status = 0;
	pid_t pid;
	int child_status;
	int sig;

	if ( (pid = wait( &child_status )) == -1 ) {
		if ( errno != EINTR ) {
			fprintf( stderr, "%s main: ", Progname );
			perror( "wait" );
			status = errno;
		}
	} else if ( WIFSTOPPED( child_status ) ) {
		fprintf( stderr, "%s: child stopped: pid %d, signal %d\n",
			Progname, pid, WSTOPSIG( child_status ) );
		fflush( stderr );
		status = ERROREXIT;
	} else if ( WIFEXITED( child_status ) ) {
		status = WEXITSTATUS( child_status );
		if ( status != 0 ) {
			fprintf( stderr, "child exited: status %d, pid %d\n",
				status, pid );
		}
	} else if ( WIFSIGNALED( child_status ) ) {
		sig = WTERMSIG( child_status );
		if ( (sig != expected_sig) && (sig != EINTR) ) {
			fprintf( stderr, "%s: child terminated on signal %d\n",
				Progname, sig );
			fflush( stderr );
			status = ERROREXIT;
		}
	} else {
		fprintf( stderr, "%s: wait returned unknown status: 0x%x\n",
			Progname, child_status );
		fflush( stderr );
		status = ERROREXIT;
	}
	return( status );
}

int
verify_lock( int fd, int region, int expect )
{
	int status = 0;
	struct flock fl1;
	struct flock fl2;

	if ( region ) {
		if ( region == REGION_ONE ) {
			if ( test_region( -1, fd, REGION_ONE, &fl1 ) == ERROREXIT ) {
				status = errno;
			} else if ( fl1.l_type != expect ) {
				printf( "%s: verify_lock: region 1 not as expected: got %s, expected %s\n",
					Progname, locktype_to_str( fl1.l_type ),
					locktype_to_str( expect ) );
				status = ERROREXIT;
			}
		} else if ( region == REGION_TWO ) {
			if ( test_region( -1, fd, REGION_TWO, &fl2 ) == ERROREXIT ) {
				status = errno;
			} else if ( fl2.l_type != expect ) {
				printf( "%s: verify_lock: region 2 not as expected: got %s, expected %s\n",
					Progname, locktype_to_str( fl2.l_type ),
					locktype_to_str( expect ) );
				status = ERROREXIT;
			}
		}
	} else if ( (test_region( -1, fd, REGION_ONE, &fl1 ) == ERROREXIT)  ||
		(test_region( -1, fd, REGION_TWO, &fl2 ) == ERROREXIT) ) {
			status = errno;
	} else if ( (fl1.l_type != F_UNLCK) || (fl2.l_type != F_UNLCK) ) {
		printf(
			"%s: verify_lock: region still locked: region 1 %s, region 2 %s\n",
			Progname, locktype_to_str( fl1.l_type ),
			locktype_to_str( fl2.l_type ) );
		status = ERROREXIT;
	}
	return( status );
}

main( int argc, char **argv )
{
	int status = 0;
	int opt;
	int repeat = DEFAULT_REPEAT;
	char *filename = DEFAULT_FILENAME;
	char *tempname = "sysidtest.temp";
	char *dir = DEFAULT_DIR;
	int fd;
	pid_t pid;
	int i;
	int phase = 0;
	int notify_count = DEFAULT_NOTIFY_COUNT;

	Progname = *argv;
	while ( (opt = getopt( argc, argv, OPTSTR )) != EOF ) {
		switch ( opt ) {
			case 'p':
				phase = atoi( optarg );
				break;
			case 'r':
				repeat = atoi( optarg );
				break;
			case 'f':
				filename = optarg;
				break;
			case 'd':
				dir = optarg;
				break;
			case 'v':
				Verbose++;
				break;
			case 'n':
				notify_count = atoi( optarg );
				break;
			default:
				usage();
		}
	}
	printf( "Test Parameters:\n" );
	printf( "\t   directory = %s\n", dir ? dir : "." );
	printf( "\t   test file = %s\n", filename );
	printf( "\trepeat count = %d\n", repeat );
	if ( dir && (chdir( dir ) == -1) ) {
		if ( errno != EINTR ) {
			status = errno;
			fprintf( stderr, "%s: ", Progname );
			perror( "chdir" );
		}
	} else if ( status = build_file( filename, REGIONS * REGION_LEN ) ) {
		;
	} else if ( (fd = open( filename, OPEN_FLAGS, OPEN_MODE )) == -1 ) {
		fprintf( stderr, "%s main: %s: ", Progname, filename );
		perror( "open" );
		status = errno;
	} else if ( lock_region( -1, fd, REGION_ONE, F_RDLCK ) == ERROREXIT ) {
		status = errno;
	} else if ( repeat == -1 ) {
		repeat = 0;
		while ( !Sigint && !status ) {
			if ( Verbose && ((++repeat % notify_count) == 0) ) {
				printf( "Starting pass %d\n", repeat );
			}
			if ( status = verify_lock( fd, REGION_TWO, F_UNLCK ) ) {
				printf( "Test failure in phase 1\n" );
				break;
			} else if ( status = fork_proc( lock_process1, filename, &pid ) ) {
				printf( "Test failure in phase 1\n" );
				break;
			} else if ( status = reap_process( 0 ) ) {
				printf( "Test failure in phase 1\n" );
				break;
			} else if ( Sigint ) {
				break;
			} else if ( status = verify_lock( fd, REGION_TWO, F_UNLCK ) ) {
				printf( "Test failure in phase 2\n" );
				break;
			} else if ( status = fork_proc( lock_process2, filename, &pid ) ) {
				printf( "Test failure in phase 2\n" );
				break;
			} else if ( (kill( pid, SIGTERM ) == -1) && (errno != ESRCH) ) {
				fprintf( stderr, "%s: ", Progname );
				perror( "kill" );
				break;
			} else if ( status = reap_process( SIGTERM ) ) {
				printf( "Test failure in phase 2\n" );
				break;
			} else if ( Sigint ) {
				break;
			} else if ( status = verify_lock( fd, REGION_TWO, F_UNLCK ) ) {
				printf( "Test failure in phase 3\n" );
				break;
			} else if ( status = fork_proc( lock_process3, filename, &pid ) ) {
					printf( "Test failure in phase 3\n" );
					break;
			} else if ( status = reap_process( 0 ) ) {
				printf( "Test failure in phase 3\n" );
				break;
			} else if ( Sigint ) {
				break;
			} else if ( (status = build_file( tempname, TEMPSIZE )) ||
				(status = fork_proc( lock_process4, tempname, &pid )) ) {
					printf( "Test failure in phase 4\n" );
					break;
			} else if ( status = reap_process( 0 ) ) {
				printf( "Test failure in phase 4\n" );
				break;
			} else if ( Sigint ) {
				break;
			} else if ( status = fork_proc( lock_process5, tempname, &pid ) ) {
					printf( "Test failure in phase 5\n" );
					break;
			} else if ( status = reap_process( 0 ) ) {
				printf( "Test failure in phase 5\n" );
				break;
			}
		}
	} else {
		if ( !(status = build_file( tempname, TEMPSIZE )) ) {
			/*
			 * phase 1
			 */
			if ( (!phase || (phase == 1)) &&
				!(status = verify_lock( fd, REGION_TWO, F_UNLCK)) ) {
					printf( "Test phase 1\n" );
					for ( i = 0; (i < repeat) && !status && !Sigint; i++ ) {
						if ( Verbose && ((i % notify_count) == 0) ) {
							printf( "Starting pass %d\n", i + 1 );
						}
						if ( status = fork_proc( lock_process1, filename,
							&pid ) ) {
								printf( "Test failure on pass %d, phase 1\n",
									i + 1 );
								break;
						} else if ( status = reap_process( 0 ) ) {
							printf( "Test failure on pass %d, phase 1\n",
								i + 1 );
							break;
						}
					}
			}
			/*
			 * phase 2
			 */
			if ( !status && (!phase || (phase == 2)) &&
				!(status = verify_lock( fd, REGION_TWO, F_UNLCK)) ) {
					printf( "Test phase 2\n" );
					for ( i = 0; (i < repeat) && !status && !Sigint; i++ ) {
						if ( Verbose && ((i % notify_count) == 0) ) {
							printf( "Starting pass %d\n", i + 1 );
						}
						if ( status = fork_proc( lock_process2, filename,
							&pid ) ) {
								printf( "Test failure on pass %d, phase 2\n",
									i + 1 );
								break;
						} else if ( status = await_lock( fd, REGION_TWO ) ) {
							printf( "Test failure on pass %d, phase 2\n",
								i + 1 );
							break;
						} else if ( (kill( pid, SIGTERM ) == -1) &&
							(errno != ESRCH) ) {
								fprintf( stderr, "%s: ", Progname );
								perror( "kill" );
								printf( "Test failure on pass %d, phase 2\n",
									i + 1 );
								break;
						} else if ( status = reap_process( SIGTERM ) ) {
							printf( "Test failure on pass %d, phase 2\n",
								i + 1 );
							break;
						}
					}
			}
			/*
			 * phase 3
			 */
			if ( !status && (!phase || (phase == 3)) &&
				!(status = verify_lock( fd, REGION_TWO, F_UNLCK)) ) {
					printf( "Test phase 3\n" );
					for ( i = 0; (i < repeat) && !status && !Sigint; i++ ) {
						if ( Verbose && ((i % notify_count) == 0) ) {
							printf( "Starting pass %d\n", i + 1 );
						}
						if ( status = fork_proc( lock_process3, filename,
							&pid ) ) {
								printf( "Test failure on pass %d, phase 3\n",
									i + 1 );
								break;
						} else if ( status = reap_process( 0 ) ) {
							printf( "Test failure on pass %d, phase 3\n",
								i + 1 );
							break;
						}
					}
			}
			/*
			 * phase 4
			 */
			if ( !status && (!phase || (phase == 4)) ) {
				printf( "Test phase 4\n" );
				for ( i = 0; (i < repeat) && !status && !Sigint; i++ ) {
					if ( Verbose && ((i % notify_count) == 0) ) {
						printf( "Starting pass %d\n", i + 1 );
					}
					if ( status = fork_proc( lock_process4, tempname, &pid ) ) {
						printf( "Test failure on pass %d, phase 4\n", i + 1 );
						break;
					} else if ( status = reap_process( 0 ) ) {
						printf( "Test failure on pass %d, phase 4\n", i + 1 );
						break;
					}
				}
			}
			/*
			 * phase 5
			 */
			if ( !status && (!phase || (phase == 5)) ) {
				printf( "Test phase 5\n" );
				for ( i = 0; (i < repeat) && !status && !Sigint; i++ ) {
					if ( Verbose && ((i % notify_count) == 0) ) {
						printf( "Starting pass %d\n", i + 1 );
					}
					if ( (status = build_file( tempname, TEMPSIZE )) ||
						(status = fork_proc( lock_process5, tempname, &pid ))
						) {
							printf( "Test failure on pass %d, phase 5\n",
								i + 1 );
							break;
					} else if ( status = reap_process( 0 ) ) {
						printf( "Test failure on pass %d, phase 5\n", i + 1 );
						break;
					}
				}
			}
		}
	}
	sigignore( SIGINT );
	kill( 0, SIGINT );
	if ( !status ) {
		status = verify_lock( fd, REGION_TWO, F_UNLCK );
	}
	unlock_region( -1, fd, REGION_ONE );
	close( fd );
	if ( status == 0 ) {
		printf( "%s: Test passed\n", Progname );
	} else {
		printf( "%s: Test failed\n", Progname );
	}
	unlink( tempname );
	exit( status );
}
