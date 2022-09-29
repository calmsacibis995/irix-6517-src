/*
 *	NAME
 *
 *		lk_test
 *
 *	SYNOPSIS
 *
 *		lk_test [-v] [-h hostname[,directory]] [-f file[,size]] [directory]
 *		lk_test_svc
 *
 *	DESCRIPTION
 *
 *		lk_test is an RPC client/server application designed to test the file
 *		and record locking functionality of a UNIX file system.  The client and
 *		the server form a pair of processes cooperating in the testing.  The
 *		client is the active process in that the test phases are initiated by
 *		the client.  The server is passive, waiting for the client to make
 *		requests of it.
 *
 *		All testing is performed in the current working directory for each
 *		process unless a directory has been specified by the argument
 *		"directory".
 *
 *		Testing is divided into two phases.  In the first phase, exclusive
 *		locking is tested.  In the second phase, shared locking is tested.
 *		The first phase tests the locking and unlocking of the entire file
 *		and of a portion of the file followed by tests of record locking
 *		(locking a range of bytes within the file), overlapping lock requests,
 *		and lock boundaries.  The second phase is similar to phase one with
 *		variations for testing shared locking.
 *
 *		In all phases, verification of locking and unlocking is done by
 *		requests to the process acting as the server.
 *
 *		When testing a distributed file system such as NFS, the following
 *		test scenarios must be executed (with three different systems):
 *
 *			1)	run lk_test_svc and lk_test on the file system server for a
 *				loopback mount;
 *			2)	run lk_test_svc on the file system server and lk_test on a
 *				single client;
 *			3)	run lk_test_svc on one client and lk_test on a second client,
 *				with neither client being the file system server;
 *
 *	OPTIONS
 *
 *		lk_test accepts the following options.
 *
 *			-v							turn on verbose mode
 *
 *		lk_test_svc accepts no options from the command line.  The -v option
 *		and the working directory name are passed to the server by the client
 *		via an RPC call.
 */
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
#include <sys/file.h>
#if _IRIX
#include <mntent.h>
#endif /* _IRIX */
#if _SUNOS
#include <sys/mnttab.h>
#endif /* _SUNOS */
#include "lk_test.h"
#include "print.h"
#include "util.h"
#include "fileops.h"
#include "localops.h"
#include "remoteops.h"

#define OPTSTR			"av"
#define FILE_SIZE		1024
#define MIN_FILE_SIZE	1024
#define FILEBUFLEN		1024

char *Progname = NULL;
int Verbose = 0;
int AlternateVerify = 0;		/* user alternate verification method */

extern int errno;
extern char *optarg;
extern int optind;

/*
 * Basic locking tests
 */

int
basic_fcntl_test( CLIENT *clnt, pathstr name, lockdesc *ldp )
{
	int status = 0;
	struct fcntlargs fargs;
	verifyargs verfarg;

	assert( valid_addresses( (caddr_t)ldp, sizeof(struct lockdesc) ) &&
		valid_addresses( (caddr_t)clnt, sizeof(CLIENT) ) );
	/*
	 * lock on the server first
	 */
	fargs.fa_cmd = F_SETLK;
	verfarg.va_name = fargs.fa_name = name;
	verfarg.va_lock = fargs.fa_lock = *ldp;
	if ( status = server_fcntl( &fargs, clnt ) ) {
		report_lock_error( "server_fcntl", "basic_fcntl_test",
			"unable to acquire lock on the server", status );
	/*
	 * verify the lock on the client
	 */
	} else if ( !verify_lock( &verfarg ) ) {
		status = -1;
		report_lock_error( "verify_lock", "basic_fcntl_test",
			"lock verification failure", status );
		fargs.fa_lock.ld_type = F_UNLCK;
		(void)server_fcntl( &fargs, clnt );
	} else {
		/*
		 * unlock on the server
		 */
		fargs.fa_lock.ld_type = F_UNLCK;
		if ( status = server_fcntl( &fargs, clnt ) ) {
			report_lock_error( "server_fcntl", "basic_fcntl_test",
				"unable to release shared lock on the server", status );
		/*
		 * verify on the client that no locks are held
		 */
		} else if ( locks_held( name ) ) {
			status = -1;
			report_lock_error( "verify_lock", "basic_fcntl_test",
				"locks held on client after server unlock", status );
		/*
		 * verify on the server that no locks are held
		 */
		} else if ( server_held( &name, clnt ) ) {
			status = -1;
			report_lock_error( "verify_lock", "basic_fcntl_test",
				"locks held on server after server unlock", status );
		} else {
			/*
			 * make the testing symmetrical by reversing the locking:
			 * lock on the client and verify on the server
			 *
			 * first, lock on the client
			 */
			fargs.fa_lock = *ldp;
			if ( status = local_fcntl( &fargs ) ) {
				report_lock_error( "local_fcntl", "basic_fcntl_test",
					"unable to acquire shared lock on the client", status );
			/*
			 * verify the locking on the server
			 */
			} else if ( !server_verify( &verfarg, clnt ) ) {
				status = -1;
				report_lock_error( "server_verify", "basic_fcntl_test",
					"server lock verification failure", status );
				fargs.fa_lock.ld_type = F_UNLCK;
				(void)local_fcntl( &fargs );
			} else {
				/*
				 * unlock on the client
				 */
				fargs.fa_lock.ld_type = F_UNLCK;
				if ( status = local_fcntl( &fargs ) ) {
					report_lock_error( "local_fcntl", "basic_fcntl_test",
						"unable to release shared lock on the client", status );
				/*
				 * verify that no locks are held from the points of view of the
				 * client and the server
				 */
				} else if ( locks_held( name ) ) {
					status = -1;
					report_lock_error( "locks_held", "basic_fcntl_test",
						"locks held on client after server unlock", status );
				} else if ( server_held( &name, clnt ) ) {
					status = -1;
					report_lock_error( "server_held", "basic_fcntl_test",
						"locks held on server after server unlock", status );
				}
			}
		}
	}
	return( status );
}

