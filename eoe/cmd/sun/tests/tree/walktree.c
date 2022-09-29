#include <stdio.h>
#include <sys/errno.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <dirent.h>
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
#define OPTSTR		"Vp"
#define CONFIGPATH			"./.exconf"

#define PROB_MIN			1
#define PROB_MAX			100

/*
 * file operation probabilities
 */
#define FREAD_PROB			50
#define FREAD_MIN			PROB_MIN
#define FREAD_MAX			FREAD_PROB
#define FSTAT_PROB			50
#define FSTAT_MIN			FREAD_MAX + 1
#define FSTAT_MAX			FREAD_MAX + FSTAT_PROB
/*
 * directory operation probabilities
 */
#define DREAD_PROB			34
#define DREAD_MIN			PROB_MIN
#define DREAD_MAX			DREAD_PROB
#define DCWD_PROB			33
#define DCWD_MIN			DREAD_MAX + 1
#define DCWD_MAX			DREAD_MAX + DCWD_PROB
#define DSTAT_PROB			33
#define DSTAT_MIN			DCWD_MAX + 1
#define DSTAT_MAX			PROB_MAX
#define FREAD_LOOP			1
#define FSTAT_LOOP			1
#define DREAD_LOOP			1
#define DSTAT_LOOP			1

#define OPERATION_RATIO		50
#define OPERATION_COUNT		500

#define NAMELIST_CHUNK		256
#define READBUFLEN			1025

#define FILE_LIST			0x1
#define DIR_LIST			0x2
#define LIST_ALL			0x4

#define MINFILE				16
#define MAXFILE				4096*64

#define PROC_ENTER(str)		if ( Verbose ) { printf( "%s\n", str ); }

enum opid {
	FILE_READ,
	FILE_STAT,
	DIR_READ,
	DIR_CWD,
	DIR_STAT,
	NULLOP
};

enum confid {
	RATIO_CONFIG,
	OP_CONFIG,
	FILE_LOOP_CONFIG,
	DIR_LOOP_CONFIG,
	FILE_PROB_CONFIG,
	DIR_PROB_CONFIG,
	NULLCONF
};

struct operation {
	char		*op_name;
	enum opid	op_id;
	int			op_rmin;
	int			op_rmax;
	int			op_loop;
};

struct configuration {
	char				*cf_key;
	enum confid			cf_id;
	int					cf_val;
	char				*cf_opname;
	struct operation	*cf_optab;
	void				(*cf_func)();
};

extern int errno;
extern char *optarg;
extern int optind;

void set_oprange();
void set_oploop();

extern char *malloc();
extern char *re_comp();
extern int re_exec();

char *Progname = "exercise";
char StartDir[MAXPATHLEN + 1];
int Verbose = 0;
int Level = 0;
struct operation Fileops[] = {
	{"read", FILE_READ, FREAD_MIN, FREAD_MAX, FREAD_LOOP},
	{"stat", FILE_STAT, FSTAT_MIN, FSTAT_MAX, 1},
	{ NULL, NULLOP, 0, 0 }
};
struct operation Dirops[] = {
	{"read", DIR_READ, DREAD_MIN, DREAD_MAX, DREAD_LOOP},
	{"chdir", DIR_CWD, DCWD_MIN, DCWD_MAX, 1},
	{"stat", DIR_STAT, DSTAT_MIN, DSTAT_MAX, DSTAT_LOOP},
	{ NULL, NULLOP, 0, 0 }
};
struct configuration Configtab[] = {
	{ "RATIO", RATIO_CONFIG, OPERATION_RATIO, NULL, NULL },
	{ "OPERATIONS", OP_CONFIG, OPERATION_COUNT, NULL, NULL },
	{ "FREADPROB", FILE_PROB_CONFIG, FREAD_PROB, "read", Fileops, set_oprange },
	{ "FSTATPROB", FILE_PROB_CONFIG, FSTAT_PROB, "stat", Fileops, set_oprange },
	{ "DREADPROB", DIR_PROB_CONFIG, DREAD_PROB, "read", Dirops, set_oprange },
	{ "DCHDIRPROB", DIR_PROB_CONFIG, DCWD_PROB, "chdir", Dirops, set_oprange },
	{ "DSTATPROB", DIR_PROB_CONFIG, DSTAT_PROB, "stat", Dirops, set_oprange },
	{ "FREADLOOP", FILE_LOOP_CONFIG, FREAD_LOOP, "read", Fileops, set_oploop },
	{ "FSTATLOOP", FILE_LOOP_CONFIG, FSTAT_LOOP, "stat", Fileops, set_oploop },
	{ "DREADLOOP", DIR_LOOP_CONFIG, DREAD_LOOP, "read", Dirops, set_oploop },
	{ "DSTATLOOP", DIR_LOOP_CONFIG, DSTAT_LOOP, "stat", Dirops, set_oploop },
	{ NULL, NULLCONF, 0, NULL, NULL }
};

