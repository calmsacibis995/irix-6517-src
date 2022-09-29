#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/common/RCS/fs.c,v 1.17 1997/08/14 20:38:47 prasadb Exp $"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/uuid.h>
#include <time.h>
#include <sys/fs/xfs_itable.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <mntent.h>
#include <dirent.h>
#include <assert.h>
#ifdef FICUS
#include <diskinfo.h>
#endif /* FICUS */
#include <task.h>

#include "types.h"
#include "jdm.h"
#include "mlog.h"
#include "fs.h"

/* default maximim path and name lengths
 */
#define FS_MAXNAMELEN_DEFAULT	256
#define FS_MAXPATHLEN_DEFAULT	1024

/* fs_info - a magic, highly heuristic function to convert a user-supplied
 * string into a file system type, character special device pathname,
 * a mount point, and file system ID (uuid). The primary source of information
 * is the getmntent(3) routines. Correspondence between "standard" disk
 * block and character device pathnames is used. The fstyp(1) utility
 * may be invoked as well.
 *
 * returns BOOL_TRUE on success, BOOL_FALSE on failure.
 *
 * coding rules: to make this very complex and lengthy decision tree
 * more graspable, some very strict coding rules were followed:
 *
 *	1) function arguments are ordered reference returns first, input
 *	parameters second.
 *
 *	2) all buffer-like refence return arguments are followed by a
 *	caller-supplied buffer size.
 *
 *	3) wherever possible functions return the result buffer pointer.
 *
 *	4) boolean functions return TRUE on success, FALSE on failure.
 *
 * all variables, parameters, and structure members are named as follows:
 *	object types:
 *		usr - user-specified mystery
 *		typ - file system type
 *		mnt - mount point pathname
 *		blk - block device pathname
 *		chr - character device pathname
 *		id - file system ID
 *		stat - stat buffer
 *		te - file system table entry
 *	object modifiers: appended to object type:
 *		b - buffer
 *		s - string
 *		d - default string
 *		p - pointer
 *	object size indication modifiers: appended to a modified object type:
 *		z - buffer size
 *		l - string length
 */

/* declarations of externally defined global variables
 */


/* definitions used locally
 */
struct fs_tab_ent {
	char *fte_blks;
	char *fte_chrs;
	char *fte_mnts;
	char *fte_typs;
	struct fs_tab_ent *fte_nextp;
};

typedef struct fs_tab_ent fs_tab_ent_t;

static	fs_tab_ent_t *fs_tabp;

/* forward declarations
 */
static void fs_tab_build( void );
static void fs_tab_free( void );
static fs_tab_ent_t *fs_tab_ent_build( struct mntent * );
static void fs_tab_ent_free( fs_tab_ent_t * );
static fs_tab_ent_t *fs_tab_lookup_blk( char * );
static fs_tab_ent_t *fs_tab_lookup_chr( char * );
static fs_tab_ent_t *fs_tab_lookup_mnt( char * );
static char *fstyp( char * );
static char *fs_blktochr( char *blkpn );
static char *fs_chrtoblk( char *chrpn );

