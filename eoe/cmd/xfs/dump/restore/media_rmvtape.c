#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/restore/RCS/media_rmvtape.c,v 1.43 1994/12/07 01:17:50 cbullis Exp $"

/*
 * restore side media layer driver to support removable tape media
 *
 */
#include <bstring.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uuid.h>
#include <sys/uuid.h>
#include <dirent.h>
#include <time.h>
#include <fcntl.h>
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
#include "mlog.h"
#include "media_rmvtape.h"



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
intgen_t begin_stream( media_t * );
static intgen_t begin_read( media_t * );
static intgen_t seek_mark( media_t *, media_marklog_t *);
static intgen_t next_mark( media_t *);
static size_t get_align_cnt( media_t * );
static void return_read_buf( media_t *, char *, size_t );
static void end_read( media_t * );
static void end_stream( media_t * );
static char *mread( media_t *, size_t, size_t *, intgen_t * );


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
	0		/* ms_recmarksep */
};


/* definition of locally defined static variables *****************************/
uint	mediaobjectix = 0;

/* media operators
 */
static media_ops_t media_ops = {
	create,		/* mo_create */
	0,		/* mo_resume */
	init,		/* mo_init */
	begin_stream,	/* mo_begin_stream */
	0,		/* mo_begin_write */
	0,		/* mo_set_mark */
	0,		/* mo_get_write_buf */
	0,		/* mo_write */
	get_align_cnt,	/* mo_get_align_cnt */
	0,		/* mo_end_write */
	begin_read,	/* mo_begin_read */
	mread,		/* mo_read */
	return_read_buf,/* mo_return_read_buf */
	0,		/* mo_get_mark */
	seek_mark,	/* mo_seek_mark */
	next_mark,	/* mo_next_mark */
	end_read,	/* mo_end_read */
	end_stream	/* mo_end_stream */
};


/* definition of locally defined global functions ****************************/


/* definition of locally defined static functions ****************************/
static intgen_t read_next_file_header( struct media *, int *, int, int);
static intgen_t init_eof( struct media *, int *, int *, int, int);
static intgen_t reached_eom( drive_t *);

/* strategy match - determines if this is the right strategy
 */