int
basic_flock_test( CLIENT *clnt, pathstr name, lockdesc *ldp )
{
	int status = 0;
	struct flockargs lkargs;
	struct flockargs unlkargs;
	verifyargs verfarg;

	assert( valid_addresses( (caddr_t)ldp, sizeof(struct lockdesc) ) &&
		valid_addresses( (caddr_t)clnt, sizeof(CLIENT) ) );
	lkargs.fa_op = LOCK_NB;
	switch (ldp->ld_type) {
		case F_RDLCK:
			lkargs.fa_op |= LOCK_SH;
			break;
		case F_WRLCK:
			lkargs.fa_op |= LOCK_EX;
			break;
		default:
			report_lock_error( locktype_to_str(ldp->ld_type),
				"basic_flock_test", "invalid lock type", status );
			return(-1);
	}
	unlkargs.fa_op = LOCK_UN;
	verfarg.va_name = lkargs.fa_name = unlkargs.fa_name = name;
	verfarg.va_lock.ld_offset = 0;
	verfarg.va_lock.ld_len = 0;
	verfarg.va_lock.ld_type = ldp->ld_type;
	/*
	 * lock on the server first
	 */
	if ( status = server_flock( &lkargs, clnt ) ) {
		report_lock_error( "server_flock", "basic_flock_test",
			"unable to acquire lock on the server", status );
	/*
	 * verify the lock on the client
	 */
	} else if ( !verify_lock( &verfarg ) ) {
		status = -1;
		report_lock_error( "verify_lock", "basic_flock_test",
			"lock verification failure", status );
		(void)server_flock( &unlkargs, clnt );
	/*
	 * unlock on the server
	 */
	} else if ( status = server_flock( &unlkargs, clnt ) ) {
		report_lock_error( "server_flock", "basic_flock_test",
			"unable to release shared lock on the server", status );
	/*
	 * verify on the client that no locks are held
	 */
	} else if ( locks_held( name ) ) {
		status = -1;
		report_lock_error( "verify_lock", "basic_flock_test",
			"locks held on client after server unlock", status );
	/*
	 * verify on the server that no locks are held
	 */
	} else if ( server_held( &name, clnt ) ) {
		status = -1;
		report_lock_error( "verify_lock", "basic_flock_test",
			"locks held on server after server unlock", status );
	/*
	 * make the testing symmetrical by reversing the locking:
	 * lock on the client and verify on the server
	 *
	 * first, lock on the client
	 */
	} else if ( status = local_flock( &lkargs ) ) {
		report_lock_error( "local_flock", "basic_flock_test",
			"unable to acquire shared lock on the client", status );
	/*
	 * verify the locking on the server
	 */
	} else if ( !server_verify( &verfarg, clnt ) ) {
		status = -1;
		report_lock_error( "server_verify", "basic_flock_test",
			"server lock verification failure", status );
		(void)local_flock( &unlkargs );
	/*
	 * unlock on the client
	 */
	} else if ( status = local_flock( &unlkargs ) ) {
		report_lock_error( "local_flock", "basic_flock_test",
			"unable to release shared lock on the client", status );
	/*
	 * verify that no locks are held from the points of view of the
	 * client and the server
	 */
	} else if ( locks_held( name ) ) {
		status = -1;
		report_lock_error( "locks_held", "basic_flock_test",
			"locks held on client after server unlock", status );
	} else if ( server_held( &name, clnt ) ) {
		status = -1;
		report_lock_error( "server_held", "basic_flock_test",
			"locks held on server after server unlock", status );
	}
	return( status );
}

int
basic_lockf_test( CLIENT *clnt, pathstr name, lockdesc *ldp )
{
	int status = 0;
	struct lockfargs lkargs;
	struct lockfargs unlkargs;
	verifyargs verfarg;

	assert( valid_addresses( (caddr_t)ldp, sizeof(struct lockdesc) ) &&
		valid_addresses( (caddr_t)clnt, sizeof(CLIENT) ) );
	lkargs.la_func = F_TLOCK;
	lkargs.la_offset = ldp->ld_offset;
	lkargs.la_size = ldp->ld_len;
	unlkargs = lkargs;
	unlkargs.la_func = F_ULOCK;
	verfarg.va_name = lkargs.la_name = unlkargs.la_name = name;
	verfarg.va_lock = *ldp;
	/*
	 * lock on the server first
	 */
	if ( status = server_lockf( &lkargs, clnt ) ) {
		report_lock_error( "server_lockf", "basic_lockf_test",
			"unable to acquire lock on the server", status );
	/*
	 * verify the lock on the client
	 */
	} else if ( !verify_lock( &verfarg ) ) {
		status = -1;
		report_lock_error( "verify_lock", "basic_lockf_test",
			"lock verification failure", status );
		(void)server_lockf( &unlkargs, clnt );
	/*
	 * unlock on the server
	 */
	} else if ( status = server_lockf( &unlkargs, clnt ) ) {
		report_lock_error( "server_lockf", "basic_lockf_test",
			"unable to release shared lock on the server", status );
	/*
	 * verify on the client that no locks are held
	 */
	} else if ( locks_held( name ) ) {
		status = -1;
		report_lock_error( "verify_lock", "basic_lockf_test",
			"locks held on client after server unlock", status );
	/*
	 * verify on the server that no locks are held
	 */
	} else if ( server_held( &name, clnt ) ) {
		status = -1;
		report_lock_error( "verify_lock", "basic_lockf_test",
			"locks held on server after server unlock", status );
	/*
	 * make the testing symmetrical by reversing the locking:
	 * lock on the client and verify on the server
	 *
	 * first, lock on the client
	 */
	} else if ( status = local_lockf( &lkargs ) ) {
		report_lock_error( "local_lockf", "basic_lockf_test",
			"unable to acquire shared lock on the client", status );
	/*
	 * verify the locking on the server
	 */
	} else if ( !server_verify( &verfarg, clnt ) ) {
		status = -1;
		report_lock_error( "server_verify", "basic_lockf_test",
			"server lock verification failure", status );
		(void)local_lockf( &unlkargs );
	/*
	 * unlock on the client
	 */
	} else if ( status = local_lockf( &unlkargs ) ) {
		report_lock_error( "local_lockf", "basic_lockf_test",
			"unable to release shared lock on the client", status );
	/*
	 * verify that no locks are held from the points of view of the
	 * client and the server
	 */
	} else if ( locks_held( name ) ) {
		status = -1;
		report_lock_error( "locks_held", "basic_lockf_test",
			"locks held on client after server unlock", status );
	} else if ( server_held( &name, clnt ) ) {
		status = -1;
		report_lock_error( "server_held", "basic_lockf_test",
			"locks held on server after server unlock", status );
	}
	return( status );
}

/*
 * Shared locking tests
 */

/*
 * overlapping shared lock testing:
 *
 *		a)	lock region-A on the server
 *			lock region-B on the client
 *			unlock region-A on the server
 *			unlock region-B on the client
 *
 *		b)	lock region-A on the client
 *			lock region-B on the server
 *			unlock region-A on the client
 *			unlock region-B on the server
 *
 *		c)	lock region-B on the server
 *			lock region-A on the client
 *			unlock region-B on the server
 *			unlock region-A on the client
 *
 *		d)	lock region-B on the client
 *			lock region-A on the server
 *			unlock region-B on the client
 *			unlock region-A on the server
 */