/* ARGSUSED */
bool_t
fs_info( char *typb,		/* out */
	 intgen_t typbz,
	 char *typd,
	 char *chrb,		/* out */
	 intgen_t chrbz,
	 char *mntb,		/* out */
	 intgen_t mntbz,
	 uuid_t *idb,		/* out */
	 char *usrs )		/* in */
{
	stat64_t statb;
	fs_tab_ent_t *tep;
	char *chrs;
	char *blks;
	char *mnts;
	char *typs;
	bool_t canstat;
	bool_t ok = BOOL_UNKNOWN;

	fs_tab_build( );

	canstat = ( stat64( usrs, &statb ) == 0 );
	if ( canstat && ( statb.st_mode & S_IFMT ) == S_IFBLK ) {
		if ( ( tep = fs_tab_lookup_blk( usrs )) != 0 ) {
			mnts = tep->fte_mnts;
			if ( mnts ) {
				assert( strlen( mnts ) < ( size_t )mntbz );
				strcpy( mntb, mnts );
			} else {
				mntb[ 0 ] = 0;
			}
			if ( ( typs = tep->fte_typs ) == 0 ) {
				if ( ( typs = fstyp( usrs )) == 0 ) {
					typs = typd;
				}
			}
			assert( strlen( typs ) < ( size_t )typbz );
			strcpy( typb, typs );
			if ( ( chrs = tep->fte_chrs ) == 0
			     &&
			     ( chrs = fs_blktochr( usrs )) == 0 ) {
				ok = BOOL_FALSE;
			} else {
				assert( strlen( chrs ) < ( size_t )chrbz );
				strcpy( chrb, chrs );
				ok = BOOL_TRUE;
			}
		} else if ( ( chrs = fs_blktochr( usrs )) != 0 ) {
			if ( ( tep = fs_tab_lookup_chr( chrs )) != 0 ) {
				mnts = tep->fte_mnts;
				if ( mnts ) {
					assert( strlen(mnts) < (size_t)mntbz );
					strcpy( mntb, mnts );
				} else {
					mntb[ 0 ] = 0;
				}
				if ( ( typs = tep->fte_typs ) == 0 ) {
					if ( ( typs = fstyp( usrs )) == 0 ) {
						typs = typd;
					}
				}
				assert( strlen( typs ) < ( size_t )typbz );
				strcpy( typb, typs );
				assert( strlen( chrs ) < ( size_t )chrbz );
				strcpy( chrb, chrs );
				ok = BOOL_TRUE;
			} else {
				ok = BOOL_FALSE;
			}
		} else {
			ok = BOOL_FALSE;
		}
	} else if ( canstat && ( statb.st_mode & S_IFMT ) == S_IFCHR ) {
		assert( strlen( usrs ) < ( size_t )chrbz );
		strcpy( chrb, usrs );
		if ( ( tep = fs_tab_lookup_chr( usrs )) != 0 ) {
			mnts = tep->fte_mnts;
			if ( mnts ) {
				assert( strlen( mnts ) < ( size_t )mntbz );
				strcpy( mntb, mnts );
			} else {
				mntb[ 0 ] = 0;
			}
			if ( ( typs = tep->fte_typs ) == 0 ) {
				if ( ( typs = fstyp( usrs )) == 0 ) {
					typs = typd;
				}
			}
			assert( strlen( typs ) < ( size_t )typbz );
			strcpy( typb, typs );
			ok = BOOL_TRUE;
		} else if ( ( blks = fs_chrtoblk( usrs )) != 0 ) {
			if ( ( tep = fs_tab_lookup_blk( blks )) != 0 ) {
				mnts = tep->fte_mnts;
				if ( mnts ) {
					assert( strlen(mnts) < (size_t)mntbz );
					strcpy( mntb, mnts );
				} else {
					mntb[ 0 ] = 0;
				}
				if ( ( typs = tep->fte_typs ) == 0 ) {
					if ( ( typs = fstyp( usrs )) == 0 ) {
						typs = typd;
					}
				}
			} else {
				mntb[ 0 ] = 0;
				if ( ( typs = fstyp( usrs )) == 0 ) {
					typs = typd;
				}
			}
			assert( strlen( typs ) < ( size_t )typbz );
			strcpy( typb, typs );
			ok = BOOL_TRUE;
		} else {
			mntb[ 0 ] = 0;
			if ( ( typs = fstyp( usrs )) == 0 ) {
				typs = typd;
			}
			assert( strlen( typs ) < ( size_t )typbz );
			strcpy( typb, typs );
			ok = BOOL_TRUE;
		}
	} else if ( ( tep = fs_tab_lookup_mnt( usrs )) != 0 ) {
		mnts = tep->fte_mnts;
		assert( strlen( mnts ) < ( size_t )mntbz );
		strcpy( mntb, mnts );
		if ( ( typs = tep->fte_typs ) == 0 ) {
			if ( ( typs = fstyp( tep->fte_blks )) == 0 ) {
				typs = typd;
			}
		}
		assert( strlen( typs ) < ( size_t )typbz );
		strcpy( typb, typs );
		chrs = tep->fte_chrs;
		if ( chrs ) {
			assert( strlen( chrs ) < ( size_t )chrbz );
			strcpy( chrb, chrs );
			ok = BOOL_TRUE;
		} else {
			blks = tep->fte_blks;
			assert( blks );
			if ( ( chrs =  fs_blktochr( blks )) != 0 ) {
				assert( strlen( chrs ) < ( size_t )chrbz );
				strcpy( chrb, chrs );
				ok = BOOL_TRUE;
			} else {
				ok = BOOL_FALSE;
			}
		}
	} else {
		ok = BOOL_FALSE;
	}

	fs_tab_free( );
	assert( ok != BOOL_UNKNOWN );

	if ( ok == BOOL_TRUE ) {
		intgen_t rval = fs_getid( mntb, idb );
		if ( rval ) {
			mlog( MLOG_NORMAL,
			      "unable to determine uuid of fs mounted at %s: "
			      "%s\n",
			      mntb,
			      strerror( errno ));
		}
		{
			u_char_t *string_uuid;
			u_int32_t uuid_stat;
			string_uuid = 0; /* keep lint happy */
			uuid_to_string( idb,
					( char ** )( &string_uuid ),
					&uuid_stat );
			assert( uuid_stat == uuid_s_ok );
			mlog( MLOG_DEBUG,
			      "fs %s uuid [%s]\n",
			      mntb,
			      string_uuid );
			free( ( void * )string_uuid );
		}
	}

	return ok;
}

