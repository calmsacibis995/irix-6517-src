#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include "types.h"
#include "mlog.h"

char *
open_pathalloc( char *dirname, char *basename, pid_t pid )
{
	size_t dirlen;
	size_t pidlen;
	char *namebuf;

	if ( strcmp( dirname, "/" )) {
		dirlen = strlen( dirname );
	} else {
		dirlen = 0;
		dirname = "";
	}

	if ( pid ) {
		assert( PID_MAX < 1000000 );
		pidlen = 1 + 6;
	} else {
		pidlen = 0;
	}
	namebuf = ( char * )calloc( 1,
				    dirlen
				    +
				    1
				    +
				    strlen( basename )
				    +
				    pidlen
				    +
				    1 );
	assert( namebuf );

	if ( pid ) {
		( void )sprintf( namebuf, "%s/%s.%d", dirname, basename, pid );
	} else {
		( void )sprintf( namebuf, "%s/%s", dirname, basename );
	}

	return namebuf;
}

intgen_t
open_trwp( char *pathname )
{
	intgen_t fd;

	fd = open( pathname, O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR );
	if ( fd < 0 ) {
		mlog( MLOG_NORMAL,
		      "could not create %s: %s\n",
		      pathname,
		      strerror( errno ));
	}

	return fd;
}

intgen_t
open_erwp( char *pathname )
{
	intgen_t fd;

	fd = open( pathname, O_EXCL | O_CREAT | O_RDWR, S_IRUSR | S_IWUSR );
	if ( fd < 0 ) {
		mlog( MLOG_DEBUG,
		      "could not create %s: %s\n",
		      pathname,
		      strerror( errno ));
	}

	return fd;
}

intgen_t
open_rwp( char *pathname )
{
	intgen_t fd;

	fd = open( pathname, O_RDWR );

	return fd;
}

intgen_t
mkdir_tp( char *pathname )
{
	intgen_t rval;

	rval = mkdir( pathname, S_IRWXU );

	return rval;
}

intgen_t
open_trwdb( char *dirname, char *basename, pid_t pid )
{
	char *pathname;
	intgen_t fd;

	pathname = open_pathalloc( dirname, basename, pid );
	fd = open_trwp( pathname );
	free( ( void * )pathname );

	return fd;
}

intgen_t
open_erwdb( char *dirname, char *basename, pid_t pid )
{
	char *pathname;
	intgen_t fd;

	pathname = open_pathalloc( dirname, basename, pid );
	fd = open_erwp( pathname );
	free( ( void * )pathname );

	return fd;
}

intgen_t
open_rwdb( char *dirname, char *basename, pid_t pid )
{
	char *pathname;
	intgen_t fd;

	pathname = open_pathalloc( dirname, basename, pid );
	fd = open_rwp( pathname );
	free( ( void * )pathname );

	return fd;
}
