#include <stdio.h>
#include <limits.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#if !SVR4
#include <strings.h>
#endif /* !SVR4 */
#include <stdlib.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include "defaults.h"
#include "parameters.h"
#include "print.h"
#if _SUNOS
#include <rpc/rpc.h>
#endif /* _SUNOS */
#include "util.h"

#define USEC_PER_SEC		1000000
#define OPTSTR				"kvst:"

char *Progname = "lock_stress";
int Sigint = 0;
int Sigchld = 0;
int Verbose = DEFAULT_VERBOSE;
FILE *Tracefp = NULL;
char *TraceFile = NULL;
int Statistics = 0;
int ActiveProcs = 0;
int ForkCount = 0;

struct lock_stats {
	long	ls_acquire;		/* total locks acquired */
	long	ls_block;		/* number of blocked requests */
	float	ls_blocktime;	/* average time blocked on a lock */
};
struct lock_stats LockStats;

char *SigNames[] = {
	"NULLSIG",
	"SIGHUP",
	"SIGINT",
	"SIGQUIT",
	"SIGILL",
	"SIGTRAP",
	"SIGABRT",
	"SIGEMT",
	"SIGFPE",
	"SIGKILL",
	"SIGBUS",
	"SIGSEGV",
	"SIGSYS",
	"SIGPIPE",
	"SIGALRM",
	"SIGTERM",
	"SIGUSR1",
	"SIGUSR2",
	"SIGCLD",
	"SIGPWR",
	"SIGWINCH",
	"SIGURG",
	"SIGPOLL",
	"SIGSTOP",
	"SIGTSTP",
	"SIGCONT",
	"SIGTTIN",
	"SIGTTOU",
	"SIGVTALRM",
	"SIGPROF",
	"SIGXCPU",
	"SIGXFSZ",
#if _SUNOS
	"SIGWAITING",
	"SIGLWP",
	"SIGFREEZE",
	"SIGTHAW",
#endif /* _SUNOS */
	""
};
#define KNOWNSIGS	(sizeof(SigNames) / sizeof(char *))

#define TV_TO_FLOAT( tv )	\
	((float)((tv).tv_sec) + ((float)((tv).tv_usec))/USEC_PER_SEC)

extern int errno;

void
sig_handler( int sig, int code, struct sigcontext *sc )
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

#if _IRIX53
int
usleep( long usec )
{
	int status = 0;

	if ( sginap( (usec * CLK_TCK) / USEC_PER_SEC ) == -1 ) {
		status = -1;
	}
	return( status );
}
#endif /* _IRIX53 */

int
reclock( int fd, pid_t pid, off_t offset, long len )
{
	int status = 0;
	struct flock fl;
	struct timeval t1;
	struct timeval t2;

	LockStats.ls_acquire++;
	fl.l_whence = SEEK_SET;
	fl.l_start = offset;
	fl.l_len = len;
	fl.l_pid = pid;
	fl.l_type = ((int)(drand48() * 2)) ? F_WRLCK : F_RDLCK;
	if ( Tracefp ) {
		fprintf( Tracefp, "reclock[%d]: offset 0x%x, len %d, type %s\n", pid,
			(int)offset, len, locktype_to_str( fl.l_type ) );
		fflush( Tracefp );
	}
	if ( fcntl( fd, F_SETLK, &fl ) == -1 ) {
		switch ( errno ) {
			case EINTR:
				if ( Tracefp ) {
					fprintf( Tracefp, "reclock[%d]: interrupted\n", pid );
					fflush( Tracefp );
				}
				break;
			case EAGAIN:
			case EACCES:
				if ( gettimeofday( &t1 ) == -1 ) {
					if ( errno != EINTR ) {
						status = errno;
						error( pid, "reclock", "gettimeofday",
							"error getting t1" );
					}
					if ( Tracefp ) {
						fprintf( Tracefp, "reclock[%d]: error %d\n", pid,
							errno );
						fflush( Tracefp );
					}
				} else if ( fcntl( fd, F_SETLKW, &fl ) == -1 ) {
					if ( errno != EINTR ) {
						status = errno;
						error( pid, "reclock", "fcntl", NULL );
					}
					if ( Tracefp ) {
						fprintf( Tracefp, "reclock[%d]: error %d\n", pid,
							errno );
						fflush( Tracefp );
					}
				} else if ( Tracefp ) {
					fprintf( Tracefp, "reclock[%d]: lock acquired\n", pid );
					fflush( Tracefp );
				}
				if ( gettimeofday( &t2 ) == -1 ) {
					if ( errno != EINTR ) {
						status = errno;
						error( pid, "reclock", "gettimeofday",
							"error getting t2" );
					}
					if ( Tracefp ) {
						fprintf( Tracefp, "reclock[%d]: error %d\n", pid,
							errno );
						fflush( Tracefp );
					}
				} else {
					LockStats.ls_block++;
					LockStats.ls_blocktime = (LockStats.ls_blocktime *
						(LockStats.ls_block - 1) + TV_TO_FLOAT(t2) -
						TV_TO_FLOAT(t1)) / LockStats.ls_block;
				}
				break;
			default:
				status = errno;
				error( pid, "reclock", "fcntl", NULL );
				if ( Tracefp ) {
					fprintf( Tracefp, "reclock[%d]: error %d\n", pid, errno );
					fflush( Tracefp );
				}
		}
	} else if ( Tracefp ) {
		fprintf( Tracefp, "reclock[%d]: lock acquired\n", pid );
		fflush( Tracefp );
	}
	return( status );
}