/* fs_mounted - a predicate determining if the specified file system
 * is actually currently mounted at its mount point.
 * will not take time to code this now - just check if mntpt is non-NULL.
 */
/* ARGSUSED */
bool_t
fs_mounted( char *typs, char *chrs, char *mnts, uuid_t *idp )
{
	return strlen( mnts ) > 0 ? BOOL_TRUE : BOOL_FALSE;
}

intgen_t
fs_getid( char *mnts, uuid_t *idb )
{
#ifdef BANYAN
	struct statvfs64 vfsstat;
#else /* BANYAN */
	struct statvfs vfsstat;
#endif /* BANYAN */
	u_int32_t uuid_stat;
	intgen_t rval;

#ifdef BANYAN
	rval = statvfs64( mnts, &vfsstat );
#else /* BANYAN */
	rval = statvfs( mnts, &vfsstat );
#endif /* BANYAN */
	if ( rval ) {
		uuid_create_nil( idb, &uuid_stat );
		assert( uuid_stat == uuid_s_ok );
		return -1;
	}
	*idb = *( uuid_t * )vfsstat.f_fstr;

	return 0;
}

size_t
fs_getinocnt( char *mnts )
{
	struct statvfs vfsstat;
	intgen_t rval;

	rval = statvfs( mnts, &vfsstat );
	if ( rval ) {
		return 0;
	}

	if ( vfsstat.f_files < vfsstat.f_ffree ) {
		return 0;
	}

	return ( size_t )( vfsstat.f_files - vfsstat.f_ffree );
}

static void
fs_tab_build( void )
{
	register struct mntent *mntentp;
	fs_tab_ent_t *tep;
	FILE *fp;

	fs_tabp = 0;
	fp = setmntent( MOUNTED, "r" );
        if ( fp == NULL ) {
		mlog( MLOG_NORMAL,
		      "Can't open %s for mount information\n",
		      MOUNTED );
		return;
	}
	while ( ( mntentp = getmntent( fp )) != 0 ) {
		tep = fs_tab_ent_build( mntentp );
		tep->fte_nextp = fs_tabp;
		fs_tabp = tep;
	}
	endmntent( fp );
}

static void
fs_tab_free( void )
{
	fs_tab_ent_t *tep;
	fs_tab_ent_t *otep;

	for ( tep = fs_tabp
	      ;
	      tep
	      ;
	      otep = tep, tep = tep->fte_nextp, fs_tab_ent_free( otep ) )

		;
}

