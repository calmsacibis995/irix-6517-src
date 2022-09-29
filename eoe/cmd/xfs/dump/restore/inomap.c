#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/restore/RCS/inomap.c,v 1.10 1998/06/11 22:20:42 cwf Exp $"

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/uuid.h>
#include <errno.h>
#include <time.h>
#include <sys/fs/xfs_itable.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/dirent.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <assert.h>

#include "types.h"
#include "jdm.h"
#include "util.h"
#include "openutil.h"
#include "mlog.h"
#include "global.h"
#include "drive.h"
#include "media.h"
#include "content.h"
#include "content_inode.h"
#include "inomap.h"


/* structure definitions used locally ****************************************/

/* restores the inomap into a file
 */
#define PERS_NAME	"inomap"

/* reserve the first page for persistent state
 */
struct pers {
	size64_t hnkcnt;
		/* number of hunks in map
		 */
	size64_t segcnt;
		/* number of segments
		 */
	ino64_t last_ino_added;
};

typedef struct pers pers_t;

#define PERSSZ	perssz

/* map context and operators
 */

/* the inomap is implemented as a linked list of chunks. each chunk contains
 * an array of map segments. a map segment contains a start ino and a
 * bitmap of 64 3-bit state values (see MAP_... in inomap.h). the SEG_macros
 * index and manipulate the 3-bit state values.
 */
struct seg {
	ino64_t base;
	u_int64_t lobits;
	u_int64_t mebits;
	u_int64_t hibits;
};

typedef struct seg seg_t;

#define SEG_SET_BASE( segp, ino )					\
	{								\
		segp->base = ino;					\
	}

#define SEG_ADD_BITS( segp, ino, state )				\
	{								\
		register ino64_t relino;				\
		relino = ino - segp->base;				\
		segp->lobits |= ( u_int64_t )( ( state >> 0 ) & 1 ) << relino; \
		segp->mebits |= ( u_int64_t )( ( state >> 1 ) & 1 ) << relino; \
		segp->hibits |= ( u_int64_t )( ( state >> 2 ) & 1 ) << relino; \
	}

#define SEG_SET_BITS( segp, ino, state )				\
	{								\
		register ino64_t relino;				\
		register u_int64_t clrmask;				\
		relino = ino - segp->base;				\
		clrmask = ~( ( u_int64_t )1 << relino );		\
		segp->lobits &= clrmask;				\
		segp->mebits &= clrmask;				\
		segp->hibits &= clrmask;				\
		segp->lobits |= ( u_int64_t )( ( state >> 0 ) & 1 ) << relino; \
		segp->mebits |= ( u_int64_t )( ( state >> 1 ) & 1 ) << relino; \
		segp->hibits |= ( u_int64_t )( ( state >> 2 ) & 1 ) << relino; \
	}

#define SEG_GET_BITS( segp, ino, state )				\
	{								\
		register ino64_t relino;				\
		relino = ino - segp->base;				\
		state = 0;						\
		state |= ( intgen_t )((( segp->lobits >> relino ) & 1 ) * 1 );\
		state |= ( intgen_t )((( segp->mebits >> relino ) & 1 ) * 2 );\
		state |= ( intgen_t )((( segp->hibits >> relino ) & 1 ) * 4 );\
	}

#define INOPERSEG	( sizeofmember( seg_t, lobits ) * NBBY )

#define HNKSZ		( 4 * PGSZ )
#define SEGPERHNK	( ( HNKSZ / sizeof( seg_t )) - 1 )

struct hnk {
	seg_t seg[ SEGPERHNK ];
	ino64_t maxino;
	struct hnk *nextp;
	char pad[sizeof( seg_t ) - sizeof( ino64_t ) - sizeof( struct hnk * )];
};

typedef struct hnk hnk_t;



/* declarations of externally defined global symbols *************************/

extern size_t pgsz;


/* forward declarations of locally defined static functions ******************/

/* inomap primitives
 */
static intgen_t map_getset( ino64_t, intgen_t, bool_t );
static intgen_t map_set( ino64_t ino, intgen_t );


/* definition of locally defined global variables ****************************/


/* definition of locally defined static variables *****************************/

static intgen_t pers_fd = -1;
	/* file descriptor for persistent inomap backing store
	 */

/* context for inomap construction - initialized by inomap_restore_pers
 */
static u_int64_t hnkcnt;
static u_int64_t segcnt;
static hnk_t *roothnkp = 0;
static hnk_t *tailhnkp;
static seg_t *lastsegp;
static ino64_t last_ino_added;


/* definition of locally defined global functions ****************************/

