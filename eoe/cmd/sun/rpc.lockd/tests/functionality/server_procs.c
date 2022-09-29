#define _RPCGEN_SVC
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <rpc/rpc.h>
#include <sys/fcntl.h>
#include "lk_test.h"
#include "print.h"
#include "util.h"
#include "fileops.h"
#include "localops.h"

char *Progname = "lk_test_svc";
int Verbose = 0;
int AlternateVerify = 0;		/* user alternate verification method */

static int Fileinit_done = 0;
static char *Starting_directory = NULL;

extern int errno;

/*
 * set the server options: verbose mode value and working directory
 * return 0 on success, -1 on failure
 */
int *
lktestproc_servopts_1( struct servopts *sop, struct svc_req  *rqstp )
{
	static int status = 0;
	char *curdir = NULL;

	if ( Verbose > 1 ) {
		printf( "%s: lktestproc_servopts_1\n", Progname );
	}
	if ( !Fileinit_done ) {
		open_file_init();
		Fileinit_done = 1;
	}
	assert( valid_addresses( (caddr_t)sop, sizeof(struct servopts) ) );
	Verbose = sop->so_verbose;
	AlternateVerify = sop->so_altverify;
	if ( !Starting_directory &&
		!(Starting_directory = getcwd( NULL, MAXPATHLEN )) ) {
			status = -1;
			if ( Verbose ) {
				(void)fprintf( stderr, "%s: unable to get current directory: ",
					Progname );
				perror( "getcwd" );
			}
	} else if ( sop->so_directory && (strcmp( sop->so_directory, "." ) != 0) ) {
		if ( (status = chdir( sop->so_directory )) && Verbose ) {
			(void)fprintf( stderr, "%s: unable to change directory: ",
				Progname );
			perror( sop->so_directory );
		} else if ( Verbose ) {
			printf( "%s: lktestproc_servopts_1:\n", Progname );
			printf( "\t          Verbose = %d\n", Verbose );
			printf( "\t              dir = %s\n", sop->so_directory );
		}
	} else if ( Verbose ) {
		curdir = getcwd( NULL, MAXPATHLEN );
		printf( "%s: lktestproc_servopts_1:\n", Progname );
		printf( "\t          Verbose = %d\n", Verbose );
		printf( "\t              dir = %s\n", curdir );
		free( curdir );
	}
	if ( Verbose > 1 ) {
		printf( "%s: lktestproc_servopts_1: return %d\n", Progname, status );
	}
	return( &status );
}

fcntlreply *
lktestproc_fcntl_1( struct fcntlargs *fap, struct svc_req *rqstp )
{
	int error = 0;
	static fcntlreply reply;

	assert( valid_addresses( (caddr_t)fap, sizeof(struct fcntlargs) ) );
	if ( Verbose ) {
		printf( "%s: lktestproc_fcntl_1: \n", Progname );
		print_fcntlargs( fap, "\t" );
		if ( Verbose > 1 ) {
			print_svcreq( rqstp, "\t" );
		}
	}
	if ( !Fileinit_done ) {
		open_file_init();
		Fileinit_done = 1;
	}
	bzero(&reply, sizeof(reply));
	error = local_fcntl( fap );
	if ( Verbose > 1 ) {
		printf( "%s: lktestproc_fcntl_1: return %d\n", Progname, error );
	}
	if (error) {
		reply.stat = FCNTL_SYSERROR;
		reply.fcntlreply_u.errno = error;
	} else if (fap->fa_cmd == F_GETLK) {
		reply.stat = FCNTL_SUCCESS;
		reply.fcntlreply_u.lock = fap->fa_lock;
	} else {
		reply.stat = FCNTL_SUCCESS;
	}
	return( &reply );
}