int
shared_fcntl_overlap_test( CLIENT *clnt, pathstr name, lockdesc *ldpA,
	lockdesc *ldpB )
{
	int status = 0;
	fcntlargs lkargsA;
	fcntlargs unlkargsA;
	verifyargs verfargA;
	fcntlargs lkargsB;
	fcntlargs unlkargsB;
	verifyargs verfargB;

	assert( valid_addresses( (caddr_t)ldpA, sizeof(struct lockdesc) ) &&
		valid_addresses( (caddr_t)ldpB, sizeof(struct lockdesc) ) &&
		valid_addresses( (caddr_t)clnt, sizeof(CLIENT) ) );
	lkargsA.fa_cmd = lkargsB.fa_cmd = F_SETLK;
	verfargA.va_lock = lkargsA.fa_lock = *ldpA;
	verfargB.va_lock = lkargsB.fa_lock = *ldpB;
	verfargA.va_name = verfargB.va_name = lkargsA.fa_name = lkargsB.fa_name =
		name;
	unlkargsA = lkargsA;
	unlkargsA.fa_lock.ld_type = F_UNLCK;
	unlkargsB = lkargsB;
	unlkargsB.fa_lock.ld_type = F_UNLCK;
	/*
	 * part 2(a) of the testing as described above
	 *
	 * lock region A on the server
	 */
	if ( status = server_fcntl( &lkargsA, clnt ) ) {
		report_lock_error( "server_fcntl", "shared_fcntl_overlap_test",
			"unable to acquire shared lock on the server", status );
	/*
	 * verify on the client
	 */
	} else if ( !verify_lock( &verfargA ) ) {
		status = -1;
		report_lock_error( "verify_lock", "shared_fcntl_overlap_test",
			"lock verification failure", status );
		(void)server_fcntl( &unlkargsA, clnt );
	/*
	 * lock region B on the client
	 */
	} else if ( status = local_fcntl( &lkargsB ) ) {
		report_lock_error( "local_fcntl", "shared_fcntl_overlap_test",
			"shared lock failure for region B on the client", status );
		(void)server_fcntl( &unlkargsA, clnt );
	/*
	 * unlock region A on the server
	 */
	} else if ( status = server_fcntl( &unlkargsA, clnt ) ) {
		report_lock_error( "server_fcntl", "shared_fcntl_overlap_test",
			"unable to release shared lock on the server", status );
		(void)local_fcntl( &unlkargsB );
	/*
	 * check that there is still a lock held from the server's point
	 * of view
	 */
	} else if ( !server_held( &name, clnt ) ) {
		status = -1;
		report_lock_error( "server_held", "shared_fcntl_overlap_test",
			"no locks held on server after client unlock", status );
		(void)local_fcntl( &unlkargsB );
	/*
	 * re-verify the client's lock for region B on the server
	 */
	} else if ( !server_verify( &verfargB, clnt ) ) {
		status = -1;
		report_lock_error( "server_verify", "shared_fcntl_overlap_test",
			"server lock verification failure for region B", status );
		(void)local_fcntl( &unlkargsB );
	/*
	 * unlock on the client (final unlock)
	 */
	} else if ( status = local_fcntl( &unlkargsB ) ) {
		report_lock_error( "local_fcntl", "shared_fcntl_overlap_test",
			"unable to release region B lock on the client", status );
	/*
	 * verify that there are no locks from the points of view of the
	 * client and the server
	 */
	} else if ( locks_held( name ) ) {
		status = -1;
		report_lock_error( "locks_held", "shared_fcntl_overlap_test",
			"locks held on client after final unlock", status );
	} else if ( server_held( &name, clnt ) ) {
		status = -1;
		report_lock_error( "server_held", "shared_fcntl_overlap_test",
			"locks held on server after final unlock", status );
	/*
	 * part 2(b) of the testing as described above
	 *
	 * first, lock region A on the client
	 */
	} else if ( status = local_fcntl( &lkargsA ) ) {
		report_lock_error( "local_fcntl", "shared_fcntl_overlap_test",
			"unable to acquire region A lock on the client", status );
	/*
	 * verify the lock from the point of view of the server
	 */
	} else if ( !server_verify( &verfargA, clnt ) ) {
		status = -1;
		report_lock_error( "server_verify", "shared_fcntl_overlap_test",
			"server lock verification failure for region A", status );
		(void)local_fcntl( &unlkargsA );
	/*
	 * lock region B on the server
	 */
	} else if ( status = server_fcntl( &lkargsB, clnt ) ) {
		report_lock_error( "server_fcntl", "shared_fcntl_overlap_test",
			"unable to acquire region B lock on the server", status );
		(void)local_fcntl( &unlkargsA );
	/*
	 * unlock region A on the client
	 */
	} else if ( status = local_fcntl( &unlkargsA ) ) {
		report_lock_error( "local_fcntl", "shared_fcntl_overlap_test",
			"unable to release region A lock on the client", status );
		(void)server_fcntl( &unlkargsB, clnt );
	/*
	 * verify from the point of view of the client that there is still a
	 * lock held
	 */
	} else if ( !locks_held( name ) ) {
		status = -1;
		report_lock_error( "locks_held", "shared_fcntl_overlap_test",
			"no locks held on server after client unlock", status );
		(void)server_fcntl( &unlkargsB, clnt );
	/*
	 * verify that the server still holds its lock as expected
	 * its lock is for region B
	 */
	} else if ( !verify_lock( &verfargB ) ) {
		status = -1;
		report_lock_error( "verify_lock", "shared_fcntl_overlap_test",
			"lock verification failure", status );
		(void)server_fcntl( &unlkargsB, clnt );
	/*
	 * unlock on the server
	 */
	} else if ( status = server_fcntl( &unlkargsB, clnt ) ) {
		report_lock_error( "server_fcntl", "shared_fcntl_overlap_test",
			"unable to release region B lock on the server", status );
	/*
	 * verify that ther are no locks from both points of view
	 */
	} else if ( locks_held( name ) ) {
		status = -1;
		report_lock_error( "verify_lock", "shared_fcntl_overlap_test",
			"locks held on client after final unlock", status );
	} else if ( server_held( &name, clnt ) ) {
		status = -1;
		report_lock_error( "verify_lock", "shared_fcntl_overlap_test",
			"locks held on server after final unlock", status );
	/*
	 * part 2(c) as described above
	 *
	 * lock region B on the server
	 */
	} else if ( status = server_fcntl( &lkargsB, clnt ) ) {
		report_lock_error( "server_fcntl", "shared_fcntl_overlap_test",
			"unable to acquire region B lock on the server", status );
	/*
	 * verify on the client
	 */
	} else if ( !verify_lock( &verfargB ) ) {
		status = -1;
		report_lock_error( "verify_lock", "shared_fcntl_overlap_test",
			"lock verification failure for region B", status );
		(void)server_fcntl( &unlkargsB, clnt );
	/*
	 * lock region A on the client
	 */
	} else if ( status = local_fcntl( &lkargsA ) ) {
		report_lock_error( "local_fcntl", "shared_fcntl_overlap_test",
			"shared lock failure for region A on the client", status );
		(void)server_fcntl( &unlkargsB, clnt );
	/*
	 * unlock region B on the server
	 */
	} else if ( status = server_fcntl( &unlkargsB, clnt ) ) {
		report_lock_error( "server_fcntl", "shared_fcntl_overlap_test",
			"unable to release region B lock on the server", status );
		(void)local_fcntl( &unlkargsA );
	/*
	 * check that there is still a lock held from the server's point
	 * of view
	 */
	} else if ( !server_held( &name, clnt ) ) {
		status = -1;
		report_lock_error( "server_held", "shared_fcntl_overlap_test",
			"no locks held on server after client unlock", status );
		(void)local_fcntl( &unlkargsA );
	/*
	 * re-verify the client's lock for region A on the server
	 */
	} else if ( !server_verify( &verfargA, clnt ) ) {
		status = -1;
		report_lock_error( "server_verify", "shared_fcntl_overlap_test",
			"server lock verification failure for region A", status );
		(void)local_fcntl( &unlkargsA );
	/*
	 * unlock on the client (final unlock)
	 */
	} else if ( status = local_fcntl( &unlkargsA ) ) {
		report_lock_error( "local_fcntl", "shared_fcntl_overlap_test",
			"unable to release region A lock on the client", status );
	/*
	 * verify that there are no locks from the points of view of the
	 * client and the server
	 */
	} else if ( locks_held( name ) ) {
		status = -1;
		report_lock_error( "locks_held", "shared_fcntl_overlap_test",
			"locks held on client after final unlock", status );
	} else if ( server_held( &name, clnt ) ) {
		status = -1;
		report_lock_error( "server_held", "shared_fcntl_overlap_test",
			"locks held on server after final unlock", status );
	/*
	 * part 2(d) of the testing as described above
	 *
	 * first, lock region B on the client
	 */
	} else if ( status = local_fcntl( &lkargsB ) ) {
		report_lock_error( "local_fcntl", "shared_fcntl_overlap_test",
			"unable to acquire region B lock on the client", status );
	/*
	 * verify the lock from the point of view of the server
	 */
	} else if ( !server_verify( &verfargB, clnt ) ) {
		status = -1;
		report_lock_error( "server_verify", "shared_fcntl_overlap_test",
			"server lock verification failure for region B", status );
		(void)local_fcntl( &unlkargsB );
	/*
	 * lock region A on the server
	 */
	} else if ( status = server_fcntl( &lkargsA, clnt ) ) {
		report_lock_error( "server_fcntl", "shared_fcntl_overlap_test",
			"unable to acquire region A lock on the server", status );
		(void)local_fcntl( &unlkargsB );
	/*
	 * unlock region B on the client
	 */
	} else if ( status = local_fcntl( &unlkargsB ) ) {
		report_lock_error( "local_fcntl", "shared_fcntl_overlap_test",
			"unable to release region B lock on the client", status );
		(void)server_fcntl( &unlkargsA, clnt );
	/*
	 * verify from the point of view of the client that there is still a
	 * lock held
	 */
	} else if ( !locks_held( name ) ) {
		status = -1;
		report_lock_error( "locks_held", "shared_fcntl_overlap_test",
			"no locks held on server after client unlock", status );
		(void)server_fcntl( &unlkargsA, clnt );
	/*
	 * verify that the server still holds its lock as expected
	 * its lock is for region A
	 */
	} else if ( !verify_lock( &verfargA ) ) {
		status = -1;
		report_lock_error( "verify_lock", "shared_fcntl_overlap_test",
			"lock verification failure for region A", status );
		(void)server_fcntl( &unlkargsA, clnt );
	/*
	 * unlock on the server
	 */
	} else if ( status = server_fcntl( &unlkargsA, clnt ) ) {
		report_lock_error( "server_fcntl", "shared_fcntl_overlap_test",
			"unable to release region A lock on the server", status );
	/*
	 * verify that ther are no locks from both points of view
	 */
	} else if ( locks_held( name ) ) {
		status = -1;
		report_lock_error( "verify_lock", "shared_fcntl_overlap_test",
			"locks held on client after final unlock", status );
	} else if ( server_held( &name, clnt ) ) {
		status = -1;
		report_lock_error( "verify_lock", "shared_fcntl_overlap_test",
			"locks held on server after final unlock", status );
	}
	return( status );
}