rv_t
inomap_restore_pers( drive_t *drivep,
		     content_inode_hdr_t *scrhdrp,
		     char *hkdir )
{
	drive_ops_t *dop = drivep->d_opsp;
	char *perspath;
	pers_t *persp;
	intgen_t fd;
	/* REFERENCED */
	intgen_t nread;
	intgen_t rval;
	/* REFERENCED */
	intgen_t rval1;
	bool_t ok;

	/* sanity checks
	 */
	assert( INOPERSEG == ( sizeof( (( seg_t * )0 )->lobits ) * NBBY ));
	assert( sizeof( hnk_t ) == HNKSZ );
	assert( HNKSZ >= pgsz );
	assert( ! ( HNKSZ % pgsz ));
	assert( sizeof( pers_t ) <= PERSSZ );

	/* get inomap info from media hdr
	 */
	hnkcnt = scrhdrp->cih_inomap_hnkcnt;
	segcnt = scrhdrp->cih_inomap_segcnt;
	last_ino_added = scrhdrp->cih_inomap_lastino;

	/* truncate and open the backing store
	 */
	perspath = open_pathalloc( hkdir, PERS_NAME, 0 );
	( void )unlink( perspath );
	fd = open( perspath,
		   O_RDWR | O_CREAT,
		   S_IRUSR | S_IWUSR );
	if ( fd < 0 ) {
		mlog( MLOG_NORMAL | MLOG_ERROR,
		      "could not open %s: %s\n",
		      perspath,
		      strerror( errno ));
		return RV_ERROR;
	}

	/* mmap the persistent hdr and space for the map
	 */
	assert( sizeof( hnk_t ) * ( size_t )hnkcnt >= pgsz );
	assert( ! ( sizeof( hnk_t ) * ( size_t )hnkcnt % pgsz ));
	persp = ( pers_t * ) mmap64( 0,
				     PERSSZ
				     +
				     sizeof( hnk_t ) * ( size_t )hnkcnt,
				     PROT_READ | PROT_WRITE,
				     MAP_SHARED | MAP_AUTOGROW,
				     fd,
				     ( off64_t )0 );
	if ( persp == ( pers_t * )-1 ) {
		mlog( MLOG_NORMAL | MLOG_ERROR,
		      "unable to map %s: %s\n",
		      perspath,
		      strerror( errno ));
		return RV_ERROR;
	}

	/* load the pers hdr
	 */
	persp->hnkcnt = hnkcnt;
	persp->segcnt = segcnt;
	persp->last_ino_added = last_ino_added;

	/* read the map in from media
	 */
	nread = read_buf( ( char * )persp + PERSSZ,
			  sizeof( hnk_t ) * ( size_t )hnkcnt,
			  ( void * )drivep,
			  ( rfp_t )dop->do_read,
			  ( rrbfp_t )dop->do_return_read_buf,
			  &rval );
	/* close up
	 */
	rval1 = munmap( ( void * )persp,
		        PERSSZ
		        +
		        sizeof( hnk_t ) * ( size_t )hnkcnt );
	assert( ! rval1 );
	( void )close( fd );
	free( ( void * )perspath );

	/* check the return code from read
	 */
	switch( rval ) {
	case 0:
		assert( ( size_t )nread == sizeof( hnk_t ) * ( size_t )hnkcnt );
		ok = inomap_sync_pers( hkdir );
		if ( ! ok ) {
			return RV_ERROR;
		}
		return RV_OK;
	case DRIVE_ERROR_EOD:
	case DRIVE_ERROR_EOF:
	case DRIVE_ERROR_EOM:
	case DRIVE_ERROR_MEDIA:
	case DRIVE_ERROR_CORRUPTION:
		return RV_CORRUPT;
	case DRIVE_ERROR_DEVICE:
		return RV_DRIVE;
	case DRIVE_ERROR_CORE:
	default:
		return RV_CORE;
	}
}

/* peels inomap from media
 */
rv_t
inomap_discard( drive_t *drivep, content_inode_hdr_t *scrhdrp )
{
	drive_ops_t *dop = drivep->d_opsp;
	u_int64_t tmphnkcnt;
	/* REFERENCED */
	intgen_t nread;
	intgen_t rval;

	/* get inomap info from media hdr
	 */
	tmphnkcnt = scrhdrp->cih_inomap_hnkcnt;

	/* read the map in from media
	 */
	nread = read_buf( 0,
			  sizeof( hnk_t ) * ( size_t )tmphnkcnt,
			  ( void * )drivep,
			  ( rfp_t )dop->do_read,
			  ( rrbfp_t )dop->do_return_read_buf,
			  &rval );
	/* check the return code from read
	 */
	switch( rval ) {
	case 0:
		assert( ( size_t )nread == sizeof( hnk_t ) * ( size_t )hnkcnt );
		return RV_OK;
	case DRIVE_ERROR_EOD:
	case DRIVE_ERROR_EOF:
	case DRIVE_ERROR_EOM:
	case DRIVE_ERROR_MEDIA:
	case DRIVE_ERROR_CORRUPTION:
		return RV_CORRUPT;
	case DRIVE_ERROR_DEVICE:
		return RV_DRIVE;
	case DRIVE_ERROR_CORE:
	default:
		return RV_CORE;
	}
}

