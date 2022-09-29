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
#include "defaults.h"
#include "parameters.h"
#include "print.h"
#if _SUNOS
#include <rpc/rpc.h>
#endif /* _SUNOS */
#include "util.h"

#define OPTSTR				"v"

char *Progname = "lock_monitor";
int Verbose = DEFAULT_VERBOSE;

extern int errno;

int
find_locks( int fd, off_t filesize )
{
	int status = 0;
	struct flock fl;
	off_t startoff = (off_t)0;
	int lock_count = 0;

	printf( "%s: lock report\n", Progname );
	while ( startoff < filesize ) {
		fl.l_start = startoff;
		fl.l_type = F_WRLCK;
		fl.l_len = 0;
		fl.l_whence = SEEK_SET;
		if ( fcntl( fd, F_GETLK, &fl ) == -1 ) {
			if ( errno != EINTR ) {
				status = errno;
				error( -1, "find_locks", "fcntl", NULL );
			}
			break;
		} else if ( fl.l_type == F_UNLCK ) {
			if ( lock_count == 0 ) {
				printf( "\tno locks held\n", Progname );
			}
			break;
		} else {
			lock_count++;
			printf( "\t%s: pid %d, sysid 0x%x, offset 0x%x, len %d\n",
				locktype_to_str( fl.l_type ), fl.l_pid, fl.l_sysid, fl.l_start,
				fl.l_len );
			if ( fl.l_len ) {
				startoff = fl.l_start + fl.l_len;
			} else {
				break;
			}
		}
	}
	return( status );
}

int
monitor_locks( struct parameters *paramp )
{
	int status = 0;
	int fd;

	/*
	 * chdir to the test directory and construct the test file
	 * then, create the stress processes and wait for them to exit
	 */
	if ( (strcmp( paramp->p_dir, DOT ) != 0) &&
		(chdir( paramp->p_dir ) == -1) ) {
			if ( errno != EINTR ) {
				status = errno;
				error( -1, "monitor_locks", "chdir", paramp->p_dir );
			}
	} else if ( (fd = open( paramp->p_file, OPEN_FLAGS, OPEN_MODE )) == -1 ) {
		if ( errno != EINTR ) {
			status = errno;
			error( -1, "monitor_locks", "open", paramp->p_file );
		}
	} else {
		while ( status == 0 ) {
			sleep( paramp->p_montime );
			status = find_locks( fd, paramp->p_filesize );
		}
	}
	return( status );
}

main( int argc, char **argv )
{
	struct parameters params;
	int status = 0;
	int opt;
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
			default:
				fprintf( stderr, "%s: illegal option\n", Progname );
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
		printf( "\t    file size: %d\n", (int)params.p_filesize );
		printf( "\tmonitor delay: %d\n", params.p_montime );
	}
	status = monitor_locks( &params );
	return( status );
}