static intgen_t
s_match( int argc, char *argv[], drive_strategy_t *dsp )
{
        int i, dclass, ret = 1;

        /* sanity check on initialization
         */
        assert( media_strategy_rmvtape.ms_id >= 0 );

        assert( dsp->ds_drivecnt == 1);
        for (i = 0 ; i < (int)dsp->ds_drivecnt; i++) {
                dclass = (int)dsp->ds_drivep[i]->d_opsp->do_get_device_class(
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
	drive_strategy_t *dsp = msp->ms_dsp;
	size_t mediaix;

	/* copy the recommended mark separation value from
	 * the drive strategy
	 */
	msp->ms_recmarksep = dsp->ds_recmarksep;

	/* copy the recommended mark media file size from
	 * the drive strategy
	 */
	msp->ms_recmfilesz = dsp->ds_recmfilesz;

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
 * non-I/O per drive init
 */
static bool_t
create( struct media *mediap )
{
	uint_t			status;
	media_context_t		*contextp;

	assert( mediap->m_contextp == 0);
	contextp = (media_context_t *)memalign( PGSZ, sizeof(media_context_t));
	mediap->m_contextp = (void *)contextp;
	
	bzero(contextp->mc_dumplabel,sizeof(contextp->mc_dumplabel));
	uuid_create_nil( &(contextp->mc_dumpid), &status);
	bzero( contextp->mc_medialabel,  sizeof(contextp->mc_medialabel) );
	uuid_create_nil( &(contextp->mc_mediaid), &status);

	return BOOL_TRUE;
}

/* init - media stream initialization second pass
 *
 * tell the drive to begin reading. this will cause the media file
 * header to be read.
 */
static bool_t
init( struct media *mediap )
{
	int 			done, fmarks;
	int			domatchdumplabel = 0, domatchdumpid = 0;
	/*
	char			*str;
	*/
	uint_t			status;
	drive_t  		*drivep = mediap->m_drivep;
	intgen_t 		rval;
	global_hdr_t		*gwritehdrp;
	media_hdr_t		*mreadhdrp;
	media_context_t		*contextp;
	media_rmvtape_spec_t	*rmv_hdrp;

	done = fmarks = 0;
	contextp = mediap->m_contextp;
	
	mreadhdrp  = mediap->m_readhdrp;
	gwritehdrp = mediap->m_gwritehdrp;
	rmv_hdrp   = (media_rmvtape_spec_t *)&(mreadhdrp->mh_specific);

	/* Check if the user has supplied a particular 
 	 * dump session label or uuid to look for.
 	 */
	if ( strlen( gwritehdrp->gh_dumplabel )) {
		/* user specified a dump session
		 */
		domatchdumplabel = 1;
		COPY_LABEL( gwritehdrp->gh_dumplabel, contextp->mc_dumplabel );
	}

	if ( ! uuid_is_nil( &gwritehdrp->gh_dumpid, &status)) {
		domatchdumpid = 1;
		COPY_UUID(gwritehdrp->gh_dumpid, contextp->mc_dumpid );
	}

	if ( ( * drivep->d_opsp->do_begin_stream )( drivep )) {
		return BOOL_FALSE;
	}

	/* the media info will be filled in when
	 * we locate the correct dump file.
	 */
	while ( !done ) {
		rval = read_next_file_header( mediap, &fmarks, 0, 0);

		switch ( rval ) {

                /* found terminator
                 */
                case MEDIA_ERROR_EOD:
			if  ( TERM_IS_SET(rmv_hdrp) ) {
                        	mlog( MLOG_NORMAL,
			 		"terminator found. "
					"no more dumps on this tape\n");
			} else if (fmarks) {
                        	mlog( MLOG_NORMAL,
					"no more dumps on this tape\n");
			} else {
                        	mlog( MLOG_NORMAL,
			     		"tape is blank\n");
			}
			done = 1;
			rval = MEDIA_ERROR_ABORT;
                        break;

                /* dump file exists  - uuids do not match
                 */
                case MEDIA_ERROR_EOF:

			rval = init_eof( mediap,
					 &done,
					 &fmarks,
					 domatchdumplabel,
					 domatchdumpid);
                        break;

                /* get next tape
                 */
                case MEDIA_ERROR_EOM:
                        mlog( MLOG_NORMAL,
                                "reached end of tape\n");

			/* rewind the tape and change the media
			 */
			if ( rval = reached_eom( drivep ) ) {
				done = 1;
			}

                        break;
                /* no previous dump.
                 */
                case MEDIA_ERROR_CORRUPTION:
			mlog( MLOG_NORMAL,
				"could not read tape file - advancing tape\n");
#ifdef NOTNOW
			mlog( MLOG_NORMAL,
				"no dumps found on this tape\n");
                        done = 1;
                        rval = MEDIA_ERROR_ABORT;
#endif
                        break;
                case MEDIA_ERROR_ABORT:
			mlog( MLOG_NORMAL,
				"tape media error - exiting\n");
                        done = 1;
                        rval = MEDIA_ERROR_ABORT;
                        break;
                default:
                        assert( 0 );
                        break;
                }
	}

	if ( !rval ) {
		/* validate the strategy id
	 	 */
		if ( mediap->m_readhdrp->mh_strategyid != 
					mediap->m_strategyp->ms_id ) {
			mlog( MLOG_NORMAL,
				 "unrecognized media strategy ID (%d).n",
				 mediap->m_readhdrp->mh_strategyid );
			rval = MEDIA_ERROR_ABORT;
		}

		if ( (!fmarks) && (mediap->m_readhdrp->mh_mediafileix != 0) ) {
			mlog( MLOG_NORMAL,
				"tape not rewound: aborting\n");
			rval = MEDIA_ERROR_ABORT;

		}
#ifdef DEBUG
		/*
		mlog( MLOG_DEBUG,
			"tape restore: file header - \n");
		mlog(MLOG_DEBUG,
		     "index of tape object within dump stream: %d\n",
		     mreadhdrp->mh_mediaix);
		mlog(MLOG_DEBUG,
		     "index of media file within tape object: %d\n",
		     mreadhdrp->mh_mediafileix);
		mlog(MLOG_DEBUG,
		     "index of media file within dump stream: %d\n",
		     mreadhdrp->mh_dumpfileix);
		mlog(MLOG_DEBUG,
		     "index of media file within dump stream "
		     "and tape object: %d\n",
		     mreadhdrp->mh_dumpmediafileix);
		mlog(MLOG_DEBUG,
		     "index of dump stream within tape object: %d\n",
		     mreadhdrp->mh_dumpmediaix);
		*/
#endif
	
		/* In verbose mode, display the labels and uuids
		 * of the dump being restored.
		mlog( MLOG_VERBOSE,
		      "session label: \"%s\"\n",contextp->mc_dumplabel);

		uuid_to_string(&(contextp->mc_dumpid), &str, &status);
		mlog( MLOG_VERBOSE,
		      "session id:    %s\n",str);
		free( ( void * )str );

		mlog( MLOG_VERBOSE,
		      "media label:   \"%s\"\n",contextp->mc_medialabel);

		uuid_to_string(&(contextp->mc_mediaid), &str, &status);
		mlog( MLOG_VERBOSE,
		      "media id:      %s\n",str);
		free( ( void * )str );
 		 */
	}
	return rval ? BOOL_FALSE : BOOL_TRUE;
}

/* begin a new stream : not used
 */
intgen_t
begin_stream( media_t *mediap )
{
	return 0;
}

/* init_eof()
 *	Handle tape end-of-file condition.
 *	If the user is not looking for a specific dump session, ask
 *	if they want to restore the current dump. If the current dump
 *	is the one the user wants to restore, reposition the tape to the
 *	beginning of the dump file and set the "done" parameter to 1, 
 * 	otherwise skip to the next tape file.
 *
 * RETURNS:
 *	0 on success
 *	MEDIA_ERROR_* on failure
 */
static intgen_t
init_eof( media_t *mediap, int *done, int *fmarks, int domatchdumplabel, int domatchdumpid )
{
	int 			reply, stop, match, rval;
	char	 		*str;
	uint_t			status;
	drive_t  		*drivep = mediap->m_drivep;
	intgen_t 		rstat;
	drive_ops_t		*dops = drivep->d_opsp;
	global_hdr_t		*greadhdrp;
	media_hdr_t		*mreadhdrp;
	media_context_t		*contextp;
	intgen_t		loglevel0;
	intgen_t		loglevel1;

	stop = match = rval = 0;
	greadhdrp = mediap->m_greadhdrp;
	mreadhdrp = mediap->m_readhdrp;
	contextp  = (media_context_t *)mediap->m_contextp;

	/* adjust the mlog level based on interactivity level
	 */
	if ( domatchdumplabel || domatchdumpid ) {
		loglevel0 = MLOG_VERBOSE;
		loglevel1 = MLOG_TRACE;
	} else {
		loglevel0 = MLOG_NORMAL;
		loglevel1 = MLOG_NORMAL;
	}

	mlog( loglevel0,
		"dump session found\n");

	/* Print session label.
	 */
	if ( strcmp(greadhdrp->gh_dumplabel,"") != 0 ) {
		mlog( loglevel1,
		      "session label: \"%s\"\n",
		      greadhdrp->gh_dumplabel);
	} else {
		mlog( loglevel1,
		      "no session label\n");
	}

	/* Print session uuid.
	 */
	if ( !uuid_is_nil(&greadhdrp->gh_dumpid, &status ) ) {
		uuid_to_string(&greadhdrp->gh_dumpid,
			&str, &status);
		mlog( loglevel1,
		      "session id:    %s\n",
		      str);
		free( ( void * )str );
	} else {
		mlog( loglevel1,
		      "no id\n");
	}

	/* Print media label.
	 */
	if ( strcmp(mreadhdrp->mh_medialabel,"") != 0 ) {
		mlog( loglevel1,
		      "media label:   \"%s\"\n",
		      mreadhdrp->mh_medialabel);
	} else {
		mlog( loglevel1,
		      "no media label\n");
	}

	/* Print media uuid.
	 */
	if ( !uuid_is_nil(&mreadhdrp->mh_mediaid, &status ) ) {
		uuid_to_string(&mreadhdrp->mh_mediaid,
			&str, &status);
		mlog( loglevel1,
		      "media id:      %s\n",
		      str);
		free( ( void * )str );
	} else {
		mlog( loglevel1,
		      "no media id\n");
	}


#ifdef DEBUG

	mlog( MLOG_DEBUG,
	      "index of tape object within dump stream: %d \n", 
	      mreadhdrp->mh_mediaix);

	mlog( MLOG_DEBUG,
	      "index of media file within tape object: %d \n", 
	      mreadhdrp->mh_mediafileix);

	mlog( MLOG_DEBUG,
	      "index of media file within dump stream: %d \n", 
	      mreadhdrp->mh_dumpfileix);

	mlog( MLOG_DEBUG,
	      "index of media file within dump stream "
	      "and tape object: %d \n", 
	      mreadhdrp->mh_dumpmediafileix);

	mlog( MLOG_DEBUG,
	      "index of dump stream within tape object: %d \n", 
	      mreadhdrp->mh_dumpmediaix);

#endif

	/* do not prompt if we are looking for a specific dump
 	 */
	if ( ! domatchdumplabel && ! domatchdumpid ) {

		if ( dlog_allowed( )) {
			reply = dlog_yesno(
					"do you want to select this dump?",
					"selected",
					"not selected",
					"request timeout",
					BOOL_UNKNOWN,
					600);

			if (reply == BOOL_TRUE) {
				/* restore this dump
				 */
				COPY_UUID( greadhdrp->gh_dumpid, 
					contextp->mc_dumpid);
				COPY_LABEL( greadhdrp->gh_dumplabel, 
					contextp->mc_dumplabel);
				match = 1;
			} else {
				reply = dlog_yesno(
					"do you want to continue searching?",
					"continuing search",
					"search aborted",
					"request timeout",
					BOOL_UNKNOWN,
					120);

				if (reply != BOOL_TRUE) {
					stop = 1;
				} 
			}
		} else {
			mlog( MLOG_NORMAL,
			      "no session label or id specified on command line"
			      " and cannot prompt: aborting\n" );
			stop = 1;
		}
	} else if ( domatchdumpid ) {
		if ( uuid_equal( &greadhdrp->gh_dumpid,
				 &contextp->mc_dumpid,
				 &status)) {
			COPY_LABEL( greadhdrp->gh_dumplabel, 
				    contextp->mc_dumplabel);
			match = 1;
		}
	} else {

		assert( domatchdumplabel );

		/* Compare the dump label specified by the
	 	 * user with the one in this file label.
		 * If there is a match we are done, else
 		 * go to next file.
 		 */
		if (  ! strcmp( greadhdrp->gh_dumplabel,
				contextp->mc_dumplabel )) {
			COPY_UUID( greadhdrp->gh_dumpid, 
				   contextp->mc_dumpid );
			match = 1;
		}
	}

	if ( stop ) {
		/* user does not want to continue searching
		 */
		if ( dops->do_rewind( drivep ) ) {
			rval = MEDIA_ERROR_ABORT;
		}
		rval = MEDIA_ERROR_ABORT;
		(*fmarks)++;
		*done = 1;
	} else if ( match ) {
		/* found the dump that the user wants
		 */
		if ( mreadhdrp->mh_mediaix != 0 ) {
			mlog( MLOG_NORMAL,
				"this is not the first tape "
				"for this dump\n");
			mlog( MLOG_NORMAL,
				"this is tape %d\n",
				mreadhdrp->mh_mediaix );
			mlog( MLOG_NORMAL,
				"please restart the restore operation "
				"with the first tape\n");
			
			*done = 1;
			rval = MEDIA_ERROR_ABORT;
		} else {
			*done = 1;

			/* copy over the media label and uuid.
			 */
			COPY_UUID( mreadhdrp->mh_mediaid, 
				contextp->mc_mediaid);
			COPY_LABEL( mreadhdrp->mh_medialabel, 
					contextp->mc_medialabel);

			/* reposition the tape
			 */
			if ( *fmarks ) {

				/* back up one filemark
				 */
			 	if (dops->do_bsf( drivep, 1, &rstat) != 1) {
						rval = MEDIA_ERROR_ABORT;
				}
			} else {
				/* rewind tape to BOT
				 */
				if ( dops->do_rewind( drivep) ) {
					rval = MEDIA_ERROR_ABORT;
				}
			}
		}
	} else {
		/* move on to next file
		 */
		*done = 0;
		if ( dops->do_fsf( drivep, 1, &rstat) != 1 ) {
			mlog( MLOG_DEBUG,
				"tape fsf operation failed\n");

			if (rstat == DRIVE_ERROR_DEVICE) {
				rval = MEDIA_ERROR_ABORT;
			}
#ifdef NOTNOW
			if ( CAN_OVERWRITE( drivep ) ) {
				/* if there is supposed to be a terminator
				 * at the end of the tape - this is an error
			 	 */
				rval = MEDIA_ERROR_ABORT;
			} else {
				rval = MEDIA_ERROR_EOD;
			}
#endif
		} 
		(*fmarks)++;
	}
	return( rval );
}

/*
 * read_next_file_header()
 *	This routine is called when the tape is positioned at a filemark or
 *	at BOT. It tries to read the file header from the tape.	If the 
 *	"match" parameter is set to 0, the routine will read tape files
 *	until it encounters one with a dump id different than the current
 *	dump id. If the "match" parameter is set to 1, the routine will read
 *	until it finds a tape file with dump id matching the current dump
 *	id. 
 *	If match is set to 0, when the routine exits the tape is positioned 
 *	at a file mark. If match is set to 1, the tape is positioned at the
 *	record immediately following the file mark. ( This is an optimization
 *	to prevent having to read the file header, back the tape up, and then
 *	read the file header again in begin_read(). )
 *
 * 	MEDIA_ERROR_EOM		- end of media
 *
 *	if match is set to 1
 * 		MEDIA_ERROR_EOF		- current file is part of the 
 *					  current dump session
 *	if match is set to 0
 * 		MEDIA_ERROR_EOF		- current file is not part of the 
 * 					  current dump session
 *
 * 	MEDIA_ERROR_EOD		- found terminator
 *	MEDIA_ERROR_ABORT	- device error
 *	MEDIA_ERROR_CORRUPTION	- no dump session found
 */
static intgen_t
read_next_file_header( 
	struct media *mediap, 
	int *fmarks, 
	int match, 
	int at_bot) 
{
	int			done = 0;
	char			*str;
	uint_t			status;
	uuid_t			dumpid;
	drive_t  		*drivep = mediap->m_drivep;
	intgen_t		rval;
	global_hdr_t		*greadhdrp;
	media_hdr_t		*mreadhdrp;
	media_rmvtape_spec_t	*rmv_hdrp;

	/* setup media label pointers
	 */
	greadhdrp  = mediap->m_greadhdrp;
	mreadhdrp  = mediap->m_readhdrp;
	rmv_hdrp   = (media_rmvtape_spec_t *)&(mreadhdrp->mh_specific);

	/* stash the current dump uuid on the stack
	 */
	COPY_UUID(greadhdrp->gh_dumpid, dumpid);

	while ( !done ) {

		mlog( MLOG_DEBUG,
			"reading tape file header\n");

		/* The tape is currently positioned at a
		 * file mark. Read the next tape file header.
		 */
		rval = drivep->d_opsp->do_begin_read( drivep );

		if ( rval ) {
			/* Read of label failed.
			 * Convert to media error codes:
			 */
			drivep->d_opsp->do_end_read( drivep );
			switch ( rval ) {
			case DRIVE_ERROR_EOM:
				done = MEDIA_ERROR_EOM;
				break;
			case DRIVE_ERROR_EOD:
				done = MEDIA_ERROR_EOD;
				break;
			case DRIVE_ERROR_EOF:
				done = MEDIA_ERROR_CORRUPTION;
				break;
			case DRIVE_ERROR_CORRUPTION:
				status = 0;
				mlog( MLOG_DEBUG,
					"advancing tape to next file mark\n");
				rval = drivep->d_opsp->do_fsf( drivep, 
					1, (intgen_t *)&status );
				if (rval != 1) {
					mlog( MLOG_NITTY,
						"tape fsf failed\n");
				} else {
					(*fmarks) += 1;
				}
				break;
			case DRIVE_ERROR_DEVICE:
				done = MEDIA_ERROR_ABORT;
				break;
			default:
				done = MEDIA_ERROR_ABORT;
				break;
			}

		} else {

			/* check if this is the end of this dump
		 	 */
			if  ( TERM_IS_SET(rmv_hdrp) ) {
				/* a terminator file as encountered.
				 */
				done = MEDIA_ERROR_EOD;
				drivep->d_opsp->do_end_read( drivep );
			} else if ( ( (match) && 
				      ( UUID_COMPARE(dumpid, 
						greadhdrp->gh_dumpid) == 0)) ||
				    ( (!match) && 
				      ( UUID_COMPARE(dumpid, 
						greadhdrp->gh_dumpid) != 0)) ) {

				/* this file is associated with a dump
				 * we may interested in
		 	 	 */
				done = MEDIA_ERROR_EOF;
				if (!match)
					drivep->d_opsp->do_end_read( drivep );
			} else if ( (match) && (!at_bot) && 
				    ( UUID_COMPARE(dumpid, 
					greadhdrp->gh_dumpid) != 0) ) {
				done = MEDIA_ERROR_EOD;
			} else {

				if ( (match) && 
				    ( UUID_COMPARE(dumpid, 
					greadhdrp->gh_dumpid) != 0) ) {
					uuid_to_string(&(greadhdrp->gh_dumpid),
						 &str, &status);
					mlog (MLOG_DEBUG,
						"skipping file from dump: %s\n",
						str);
					free( ( void * )str );
				}



				drivep->d_opsp->do_end_read( drivep );

				/* This file does not belong to a dump
				 * we are interested in, skip it.
				 */
				status = 0;
				rval = drivep->d_opsp->do_fsf( drivep, 
					1, (intgen_t *)&status );
				if (rval != 1) {
					mlog( MLOG_DEBUG,
						"tape fsf command failed\n");
				} else {
					/* increment the file mark count
					 */
					(*fmarks) += 1;
				}
			}
		}


#ifdef DEBUG
		if (!done) {

			mlog( MLOG_DEBUG,
				"reading media file\n");

			/* Print session label.
			 */
			if ( strcmp(greadhdrp->gh_dumplabel,"") != 0 ) {
				mlog( MLOG_DEBUG,
				      "session label: \"%s\"\n",
				      greadhdrp->gh_dumplabel);
			} else {
				mlog( MLOG_DEBUG,
				      "no session label\n");
			}
	
			/* Print session uuid.
			 */
			if ( !uuid_is_nil(&greadhdrp->gh_dumpid, &status ) ) {
				uuid_to_string(&greadhdrp->gh_dumpid,
				&str, &status);
				mlog( MLOG_DEBUG,
				      "session id:    %s\n",
				      str);
				free( ( void * )str );
			} else {
				mlog( MLOG_DEBUG,
				      "no session id\n");
			}

			/* Print media label.
			 */
			if ( strcmp(mreadhdrp->mh_medialabel,"") != 0 ) {
				mlog( MLOG_DEBUG,
		      			"media label:   \"%s\"\n",
		      			mreadhdrp->mh_medialabel);
			} else {
				mlog( MLOG_DEBUG,
				      "no media label\n");
			}

			/* Print media uuid.
			 */
			if ( !uuid_is_nil(&mreadhdrp->mh_mediaid, &status ) ) {
				uuid_to_string(&mreadhdrp->mh_mediaid,
					&str, &status);
				mlog( MLOG_DEBUG,
				      "media id:      %s\n",
				      str);
				free( ( void * )str );
			} else {
				mlog( MLOG_DEBUG,
				      "no media id\n");
			}
	
			mlog(MLOG_DEBUG,
			     "index of tape object within dump stream: %d\n",
			     mreadhdrp->mh_mediaix);
			mlog(MLOG_DEBUG,
			     "index of media file within tape object: %d\n",
			     mreadhdrp->mh_mediafileix);
			mlog(MLOG_DEBUG,
			     "index of media file within dump stream: %d\n",
			     mreadhdrp->mh_dumpfileix);
			mlog(MLOG_DEBUG,
			     "index of media file within dump stream "
			     "and tape object: %d\n",
			     mreadhdrp->mh_dumpmediafileix);
			mlog(MLOG_DEBUG,
			     "index of dump stream within tape object: %d\n",
			     mreadhdrp->mh_dumpmediaix);
		}
#endif
	}
	return( done );
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

/* begin_read - prepare the media for reading
 *
 * already done during initialization
 */
static intgen_t
begin_read( media_t *mediap )
{
	uint_t			status;
	drive_t 		*drivep = mediap->m_drivep;
	intgen_t 		rval, done, fmarks, at_bot;
	global_hdr_t 		*ghdrp =  mediap->m_greadhdrp;
	media_hdr_t 		*hdrp =  mediap->m_readhdrp;
	media_rmvtape_spec_t	*rmv_hdrp;
	media_context_t 	*contextp;

	
	contextp = (media_context_t *)mediap->m_contextp;

	/* get the media specific header
	 */
	rmv_hdrp = (media_rmvtape_spec_t *)hdrp->mh_specific;

	done = fmarks = at_bot = 0;

	/* keep reading files until we get a good label
	 * or until there is an error
	 */
	while ( !done ) {

		rval = read_next_file_header( mediap, &fmarks, 1, at_bot);

		switch ( rval ) {
		case 0:
			/* read_next_file_header should never return 0.
			 */
			assert( 0 );
			break;

		case MEDIA_ERROR_EOD:
			/* reached end-of-data on the tape
			 */
			mlog(MLOG_DEBUG,
				"beginning media read at end-of-data\n");
	
			rval =  MEDIA_ERROR_EOD;
			done = 1;
			break;

		case MEDIA_ERROR_EOF:

			/* after read_next_file_header() is called with
			 * the match parameter set to 1, and a match is
			 * found, the tape does not need to be repositioned.
			 */
			mlog(MLOG_DEBUG,
				"beginning media read at end-of-file\n");

			/* check if this file matches the current dump
			 * stream
			 */
			if ( TERM_IS_SET( rmv_hdrp ) ) {
				rval = MEDIA_ERROR_EOD;
			} else if ( UUID_COMPARE( ghdrp->gh_dumpid,
						contextp->mc_dumpid) == 0 ) {
				/* check if a new tape has been started. If so
			 	 * update the media context with the next media
			 	 * uuid and label.
			 	 */
				done = 1;
				rval = 0;
				if ( uuid_is_nil( &contextp->mc_mediaid, &status) ) {
					COPY_UUID( hdrp->mh_mediaid, 
						contextp->mc_mediaid);
					COPY_LABEL( hdrp->mh_medialabel, 
						contextp->mc_medialabel);
					if ( hdrp->mh_mediaix != mediaobjectix){
						mlog( MLOG_NORMAL,
							"this is tape %d"
							"in the dump session."
							"\n", 
							hdrp->mh_mediaix);
						mlog( MLOG_NORMAL,
							"the next tape to be "	
							"restored is tape "
							"%d\n",
							mediaobjectix);
						mlog( MLOG_NORMAL,
							"please insert tape "
							"%d \n",
							mediaobjectix);
						drivep->d_opsp->do_end_read( 
							drivep );

						/* rewind tape and change 
						 * the media
			 			 */
						if ( rval=reached_eom(drivep)) {
							done = 1;
						} else {
							done = 0;
						}

						/* set media id to nil, it will
						 * be read from next tape.
 			 			 */
						uuid_create_nil( &(contextp->mc_mediaid), &status);
					}
				}
			} else {
				/* this should never happen
				 */
				assert( 0 );
			}
			break;

		case MEDIA_ERROR_EOM:
			/* reached end-of-media on the tape
			 */
			mlog(MLOG_DEBUG,
				"beginning media read at end-of-media\n");
	
			/* rewind tape and change the media
			 */
			if (rval = reached_eom( drivep ) )  {
				done = 1;
			}

			/* set media id to nil, it will be read from next
			 * tape.
 			 */
			uuid_create_nil( &(contextp->mc_mediaid), &status);
			mediaobjectix++;
			at_bot = 1;
			break;


		case MEDIA_ERROR_CORRUPTION:
			mlog( MLOG_DEBUG,
				"beginning media read - file corruption\n");

			done = 1;
			break;
		case MEDIA_ERROR_ABORT:
			mlog( MLOG_DEBUG,
				"beginning media read - abort \n");
			done = 1;
			break;

		default:
			/* driver error condition
 			 */
			assert( 0 );
			rval = MEDIA_ERROR_CORE;
			break;
		}
	}

#ifdef DEBUG	
	mlog( MLOG_DEBUG,
		"tape restore: file header - \n");
	mlog(MLOG_DEBUG,
	     "index of tape object within dump stream: %d\n",
	     mediap->m_readhdrp->mh_mediaix);
	mlog(MLOG_DEBUG,
	     "index of media file within tape object: %d\n",
	     mediap->m_readhdrp->mh_mediafileix);
	mlog(MLOG_DEBUG,
	     "index of media file within dump stream: %d\n",
	     mediap->m_readhdrp->mh_dumpfileix);
	mlog(MLOG_DEBUG,
	     "index of media file within dump stream "
	     "and tape object: %d\n",
	     mediap->m_readhdrp->mh_dumpmediafileix);
	mlog(MLOG_DEBUG,
	     "index of dump stream within tape object: %d\n",
	     mediap->m_readhdrp->mh_dumpmediaix);
#endif

	if ( rval ) {
		/* return error condition
		 */
		return( rval );
	} else {
		/* validate the strategy id
		 */
		if ( mediap->m_readhdrp->mh_strategyid != 
					mediap->m_strategyp->ms_id ) {
			mlog( MLOG_NORMAL,
				 "unrecognized media strategy ID (%d)\n",
				 mediap->m_readhdrp->mh_strategyid );
			rval = MEDIA_ERROR_ABORT;
		}

		return rval;
	}
}

/* read - supply the caller with some filled buffer
 */
static char *
mread( media_t *mediap,
      size_t wanted_bufsz,
      size_t *actual_bufszp,
      intgen_t *statp )
{
	char 		*bufp;
	drive_t 	*drivep = mediap->m_drivep;

	bufp = ( * drivep->d_opsp->do_read )( drivep,
					      wanted_bufsz,
					      actual_bufszp,
					      statp );
	switch ( *statp ) {
	case 0:
		/* successful read
		 */
		break;
	case DRIVE_ERROR_EOF:
		/* reached end-of-file on tape
		 */
		*statp = MEDIA_ERROR_EOF;
		break;
	case DRIVE_ERROR_EOD:
		/* reached end-of-data on tape
		 */
		*statp =  MEDIA_ERROR_EOD;
		break;
	case DRIVE_ERROR_EOM:
		/* reached end-of-media on tape
		 */
		mlog(MLOG_DEBUG,
			"media read reached end-of-media\n");

		/* stop read
		 */
		drivep->d_opsp->do_end_read( drivep );
		*statp = MEDIA_ERROR_EOM;
		break;

	case DRIVE_ERROR_CORRUPTION:

		mlog(MLOG_DEBUG,
			"media read - corruption \n");
		*statp = MEDIA_ERROR_CORRUPTION;
		break;
	case DRIVE_ERROR_DEVICE:

		*statp = MEDIA_ERROR_ABORT;
		break;
	case DRIVE_ERROR_CORE:
	default:
		/* driver error condition
	 	 */
		assert(0);
		*statp = MEDIA_ERROR_CORE;
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

/* end_read - tell the media manager we are done reading the media file
 * just discards any buffered data
 */
static void
end_read( media_t *mediap )
{
	drive_t *drivep = mediap->m_drivep;

	( * drivep->d_opsp->do_end_read )( drivep );
}

/* end_session()
 *
 */
static void
end_stream( media_t *mediap )
{
	drive_t *drivep = mediap->m_drivep;

	( * drivep->d_opsp->do_end_stream )( drivep );
}

/* seek_mark()
 *
 */
static intgen_t
seek_mark( media_t *mediap, media_marklog_t *marklogp)
{
	drive_t 	*drivep = mediap->m_drivep;
	intgen_t	rval;

	rval = drivep->d_opsp->do_seek_mark( drivep, (drive_marklog_t *)marklogp);

	switch (rval) {
	case DRIVE_ERROR_EOF:
		/* encountered end-of-file
		 */
		rval = MEDIA_ERROR_EOF;
		break;
	case DRIVE_ERROR_EOD:
		rval =  MEDIA_ERROR_EOD;
		break;
	case DRIVE_ERROR_EOM:
		/* get next media item
		 * 
		 * rewind current tape
		 */

		mlog(MLOG_DEBUG,
			"media seek mark at end-of-media\n");

		if ( (rval = drivep->d_opsp->do_rewind( drivep )) ) {
			mlog(MLOG_DEBUG,
				"media seek mark rewind failed\n");
			rval =  MEDIA_ERROR_ABORT;
		}
		rval = MEDIA_ERROR_EOM;
		break;
	case DRIVE_ERROR_DEVICE:
		rval =  MEDIA_ERROR_ABORT;
		break;
	case DRIVE_ERROR_CORRUPTION:
		mlog(MLOG_DEBUG,
			"media seek mark  - corruption \n");
		rval =  MEDIA_ERROR_CORRUPTION;
		break;
	default:
		assert( 0 );
		rval =  MEDIA_ERROR_ABORT;
		break;
	}
	return( rval );
}

/* next_mark()
 *
 */
static intgen_t
next_mark( media_t *mediap)
{
	intgen_t	rval;
	drive_t 	*drivep = mediap->m_drivep;
	drive_mark_t	dmark;

	rval = ( * drivep->d_opsp->do_next_mark )( drivep, &dmark );

	switch (rval) {
	case 0:
		/* success
		 */
		break;
	case DRIVE_ERROR_EOF:
		/* encountered end-of-file
		 */
		rval = MEDIA_ERROR_EOF;
		break;
	case DRIVE_ERROR_EOD:
		rval =  MEDIA_ERROR_EOD;
		break;
	case DRIVE_ERROR_EOM:
		/* get next media item
		 * 
		 * rewind current tape
		 */
		mlog(MLOG_DEBUG,
			"media next mark at end-of-media\n");
		rval = MEDIA_ERROR_EOM;
		break;
	case DRIVE_ERROR_DEVICE:
		rval =  MEDIA_ERROR_ABORT;
		break;
	default:
		assert( 0 );
		rval =  MEDIA_ERROR_ABORT;
		break;
	}
	return ( rval );
}

/* reached_eom()
 *	Handle tape end-of-media condition.
 *	Rewind the tape and promp the use to insert a new tape.
 *
 * RETURNS:
 *	0 on sucess
 *	MEDIA_ERROR_ABORT on failure
 */
static intgen_t
reached_eom( drive_t *drivep)
{
        drive_ops_t             *dops = drivep->d_opsp;
	intgen_t rval;

	if ( rval = dops->do_rewind( drivep ) ) {
		mlog(MLOG_NORMAL,
			"rewind of media at EOM failed\n");
		rval = MEDIA_ERROR_ABORT;
	} else if ( rval =
		dops->do_change_media(drivep,(char *)0)) {
		if ( rval == DRIVE_ERROR_TIMEOUT ) {
			rval = MEDIA_ERROR_TIMEOUT;
		} else if ( rval == DRIVE_ERROR_STOP ) {
			rval = MEDIA_ERROR_STOP;
		} else {
			mlog(MLOG_NORMAL,
				"change media failed\n");
			rval = MEDIA_ERROR_ABORT;
		}
	}
	return( rval );
}