bool_t
inomap_sync_pers( char *hkdir )
{
	char *perspath;
	pers_t *persp;
	hnk_t *hnkp;

	/* sanity checks
	 */
	assert( sizeof( hnk_t ) == HNKSZ );
	assert( HNKSZ >= pgsz );
	assert( ! ( HNKSZ % pgsz ));

	/* only needed once per session
	 */
	if ( pers_fd >= 0 ) {
		return BOOL_TRUE;
	}

	/* open the backing store. if not present, ok, hasn't been created yet
	 */
	perspath = open_pathalloc( hkdir, PERS_NAME, 0 );
	pers_fd = open( perspath, O_RDWR );
	if ( pers_fd < 0 ) {
		return BOOL_TRUE;
	}

	/* mmap the persistent hdr
	 */
	persp = ( pers_t * ) mmap64( 0,
				     PERSSZ,
				     PROT_READ | PROT_WRITE,
				     MAP_SHARED,
				     pers_fd,
				     ( off64_t )0 );
	if ( persp == ( pers_t * )-1 ) {
		mlog( MLOG_NORMAL | MLOG_ERROR,
		      "unable to map %s hdr: %s\n",
		      perspath,
		      strerror( errno ));
		return BOOL_FALSE;
	}

	/* read the pers hdr
	 */
	hnkcnt = persp->hnkcnt;
	segcnt = persp->segcnt;
	last_ino_added = persp->last_ino_added;

	/* mmap the pers inomap
	 */
	assert( hnkcnt * sizeof( hnk_t ) <= ( size64_t )INT32MAX );
	roothnkp = ( hnk_t * ) mmap64( 0,
				       sizeof( hnk_t ) * ( size_t )hnkcnt,
				       PROT_READ | PROT_WRITE,
				       MAP_SHARED,
				       pers_fd,
				       ( off64_t )PERSSZ );
	if ( roothnkp == ( hnk_t * )-1 ) {
		mlog( MLOG_NORMAL | MLOG_ERROR,
		      "unable to map %s: %s\n",
		      perspath,
		      strerror( errno ));
		return BOOL_FALSE;
	}

	/* correct the next pointers
	 */
	for ( hnkp = roothnkp
	      ;
	      hnkp < roothnkp + ( intgen_t )hnkcnt - 1
	      ;
	      hnkp++ ) {
		hnkp->nextp = hnkp + 1;
	}
	hnkp->nextp = 0;

	/* calculate the tail pointers
	 */
	tailhnkp = hnkp;
	assert( hnkcnt > 0 );
	lastsegp = &tailhnkp->seg[ ( intgen_t )( segcnt
						 -
						 SEGPERHNK * ( hnkcnt - 1 )
						 -
						 1 ) ];
	
	/* now all inomap operators will work
	 */
	return BOOL_TRUE;
}

/* de-allocate the persistent inomap
 */
void
inomap_del_pers( char *hkdir )
{
	char *perspath = open_pathalloc( hkdir, PERS_NAME, 0 );
	( void )unlink( perspath );
	free( ( void * )perspath );
}

/* mark all included non-dirs as MAP_NDR_NOREST
 */
void
inomap_sanitize( void )
{
	hnk_t *hnkp;
	seg_t *segp;

	/* step through all hunks, segs, and inos
	 */
	for ( hnkp = roothnkp
	      ;
	      hnkp != 0
	      ;
	      hnkp = hnkp->nextp ) {
		for ( segp = hnkp->seg
		      ;
		      segp < hnkp->seg + SEGPERHNK
		      ;
		      segp++ ) {
			ino64_t ino;
			if ( hnkp == tailhnkp && segp > lastsegp ) {
				return;
			}
			for ( ino = segp->base
			      ;
			      ino < segp->base + INOPERSEG
			      ;
			      ino++ ) {
				intgen_t state;
				if ( ino > last_ino_added ) {
					return;
				}
				SEG_GET_BITS( segp, ino, state );
				if ( state == MAP_NDR_CHANGE ) {
					state = MAP_NDR_NOREST;
					SEG_SET_BITS( segp, ino, state );
				}
			}
		}
	}
}

/* called to mark a non-dir ino as TO be restored
 */
void
inomap_rst_add( ino64_t ino )
{
		assert( pers_fd >= 0 );
		( void )map_set( ino, MAP_NDR_CHANGE );
}

/* called to mark a non-dir ino as NOT to be restored
 */
void
inomap_rst_del( ino64_t ino )
{
		assert( pers_fd >= 0 );
		( void )map_set( ino, MAP_NDR_NOREST );
}