int
recunlock( int fd, pid_t pid, off_t offset, long len )
{
	int status = 0;
	struct flock fl;

	if ( Tracefp ) {
		fprintf( Tracefp, "recunlock[%d]: offset 0x%x, len %d\n", pid,
			(int)offset, len );
		fflush( Tracefp );
	}
	fl.l_whence = SEEK_SET;
	fl.l_start = offset;
	fl.l_len = len;
	fl.l_pid = pid;
	fl.l_type = F_UNLCK;
	if ( fcntl( fd, F_SETLK, &fl ) == -1 ) {
		if ( errno != EINTR ) {
			status = errno;
			error( pid, "recunlock", "fcntl", NULL );
		}
		if ( Tracefp ) {
			fprintf( Tracefp, "recunlock[%d]: error %d\n", pid, errno );
			fflush( Tracefp );
		}
	}
	if ( Tracefp ) {
		fprintf( Tracefp, "recunlock[%d]: done (status %d, error %d)\n", pid,
			status, errno );
		fflush( Tracefp );
	}
	return( status );
}

int
readop( int fd, pid_t pid, long len )
{
	int status = 0;
	int bytes;
	char buf[BUFLEN];

	if ( Tracefp ) {
		fprintf( Tracefp, "readop[%d]: %d bytes\n", pid, len );
		fflush( Tracefp );
	}
	while ( len && !status ) {
		switch ( bytes = read( fd, buf, MIN( len, BUFLEN ) ) ) {
			case 0:		/* EOF -- just ignore and return 0 */
				len = 0;
				break;
			case -1:	/* read error */
				if ( errno != EINTR ) {
					error( pid, "readop", "read", NULL );
					status = errno;
				}
				if ( Tracefp ) {
					fprintf( Tracefp, "readop[%d]: error %d\n", pid, errno );
					fflush( Tracefp );
				}
				break;
			default:
				len -= bytes;
		}
	}
	return( status );
}

