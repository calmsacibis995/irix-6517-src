#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/mman.h>

#define OPTSTR				"Vvbtorwml:O:S:c:a:i:"
#define DEFAULT_MODE		MIXED
#define DEFAULT_REPEAT		-1
#define DEFAULT_FILE		"mmap_testfile"
#define DEFAULT_OPENCLOSE	0
#define DEFAULT_TRUNCATE	0
#define DEFAULT_MAXOFF		64 * 1024
#define DEFAULT_MAXSIZE		64 * 1024
#define DEFAULT_IOLEN		1024
#define DEFAULT_BUILD		0
#define DEFAULT_LOCKING		0
#define DEFAULT_ACCESS		SEQUENTIAL
#define CREATE_MODE			0666

enum iomode { READ, WRITE, MIXED };

enum accessmode { SEQUENTIAL, RANDOM };

int Quitsig = 0;
int Verbose = 0;
int Verify = 0;

extern char *optarg;
extern int optind;
extern int errno;

void
sigint_handler()
{
	Quitsig = 1;
}

void
usage( char *progname )
{
	char *op = OPTSTR;

	fprintf( stderr, "usage: %s", progname );
	while ( *op ) {
		if ( *(op + 1) == ':' ) {
			fprintf( stderr, " [-%c arg]", *op );
			op++;
		} else {
			fprintf( stderr, " [-%c]", *op );
		}
		op++;
	}
	fprintf( stderr, " [file]\n" );
}

char *
iomode_to_str( enum iomode mode )
{
	char *str;

	switch ( mode ) {
		case READ:
			str = "READ";
			break;
		case WRITE:
			str = "WRITE";
			break;
		case MIXED:
			str = "MIXED";
			break;
		default:
			str = "INVALID MODE";
	}
	return( str );
}

int
iomode_to_openflags( enum iomode mode )
{
	int openflags = 0;

	switch ( mode ) {
		case READ:
			openflags  = O_RDONLY;
			break;
		case WRITE:
			openflags = O_WRONLY;
			break;
		case MIXED:
			openflags = O_RDWR;
			break;
		default:
			fprintf( stderr, "iomode_to_openflags: bad I/O mode 0x%x\n",
				(int)mode );
			exit( -1 );
	}
	return( openflags );
}

void
fill_buffer( char *buf, off_t offset, int len )
{
	unsigned c = offset % 256;

	while ( len-- ) {
		*buf = c;
		c = (c + 1) % 256;
		buf++;
	}
}

int
check_buffer( char *buf, off_t offset, int len )
{
	unsigned c = offset % 256;
	int status = 1;
	int i;

	for ( i = 0; (i < len) && (*buf == c); c = (c + 1) % 256, i++, buf++ )
		;
	if ( i < len ) {
		status = 0;
		fprintf( stderr,
			"buffer contents error, offset %d: got 0x%x expected 0x%x\n",
			offset + i, (unsigned)*buf, c );
	}
	return( status );
}

int
do_io(
	int fd,
	enum accessmode access,
	enum iomode mode,
	off_t max_offset,
	int max_size,
	char *buf,
	int locking,
	int lockmode)
{
	off_t offset;
	off_t seek_off;
	int len;
	int status = 0;
	int n;
	struct stat sb;
	struct flock fl;

	if ( mode == MIXED ) {
		mode = ((int)(drand48() * 2.0)) ? READ : WRITE;
	}
	switch ( access ) {
		case RANDOM:
			if ( fstat( fd, &sb ) == -1 ) {
				if ( errno != EINTR ) {
					perror( "do_io fstat" );
				}
				return( errno );
			}
			len = (int)(drand48() * (double)(max_size + 1));
			if ( mode == READ ) {
				if ( sb.st_size == 0 ) {
					return( 0 );
				}
				max_offset = sb.st_size - len;
				if ( max_offset < 0 ) {
					max_offset = 0;
					len = (int)(drand48() * (double)(sb.st_size + 1));
				}
			}
			offset = (off_t)(drand48() * (double)(max_offset + 1));
			break;
		case SEQUENTIAL:
			offset = max_offset;
			len = max_size;
			break;
		default:
			fprintf( stderr, "do_io: bad access mode 0x%x\n", (int)access );
			return( -1 );
	}
	/*
	 * buf is the memory map address for the file
	 * seek by adding offset to buf
	 */
	buf += offset;
	if ( locking ) {
		if ( Verbose ) {
			printf( "locking offset %d, len %d, mode %s\n", (int)offset,
				len, (lockmode == F_RDLCK) ? "F_RDLCK" : "F_WRLCK" );
		}
		fl.l_type = lockmode;
		fl.l_whence = SEEK_SET;
		fl.l_start = offset;
		fl.l_len = len;
		if ( fcntl( fd, F_SETLKW, &fl ) == -1 ) {
			status = errno;
			if ( errno != EINTR ) {
				perror( "do_io: fcntl(UNLOCK)" );
			}
			return( status );
		}
	}
	switch ( mode ) {
		case READ:
			if ( Verbose ) {
				printf( "reading offset %d, len %d\n", (int)offset, len );
			}
			if ( !check_buffer( buf, offset, len ) ) {
				fprintf( stderr, "do_io: invalid buffer contents\n" );
				status = -1;
			}
			break;
		case WRITE:
			if ( Verbose ) {
				printf( "writing offset %d, len %d\n", (int)offset, len );
			}
			fill_buffer( buf, offset, len );
			if ( Verify && !check_buffer( buf, offset, len ) ) {
				fprintf( stderr, "do_io: invalid buffer contents\n" );
				status = -1;
			}
			break;
		default:
			fprintf( stderr, "do_io: Invalid mode\n" );
			status = -1;
	}
	if ( locking ) {
		if ( Verbose ) {
			printf( "unlocking offset %d, len %d\n", (int)offset, len );
		}
		fl.l_type = F_UNLCK;
		fl.l_whence = SEEK_SET;
		fl.l_start = offset;
		fl.l_len = len;
		if ( fcntl( fd, F_SETLKW, &fl ) == -1 ) {
			if ( !status ) {
				status = errno;
			}
			if ( errno != EINTR ) {
				perror( "do_io: fcntl(UNLOCK)" );
			}
		}
	}
	return( status );
}

