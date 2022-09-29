/*
 * file operations:
 *
 *		open
 *		close
 *		lock
 *		unlock
 *		check for locks
 *		verify lock
 */
#include <sys/param.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <string.h>
#if !SVR4
#include <strings.h>
#endif /* !SVR4 */
#include <stdlib.h>
#include <assert.h>
#include <rpc/rpc.h>
#include "lk_test.h"
#include "print.h"
#include "util.h"
#include "fileops.h"

#define IOBUFLEN		1024
#define HASHSIZE		11

#define MATCH_LOCK_TYPE( lp, type )	((lp)->ld_type == (type))

struct filerec {
	int				fr_fd;
	int				fr_hash;
	char			fr_name[MAXPATHLEN];
	struct filerec	*fr_next;
	struct filerec	*fr_prev;
};

#if SVR4
#define bzero(s,n)		(void)memset(s,0,n)
#endif /* SVR4 */

static struct filerec *filehash[HASHSIZE];
static int Fileinit_done = 0;

extern int errno;

extern char *Progname;
extern int Verbose;
extern int AlternateVerify;

/*
 * File open/close functions
 */

int
open_file_init(void)
{
	if ( !Fileinit_done ) {
		bzero( filehash, HASHSIZE*sizeof(struct filerec *) );
		Fileinit_done = 1;
	}
}

static int
hash( char *strp )
{
	int hv = 0;

	assert( valid_addresses( (caddr_t)strp, (int)1 ) );
	while ( *strp++ ) {
		hv ^= *strp;
	}
	return( hv % HASHSIZE );
}

static struct filerec *
hash_search( pathstr file_name )
{
	struct filerec *frp = NULL;

	if ( Verbose > 2 ) {
		(void)printf( "%s: hash_search: file %s\n", Progname, file_name );
		(void)fflush( stdout );
	}
	assert( valid_addresses( (caddr_t)file_name, 1 ) );
	for ( frp = filehash[ hash( file_name ) ]; frp; frp = frp->fr_next ) {
		if ( strcmp( file_name, frp->fr_name ) == 0 ) {
			break;
		}
	}
	if ( Verbose > 2 ) {
		if ( frp ) {
			(void)printf( "%s: hash_search: success\n", Progname );
		} else {
			(void)printf( "%s: hash_search: failed for %s\n", Progname,
				file_name );
		}
		(void)fflush( stdout );
	}
	return( frp );
}

static void
hash_insert( struct filerec *frp )
{
	int hv = hash( frp->fr_name );
	struct filerec *hp = filehash[hv];

	assert( valid_addresses( (caddr_t)frp, sizeof(struct filerec) ) );
	if ( Verbose > 2 ) {
		(void)printf( "%s: hash_insert: file %s, fd %d, hash %d\n",
			Progname, frp->fr_name, frp->fr_fd, hv );
		(void)fflush( stdout );
	}
	frp->fr_prev = NULL;
	frp->fr_hash = hv;
	if ( hp ) {
		frp->fr_next = hp;
		hp->fr_prev = frp;
		filehash[hv] = frp;
	} else {
		frp->fr_next = NULL;
		filehash[hv] = frp;
	}
}

static void
hash_remove( struct filerec *frp )
{
	struct filerec *tfrp = filehash[frp->fr_hash];

	assert( valid_addresses( (caddr_t)frp, sizeof(struct filerec) ) );
	if ( Verbose > 2 ) {
		(void)printf( "%s: hash_remove: file %s, fd %d, hash %d\n",
			Progname, frp->fr_name, frp->fr_fd, frp->fr_hash );
		(void)fflush( stdout );
	}
	if ( frp == tfrp ) {
		filehash[frp->fr_hash] = frp->fr_next;
		if ( frp->fr_next ) {
			frp->fr_next->fr_prev = NULL;
		}
	} else {
		if ( frp->fr_next ) {
			frp->fr_next->fr_prev = frp->fr_prev;
		}
		if ( frp->fr_prev ) {
			frp->fr_prev->fr_next = frp->fr_next;
		}
	}
}