int *
lktestproc_flock_1( struct flockargs *flap, struct svc_req *rqstp )
{
	static int error;

	assert( valid_addresses( (caddr_t)flap, sizeof(struct flockargs) ) );
	if ( Verbose ) {
		printf( "%s: lktestproc_flock_1: \n", Progname );
		print_flockargs( flap, "\t" );
		if ( Verbose > 1 ) {
			print_svcreq( rqstp, "\t" );
		}
	}
	if ( !Fileinit_done ) {
		open_file_init();
		Fileinit_done = 1;
	}
	error = local_flock( flap );
	if ( Verbose > 1 ) {
		printf( "%s: lktestproc_flock_1: return %d\n", Progname, error );
	}
	return( &error );
}

int *
lktestproc_lockf_1( struct lockfargs *lfap, struct svc_req *rqstp )
{
	static int error;

	assert( valid_addresses( (caddr_t)lfap, sizeof(struct lockfargs) ) );
	if ( Verbose ) {
		printf( "%s: lktestproc_lockf_1: \n", Progname );
		print_lockfargs( lfap, "\t" );
		if ( Verbose > 1 ) {
			print_svcreq( rqstp, "\t" );
		}
	}
	if ( !Fileinit_done ) {
		open_file_init();
		Fileinit_done = 1;
	}
	error = local_lockf( lfap );
	if ( Verbose > 1 ) {
		printf( "%s: lktestproc_lockf_1: return %d\n", Progname, error );
	}
	return( &error );
}


bool_t *
lktestproc_held_1(pathstr *name, struct svc_req *rqstp)
{
	static bool_t status;

	assert( valid_addresses( (caddr_t)name, sizeof(*name) ) &&
		valid_addresses( (caddr_t)*name, 1 ) );
	if ( Verbose ) {
		printf( "%s: lktestproc_held_1: \n", Progname );
		printf( "\tfilename = %s\n", *name );
		if ( Verbose > 1 ) {
			print_svcreq( rqstp, "\t" );
		}
	}
	if ( !Fileinit_done ) {
		open_file_init();
		Fileinit_done = 1;
	}
	status = locks_held( *name );
	if ( Verbose > 1 ) {
		printf( "%s: lktestproc_held_1: return %d\n", Progname, status );
	}
	return( &status );
}

bool_t *
lktestproc_verify_1(verifyargs *vap, struct svc_req *rqstp)
{
	static bool_t status;

	assert( valid_addresses((caddr_t)vap, sizeof(verifyargs)));
	if (Verbose) {
		printf( "%s: lktestproc_verify_1 \n", Progname );
		print_verifyargs(vap, "\t");
		if ( Verbose > 1 ) {
			print_svcreq( rqstp, "\t" );
		}
	}
	if ( !Fileinit_done ) {
		open_file_init();
		Fileinit_done = 1;
	}
	status = verify_lock( vap );
	if ( Verbose > 1 ) {
		printf( "%s: lktestproc_verify_1: return %d\n", Progname, status );
	}
	return(&status);
}

/* ARGSUSED */
int *
lktestproc_reset_1( void *argp, struct svc_req *rqstp )
{
	static int status = 0;

	closeall();
	if ( !Starting_directory ) {
		if ( !(Starting_directory = getcwd( NULL, MAXPATHLEN )) ) {
			status = -1;
			if ( Verbose ) {
				(void)fprintf( stderr, "%s: unable to get current directory: ",
					Progname );
				perror( "getcwd" );
			}
		} else if ( Verbose ) {
			printf( "%s: lktestproc_reset_1:\n", Progname );
			printf( "\tdir = %s\n", Starting_directory );
		}
	} else if ( status = chdir( Starting_directory ) ) {
		if ( Verbose ) {
			(void)fprintf( stderr, "%s: unable to chdir back to %s: ",
				Progname, Starting_directory );
			perror( "chdir"  );
		}
		if ( errno ) {
			status = errno;
		} else {
			status = -1;
		}
	} else if ( Verbose ) {
		printf( "%s: lktestproc_reset_1:\n", Progname );
		printf( "\tdir = %s\n", Starting_directory );
	}
	Verbose = 0;
	return( &status );
}