int
build_file( char *filename, int size )
{
	char buf[8192];
	int status = 0;
	off_t offset;
	int resid;
	int n;
	int fd;

	if ( (fd = open( filename, O_WRONLY | O_CREAT | O_TRUNC, CREATE_MODE ))
		== -1 ) {
			if ( errno != EINTR ) {
				perror( filename );
			}
			status = errno;
	} else {
		for ( offset = 0; offset < size; offset += sizeof(buf) ) {
			fill_buffer( buf, offset, sizeof(buf) );
			for ( resid = sizeof(buf); (status == 0) && (resid > 0); ) {
				if ( (n = write( fd, buf, resid )) == -1 ) {
					if ( errno != EINTR ) {
						perror( "build_file write" );
					}
					status = errno;
					break;
				} else if ( n == 0 ) {
					fprintf( stderr, "build_file: 0-byte write\n" );
					status = -1;
					break;
				} else {
					resid -= n;
				}
			}
		}
		close( fd );
	}
	return( status );
}

char *
accessmode_to_str( enum accessmode access )
{
	static char str[16];

	switch ( access ) {
		case RANDOM:
			strcpy( str, "RANDOM" );
			break;
		case SEQUENTIAL:
			strcpy( str, "SEQUENTIAL" );
			break;
		default:
			strcpy( str, "BAD ACCESS MODE" );
	}
	return( str );
}

enum accessmode
str_to_accessmode( char *str )
{
	enum accessmode access;

	if ( strcasecmp( str, "RANDOM" ) == 0 ) {
		access = RANDOM;
	} else if ( strcasecmp( str, "SEQUENTIAL" ) == 0 ) {
		access = SEQUENTIAL;
	} else {
		access = DEFAULT_ACCESS;
		fprintf( stderr, "Invalid access mode %s, defaulting to %s\n",
			str, accessmode_to_str( access ) );
	}
	return( access );
}