int
open_file( pathstr file_name, int flags, int mode )
{
	int fd = -1;
	struct filerec *frp;

	if ( Verbose > 1 ) {
		printf( "%s: open_file( %s, 0x%x )\n", Progname, file_name, flags );
	}
	assert( Fileinit_done );
	assert( valid_addresses( (caddr_t)file_name, 1 ) );
	if ( frp = hash_search( file_name ) ) {
		fd = frp->fr_fd;
	} else if ( (fd = open( file_name, flags, mode )) == -1 ) {
		perror( "open_file: open" );
	} else if ( frp = (struct filerec *)malloc( sizeof(struct filerec) ) ) {
		frp->fr_fd = fd;
		strcpy( frp->fr_name, file_name );
		hash_insert( frp );
	} else {
		perror( "open_file: malloc" );
		close( fd );
		fd = -1;
	}
	if ( Verbose > 1 ) {
		printf( "%s: open_file: return %d\n", Progname, fd );
	}
	return( fd );
}

int
close_file( pathstr file_name )
{
	int status = 0;
	struct filerec *frp;

	if ( Verbose > 1 ) {
		printf( "%s: close_file( %s )\n", Progname, file_name );
	}
	assert( Fileinit_done );
	assert( valid_addresses( (caddr_t)file_name, 1 ) );
	if ( frp = hash_search( file_name ) ) {
		(void)close( frp->fr_fd );
		hash_remove( frp );
		free( frp );
	} else {
		(void)fprintf( stderr, "%s: close_file: %s not found in hash\n",
			Progname, file_name );
		status = -1;
	}
	if ( Verbose > 1 ) {
		printf( "%s: close_file: return %d\n", Progname, status );
	}
	return( status );
}

/*
 * close all open files
 */
void
closeall(void)
{
	struct filerec *frp;
	int hv;

	for ( hv = 0; hv < HASHSIZE; hv++ ) {
		while ( frp = filehash[hv] ) {
			(void)close( frp->fr_fd );
			hash_remove( frp );
			free( frp );
		}
	}
}

bool_t
locks_held( pathstr file_name )
{
	int status = 1;
	int fd = -1;
	struct flock fl;

	assert( valid_addresses( (caddr_t)file_name, 1 ) );
	if ( Verbose > 1 ) {
		(void)printf("%s: locks_held\n", Progname);
	}
	if ( (fd = open_file( file_name, OPEN_FLAGS, DEFAULT_MODE )) == -1 ) {
		(void)fprintf( stderr, "%s: locks_held: unable to open %s\n",
			Progname, file_name );
	} else if (AlternateVerify) {
		fl.l_type = F_WRLCK;
		fl.l_whence = SEEK_SET;
		fl.l_start = 0;
		fl.l_len = 0;
		if ( fcntl( fd, F_SETLK, &fl ) ) {
			switch (errno) {
				case EAGAIN:
				case EACCES:
					status = 1;
					break;
				default:
					perror( "locks_held: fcntl" );
					status = 0;
			}
		} else {
			status = 0;
			fl.l_type = F_UNLCK;
			if ( fcntl( fd, F_SETLK, &fl) ) {
				perror( "locks_held: fcntl" );
			}
		}
	} else {
		fl.l_type = F_WRLCK;
		fl.l_whence = SEEK_SET;
		fl.l_start = 0;
		fl.l_len = 0;
		if ( fcntl( fd, F_GETLK, &fl ) ) {
			perror( "locks_held: fcntl" );
			status = 0;
		} else {
			status = (fl.l_type != F_UNLCK);
		}
	}
	if ( Verbose > 1 ) {
		printf( "%s: locks_held: return %d\n", Progname, status );
	}
	return( status );
}

