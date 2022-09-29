#include <stdio.h>
#include <sys/types.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <stdlib.h>

char *Progname;

extern int errno;

int
main( int argc, char **argv )
{
	int status = 0;
	char *fsname;
	struct statfs statfs_buf;
	struct statvfs statvfs_buf;
	int fd;

	Progname = *argv++;
	while ( --argc ) {
		fsname = *argv++;
		/*
		 * check statfs and fstatfs
		 */
		if ( statfs( fsname, &statfs_buf, sizeof(statfs_buf), 0) ==
			-1 ) {
				status = errno;
				perror( fsname );
				fprintf( stderr, "statfs failed\n" );
		} else if ( (fd = open( fsname, O_RDONLY, 0 )) == -1 ) {
			status = errno;
			perror( fsname );
			fprintf( stderr, "open failed\n" );
			continue;
		} else if ( fstatfs( fd, &statfs_buf, sizeof(statfs_buf), 0)
			== -1 ) {
				status = errno;
				perror( fsname );
				fprintf( stderr, "fstatfs failed\n" );
		} else {
			printf( "statfs and fstatfs succeeded for %s\n", fsname );
		}
		if ( statvfs( fsname, &statvfs_buf ) == -1 ) {
			status = errno;
			perror( fsname );
			fprintf( stderr, "statvfs failed\n" );
		} else if ( fstatvfs( fd, &statvfs_buf ) == -1 ) {
			status = errno;
			perror( fsname );
			fprintf( stderr, "fstatvfs failed\n" );
		} else {
			printf( "statvfs and fstatvfs succeeded for %s\n", fsname );
		}
		close( fd );
	}
	return( status );
}
