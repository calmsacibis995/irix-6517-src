#include <sys/param.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <sys/file.h>
#include <rpc/rpc.h>
#include <rpc/svc.h>
#include "lk_test.h"

extern char *Progname;
extern int Verbose;
extern int errno;

static char *
flockop_to_str(int op)
{
	static char opstr[25];

	opstr[0] = 0;
	if (op & LOCK_SH) {
		strcat(opstr, "LOCK_SH ");
	}
	if (op & LOCK_EX) {
		strcat(opstr, "LOCK_EX ");
	}
	if (op & LOCK_NB) {
		strcat(opstr, "LOCK_NB ");
	}
	if (op & LOCK_UN) {
		strcat(opstr, "LOCK_UN ");
	}
	return(opstr);
}

void
print_flockargs( struct flockargs *fap, char *leader )
{
	printf( "%sfilename = %s\n", leader, fap->fa_name );
	printf( "%s      op = %s\n", leader, flockop_to_str( fap->fa_op ) );
}

static char *
fcntlcmd_to_str(int cmd)
{
	static char *cmdstr = "UNKNOWN";

	switch (cmd) {
		case F_GETLK:
			cmdstr = "F_GETLK";
			break;
		case F_SETLK:
			cmdstr = "F_SETLK";
			break;
		default:
			cmdstr = "ILLEGAL";
	}
	return(cmdstr);
}

void
print_fcntlargs( struct fcntlargs *fap, char *leader )
{
	printf( "%sfilename = %s\n", leader, fap->fa_name );
	printf( "%s     cmd = %s\n", leader, fcntlcmd_to_str( fap->fa_cmd ) );
	printf( "%s  offset = %d\n", leader, fap->fa_lock.ld_offset );
	printf( "%s     len = %d\n", leader, fap->fa_lock.ld_len );
	printf( "%s    type = %s\n", leader,
		locktype_to_str(fap->fa_lock.ld_type) );
}

void
print_verifyargs( struct verifyargs *vap, char *leader )
{
	printf( "%sfilename = %s\n", leader, vap->va_name );
	printf( "%s  offset = %d\n", leader, vap->va_lock.ld_offset );
	printf( "%s     len = %d\n", leader, vap->va_lock.ld_len );
	printf( "%s    type = %s\n", leader,
		locktype_to_str(vap->va_lock.ld_type) );
}

static char *
lockfunc_to_str( int func )
{
	static char *funcstr = "UNKNOWN";

	switch (func) {
		case F_ULOCK:
			funcstr = "F_ULOCK";
			break;
		case F_LOCK:
			funcstr = "F_LOCK";
			break;
		case F_TLOCK:
			funcstr = "F_TLOCK";
			break;
		case F_TEST:
			funcstr = "F_TEST";
			break;
		default:
			funcstr = "ILLEGAL";
	}
	return(funcstr);
}

void
print_lockfargs( struct lockfargs *lap, char *leader )
{
	printf( "%sfilename = %s\n", leader, lap->la_name );
	printf( "%s     cmd = %s\n", leader, lockfunc_to_str( lap->la_func ) );
	printf( "%s  offset = %d\n", leader, lap->la_offset );
	printf( "%s     len = %d\n", leader, lap->la_size );
}