int
lockop( int fd, pid_t pid, struct parameters *paramp )
{
	int status = 0;
	long recsize;
	long rangeval;
	off_t offset;

	assert( valid_parameters( paramp ) );
	if ( (offset = lseek( fd, (off_t)0, SEEK_CUR )) == -1 ) {
		error( pid, "lockop", "lseek(fd, 0, SEEK_CUR)", NULL );
		status = errno;
	} else {
		rangeval = paramp->p_recsize.r_high - paramp->p_recsize.r_low;
		if ( rangeval == 0 ) {
			recsize = paramp->p_recsize.r_low;
		} else {
			recsize = (long)(drand48() * (rangeval + 1)) +
				paramp->p_recsize.r_low;
		}
		switch ( paramp->p_access ) {
			case RANDOM:
				/*
				 * for random access, select the file offset, lock the record,
				 * read recsize bytes, unlock the record
				 */
				offset = (long)(drand48() * (paramp->p_filesize - recsize));
				if ( (offset < 0) ||
					(offset >= (paramp->p_filesize - recsize)) ) {
						fprintf( stderr, "%s: lockop: bad offset: %d\n",
							Progname, (int)offset );
						fprintf( stderr,
							"%s: lockop: expected offset range [0,%d)\n",
							Progname, paramp->p_filesize - recsize );
						assert( (offset >= 0) &&
							(offset < (paramp->p_filesize - recsize)) );
				}
				if ( lseek( fd, offset, SEEK_SET ) == -1 ) {
					error( pid, "lockop", "lseek(fd, offset, SEEK_SET)", NULL );
					status = errno;
				} else if ( !(status = reclock( fd, pid, offset, recsize )) ) {
					if ( !(status = readop( fd, pid, recsize )) ) {
						status = recunlock( fd, pid, offset, recsize );
					} else {
						(void)recunlock( fd, pid, offset, recsize );
					}
				}
				break;
			case SEQUENTIAL:
				/*
				 * for sequential access, lock the record, read recsize
				 * bytes, unlock the record
				 */
				if ( (paramp->p_filesize == offset) &&
					((offset = lseek( fd, 0, SEEK_SET )) == -1) ) {
						error( pid, "lockop", "lseek(fd, 0, SEEK_SET)", NULL );
						status = errno;
				} else {
					recsize = MIN( recsize, paramp->p_filesize - offset );
					if ( !(status = reclock( fd, pid, offset, recsize )) ) {
						if ( !(status = readop( fd, pid, recsize )) ) {
							status = recunlock( fd, pid, offset, recsize );
						} else {
							(void)recunlock( fd, pid, offset, recsize );
						}
					}
				}
				break;
			case BADMETHOD:
				error( pid, "lockop", NULL,
					"bad access method parameter: BADMETHOD" );
				status = -1;
				break;
			default:
				fprintf( stderr,
					"%s[%d] lockop: bad access method parameter: 0x%x\n",
					Progname, pid, (int)paramp->p_access );
				fflush( stderr );
				status = -1;
		}
	}
	return( status );
}

int
do_lock_operations( int fd, pid_t pid, struct parameters *paramp )
{
	int status = 0;

	assert( valid_parameters( paramp ) );
	bzero( &LockStats, sizeof( LockStats ) );
	if ( signal( SIGINT, sig_handler ) == SIG_ERR ) {
		error( pid, "do_lock_operations", "signal(SIGINT)", NULL );
		status = errno;
	} else if ( paramp->p_repeat == -1 ) {
		while ( !Sigint ) {
			if ( status = lockop( fd, pid, paramp ) ) {
				break;
			} else if ( usleep( paramp->p_pausetime ) == -1 ) {
				if ( errno != EINTR ) {
					error( pid, "do_lock_operations", "usleep", NULL );
					break;
				}
			}
		}
	} else {
		while ( !Sigint && paramp->p_repeat-- ) {
			if ( status = lockop( fd, pid, paramp ) ) {
				break;
			} else if ( usleep( paramp->p_pausetime ) == -1 ) {
				if ( errno != EINTR ) {
					error( pid, "do_lock_operations", "usleep", NULL );
					break;
				}
			}
		}
	}
	return( Sigint ? 0 : status );
}

void
report_stats( pid_t pid )
{
	if ( Statistics ) {
		printf( "Lock Statistics[%d]: acquire: %d, block: %d, wait: %f sec\n",
			pid, LockStats.ls_acquire, LockStats.ls_block,
			LockStats.ls_blocktime );
	}
}

