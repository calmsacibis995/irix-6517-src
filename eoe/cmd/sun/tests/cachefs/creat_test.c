#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/errno.h>

#define RW_ALL_MODE		0666
#define RO_MODE			0444

#define TESTFILE		"creat.testfile"
#define WRITESTRING		"test data"

extern int errno;

/*
 * Test to show O_CREAT effect over NFS.
 */

main ( argc, argv )
	int argc;
	char **argv;
{

	int status;
	int fd;
	int bytes;
	int test_status = 0;
	char *progname;

	progname = *argv;

	/*
	 * Remove the file.
	 */

	status = unlink( TESTFILE );

	/*
	 * process any error from unlink
	 * ENOENT is acceptable, all others are unacceptable
	 */

	if ( status == -1 ) {
		switch( errno ) {
			case ENOENT:
				break;
			default:
				fprintf( stderr, "%s: unlink", progname );
				perror( TESTFILE );
				exit( errno );
		}
	}

	/* 
	 * Open it read/write.  Add some data.  Close it.
	 */

	fd = open( TESTFILE, O_RDWR | O_CREAT, RW_ALL_MODE );

	/*
	 * check for an error
	 * this first open should succeed
	 */

	if ( fd == -1 ) {
		fprintf( stderr, "%s: open(O_RDWR | O_CREAT): ", progname );
		perror( TESTFILE );
		exit( errno );
	}
	bytes = write (fd, WRITESTRING, sizeof( WRITESTRING ));
	switch ( bytes ) {
		case -1:
			fprintf( stderr, "%s: ", progname );
			perror( "write" );
			exit( errno );
		case 0:
			fprintf( stderr, "%s: 0-byte write\n", progname );
			exit( -1 );
		default:
			if ( bytes < sizeof( WRITESTRING ) ) {
				fprintf( stderr, "%s: WARNING: short write (%d of %d)\n",
					progname, bytes, sizeof( WRITESTRING ) );
			}
	}
	status = close (fd);
	if ( status == -1 ) {
		fprintf( stderr, "%s: ", progname );
		perror( "close" );
		exit( errno );
	}

	/* 
	 * Change the status to read only, try to open it again.
	 * The man pages states: "O_CREAT  If the file exists, this 
	 * flag has no effect."  But it does.
	 */

	status = chmod( TESTFILE, RO_MODE );
	if ( status == -1 ) {
		fprintf( stderr, "%s: chmod(%o): ", progname, RO_MODE );
		perror( TESTFILE );
		exit( errno );
	}
	fd = open(TESTFILE, O_RDONLY | O_CREAT, RW_ALL_MODE);
	if ( fd == -1 ) {
		fprintf( stderr, "%s: open(O_RDONLY | O_CREAT): ", progname );
		perror( TESTFILE );
		test_status = errno;
	} else {
		printf( "%s: open(%s, O_RDONLY | O_CREAT, %o) succeeded\n", progname,
			TESTFILE, RW_ALL_MODE );
		status = close( fd );
		if ( status == -1 ) {
			fprintf( stderr, "%s: ", progname );
			perror( "close" );
			exit( errno );
		}
	}
	fd = open(TESTFILE, O_RDONLY, RW_ALL_MODE);
	if ( fd == -1 ) {
		fprintf( stderr, "%s: open(O_RDONLY): ", progname );
		perror( TESTFILE );
		test_status = errno;
	} else {
		printf( "%s: open(%s, O_RDONLY, %o) succeeded\n", progname, TESTFILE,
			RW_ALL_MODE );
		status = close( fd );
		if ( status == -1 ) {
			fprintf( stderr, "%s: ", progname );
			perror( "close" );
			exit( errno );
		}
	}

	exit( test_status );
}