main( int argc, char **argv )
{
	char *progname = *argv;
	char *filename = DEFAULT_FILE;
	enum iomode mode = DEFAULT_MODE;
	int repeat = DEFAULT_REPEAT;
	int opt;
	int modeset = 0;
	int status = 0;
	int argerror = 0;
	int openclose = DEFAULT_OPENCLOSE;
	int fd = -1;
	int i;
	int openflags = O_CREAT;
	int truncate = DEFAULT_TRUNCATE;
	off_t max_offset = DEFAULT_MAXOFF;
	int max_size = DEFAULT_MAXSIZE;
	char *buf;
	int build = DEFAULT_BUILD;
	int locking = DEFAULT_LOCKING;
	int lockmode = 0;
	enum accessmode access = DEFAULT_ACCESS;
	off_t offset;
	int size;
	int iolen = DEFAULT_IOLEN;

	signal( SIGINT, sigint_handler );
	while ( (opt = getopt( argc, argv, OPTSTR )) != EOF ) {
		switch ( opt ) {
			case 'i':
				iolen = atoi( optarg );
				break;
			case 'a':	/* do sequential I/O */
				access = str_to_accessmode( optarg );
				break;
			case 'V':	/* verify writes */
				Verify = 1;
				break;
			case 'o':	/* open/close on each I/O pass */
				openclose = 1;
				break;
			case 'r':	/* read only */
				mode = READ;
				modeset++;
				break;
			case 'w':	/* write only */
				mode = WRITE;
				modeset++;
				break;
			case 'm':	/* read and write */
				mode = MIXED;
				modeset++;
				break;
			case 'c':	/* set the repeat count */
				repeat = atoi( optarg );
				break;
			case 't':	/* truncate the file on opening */
				truncate = 1;
				break;
			case 'O':	/* set the maximum file offset */
				max_offset = atoi( optarg );
				break;
			case 'S':	/* set the maximum I/O size */
				max_size = atoi( optarg );
				break;
			case 'b':	/* build the file */
				build = 1;
				break;
			case 'l':	/* do file and record locking */
				locking = 1;
				if ( strncmp( optarg, "shar", 4 ) == 0 ) {
					lockmode = F_RDLCK;
				} else if ( strncmp( optarg, "excl", 4 ) == 0 ) {
					lockmode = F_WRLCK;
				} else {
					fprintf( stderr, "Invalid locking mode %s\n", optarg );
					fprintf( stderr,
						"acceptable locking modes are shared and exclusive\n" );
					argerror++;
				}
				break;
			case 'v': /* be verbose about operations */
				Verbose = 1;
				break;
			default:
				usage( progname );
				argerror++;
		}
	}
	if ( argerror ) {
		exit( -1 );
	}
	if ( modeset > 1 ) {
		fprintf( stderr, "Warning: multiple I/O mode settings\n" );
		fprintf( stderr, "Final I/O mode is %s\n", iomode_to_str( mode ) );
	}
	if ( optind < argc ) {
		filename = argv[optind];
	}
	if ( build ) {
		status = build_file( filename, max_offset + max_size );
		exit( status );
	}
	openflags |= iomode_to_openflags( Verify ? MIXED : mode );
	if ( truncate ) {
		openflags |= O_TRUNC;
	}
	srand48( (long)time( NULL ) );
	switch ( access ) {
		case RANDOM:
			offset = max_offset;
			size = max_size;
			break;
		case SEQUENTIAL:
			offset = 0;
			size = iolen;
			break;
		default:
			fprintf( stderr, "Invalid access mode 0x%x\n", access );
			exit( -1 );
	}
	if ( !openclose ) {
		if ( (fd = open( filename, openflags, CREATE_MODE )) == -1 ) {
				if ( errno != EINTR ) {
					perror( filename );
				}
				exit( errno );
		} else if ( !(buf = (char *)mmap( NULL, max_offset + max_size,
			(mode == READ) ? PROT_READ : PROT_READ | PROT_WRITE,
			MAP_SHARED | MAP_AUTOGROW, fd, (off_t)0 )) ) {
				if ( errno != EINTR ) {
					perror( "mmap" );
				}
				exit( errno );
		}
	}
	if ( repeat < 0 ) {
		while ( !Quitsig && !status ) {
			if ( openclose ) {
				if ( (fd = open( filename, openflags, CREATE_MODE )) == -1 ) {
					if ( errno != EINTR ) {
						perror( filename );
					}
					status = errno;
					break;
				} else if ( !(buf = (char *)mmap( NULL, max_offset + max_size,
					(mode == READ) ? PROT_READ : PROT_READ | PROT_WRITE,
					MAP_SHARED | MAP_AUTOGROW, fd, (off_t)0 )) ) {
						if ( errno != EINTR ) {
							perror( "mmap" );
						}
						status = errno;
						break;
				}
			}
			status = do_io( fd, access, mode, offset, size, buf, locking,
				lockmode );
			switch ( access ) {
				case RANDOM:
					break;
				case SEQUENTIAL:
					offset = (offset + size) % (max_offset + max_size);
					break;
				default:
					fprintf( stderr, "Invalid access mode 0x%x\n", access );
					exit( -1 );
			}
			if ( openclose ) {
				munmap( (void *)buf, max_offset + max_size );
				buf = NULL;
				close( fd );
				fd = -1;
			}
		}
	} else {
		for ( i = 1; (i <= repeat) && !Quitsig && !status; i++ ) {
			if ( openclose ) {
				if ( (fd = open( filename, openflags, CREATE_MODE )) == -1 ) {
					if ( errno != EINTR ) {
						perror( filename );
					}
					status = errno;
					break;
				} else if ( !(buf = (char *)mmap( NULL, max_offset + max_size,
					(mode == READ) ? PROT_READ : PROT_READ | PROT_WRITE,
					MAP_SHARED | MAP_AUTOGROW, fd, (off_t)0 )) ) {
						if ( errno != EINTR ) {
							perror( "mmap" );
						}
						status = errno;
						break;
				}
			}
			status = do_io( fd, access, mode, offset, size, buf, locking,
				lockmode );
			switch ( access ) {
				case RANDOM:
					break;
				case SEQUENTIAL:
					offset = (offset + size) % (max_offset + max_size);
					break;
				default:
					fprintf( stderr, "Invalid access mode 0x%x\n", access );
					exit( -1 );
			}
			if ( openclose ) {
				munmap( (void *)buf, max_offset + max_size );
				buf = NULL;
				close( fd );
				fd = -1;
			}
		}
	}
	if ( fd > 0 ) {
		close( fd );
	}
	exit( status );
}