int
shared_fcntl_test_part1(CLIENT *clnt, pathstr name, lockdesc *ldp)
{
	int status = 0;
	fcntlargs lkargs;
	fcntlargs unlkargs;
	verifyargs verfarg;

	assert( valid_addresses( (caddr_t)ldp, sizeof(struct lockdesc) ) &&
		valid_addresses( (caddr_t)clnt, sizeof(CLIENT) ) );
	lkargs.fa_cmd = F_SETLK;
	verfarg.va_lock = lkargs.fa_lock = *ldp;
	verfarg.va_name = lkargs.fa_name = name;
	unlkargs = lkargs;
	unlkargs.fa_lock.ld_type = F_UNLCK;
	/*
	 * Part 1.
	 *
	 * lock on the server
	 */
	if ( status = server_fcntl( &lkargs, clnt ) ) {
		report_lock_error( "server_fcntl", "shared_fcntl_test_part1",
			"unable to acquire shared lock on the server", status );
	/*
	 * verify on the client
	 */
	} else if ( !verify_lock( &verfarg ) ) {
		status = -1;
		report_lock_error( "verify_lock", "shared_fcntl_test_part1",
			"lock verification failure", status );
		(void)server_fcntl( &unlkargs, clnt );
	/*
	 * lock also on the client
	 */
	} else if ( status = local_fcntl( &lkargs ) ) {
		report_lock_error( "local_fcntl", "shared_fcntl_test_part1",
			"shared lock failure on the client with lock held on server",
			status );
		(void)server_fcntl( &unlkargs, clnt );
	/*
	 * unlock on the server
	 */
	} else if ( status = server_fcntl( &unlkargs, clnt ) ) {
		report_lock_error( "server_fcntl", "shared_fcntl_test_part1",
			"unable to release shared lock on the server", status );
		(void)local_fcntl( &unlkargs );
	/*
	 * check that there is still a lock held from the server's point
	 * of view
	 */
	} else if ( !server_held( &name, clnt ) ) {
		status = -1;
		report_lock_error( "server_held", "shared_fcntl_test_part1",
			"no locks held on server after client unlock", status );
		(void)local_fcntl( &unlkargs );
	/*
	 * re-verify the client's lock on the server
	 */
	} else if ( !server_verify( &verfarg, clnt ) ) {
		status = -1;
		report_lock_error( "server_verify", "shared_fcntl_test_part1",
			"server lock verification failure", status );
		(void)local_fcntl( &unlkargs );
	/*
	 * unlock on the client (final unlock)
	 */
	} else if ( status = local_fcntl( &unlkargs ) ) {
		report_lock_error( "local_fcntl", "shared_fcntl_test_part1",
			"unable to release shared lock on the client", status );
	/*
	 * verify that there are no locks from the points of view of the
	 * client and the server
	 */
	} else if ( locks_held( name ) ) {
		status = -1;
		report_lock_error( "locks_held", "shared_fcntl_test_part1",
			"locks held on client after server unlock", status );
	} else if ( server_held( &name, clnt ) ) {
		status = -1;
		report_lock_error( "server_held", "shared_fcntl_test_part1",
			"locks held on server after server unlock", status );
	/*
	 * make the test symmetrical by reversing the above
	 * locking/unlocking sequence
	 *
	 * first, lock on the client
	 */
	} else if ( status = local_fcntl( &lkargs ) ) {
		report_lock_error( "local_fcntl", "shared_fcntl_test_part1",
			"unable to acquire shared lock on the client", status );
	/*
	 * verify the lock from the point of view of the server
	 */
	} else if ( !server_verify( &verfarg, clnt ) ) {
		status = -1;
		report_lock_error( "server_verify", "shared_fcntl_test_part1",
			"server lock verification failure", status );
		(void)local_fcntl( &unlkargs );
	/*
	 * lock on the server
	 */
	} else if ( status = server_fcntl( &lkargs, clnt ) ) {
		report_lock_error( "server_fcntl", "shared_fcntl_test_part1",
			"unable to acquire shared lock on the server", status );
			(void)local_fcntl( &unlkargs );
	/*
	 * unlock on the client
	 */
	} else if ( status = local_fcntl( &unlkargs ) ) {
		report_lock_error( "local_fcntl", "shared_fcntl_test_part1",
			"unable to release shared lock on the client", status );
		(void)server_fcntl( &unlkargs, clnt );
	/*
	 * verify from the point of view of the client that there is
	 * still a lock held
	 */
	} else if ( !locks_held( name ) ) {
		status = -1;
		report_lock_error( "locks_held", "shared_fcntl_test_part1",
			"no locks held on server after client unlock", status );
		(void)server_fcntl( &unlkargs, clnt );
	/*
	 * verify that the server still holds its lock as expected
	 */
	} else if ( !verify_lock( &verfarg ) ) {
		status = -1;
		report_lock_error( "verify_lock", "shared_fcntl_test_part1",
			"lock verification failure", status );
		(void)server_fcntl( &unlkargs, clnt );
	/*
	 * unlock on the server
	 */
	} else if ( status = server_fcntl( &unlkargs, clnt ) ) {
		report_lock_error( "server_fcntl", "shared_fcntl_test_part1",
			"unable to release shared lock on the server",
			status );
	/*
	 * verify that ther are no locks from both points of view
	 */
	} else if ( locks_held( name ) ) {
		status = -1;
		report_lock_error( "verify_lock", "shared_fcntl_test_part1",
			"locks held on client after server unlock",
			status );
	} else if ( server_held( &name, clnt ) ) {
		status = -1;
		report_lock_error( "verify_lock", "shared_fcntl_test_part1",
			"locks held on server after server unlock",
			status );
	}
	return(status);
}