/* called to ask if any inos in the given range need to be restored.
 * range is inclusive
 */
bool_t
inomap_rst_needed( ino64_t firstino, ino64_t lastino )
{
	hnk_t *hnkp;
	seg_t *segp;

	/* if inomap not restored/resynced, just say yes
	 */
	if ( ! roothnkp ) {
		return BOOL_TRUE;
	}

	/* may be completely out of range
	 */
	if ( firstino > last_ino_added ) {
		return BOOL_FALSE;
	}

	/* find the hunk/seg containing first ino or any ino beyond
	 */
	for ( hnkp = roothnkp ; hnkp != 0 ; hnkp = hnkp->nextp ) {
		if ( firstino > hnkp->maxino ) {
			continue;
		}
		for ( segp = hnkp->seg; segp < hnkp->seg + SEGPERHNK ; segp++ ){
			if ( hnkp == tailhnkp && segp > lastsegp ) {
				return BOOL_FALSE;
			}
			if ( firstino < segp->base + INOPERSEG ) {
				goto begin;
			}
		}
	}
	return BOOL_FALSE;

begin:
	/* search until at least one ino is needed or until beyond last ino
	 */
	for ( ; ; ) {
		ino64_t ino;

		if ( segp->base > lastino ) {
			return BOOL_FALSE;
		}
		for ( ino = segp->base ; ino < segp->base + INOPERSEG ; ino++ ){
			intgen_t state;
			if ( ino < firstino ) {
				continue;
			}
			if ( ino > lastino ) {
				return BOOL_FALSE;
			}
			SEG_GET_BITS( segp, ino, state );
			if ( state == MAP_NDR_CHANGE ) {
				return BOOL_TRUE;
			}
		}
		segp++;
		if ( hnkp == tailhnkp && segp > lastsegp ) {
			return BOOL_FALSE;
		}
		if ( segp >= hnkp->seg + SEGPERHNK ) {
			hnkp = hnkp->nextp;
			if ( ! hnkp ) {
				return BOOL_FALSE;
			}
			segp = hnkp->seg;
		}
	}
	/* NOTREACHED */
}

/* calls the callback for all inos with an inomap state included
 * in the state mask. stops iteration when inos exhaused or cb
 * returns FALSE.
 */
void
inomap_cbiter( intgen_t statemask,
	       bool_t ( * cbfunc )( void *ctxp, ino64_t ino ),
	       void *ctxp )
{
	hnk_t *hnkp;
	seg_t *segp;

	/* step through all hunks, segs, and inos
	 */
	for ( hnkp = roothnkp
	      ;
	      hnkp != 0
	      ;
	      hnkp = hnkp->nextp ) {
		for ( segp = hnkp->seg
		      ;
		      segp < hnkp->seg + SEGPERHNK
		      ;
		      segp++ ) {
			ino64_t ino;
			if ( hnkp == tailhnkp && segp > lastsegp ) {
				return;
			}
			for ( ino = segp->base
			      ;
			      ino < segp->base + INOPERSEG
			      ;
			      ino++ ) {
				intgen_t state;
				if ( ino > last_ino_added ) {
					return;
				}
				SEG_GET_BITS( segp, ino, state );
				if ( statemask & ( 1 << state )) {
					bool_t ok;
					ok = ( cbfunc )( ctxp, ino );
					if ( ! ok ) {
						return;
					}
				}
			}
		}
	}
}

/* definition of locally defined static functions ****************************/

/* map_getset - locates and gets the state of the specified ino,
 * and optionally sets the state to a new value.
 */
static intgen_t
map_getset( ino64_t ino, intgen_t newstate, bool_t setflag )
{
	hnk_t *hnkp;
	seg_t *segp;

	if ( ino > last_ino_added ) {
		return MAP_INO_UNUSED;
	}
	for ( hnkp = roothnkp ; hnkp != 0 ; hnkp = hnkp->nextp ) {
		if ( ino > hnkp->maxino ) {
			continue;
		}
		for ( segp = hnkp->seg; segp < hnkp->seg + SEGPERHNK ; segp++ ){
			if ( hnkp == tailhnkp && segp > lastsegp ) {
				return MAP_INO_UNUSED;
			}
			if ( ino < segp->base ) {
				return MAP_INO_UNUSED;
			}
			if ( ino < segp->base + INOPERSEG ) {
				intgen_t state;
				SEG_GET_BITS( segp, ino, state );
				if ( setflag ) {
					SEG_SET_BITS( segp, ino, newstate );
				}
				return state;
			}
		}
		return MAP_INO_UNUSED;
	}
	return MAP_INO_UNUSED;
}

static intgen_t
map_set( ino64_t ino, intgen_t state )
{
	intgen_t oldstate;

 	oldstate = map_getset( ino, state, BOOL_TRUE );
	return oldstate;
}