static int
alternate_verify_fcntl(struct verifyargs *vap, fcntlargs *fargs,
	off_t region_start, off_t region_len, off_t filesize)
{
	int status = 1;
	int error;
	int flags;
	
	if ( Verbose > 1 ) {
		(void)printf("%s: alternate_verify_fcntl\n", Progname);
	}
	/*
	 * If the locked region does not start at offset 0, a lock preceding
	 * the existing one can be acquired.
	 */
	if (region_start != 0) {
		/*
		 * verify that we can lock everything up to the start of the
		 * existing lock
		 */
		fargs->fa_lock.ld_offset = 0;
		fargs->fa_lock.ld_len = region_start;
		error = local_fcntl( fargs );
		switch ( error ) {
			case 0:			/* expected case */
				fargs->fa_lock.ld_type = F_UNLCK;
				if ( error = local_fcntl( fargs ) ) {
					errno = error;
					perror( "alternate_verify_fcntl: local_fcntl(F_UNLCK)" );
					return( 0 );
				}
				break;
			case EACCES:
			case EAGAIN:
				if ( Verbose ) {
					printf( "%s: alternate_verify_fcntl: unable to "
						"acquire preceding lock\n", Progname );
				}
				return(0);
			default:
				errno = error;
				perror( "alternate_verify_fcntl: local_fcntl" );
				return( 0 );
		}
	}
	fargs->fa_lock = vap->va_lock;
	/*
	 * If the locked region does not extend to the end of the file,
	 * a lock following the existing one can be acquired.
	 */
	if (region_len != filesize) {
		/*
		 * verify that we can lock everything from the end of the
		 * existing lock to the ed of the file
		 */
		fargs->fa_lock.ld_offset = region_start + region_len;
		fargs->fa_lock.ld_len = 0;
		error = local_fcntl( fargs );
		switch ( error ) {
			case 0:			/* expected case */
				fargs->fa_lock.ld_type = F_UNLCK;
				if ( error = local_fcntl( fargs ) ) {
					errno = error;
					perror( "alternate_verify_fcntl: local_fcntl(F_UNLCK)" );
					return( 0 );
				}
				break;
			case EACCES:
			case EAGAIN:
				status = 0;
				if ( Verbose ) {
					printf( "%s: alternate_verify_fcntl: unable to "
						"acquire following lock\n", Progname );
				}
				break;
			default:
				errno = error;
				perror( "alternate_verify_fcntl: local_fcntl" );
				return( 0 );
		}
	}
	if ( Verbose > 1 ) {
		printf( "%s: alternate_verify_fcntl: return %d\n", Progname, status );
	}
	return(status);
}