int
shared_fcntl_test_part2(CLIENT *clnt, pathstr name, lockdesc *ldp,
	off_t filesize)
{
	lockdesc lkdescB;
	off_t regionA_len;

	assert( valid_addresses( (caddr_t)ldp, sizeof(struct lockdesc) ) &&
		valid_addresses( (caddr_t)clnt, sizeof(CLIENT) ) );
	/*
	 * Part 2.
	 *
	 * region-A is described by ldp
	 * region-B is described by lkdescB
	 * define region B so that it is strictly contained within region A
	 */
	regionA_len = ldp->ld_len ? ldp->ld_len : (filesize - ldp->ld_offset);
	lkdescB.ld_offset = ldp->ld_offset + MAX( 1, (regionA_len / 4) );
	lkdescB.ld_len = MAX( 1, (regionA_len / 4) );
	lkdescB.ld_type = F_RDLCK;
	assert( lkdescB.ld_offset > ldp->ld_offset );
	assert( (lkdescB.ld_offset + lkdescB.ld_len) <
		(ldp->ld_offset + regionA_len) );
	return(shared_fcntl_overlap_test( clnt, name, ldp, &lkdescB ));
}

int
shared_fcntl_test_part3(CLIENT *clnt, pathstr name, lockdesc *ldp,
	off_t filesize)
{
	int status = 0;
	lockdesc lkdescB;
	off_t regionA_len;
	off_t regionB_end;

	assert( valid_addresses( (caddr_t)ldp, sizeof(struct lockdesc) ) &&
		valid_addresses( (caddr_t)clnt, sizeof(CLIENT) ) );
	regionA_len = ldp->ld_len ? ldp->ld_len : (filesize - ldp->ld_offset);
	/*
	 * Part 3.
	 *
	 * part 3 consists of front and end overlapping lock testing.
	 * To do this, the lock described by ldp (region A) cannot cover
	 * the entire file.  If the lock described by ldp (region A) does
	 * not start at 0, then two overlapping regions can be
	 * constructed.  Also, if region A starts at 0 but does not
	 * extend to the end of the file, two overlapping regions can be
	 * constructed.
	 */
	if (ldp->ld_offset) {
		/*
		 * construct region B to overlap the front of region A
		 */
		lkdescB.ld_offset =
			MAX( ldp->ld_offset - MAX( 1, (regionA_len / 4) ), 0 );
		regionB_end = MIN( ldp->ld_offset + MAX( 1, (regionA_len / 4) ),
			(ldp->ld_offset + regionA_len - 1) );
		lkdescB.ld_len = regionB_end - lkdescB.ld_offset;
		lkdescB.ld_type = F_RDLCK;
		/*
		 * assert the conditions for the two regions
		 */
		assert( lkdescB.ld_offset >= 0 );
		assert( lkdescB.ld_len > 0 );
		assert( lkdescB.ld_offset < ldp->ld_offset );
		assert( regionB_end > 0 );
		assert( regionB_end < (ldp->ld_offset + regionA_len) );
		assert( regionB_end > ldp->ld_offset );
		if ( status = shared_fcntl_overlap_test( clnt, name, ldp, &lkdescB ) ) {
			report_lock_error( "shared_fcntl_overlap_test",
				"shared_fcntl_test_part3", "part 3 failed", status );
		}
	}
	if ( !status && ((ldp->ld_offset + regionA_len) < filesize) ) {
		/*
		 * construct region B to overlap the end of region A
		 */
		lkdescB.ld_offset = MAX( (ldp->ld_offset + regionA_len) -
			MAX( 1, (regionA_len / 4) ), ldp->ld_offset + 1 );
		regionB_end = MIN( (ldp->ld_offset + regionA_len) +
			MAX( 1, (regionA_len / 4) ), filesize );
		lkdescB.ld_len = regionB_end - lkdescB.ld_offset;
		lkdescB.ld_type = F_RDLCK;
		/*
		 * assert the conditions for the two regions
		 */
		assert( lkdescB.ld_len > 0 );
		assert( lkdescB.ld_offset > ldp->ld_offset );
		assert( lkdescB.ld_offset < (ldp->ld_offset + regionA_len) );
		assert( regionB_end <= filesize );
		assert( regionB_end > (ldp->ld_offset + regionA_len) );
		if ( status = shared_fcntl_overlap_test( clnt, name, ldp, &lkdescB ) ) {
			report_lock_error( "shared_fcntl_overlap_test",
				"shared_fcntl_test_part3", "part 3 failed", status );
		}
	}
	return(status);
}

/*
 * perform several special lock tests here
 *
 * 1. First, verify that the following lock sequences behave as expected:
 *
 *		a)	lock on server (verify on client)
 *			lock on client (no verification is possible)
 *			unlock on server (verify on the server)
 *			unlock on client (verify on both)
 *
 *		b)	lock on client (verify on server)
 *			lock on server (no verification)
 *			unlock on client (verify on the client)
 *			unlock on server (verify on both)
 *
 * 2. Second, verify the same sequences as above but with the two locks
 *    not being identical in that one locked region is contained within
 *    the other.  Let region-A contain region-B.  The following sequences
 *    must be verified:
 *
 *		a)	lock region-A on the server
 *			lock region-B on the client
 *			unlock region-A on the server
 *			unlock region-B on the client
 *
 *		b)	lock region-A on the client
 *			lock region-B on the server
 *			unlock region-A on the client
 *			unlock region-B on the server
 *
 *		c)	lock region-B on the server
 *			lock region-A on the client
 *			unlock region-B on the server
 *			unlock region-A on the client
 *
 *		d)	lock region-B on the client
 *			lock region-A on the server
 *			unlock region-B on the client
 *			unlock region-A on the server
 *
 * 3. Third, verify overlapping locking.  Let region-A overlap the front
 *    of region-B but not contain region-B.  Thus, the start of region-A
 *    is strictly less than the start of region-B and the start of
 *    region-B is strictly less than the end of region-A.  The following
 *    sequences must be verified:
 *
 *		a)	lock region-A on the server
 *			lock region-B on the client
 *			unlock region-A on the server
 *			unlock region-B on the client
 *
 *		b)	lock region-A on the client
 *			lock region-B on the server
 *			unlock region-A on the client
 *			unlock region-B on the server
 *
 *		c)	lock region-B on the server
 *			lock region-A on the client
 *			unlock region-B on the server
 *			unlock region-A on the client
 *
 *		d)	lock region-B on the client
 *			lock region-A on the server
 *			unlock region-B on the client
 *			unlock region-A on the server
 *
 * There is a hole in the above testing in that no verification can be
 * done when both the client and the server hold locks.  A process cannot
 * verify its own locks.  This sort of verification requires one or two
 * verifier processes.  This is a possible extension to this test
 * application.  An RPC service for lock verification can be defined.
 * This is a little complicated in that when the client and server locking
 * processes are on different systems, a lock verification server will
 * need to be running on each system and each verification will have to be
 * done on both.
 */
