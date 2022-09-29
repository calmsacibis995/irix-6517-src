#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/dump/RCS/media_rmvtape.c,v 1.51 1994/12/07 01:17:48 cbullis Exp $"

/*
 * dump side media layer driver to support removable tape medi
 *
 */
#include <bstring.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uuid.h>
#include <sys/dirent.h>
#include <fcntl.h>
#include <time.h>
#include <sys/fs/xfs_itable.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "types.h"
#include "jdm.h"
#include "util.h"
#include "stream.h"
#include "dlog.h"
#include "global.h"
#include "drive.h"
#include "media.h"
#include "media_rmvtape.h"
#include "mlog.h"


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
static intgen_t mwrite( media_t *, char *, size_t );
static size_t get_align_cnt( media_t * );
static intgen_t end_write( media_t *, off64_t * );
static void end_stream( media_t * );


/* definition of locally defined global variables ****************************/

/* media strategy for stdout. referenced by media.c
 */
media_strategy_t media_strategy_rmvtape = {
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
	mwrite,		/* mo_write */
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
static intgen_t read_next_file_header( struct media *, int *); 
static intgen_t advance_to_end_of_dump( struct media *); 
static intgen_t reached_eod( struct media *, int *);
static intgen_t reached_eof( struct media *, int *);
static intgen_t reached_eom( struct media *);
static void change_media_label(media_t *mediap);

/* strategy match - determines if this is the right strategy
 */
static intgen_t
s_match( int argc, char *argv[], drive_strategy_t *dsp )
{
	int dclass, ret = 1;
	size_t i;

	/* sanity check on initialization
	 */
	assert( media_strategy_rmvtape.ms_id >= 0 );

	assert( dsp->ds_drivecnt == 1);
	for (i = 0 ; i < dsp->ds_drivecnt; i++) {
		dclass = dsp->ds_drivep[i]->d_opsp->do_get_device_class( 
			dsp->ds_drivep[i]);
		if (dclass != DEVICE_TAPE_REMOVABLE) {
			ret = 0;
		}
	}
	
	return ret;
}

/* strategy create - initializes the strategy, and each manager
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


/* strategy init - second pass strategy init
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


/* create - media stream nit first pass
 */
static bool_t
create( struct media *mediap )
{
	uint_t			status;
	media_hdr_t		*mreadhdrp;
	global_hdr_t		*greadhdrp;
	media_context_t		*contextp;

	contextp = (media_context_t *)memalign( PGSZ, sizeof( media_context_t));
	mediap->m_contextp = (void *)contextp;

	greadhdrp   = mediap->m_greadhdrp;
	mreadhdrp   = mediap->m_readhdrp;

	/* initialize dump and media ids to nil
	 */
	uuid_create_nil( &greadhdrp->gh_dumpid, &status);
	uuid_create_nil( &mreadhdrp->mh_mediaid, &status);

	return BOOL_TRUE;
}


/* init - media stream initialization second pass
 */
static bool_t
init( struct media *mediap )
{
	drive_t  		*drivep = mediap->m_drivep;
	intgen_t		rval;

	if ( ( * drivep->d_opsp->do_begin_stream )( drivep )) {
		return BOOL_FALSE;
	}

	rval = advance_to_end_of_dump( mediap );
	if ( rval == 0 ) {
		return BOOL_TRUE;
	} else {
		return BOOL_FALSE;
	}
}


/* begin a new stream : not used
 */
intgen_t
begin_stream( media_t *mediap )
{
	return 0;
}


/*
 * advance_to_end_of_dump()
 *
 *	This routine is called when a media object is inserted to
 *	position the device pointer at a location where the current dump 
 * 	can be written.
 *
 *
 * RETURNS:
 *	0 on success
 *	MEDIA_ERROR_* on failure
 *
 */
static intgen_t
advance_to_end_of_dump( struct media *mediap )
{

	int			fmarks, tapes;
	char			*str;
	uint_t			status;
	drive_t  		*drivep = mediap->m_drivep;
	intgen_t		rval, done;
	drive_ops_t		*dops = drivep->d_opsp;
	global_hdr_t		*gwritehdrp;
	media_hdr_t		*mreadhdrp, *mwritehdrp;
	media_context_t		*contextp;


	mlog( MLOG_DEBUG,
	      "advancing to end of last media file\n" );

	/* setup media label pointers
	 */
	gwritehdrp  = mediap->m_gwritehdrp;
	mreadhdrp   = mediap->m_readhdrp;
	mwritehdrp  = mediap->m_writehdrp;
	contextp    = mediap->m_contextp;


	/* get index of the current tape in the dump stream
	 */
	tapes = (int)mwritehdrp->mh_mediaix;

	rval = done = 0;
	fmarks = 0;

	/* advance the tape until end of data is reached.
	 * new dumps can only be appended to the end of the tape
	 */
	while ( !done ) {
		rval = read_next_file_header( mediap, &fmarks );
		switch ( rval ) {

		/* reach EOD or a terminator.
		 */
		case MEDIA_ERROR_EOD:
			rval = reached_eod( mediap, &fmarks );
			done = 1;
			if ((!fmarks) && (tapes)) {
				/* On a blank tape, other than the first, 
				 * prompt user to enter a new media label 
			 	 */
				change_media_label( mediap );
			} 
			break;

		/* dump file exists  - uuids do not match
		 */
		case MEDIA_ERROR_EOF:
			if ( (rval = reached_eof( mediap, &fmarks)) != 0 )
				done = 1;
			break;

		/* Reached end of tape, so change tapes.
		 */
		case MEDIA_ERROR_EOM:
			if ( (rval = reached_eom( mediap )) != 0 )
				done = 1;
			fmarks = 0;
			tapes++;
			break;

		/* no previous dump.
		 */
		case MEDIA_ERROR_CORRUPTION:
			if ((!fmarks) && (tapes)) {
				mlog( MLOG_VERBOSE,
					"no previous dumps "
					"on tape\n");

				/* On a tape without XFS dumps,
				 * prompt user to enter a new media label 
			 	 */
				change_media_label( mediap );
				dops->do_rewind( drivep );
				rval = 0;
			} else if ( (!fmarks) ) {
				mlog( MLOG_VERBOSE,
					"no previous dumps "
					"on tape\n");
				dops->do_rewind( drivep );
				rval = 0;
			} else {
				mlog( MLOG_NORMAL,
					"WARNING: the last media file on this tape "
					"may be corrupted\n");

				mlog( MLOG_NORMAL,
					"WARNING: file mark at the end of dump "
					"cannot be located\n");
			
				mlog( MLOG_NORMAL,
					"WARNING: no more dumps may be added to "
					"this tape\n");
				rval = MEDIA_ERROR_ABORT;
			}
			done = 1;
			break;

		/* tape media error
		 */
		case MEDIA_ERROR_ABORT:
			done = 1;
			if ( fmarks ) {
				rval = MEDIA_ERROR_ABORT;
			} else {
				mlog (MLOG_VERBOSE,
					"no dumps on this tape\n");
				rval = 0;
			}
			break;
		case MEDIA_ERROR_CORE:
			done = 1; 
			break;
		default:
			assert( 0 );
			break;
		}
	}

	/* If a dump is being initiated, fix up the
	 * the media write header information.
	 */
	if ( !rval )  {
	
		/* This is the index of this file within this 
		 * media object (tape).
		 */
		mwritehdrp->mh_mediafileix = ( size_t )fmarks;


#ifdef DEBUG
		if ((fmarks) & (mreadhdrp->mh_mediafileix != (fmarks))) {
			mlog(MLOG_DEBUG,
				"advance to end of dump: "
				"hdr has wrong fmark count\n");
			mlog(MLOG_DEBUG,
				"fmarks = %d, "
				"mreadhdrp->mh_mediafileix = %d\n",
				fmarks,
				mreadhdrp->mh_mediafileix);
		}
#endif


		/* This is the index of the media object (tape) 
		 * within this dump stream.
		 */
		mwritehdrp->mh_mediaix = ( size_t )tapes;

		if (fmarks || (tapes - ((int)mwritehdrp->mh_mediaix)) ) {
			mwritehdrp->mh_dumpmediaix = 
				mreadhdrp->mh_dumpmediaix + 1;
		} else {
			mwritehdrp->mh_dumpmediaix = 0;
		}

#ifdef DEBUG
		mlog(MLOG_DEBUG,
		     "advance to end of dump: tape = %d, file = %d\n", 
		     tapes, fmarks);
		mlog(MLOG_DEBUG,
		     "index of tape object within dump stream: %d\n",
		     mwritehdrp->mh_mediaix);
		mlog(MLOG_DEBUG,
		     "index of media file within tape object: %d\n",
		     mwritehdrp->mh_mediafileix);
		mlog(MLOG_DEBUG,
		     "index of media file within dump stream: %d\n",
		     mwritehdrp->mh_dumpfileix);
		mlog(MLOG_DEBUG,
		     "index of media file within dump stream "
		     "and tape object: %d\n",
		     mwritehdrp->mh_dumpmediafileix);
		mlog(MLOG_DEBUG,
		     "index of dump stream within tape object: %d\n",
		     mwritehdrp->mh_dumpmediaix);
#endif

		/* fix up the writehdrp, if necessary,
		 * to use existing media id and label.
		 */
		if ( (fmarks || tapes) && 
		     (!(uuid_is_nil(&(contextp->mc_mediaid), &status)) ) ) {

			COPY_UUID(contextp->mc_mediaid, 
				mwritehdrp->mh_mediaid);
			COPY_LABEL(contextp->mc_medialabel, 
				mwritehdrp->mh_medialabel);
		} else {
			/* this is done to initialize the context
			 * for the first write to tape.
			 */
			COPY_UUID(mwritehdrp->mh_mediaid, 
				contextp->mc_mediaid);
			COPY_LABEL(mwritehdrp->mh_medialabel, 
				contextp->mc_medialabel);
		}

		COPY_UUID(gwritehdrp->gh_dumpid, contextp->mc_dumpid);
		COPY_LABEL(gwritehdrp->gh_dumplabel, contextp->mc_dumplabel);



		/* Display the name of the session.
		 */
		if ( strcmp(gwritehdrp->gh_dumplabel,"") != 0 ) {
			mlog( MLOG_DEBUG,
				"session label: \"%s\"\n",
					gwritehdrp->gh_dumplabel);
		} else {
				mlog( MLOG_DEBUG,
				"no session label\n");
		}
	
		/* Display the uuid of the session.
		 */
		if ( !uuid_is_nil(&gwritehdrp->gh_dumpid, &status ) ) {
			uuid_to_string(&gwritehdrp->gh_dumpid,
				&str,
				&status);
			mlog( MLOG_DEBUG,
				"the session id: %s\n",
				str);
			free( ( void * )str );
		} else {
			mlog( MLOG_DEBUG,
				"no session id\n");
		}

		/* Display the name of the media.
		 */
		if ( strcmp(mwritehdrp->mh_medialabel,"") != 0 ) {
			mlog( MLOG_DEBUG,
				"media label: \"%s\"\n",
				mwritehdrp->mh_medialabel);
		} else {
			mlog( MLOG_DEBUG,
				"no media label\n");
		}

		/* Display the uuid of the media.
		 */
		if ( !uuid_is_nil(&mwritehdrp->mh_mediaid, &status ) ) {
			uuid_to_string(&mwritehdrp->mh_mediaid,
				&str,
				&status);
			mlog( MLOG_DEBUG,
				"media id: %s\n",
				str);
			free( ( void * )str );
		} else {
			mlog( MLOG_DEBUG,
				"no media id\n");
		}
	}

	return ( rval );
}

/* reached_eom()
 *	Handle the end-of-media tape condition.
 *	Rewind the tape and prompt the user for another.
 *	
 * RETURNS:
 *	0 on success
 *	MEDIA_ERROR_ABORT on failure
 *
 */
static intgen_t
reached_eom( struct media *mediap)
{
	drive_t  	*drivep = mediap->m_drivep;
	intgen_t 	rval;
	drive_ops_t	*dops = drivep->d_opsp;

	mlog( MLOG_NORMAL,
		"reached end of tape\n");

	if (rval = dops->do_rewind( drivep ) ) {
		mlog(MLOG_NORMAL,
			"WARNING: rewind media failed\n");
		rval = MEDIA_ERROR_ABORT;
	} else if ( rval = dops->do_change_media(drivep,(char *)0)) {
		if (rval == DRIVE_ERROR_TIMEOUT) {
			mlog(MLOG_TRACE,
				"media change failed, due to timeout\n");
			rval = MEDIA_ERROR_TIMEOUT;
		} else if (rval == DRIVE_ERROR_STOP) {
			mlog(MLOG_TRACE,
				"media change declined\n");
			rval = MEDIA_ERROR_STOP;
		} else {
			mlog(MLOG_TRACE,
				"media change failed\n");
			rval = MEDIA_ERROR_ABORT;
		}
	}
	return( rval );
}


/* reached_eod()
 *	Handle the end-of-data tape condition ( or termintor ).
 *	If the drive supports overwriting, position the tape immediately
 *	before the terminator, otherwise position the tape at EOD.
 *
 *
 * RETURNS:
 *	0 on success
 *	MEDIA_ERROR_ABORT on failure
 */
static intgen_t
reached_eod( struct media *mediap, int *fmarks)
{
	uint_t			status = 0;
	drive_t  		*drivep = mediap->m_drivep;
	intgen_t		rval;
	drive_ops_t		*dops = drivep->d_opsp;
	media_hdr_t		*mreadhdrp;
	media_context_t		*contextp;
	media_rmvtape_spec_t    *rmv_hdrp;


	/* setup media label pointers
	 */
	mreadhdrp  = mediap->m_readhdrp;
	rmv_hdrp   = (media_rmvtape_spec_t *)&(mreadhdrp->mh_specific);
	contextp   = (media_context_t *)mediap->m_contextp;

	/* Determine if the tape reached EOD or found a terminator.
	 */
	if  ( TERM_IS_SET( rmv_hdrp ) ) {
		mlog( MLOG_VERBOSE,
			"dump terminator found\n");
		/* If this dump is being created via RMT
		 * the drive layer does not know to set the OVERWRITE bit.
		 * Since a TERMINATOR already exists on the tape, the tape
		 * drive supports overwrite.
		 */
		drivep->d_capabilities |= DRIVE_CAP_OVERWRITE;
	} else {
		mlog( MLOG_VERBOSE,
			"end of data found\n");
	}

	/* there are other dumps already on this tape so 
	 * keep the media id and label.
	 */
	if ( *fmarks ) {
		COPY_UUID(mreadhdrp->mh_mediaid, contextp->mc_mediaid);
		COPY_LABEL(mreadhdrp->mh_medialabel, contextp->mc_medialabel);
	}

	/* position the tape to just
	 * before the terminator, or at EOD 
	 */
	if (CAN_OVERWRITE( drivep ) ) {
		if ( (*fmarks) ) {
			mlog( MLOG_VERBOSE,
			      "backspacing to overwrite terminator\n" );
			if ( dops->do_bsf( drivep, 1, 
					(intgen_t *)&status ) == 1 ) {
				rval = 0;
			} else {
				mlog( MLOG_DEBUG,
					"bsf error\n");
				rval = MEDIA_ERROR_ABORT;
			}
			*fmarks--;
		} else {
			if ( dops->do_rewind( drivep ) ) {
				mlog( MLOG_DEBUG,
					"rewind error\n");
				rval = MEDIA_ERROR_ABORT;
			}
		}
	} else {
		if ( (rval = dops->do_feod( drivep)) ) {
			mlog( MLOG_DEBUG,
				"feod error\n");
			rval = MEDIA_ERROR_ABORT;
		}
	}
	return( rval );
}

/* reached_eof()
 *	Handle the end-of-file tape condition.
 *	A dump file is already present on the tape. Check for duplicate
 *	dump labels, and existing media labels.	Advance the tape to the 
 *	next file mark.
 *
 * RETURNS:
 *	0 on success
 *	MEDIA_ERROR_ABORT on failure
 */
static intgen_t
reached_eof( struct media *mediap, int *fmarks)
{
	drive_t  		*drivep = mediap->m_drivep;
	intgen_t		rval = 0,  istat;
	drive_ops_t		*dops = drivep->d_opsp;
	global_hdr_t		*greadhdrp, *gwritehdrp;
	media_hdr_t		*mreadhdrp, *mwritehdrp;
	media_context_t		*contextp;

	/* setup media label pointers
	 */
	greadhdrp  	= mediap->m_greadhdrp;
	mreadhdrp  	= mediap->m_readhdrp;
	gwritehdrp  	= mediap->m_gwritehdrp;
	mwritehdrp  	= mediap->m_writehdrp;
	contextp 	= (media_context_t *)mediap->m_contextp;

	/* media id exists for this tape - save it.
	 */
/* BUGLY: THIS DOES NOT NEED TO BE DONE HERE
 *	IT DONE IN REACHED EOD.
 */
	COPY_UUID(mreadhdrp->mh_mediaid, contextp->mc_mediaid);
	COPY_LABEL(mreadhdrp->mh_medialabel, contextp->mc_medialabel);
	
	/* Check for a duplicate dump label.
	 */
	if ( strcmp(greadhdrp->gh_dumplabel,"") != 0 ) {
		if ( strcmp(greadhdrp->gh_dumplabel, 
				gwritehdrp->gh_dumplabel) == 0 ) {

			mlog( MLOG_NORMAL,
				"duplicate dump label \"%s\"\n",
				gwritehdrp->gh_dumplabel);

			rval = 	MEDIA_ERROR_ABORT;
			return( rval );
		}
	}

	/* Check for a matching media label.
	 */
	if (( strcmp(mreadhdrp->mh_medialabel,"") != 0 )  &&
	    ( strcmp(mwritehdrp->mh_medialabel,"") != 0 ) ) {
		if ( strcmp(mreadhdrp->mh_medialabel, 
				mwritehdrp->mh_medialabel) != 0 ) {

			mlog( MLOG_NORMAL,
				"media label mismatch: \n");

			mlog( MLOG_NORMAL,
				"previous label \"%s\" \n",
				mreadhdrp->mh_medialabel);

			mlog( MLOG_NORMAL,
				"new label \"%s\" \n",
				mwritehdrp->mh_medialabel);

			rval = 	MEDIA_ERROR_ABORT;
			return( rval );
		}
	}

	/* move on to next file
	 */
	if ( dops->do_fsf( drivep, 1, &istat) != 1 ) {
		mlog( MLOG_DEBUG,
			"fsf did not advance a file\n");

#ifdef NOTNOW
		/* if there is supposed to be a terminator
		 * at the end of the tape then the fsf command
		 * should not fail.
		 */
		if ( CAN_OVERWRITE( drivep ) ) {
			mlog( MLOG_NORMAL,
			 	"this tape does not have a terminator record\n");
			rval = MEDIA_ERROR_ABORT;
		}
#endif
	} else {
		(*fmarks)++;
	}

	return( rval );
}

/*
 * read_next_file_header()
 *	The tape is always positioned at a file mark or at BOT when
 *	this routine is called. It tries to read the dump file header at
 *	the current tape position. If a dump file exists, the tape is 
 *	advanced until the next dump session.
 *
 * RETURNS:
 * 		MEDIA_ERROR_EOM		- end of media
 * 		MEDIA_ERROR_EOF		- end of current dump session
 * 		MEDIA_ERROR_EOD		- found terminator
 * 		MEDIA_ERROR_CORRUPTION	- blank tape
 *		MEDIA_ERROR_ABORT	- error
 */
static intgen_t
read_next_file_header( struct media *mediap, int *fmarks ) 
{
	uint_t			status;
	uuid_t			dumpid, mediaid;
	intgen_t		rval, done, istat;
	drive_t  		*drivep = mediap->m_drivep;
	global_hdr_t		*greadhdrp;
	media_hdr_t		*mreadhdrp;
	media_rmvtape_spec_t	*rmv_hdrp;

	status = 0;
	istat = done = 0;

	/* setup media label pointers
	 */
	greadhdrp  = mediap->m_greadhdrp;
	mreadhdrp  = mediap->m_readhdrp;
	rmv_hdrp   = (media_rmvtape_spec_t *)&(mreadhdrp->mh_specific);

	/* copy the current media and dump uuids
	 */
	COPY_UUID(mreadhdrp->mh_mediaid, mediaid);
	COPY_UUID(greadhdrp->gh_dumpid, dumpid);

	/* find the start of the next dump session on the tape
	 */
	while ( !done ) {

		mlog( MLOG_DEBUG,
			"reading tape file header\n");

		/* read the file header from tape
		 */
		rval = drivep->d_opsp->do_begin_read( drivep);
		if ( ! rval ) {
			char *str;
			mlog( MLOG_TRACE,
			      "media file encountered:\n" );
			mlog( MLOG_TRACE,
			      "media label: \"%s\"\n",
			      mreadhdrp->mh_medialabel );
			mlog( MLOG_TRACE,
			      "dump label: \"%s\"\n",
			      greadhdrp->gh_dumplabel );
			uuid_to_string(&greadhdrp->gh_dumpid,
				&str,
				&status);
			mlog( MLOG_TRACE,
			      "dump id: \"%s\"\n",
			      str);
			free( ( void * )str );
			mlog( MLOG_TRACE,
			      "media file index within media object: %u\n",
			      mreadhdrp->mh_mediafileix );
			mlog( MLOG_TRACE,
			      "index of dump within media object: %u\n",
			      mreadhdrp->mh_dumpmediaix );
			mlog( MLOG_TRACE,
			      "media object index within dump stream: %u\n",
			      mreadhdrp->mh_mediaix );
			mlog( MLOG_TRACE,
			      "media file index within "
			      "dump and media object: %u\n",
			      mreadhdrp->mh_dumpmediafileix );
			mlog( MLOG_TRACE,
			      "media file index within dump: %u\n",
			      mreadhdrp->mh_dumpfileix );
		}

		/* stop the read stream
		 */
		drivep->d_opsp->do_end_read( drivep );

		/* check for errors when reading file header
		 */
		if ( rval ) {
			switch( rval )  {
			case DRIVE_ERROR_EOM:
				done = MEDIA_ERROR_EOM;
				break;
			case DRIVE_ERROR_EOD:
				done = MEDIA_ERROR_EOD;
				break;
			case DRIVE_ERROR_EOF:
			case DRIVE_ERROR_CORRUPTION:
			case DRIVE_ERROR_FORMAT:
				done = MEDIA_ERROR_CORRUPTION;
				break;
			case DRIVE_ERROR_DEVICE:
				done = MEDIA_ERROR_ABORT;
				break;
			case DRIVE_ERROR_CORE:
			default:
				/* driver error
 				 */
				done = MEDIA_ERROR_CORE;
				break;
			}
		} else {
			/* check if this is the end of this dump
		 	 */
			if  ( TERM_IS_SET( rmv_hdrp) ) {

				/* reached EOD on the tape
			 	 */
				done = MEDIA_ERROR_EOD;
			} else if ( UUID_COMPARE(dumpid, 
					greadhdrp->gh_dumpid) != 0 ) {

				/* reached start of a new dump session.  
				 */
				 done = MEDIA_ERROR_EOF;
			} else {

				/* this file is part of the same dump
				 * session, seek to the next file mark
				 */
				istat = 0;
				rval = drivep->d_opsp->do_fsf( drivep, 
					1, &istat );
				if ( rval != 1 ) {
					mlog( MLOG_DEBUG,
						"fsf did not advance a file\n");

/*
 BUGLY - NOT YET
					if  (istat == DRIVE_ERROR_EOM)
						done = MEDIA_ERROR_EOM;
					}

					if  (istat == DRIVE_ERROR_EOD) ) {
						done = MEDIA_ERROR_EOD;
					}
*/
#ifdef NOTNOW
					if ( CAN_OVERWRITE( drivep ) ) {
						done = MEDIA_ERROR_ABORT;
						mlog( MLOG_NORMAL,
						 	"this tape does not "
							"have a terminator "
							"record\n");
					} else {
						done = MEDIA_ERROR_EOD;
					}
#endif
				} else {
					/* increment the file mark count
					 */
					(*fmarks) += 1;
				}
			}
		}
	}
	return( done );
}


/* begin_write - prepare the media for writing
 *
 * RETURNS:
 *	0 on success
 *	MEDIA_ERROR_* on failure
 */
static intgen_t
begin_write( media_t *mediap )
{
	char			*str;
	uuid_t			ouuid;
	uint_t			status;
	drive_t 		*drivep = mediap->m_drivep;
	intgen_t 		rval;
	media_hdr_t 		*whdrp   = mediap->m_writehdrp;
	media_hdr_t 		*rhdrp   = mediap->m_readhdrp;
	media_rmvtape_spec_t	*rmv_hdrp;
	media_context_t		*contextp = (media_context_t *)mediap->m_contextp;

	/* Clear the media specific header flags
	 * (to be sure that TERMINATOR is not set)
	 */
	rmv_hdrp = (media_rmvtape_spec_t *)whdrp->mh_specific;
	rmv_hdrp->mrmv_flags = 0;

	/* Call drive level begin write routine.
	 * The drive begin_write routine will write the media header 
	 * to the media.
	 */
	rval = drivep->d_opsp->do_begin_write( drivep );
	switch ( rval ) {
	case 0:
		break;
	case DRIVE_ERROR_EOM:
		rval = reached_eom( mediap );
		if ( !rval ) {
			/* update indices
			 */
			whdrp->mh_mediaix++;
			whdrp->mh_dumpmediafileix = 0;

 			/* These two indices will be updated after reading 
			 * the new tape, if the new tape already contains
			 * valid dumps.
			 */
			whdrp->mh_mediafileix = 0;
			whdrp->mh_dumpmediaix = 0;

			/* copy old label and uuid to prev fields
		 	 */
			COPY_LABEL( whdrp->mh_medialabel, 
				whdrp->mh_prevmedialabel);

			COPY_UUID( whdrp->mh_mediaid,
				whdrp->mh_prevmediaid);

			/* allocate a mediaid for the new media.
			 * this will be overwritten in the 
			 * advance_to_end_of_dump() routine, if the new
			 * media object already contains dumps
		 	 */
			uuid_create( &contextp->mc_mediaid, &status);
			COPY_UUID( contextp->mc_mediaid, ouuid);
			
			/* zap write media label so that reached_eof() 
			 * will not complain about mismatched media labels.
			 */
			whdrp->mh_medialabel[0] = '\0';

			rval = advance_to_end_of_dump( mediap );
		
			/* if the tape already contains dumps,
			 * get the label info.
			 */
			if ( (!rval) &&
			     (!uuid_equal( &contextp->mc_mediaid, 
				&ouuid, &status)) )  {
				
				if ( CAN_OVERWRITE(drivep) ) {
					whdrp->mh_mediafileix = 
						rhdrp->mh_mediafileix;
				} else {
					whdrp->mh_mediafileix = 
						rhdrp->mh_mediafileix + 1;
				}

				whdrp->mh_dumpmediaix = 
						rhdrp->mh_dumpmediaix + 1;

				COPY_UUID( contextp->mc_mediaid, 
						whdrp->mh_mediaid);
				COPY_LABEL( contextp->mc_medialabel, 
						whdrp->mh_medialabel);
			} 
		}

                /* restart write
                 */
                if ( !rval ) {
			mlog(MLOG_VERBOSE,
				"begin write to new tape\n");

			/* Display the name of the media.
			 */
			if ( strcmp(whdrp->mh_medialabel,"") != 0 ) {
				mlog( MLOG_NORMAL | MLOG_STDOUT,
					"media label: \"%s\"\n",
					whdrp->mh_medialabel);
			} else {
				mlog( MLOG_NORMAL | MLOG_STDOUT,
					"no media label\n");
			}

			/* Display the uuid of the media.
			 */
			if ( !uuid_is_nil(&whdrp->mh_mediaid, &status ) ) {
				uuid_to_string(&whdrp->mh_mediaid,
					&str,
					&status);
				mlog( MLOG_NORMAL | MLOG_STDOUT,
					"media id: %s\n",
					str);
				free( (void *)str);
			} else {
				mlog( MLOG_NORMAL | MLOG_STDOUT,
					"no media id\n");
			}

			rval = drivep->d_opsp->do_begin_write( drivep );
			if ( rval ) {
				mlog(MLOG_DEBUG,
					"RMVTAPE: begin write failed\n");
                        	rval = MEDIA_ERROR_ABORT;
			}
                }
		/* we should return a rval of 0 (not MEDIA_ERROR_EOM)
		 */
		break;
	case DRIVE_ERROR_EOF:
		rval = MEDIA_ERROR_ABORT;
		break;
	case DRIVE_ERROR_MEDIA:
		rval = MEDIA_ERROR_ABORT;
		break;
	case DRIVE_ERROR_DEVICE:
		rval = MEDIA_ERROR_ABORT;
		break;
	case DRIVE_ERROR_CORE:
	default:
		rval = MEDIA_ERROR_CORE;
		break;
	}

	return (rval);
}

/* markcb 
 * 	Forward the caller's set_mark request to the
 * 	drive manager. intercept the callback, in order to doctor
 * 	the media_marklog_t before the caller sees it.
 * 
 * RETURNS: 
 *	none
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

/* set_mark()
 * 	Forward the caller's set_mark request to the
 * 	drive manager. intercept the callback, in order to doctor
 * 	the media_marklog_t before the caller sees it.
 *
 * RETURNS:
 *	none
 */
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

/* mwrite  
 * 	Write the portion of the buffer filled by the caller,
 * 	and accept ownership of the buffer lended to the caller 
 *	by get_write_buf()
 *
 * RETURNS:
 *	0 on success
 *	MEDIA_ERROR_* on failure
 */
static intgen_t
mwrite( media_t *mediap, char *bufp, size_t writesz )
{
	drive_t 	*drivep = mediap->m_drivep;
	intgen_t 	rval;

	/* issue the write to the drive layer
	 */
	rval = ( * drivep->d_opsp->do_write )( drivep, bufp, writesz );

	switch( rval ) {
	case 0:
		/* write was a success		
		 */
		break;
	case DRIVE_ERROR_EOM:
		/* got EOM on write
		 */
		mlog(MLOG_DEBUG,
			"RMVTAPE: end-of-media in mwrite\n");
		rval = MEDIA_ERROR_EOM;
		break;
	case DRIVE_ERROR_EOF:
		rval = MEDIA_ERROR_ABORT;
		break;
	case DRIVE_ERROR_MEDIA:
		rval = MEDIA_ERROR_ABORT;
		break;
	case DRIVE_ERROR_DEVICE:
		rval = MEDIA_ERROR_ABORT;
		break;
	case DRIVE_ERROR_CORE:
	default:
		assert( 0 );
		rval = MEDIA_ERROR_ABORT;
		break;
	}
	return( rval );
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

/* end_write  
 * 	Flush any buffer data to the tape.
 *	Increment the media indices.
 * 
 * RETURNS: 
 * 	0 on success
 *	MEDIA_ERROR_* on failure
 */
static intgen_t
end_write( media_t *mediap, off64_t *ncommittedp )
{
	drive_t 	*drivep = mediap->m_drivep;
	intgen_t	 rval;
	media_hdr_t 	*hdrp = mediap->m_writehdrp;

	rval = ( * drivep->d_opsp->do_end_write )( drivep, ncommittedp );
	
	hdrp->mh_mediafileix++;
	hdrp->mh_dumpfileix++;
	hdrp->mh_dumpmediafileix++;

#ifdef DEBUG
	mlog( MLOG_DEBUG, 
		"end write\n");
	mlog( MLOG_DEBUG, 		
		"mediaix         = %d\n",
		hdrp->mh_mediaix);
	mlog( MLOG_DEBUG, 
		"mediafileix     = %d\n",
		hdrp->mh_mediafileix);
	mlog( MLOG_DEBUG, 
		"dumpfileix      = %d\n",
		hdrp->mh_dumpfileix);
	mlog( MLOG_DEBUG, 
		"dumpmediafileix = %d\n",
		hdrp->mh_dumpmediafileix);
	mlog( MLOG_DEBUG, 
		"dumpmediaix     = %d\n",
		hdrp->mh_dumpmediaix);
#endif

	switch( rval ) {
	case 0:
		return 0;
	case DRIVE_ERROR_EOM:
		return MEDIA_ERROR_EOM;
	default:
		return MEDIA_ERROR_ABORT;
	}
}

/* end_session  
 *	Write a terminator file to the tape if the tape device supports
 *	overwrite. Rewind the tape and forward the end session to the 
 *	driver layer.
 *
 * RETURN:
 *	none
 */
static void
end_stream( media_t *mediap )
{
	int			rval = 0;
	uuid_t			ouuid;
	uint_t			status;
	drive_t 		*drivep = mediap->m_drivep;
	drive_ops_t		*dops   = drivep->d_opsp;
	media_hdr_t 		*whdrp = mediap->m_writehdrp;
	media_hdr_t 		*rhdrp = mediap->m_readhdrp;
	media_rmvtape_spec_t	*rmv_hdrp;
	media_context_t		*contextp;

	contextp = (media_context_t *)mediap->m_contextp;

	/* only write a terminator on those devices which allow overwrite.
	 */
	if ( CAN_OVERWRITE( drivep ) ) {
		/* Set the TERMINATOR flag to mark end of dump.
	 	 */
		rmv_hdrp = (media_rmvtape_spec_t *)whdrp->mh_specific;
		rmv_hdrp->mrmv_flags |= RMVMEDIA_TERMINATOR_BLOCK;

		/* begin write
	 	 * this will cause the terminator header to be written
	 	 */
		mlog( MLOG_TRACE,
		      "beginning media terminator\n" );
		rval = dops->do_begin_write( drivep );

		if (rval == DRIVE_ERROR_EOM) {
			/* if this is an EOM condition, get the next tape
			 * we have to handle this condition here instead
			 * of in begin_write().
 		 	 */

			mlog(MLOG_DEBUG,
				"RMVTAPE: got EOM in end session\n");

			if ( !(rval = reached_eom( mediap ) ) ) {
				whdrp->mh_mediaix++;
				whdrp->mh_dumpmediafileix = 0;

				whdrp->mh_mediafileix = 0;
				whdrp->mh_dumpmediaix = 0;

				/* copy old label and uuid to prev fields
				 */
				COPY_LABEL( whdrp->mh_medialabel,
					whdrp->mh_prevmedialabel);
				COPY_UUID( whdrp->mh_prevmediaid, 
					whdrp->mh_mediaid);

				/* allocate a mediaid for the new media
			 	 */
				uuid_create( &contextp->mc_mediaid, &status);
				COPY_UUID( contextp->mc_mediaid, ouuid);

				rval = advance_to_end_of_dump( mediap );

				if ( (!rval) &&
				     (!uuid_equal( &contextp->mc_mediaid,
					&ouuid, &status)) ) {
					whdrp->mh_mediafileix =
						rhdrp->mh_mediafileix + 1;
					whdrp->mh_dumpmediaix =
						rhdrp->mh_dumpmediaix + 1;

					COPY_UUID( contextp->mc_mediaid,
						whdrp->mh_mediaid);
				}

				/* restart write
			 	 */
				if ( ! rval ) { 
					mlog( MLOG_TRACE,
						"beginning media terminator "
						"(retry)\n" );
					rval = dops->do_begin_write( drivep );
					if ( rval ) {
						mlog(MLOG_DEBUG,
							"RMVTAPE: begin write "
							"failed\n");
						rval = MEDIA_ERROR_ABORT;
					}
				}
			}
		}
	
		/* If the terminator record could not be written,
		 * print a message.
		 */
		if ( rval ) {
			mlog(MLOG_DEBUG,
				"cannot write media " 
				"terminator to dump device\n");
		} else {
			off64_t ncommitted;

			/* end write
			 */
			mlog( MLOG_TRACE,
			      "ending media terminator\n" );
			rval = dops->do_end_write( drivep, &ncommitted );
			if ( rval ) {
				assert( 0 );
				rval = MEDIA_ERROR_ABORT;
			}
			mlog( MLOG_NITTY,
			      "media terminator %lld bytes committed\n",
			      ncommitted );

			whdrp->mh_mediafileix++;
			whdrp->mh_dumpmediafileix++;
		}
	}

	if ( !rval ) {
		/* rewind the tape
	 	 */
		rval = dops->do_rewind( drivep );
	}

	/* close the tape device
	 */
	dops->do_end_stream( drivep );

	return;
}


/* change_media_label()
 *	Ask the user if they want to label the next media object.
 *
 *
 * SIDE EFFECT:
 *	Changes the mh_medialabel field in the media write header.
 *
 * RETURNS:
 *	none
 */
static void
change_media_label(media_t *mediap)
{
	int			reply;
	char			ackstr[ GLOBAL_HDR_STRING_SZ + 40 ];
	media_hdr_t 		*whdrp  = mediap->m_writehdrp;
	media_context_t		*contextp;

	if ( ! dlog_allowed( )) {
		return;
	}

	contextp    = mediap->m_contextp;

	if ( strcmp(whdrp->mh_prevmedialabel,"") != 0 ) {
                mlog( MLOG_VERBOSE,
                      "previous media label: \"%s\"\n",
                      whdrp->mh_prevmedialabel);
        } else {
                mlog( MLOG_VERBOSE,
                      "no previous media label\n");
        }

	reply = dlog_yesno(
		"do you want to label the new media?",
		"",
		"blank media label",
		"timeout: blank media label",
		BOOL_UNKNOWN,
		600);

	if (reply != BOOL_TRUE) {
		/* blank media label.
		 */
		bzero( contextp->mc_medialabel, GLOBAL_HDR_STRING_SZ);
		return;
	}

	/* get new media label
	 */
	bzero( contextp->mc_medialabel, GLOBAL_HDR_STRING_SZ);
	dlog_multi_begin( NULL, 0, BOOL_FALSE );
	dlog_multi_get_string(
		"enter media label",
		contextp->mc_medialabel, 
		GLOBAL_HDR_STRING_SZ,
		120,
		BOOL_FALSE);

	bzero( ackstr, GLOBAL_HDR_STRING_SZ + 40);
	sprintf(ackstr,"new media label: %s",contextp->mc_medialabel);
	dlog_multi_end( ackstr, BOOL_FALSE );
	return;
}
