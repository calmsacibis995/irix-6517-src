#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/dump/RCS/media_simple.c,v 1.12 1994/11/02 02:04:28 cbullis Exp $"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uuid.h>
#include <time.h>
#include <sys/fs/xfs_itable.h>
#include <stdio.h>
#include <memory.h>
#include <assert.h>

#include "types.h"
#include "jdm.h"
#include "util.h"
#include "stream.h"
#include "global.h"
#include "drive.h"
#include "media.h"
#include "mlog.h"


/* media_simple.c - passthrough media strategy
 */


/* structure definitions used locally ****************************************/


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
static bool_t init( media_t * );
static intgen_t begin_stream( media_t * );
static intgen_t begin_write( media_t * );
static void set_mark( media_t *, media_mcbfp_t, void *, media_mark_t * );
static char *get_write_buf( media_t *, size_t, size_t * );
static intgen_t write( media_t *, char *, size_t );
static size_t get_align_cnt( media_t * );
static intgen_t end_write( media_t *, off64_t * );
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
	0,		/* ms_recmarksep */
	0,		/* ms_recmfilesz */
};


/* definition of locally defined static variables *****************************/

/* media operators
 */
static media_ops_t media_ops = {
	create,		/* mo_create */
	0,		/* mo_resume */
	init,		/* mo_init */
	begin_stream,	/* mo_begin_stream */
	begin_write,	/* mo_begin_write */
	set_mark,	/* mo_set_mark */
	get_write_buf,	/* mo_get_write_buf */
	write,		/* mo_write */
	get_align_cnt,	/* mo_get_align_cnt */
	end_write,	/* mo_end_write */
	0,		/* mo_begin_read */
	0,		/* mo_read */
	0,		/* mo_return_read_buf */
	0,		/* mo_get_mark */
	0,		/* mo_seek_mark */
	0,		/* mo_next_mark */
	0,		/* mo_end_read */
	end_stream	/* mo_end_stream */
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

/* strategy create - first pass strategy init: no I/O
 */
static bool_t
s_create( media_strategy_t *msp, int argc, char *argv[] )
{
	size_t mediaix;

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


/* strategy create - second pass strategy init: I/O allowed
 */
static bool_t
s_init( media_strategy_t *msp, int argc, char *argv[] )
{
	drive_strategy_t *dsp = msp->ms_dsp;
	size_t mediaix;

	/* copy the recommended mark media file size from
	 * the drive strategy
	 */
	msp->ms_recmfilesz = dsp->ds_recmfilesz;

	/* copy the recommended mark separation value from
	 * the drive strategy
	 */
	msp->ms_recmarksep = dsp->ds_recmarksep;

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


/* init - media stream initialization pass 1
 */
static bool_t
create( struct media *mediap )
{
	return BOOL_TRUE;
}


/* init - media stream initialization pass 2
 */
static bool_t
init( struct media *mediap )
{
	return BOOL_TRUE;
}


/* begin a new stream
 */
intgen_t
begin_stream( media_t *mediap )
{
	drive_t *drivep = mediap->m_drivep;
	intgen_t rval;

	rval = ( * drivep->d_opsp->do_begin_stream )( drivep );
	if ( rval ) {
		return MEDIA_ERROR_ABORT;
	}
	return 0;
}


/* begin_write - prepare the media for writing
 */
static intgen_t
begin_write( media_t *mediap )
{
	media_hdr_t *mwhdrp = mediap->m_writehdrp;
	drive_t *drivep = mediap->m_drivep;
	u_int32_t uuidstat;
	intgen_t rval;

	rval = ( * drivep->d_opsp->do_begin_write )( drivep );
	switch ( rval ) {
	case DRIVE_ERROR_EOM:
		if ( ! drivep->d_opsp->do_change_media ) {
			return MEDIA_ERROR_ABORT;
		}
		rval = ( * drivep->d_opsp->do_change_media )( drivep, "" );
		if ( rval ) {
			return MEDIA_ERROR_ABORT;
		}
		memcpy( mwhdrp->mh_medialabel,
			mwhdrp->mh_prevmedialabel,
			sizeof( mwhdrp->mh_prevmedialabel ));
		mwhdrp->mh_prevmediaid = mwhdrp->mh_mediaid;
		uuid_create( &mwhdrp->mh_mediaid, &uuidstat );
		assert( uuidstat == uuid_s_ok );
		mwhdrp->mh_mediaix++;
		mwhdrp->mh_dumpmediafileix++;
		rval = ( * drivep->d_opsp->do_begin_write )( drivep );
		if ( rval ) {
			return MEDIA_ERROR_ABORT;
		}
		return 0;
	case DRIVE_ERROR_CORE:
		return MEDIA_ERROR_CORE;
	case 0:
		return 0;
	default:
		return MEDIA_ERROR_ABORT;
	}
}

/* set_mark - forward the caller's set_mark request to the
 * drive manager. intercept the callback, in order to doctor
 * the media_marklog_t before the caller sees it.
 */
static void
markcb( void *contextp, drive_mark_t *dmp, bool_t committed )
{
	media_mark_t *mmp = ( media_mark_t * )dmp;

	assert( dmp == &mmp->mm_drivemark );

	/* load the drive managers mark log into mine; the caller will
	 * log the whole thing.
	 */
	mmp->mm_log.mml_drivelog = dmp->dm_log;

	/* invoke the caller's callback
	 */
	( * mmp->mm_cbfuncp )( mmp->mm_cbcontextp, mmp, committed );
}

static void
set_mark( media_t *mediap,
	  media_mcbfp_t cbfuncp,
	  void *cbcontextp,
	  media_mark_t *markp )
{
	drive_t *drivep = mediap->m_drivep;

	/* load up the mark
	 */
	markp->mm_cbfuncp = cbfuncp;
	markp->mm_cbcontextp = cbcontextp;

	/* call the drive manager to set a drive mark
	 */
	( * drivep->d_opsp->do_set_mark )( drivep,
					   markcb,
					   mediap,
					   ( drive_mark_t * )markp );
}

/* get_write_buf - supply the caller with buffer space. the caller
 * will fill the space with data, then call write() to request that
 * some or all of the buffer be written and to return the buffer space.
 */
static char *
get_write_buf( media_t *mediap, size_t wanted_bufsz, size_t *actual_bufszp )
{
	drive_t *drivep = mediap->m_drivep;
	char *bufp;

	bufp = ( * drivep->d_opsp->do_get_write_buf )( drivep,
						       wanted_bufsz,
						       actual_bufszp );
	return bufp;
}

/* write - write the portion of the buffer filled by the caller,
 * and accept ownership of the buffer lended to the caller by get_write_buf()
 */
static intgen_t
write( media_t *mediap, char *bufp, size_t writesz )
{
	drive_t *drivep = mediap->m_drivep;
	intgen_t rval;

	rval = ( * drivep->d_opsp->do_write )( drivep, bufp, writesz );
	switch( rval ) {
	case 0:
		return 0;
	case DRIVE_ERROR_EOM:
		return MEDIA_ERROR_EOM;
	default:
		return MEDIA_ERROR_ABORT;
	}
}

/* get_align_cnt - return the number of bytes which must be written
 * such that the next write will be page-aligned.
 */
static size_t
get_align_cnt( media_t *mediap )
{
	drive_t *drivep = mediap->m_drivep;
	return ( * drivep->d_opsp->do_get_align_cnt )( drivep );
}

/* end_write - flush any buffer data
 */
static intgen_t
end_write( media_t *mediap, off64_t *ncommittedp )
{
	drive_t *drivep = mediap->m_drivep;
	intgen_t rval;

	rval = ( * drivep->d_opsp->do_end_write )( drivep, ncommittedp );
	switch( rval ) {
	case 0:
		return 0;
	case DRIVE_ERROR_EOM:
		return MEDIA_ERROR_EOM;
	default:
		return MEDIA_ERROR_ABORT;
	}
}

static void
end_stream( media_t *mediap )
{
	drive_t *drivep = mediap->m_drivep;

	( * drivep->d_opsp->do_end_stream )( drivep );
}