void
stress_process( pid_t pid, struct parameters *paramp )
{
	int status = 0;
	int fd;
	char name[1024];

	assert( valid_parameters( paramp ) );
	if ( TraceFile ) {
		sprintf( name, "%s.%d", TraceFile, pid );
		if ( !(Tracefp = fopen( name, "w" )) ) {
			error( pid, "stress_process", "fopen", TraceFile );
			status = errno ? errno : ERROREXIT;
		}
	}
	if ( (fd = open( paramp->p_file, OPEN_FLAGS, OPEN_MODE )) == -1 ) {
		if ( errno != EINTR ) {
			status = errno;
			error( pid, "stress_process", "open", paramp->p_file );
		}
		if ( Tracefp && fclose( Tracefp ) == EOF ) {
			error( pid, "stress_process", "fclose", TraceFile );
		}
	} else {
		srand48( time( NULL ) );
		status = do_lock_operations( fd, pid, paramp );
		report_stats( pid );
		if ( close( fd ) == -1 ) {
			error( pid, "stress_process", "close", NULL );
			if ( !status ) {
				status = errno;
			}
		}
		if ( Tracefp && fclose( Tracefp ) == EOF ) {
			error( pid, "stress_process", "fclose", TraceFile );
		}
	}
	if ( Verbose > 1 ) {
		printf( "%s[%d] stress_process: exiting with status %d\n", Progname,
			pid, status );
	}
	exit( status );
}

int
fork_one_proc( struct parameters *paramp, int *pidlist, int index )
{
	int pid;
	int status = 0;

	switch ( pid = fork() ) {
		case 0:		/* child process */
			if ( (pid = getpid()) == -1 ) {
				error( pid, "fork_one_proc", "getpid", NULL );
				status = errno ? errno : ERROREXIT;
				if ( Verbose > 1 ) {
					printf( "%s[%d] fork_one_proc: exiting with status %d\n",
						Progname, pid, status );
				}
				exit( status );
			} else {
				stress_process( pid, paramp );
				error( pid, "fork_one_proc", NULL,
					"returned from stress_process" );
				if ( Verbose > 1 ) {
					printf( "%s[%d] fork_one_proc: exiting with status %d\n",
						Progname, pid, ERROREXIT );
				}
				exit( ERROREXIT );
			}
			break;
		case -1:	/* fork error */
			status = errno;
			error( pid, "fork_one_proc", "fork", NULL );
			fprintf(stderr, "%s: %d forks, %d active processes\n", Progname,
				ForkCount, ActiveProcs);
			break;
		default:	/* parent process */
			ForkCount++;
			ActiveProcs++;
			if ( pidlist ) {
				pidlist[index] = pid;
			}
			if ( Verbose ) {
				printf( "process %d: %d\n", index, pid );
			}
	}
	return( status );
}

/*
 * create the stress processes
 * return 0 on success, otherwise, return the errno if there is one
 * or -1 if there is no errno to return
 */
int
fork_procs( struct parameters *paramp, int *pidlist )
{
	int i;
	int status = 0;

	assert( valid_parameters( paramp ) );
	for ( i = 0; (i < paramp->p_processes) && !status; i++ ) {
		status = fork_one_proc( paramp, pidlist, i );
	}
	return( status );
}

char *
signum_to_str( int sig )
{
	char *str = "UNKNOWN";

	if ( (sig >= 0) && (sig < KNOWNSIGS) ) {
		str = SigNames[sig];
	}
	return( str );
}

int
kill_one_proc( struct parameters *paramp, int *pidlist, int index )
{
	pid_t pid;
	int waitstat;
	int status = 0;
	int sig;

	assert( valid_parameters( paramp ) );
	assert( valid_addresses( (caddr_t)pidlist,
		paramp->p_processes * sizeof(int) ) );
	sig = ((int)(drand48() * 2)) ? paramp->p_signum : SIGINT;
	if ( Verbose ) {
		printf( "%s: sending signal %s to process %d\n", Progname,
			signum_to_str( sig ), pidlist[index] );
	}
	if ( kill( pidlist[index], sig ) == -1 ) {
		error( -1, "kill_one_proc", "kill", NULL );
		status = errno;
	} else {
		switch (pid = waitpid(pidlist[index], &waitstat, 0)) {
			case 0:
				ActiveProcs--;
				break;
			case -1:
				if (errno != EINTR) {
					error(-1, "kill_one_proc", "waitpid", NULL);
				}
				break;
			default:
				if (WIFEXITED(waitstat) || WIFSIGNALED(waitstat)) {
					ActiveProcs--;
				}
				if (pid != pidlist[index]) {
					fprintf(stderr, "%s: waitpid returned %d, expected %d\n",
						Progname, (int)pid, (int)pidlist[index]);
				}
		}
	}
	return( status );
}