int
shared_fcntl_test( CLIENT *clnt, pathstr name, lockdesc *ldp )
{
	int status = 0;
	struct stat sb;

	assert( valid_addresses( (caddr_t)ldp, sizeof(struct lockdesc) ) &&
		valid_addresses( (caddr_t)clnt, sizeof(CLIENT) ) );
	if (status = shared_fcntl_test_part1(clnt, name, ldp)) {
		report_lock_error( "shared_fcntl_test_part1", "shared_fcntl_test",
			"shared test part 1 failed", status );
	} else {
		/*
		 * now, for parts 2 and 3.
		 * we only do these if part 1 did not fail and the length of region A
		 * is greater than 2
		 */
		if ( stat( name, &sb ) == -1 ) {
			report_lock_error( "stat", "shared_fcntl_test", name, errno );
			return( -1 );
		}
		if ( (sb.st_size > 2) || (ldp->ld_len > 2) ) {
			if (status = shared_fcntl_test_part2(clnt, name, ldp, sb.st_size)) {
				report_lock_error( "shared_fcntl_test_part2",
					"shared_fcntl_test", "shared test part 2 failed", status );
			} else if (status = shared_fcntl_test_part3(clnt, name, ldp,
				sb.st_size)) {
					report_lock_error( "shared_fcntl_test_part3",
						"shared_fcntl_test", "shared test part 3 failed",
						status );
			}
		} else if ( Verbose ) {
			printf( "%s: shared_fcntl_test: skipping parts 2 and 3\n",
				Progname );
		}
	}
	return( status );
}

int
shared_flock_test( CLIENT *clnt, pathstr name, lockdesc *ldp )
{
	int status = 0;
	flockargs lkargs;
	flockargs unlkargs;
	verifyargs verfarg;

	assert( valid_addresses( (caddr_t)ldp, sizeof(struct lockdesc) ) &&
		valid_addresses( (caddr_t)clnt, sizeof(CLIENT) ) );
	assert(ldp->ld_type == F_RDLCK);
	lkargs.fa_op = LOCK_NB | LOCK_SH;
	unlkargs.fa_op = LOCK_UN;
	verfarg.va_name = lkargs.fa_name = unlkargs.fa_name = name;
	verfarg.va_lock.ld_offset = 0;
	verfarg.va_lock.ld_len = 0;
	verfarg.va_lock.ld_type = ldp->ld_type;
	/*
	 * lock on the server
	 */
	if ( status = server_flock( &lkargs, clnt ) ) {
		report_lock_error( "server_flock", "shared_flock_test",
			"unable to acquire shared lock on the server", status );
	/*
	 * verify on the client
	 */
	} else if ( !verify_lock( &verfarg ) ) {
		status = -1;
		report_lock_error( "verify_lock", "shared_flock_test",
			"lock verification failure", status );
		(void)server_flock( &unlkargs, clnt );
	/*
	 * lock also on the client
	 */
	} else if ( status = local_flock( &lkargs ) ) {
		report_lock_error( "local_flock", "shared_flock_test",
			"shared lock failure on the client with lock held on server",
			status );
		(void)server_flock( &unlkargs, clnt );
	/*
	 * unlock on the server
	 */
	} else if ( status = server_flock( &unlkargs, clnt ) ) {
		report_lock_error( "server_flock", "shared_flock_test",
			"unable to release shared lock on the server", status );
		(void)local_flock( &unlkargs );
	/*
	 * check that there is still a lock held from the server's point
	 * of view
	 */
	} else if ( !server_held( &name, clnt ) ) {
		status = -1;
		report_lock_error( "server_held", "shared_flock_test",
			"no locks held on server after client unlock", status );
		(void)local_flock( &unlkargs );
	/*
	 * re-verify the client's lock on the server
	 */
	} else if ( !server_verify( &verfarg, clnt ) ) {
		status = -1;
		report_lock_error( "server_verify", "shared_flock_test",
			"server lock verification failure", status );
		(void)local_flock( &unlkargs );
	/*
	 * unlock on the client (final unlock)
	 */
	} else if ( status = local_flock( &unlkargs ) ) {
		report_lock_error( "local_flock", "shared_flock_test",
			"unable to release shared lock on the client", status );
	/*
	 * verify that there are no locks from the points of view of the
	 * client and the server
	 */
	} else if ( locks_held( name ) ) {
		status = -1;
		report_lock_error( "locks_held", "shared_flock_test",
			"locks held on client after server unlock", status );
	} else if ( server_held( &name, clnt ) ) {
		status = -1;
		report_lock_error( "server_held", "shared_flock_test",
			"locks held on server after server unlock", status );
	/*
	 * make the test symmetrical by reversing the above
	 * locking/unlocking sequence
	 *
	 * first, lock on the client
	 */
	} else if ( status = local_flock( &lkargs ) ) {
		report_lock_error( "local_flock", "shared_flock_test",
			"unable to acquire shared lock on the client", status );
	/*
	 * verify the lock from the point of view of the server
	 */
	} else if ( !server_verify( &verfarg, clnt ) ) {
		status = -1;
		report_lock_error( "server_verify", "shared_flock_test",
			"server lock verification failure", status );
		(void)local_flock( &unlkargs );
	/*
	 * lock on the server
	 */
	} else if ( status = server_flock( &lkargs, clnt ) ) {
		report_lock_error( "server_flock", "shared_flock_test",
			"unable to acquire shared lock on the server", status );
			(void)local_flock( &unlkargs );
	/*
	 * unlock on the client
	 */
	} else if ( status = local_flock( &unlkargs ) ) {
		report_lock_error( "local_flock", "shared_flock_test",
			"unable to release shared lock on the client", status );
		(void)server_flock( &unlkargs, clnt );
	/*
	 * verify from the point of view of the client that there is
	 * still a lock held
	 */
	} else if ( !locks_held( name ) ) {
		status = -1;
		report_lock_error( "locks_held", "shared_flock_test",
			"no locks held on server after client unlock", status );
		(void)server_flock( &unlkargs, clnt );
	/*
	 * verify that the server still holds its lock as expected
	 */
	} else if ( !verify_lock( &verfarg ) ) {
		status = -1;
		report_lock_error( "verify_lock", "shared_flock_test",
			"lock verification failure", status );
		(void)server_flock( &unlkargs, clnt );
	/*
	 * unlock on the server
	 */
	} else if ( status = server_flock( &unlkargs, clnt ) ) {
		report_lock_error( "server_flock", "shared_flock_test",
			"unable to release shared lock on the server",
			status );
	/*
	 * verify that ther are no locks from both points of view
	 */
	} else if ( locks_held( name ) ) {
		status = -1;
		report_lock_error( "verify_lock", "shared_flock_test",
			"locks held on client after server unlock",
			status );
	} else if ( server_held( &name, clnt ) ) {
		status = -1;
		report_lock_error( "verify_lock", "shared_flock_test",
			"locks held on server after server unlock",
			status );
	}
	return(status);
}

