#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/restore/RCS/media_simple.c,v 1.14 1995/07/31 19:05:15 cbullis Exp $"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uuid.h>
#include <time.h>
#include <sys/fs/xfs_itable.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "types.h"
#include "jdm.h"
#include "util.h"
#include "mlog.h"
#include "stream.h"
#include "global.h"
#include "drive.h"
#include "media.h"


/* media_simple.c - passthrough media strategy
 */


/* structure definitions used locally ****************************************/

/* context for each stream
 */
struct context {
	size_t mc_begin_read_cnt;
	uuid_t mc_resumemediaid;
	bool_t mc_resumed_restore;
};

typedef struct context context_t;


/* declarations of externally defined global symbols *************************/


/* forward declarations of locally defined static functions ******************/

/* strategy functions
 */
static intgen_t s_match( int, char *[], drive_strategy_t * );
static bool_t s_create( media_strategy_t *msp, int, char *[] );
static bool_t s_init( media_strategy_t *msp, int, char *[] );
static void s_complete( media_strategy_t *msp );

/* manager operators
 */
static bool_t create( media_t * );
static void resume( media_t *, uuid_t *, size_t );
static bool_t init( media_t * );
static intgen_t begin_stream( media_t * );
static size_t get_align_cnt( media_t * );
static intgen_t begin_read( media_t * );
static char *read( media_t *, size_t, size_t *, intgen_t * );
static void return_read_buf( media_t *, char *, size_t );
static intgen_t next_mark( media_t * );
static void end_read( media_t * );
static void end_stream( media_t * );


/* definition of locally defined global variables ****************************/

/* media strategy for stdout. referenced by media.c
 */
media_strategy_t media_strategy_simple = {
	-1,		/*ms_id */
	s_match,	/* ms_match */
	s_create,	/* ms_create */
	s_init,		/* ms_init */
	s_complete,	/* ms_complete */
	0,		/* ms_mediap */
	0,		/* ms_mediacnt */
	0,		/* ms_contextp */
	0,		/* ms_dsp */
	0		/* ms_recmarksep */
};


/* definition of locally defined static variables *****************************/

/* media operators
 */
static media_ops_t media_ops = {
	create,		/* mo_create */
	resume,		/* mo_resume */
	init,		/* mo_init */
	begin_stream,	/* mo_begin_stream */
	0,		/* mo_begin_write */
	0,		/* mo_set_mark */
	0,		/* mo_get_write_buf */
	0,		/* mo_write */
	get_align_cnt,	/* mo_get_align_cnt */
	0,		/* mo_end_write */
	begin_read,	/* mo_begin_read */
	read,		/* mo_read */
	return_read_buf,/* mo_return_read_buf */
	0,		/* mo_get_mark */
	0,		/* mo_seek_mark */
	next_mark,	/* mo_next_mark */
	end_read,	/* mo_end_read */
	end_stream,	/* mo_end_stream */
};


/* definition of locally defined global functions ****************************/


/* definition of locally defined static functions ****************************/

/* strategy match - determines if this is the right strategy
 */
static intgen_t
s_match( int argc, char *argv[], drive_strategy_t *dsp )
{
	size_t driveix;

	/* sanity check on initialization
	 */
	assert( media_strategy_simple.ms_id >= 0 );

	/* check all drive managers; if media removable, no match
	 */
	for ( driveix = 0 ; driveix < dsp->ds_drivecnt; driveix++ ) {
		drive_t *drivep = dsp->ds_drivep[ driveix ];
		drive_ops_t *opsp = drivep->d_opsp;
		intgen_t class = ( * opsp->do_get_device_class )( drivep );
		if ( class != DEVICE_NONREMOVABLE ){
			return 0;
		}
	}

	mlog( MLOG_NITTY,
	      "media_simple strategy match\n" );

	return 1;
}

/* strategy create - initializes the strategy, and each manager
 */
