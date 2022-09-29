#ident "$Id: jdm.c,v 1.1 1999/03/05 23:44:05 cwf Exp $"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/handle.h>
#include <sys/fs/xfs_itable.h>
#include <assert.h>

#include "types.h"
#include "jdm.h"
#include "util.h"


/*
 *  There are copies of this file in cmd/xfs/dump/common/jdm.c
 *  and cmd/rfind/fsdump/jdm.c.
 */

/* structure definitions used locally ****************************************/

/* internal fshandle - typecast to a void for external use
 */
#define FSHANDLE_SZ		8

struct fshandle {
	char fsh_space[ FSHANDLE_SZ ];
};

typedef struct fshandle fshandle_t;


/* private file handle - for use by open_by_handle
 */
#ifdef BANYAN
#define FILEHANDLE_SZ		24
#define FILEHANDLE_SZ_FOLLOWING	14
#else
#define FILEHANDLE_SZ		20
#define FILEHANDLE_SZ_FOLLOWING	10
#endif /* BANYAN */
#define FILEHANDLE_SZ_PAD	2

struct filehandle {
	fshandle_t fh_fshandle;		/* handle of fs containing this inode */
	int16_t fh_sz_following;	/* bytes in handle after this member */
	char fh_pad1[ 2 ];		/* padding; however, must be zeroed */
	u_int32_t fh_gen;		/* generation count */
#ifdef BANYAN
	ino64_t fh_ino;			/* 64 bit ino */
#else
	ino_t fh_ino;			/* 32 bit ino */
#endif /* BANYAN */
};

typedef struct filehandle filehandle_t;



/* declarations of externally defined global symbols *************************/


/* forward declarations of locally defined static functions ******************/

static void jdm_fill_filehandle( filehandle_t *, fshandle_t *, xfs_bstat_t * );


/* definition of locally defined global variables ****************************/


/* definition of locally defined static variables *****************************/


/* definition of locally defined global functions ****************************/

jdm_fshandle_t *
jdm_getfshandle( char *mntpnt )
{
	fshandle_t *fshandlep;
	size_t fshandlesz;
	intgen_t rval;

	/* sanity checks
	 */
	assert( sizeof( fshandle_t ) == FSHANDLE_SZ );
	assert( sizeof( filehandle_t ) == FILEHANDLE_SZ );
	assert( sizeof( filehandle_t )
		-
		offsetofmember( filehandle_t, fh_pad1 )
		==
		FILEHANDLE_SZ_FOLLOWING );
	assert( sizeofmember( filehandle_t, fh_pad1 ) == FILEHANDLE_SZ_PAD );
	assert( FILEHANDLE_SZ_PAD == sizeof( int16_t ));

	fshandlep = 0; /* for lint */
	fshandlesz = sizeof( *fshandlep );
	rval = path_to_fshandle( mntpnt, ( void ** )&fshandlep, &fshandlesz );
	assert( fshandlesz == sizeof( *fshandlep ));
	if ( rval ) {
		return 0;
	} else {
		return ( jdm_fshandle_t * )fshandlep;
	}
}


intgen_t
jdm_open( jdm_fshandle_t *fshp, xfs_bstat_t *statp, intgen_t oflags )
{
	register fshandle_t *fshandlep = ( fshandle_t * )fshp;
	filehandle_t filehandle;
	intgen_t fd;

	jdm_fill_filehandle( &filehandle, fshandlep, statp );
	fd = open_by_handle( ( void * )&filehandle,
			     sizeof( filehandle ),
			     oflags );
	return fd;
}


intgen_t
jdm_readlink( jdm_fshandle_t *fshp,
	      xfs_bstat_t *statp,
	      char *bufp, size_t bufsz )
{
	register fshandle_t *fshandlep = ( fshandle_t * )fshp;
	filehandle_t filehandle;
	intgen_t rval;

	jdm_fill_filehandle( &filehandle, fshandlep, statp );
	rval = readlink_by_handle( ( void * )&filehandle,
				   sizeof( filehandle ),
				   ( void * )bufp,
				   bufsz );
	return rval;
}

intgen_t
jdm_attr_multi(	jdm_fshandle_t *fshp,
	      xfs_bstat_t *statp,
	      char *bufp, int rtrvcnt, int flags)
{
	register fshandle_t *fshandlep = ( fshandle_t * )fshp;
	filehandle_t filehandle;
	intgen_t rval;

	jdm_fill_filehandle( &filehandle, fshandlep, statp );
	rval = attr_multi_by_handle ( ( void * )&filehandle,
				      sizeof( filehandle ),
				      (void *) bufp,
				      rtrvcnt, flags);
	return rval;
}

/*	ARGSUSED	*/
intgen_t
jdm_attr_list(	jdm_fshandle_t *fshp,
		xfs_bstat_t *statp,
		char *bufp, size_t bufsz, int flags, 
		struct attrlist_cursor *cursor)
{
	register fshandle_t *fshandlep = ( fshandle_t * )fshp;
	filehandle_t filehandle;
	intgen_t rval;

	jdm_fill_filehandle( &filehandle, fshandlep, statp );
	rval = attr_list_by_handle (( void * )&filehandle,
			sizeof( filehandle ),
			bufp, bufsz, flags, cursor);
	return rval;
}



/* definition of locally defined static functions ****************************/

static void
jdm_fill_filehandle( filehandle_t *handlep,
		     fshandle_t *fshandlep, 
		     xfs_bstat_t *statp )
{
	handlep->fh_fshandle = *fshandlep;
	handlep->fh_sz_following = FILEHANDLE_SZ_FOLLOWING;
	*( int16_t * )handlep->fh_pad1 = 0; /* see assert above */
	handlep->fh_gen = statp->bs_gen;
#ifdef BANYAN
	handlep->fh_ino = statp->bs_ino;
#else
	assert( statp->bs_ino <= ( ino64_t )INOMAX );
	handlep->fh_ino = ( ino_t )statp->bs_ino;
#endif
}