int
basic_file_test( CLIENT *clnt, pathstr file_name, int size, locktype lktype )
{
	lockdesc lkdesc;
	int status = 0;

	assert( size > 0 );
	assert( valid_addresses( (caddr_t)file_name, 1 ) &&
		valid_addresses( (caddr_t)clnt, sizeof(CLIENT) ) );
	/*
	 * Test the locking of an entire file using fcntl, flock, and lockf.
	 */
	lkdesc.ld_offset = (off_t)0;
	lkdesc.ld_len = 0;
	lkdesc.ld_type = (lktype == LOCK_SHARED) ? F_RDLCK : F_WRLCK;
	if ((status = basic_fcntl_test(clnt, file_name, &lkdesc)) == 0) {
		if ((status = basic_flock_test(clnt, file_name, &lkdesc)) == 0) {
			if (lktype == LOCK_EXCLUSIVE) {
				status = basic_lockf_test(clnt, file_name, &lkdesc);
			}
		}
	}
	return(status);
}

int
shared_file_test( CLIENT *clnt, pathstr file_name, int size )
{
	lockdesc lkdesc;
	int status = 0;

	assert( size > 0 );
	assert( valid_addresses( (caddr_t)file_name, 1 ) &&
		valid_addresses( (caddr_t)clnt, sizeof(CLIENT) ) );
	lkdesc.ld_offset = 0;
	lkdesc.ld_len = 0;
	lkdesc.ld_type = F_RDLCK;
	if ((status = shared_fcntl_test( clnt, file_name, &lkdesc)) == 0) {
		status = shared_flock_test( clnt, file_name, &lkdesc);
	}
	return( status );
}

/*
 * test record locking
 * record locking is defined as locking a portion of the file
 */
int
basic_record_test( CLIENT *clnt, pathstr file_name, int size,
	locktype lktype )
{
	lockdesc lkdesc;
	int status;

	assert( size > 0 );
	assert( valid_addresses( (caddr_t)file_name, 1 ) &&
		valid_addresses( (caddr_t)clnt, sizeof(CLIENT) ) );
	lkdesc.ld_offset = size/4;
	lkdesc.ld_len = size/4;
	lkdesc.ld_type = (lktype == LOCK_SHARED) ? F_RDLCK : F_WRLCK;
	if ((status = basic_fcntl_test(clnt, file_name, &lkdesc)) == 0) {
		if (lktype == LOCK_EXCLUSIVE) {
			status = basic_lockf_test(clnt, file_name, &lkdesc);
		}
	}
	return(status);
}

int
shared_record_test( CLIENT *clnt, pathstr file_name, int size )
{
	lockdesc lkdesc;
	int status;

	assert( size > 0 );
	assert( valid_addresses( (caddr_t)file_name, 1 ) &&
		valid_addresses( (caddr_t)clnt, sizeof(CLIENT) ) );
	lkdesc.ld_type = F_RDLCK;
	lkdesc.ld_offset = size/4;
	lkdesc.ld_len = size/4;
	return( shared_fcntl_test( clnt, file_name, &lkdesc) );
}

int
test_shared_file( CLIENT *clnt, pathstr file_name, int size )
{
	int status = 0;

	assert( size > 0 );
	assert( valid_addresses( (caddr_t)file_name, 1 ) &&
		valid_addresses( (caddr_t)clnt, sizeof(CLIENT) ) );
	if ( status = basic_file_test( clnt, file_name, size, LOCK_SHARED ) ) {
		report_lock_error( "basic_file_test", "test_shared_file",
			"basic shared file locking test failed", 0 );
	} else if ( status = shared_file_test( clnt, file_name, size ) ) {
		report_lock_error( "shared_file_test", "test_shared_file",
			"special shared file locking test failed", 0 );
	}
	return( status );
}

int
test_shared_record( CLIENT *clnt, pathstr file_name, int size )
{
	int status = 0;

	assert( size > 0 );
	assert( valid_addresses( (caddr_t)file_name, 1 ) &&
		valid_addresses( (caddr_t)clnt, sizeof(CLIENT) ) );
	if ( status = basic_record_test( clnt, file_name, size, F_RDLCK ) ) {
		report_lock_error( "basic_record_test", "test_shared_record",
			"basic shared record locking test failed", 0 );
	} else if ( status = shared_record_test( clnt, file_name, size ) ) {
		report_lock_error( "shared_record_test", "test_shared_record",
			"special shared record locking test failed", 0 );
	}
	return( status );
}

/*
 * phase one testing:  test exclusive locking
 */
int
phase_one( CLIENT *clnt, pathstr file_name, int size )
{
	int status = 0;

	if ( Verbose ) {
		(void)printf( "%s: file and record locking test phase 1\n", Progname );
		(void)fflush( stdout );
	}
	assert( size > 0 );
	assert( valid_addresses( (caddr_t)file_name, 1 ) &&
		valid_addresses( (caddr_t)clnt, sizeof(CLIENT) ) );
	if ( Verbose > 1 ) {
		(void)printf( "%s: phase_one: file %s, size %d\n", Progname, file_name,
			size );
		(void)fflush( stdout );
	}
	if ( status = basic_file_test( clnt, file_name, size, LOCK_EXCLUSIVE ) ) {
		report_lock_error( "basic_file_test", "phase_one",
			"exclusive file locking test failed", 0 );
	} else {
		if ( Verbose ) {
			(void)printf( "%s: file locking test passed\n", Progname );
			(void)fflush( stdout );
		}
		if ( status = basic_record_test( clnt, file_name, size,
			LOCK_EXCLUSIVE ) ) {
				report_lock_error( "basic_record_test", "phase_one",
					"exclusive record locking test failed", 0 );
		} else if ( Verbose ) {
			(void)printf( "%s: record locking test passed\n", Progname );
			(void)fflush( stdout );
		}
	}
	return( status );
}

/*
 * phase two testing:  test shared locks
 */
int
phase_two( CLIENT *clnt, pathstr file_name, int size )
{
	int status = 0;

	if ( Verbose ) {
		(void)printf( "%s: file and record locking test phase 2\n", Progname );
		(void)fflush( stdout );
	}
	assert( size > 0 );
	assert( valid_addresses( (caddr_t)file_name, 1 ) &&
		valid_addresses( (caddr_t)clnt, sizeof(CLIENT) ) );
	if ( Verbose > 1 ) {
		(void)printf( "%s: phase_two: file %s, size %d\n", Progname, file_name,
			size );
		(void)fflush( stdout );
	}
	if ( status = test_shared_file( clnt, file_name, size ) ) {
		report_lock_error( "test_shared_file", "phase_two",
			"shared file locking test failed", 0 );
	} else {
		if ( Verbose ) {
			(void)printf( "%s: shared file locking test passed\n", Progname );
			(void)fflush( stdout );
		}
		if ( status = test_shared_record( clnt, file_name, size ) ) {
			report_lock_error( "test_shared_record", "phase_two",
				"shared record locking test failed", 0 );
		} else if ( Verbose ) {
			(void)printf( "%s: shared record locking test passed\n", Progname );
			(void)fflush( stdout );
		}
	}
	return( status );
}

/*
 * perform all of the test phases
 */
int
perform_tests( CLIENT *clnt, pathstr file_name, int size )
{
	int status = 0;

	assert( size > 0 );
	assert( valid_addresses( (caddr_t)file_name, 1 ) &&
		valid_addresses( (caddr_t)clnt, sizeof(CLIENT) ) );
	if ( (status = phase_one( clnt, file_name, size )) == 0 ) {
		status = phase_two( clnt, file_name, size );
	}
	clnt_destroy( clnt );
	return( status );
}

