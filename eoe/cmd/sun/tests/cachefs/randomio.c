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

#define OPTSTR				"MVvbtorwml:O:S:c:"
#define DEFAULT_MODE		MIXED
#define DEFAULT_REPEAT		-1
#define DEFAULT_FILE		"randomio_testfile"
#define DEFAULT_OPENCLOSE	0
#define DEFAULT_TRUNCATE	0
#define DEFAULT_MAXOFF		64 * 1024
#define DEFAULT_MAXSIZE		64 * 1024
#define DEFAULT_BUILD		0
#define DEFAULT_LOCKING		0
#define DEFAULT_MMAP		0
#define CREATE_MODE			0666

enum iomode { READ, WRITE, MIXED };

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
	enum iomode mode,
	off_t max_offset,
	int max_size,
	char *buf,
	int locking,
	int lockmode,
	int memorymap )
{
	off_t offset;
	off_t seek_off;
	int len;
	int status = 0;
	int n;
	struct stat sb;
	struct flock fl;

	if ( fstat( fd, &sb ) == -1 ) {
		if ( errno != EINTR ) {
			perror( "do_io fstat" );
		}
		return( errno );
	}
	if ( mode == MIXED ) {
		mode = ((int)(drand48() * 2.0)) ? READ : WRITE;
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
	if ( memorymap ) {
		/*
		 * if memory mapping, buf is the memory map address for the file
		 * seek by adding offset to buf
		 */
		buf += offset;
	} else if ( (seek_off = lseek( fd, offset, SEEK_SET )) == -1 ) {
		if ( errno != EINTR ) {
			perror( "do_io lseek" );
		}
		return( errno );
	} else if ( seek_off != offset ) {
		fprintf( stderr, "do_io: lseek returned 0x%x, expected 0x%x\n",
			(int)seek_off, (int)offset );
		return( -1 );
	}
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
			if ( memorymap ) {
				if ( !check_buffer( buf, offset, len ) ) {
					fprintf( stderr, "do_io: invalid buffer contents\n" );
					status = -1;
				}
			} else if ( (n = read( fd, buf, len )) == -1 ) {
				status = errno;
				if ( errno != EINTR ) {
					perror( "do_io read" );
				}
			} else if ( n != len ) {
				fprintf( stderr, "do_io: bad read, %d of %d bytes\n",
					(int)n, (int)len );
				status = -1;
			} else if ( !check_buffer( buf, offset, len ) ) {
				fprintf( stderr, "do_io: invalid buffer contents\n" );
				status = -1;
			}
			break;
		case WRITE:
			if ( Verbose ) {
				printf( "writing offset %d, len %d\n", (int)offset, len );
			}
			fill_buffer( buf, offset, len );
			if ( memorymap ) {
				if ( Verify && !check_buffer( buf, offset, len ) ) {
					fprintf( stderr, "do_io: invalid buffer contents\n" );
					status = -1;
				}
			} else if ( (n = write( fd, buf, len )) == -1 ) {
				status = errno;
				if ( errno != EINTR ) {
					perror( "do_io write" );
				}
			} else if ( n != len ) {
				fprintf( stderr, "do_io: bad write, %d of %d bytes\n",
					(int)n, (int)len );
				status = -1;
			} else if ( Verify ) {
				if ( (seek_off = lseek( fd, offset, SEEK_SET )) == -1 ) {
					if ( errno != EINTR ) {
						perror( "do_io lseek" );
					}
					status = errno;
				} else if ( seek_off != offset ) {
					fprintf( stderr,
						"do_io: lseek returned 0x%x, expected 0x%x\n",
						(int)seek_off, (int)offset );
					status = -1;
				} else if ( (n = read( fd, buf, len )) == -1 ) {
					status = errno;
					if ( errno != EINTR ) {
						perror( "do_io read" );
					}
				} else if ( n != len ) {
					fprintf( stderr, "do_io: bad read, %d of %d bytes\n",
						(int)n, (int)len );
					status = -1;
				} else if ( !check_buffer( buf, offset, len ) ) {
					fprintf( stderr, "do_io: invalid buffer contents\n" );
					status = -1;
				}
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
	int max_offset = DEFAULT_MAXOFF;
	int max_size = DEFAULT_MAXSIZE;
	char *buf;
	int build = DEFAULT_BUILD;
	int locking = DEFAULT_LOCKING;
	int lockmode = 0;
	int memorymap = DEFAULT_MMAP;

	signal( SIGINT, sigint_handler );
	while ( (opt = getopt( argc, argv, OPTSTR )) != EOF ) {
		switch ( opt ) {
			case 'V':	/* verify writes */
				Verify = 1;
				break;
			case 'M':	/* mmap file */
				memorymap = 1;
				break;
			case 'o':
				openclose = 1;
				break;
			case 'r':
				mode = READ;
				modeset++;
				break;
			case 'w':
				mode = WRITE;
				modeset++;
				break;
			case 'm':
				mode = MIXED;
				modeset++;
				break;
			case 'c':
				repeat = atoi( optarg );
				break;
			case 't':
				truncate = 1;
				break;
			case 'O':
				max_offset = atoi( optarg );
				break;
			case 'S':
				max_size = atoi( optarg );
				break;
			case 'b':
				build = 1;
				break;
			case 'l':
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
			case 'v':
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
	if ( !memorymap && !(buf = malloc( max_size )) ) {
		fprintf( stderr, "out of memory\n" );
		if ( errno ) {
			perror( "malloc" );
			status = errno;
		} else {
			status = -1;
		}
		exit( status );
	}
	if ( !openclose ) {
		if ( (fd = open( filename, openflags, CREATE_MODE )) == -1 ) {
				if ( errno != EINTR ) {
					perror( filename );
				}
				exit( errno );
		} else if ( memorymap && !(buf = (char *)mmap( NULL,
			max_offset + max_size,
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
				} else if ( memorymap && !(buf = (char *)mmap( NULL,
					max_offset + max_size,
					(mode == READ) ? PROT_READ : PROT_READ | PROT_WRITE,
					MAP_SHARED | MAP_AUTOGROW, fd, (off_t)0 )) ) {
						if ( errno != EINTR ) {
							perror( "mmap" );
						}
						status = errno;
						break;
				}
			}
			status = do_io( fd, mode, max_offset, max_size, buf, locking,
				lockmode, memorymap );
			if ( openclose ) {
				if ( memorymap ) {
					munmap( (void *)buf, max_offset + max_size );
					buf = NULL;
				}
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
				} else if ( memorymap && !(buf = (char *)mmap( NULL,
					max_offset + max_size,
					(mode == READ) ? PROT_READ : PROT_READ | PROT_WRITE,
					MAP_SHARED | MAP_AUTOGROW, fd, (off_t)0 )) ) {
						if ( errno != EINTR ) {
							perror( "mmap" );
						}
						status = errno;
						break;
				}
			}
			status = do_io( fd, mode, max_offset, max_size, buf, locking,
				lockmode, memorymap );
			if ( openclose ) {
				if ( memorymap ) {
					munmap( (void *)buf, max_offset + max_size );
					buf = NULL;
				}
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
