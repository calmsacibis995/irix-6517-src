#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <stdio.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <stdlib.h>

char *
get_uid_name( uid_t uid )
{
	struct passwd *pwp;
	char *name = "unknown uid";

	if ( pwp = getpwuid( uid ) ) {
		name = pwp->pw_name;
	}
	return( name );
}

char *
get_gid_name( gid_t gid )
{
	struct group *grp;
	char *name = "unknown gid";

	if ( grp = getgrgid( gid ) ) {
		name = grp->gr_name;
	}
	return( name );
}

main( int argc, char **argv )
{
	int status;
	char *progname;
	char *filename;
	struct stat sb;
	struct statvfs svb;

	progname = *argv++;
	argc--;
	if ( argc ) {
		setpwent();
		setgrent();
		while ( argc-- ) {
			filename = *argv++;
			if ( stat( filename, &sb ) == -1 ) {
				perror( filename );
			} else if ( statvfs( filename, &svb ) == -1 ) {
				perror( filename );
			} else {
				printf( "%s:\n", filename );
				printf( "\tst_dev 0x%x\n", (int)sb.st_dev );
				printf( "\tst_ino %d\n", (int)sb.st_ino );
				printf( "\tst_mode 0x%x\n", (int)sb.st_mode );
				printf( "\tst_nlink %d\n", (int)sb.st_nlink );
				printf( "\tst_uid %s(%d)\n", get_uid_name( sb.st_uid ),
					(int)sb.st_uid );
				printf( "\tst_gid %s(%d)\n", get_gid_name( sb.st_gid ),
					(int)sb.st_gid );
				printf( "\tst_rdev 0x%x\n", (int)sb.st_rdev );
				printf( "\tst_size %d\n", (int)sb.st_size );
				printf( "\tst_atim %s", ctime( &sb.st_atim.tv_sec ) );
				printf( "\tst_mtim %s", ctime( &sb.st_mtim.tv_sec ) );
				printf( "\tst_ctim %s", ctime( &sb.st_ctim.tv_sec ) );
				printf( "\tst_blksize %d(%d, %d)\n", (int)sb.st_blksize,
					(int)svb.f_bsize, (int)svb.f_frsize );
				printf( "\tst_blocks %d\n", (int)sb.st_blocks );
				printf( "\tst_fstype %s\n", sb.st_fstype );
			}
		}
		endpwent();
		endgrent();
	} else {
	}
	return( status );
}
