#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/errno.h>
#include "tests.h"
#include "subr.h"

#define DEFAULT_LEVELS		5
#define DEFAULT_MINDIRS		5
#define DEFAULT_MAXDIRS		10
#define DEFAULT_MINFILES	5
#define DEFAULT_MAXFILES	10
#define DEFAULT_MINSIZE		4096
#define DEFAULT_MAXSIZE		4096*4
#define BASE_FILE_NAME		"testfile"
#define BASE_DIR_NAME		"testdir"
#define DIRPERMS			0755

char *Progname = "checktree";
int Verbose = 0;

extern char *optarg;
extern int optind;
extern int errno;

void
usage( void )
{
	fprintf( stderr, "usage: %s [-h height] [-d mindirs,maxdirs] [-f minfiles,maxfiles] [-Vpi] [dir1 dir2 ...]\n", Progname );
}

void
set_range( char *argp, int *minval, int *maxval )
{
	char *tokp;

	if ( argp ) {
		tokp = strtok( argp, ", " );
		if ( tokp ) {
			*minval = atoi( tokp );
			tokp = strtok( NULL, " " );
			if ( tokp ) {
				*maxval = atoi( tokp );
			} else {
				fprintf( stderr,
				"set_range: invalid range specification\n" );
				exit( 1 );
			}
		} else {
			*minval = *maxval = atoi( argp );
		}
	} else {
		fprintf( stderr, "set_range: NULL argp\n" );
		exit( 1 );
	}
}

main( int argc, char **argv )
{
	int opt;
	int argerr = 0;
	int lev = DEFAULT_LEVELS;
	int mindirs = DEFAULT_MINDIRS;
	int maxdirs = DEFAULT_MAXDIRS;
	int minfiles = DEFAULT_MINFILES;
	int maxfiles = DEFAULT_MAXFILES;
	int parallel = 0;
	int children = 0;
	int use_current_dir = 0;
	int i;
	int ignore_err = 0;
	int files_checked = 0;
	int dirs_checked = 0;
	int toterror = 0;
	int pid;

	Progname = *argv;
	while ( (opt = getopt( argc, argv, "h:d:f:Vpi" )) != -1 ) {
		switch ( opt ) {
			case 'h':		/* set tree height (levels) */
				lev = atoi( optarg );
				break;
			case 'd':		/* set min and max dirs */
				set_range( optarg, &mindirs, &maxdirs );
				break;
			case 'f':		/* set min and max files */
				set_range( optarg, &minfiles, &maxfiles );
				break;
			case 'p':
				parallel = 1;
				break;
			case 'i':
				ignore_err = 1;
				break;
			case 'V':		/* verbosity & debugging */
				Verbose++;
				break;
			default:
				argerr++;
		}
	}
	if ( argerr ) {
		usage();
		exit( 1 );
	}
	initialize();
	if ( optind >= argc ) {
		use_current_dir = 1;
	}
	if ( Verbose ) {
		printf( "%s:\n", Progname );
		printf( "\ttarget directories:\n" );
		if ( use_current_dir ) {
			printf( "\t\tcurrent dir\n" );
		} else {
			for ( i = optind; i < argc; i++ ) {
				printf( "\t\t%s\n", argv[i] );
			}
		}
		printf( "\t%d levels\n", lev );
		printf( "\t%d - %d subdirs at each level\n", mindirs, maxdirs );
		printf( "\t%d - %d files at each level\n", minfiles, maxfiles );
	}
	if ( use_current_dir ) {
		checktree( lev, maxfiles, maxdirs, BASE_FILE_NAME,
			BASE_DIR_NAME, &files_checked, &dirs_checked, &toterror,
			ignore_err );
		printf( "checktree: %d files, %d dirs, %d errors\n", files_checked,
			dirs_checked, toterror );
	} else if ( parallel ) {
		while ( optind < argc ) {
			if ( (pid = fork()) == -1 ) {
				error( "fork error" );
			} else if ( pid ) {
				children++;
			} else {
				mtestdir( argv[optind] );
				checktree( lev, maxfiles, maxdirs, BASE_FILE_NAME,
					BASE_DIR_NAME, &files_checked, &dirs_checked, &toterror,
					ignore_err );
				printf( "checktree: %d files, %d dirs, %d errors\n",
					files_checked, dirs_checked, toterror );
				exit( toterror );
			}
			optind++;
		}
		/*
		 * wait for all processes to finish
		 */
		while ( wait( NULL ) != -1 ) {
			;
		}
	} else {
		while ( optind < argc ) {
			mtestdir( argv[optind] );
			checktree( lev, maxfiles, maxdirs,
				BASE_FILE_NAME, BASE_DIR_NAME,
				&files_checked, &dirs_checked, &toterror,
				ignore_err );
			printf( "checktree: %d files, %d dirs, %d errors\n",
				files_checked, dirs_checked, toterror );
			optind++;
		}
	}
	return( toterror );
}