int
build_file( pathstr file_name, int size )
{
	int i;
	int status = 0;
	int len;
	int fd;
	char *mbp;
	int n;
	struct stat statbuf;
	char filebuf[FILEBUFLEN];
	int writes;

	assert( size > 0 );
	assert( valid_addresses( (caddr_t)file_name, 1 ) );
	if ( (status = stat( file_name, &statbuf )) || (statbuf.st_size < size) ) {
		if ( Verbose > 1 ) {
			(void)printf( "%s: build_file: file %s, size %d\n",
				Progname, file_name, size );
			(void)fflush( stdout );
		}
		/*
		 * if the file exists, we need to change its mode
		 */
		if ( status == 0 ) {
			if ( chmod( file_name, CREATE_MODE ) == -1 ) {
				status = errno;
				report_lock_error( "chmod", "build_file", file_name, errno );
				return( status );
			}
		}
		status = 0;
		if ( (fd = open_file( file_name, CREATE_FLAGS, CREATE_MODE )) == -1 ) {
			status = errno;
			report_lock_error( "open_file", "build_file", file_name, errno );
		} else {
			/*
			 * fill in the file buffer
			 */
			for ( i = 0; i < FILEBUFLEN; i++ ) {
				filebuf[i] = (char)(i % 128);
			}
			/*
			 * calculate the number of writes to be done
			 */
			writes = (size + FILEBUFLEN - 1) / FILEBUFLEN;
			/*
			 * do all the writes
			 */
			while ( writes-- && !status ) {
				/*
				 * set the write length
				 * this will be FILEBUFLEN or size
				 * size indicates the remaining length to write
				 */
				if ( size >= FILEBUFLEN ) {
					len = FILEBUFLEN;
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
						status = errno;
						report_lock_error( "write", "build_file", file_name,
							errno );
						break;
					} else if ( n == 0 ) {
						fprintf( stderr, "%s: zero-length write\n",
							Progname );
						status = -1;
						break;
					} else {
						len -= n;
						mbp += n;
					}
				} while ( len );
			}
			/*
			 * make sure the mode is correct
			 */
			if ( status == 0 ) {
				if ( chmod( file_name, CREATE_MODE ) == -1 ) {
					status = errno;
					report_lock_error( "chmod", "build_file", file_name,
						errno );
					return( status );
				}
			}
		}
	} else if ( ((statbuf.st_mode | CREATE_MODE) != CREATE_MODE) &&
		(chmod( file_name, CREATE_MODE ) == -1) ) {
			report_lock_error( "chmod", "build_file", file_name, errno );
			status = errno;
	} else if ( open_file( file_name, OPEN_FLAGS, DEFAULT_MODE ) == -1 ) {
		fprintf( stderr, "%s: %s: unable to open file\n", Progname, file_name );
		status = errno;
	}
	return( status );
}

CLIENT *
server_init( char *hostname, pathstr path )
{
	struct servopts sops;
	CLIENT *clnt = NULL;
	int status;

	assert( valid_addresses( (caddr_t)hostname, 1 ) &&
		valid_addresses( (caddr_t)path, 1 ) );
	if ( clnt = clnt_create( hostname, LOCKTESTPROG, LKTESTVERS_ONE, "tcp" ) ) {
		if ( status = reset_server( clnt ) ) {
			fprintf( stderr,
				"%s: server_init: server reset failed, status = %d\n",
				Progname );
			clnt_destroy( clnt );
			clnt = NULL;
			if ( status > 0 ) {
				errno = status;
				report_lock_error( "set_server_opts", "server_init", hostname,
					errno );
			}
		} else {
			sops.so_verbose = Verbose;
			sops.so_altverify = AlternateVerify;
			sops.so_directory = path;
			if ( status = set_server_opts( &sops, clnt ) ) {
				clnt_destroy( clnt );
				clnt = NULL;
				fprintf( stderr, "%s: server_init: status = %d\n", Progname,
					status );
				if ( status > 0 ) {
					errno = status;
					report_lock_error( "set_server_opts", "server_init",
						hostname, errno );
				}
			}
		}
	} else {
		clnt = NULL;
		fprintf( stderr, "%s: server_init: %s\n", Progname,
			clnt_spcreateerror( hostname ) );
	}
	return( clnt );
}

void
usage(void)
{
	(void)fprintf( stderr, "usage: %s [-v] [path] [host:path]\n", Progname );
}

/*
 * starting point
 */

int
main( int argc, char **argv )
{
	struct stat sb;
	struct mntent *mnt;
	char pathbuf[MAXPATHLEN];
	int status = 0;
	char *hostname = NULL;
	pathstr filename = "testfile";
	pathstr localpath = NULL;
	pathstr remotepath = NULL;
	int size = FILE_SIZE;
	char *token;
	int opt;
	CLIENT *clnt;

	Progname = *argv;
	while ( (opt = getopt( argc, argv, OPTSTR )) != EOF ) {
		switch ( opt ) {
			case 'a':
				AlternateVerify++;
				break;
			case 'v':
				Verbose++;
				break;
			default:
				usage();
				exit( -1 );
		}
	}
	/*
	 * if there is a remaining argument, it must be the path to the test
	 * file, either the directory in which the file resides or the file
	 * itself -- this path is the local path
	 */
	if ( optind < argc ) {
		localpath = argv[optind];
		/*
		 * if the supplied path is to a file, set the file name to NULL
		 * we will pick up the file name after the absolute path conversion
		 */
		if (((stat(localpath, &sb) == -1) && (errno == ENOENT)) ||
			isfile(localpath)) {
				/*
				 * extract the file name from the path (last component)
				 */
				filename = strrchr(localpath, '/');
				assert(filename);
				*filename = '\0';
				filename++;
		}
		optind++;
		/*
		 * get to the directory where the testing is to be performed
		 */
		if (chdir(localpath) == -1) {
			report_lock_error( "chdir", "main", localpath, errno );
			exit(errno);
		}
	}
	/*
	 * convert the local path name to an absolue path
	 */
	localpath = absolute_path(NULL);
	assert(localpath);
	assert(filename);
	/*
	 * locate the mount point for the given absolute path
	 */
	mnt = find_mount_point(localpath);
	assert(mnt);
	if (Verbose) {
		printf("%s: fsname %s\n", Progname, mnt->mnt_fsname);
	}
	remotepath = localpath;
	if (optind < argc) {
		get_host_info(argv[optind], &hostname, &remotepath);
	} else {
		/*
		 * given the file system name for the mount point, determine the
		 * host name
		 */
		get_host_info(mnt->mnt_fsname, &hostname, &remotepath);
		if (isfile(remotepath)) {
			fprintf(stderr, "%s cannot be a regular file\n", remotepath);
			exit(-1);
		}
	}
	assert(hostname);
	assert(remotepath);
	if ( Verbose ) {
		printf( "%s: hostname = %s, filename = %s, filesize = %d\n", Progname,
			hostname, filename, size );
		printf( "%s: localpath = %s\n", Progname, localpath );
		printf( "%s: remotepath = %s\n", Progname, remotepath );
	}
	open_file_init();
	if ( clnt = server_init( hostname, remotepath ) ) {
		if ( !(status = build_file( filename, size )) ) {
			status = perform_tests( clnt, filename, size );
		} else {
			(void)fprintf( stderr, "%s: execute: build_file failed\n",
				Progname );
		}
		if ( !status ) {
			printf( "%s: passed\n", Progname );
		} else {
			printf( "%s: failed\n", Progname );
		}
	} else {
		status = -1;
	}
	return( status );
}