int
kill_procs( struct parameters *paramp, int *pidlist )
{
	int i;
	int status = 0;

	srand48( time( NULL ) );
	while ( !status && !Sigint && !Sigchld ) {
		sleep( (unsigned)paramp->p_killtime );
		i = (int)(drand48() * paramp->p_processes);
		if ( status = kill_one_proc( paramp, pidlist, i ) ) {
			break;
		} else {
			status = fork_one_proc( paramp, pidlist, i );
		}
	}
	return( status );
}

char *
access_to_str( enum access_method access )
{
	static char *strp;

	switch ( access ) {
		case RANDOM:
			strp = "RANDOM";
			break;
		case SEQUENTIAL:
			strp = "SEQUENTIAL";
			break;
		case BADMETHOD:
			strp = "BADMETHOD";
			break;
		default:
			strp = "UNKNOWN";
	}
	return( strp );
}

build_file( struct parameters *paramp )
{
	int i;
	int status = 0;
	int len;
	int fd = -1;
	char *mbp;
	int n;
	struct stat statbuf;
	char filebuf[BUFLEN];
	int writes;
	int size = paramp->p_filesize;

	assert( size > 0 );
	assert( valid_parameters( paramp ) );
	if ( (status = stat( paramp->p_file, &statbuf )) ||
		(statbuf.st_size < size) ) {
			if ( Verbose > 1 ) {
				(void)printf( "%s: build_file: file %s, size %d\n",
					Progname, paramp->p_file, size );
				(void)fflush( stdout );
			}
			/*
			 * if the file exists, we need to change its mode
			 */
			if ( status == 0 ) {
				if ( chmod( paramp->p_file, CREATE_MODE ) == -1 ) {
					error( -1, "build_file", "chmod", paramp->p_file );
					status = errno;
					return( status );
				}
			}
			status = 0;
			if ( (fd = open( paramp->p_file, CREATE_FLAGS, CREATE_MODE )) ==
				-1 ) {
					error( -1, "build_file", "open", paramp->p_file );
					status = errno;
			} else {
				/*
				 * fill in the file buffer
				 */
				for ( i = 0; i < BUFLEN; i++ ) {
					filebuf[i] = (char)(i % 128);
				}
				/*
				 * calculate the number of writes to be done
				 */
				writes = (size + BUFLEN - 1) / BUFLEN;
				/*
				 * do all the writes
				 */
				while ( writes-- && !status ) {
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
							error( -1, "build_file", "write", NULL );
							status = errno;
							break;
						} else if ( n == 0 ) {
							error( -1, "build_file", "write",
								"zero-length write" );
							status = -1;
							break;
						} else {
							len -= n;
							mbp += n;
						}
					} while ( len );
				}
				if ( close( fd ) == -1 ) {
					error( -1, "build_file", "close", NULL );
					status = errno;
				}
			}
	} else if ( ((statbuf.st_mode & CREATE_MODE) != CREATE_MODE) &&
		(chmod( paramp->p_file, CREATE_MODE ) == -1) ) {
			error( -1, "build_file", "chmod", paramp->p_file );
			status = errno;
	}
	return( status );
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