static fs_tab_ent_t *
fs_tab_ent_build( struct mntent *mntentp )
{
	fs_tab_ent_t *tep;
	char *cp;
	char *chrs;

	tep = ( fs_tab_ent_t * )calloc( 1, sizeof( fs_tab_ent_t ));
	assert( tep );

	if ( mntentp->mnt_dir ) {
		cp = calloc( 1, strlen( mntentp->mnt_dir ) + 1 );
		assert( cp );
		( void )strcpy( cp, mntentp->mnt_dir );
		tep->fte_mnts = cp;
	} else {
		tep->fte_mnts = 0;
	}

	if ( mntentp->mnt_type ) {
		cp = calloc( 1, strlen( mntentp->mnt_type ) + 1 );
		assert( cp );
		( void )strcpy( cp, mntentp->mnt_type );
		tep->fte_typs = cp;
	} else {
		tep->fte_typs = 0;
	}

	if ( mntentp->mnt_fsname ) {
		cp = calloc( 1, strlen( mntentp->mnt_fsname ) + 1 );
		assert( cp );
		( void )strcpy( cp, mntentp->mnt_fsname );
		tep->fte_blks = cp;
	} else {
		tep->fte_blks = 0;
	}

	chrs = hasmntopt( mntentp, MNTOPT_RAW );
	if ( chrs ) {
		char *sep;
		chrs += strlen( MNTOPT_RAW ) + 1; /* adjust for "raw=" */
		sep = strchr( chrs, ',' );
		if ( sep ) {
			*sep = 0;
		}
		cp = calloc( 1, strlen( chrs ) + 1 );
		assert( cp );
		( void )strcpy( cp, chrs );
		tep->fte_chrs = cp;
		if ( sep ) {
			*sep = ',';
		}
	} else {
		tep->fte_chrs = 0;
	}

	return tep;
}

static void
fs_tab_ent_free( fs_tab_ent_t *tep )
{
	if ( tep->fte_chrs ) free( tep->fte_chrs );
	if ( tep->fte_blks ) free( tep->fte_blks );
	if ( tep->fte_mnts ) free( tep->fte_mnts );
	if ( tep->fte_typs ) free( tep->fte_typs );
	memset( ( void * )tep, 0, sizeof( *tep )); /* bug catcher */
	free( tep );
}

static fs_tab_ent_t *
fs_tab_lookup_blk( char *blks )
{
	fs_tab_ent_t *tep;

	for ( tep = fs_tabp ; tep ; tep = tep->fte_nextp ) {
		stat64_t stata;
		bool_t aok;
		stat64_t statb;
		bool_t bok;

		if ( ! tep->fte_blks ) {
			continue;
		}

		if ( ! strcmp( tep->fte_blks, blks )) {
			return tep;
		}

		aok = ! stat64( blks, &stata );
		bok = ! stat64( tep->fte_blks, &statb );
		if ( aok && bok && stata.st_rdev == statb.st_rdev ) {
			return tep;
		}
	}
	return 0;
}

static fs_tab_ent_t *
fs_tab_lookup_chr( char *chrs )
{
	fs_tab_ent_t *tep;

	for ( tep = fs_tabp ; tep ; tep = tep->fte_nextp ) {
		if ( tep->fte_chrs && ! strcmp( tep->fte_chrs, chrs )) {
			return tep;
		}
	}
	return 0;
}

static fs_tab_ent_t *
fs_tab_lookup_mnt( char *mnts )
{
	fs_tab_ent_t *tep;

	for ( tep = fs_tabp ; tep ; tep = tep->fte_nextp ) {
		if ( tep->fte_mnts && ! strcmp( tep->fte_mnts, mnts )) {
			return tep;
		}
	}
	return 0;
}

#define FSTYP "/usr/sbin/fstyp"