void
usage( void )
{
	fprintf( stderr, "usage: %s [-Vp] [dir1 dir2 ...]\n", Progname );
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

struct configuration *
lookup_config( char *str )
{
	struct configuration *config_ent = NULL;
	int i;

	if ( str ) {
		for ( i = 0; Configtab[i].cf_key; i++ ) {
			if ( !strcmp( str, Configtab[i].cf_key ) ) {
				config_ent = &Configtab[i];
				break;
			}
		}
	}
	return( config_ent );
}

struct operation *
lookup_op_byname( char *name, struct operation *table )
{
	struct operation *opent = NULL;

	if ( name && table && table->op_name ) {
		for ( ; table->op_name; table++ ) {
			if ( strcmp( name, table->op_name ) ) {
				opent = table;
				break;
			}
		}
	}
	return( opent );
}

struct operation *
lookup_op_byrange( int val, struct operation *table )
{
	struct operation *opent = NULL;

	if ( table && table->op_name ) {
		for ( ; table->op_name; table++ ) {
			if ( (table->op_rmin <= val) && (table->op_rmax >= val) ) {
				opent = table;
				break;
			}
		}
	}
	return( opent );
}

void
set_oprange( struct operation *opent, int val )
{
	struct operation *prev = opent - 1;

	if ( (opent == Fileops) || (opent == Dirops) ) {
		opent->op_rmin = PROB_MIN;
		opent->op_rmax = opent->op_rmin + val;
		while( (prev = opent++) && (opent->op_name) ) {
			opent->op_rmin = prev->op_rmax + 1;
			opent->op_rmax = opent->op_rmin + val;
		}
	} else {
		do {
			opent->op_rmin = prev->op_rmax + 1;
			opent->op_rmax = opent->op_rmin + val;
			prev = opent++;
		} while ( opent->op_name );
	}
}

void
set_oploop( struct operation *opent, int val )
{
	opent->op_loop = val;
}

void
check_tables( void )
{
	int i;
	int filesum = 0;
	int dirsum = 0;
	int errcount = 0;

	PROC_ENTER( "check_tables" );
	for ( i = 0; Configtab[i].cf_key; i++ ) {
		switch ( Configtab[i].cf_id ) {
			case FILE_PROB_CONFIG:
				filesum += Configtab[i].cf_val;
				break;
			case DIR_PROB_CONFIG:
				dirsum += Configtab[i].cf_val;
				break;
			case RATIO_CONFIG:
				if ( (Configtab[i].cf_val > PROB_MAX) ||
					(Configtab[i].cf_val < PROB_MIN) ) {
						fprintf( stderr, "check_tables: RATIO out of range\n" );
						errcount++;
				} else if ( Configtab[i].cf_val == PROB_MAX ) {
					fprintf( stderr,
						"check_tables: WARNING: RATIO == PROB_MAX\n" );
				} else if ( Configtab[i].cf_val == PROB_MIN ) {
					fprintf( stderr,
						"check_tables: WARNING: RATIO == PROB_MIN\n" );
				}
		}
	}
	if ( filesum != PROB_MAX ) {
		fprintf( stderr,
			"check_tables: file probabilities do not sum to %d\n",
			PROB_MAX );
		errcount++;
	}
	if ( dirsum != PROB_MAX ) {
		fprintf( stderr,
			"check_tables: dir probabilities do not sum to %d\n",
			PROB_MAX );
		errcount++;
	}
	for ( i = 0; Fileops[i].op_name; i++ ) {
		if ( (Fileops[i].op_rmax > PROB_MAX) ||
			(Fileops[i].op_rmin < PROB_MIN) ) {
				fprintf( stderr, "check_tables: file op %s: bad range\n",
					Fileops[i].op_name );
				errcount++;
		}
	}
	for ( i = 0; Dirops[i].op_name; i++ ) {
		if ( (Dirops[i].op_rmax > PROB_MAX) ||
			(Dirops[i].op_rmin < PROB_MIN) ) {
				fprintf( stderr, "check_tables: dir op %s: bad range\n",
					Dirops[i].op_name );
				errcount++;
		}
	}
	if ( errcount ) {
		exit( 1 );
	}
}

struct operation *
select_optab( void )
{
	struct configuration *configent;
	struct operation *optab = NULL;

	if ( configent = lookup_config( "RATIO" ) ) {
		if ( GEN_NUM( PROB_MIN, PROB_MAX ) <= configent->cf_val ) {
			optab = Fileops;
		} else {
			optab = Dirops;
		}
	} else {
		fprintf( stderr, "select_optab: unable to find RATIO configurator\n" );
	}
	return( optab );
}

struct operation *
select_op( void )
{
	struct operation *table;
	struct operation *opent = NULL;

	if ( table = select_optab() ) {
		opent = lookup_op_byrange( GEN_NUM( PROB_MIN, PROB_MAX ), table );
	} else {
		fprintf( stderr, "select_op: optab selection error\n" );
	}
	return( opent );
}

void
check_dir_list( char **lp, int cnt )
{
	char *cp;
	int spaces;

	if ( lp ) {
		while ( cnt-- ) {
			if ( cp = lp[cnt] ) {
				spaces = 0;
				while( *cp ) {
					if ( isspace(*cp) ) {
						spaces++;
					}
					cp++;
				}
				if ( spaces == strlen( lp[cnt] ) ) {
					printf( "check_dir_list: blank file name in entry %d\n",
						cnt );
				}
			} else {
				printf( "check_dir_list: NULL entry %d\n", cnt );
			}
		}
	}
}

char **
list_dir( int type, char *path, int *count )
{
	DIR *dirp;
	struct dirent *dentp;
	char **namelist = NULL;
	char *name;
	struct stat statbuf;
	char filename[MAXPATHLEN];

	if ( dirp = opendir( path ) ) {
		namelist = (char **)malloc( sizeof(char *) * NAMELIST_CHUNK );
		if ( namelist ) {
			*count = 0;
			while ( dentp = readdir( dirp ) ) {
				sprintf( filename, "%s/%s", path, dentp->d_name );
				if ( stat( filename, &statbuf ) == -1 ) {
					error( "list_dir: stat error for %s in dir %s",
						dentp->d_name, path );
				} else {
					if ( (type == LIST_ALL) ||
						((type == FILE_LIST) && S_ISREG( statbuf.st_mode )) ||
						((type == DIR_LIST) && S_ISDIR( statbuf.st_mode )) ) {
							name = malloc( strlen(dentp->d_name) + 1 );
							if ( name ) {
								namelist[*count] = name;
								(*count)++;
								strcpy( name, dentp->d_name );
							} else {
								error(
									"list_dir: unable to malloc name space" );
							}
					}
				}
			}
			if ( !*count ) {
				free( namelist );
				namelist = NULL;
			}
			if ( closedir( dirp ) == -1 ) {
				error( "list_dir: close error on dir %s", path );
			}
		} else {
			error( "list_dir: unable to malloc name list" );
		}
	} else {
		error( "list_dir: open error on dir %s", path );
	}
	if ( (Verbose > 1) && namelist ) {
		check_dir_list( namelist, *count );
	}
	return( namelist );
}

void
free_dir_list( char **list, int count )
{
	char **lp = list;

	if ( list ) {
		while ( count ) {
			if ( *lp ) {
				free( *lp );
				*lp = NULL;
			}
			count--;
			lp++;
		}
		free( list );
	}
}

char *
find_file( char *exp )
{
	char *name = NULL;
	char *errstr;
	int i;
	DIR *dirp;
	struct dirent *dentp;
	struct stat statbuf;

	PROC_ENTER( "find_file" );
	if ( errstr = re_comp( exp ) ) {
		fprintf( stderr, "find_file: re_comp: %s", errstr );
	} else if ( dirp = opendir( "." ) ) {
		while ( dentp = readdir( dirp ) ) {
			if ( stat( dentp->d_name, &statbuf ) == -1 ) {
				error( "list_dir: stat error for %s", dentp->d_name );
			} else if ( re_exec( dentp->d_name ) ) {
				name = malloc( strlen(dentp->d_name) + 1 );
				if ( name ) {
					strcpy( name, dentp->d_name );
				} else {
					error( "list_dir: unable to malloc name space" );
				}
				break;
			}
		}
		if ( closedir( dirp ) == -1 ) {
			error( "list_dir: close error on dir ." );
		}
	} else {
		error( "list_dir: open error on dir ." );
	}
	return( name );
}

/*
 * read the current directory
 * pick a regular file, but not *.DIRINFO
 * return the file name
 */
char *
select_file( void )
{
	char **flist;
	char *name = NULL;
	int count = 0;
	int n;
	char *errstr;

	if ( flist = list_dir( FILE_LIST, ".", &count ) ) {
		if ( errstr = re_comp( DIRINFO_FORM ) ) {
			fprintf( stderr, "select_file: re_comp: %s", errstr );
		} else {
			if ( count > 1 ) {
				n = GEN_NUM( 0, count-1 );
				if ( re_exec( flist[n] ) ) {
					n = (n + 1) % count;
				}
				name = flist[n];
				flist[n] = NULL;
			}
		}
		free_dir_list( flist, count );
	}
	return( name );
}

/*
 * read the current directory
 * pick a directory
 * return the file name
 */
char *
select_dir( void )
{
	char **flist;
	char *name = NULL;
	int count = 0;
	int n;

	if ( flist = list_dir( DIR_LIST, ".", &count ) ) {
		n = GEN_NUM( 0, count-1 );
		name = flist[n];
		flist[n] = NULL;
		free_dir_list( flist, count );
	}
	return( name );
}

void
do_file_read( struct operation *opent )
{
	char *name;
	int fd;
	int res;
	char buffer[READBUFLEN];
	int i;

	PROC_ENTER( "do_file_read" );
	if ( name = select_file() ) {
		if ( Verbose ) {
			printf( "do_file_read: reading file %s\n", name );
		}
		for ( i = 0; i < opent->op_loop; i++ ) {
			if ( (fd = open( name, O_RDONLY )) == -1 ) {
				error( "do_file_read: unable to open %s for reading", name );
				break;
			} else {
				while ( (res = read( fd, buffer, BUFLEN )) > 0 ) {
					;
				}
				if ( res == -1 ) {
					error( "do_file_read: read error on %s", name );
					break;
				}
				if ( close( fd ) == -1 ) {
					error( "do_file_read: close error on %s", name );
					break;
				}
			}
		}
		free( name );
	}
}

void
do_file_stat( struct operation *opent )
{
	char *name;
	int fd;
	int i;
	struct stat statbuf;

	PROC_ENTER( "do_file_stat" );
	if ( name = select_file() ) {
		if ( Verbose ) {
			printf( "do_file_stat: stating file %s\n", name );
		}
		for ( i = 0; i < opent->op_loop; i++ ) {
			switch ( GEN_NUM( 1, 3 ) ) {
				case 1:
					if ( stat( name, &statbuf ) == -1 ) {
						error( "do_file_stat: stat error on %s", name );
						return;
					}
					break;
				case 2:
					if ( lstat( name, &statbuf ) == -1 ) {
						error( "do_file_stat: lstat error on %s", name );
						return;
					}
					break;
				case 3:
					if ( (fd = open( name, O_RDONLY )) == -1 ) {
						error( "do_file_stat: open error on %s", name );
						return;
					} else if ( fstat( fd, &statbuf ) == -1 ) {
						error( "do_file_stat: fstat error on %s", name );
						return;
					} else if ( close( fd ) == -1 ) {
						error( "do_file_stat: close error on %s", name );
						return;
					}
					break;
				default:
					error( "do_file_stat: bad stat op" );
			}
		}
		free( name );
	}
}

void
do_dir_read( struct operation *opent )
{
	char *name;
	char **namelist;
	int count;
	int i;

	PROC_ENTER( "do_dir_read" );
	if ( name = select_dir() ) {
		if ( Verbose ) {
			printf( "do_dir_read: reading dir %s\n", name );
		}
		for ( i = 0; i < opent->op_loop; i++ ) {
			namelist = list_dir( LIST_ALL, name, &count );
		}
		free( name );
	}
}

void
do_chdir( struct operation *opent )
{
	char *name;
	int isdotdot;

	PROC_ENTER( "do_chdir" );
	if ( name = select_dir() ) {
		/*
		 * don't cd to .
		 */
		if ( strcmp( name, "." ) != 0 ) {
			/*
			 * if we're at the top level, don't cd ..
			 */
			isdotdot = (strcmp( name, ".." ) == 0);
			if ( (Level == 0) && !isdotdot ) {
				if ( Verbose ) {
					printf( "cd to %s\n", name );
				}
				if ( chdir( name ) == -1 ) {
					error( "do_chdir: unable to cd to %s", name );
				} else if ( isdotdot ) {
					Level--;
				} else {
					Level++;
				}
			}
		}
		free( name );
	}
}

void
do_dir_stat( struct operation *opent )
{
	char *name;
	int fd;
	struct stat statbuf;
	int i;

	if ( name = select_dir() ) {
		if ( Verbose ) {
			printf( "do_dir_stat: stating dir %s\n", name );
		}
		for ( i = 0; i < opent->op_loop; i++ ) {
			switch ( GEN_NUM( 1, 3 ) ) {
				case 1:
					if ( stat( name, &statbuf ) == -1 ) {
						error( "do_dir_stat: stat error on %s", name );
						return;
					}
					break;
				case 2:
					if ( lstat( name, &statbuf ) == -1 ) {
						error( "do_dir_stat: lstat error on %s", name );
						return;
					}
					break;
				case 3:
					if ( (fd = open( name, O_RDONLY )) == -1 ) {
						error( "do_dir_stat: open error on %s", name );
						return;
					} else if ( fstat( fd, &statbuf ) == -1 ) {
						error( "do_dir_stat: fstat error on %s", name );
						return;
					} else if ( close( fd ) == -1 ) {
						error( "do_dir_stat: close error on %s", name );
						return;
					}
					break;
				default:
					error( "do_dir_stat: bad stat op" );
			}
		}
		free( name );
	}
}

void
do_op( struct operation *opent )
{
	char *cwd = NULL;

	if ( (cwd = getcwd( NULL, MAXPATHLEN )) == NULL ) {
		perror( "do_op: getcwd" );
	} else if ( strncmp( cwd, StartDir, strlen( StartDir ) ) != 0 ) {
		fprintf( stderr, "do_op: illegal working directory: %s\n", cwd );
		exit( 1 );
	} else {
		switch ( opent->op_id ) {
			case FILE_READ:
				do_file_read( opent );
				break;
			case FILE_STAT:
				do_file_stat( opent );
				break;
			case DIR_READ:
				do_dir_read( opent );
				break;
			case DIR_CWD:
				do_chdir( opent );
				break;
			case DIR_STAT:
				do_dir_stat( opent );
				break;
			default:
				fprintf( stderr, "do_op: bad op entry: %s\n", opent->op_name );
		}
	}
}

void
traverse_tree( char *name )
{
	int opcount;
	struct configuration *configent;
	struct operation *opent;
	int lev;
	int ignore_err = 0;
	int files_checked = 0;
	int dirs_checked = 0;
	int toterror = 0;
	char *dirinfo;

	PROC_ENTER( "traverse_tree" );
	mtestdir( name );
	if ( getwd( StartDir ) == NULL ) {
		fprintf( stderr, "traverse_tree: getwd: %s\n", StartDir );
		exit( 1 );
	} else {
		if ( configent = lookup_config( "OPERATIONS" ) ) {
			opcount = configent->cf_val;
			while ( opcount-- ) {
				if ( opent = select_op() ) {
					do_op( opent );
				} else {
					fprintf( stderr,
						"traverse_tree: operation selection error\n" );
					exit( 1 );
				}
			}
		} else {
			fprintf( stderr,
			"traverse_tree: unable to find operation count configurator\n" );
			exit( 1 );
		}
	}
}

void
get_config( void )
{
	char *path;
	FILE *fp;
	char config[256];
	struct configuration *configent;
	struct operation *opent;
	char *tok;
	char *getenv();
	FILE *fopen();

	PROC_ENTER( "get_config" );
	if ( (path = getenv("NFSTESTCONF")) == NULL ) {
		path = CONFIGPATH;
	}
	if ( fp = fopen( path, "r" ) ) {
		if ( Verbose ) {
			printf( "Configuration:\n " );
		}
		while ( fgets( config, 256, fp ) ) {
			tok = strtok( config, " =" );
			if ( !(configent = lookup_config( tok )) ) {
				fprintf( stderr, "get_config: %s: invalid configurator\n",
					config );
			} else {
				if ( Verbose ) {
					printf( "\t%s = ", tok );
				}
				tok = strtok( NULL, " " );
				if ( !tok ) {
					fprintf( stderr, "get_config: %s: invalid configurator\n",
						config );
				} else {
					configent->cf_val = atoi( tok );
					if ( Verbose ) {
						printf( "%d", configent->cf_val );
					}
					if ( configent->cf_opname ) {
						opent = lookup_op_byname( configent->cf_opname,
							configent->cf_optab );
						if ( !opent ) {
							fprintf( stderr,
								"get_config: %s: invalid operation\n",
								configent->cf_opname );
						} else {
							(*configent->cf_func)( opent, configent->cf_val );
						}
					}
				}
				if ( Verbose ) {
					printf( "\n" );
				}
			}
		}
		(void)fclose( fp );
	}
	check_tables();
}

main( int argc, char **argv )
{
	int opt;
	int opterr = 0;
	int parallel = 0;
	int children = 0;
	int pid;
	int use_current_dir = 0;
	int i;
	int files_checked = 0;
	int dirs_checked = 0;
	int toterror = 0;

	Progname = *argv;
	while ( (opt = getopt( argc, argv, OPTSTR )) != -1 ) {
		switch ( opt ) {
			case 'p':
				parallel = 1;
				break;
			case 'V':
				Verbose++;
				break;
			deafult:
				opterr++;
		}
	}
	if ( opterr ) {
		usage();
		exit( 1 );
	}
	initialize();
	get_config();
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
	}
	if ( parallel ) {
		for ( i = optind; i < argc; i++ ) {
			if ( (pid = fork()) == -1 ) {
				error( "main: fork error" );
			} else if ( pid ) {
				children++;
			} else {
				traverse_tree( argv[i] );
				exit( 0 );
			}
		}
		/*
		 * wait for all processes to finish
		 */
		while ( wait( NULL ) != -1 ) {
			;
		}
	} else {
		for ( i = optind; i < argc; i++ ) {
			traverse_tree( argv[i] );
		}
	}
	return( 0 );
}