static bool_t
s_create( media_strategy_t *msp, int argc, char *argv[] )
{
	drive_strategy_t *dsp = msp->ms_dsp;
	size_t mediaix;

	/* copy the recommended mark separation value from
	 * the drive strategy
	 */
	msp->ms_recmarksep = dsp->ds_recmarksep;

	/* give each media manager its operators, and call
	 * each manager's create operator
	 */
	for ( mediaix = 0 ; mediaix < msp->ms_mediacnt ; mediaix++ ) {
		media_t *mediap;
		bool_t ok;

		mediap = msp->ms_mediap[ mediaix ];
		mediap->m_opsp = &media_ops;
		ok = ( * mediap->m_opsp->mo_create )( mediap );
		if ( ! ok ) {
			return BOOL_FALSE;
		}
	}

	return BOOL_TRUE;
}

/* strategy init - second pass init
 */
static bool_t
s_init( media_strategy_t *msp, int argc, char *argv[] )
{
	size_t mediaix;

	/* call each manager's init operator
	 */
	for ( mediaix = 0 ; mediaix < msp->ms_mediacnt ; mediaix++ ) {
		media_t *mediap;
		bool_t ok;

		mediap = msp->ms_mediap[ mediaix ];
		ok = ( * mediap->m_opsp->mo_init )( mediap );
		if ( ! ok ) {
			return BOOL_FALSE;
		}
	}

	return BOOL_TRUE;
}


/* strategy completion - not used
 */
void
s_complete( struct media_strategy *msp )
{
}

/* create - media stream initialization first pass
 *
 * initialization which does not require drive I/O
 */
static bool_t
create( struct media *mediap )
{
	context_t *contextp;

	contextp = ( context_t * )calloc( 1, sizeof( context_t ));
	assert( contextp );
	mediap->m_contextp = ( void * )contextp;

	return BOOL_TRUE;
}


/* receives resume info from the content layer
 */
static void
resume( media_t *mediap, uuid_t *mediaidp, size_t mediaix )
{
	context_t *contextp = ( context_t * )mediap->m_contextp;

	contextp->mc_resumemediaid = *mediaidp;
	contextp->mc_resumed_restore = BOOL_TRUE;
}


/* init - media stream initialization second pass
 *
 * tell the drive to begin reading. this will cause the media file
 * header to be read.
 */
static bool_t
init( struct media *mediap )
{
	context_t *contextp = ( context_t * )mediap->m_contextp;
	drive_t *drivep = mediap->m_drivep;

	/* pass this straight through to the drive manager,
	 * since there is no media management to do.
	 */
	if ( ( * drivep->d_opsp->do_begin_stream )( drivep )) {
		return BOOL_FALSE;
	}

	if ( ( * drivep->d_opsp->do_begin_read )( drivep )) {
		return BOOL_FALSE;
	}

	/* validate the strategy id
	 */
	if ( mediap->m_readhdrp->mh_strategyid != mediap->m_strategyp->ms_id ) {
		mlog( MLOG_NORMAL,
		      "unrecognized media strategy ID (%d)\n",
		      mediap->m_readhdrp->mh_strategyid );
		return BOOL_FALSE;
	}

	/* if a resumed restore, besure the media id matches
	 */
	if ( contextp->mc_resumed_restore ) {
		u_int32_t uuidstat;
		if ( ! uuid_equal( &mediap->m_readhdrp->mh_mediaid,
				   &contextp->mc_resumemediaid,
				   &uuidstat )) {
			mlog( MLOG_NORMAL,
			      "dump being resumed is not "
			      "the dump being restored when interrupted\n" );
			return BOOL_FALSE;
		}
	}

	/* use the context pointer to record how many times
	 * begin_read is called.
	 */
	contextp->mc_begin_read_cnt = 0;

	return BOOL_TRUE;
}


/* get_align_cnt - return the number of bytes which must be read
 * such that the next read will be page-aligned.
 */
static size_t
get_align_cnt( media_t *mediap )
{
	drive_t *drivep = mediap->m_drivep;
	return ( * drivep->d_opsp->do_get_align_cnt )( drivep );
}

/* begin a new stream : not used
 */
intgen_t
begin_stream( media_t *mediap )
{
	return 0;
}