static char *
fstyp( char *devs )
{
	static char typ[ 20 ];
	pid_t pid;
	char tmpnm[ 32 ];
	char cmdline[ 100 ];
	int fd;
	int nread;

	pid = get_pid( );
	assert( pid > 1 );

	if ( ! devs ) {
		return 0;
	}

	sprintf( tmpnm, "/tmp/fstyp.%d", pid );
	sprintf( cmdline, "%s %s > %s", FSTYP, devs, tmpnm );
	assert( strlen( cmdline ) < sizeof( cmdline ));
	if ( system( cmdline ) != 0 ) {
		unlink( tmpnm );
		return 0;
	}
	if ( ( fd = open( tmpnm, O_RDONLY )) < 0 ) {
		unlink( tmpnm );
		return 0;
	}

	if ( ( nread = read( fd, typ, sizeof( typ ) - 1 )) < 0 ) {
		close( fd );
		unlink( tmpnm );
		return 0;
	}

	if ( nread == 0 ) {
		close( fd );
		unlink( tmpnm );
		return 0;
	}

	typ[ nread ] = 0;
	close( fd );
	unlink( tmpnm );
	return typ;
}

/* fs_blktochr, fs_chrtoblk - heuristics for converting the full pathname
 * of a block or character special device disk into the corresponding
 * character or block special device disk pathname.
 *
 * returns the alternate device pathname on success, zero on failure.
 *
 * the fast way is tried first. if the given path starts with /dev/dsk/
 * (/dev/rdsk), constructs a corresponding /dev/rdsk (/dev/dsk) and stats
 * that to see if st_rdev matches. failing that, tries hard way: search all
 * files in /dev/rdsk (/dev/dsk), /dev, /root/dev/rdsk (/root/dev/dsk),
 * and /root/dev for a matching st_rdev.
 *
 * vocabulary:
 *	blk - block device
 *	chr - character device
 *	pn - pathname
 *	sb - stat buffer
 *	sz - buffer size
 *	sl - string length
 *	rt - base of device full path name
 */
