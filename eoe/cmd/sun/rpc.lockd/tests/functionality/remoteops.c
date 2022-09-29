#define _RPCGEN_CLNT
#include <sys/param.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/errno.h>
#include <string.h>
#if !SVR4
#include <strings.h>
#endif /* !SVR4 */
#include <stdlib.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <rpc/rpc.h>
#include <assert.h>
#include <sys/stat.h>
#include "lk_test.h"
#include "print.h"
#include "util.h"
#include "fileops.h"

extern char *Progname;
extern int errno;

int
server_fcntl( fcntlargs *fap, CLIENT *clnt )
{
	struct fcntlreply *replyp;
	int status = 0;

	assert( valid_addresses( (caddr_t)fap, sizeof(struct fcntlargs) ) &&
		valid_addresses( (caddr_t)clnt, sizeof(CLIENT) ) );
	if ( replyp = lktestproc_fcntl_1( fap, clnt ) ) {
		switch (replyp->stat) {
			case FCNTL_SYSERROR:
				status = replyp->fcntlreply_u.errno;
				break;
			case FCNTL_SUCCESS:
				if (fap->fa_cmd == F_GETLK) {
					fap->fa_lock = replyp->fcntlreply_u.lock;
				}
				break;
			default:
				fprintf(stderr, "%s: server_fcntl: unknown reply value %d\n",
					Progname, replyp->stat);
				status = -1;
		}
	} else {
		fprintf( stderr, "%s: ", Progname );
		clnt_perror( clnt, "server_fcntl: lktestproc_fcntl_1 returned NULL" );
		status = -1;
	}
	return( status );
}

int
server_flock( flockargs *fap, CLIENT *clnt )
{
	int *replyp;
	int status;

	assert( valid_addresses( (caddr_t)fap, sizeof(struct flockargs) ) &&
		valid_addresses( (caddr_t)clnt, sizeof(CLIENT) ) );
	if ( replyp = lktestproc_flock_1( fap, clnt ) ) {
		status = *replyp;
	} else {
		fprintf( stderr, "%s: ", Progname );
		clnt_perror( clnt, "server_flock: lktestproc_flock_1 returned NULL" );
		status = -1;
	}
	return( status );
}

int
server_lockf( lockfargs *lap, CLIENT *clnt )
{
	int *replyp;
	int status;

	assert( valid_addresses( (caddr_t)lap, sizeof(struct lockfargs) ) &&
		valid_addresses( (caddr_t)clnt, sizeof(CLIENT) ) );
	if ( replyp = lktestproc_lockf_1( lap, clnt ) ) {
		status = *replyp;
	} else {
		fprintf( stderr, "%s: ", Progname );
		clnt_perror( clnt, "server_lockf: lktestproc_lockf_1 returned NULL" );
		status = -1;
	}
	return( status );
}

int
set_server_opts( servopts *sop, CLIENT *clnt )
{
	int *statusp;
	int status;

	assert( valid_addresses( (caddr_t)sop, sizeof(struct servopts) ) &&
		valid_addresses( (caddr_t)clnt, sizeof(CLIENT) ) );
	if ( statusp = lktestproc_servopts_1( sop, clnt ) ) {
		status = *statusp;
	} else {
		fprintf( stderr, "%s: ", Progname );
		clnt_perror( clnt, "server_lock: lktestproc_servopts_1 returned NULL" );
		status = -1;
	}
	return( status );
}

bool_t
server_verify( verifyargs *vap, CLIENT *clnt )
{
	bool_t *statusp;
	int status;

	assert( valid_addresses( (caddr_t)vap, sizeof(struct verifyargs) ) &&
		valid_addresses( (caddr_t)clnt, sizeof(CLIENT) ) );
	if ( statusp = lktestproc_verify_1( vap, clnt ) ) {
		status = *statusp;
	} else {
		fprintf( stderr, "%s: ", Progname );
		clnt_perror( clnt,
			"server_verify_lock: lktestproc_verify_1 returned NULL" );
		status = 0;
	}
	return( status );
}

bool_t
server_held( pathstr *file_name, CLIENT *clnt )
{
	bool_t *statusp;
	int status;

	assert( valid_addresses( (caddr_t)file_name, 1 ) &&
		valid_addresses( (caddr_t)clnt, sizeof(CLIENT) ) );
	if ( statusp = lktestproc_held_1( file_name, clnt ) ) {
		status = *statusp;
	} else {
		fprintf( stderr, "%s: ", Progname );
		clnt_perror( clnt,
			"server_locks_held: lktestproc_held_1 returned NULL" );
		status = -1;
	}
	return( status );
}

int
reset_server( CLIENT *clnt )
{
	int *statusp;
	int status;

	if ( statusp = lktestproc_reset_1( NULL, clnt ) ) {
		status = *statusp;
	} else {
		fprintf( stderr, "%s: ", Progname );
		clnt_perror( clnt, "reset_server: lktestproc_reset_1 returned NULL" );
		status = -1;
	}
	return( status );
}