main( int argc, char **argv )
{
	char *optval;
	char *paramstr;
	struct parameters params;
	int status = 0;
	int pid;
	int child_status;
	char *token;
	int opt;
	int killopt = 0;
	int *pidlist = NULL;
	extern int optind;
	extern char *optarg;

	Progname = *argv;
	SET_DEFAULTS( &params );
	/*
	 * process any options
	 */
	while ( (opt = getopt( argc, argv, OPTSTR )) != EOF ) {
		switch ( opt ) {
			case 'v':
				Verbose++;
				break;
			case 't':
				TraceFile = optarg;
				break;
			case 's':
				Statistics = 1;
				break;
			case 'k':
				killopt = 1;
				break;
			default:
				usage();
				exit( ERROREXIT );
		}
	}
	/*
	 * process the testing parameters
	 */
	if ( (optind < argc) &&
		(status = get_parameters( argv[optind], &params )) ) {
			exit( status );
	}
	if ( Verbose ) {
		printf( "Parameters:\n" );
		printf( "\tverbose level: %d\n", Verbose );
		printf( "\t         file: %s\n", params.p_file );
		printf( "\t          dir: %s\n", params.p_dir );
		printf( "\t    file size: %d\n", params.p_filesize );
		printf( "\t    processes: %d\n", params.p_processes );
		printf( "\taccess method: %s\n", access_to_str( params.p_access ) );
		printf( "\t   pause time: %d microsec\n", params.p_pausetime );
		printf( "\t  record size: [%d,%d]\n", params.p_recsize.r_low,
			params.p_recsize.r_high );
		printf( "\t       repeat: %d\n", params.p_repeat );
	}
	if ( killopt &&
		!(pidlist = (int *)malloc( params.p_processes * sizeof(int) )) ) {
			if ( errno !=- EINTR ) {
				status = errno;
				error( -1, "main", "malloc", NULL );
			}
	/*
	 * chdir to the test directory and construct the test file
	 * then, create the stress processes and wait for them to exit
	 */
	} else if ( (strcmp( params.p_dir, DOT ) != 0) &&
		(chdir( params.p_dir ) == -1) ) {
			if ( errno != EINTR ) {
				status = errno;
				error( -1, "main", "chdir", params.p_file );
			}
	} else if ( status = build_file( &params ) ) {
		fprintf( stderr, "%s: unable to build the test file\n", Progname );
		fflush( stderr );
	} else if ( status = fork_procs( &params, pidlist ) ) {
		fprintf( stderr, "%s: unable to create processes\n", Progname );
		fflush( stderr );
	} else if ( signal( SIGINT, sig_handler ) == SIG_ERR ) {
		error( -1, "main", "signal(SIGINT)", NULL );
		status = errno;
	/*
	} else if ( signal( SIGCHLD, sig_handler ) == SIG_ERR ) {
		error( -1, "main", "signal(SIGCHLD)", NULL );
		status = errno;
	*/
	} else if ( killopt ) {
		status = kill_procs( &params, pidlist );
	} else if ( (pid = wait( &child_status )) == -1 ) {
		if ( errno != EINTR ) {
			error( -1, "main", "wait", NULL );
			status = errno;
		}
	} else if ( WIFSTOPPED( child_status ) ) {
		fprintf( stderr, "%s: child stopped: pid %d, signal %d\n", Progname,
			pid, WSTOPSIG( child_status ) );
		fflush( stderr );
		status = -1;
	} else if ( WIFEXITED( child_status ) ) {
		if ( Verbose > 1 ) {
			printf( "child exited: 0x%x, pid %d\n", child_status, pid );
		}
		status = WEXITSTATUS( child_status );
	} else if ( WIFSIGNALED( child_status ) ) {
		fprintf( stderr, "%s: child terminated on signal %d\n", Progname,
			WTERMSIG( child_status ) );
		fflush( stderr );
		status = -1;
	} else {
		fprintf( stderr, "%s: wait returned unknown status: 0x%x\n",
			Progname, child_status );
		fflush( stderr );
		status = -1;
	}
	if ( status ) {
		printf( "Lock stress failure: status = %d\n", status );
	} else {
		printf( "No lock stress failures\n" );
	}
	fflush( stdout );
	if ( sigignore( SIGINT ) == -1 ) {
		error( -1, "main", "sigignore(SIGINT)", NULL );
	}
	if (kill( 0, SIGINT ) == -1) {
		error( -1, "main", "kill(0, SIGINT)", NULL );
	}
	return( status );
}