static char *
fs_blktochr( char *blkpn )
{
#ifdef FICUS
	return findrawpath( blkpn );
#else /* FICUS */
	static char *dirlist[] = {
		"/dev/rdsk",
		"/dev",
		"/root/dev/rdsk",
		"/root/dev",
		0
	};
	static char chrpn[ 200 ];
	/* REFERENCED */
	size_t chrpnsz = sizeof( chrpn );
	char *blkrt = "/dev/dsk/";
	char *chrrt = "/dev/rdsk/";
	/* REFERENCED */
	size_t blkpnsl = strlen( blkpn );
	size_t blkrtsl = strlen( blkrt );
	/* REFERENCED */
	size_t chrrtsl = strlen( chrrt );
	stat64_t blksb;
	stat64_t chrsb;
	char **cdir;
	DIR *dirp;
#ifdef BANYAN
	dirent64_t *direntp;
#else /* BANYAN */
	dirent_t *direntp;
#endif /* BANYAN */
	/* REFERENCED */
	intgen_t rval;

	/* first the easy way
	 */

	/* verify I was handed a block special device
	 */
	rval = stat64( blkpn, &blksb );
	assert( rval == 0 );
	assert( ( blksb.st_mode & S_IFMT ) == S_IFBLK );

	/* if the base of the block pathname is standard, check for
	 * a similarly standard character pathname.
	 */
	if ( ! strncmp( blkpn, blkrt, blkrtsl )) {
		/* make sure the result buffer is big enough to
		 * hold the character pathname, null-terminated.
		 */
		assert( blkpnsl + chrrtsl < chrpnsz + blkrtsl );
		/* convert the block pathname into the corresponding
		 * character pathname.
		 */
		sprintf( chrpn, "%s%s", chrrt, blkpn + blkrtsl );
		/* if the corresponding file exists, is a character
		 * special device, and has a matching raw dev_t,
		 * we've found it.
		 */
		if ( stat64( chrpn, &chrsb ) == 0
			    && ( chrsb.st_mode & S_IFMT ) == S_IFCHR
				    && chrsb.st_rdev == blksb.st_rdev ) {
			return chrpn;
		}
	}

	/* now the hardway: check every file in the "likely" directories
	 * for a character special file with the same raw dev_t as the
	 * block special file.
	 */
	for( cdir = dirlist ; *cdir ; cdir++ ) { 
		if ( ( dirp = opendir( *cdir )) == 0 ) {
			continue;
		}
#ifdef BANYAN
		while ( ( direntp = readdir64( dirp )) != 0 ) {
#else /* BANYAN */
		while ( ( direntp = readdir( dirp )) != 0 ) {
#endif /* BANYAN */
			char *fnm = direntp->d_name;
			if ( stat64( fnm, &chrsb ) == 0
				&& ( chrsb.st_mode & S_IFMT ) == S_IFCHR
					&& chrsb.st_rdev == blksb.st_rdev ) {
				assert( strlen( *cdir ) + 1 + strlen( fnm )
					<
					chrpnsz );
				sprintf( chrpn, "%s/%s", *cdir, fnm );
				closedir( dirp );
				return chrpn;
			}
		}
		closedir( dirp );
	}

	return 0;
#endif /* FICUS */
}

static char *
fs_chrtoblk( char *chrpn )
{
#ifdef FICUS
	return findblockpath( chrpn );
#else /* FICUS */
	static char *dirlist[] = {
		"/dev/dsk",
		"/dev",
		"/root/dev/dsk",
		"/root/dev",
		0
	};
	static char blkpn[ 200 ];
	/* REFERENCED */
	size_t blkpnsz = sizeof( blkpn );
	char *chrrt = "/dev/rdsk/";
	char *blkrt = "/dev/dsk/";
	/* REFERENCED */
	size_t chrpnsl = strlen( chrpn );
	size_t chrrtsl = strlen( chrrt );
	/* REFERENCED */
	size_t blkrtsl = strlen( blkrt );
	stat64_t chrsb;
	stat64_t blksb;
	char **cdir;
	DIR *dirp;
#ifdef BANYAN
	dirent64_t *direntp;
#else /* BANYAN */
	dirent_t *direntp;
#endif /* BANYAN */
	/* REFERENCED */
	intgen_t rval;

	/* first the easy way
	 */

	/* verify I was handed a character special device
	 */
	rval = stat64( chrpn, &chrsb );
	assert( rval == 0 );
	assert( ( chrsb.st_mode & S_IFMT ) == S_IFCHR );

	/* if the base of the character pathname is standard, check for
	 * a similarly standard block pathname.
	 */
	if ( ! strncmp( chrpn, chrrt, chrrtsl )) {
		/* make sure the result buffer is big enough to
		 * hold the block pathname, null-terminated.
		 */
		assert( chrpnsl + blkrtsl < blkpnsz + chrrtsl );
		/* convert the character pathname into the corresponding
		 * block pathname.
		 */
		sprintf( blkpn, "%s%s", blkrt, chrpn + chrrtsl );
		/* if the corresponding file exists, is a block
		 * special device, and has a matching raw dev_t,
		 * we've found it.
		 */
		if ( stat64( blkpn, &blksb ) == 0
			    && ( blksb.st_mode & S_IFMT ) == S_IFBLK
				    && blksb.st_rdev == chrsb.st_rdev ) {
			return blkpn;
		}
	}

	/* now the hardway: check every file in the "likely" directories
	 * for a block special file with the same raw dev_t as the
	 * character special file.
	 */
	for( cdir = dirlist ; *cdir ; cdir++ ) { 
		if ( ( dirp = opendir( *cdir )) == 0 ) {
			continue;
		}
#ifdef BANYAN
		while ( ( direntp = readdir64( dirp )) != 0 ) {
#else /* BANYAN */
		while ( ( direntp = readdir( dirp )) != 0 ) {
#endif /* BANYAN */
			char *fnm = direntp->d_name;
			if ( stat64( fnm, &blksb ) == 0
				&& ( blksb.st_mode & S_IFMT ) == S_IFBLK
					&& blksb.st_rdev == chrsb.st_rdev ) {
				assert( strlen( *cdir ) + 1 + strlen( fnm )
					<
					blkpnsz );
				sprintf( blkpn, "%s/%s", *cdir, fnm );
				closedir( dirp );
				return blkpn;
			}
		}
		closedir( dirp );
	}

	return 0;
#endif /* FICUS */
}