static int
verify_fcntl(struct verifyargs *vap)
{
	int status = 1;
	struct stat statbuf;
	int error;
	int flags;
	off_t region_start;
	off_t region_len;
	fcntlargs fargs;

	/*
	 * stat the file to get its size
	 */
	if ( stat( vap->va_name, &statbuf ) == -1 ) {
		fprintf( stderr, "%s: verify_fcntl: unable to stat file: ",
			Progname );
		perror( vap->va_name );
		return( 0 );
	}
	region_start = vap->va_lock.ld_offset;
	region_len = vap->va_lock.ld_len ? vap->va_lock.ld_len :
		statbuf.st_size - vap->va_lock.ld_offset;
	fargs.fa_name = vap->va_name;
	fargs.fa_lock = vap->va_lock;
	fargs.fa_lock.ld_type = F_WRLCK;
	if (AlternateVerify) {
		fargs.fa_cmd = F_SETLK;
		if (!alternate_verify_fcntl(vap, &fargs, region_start, region_len,
			statbuf.st_size)) {
				return(0);
		}
	} else {
		/*
		 * first, verify that the lock is set as expected
		 * use fcntl to do this
		 */
		fargs.fa_cmd = F_GETLK;
		if ( error = local_fcntl( &fargs ) ) {
			errno = error;
			perror( "verify_fcntl: local_fcntl" );
			return( 0 );
		} else if ( (fargs.fa_lock.ld_offset != region_start) ||
			(fargs.fa_lock.ld_len != vap->va_lock.ld_len) ||
			!MATCH_LOCK_TYPE( &fargs.fa_lock, vap->va_lock.ld_type ) ) {
				if ( Verbose ) {
					printf( "%s: verify_fcntl: lock mismatch\n", Progname );
					printf( "%s: verify_fcntl: offset = 0x%x(expected 0x%x)\n",
						Progname, fargs.fa_lock.ld_offset, region_start );
					printf( "%s: verify_fcntl: len = %d(expected %d)\n",
						Progname, fargs.fa_lock.ld_len, vap->va_lock.ld_len );
					printf( "%s: verify_fcntl: type = %s(expected %s)\n",
						Progname, locktype_to_str( fargs.fa_lock.ld_type ), 
						locktype_to_str( vap->va_lock.ld_type ) );
				}
				return( 0 );
		}
		/*
		 * it is assumed that the lock being verified is the only lock
		 * verify that this is the case
		 * first, try to lock the entire file.  fcntl should return an flock
		 * structure which matches the one described by the lock arguments (fap)
		 * this verification works for both shared and exclusive locks
		 */
		fargs.fa_lock.ld_offset = 0;
		fargs.fa_lock.ld_len = 0;
		fargs.fa_lock.ld_type = F_WRLCK;
		if ( error = local_fcntl( &fargs ) == -1 ) {
			errno = error;
			perror( "verify_fcntl: fcntl" );
			return( 0 );
		} else if ( (fargs.fa_lock.ld_offset != region_start) ||
			(fargs.fa_lock.ld_len != vap->va_lock.ld_len) ||
			!MATCH_LOCK_TYPE( &fargs.fa_lock, vap->va_lock.ld_type ) ) {
				if ( Verbose ) {
					printf( "%s: verify_fcntl: lock mismatch\n", Progname );
					printf( "%s: verify_fcntl: offset = 0x%x(expected 0x%x)\n",
						Progname, fargs.fa_lock.ld_offset, region_start );
					printf( "%s: verify_fcntl: len = %d(expected %d)\n",
						Progname, fargs.fa_lock.ld_len, vap->va_lock.ld_len );
					printf( "%s: verify_fcntl: type = %s(expected %s)\n",
						Progname, locktype_to_str( fargs.fa_lock.ld_type ),
						locktype_to_str( vap->va_lock.ld_type ) );
				}
				return( 0 );
		}
		/*
		 * for a shared lock, make sure that a shared lock of the whole
		 * file will be successful
		 */
		if ( vap->va_lock.ld_type == F_RDLCK ) {
			fargs.fa_lock.ld_offset = 0;
			fargs.fa_lock.ld_len = 0;
			fargs.fa_lock.ld_type = F_RDLCK;
			if ( error = local_fcntl( &fargs ) ) {
				errno = error;
				perror( "verify_fcntl: fcntl" );
				return( 0 );
			} else if ( fargs.fa_lock.ld_type != F_UNLCK ) {
				if ( Verbose ) {
					printf( "%s: verify_fcntl: lock mismatch\n", Progname );
					printf( "%s: verify_fcntl: offset = 0x%x(expected 0x%x)\n",
						Progname, fargs.fa_lock.ld_offset, region_start );
					printf( "%s: verify_fcntl: len = %d(expected %d)\n",
						Progname, fargs.fa_lock.ld_len, vap->va_lock.ld_len );
					printf( "%s: verify_fcntl: type = %s(expected %s)\n",
						Progname, locktype_to_str( fargs.fa_lock.ld_type ),
						locktype_to_str( vap->va_lock.ld_type ) );
				}
				return( 0 );
			}
		}
		/*
		 * reset the flock structure (it was overwritten by fcntl)
		 * if the offset of the lock being verified is non-zero, verify that
		 * the region preceding the lock can be exclusively locked
		 */
		fargs.fa_lock.ld_offset = 0;
		fargs.fa_lock.ld_type = F_WRLCK;
		if ( region_start ) {
			fargs.fa_lock.ld_len = region_start;
			if ( error = local_fcntl( &fargs ) ) {
				errno = error;
				perror( "verify_fcntl: fcntl" );
				return( 0 );
			} else if ( fargs.fa_lock.ld_type != F_UNLCK ) {
				if ( Verbose ) {
					printf( "%s: verify_fcntl: extra lock at front of file\n",
						Progname );
					printf( "%s: verify_fcntl: offset = 0x%x\n", Progname,
						fargs.fa_lock.ld_offset );
					printf( "%s: verify_fcntl: len = %d\n", Progname,
						fargs.fa_lock.ld_len );
					printf( "%s: verify_fcntl: type = %s\n", Progname,
						locktype_to_str( fargs.fa_lock.ld_type ) );
				}
				return( 0 );
			}
		}
		/*
		 * reset the flock structure (it was overwritten by fcntl)
		 * if there is some room at the end of the file following the locked
		 * region, verify that the region following the lock can be exclusively
		 * locked
		 */
		fargs.fa_lock = vap->va_lock;
		fargs.fa_lock.ld_type = F_WRLCK;
		if ( vap->va_lock.ld_len &&
			(vap->va_lock.ld_len + region_start < statbuf.st_size) ) {
				fargs.fa_lock.ld_offset = region_start + vap->va_lock.ld_len;
				fargs.fa_lock.ld_len = 0;
				if ( error = local_fcntl( &fargs) ) {
					errno = error;
					perror( "verify_fcntl: fcntl" );
					return( 0 );
				} else if ( fargs.fa_lock.ld_type != F_UNLCK ) {
					if ( Verbose ) {
						printf( "%s: verify_fcntl: extra lock at end of file\n",
							Progname );
						printf( "%s: verify_fcntl: offset = 0x%x\n", Progname,
							fargs.fa_lock.ld_offset );
						printf( "%s: verify_fcntl: len = %d\n", Progname,
							fargs.fa_lock.ld_len );
						printf( "%s: verify_fcntl: type = %s\n", Progname,
							locktype_to_str( fargs.fa_lock.ld_type ) );
					}
					return( 0 );
				}
		}
	}
	/*
	 * now, attempt to acquire some conflicting locks
	 * first, try an identical exclusive lock
	 * expect to fail
	 */
	fargs.fa_lock = vap->va_lock;
	fargs.fa_lock.ld_type = F_WRLCK;
	fargs.fa_cmd = F_SETLK;
	switch ( error = local_fcntl( &fargs ) ) {
		case EAGAIN:
		case EACCES:
			break;
		case -1:
			(void)fprintf( stderr,
				"%s: verify_fcntl: local_fcntl failed\n", Progname );
			print_fcntlargs( &fargs, "\t" );
			return( 0 );
			break;
		case 0:
			(void)fprintf( stderr,
			"%s: verify_fcntl: identical lock succeeded (failure expected)\n",
			Progname );
			print_fcntlargs( &fargs, "\t" );
			fargs.fa_lock.ld_type = F_UNLCK;
			(void)local_fcntl( &fargs );
			return( 0 );
			break;
		default:
			(void)fprintf( stderr,
				"%s: verify_fcntl: unexpected error: ", Progname );
			errno = error;
			perror( "local_fcntl" );
			print_fcntlargs( &fargs, "\t" );
			return( 0 );
	}
	/*
	 * if the lock is shared, verify that another shared lock will succeed
	 */
	if ( vap->va_lock.ld_type == F_RDLCK ) {
		fargs.fa_lock.ld_type = F_RDLCK;
		switch ( error = local_fcntl( &fargs ) ) {
			case 0:
				fargs.fa_lock.ld_type = F_UNLCK;
				if ( error = local_fcntl( &fargs ) ) {
					fprintf( stderr,
						"%s: verify_fcntl: unable to unlock file\n", Progname );
					if ( error ) {
						errno = error;
						perror( "local_fcntl" );
					}
					return( 0 );
				}
				break;
			case -1:
				(void)fprintf( stderr,
					"%s: verify_fcntl: local_fcntl failed\n", Progname );
				print_fcntlargs( &fargs, "\t" );
				return( 0 );
				break;
			default:
				(void)fprintf( stderr,
					"%s: verify_fcntl: unexpected error: ", Progname );
				errno = error;
				perror( "local_fcntl" );
				print_fcntlargs( &fargs, "\t" );
				return( 0 );
		}
	}
	/*
	 * next, lock the entire file exclusively
	 * expect to fail
	 */
	fargs.fa_lock.ld_offset = 0;
	fargs.fa_lock.ld_len = 0;
	fargs.fa_lock.ld_type = F_WRLCK;
	switch ( error = local_fcntl( &fargs ) ) {
		case EAGAIN:
		case EACCES:
			break;
		case -1:
			(void)fprintf( stderr,
				"%s: verify_fcntl: local_fcntl failed\n", Progname );
			print_fcntlargs( &fargs, "\t" );
			return( 0 );
			break;
		case 0:
			(void)fprintf( stderr,
				"%s: verify_fcntl: file lock succeeded (failure expected)\n",
				Progname );
			print_fcntlargs( &fargs, "\t" );
			fargs.fa_lock.ld_type = F_UNLCK;
			(void)local_fcntl( &fargs );
			return( 0 );
			break;
		default:
			(void)fprintf( stderr,
				"%s: verify_fcntl: unexpected error: ", Progname );
			errno = error;
			perror( "local_fcntl" );
			print_fcntlargs( &fargs, "\t" );
			return( 0 );
	}
	/*
	 * next, lock the entire file shared
	 * expect to succeed
	 */
	if ( vap->va_lock.ld_type == F_RDLCK ) {
		fargs.fa_lock.ld_type = F_RDLCK;
		switch ( error = local_fcntl( &fargs ) ) {
			case 0:
				fargs.fa_lock.ld_type = F_UNLCK;
				if ( error = local_fcntl( &fargs ) ) {
					fprintf( stderr,
						"%s: verify_fcntl: unable to unlock file\n",
						Progname );
					if ( error ) {
						errno = error;
						perror( "local_fcntl" );
					}
					return( 0 );
				}
				break;
			case -1:
				(void)fprintf( stderr,
					"%s: verify_fcntl: local_fcntl failed\n", Progname );
				print_fcntlargs( &fargs, "\t" );
				return( 0 );
				break;
			default:
				(void)fprintf( stderr,
					"%s: verify_fcntl: unexpected error: ", Progname );
				errno = error;
				perror( "local_fcntl" );
				print_fcntlargs( &fargs, "\t" );
				return( 0 );
		}
	}
	/*
	 * next, try a front-overlapping lock if possible
	 * expect to fail in the exclusive case and succeed in the shared case
	 */
	if ( region_start ) {
		fargs.fa_lock.ld_offset = region_start - 1;
		fargs.fa_lock.ld_len = 2;
		fargs.fa_lock.ld_type = F_WRLCK;
		switch ( error = local_fcntl( &fargs ) ) {
			case EAGAIN:
			case EACCES:
				break;
			case -1:
				(void)fprintf( stderr,
					"%s: verify_fcntl: local_fcntl failed\n", Progname );
				print_fcntlargs( &fargs, "\t" );
				return( 0 );
				break;
			case 0:
				(void)fprintf( stderr,
	"%s: verify_fcntl: front-overlapping lock succeeded (failure expected)\n",
					Progname );
				print_fcntlargs( &fargs, "\t" );
				fargs.fa_lock.ld_type = F_UNLCK;
				(void)local_fcntl( &fargs );
				return( 0 );
				break;
			default:
				(void)fprintf( stderr,
					"%s: verify_fcntl: unexpected error: ", Progname );
				errno = error;
				perror( "local_fcntl" );
				print_fcntlargs( &fargs, "\t" );
				return( 0 );
		}
		if ( vap->va_lock.ld_type == F_RDLCK ) {
			fargs.fa_lock.ld_type = F_RDLCK;
			switch ( error = local_fcntl( &fargs ) ) {
				case 0:
					fargs.fa_lock.ld_type = F_UNLCK;
					if ( error = local_fcntl( &fargs ) ) {
						fprintf( stderr,
							"%s: verify_fcntl: unable to unlock file\n",
							Progname );
						if ( error ) {
							errno = error;
							perror( "local_fcntl" );
						}
						return( 0 );
					}
					break;
				case -1:
					(void)fprintf( stderr,
						"%s: verify_fcntl: local_fcntl failed\n", Progname );
					print_fcntlargs( &fargs, "\t" );
					return( 0 );
					break;
				default:
					(void)fprintf( stderr,
						"%s: verify_fcntl: unexpected error: ", Progname );
					errno = error;
					perror( "local_fcntl" );
					print_fcntlargs( &fargs, "\t" );
					return( 0 );
			}
		}
	}
	/*
	 * next, try an end-overlapping lock if possible
	 * expect to fail in the exclusive case and succeed in the shared case
	 */
	if ( vap->va_lock.ld_len &&
		(vap->va_lock.ld_len + region_start < statbuf.st_size) ) {
			fargs.fa_lock.ld_offset = vap->va_lock.ld_len + region_start - 1;
			fargs.fa_lock.ld_len = 2;
			fargs.fa_lock.ld_type = F_WRLCK;
			switch ( error = local_fcntl( &fargs ) ) {
				case EAGAIN:
				case EACCES:
					break;
				case -1:
					(void)fprintf( stderr,
						"%s: verify_fcntl: local_fcntl failed\n", Progname );
					print_fcntlargs( &fargs, "\t" );
					return( 0 );
					break;
				case 0:
					(void)fprintf( stderr,
		"%s: verify_fcntl: end-overlapping lock succeeded (failure expected)\n",
						Progname );
					print_fcntlargs( &fargs, "\t" );
					fargs.fa_lock.ld_type = F_UNLCK;
					(void)local_fcntl( &fargs );
					return( 0 );
					break;
				default:
					(void)fprintf( stderr,
						"%s: verify_fcntl: unexpected error: ", Progname );
					errno = error;
					perror( "local_fcntl" );
					print_fcntlargs( &fargs, "\t" );
					return( 0 );
			}
			if ( vap->va_lock.ld_type == F_RDLCK ) {
				fargs.fa_lock.ld_type = F_RDLCK;
				switch ( error = local_fcntl( &fargs ) ) {
					case 0:
						fargs.fa_lock.ld_type = F_UNLCK;
						if ( error = local_fcntl( &fargs ) ) {
							fprintf( stderr,
								"%s: verify_fcntl: unable to unlock file\n",
								Progname );
							if ( error ) {
								errno = error;
								perror( "local_fcntl" );
							}
							return( 0 );
						}
						break;
					case -1:
						(void)fprintf( stderr,
							"%s: verify_fcntl: local_fcntl failed\n",
							Progname );
						print_fcntlargs( &fargs, "\t" );
						return( 0 );
						break;
					default:
						(void)fprintf( stderr,
							"%s: verify_fcntl: unexpected error: ", Progname );
						errno = error;
						perror( "local_fcntl" );
						print_fcntlargs( &fargs, "\t" );
						return( 0 );
				}
			}
	}
	/*
	 * next, try one byte at the end of the lock range
	 * expect to fail
	 */
	if ( vap->va_lock.ld_len ) {
		fargs.fa_lock.ld_offset = region_start + vap->va_lock.ld_len - 1;
	} else {
		fargs.fa_lock.ld_offset = statbuf.st_size - 1;
	}
	fargs.fa_lock.ld_len = 0;
	fargs.fa_lock.ld_type = F_WRLCK;
	switch ( error = local_fcntl( &fargs ) ) {
		case EAGAIN:
		case EACCES:
			break;
		case -1:
			(void)fprintf( stderr,
				"%s: verify_fcntl: local_fcntl failed\n", Progname );
			print_fcntlargs( &fargs, "\t" );
			return( 0 );
			break;
		case 0:
			(void)fprintf( stderr,
			"%s: verify_fcntl: last-byte lock succeeded (failure expected)\n",
			Progname );
			print_fcntlargs( &fargs, "\t" );
			fargs.fa_lock.ld_type = F_UNLCK;
			(void)local_fcntl( &fargs );
			return( 0 );
			break;
		default:
			(void)fprintf( stderr,
				"%s: verify_fcntl: unexpected error: ", Progname );
			errno = error;
			perror( "local_fcntl" );
			print_fcntlargs( &fargs, "\t" );
			return( 0 );
	}
	if ( vap->va_lock.ld_type == F_RDLCK ) {
		fargs.fa_lock.ld_type = F_RDLCK;
		switch ( error = local_fcntl( &fargs ) ) {
			case 0:
				fargs.fa_lock.ld_type = F_UNLCK;
				if ( error = local_fcntl( &fargs ) ) {
					fprintf( stderr,
						"%s: verify_fcntl: unable to unlock file\n",
						Progname );
					if ( error ) {
						errno = error;
						perror( "local_fcntl" );
					}
					return( 0 );
				}
				break;
			case -1:
				(void)fprintf( stderr,
					"%s: verify_fcntl: local_fcntl failed\n", Progname );
				print_fcntlargs( &fargs, "\t" );
				return( 0 );
				break;
			default:
				(void)fprintf( stderr,
					"%s: verify_fcntl: unexpected error: ", Progname );
				errno = error;
				perror( "local_fcntl" );
				print_fcntlargs( &fargs, "\t" );
				return( 0 );
		}
	}
	/*
	 * next, try one byte at the front of the lock range
	 * expect to fail
	 */
	fargs.fa_lock.ld_offset = region_start;
	fargs.fa_lock.ld_len = 1;
	fargs.fa_lock.ld_type = F_WRLCK;
	switch ( error = local_fcntl( &fargs ) ) {
		case EAGAIN:
		case EACCES:
			break;
		case -1:
			(void)fprintf( stderr,
				"%s: verify_fcntl: local_fcntl failed\n", Progname );
			print_fcntlargs( &fargs, "\t" );
			return( 0 );
			break;
		case 0:
			(void)fprintf( stderr,
			"%s: verify_fcntl: first-byte lock succeeded (failure expected)\n",
			Progname );
			print_fcntlargs( &fargs, "\t" );
			fargs.fa_lock.ld_type = F_UNLCK;
			(void)local_fcntl( &fargs );
			return( 0 );
			break;
		default:
			(void)fprintf( stderr,
				"%s: verify_fcntl: unexpected error: ", Progname );
			errno = error;
			perror( "local_fcntl" );
			print_fcntlargs( &fargs, "\t" );
			return( 0 );
	}
	if ( vap->va_lock.ld_type == F_RDLCK ) {
		fargs.fa_lock.ld_type = F_RDLCK;
		switch ( error = local_fcntl( &fargs ) ) {
			case 0:
				fargs.fa_lock.ld_type = F_UNLCK;
				if ( error = local_fcntl( &fargs ) ) {
					fprintf( stderr,
						"%s: verify_fcntl: unable to unlock file\n",
						Progname );
					if ( error ) {
						errno = error;
						perror( "local_fcntl" );
					}
					return( 0 );
				}
				break;
			case -1:
				(void)fprintf( stderr,
					"%s: verify_fcntl: local_fcntl failed\n", Progname );
				print_fcntlargs( &fargs, "\t" );
				return( 0 );
				break;
			default:
				(void)fprintf( stderr,
					"%s: verify_fcntl: unexpected error: ", Progname );
				errno = error;
				perror( "local_fcntl" );
				print_fcntlargs( &fargs, "\t" );
				return( 0 );
		}
	}
	/*
	 * next, try a region within the lock range but not identical to the
	 * lock range
	 * expect to fail
	 */
	fargs.fa_lock.ld_len = MAX( 1, region_len/4 );
	fargs.fa_lock.ld_offset = region_start + fargs.fa_lock.ld_len;
	fargs.fa_lock.ld_type = F_WRLCK;
	assert( fargs.fa_lock.ld_len > 0 );
	assert( fargs.fa_lock.ld_offset > vap->va_lock.ld_offset );
	assert( fargs.fa_lock.ld_offset <= (vap->va_lock.ld_offset + region_len) );
	switch ( error = local_fcntl( &fargs ) ) {
		case EAGAIN:
		case EACCES:
			break;
		case -1:
			(void)fprintf( stderr,
				"%s: verify_fcntl: local_fcntl failed\n", Progname );
			print_fcntlargs( &fargs, "\t" );
			return( 0 );
			break;
		case 0:
			(void)fprintf( stderr,
			"%s: verify_fcntl: sub-range lock succeeded (failure expected)\n",
			Progname );
			print_fcntlargs( &fargs, "\t" );
			fargs.fa_lock.ld_type = F_UNLCK;
			(void)local_fcntl( &fargs );
			return( 0 );
			break;
		default:
			(void)fprintf( stderr,
				"%s: verify_fcntl: unexpected error: ", Progname );
			errno = error;
			perror( "local_fcntl" );
			print_fcntlargs( &fargs, "\t" );
			return( 0 );
	}
	if ( vap->va_lock.ld_type == F_RDLCK ) {
		fargs.fa_lock.ld_type = F_RDLCK;
		switch ( error = local_fcntl( &fargs ) ) {
			case 0:
				fargs.fa_lock.ld_type = F_UNLCK;
				if ( error = local_fcntl( &fargs ) ) {
					fprintf( stderr,
						"%s: verify_fcntl: unable to unlock file\n",
						Progname );
					if ( error ) {
						errno = error;
						perror( "local_fcntl" );
					}
					status = 0;
				}
				break;
			case -1:
				(void)fprintf( stderr,
					"%s: verify_fcntl: local_fcntl failed\n", Progname );
				print_fcntlargs( &fargs, "\t" );
				status = 0;
				break;
			default:
				(void)fprintf( stderr,
					"%s: verify_fcntl: unexpected error: ", Progname );
				errno = error;
				perror( "local_fcntl" );
				print_fcntlargs( &fargs, "\t" );
				status = 0;
		}
	}
	return(status);
}

/*
 * verify that the lock is correctly held
 * return true or false
 *
 * the following verifications are done:
 *		1.	use fcntl to find out what is locked
 *		2.	attempt to acquire a conflicting lock
 *		3.	check locked region boundaries
 */
int
verify_lock( struct verifyargs *vap )
{
	int status = 1;
	int error;
	int flags;

	assert( valid_addresses( (caddr_t)vap, sizeof(struct verifyargs) ) );
	if ( Verbose > 1 ) {
		(void)printf("%s: verify_lock\n", Progname);
	}
	status = verify_fcntl(vap);
	if ( Verbose > 1 ) {
		printf( "%s: verify_lock: return %d\n", Progname, status );
	}
	return( status );
}