/* begin_read - prepare the media for reading
 *
 * first already done during initialization
 */
static intgen_t
begin_read( media_t *mediap )
{
	context_t *contextp = ( context_t * )mediap->m_contextp;
	global_hdr_t *grhdrp = mediap->m_greadhdrp;
	drive_t *drivep = mediap->m_drivep;
	drive_ops_t *dop = drivep->d_opsp;
	intgen_t rval;

	if ( contextp->mc_begin_read_cnt ) {
		u_int32_t uuidstat;
		if ( dop->do_change_media ) {
			uuid_t dumpid;

			rval = ( * dop->do_change_media )( drivep, "" );
			if ( rval == DRIVE_ERROR_EOD ) {
				return MEDIA_ERROR_EOD;
			} else if ( rval ) {
				return MEDIA_ERROR_ABORT;
			}

			dumpid = grhdrp->gh_dumpid;
			rval = ( * dop->do_begin_read )( drivep );
			if ( rval == DRIVE_ERROR_EOD ) {
				return MEDIA_ERROR_EOD;
			} else if ( rval ) {
				return MEDIA_ERROR_ABORT;
			} else if ( ! uuid_equal( &dumpid,
						  &grhdrp->gh_dumpid,
						  &uuidstat )) {
				mlog( MLOG_NORMAL,
				      "new media file not from "
				      "same dump session: aborting\n" );
				return MEDIA_ERROR_ABORT;
			} else {
				assert( uuidstat == uuid_s_ok );
				return 0;
			}
		} else {
			return MEDIA_ERROR_EOD;
		}
	} else {
		contextp->mc_begin_read_cnt = 1;
		return 0;
	}
}

/* read - supply the caller with some filled buffer
 */
static char *
read( media_t *mediap,
      size_t wanted_bufsz,
      size_t *actual_bufszp,
      intgen_t *statp )
{
	drive_t *drivep = mediap->m_drivep;
	char *bufp;

	bufp = ( * drivep->d_opsp->do_read )( drivep,
					      wanted_bufsz,
					      actual_bufszp,
					      statp );
	switch ( *statp ) {
	case 0:
		break;
	case DRIVE_ERROR_EOF:
		*statp = MEDIA_ERROR_EOF;
		break;
	case DRIVE_ERROR_EOM:
	case DRIVE_ERROR_EOD:
		*statp = MEDIA_ERROR_EOD;
		break;
	default:
		*statp = MEDIA_ERROR_ABORT;
		break;
	}
	return bufp;
}

/* return_read_buf - lets the caller give back all or a portion of the
 * buffer obtained from a call to do_read().
 */
static void
return_read_buf( media_t *mediap, char *bufp, size_t bufsz )
{
	drive_t *drivep = mediap->m_drivep;

	( * drivep->d_opsp->do_return_read_buf )( drivep, bufp, bufsz );
}

static intgen_t
next_mark( media_t *mediap )
{
	drive_t *drivep = mediap->m_drivep;
	drive_mark_t dmark;
	intgen_t rval;

	rval = ( * drivep->d_opsp->do_next_mark )( drivep, &dmark );

	switch (rval) {
	case 0:
		return 0;
	case DRIVE_ERROR_EOF:
		return MEDIA_ERROR_EOF;
	case DRIVE_ERROR_EOD:
		return  MEDIA_ERROR_EOD;
	case DRIVE_ERROR_EOM:
		return MEDIA_ERROR_EOM;
	case DRIVE_ERROR_DEVICE:
		return  MEDIA_ERROR_ABORT;
	default:
		assert( 0 );
		return  MEDIA_ERROR_ABORT;
	}
}
/* end_read - tell the media manager we are done reading the media file
 * just discards any buffered data
 */
void
end_read( media_t *mediap )
{
	drive_t *drivep = mediap->m_drivep;

	( * drivep->d_opsp->do_end_read )( drivep );
}

void
end_stream( media_t *mediap )
{
	drive_t *drivep = mediap->m_drivep;

	( * drivep->d_opsp->do_end_stream )( drivep );
}
