#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/restore/RCS/content_inode.c,v 1.54 1995/07/26 01:15:38 orosz Exp $"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/dirent.h>
#include <sys/uuid.h>
#include <time.h>
#include <sys/fs/xfs_itable.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "types.h"
#include "stream.h"
#include "state.h"
#include "jdm.h"
#include "getopt.h"
#include "util.h"
#include "openutil.h"
#include "stream.h"
#include "mlog.h"
#include "dlog.h"
#include "global.h"
#include "drive.h"
#include "media.h"
#include "content.h"
#include "content_inode.h"
#include "inomap.h"
#include "tree.h"
#include "cldmgr.h"
#include "cleanup.h"

/* content_inode.c - inode-style content strategy for standard in
 */


/* structure definitions used locally ****************************************/

#define WRITE_TRIES_MAX	3

/* strategy context
 */
struct scontext {
	content_strategy_t *sc_csp;
	char *sc_hkdir;
	char *sc_statepath;
	state_t *sc_statep;
	bool_t sc_isxfs;
	subtree_t *sc_subtreep;
	bool_t sc_inter;
};

typedef struct scontext scontext_t;


/* context structure to hang from the generic content_t's contextp
 */
typedef enum { STATE_NONE, STATE_DIR, STATE_NONDIR } statstate_t;

struct context {
	char *cc_symlinkbufp;		/* to store symlink path */
	size_t cc_symlinkbufsz;
	char *cc_hkdir;			/* where misc housekeeping files kept */
	state_t *cc_statep;
	statstate_t cc_statstate;
	u_int64_t cc_statdone;
	u_int64_t cc_statcnt;
	u_int64_t cc_datasz;
	u_int64_t cc_datadone;
	ino64_t cc_lastino;
	bool_t cc_lastoverwrite;
};

typedef struct context context_t;


/* exception codes used by main restore loop in begin()
 */
#define XCODE_INPROGRESS	0
#define XCODE_DONE		1
#define XCODE_NEXT		2
#define XCODE_STOP		3
#define XCODE_ABORT		4
#define XCODE_RVAL		5
#define XCODE_RVAL_HDR		6
#define XCODE_CORE		7
#define XCODE_CORRUPTION	8
#define XCODE_CORRUPTION_HDR	9
#define XCODE_STOP_INTER	10


/* name of the restore housekeeping directory
 */
#define HOUSEKEEPINGDIR	"xfsrestorehousekeeping"

/* name of the restore state file
 */
#define STATEFILE	"state"


/* declarations of externally defined global symbols *************************/

extern void usage( void );
extern char *homedir;


/* forward declarations of locally defined static functions ******************/

/* strategy functions
 */
static intgen_t s_match( int, char *[], media_strategy_t * );
static bool_t s_create( content_strategy_t *, int, char *[] );
static bool_t s_init( content_strategy_t *, int, char *[] );
static size_t s_stat( content_strategy_t *, char [][ DLOG_MULTI_STATSZ ], size_t );
static void s_complete( content_strategy_t * );

/* manager operators
 */
static intgen_t create( content_t * );
static intgen_t init( content_t * );
static intgen_t begin_stream( content_t *, size_t );

/* regular file restore functions
 */
static intgen_t restore_file( content_t *, filehdr_t *, tree_t *, bool_t );
static bool_t restore_file_cb( void *, char *, char * );
static bool_t restore_reg( content_t *,
			   filehdr_t *,
			   intgen_t *,
			   char *,
			   bool_t );
static bool_t restore_spec(content_t *, filehdr_t *, intgen_t *, char * );
static bool_t restore_symlink( content_t *,
			       filehdr_t *,
			       intgen_t *,
			       char *,
			       bool_t );
static intgen_t read_filehdr( media_t *, filehdr_t *, bool_t );
static intgen_t read_extenthdr( media_t *, extenthdr_t *, bool_t );
static intgen_t read_dirent( media_t *, direnthdr_t *, size_t, bool_t );
static intgen_t discard_padding( size_t, media_t * );
static bool_t restore_extent( filehdr_t *,
			      extenthdr_t *,
			      int,
			      char *,
			      media_t *,
			      off64_t *,
			      intgen_t * );
static char *ehdr_typestr( int32_t );
static void hkd_abort_cleanup( void *, void * );


/* definition of locally defined global variables ****************************/

/* strategy-specific context
 */
scontext_t scontext_inode = {
	0,		/* sc_csp */
	0,		/* sc_hkdir */
	0,		/* sc_statepath */
	0,		/* sc_statep */
	BOOL_FALSE,	/* sc_isxfs */
	0,		/* sc_subtreep */
	BOOL_FALSE,	/* sc_inter */
};

/* content strategy . referenced by content.c
 */
content_strategy_t content_strategy_inode = {
	-1,		/* cs_id */
	0,		/* flags */
	0,		/* newertime */
	s_match,	/* cs_match */
	s_create,	/* cs_create */
	s_init,		/* cs_init */
	0,		/* cs_stat */
	s_complete,	/* cs_complete */
	0,		/* cs_contentp */
	0,		/* cs_contentcnt */
	&scontext_inode,/* cs_contextp */
	0		/* cs_msp */
};


/* definition of locally defined static variables *****************************/

/* content operators
 */
static content_ops_t content_ops = {
	create,		/* co_create */
	init,		/* co_init */
	begin_stream	/* co_begin_stream */
};


/* definition of locally defined global functions ****************************/


/* definition of locally defined static functions ****************************/

/* strategy match - determines if this is the right strategy
 *
 * for now, only one strategy.
 */
static intgen_t
s_match( int argc, char *argv[], media_strategy_t *msp )
{
	/* sanity check on initialization
	 */
	assert( content_strategy_inode.cs_id >= 0 );
	return 1;
}

/* strategy init - initializes the strategy, and each manager
 */
static bool_t
s_create( content_strategy_t *csp, int argc, char *argv[] )
{
	scontext_t *scontextp = ( scontext_t * )csp->cs_contextp;
	content_t *contentp = csp->cs_contentp[ 0 ];

	content_hdr_t *cwhdrp = contentp->c_writehdrp;
	char *dstdir = cwhdrp->ch_mntpnt;
	bool_t toc = csp->cs_flags & CONTENT_STRATEGY_FLAGS_TOC;
	bool_t cumulative =
	    contentp->c_strategyp->cs_flags & CONTENT_STRATEGY_FLAGS_CUMULATIVE;
	bool_t resumed_restore =
	    contentp->c_strategyp->cs_flags & CONTENT_STRATEGY_FLAGS_RESUME;
	subtree_t *subtreep;
	bool_t inter;
	intgen_t c;
	size_t contentix;
	char *hkd;
	char *hkdir;
	state_t *statep;
	char *statepath;
	pid_t pid;
	bool_t isxfs;
	bool_t ok;

	/* basic sanity checks
	 */
	assert( sizeof( mode_t ) == MODE_SZ );
	assert( sizeof( timestruct_t ) == TIMESTRUCT_SZ );
	assert( sizeof( bstat_t ) == BSTAT_SZ );
	assert( sizeof( xfs_bstat_t ) <= sizeof( bstat_t ));
	assert( sizeof( filehdr_t ) == FILEHDR_SZ );
	assert( sizeof( extenthdr_t ) == EXTENTHDR_SZ );
	assert( sizeof( direnthdr_t ) == DIRENTHDR_SZ );

	/* give tree a chance to parse the command line for subtree
	 * options. save returned ptr in context. may flag an error
	 */
	subtreep = tree_cmdline_parse( argc, argv, &inter, &ok );
	if ( ! ok ) {
		return BOOL_FALSE;
	}
	scontextp->sc_subtreep = subtreep;
	scontextp->sc_inter = inter;

	/* subtree restores preclude some other options
	 */
	if ( subtreep ) {
		if ( cumulative ) {
			mlog( MLOG_NORMAL,
			      "-%c and -%c option cannot be used together\n",
			      GETOPT_SUBTREE,
			      GETOPT_CUMULATIVE );
			usage( );
			return BOOL_FALSE;
		}
		if ( resumed_restore ) {
			mlog( MLOG_NORMAL,
			      "-%c and -%c option cannot be used together\n",
			      GETOPT_SUBTREE,
			      GETOPT_RESUME );
			usage( );
			return BOOL_FALSE;
		}
	}

	if ( inter ) {
		if ( cumulative ) {
			mlog( MLOG_NORMAL,
			      "-%c and -%c option cannot be used together\n",
			      GETOPT_INTERACTIVE,
			      GETOPT_CUMULATIVE );
			usage( );
			return BOOL_FALSE;
		}
		if ( resumed_restore ) {
			mlog( MLOG_NORMAL,
			      "-%c and -%c option cannot be used together\n",
			      GETOPT_INTERACTIVE,
			      GETOPT_RESUME );
			usage( );
			return BOOL_FALSE;
		}
	}

	/* determine if the destination is an xfs file system
	 */
	isxfs = isinxfs( dstdir );

	/* check the command line for alternate housekeeping directory
	 */
	hkd = 0;
	optind = 1;
	opterr = 0;
	while ( ( c = getopt( argc, argv, GETOPT_CMDSTRING )) != EOF ) {
		switch ( c ) {
		case GETOPT_WORKSPACE:
			if ( ! optarg || optarg[ 0 ] == '-' ) {
				mlog( MLOG_NORMAL,
				      "-%c argument missing\n",
				      optopt );
				usage( );
				return BOOL_FALSE;
			}
			hkd = optarg;
			break;
		}
	}

	/* generate a full pathname for the housekeeping dir.
	 * the housekeeping dir will by default be placed in the
	 * destination directory, unless this is a toc, in which case
	 * it will be placed in the current directory. in either case, an
	 * alternate directory may be specified on the command line.
	 * if this is a toc, modify the housekeeping dir's name with 
	 * the pid.
	 */
	if ( ! hkd ) {
		if ( toc ) {
			assert( homedir[ 0 ] == '/' );
			hkd = homedir;
		} else {
			hkd = dstdir;
		}
	}
	if ( toc ) {
		pid = getpid( );
	} else {
		pid = 0;
	}
	if ( hkd[ 0 ] == '/' ) {
		hkdir = open_pathalloc( hkd, HOUSEKEEPINGDIR, pid );
	} else {
		char *tmphkdir;
		tmphkdir = open_pathalloc( homedir, hkd, 0 );
		hkdir = open_pathalloc( tmphkdir, HOUSEKEEPINGDIR, pid );
		free( ( void * )tmphkdir );
	}

	/* set my file creation mask to zero, to avoid modifying the
	 * dumped mode bits
	 */
	( void )umask( 0 );

	/* generate a pathname for the state file
	 */
	statepath = open_pathalloc( hkdir, STATEFILE, pid );

	/* attempt to open an existing state file.
	 * this should only succeed if a dump was interrupted or
	 * we had been doing a cumulative dump.
	 */
	statep = state_open( statepath );
	if ( resumed_restore ) {
		if ( ! statep ) {
			mlog( MLOG_NORMAL,
			      "resume option (%c) invalid: "
			      "no previous restore to resume\n",
			      GETOPT_RESUME );
			return BOOL_FALSE;
		}
		if ( statep->s_interruptedstreamcnt == 0 ) {
			mlog( MLOG_NORMAL,
			      "resume option (%c) invalid: "
			      "previous restore was not interrupted\n",
			      GETOPT_RESUME );
			return BOOL_FALSE;
		}
	}
	if ( ! resumed_restore && statep && statep->s_interruptedstreamcnt ) {
		mlog( MLOG_NORMAL,
		      "the previous restore into this directory "
		      "was interrupted: "
		      "use resume (-%c) option, "
		      "or rm -rf %s\n",
		      GETOPT_RESUME,
		      hkdir );
		return BOOL_FALSE;
	}
	if ( statep && ! cumulative && ! resumed_restore ) {
		mlog( MLOG_NORMAL,
		      "%s present: remove prior to restore\n",
		      hkdir );
		return BOOL_FALSE;
	}
	if ( statep && statep->s_streamcnt != csp->cs_contentcnt ) {
		mlog( MLOG_NORMAL,
		      "previous restore stream count mismatch: "
		      "previously %d, now %u\n",
		      statep->s_streamcnt,
		      csp->cs_contentcnt );
		return BOOL_FALSE;
	}
	if ( statep && statep->s_cumulative && ! cumulative )
	{
		mlog( MLOG_NORMAL,
		      "the destination dir contains a cumulative restore: "
		      "must use the -%c option, "
		      "or remove %s\n",
		      GETOPT_CUMULATIVE,
		      hkdir );
		return BOOL_FALSE;
	}
	if ( statep && ! statep->s_cumulative && cumulative )
	{
		mlog( MLOG_NORMAL,
		      "the destination dir contains a partial restore, "
		      "which was not initiated with the -%c option\n",
		      GETOPT_CUMULATIVE );
		return BOOL_FALSE;
	}

	/* load the strategy-specific context
	 */
	scontextp->sc_csp = csp;
	scontextp->sc_hkdir = hkdir;
	scontextp->sc_statepath = statepath;
	scontextp->sc_statep = statep;
	scontextp->sc_isxfs = isxfs;

	/* give each content manager its operators, patch in
	 * content-specific header info, and call each manager's
	 * create operator
	 */
	for ( contentix = 0 ; contentix < csp->cs_contentcnt ; contentix++ ) {
		content_t *contentp;
		context_t *contextp;

		contentp = csp->cs_contentp[ contentix ];
		contentp->c_opsp = &content_ops;
		contextp = ( context_t * )calloc( 1, sizeof( context_t ));
		assert( contextp );
		contextp->cc_hkdir = hkdir;
		contextp->cc_statep = statep;
		contentp->c_contextp = contextp;

		/* init per-stream stat stat display state
		 */
		contextp->cc_statstate = ( statstate_t )STATE_NONE;

		ok = ( * contentp->c_opsp->co_create )( contentp );
		if ( ! ok ) {
			return BOOL_FALSE;
		}
	}
	/* turn on SIGINT handler status reporting
	 */
	csp->cs_stat = s_stat;


	return BOOL_TRUE;
}

static bool_t
s_init( content_strategy_t *csp, int argc, char *argv[] )
{
	scontext_t *scontextp = ( scontext_t * )csp->cs_contextp;
	content_t *contentp = csp->cs_contentp[ 0 ];
	global_hdr_t *grhdrp = contentp->c_greadhdrp;
	content_hdr_t *crhdrp = contentp->c_readhdrp;
	content_inode_hdr_t *scrhdrp = ( content_inode_hdr_t * )
				       crhdrp->ch_specific;
	intgen_t level = scrhdrp->cih_level;
	size_t contentix;
	char *hkdir = scontextp->sc_hkdir;
	char *statepath = scontextp->sc_statepath;
	state_t *statep = scontextp->sc_statep;
	stat_t statbuf;
	bool_t cumulative =
	    contentp->c_strategyp->cs_flags & CONTENT_STRATEGY_FLAGS_CUMULATIVE;
	bool_t resumed_restore =
	    contentp->c_strategyp->cs_flags & CONTENT_STRATEGY_FLAGS_RESUME;
	subtree_t *subtreep = scontextp->sc_subtreep;
	bool_t inter = scontextp->sc_inter;
	int32_t dumpattr;
	bool_t incr;
	bool_t resume;
	bool_t delta;
	intgen_t rval;
	bool_t ok;

	/* the read hdr is now available: see if the strategy id matches
	 */
	if ( content_strategy_inode.cs_id != crhdrp->ch_strategyid ) {
		mlog( MLOG_NORMAL,
		      "unrecognized content strategy id: %d\n",
		      crhdrp->ch_strategyid );
		return BOOL_FALSE;
	}

	/* see if the fs type matches
	 */
	if ( strcmp( crhdrp->ch_fstype, "xfs" )) {
		mlog( MLOG_NORMAL,
		      "unrecognized file system type: %s\n",
		      crhdrp->ch_fstype );
		return BOOL_FALSE;
	}

	/* extract the dump attributes from the read header
	 */
	dumpattr = scrhdrp->cih_dumpattr;
	incr = ( dumpattr & CIH_DUMPATTR_INCREMENTAL ) ? BOOL_TRUE
						       : BOOL_FALSE;
	resume = ( dumpattr & CIH_DUMPATTR_RESUME ) ? BOOL_TRUE
						    : BOOL_FALSE;

	/* determine if this is a delta, to be applied to the current
	 * contents of the destination. incremental and resumed dumps
	 * are both deltas.
	 */
	delta = incr || resume;

	/* validate this is a sensible media file
	 */
	if ( crhdrp->ch_strategyid != contentp->c_strategyp->cs_id ) {
		mlog( MLOG_NORMAL,
		      "unrecognized content strategy ID (%d)\n",
		      contentp->c_strategyp->cs_id );
		return BOOL_FALSE;
	}

	/* create the housekeeping directory, complaining if it
	 * already exists, unless this is a delta being applied to
	 * a cumulative restore, or we are resuming a restore,
	 * in which case it must already exist.
	 */
	rval = lstat( hkdir, &statbuf );
	if ( cumulative && delta ) {
		if ( rval  || ( statbuf.st_mode & S_IFMT ) != S_IFDIR ) {
			mlog( MLOG_NORMAL,
			      "cumulative (-%c) restores must begin with "
			      "a level 0 dump: "
			      "this dump is a level %d %s dump\n",
			      GETOPT_CUMULATIVE,
			      level,
			      incr ? "incremental" : "resumed" );
			return BOOL_FALSE;
		}
	} else if ( ! resumed_restore ) {
		if ( ! rval ) {
			mlog( MLOG_NORMAL,
			      "%s present: remove prior to restore\n",
			      hkdir );
			return BOOL_FALSE;
		}
	}
	if ( ! cumulative ) {
		( void )cleanup_register( hkd_abort_cleanup, scontextp, 0 );
	}
	if ( ! ( cumulative && delta ) && ! resumed_restore ) {
		mlog( MLOG_DEBUG,
		      "creating %s\n",
		      hkdir );
		rval = mkdir_tp( hkdir );
		if ( rval ) {
			mlog( MLOG_NORMAL,
			      "could not create %s: %s\n",
			      hkdir,
			      strerror( errno ));
			return BOOL_FALSE;
		}
	}

	/* if this is not a resumed restore and is a base cumulative,
	 * create a base state
	 */
	if ( ! resumed_restore && ( ! cumulative || ! delta )) {
		assert( ! statep );
		statep = state_create( statepath,
				       csp->cs_contentcnt,
				       cumulative );
		scontextp->sc_statep = statep;
		for ( contentix = 0
		      ;
		      contentix < csp->cs_contentcnt
		      ;
		      contentix++ ) {
			content_t *contentp;
			context_t *contextp;
			contentp = csp->cs_contentp[ contentix ];
			contextp = ( context_t * )contentp->c_contextp;
			contextp->cc_statep = statep;
		}

	}

	/* in any case, the state must now exist
	 */
	assert( statep );

	/* tell the reader what this is
	 */
	mlog( MLOG_VERBOSE,
	      "%s%s%s%srestore of level %d %sdump of %s:%s %s %s\n",
	      resumed_restore ? "resuming " : "",
	      cumulative ? "cumulative " : "",
	      inter ? "interactive " : "",
	      subtreep ? "subtree " : "",
	      level,
	      incr ? "incremental " : "",
	      grhdrp->gh_hostname,
	      crhdrp->ch_mntpnt,
	      resume ? "resumed" : "created",
	      ctimennl( & grhdrp->gh_timestamp ));

	for ( contentix = 0 ; contentix < csp->cs_contentcnt ; contentix++ ) {
		content_t *contentp;

		contentp = csp->cs_contentp[ contentix ];

		ok = ( * contentp->c_opsp->co_init )( contentp );
		if ( ! ok ) {
			return BOOL_FALSE;
		}
	}

	return BOOL_TRUE;
}

static size_t
s_stat( content_strategy_t *csp,
	char statln[ ][ DLOG_MULTI_STATSZ ],
	size_t lnmax )
{
	content_t *contentp;
	context_t *contextp;
	double percent;
	time_t elapsed;

	if ( lnmax < 1 ) {
		return 0;
	}

	contentp = csp->cs_contentp[ 0 ];
	if ( ! contentp ) {
		return 0;
	}

	contextp = ( context_t * )contentp->c_contextp;
	if ( ! contextp ) {
		return 0;
	}

	if ( contextp->cc_statstate == ( statstate_t )STATE_NONE ) {
		percent = 0.0;
	} else if ( contextp->cc_datasz != 0 ) {
		percent = ( double )contextp->cc_datadone
			  /
			  ( double )contextp->cc_datasz;
		percent *= 100.0;
	} else {
		percent = 100.0;
	}
	if ( percent > 100.0 ) {
		percent = 100.0;
	}

	elapsed = time( 0 ) - contentp->c_gwritehdrp->gh_timestamp;

	switch ( contextp->cc_statstate ) {
	case ( statstate_t )STATE_NONE:
	case ( statstate_t )STATE_DIR:
	case ( statstate_t )STATE_NONDIR:
		if ( contextp->cc_datasz ) {
			sprintf( statln[ 0 ],
				 "status: %lld/%lld files restored, "
				 "%.2lf percent complete, "
				 "%u seconds elapsed\n",
				 contextp->cc_statdone,
				 contextp->cc_statcnt,
				 percent,
				 elapsed );
		} else {
			sprintf( statln[ 0 ],
				 "status: %lld/%lld files restored, "
				 "%u seconds elapsed\n",
				 contextp->cc_statdone,
				 contextp->cc_statcnt,
				 elapsed );
		}
		assert( strlen( statln[ 0 ] ) < DLOG_MULTI_STATSZ );
		return 1;
	default:
		return 0;
	}
}

static void
s_complete( content_strategy_t *csp )
{
}

/* init - do content stream initialization
 */
static bool_t
create( content_t *contentp )
{
	media_t *mediap = contentp->c_mediap;
	media_ops_t *mop = mediap->m_opsp;
	bool_t resumed_restore =
	    contentp->c_strategyp->cs_flags & CONTENT_STRATEGY_FLAGS_RESUME;
	context_t *contextp = ( context_t * )contentp->c_contextp;

	/* allocate a buffer for sym links
	 */
	contextp->cc_symlinkbufsz = MAXPATHLEN + SYMLINK_ALIGN;
	contextp->cc_symlinkbufp =
			    ( char * )calloc( 1, contextp->cc_symlinkbufsz );
	assert( contextp->cc_symlinkbufp );

	/* give media layer resume info
	 */
	if ( resumed_restore && mop->mo_resume ) {
		state_t *statep = contextp->cc_statep;
		media_t *mediap = contentp->c_mediap;
		media_ops_t *mop = mediap->m_opsp;
		drive_t *drivep = mediap->m_drivep;
		intgen_t streamix = drivep->d_index;
		statestream_t *ssp;
		assert( statep );
		ssp = &statep->s_stream[ streamix ];
		( * mop->mo_resume )( mediap,
				      &ssp->ss_mediaid,
				      ssp->ss_mediafileix );
	}

	return BOOL_TRUE;
}

static bool_t
init( content_t *contentp )
{
	content_hdr_t *crhdrp = contentp->c_readhdrp;
	content_inode_hdr_t *scrhdrp = ( content_inode_hdr_t * )
				       crhdrp->ch_specific;
	intgen_t level = scrhdrp->cih_level;
	context_t *contextp = contentp->c_contextp;
	intgen_t streamix = contentp->c_mediap->m_drivep->d_index;
	state_t *statep = contextp->cc_statep;
	statestream_t *ssp;
	bool_t cumulative =
	    contentp->c_strategyp->cs_flags & CONTENT_STRATEGY_FLAGS_CUMULATIVE;
	bool_t resumed_restore =
	    contentp->c_strategyp->cs_flags & CONTENT_STRATEGY_FLAGS_RESUME;
	int32_t dumpattr = scrhdrp->cih_dumpattr;
	bool_t incr = ( dumpattr & CIH_DUMPATTR_INCREMENTAL ) ? BOOL_TRUE
							      : BOOL_FALSE;
	bool_t resume = ( dumpattr & CIH_DUMPATTR_RESUME ) ? BOOL_TRUE
							   : BOOL_FALSE;

	/* retrieve the non-metadata size and init the accum
	 */
	contextp->cc_datasz = scrhdrp->cih_inomap_datasz;
	contextp->cc_datadone = 0;

	if ( ! cumulative || resumed_restore ) {
		return BOOL_TRUE;
	}

	/* validate the uuid of the delta
	 */
	assert( statep );
	ssp = &statep->s_stream[ streamix ];
	if ( resume ) {
		uuid_t *baseidp = &scrhdrp->cih_resume_id;
		u_int32_t uuidstat;
		if ( ! uuid_equal( &ssp->ss_sessionid, baseidp, &uuidstat )) {
			mlog( MLOG_NORMAL,
			      "last accumulated dump does not match base "
			      " of this resumed dump\n" );
			return BOOL_FALSE;
		}
	} else if ( incr ) {
		uuid_t *baseidp = &scrhdrp->cih_last_id;
		u_int32_t uuidstat;
		assert( level > 0 );
		if ( ! uuid_equal( &ssp->ss_sessionid, baseidp, &uuidstat )) {
			mlog( MLOG_NORMAL,
			      "last accumulated dump does not match base "
			      " of this incremental dump\n" );
			return BOOL_FALSE;
		}
	}

	return BOOL_TRUE;
}

/* begin_stream - called by stream process to invoke the restore stream
 */
static intgen_t
begin_stream( content_t *contentp, size_t vmsize )
{
	context_t *contextp = ( context_t * )contentp->c_contextp;
	media_t *mediap = contentp->c_mediap;
	intgen_t streamix = mediap->m_drivep->d_index;
	state_t *statep = contextp->cc_statep;
	statestream_t *ssp;
	media_ops_t *mop = mediap->m_opsp;
	content_hdr_t *crhdrp = contentp->c_readhdrp;
	content_hdr_t *cwhdrp = contentp->c_writehdrp;
	global_hdr_t *grhdrp = contentp->c_greadhdrp;
	content_inode_hdr_t *scrhdrp = ( content_inode_hdr_t * )
				       crhdrp->ch_specific;
	media_hdr_t *mrhdrp = mediap->m_readhdrp;
	bool_t toc =
	    contentp->c_strategyp->cs_flags & CONTENT_STRATEGY_FLAGS_TOC;
	bool_t cumulative =
	    contentp->c_strategyp->cs_flags & CONTENT_STRATEGY_FLAGS_CUMULATIVE;
	char *hkdir = contextp->cc_hkdir;
	scontext_t *scontextp =
	    ( scontext_t * )contentp->c_strategyp->cs_contextp;
	subtree_t *subtreep = scontextp->sc_subtreep;
	bool_t inter = scontextp->sc_inter;
	tree_t *treep;
	char *dstdir;
	filehdr_t fhdr;
	direnthdr_t *dhdrp;
	size_t direntbufsz;
	intgen_t xcode;
	bool_t delta;
	bool_t need_map_and_dirs;
	bool_t need_resync;
	bool_t fhcs;
	bool_t ehcs;
	bool_t dhcs;
	ino64_t ino_uc;
	off64_t off_uc;
	intgen_t rval;
	bool_t need_to_end_read;
	time_t elapsed;

	assert( statep );
	ssp = &statep->s_stream[ streamix ];

	/* extract the destination directory from the write header
	 * (it was placed there by content_init).
	 */
	dstdir = cwhdrp->ch_mntpnt;

	/* determine if this is a delta, to be applied to the current
	 * contents of the destination. incremental and resumed dumps
	 * are both deltas.
	 */
	if ( scrhdrp->cih_dumpattr
	     &
	     ( CIH_DUMPATTR_INCREMENTAL | CIH_DUMPATTR_RESUME )) {

		delta = BOOL_TRUE;
	} else {
		delta = BOOL_FALSE;
	}
	
	/* determine what hdr checksums are available
	 */
	fhcs = ( scrhdrp->cih_dumpattr & CIH_DUMPATTR_FILEHDR_CHECKSUM )
	       ?
	       BOOL_TRUE
	       :
	       BOOL_FALSE;
	ehcs = ( scrhdrp->cih_dumpattr & CIH_DUMPATTR_EXTENTHDR_CHECKSUM )
	       ?
	       BOOL_TRUE
	       :
	       BOOL_FALSE;
	dhcs = ( scrhdrp->cih_dumpattr & CIH_DUMPATTR_DIRENTHDR_CHECKSUM )
	       ?
	       BOOL_TRUE
	       :
	       BOOL_FALSE;

	/* set the current directory to dstdir. the tree abstraction
	 * depends on the current directory being the root of the
	 * destination file system.
	 */
	rval = chdir( dstdir );
	if ( rval ) {
		mlog( MLOG_NORMAL,
		      "chdir %s failed: %s\n",
		      dstdir,
		      strerror( errno ));
		return STREAM_EXIT_ABORT;
	}

	/* set my file creation mask to zero, to avoid modifying the
	 * dumped mode bits
	 */
	( void )umask( 0 );

	/* set up a directory entry buffer. must be big enough
	 * to hold a direnthdr_t, and maximum length name,
	 * and a NULL termination.
	 */
	direntbufsz = sizeof( direnthdr_t )
		      +
		      MAXNAMELEN
		      +
		      DIRENTHDR_ALIGN;
	dhdrp = ( direnthdr_t * )calloc( 1, direntbufsz );
	assert( dhdrp );

	/* tell the media layer to begin a stream
	 */
	rval = ( * mediap->m_opsp->mo_begin_stream )( mediap );
	if ( rval ) {
		if ( rval == MEDIA_ERROR_ABORT ) {
			return STREAM_EXIT_ABORT;
		} else {
			return STREAM_EXIT_CORE;
		}
	}

	/* read and restore media files until the media layer
	 * tells me there are no more
	 */
	if ( ! toc && ! inter ) {
		ssp->ss_interrupted = BOOL_TRUE;
	}
	xcode = XCODE_INPROGRESS;
	need_map_and_dirs = BOOL_TRUE;
	need_resync = BOOL_FALSE;
	need_to_end_read = BOOL_FALSE;
	ino_uc = 0;
	off_uc = 0;
	treep = 0;
	for ( ; ; ) {
		assert( xcode == XCODE_INPROGRESS );

		if ( ! need_resync ) {
			intgen_t mft;

			/* tell the media manager to begin a new media file
			 */
			mlog( MLOG_VERBOSE,
			      "beginning media file\n" );

			rval = ( * mediap->m_opsp->mo_begin_read )( mediap );
			if ( rval ) {
				xcode = XCODE_RVAL;
				assert( need_to_end_read == BOOL_FALSE );
				goto decision;
			}
			need_to_end_read = BOOL_TRUE;

			mlog( MLOG_VERBOSE,
			      "media file %u (media %u, file %u)\n",
			      mrhdrp->mh_dumpfileix,
			      mrhdrp->mh_mediaix,
			      mrhdrp->mh_mediafileix );

			/* update the stream state (for restartable restores)
			 */
			ssp->ss_sessionid = grhdrp->gh_dumpid;
			ssp->ss_mediaid = mrhdrp->mh_mediaid;
			ssp->ss_mediafileix = mrhdrp->mh_mediafileix;

			/* validate the read header
			 */
			mlog( MLOG_TRACE,
			      "validating the media file header\n" );
			if ( crhdrp->ch_strategyid
			     !=
			     contentp->c_strategyp->cs_id ) {
				mlog( MLOG_NORMAL,
				      "unrecognized content strategy ID (%d)\n",
				      contentp->c_strategyp->cs_id );
				xcode = XCODE_ABORT;
				goto decision;
			}
			mft = scrhdrp->cih_mediafiletype;
			if ( mft != CIH_MEDIAFILETYPE_DATA ) {
				char *mftstr;

				switch ( mft ) {
				case CIH_MEDIAFILETYPE_INDEX:
					mftstr = "index";
					break;
				case CIH_MEDIAFILETYPE_INVENTORY:
					mftstr = "inventory";
					break;
				default:
					mftstr = "unknown";
					break;
				}

				mlog( MLOG_VERBOSE,
				      "skipping %s media file\n",
				      mftstr );
				xcode = XCODE_NEXT;
				goto decision;
			}
		}

		if ( need_map_and_dirs ) {

			/* verify that we don't attemt to resync until
			 * we've successfully read the inomap and dir dump.
			 */
			assert( ! need_resync );

			/* read the inomap
			 */
			mlog( MLOG_VERBOSE,
			      "reading ino map\n" );
			rval = inomap_restore( contentp );
			if ( rval ) {
				xcode = XCODE_RVAL;
				goto decision;
			}

			/* initialize the tree
			 */
			mlog( MLOG_VERBOSE,
			      "initializing the map tree\n" );
			treep = tree_init( subtreep,
					   inter,
					   toc,
					   cumulative,
					   delta,
					   scrhdrp->cih_rootino,
					   scrhdrp->cih_inomap_firstino,
					   scrhdrp->cih_inomap_lastino,
					   scrhdrp->cih_inomap_dircnt,
					   scrhdrp->cih_inomap_nondircnt,
					   hkdir,
					   vmsize,
					   contentp->c_strategyp );
			if ( ! treep ) {
				xcode = XCODE_ABORT;
				goto decision;
			}

			/* turn on directory status reporting
			 */
			contextp->cc_statdone = 0;
			contextp->cc_statcnt = scrhdrp->cih_inomap_nondircnt;
			contextp->cc_statstate = ( statstate_t )STATE_DIR;

			/* read the directories, and build the tree
			 */
			mlog( MLOG_VERBOSE,
			      "reading the directory hierarchy\n" );
			for ( ; ; ) {
				/* read the file header
				 */
				rval = read_filehdr( mediap, &fhdr, fhcs );
				if ( rval ) {
					xcode = XCODE_RVAL_HDR;
					goto decision;
				}

				/* if this is a null file hdr, we're done
				 * reading dirs, and there are no nondirs.
				 * break out, where tree construction will
				 * be done.
				 */
				if ( fhdr.fh_flags & FILEHDR_FLAGS_NULL ) {
					break;
				}

				/* update the record of ino/offset under
				 * construction
				 */
				ino_uc = fhdr.fh_stat.bs_ino;
				off_uc = fhdr.fh_offset;

				/* if its not a directory, must be the
				 * first non-dir file. break out for tree
				 * construction.
				 */
				if ( ( fhdr.fh_stat.bs_mode & S_IFMT )
				     !=
				     S_IFDIR ) {
					break;
				}

				/* if stop requested bail out gracefully
				 */
				if ( cldmgr_stop_requested( )) {
					mlog( MLOG_NORMAL,
					      "stop requested:"
					      " restore terminated "
					      "prematurely\n" );
					xcode = XCODE_STOP;
					break;
				}

				/* add the directory to the tree. save the
				 * returned tree node id, to associate with
				 * the directory entries.
				 */
				tree_begindir( treep, &fhdr );

				/* read the directory entries, and populate the
				 * tree with them. we can tell when we are done
				 * by looking for a null dirent.
				 */
				for ( ; ; ) {
					register size_t namelen;

					rval = read_dirent( mediap,
							    dhdrp,
							    direntbufsz,
							    dhcs );
					if ( rval ) {
						xcode = XCODE_RVAL;
						break;
					}

					/* if null, we're done with this dir.
					 * break out of this inner loop and
					 * move on th the next dir.
					 */
					if ( dhdrp->dh_ino == 0 ) {
						break;
					}
					namelen = strlen( dhdrp->dh_name );
					assert( namelen < MAXNAMELEN );

					/* add this dirent to the tree.
					 */
					tree_addent( treep,
						     dhdrp->dh_ino,
						     dhdrp->dh_name,
						     namelen );
				}
				tree_enddir( treep );
				if ( xcode != XCODE_INPROGRESS ) {
					goto decision;
				}
			}

			/* at this point we've successfully read the dirdump
			 * and built the directory tree. do subtree post-
			 * processing if needed.
			 */
			need_map_and_dirs = BOOL_FALSE;
			if ( subtreep || inter ) {
				bool_t quitnow;
				quitnow = tree_subtree_complete( treep );
				if ( quitnow ) {
					xcode = XCODE_STOP_INTER;
					goto decision;
				}
			}

			/* turn on non-directory status reporting
			 */
			contextp->cc_statstate = ( statstate_t )STATE_NONDIR;

		} else {
			/* we arrive here if we're in a subsequent media file
			 * and we've already read the inomap and dirdump,
			 * or we're in the same media file but encountered
			 * media corruption during the non-dir restore.
			 * seek to the next valid mark, and resume.
			 */
			if ( ! mop->mo_next_mark ) {
				if ( need_resync ) {
					mlog( MLOG_NORMAL,
					      "media not capable of "
					      "resynchronization: "
					      "aborting restore\n" );
					xcode = XCODE_ABORT;
				} else {
					xcode = XCODE_DONE;
				}
				goto decision;
			}
			rval = ( * mop->mo_next_mark )( mediap );
			if ( rval ) {
				if ( need_resync ) {
					mlog( MLOG_NORMAL,
					      "WARNING: "
					      "resynchronization failed: "
					      "advancing to "
					      "next media file\n" );
					need_resync = BOOL_FALSE;
				}
				xcode = XCODE_RVAL;
				goto decision;
			}

			/* read the file header
			 */
			rval = read_filehdr( mediap, &fhdr, fhcs );
			if ( rval ) {
				xcode = XCODE_RVAL_HDR;
				goto decision;
			}

			/* update the record of ino/offset under
			 * construction
			 */
			ino_uc = fhdr.fh_stat.bs_ino;
			off_uc = fhdr.fh_offset;
			
			/* found the next valid file header.
			 * if this was a re-sync, tell operator
			 * what was lost.
			 */
			if ( need_resync ) {
				need_resync = BOOL_FALSE;
				mlog( MLOG_NORMAL,
				      "WARNING: "
				      "resynchronization achieved at "
				      "ino %llu offset %lld\n",
				      fhdr.fh_stat.bs_ino,
				      fhdr.fh_offset );
			}
		}

		/* the remainder of the media contains dumped non-directory
		 * files. pull 'em off and populate the real directory tree.
		 * note that we've already read the header of the first non-dir
		 * file.
		 */
		if ( ! toc ) {
			mlog( MLOG_VERBOSE,
			      "restoring non-directory files\n" );
		} else {
			mlog( MLOG_VERBOSE,
			      "reading non-directory files\n" );
		}
		for ( ; ; ) {
			/* if this is a null file hdr, this media file
			 * is restored. go get another one.
			 */
			if ( fhdr.fh_flags & FILEHDR_FLAGS_NULL ) {
				if ( fhdr.fh_flags & FILEHDR_FLAGS_END ) {
					xcode = XCODE_DONE;
				} else {
					xcode = XCODE_NEXT;
				}
				goto decision;
			}
				
			/* if stop requested bail out gracefully
			 */
			if ( cldmgr_stop_requested( )) {
				mlog( MLOG_NORMAL,
				      "stop requested:"
				      " restore terminated prematurely\n" );
				xcode = XCODE_STOP;
				goto decision;
			}

			rval = restore_file( contentp, &fhdr, treep, ehcs );
			if ( rval ) {
				xcode = XCODE_RVAL;
				goto decision;
			}

			/* read the next header.
			 */
			rval = read_filehdr( mediap, &fhdr, fhcs );
			if ( rval ) {
				xcode = XCODE_RVAL_HDR;
				goto decision;
			}

			/* update the record of ino/offset under construction
			 */
			if ( fhdr.fh_stat.bs_ino != ino_uc ) {
				contextp->cc_statdone++;
			}
			ino_uc = fhdr.fh_stat.bs_ino;
			off_uc = fhdr.fh_offset;
		}
		assert( 0 );
decision:
		/* convert media layer return codes into xcodes
		 */
		if ( xcode == XCODE_RVAL || xcode == XCODE_RVAL_HDR ) {
			assert( rval );
			switch ( rval ) {
			case MEDIA_ERROR_CORRUPTION:
				if ( xcode == XCODE_RVAL ) {
					xcode = XCODE_CORRUPTION;
				} else {
					xcode = XCODE_CORRUPTION_HDR;
				}
				break;
			case MEDIA_ERROR_EOF:
			case MEDIA_ERROR_EOM:
				xcode = XCODE_NEXT;
				break;
			case MEDIA_ERROR_EOD:
				xcode = XCODE_DONE;
				break;
			case MEDIA_ERROR_STOP:
			case MEDIA_ERROR_TIMEOUT:
				xcode = XCODE_STOP;
				break;
			case MEDIA_ERROR_ABORT:
				xcode = XCODE_ABORT;
				break;
			case MEDIA_ERROR_CORE:
			default:
				xcode = XCODE_CORE;
				break;
			}
		}

		if ( xcode == XCODE_CORRUPTION ) {
			if ( need_map_and_dirs ) {
				assert( ! need_resync );
				mlog( MLOG_NORMAL,
				      "WARNING: media corruption encountered "
				      "during directory restore at ino %llu: "
				      " skipping remainder of media file\n",
				      ino_uc );
			} else {
				mlog( MLOG_NORMAL,
				      "WARNING: media corruption encountered "
				      "at ino %llu offset %lld: "
				      "resynchronizing\n",
				      ino_uc,
				      off_uc );
				need_resync = BOOL_TRUE;
			}

			/* check here for stop request: bail out gracefully
			 */
			if ( cldmgr_stop_requested( )) {
				mlog( MLOG_NORMAL,
				      "stop requested:"
				      " restore terminated "
				      "prematurely\n" );
				xcode = XCODE_STOP;
			} else {
				xcode = XCODE_INPROGRESS;
			}
		}

		if ( xcode == XCODE_CORRUPTION_HDR ) {
			if ( need_map_and_dirs ) {
				assert( ! need_resync );
				mlog( MLOG_NORMAL,
				      "WARNING: media corruption encountered "
				      "during "
				      "directory restore in directory header "
				      "after ino %llu: "
				      " skipping remainder of media file\n",
				      ino_uc );
			} else {
				mlog( MLOG_NORMAL,
				      "WARNING: media corruption encountered "
				      "in file header after "
				      "ino %llu offset %lld: "
				      "resynchronizing\n",
				      ino_uc,
				      off_uc );
				need_resync = BOOL_TRUE;
			}

			/* check here for stop request: bail out gracefully
			 */
			if ( cldmgr_stop_requested( )) {
				mlog( MLOG_NORMAL,
				      "stop requested:"
				      " restore terminated "
				      "prematurely\n" );
				xcode = XCODE_STOP;
			} else {
				xcode = XCODE_INPROGRESS;
			}
		}

		if ( need_to_end_read && ! need_resync ) {
			mlog( MLOG_VERBOSE,
			      "ending media file\n" );
			( * mediap->m_opsp->mo_end_read )( mediap );
			need_to_end_read = BOOL_FALSE;
		}

		if ( xcode == XCODE_NEXT ) {
			xcode = XCODE_INPROGRESS;
		}

		if ( xcode == XCODE_DONE ) {
			break;
		}

		if ( xcode == XCODE_STOP ) {
			break;
		}

		if ( xcode == XCODE_STOP_INTER ) {
			break;
		}

		if ( xcode == XCODE_ABORT ) {
			return STREAM_EXIT_ABORT;
		}

		if ( xcode == XCODE_CORE ) {
			return STREAM_EXIT_CORE;
		}
	}

	/* done with the dirent buffer
	 */
	free( ( void * )dhdrp );

	/* tell the media manager we are done
	 */
	(* mediap->m_opsp->mo_end_stream )( mediap );

	/* go back and restore the directory attributes (mode, permission, etc.)
	 */
	if ( treep && ! toc && xcode != XCODE_STOP_INTER ) {
		mlog( MLOG_VERBOSE,
		      "restoring directory attributes\n" );
		tree_setattr( treep );
	}
	if ( treep ) {
		tree_complete( treep );
	}

	elapsed = time( 0 ) - contentp->c_gwritehdrp->gh_timestamp;

	if ( xcode == XCODE_STOP ) {
		mlog( MLOG_NORMAL,
		      "restore stopped prematurely: %u seconds elapsed\n",
		      elapsed );
		return STREAM_EXIT_STOP;
	}

	if ( xcode == XCODE_STOP_INTER ) {
		mlog( MLOG_NORMAL,
		      "interactive restore stopped prematurely: %u seconds elapsed\n",
		      elapsed );
		return STREAM_EXIT_STOP;
	}

	if ( xcode == XCODE_DONE ) {
		ssp->ss_interrupted = BOOL_FALSE;
		mlog( MLOG_VERBOSE,
		      "%s complete: %u seconds elapsed\n",
		      toc ? "listing of contents" : "restore",
		      elapsed );
		return STREAM_EXIT_SUCCESS;
	}

	/* can't get here
	 */
	assert( 0 );
	return STREAM_EXIT_CORE;
}

/* restore_file - knows how to restore non-directory files
 *
 * uses the tree's callback iterator, which will call me for each
 * link to the specified inode.
 */
struct cb_context {
	content_t *cb_contentp;
	filehdr_t *cb_fhdrp;
	intgen_t cb_stat;
	bool_t cb_ehcs;
};

typedef struct cb_context cb_context_t;

static intgen_t
restore_file( content_t *contentp, filehdr_t *fhdrp, tree_t *treep, bool_t ehcs)
{
	cb_context_t context;
	bool_t all_links_created;

	/* ask the tree to call me back for each link to this inode.
	 * my callback will restore the file the first time it is
	 * invoked, and create a hard link in subsequent calls.
	 */
	context.cb_contentp = contentp;
	context.cb_fhdrp = fhdrp;
	context.cb_stat = 0;
	context.cb_ehcs = ehcs;
	all_links_created = tree_cb_links( treep,
					   fhdrp->fh_stat.bs_ino,
					   restore_file_cb,
					   &context );
	
	if ( context.cb_stat ) {
		return context.cb_stat;
	} else if ( ! all_links_created ) {
		return MEDIA_ERROR_ABORT; /* media not synced */
	} else {
		return 0;
	}
}

static bool_t
restore_file_cb( void *p, char *path, char *firstpath )
{
	cb_context_t *contextp = ( cb_context_t * )p;
	content_t *contentp = contextp->cb_contentp;
	filehdr_t *fhdrp = contextp->cb_fhdrp;
	intgen_t *statp = &contextp->cb_stat;
	bool_t ehcs = contextp->cb_ehcs;
	bstat_t *statbufp = &fhdrp->fh_stat;
	bool_t toc =
		contentp->c_strategyp->cs_flags & CONTENT_STRATEGY_FLAGS_TOC;
	int rval;
	bool_t ok;

	if ( ! firstpath ) {
		/* call type-specific function to create the file
		 */
		switch( statbufp->bs_mode & S_IFMT ) {
		case S_IFREG:
			ok = restore_reg( contentp, fhdrp, statp, path, ehcs );
			if ( ! ok ) {
				return BOOL_FALSE;
			}
			break;
		case S_IFBLK:
		case S_IFCHR:
		case S_IFIFO:
		case S_IFNAM:
			ok = restore_spec( contentp, fhdrp, statp, path );
			if ( ! ok ) {
				return BOOL_FALSE;
			}
			break;
		case S_IFLNK:
			ok = restore_symlink( contentp,
					      fhdrp,
					      statp,
					      path,
					      ehcs );
			if ( ! ok ) {
				return BOOL_FALSE;
			}
			break;
		default:
			mlog( MLOG_NORMAL,
			      "WARNING: ino %llu: unknown file type: %08x\n",
			      statbufp->bs_ino,
			      statbufp->bs_mode );
			return BOOL_FALSE;
		/* not yet implemented
		case S_IFMNT:
		 */
		}
	} else {
		if ( ! toc ) {
			char *reasonstr;
			if ( content_overwrite_ok( contentp->c_strategyp,
						   path,
						   statbufp->bs_ctime.tv_sec,
						   statbufp->bs_mtime.tv_sec,
						   &reasonstr )) {
				mlog( MLOG_TRACE,
				      "linking %s to %s\n",
				      firstpath,
				      path );
				rval = unlink( path );
				if ( rval && errno != ENOENT ) {
					mlog( MLOG_VERBOSE,
					      "WARNING: unable to unlink "
					      "current file prior to linking "
					      "%s to %s:"
					      " %s\n",
					      firstpath,
					      path,
					      strerror( errno ));
				} else {
					rval = link( firstpath, path );
					if ( rval ) {
						mlog( MLOG_NORMAL,
						      "WARNING: attempt to "
						      "link %s to %s failed:"
						      " %s\n",
						      firstpath,
						      path,
						      strerror( errno ));
					}
				}
			} else {
				mlog( MLOG_NORMAL,
				      "NOT: linking %s to %s: %s\n",
				      firstpath,
				      path,
				      reasonstr );
			}
		} else {
			mlog( MLOG_NORMAL | MLOG_STDOUT,
			      "%s\n",
			      path );
		}
	}

	return BOOL_TRUE;
}

static bool_t
restore_reg( content_t *contentp,
	     filehdr_t *fhdrp,
	     intgen_t *statp,
	     char *path,
	     bool_t ehcs )
{
	scontext_t *scontextp =
			    ( scontext_t * )contentp->c_strategyp->cs_contextp;
	context_t *contextp = ( context_t * )contentp->c_contextp;
	media_t *mediap = contentp->c_mediap;
	bool_t toc =
		contentp->c_strategyp->cs_flags & CONTENT_STRATEGY_FLAGS_TOC;
	bstat_t *bstatp = &fhdrp->fh_stat;
	bool_t isxfs = scontextp->sc_isxfs;
	subtree_t *subtreep = scontextp->sc_subtreep;
	bool_t inter = scontextp->sc_inter;
	intgen_t fd;
	intgen_t rval;
	bool_t ok;

	*statp = 0;

	if ( path ) {
		off64_t offset;
		stat64_t stat;
		struct fsxattr fsxattr;

		offset = fhdrp->fh_offset;
		if ( offset ) {
			if ( ! toc ) {
				mlog( MLOG_TRACE,
				      "restoring regular file ino %llu %s"
				      " (offset %lld)\n",
				      bstatp->bs_ino,
				      path,
				      offset );
			} else {
				mlog( MLOG_NORMAL | MLOG_STDOUT,
				      "%s (offset %lld)\n",
				      path,
				      offset );
			}
		} else {
			if ( ! toc ) {
				mlog( MLOG_TRACE,
				      "restoring regular file ino %llu %s\n",
				      bstatp->bs_ino,
				      path );
			} else {
				mlog( MLOG_NORMAL | MLOG_STDOUT,
				      "%s\n",
				      path );
			}
		}

		if ( ! toc ) {
			char *reasonstr;
			bool_t isrealtime;
			intgen_t oflags;

			if ( contextp->cc_lastino != bstatp->bs_ino ) {
				bool_t ok;

				ok = content_overwrite_ok( contentp->c_strategyp,
							   path,
							   bstatp->bs_ctime.tv_sec,
							   bstatp->bs_mtime.tv_sec,
							   &reasonstr );
				contextp->cc_lastino = bstatp->bs_ino;
				contextp->cc_lastoverwrite = ok;
			}

			if ( contextp->cc_lastoverwrite ) {
				stat_t statbuf;

				/* if not an ordinary file, unlink it
				 */
				if ( ! lstat( path, &statbuf )) {
					mode_t mode = statbuf.st_mode;
					if ( ( mode & S_IFMT ) != S_IFREG ) {
						rval = unlink( path );
						if ( rval ) {
							mlog( MLOG_NORMAL,
							      "WARNING: unable "
							      "to unlink "
							      "current file "
							      "(mode 0x%x) "
							      "prior to "
							      "creating "
							      "ordinary file "
							      "ino %llu %s: "
							      "%s: "
							      "discarding\n",
							      statbuf.st_mode,
							      fhdrp
							      ->
							      fh_stat.bs_ino,
							      path,
							      strerror( errno));
							fd = -1;
							goto bypass;
						}
					}
				}

				isrealtime = ( bool_t )( bstatp->bs_xflags
							 &
							 XFS_XFLAG_REALTIME );
				oflags = O_CREAT | O_RDWR;
				if ( isxfs && isrealtime ) {
					oflags |= O_DIRECT;
				}
				
				fd = open( path, oflags, S_IRUSR | S_IWUSR );
				if ( fd < 0 ) {
					mlog( MLOG_NORMAL,
					      "WARNING: open of %s failed: "
					      "%s: discarding ino %llu\n",
					      path,
					      strerror( errno ),
					      fhdrp->fh_stat.bs_ino );
					fd = -1;
				} else {
					rval = fstat64( fd, &stat );
					if ( rval != 0 ) {
						mlog( MLOG_VERBOSE,
						      "WARNING: attempt to stat %s "
						      "failed: %s\n",
						      path,
						      strerror( errno ));
					} else {
						if ( stat.st_size
						     >
						     bstatp->bs_size ) {
							mlog( MLOG_TRACE,
							      "truncating %s"
							      " from %lld "
							      "to %lld\n",
							      path,
							      stat.st_size,
							      bstatp->bs_size );
							rval =
							   ftruncate64( fd,
							       bstatp->bs_size);
							if ( rval != 0 ) {
							mlog( MLOG_VERBOSE,
							      "WARNING: attempt"
							      " to truncate %s"
							      " failed: %s\n",
							      path,
							     strerror( errno ));
							}
						}
					}

					/* set the extended attributes
					 */
					if ( isxfs ) {
#ifdef F_FSSETDM
						fsdmidata_t fssetdm;
#endif /* F_FSSETDM */
						( void )memset((void *)&fsxattr,
								0,
							     sizeof( fsxattr ));
						fsxattr.fsx_xflags =
						    bstatp->bs_xflags;
						assert( bstatp->bs_extsize
							>=
							0 );
						fsxattr.fsx_extsize =
						    ( u_int32_t )
						    bstatp->bs_extsize;
						rval = fcntl( fd,
							      F_FSSETXATTR,
							     (void *)&fsxattr);
						if ( rval ) {
							mlog( MLOG_NORMAL,
							      "WARNING: attempt to set "
							      "extended "
							      "attributes "
							      "(xflags 0x%x, "
							      "extsize = 0x%x)"
							      "of %s failed: "
							      "%s\n",
							      bstatp->bs_xflags,
							     bstatp->bs_extsize,
							      path,
							      strerror(errno));
						}
#ifdef F_FSSETDM
						fssetdm.fsd_dmevmask =
							bstatp->bs_dmevmask;
						fssetdm.fsd_padding = 0;
						fssetdm.fsd_dmstate =
							bstatp->bs_dmstate;
						rval = fcntl( fd,
							      F_FSSETDM,
							      ( void * )
							      &fssetdm );
						if ( rval ) {
							mlog( MLOG_NORMAL,
							      "WARNING: attempt"
							      " to set "
							      "DMI "
							      "attributes "
							      "of %s failed: "
							      "%s\n",
							      path,
							      strerror(errno));
						}
#endif /* F_FSSETDM */
					}
				}
			} else {
				mlog( MLOG_NORMAL,
				      "NOT: restoring %s: %s\n",
				      path,
				      reasonstr );
				fd = -1;
			}
		} else {
			fd = -1;
		}
	} else {
		if ( ! subtreep && ! inter ) {
			mlog( MLOG_VERBOSE,
			      "WARNING: discarding regular file ino %llu: "
			      "no links\n",
			      bstatp->bs_ino );
		}
		fd = -1;
	}

bypass:

	/* copy data extents from media to the file
	 */
	ok = BOOL_TRUE;
	for ( ; ; ) {
		extenthdr_t ehdr;
		off64_t bytesread;

		/* read the extent header
		 */
		rval = read_extenthdr( mediap, &ehdr, ehcs );
		if ( rval ) {
			*statp = rval;
			ok = BOOL_FALSE;
			break;
		}
		mlog( MLOG_NITTY,
		      "read extent hdr type %s offset %lld sz %lld flags %x\n",
		      ehdr_typestr( ehdr.eh_type ),
		      ehdr.eh_offset,
		      ehdr.eh_sz,
		      ehdr.eh_flags );


		/* if we see the specially marked last extent hdr,
		 * we are done.
		 */
		if ( ehdr.eh_type == EXTENTHDR_TYPE_LAST ) {
			break;
		}

		/* if its an ALIGNment extent, discard the extent.
		 */
		if ( ehdr.eh_type == EXTENTHDR_TYPE_ALIGN ) {
			size_t sz;
			assert( ehdr.eh_sz <= INTGENMAX );
			sz = ( size_t )ehdr.eh_sz;
			rval = discard_padding( sz, mediap );
			if ( rval ) {
				*statp = rval;
				ok = BOOL_FALSE;
				break;
			}
			continue;
		}

		/* real data
		 */
		assert( ehdr.eh_type == EXTENTHDR_TYPE_DATA );
		bytesread = 0;
		ok = restore_extent( fhdrp,
				     &ehdr,
				     fd,
				     path,
				     mediap,
				     &bytesread,
				     statp );
		if ( ! ok ) {
			break;
		}
		contextp->cc_datadone += ( u_int64_t )bytesread;
	}

	if ( fd != -1 ) {
		struct utimbuf utimbuf;


		/* restore the attributes
		 */

		/* set the access and modification times
		 */
		utimbuf.actime =
			    ( time_t )bstatp->bs_atime.tv_sec;
		utimbuf.modtime =
			    ( time_t )bstatp->bs_mtime.tv_sec;
		rval = utime( path, &utimbuf );
		if ( rval ) {
			mlog( MLOG_VERBOSE,
			      "WARNING: unable to set access and modification"
			      " times of %s: %s\n",
			      path,
			      strerror( errno ));
		}

		/* set the permissions/mode
		 */
		rval = fchmod( fd, ( mode_t )bstatp->bs_mode );
		if ( rval ) {
			mlog( MLOG_VERBOSE,
			      "WARNING: unable to set mode "
			      "of %s: %s\n",
			      path,
			      strerror( errno ));
		}

		/* set the owner and group
		 */
		rval = fchown( fd,
			       ( uid_t )bstatp->bs_uid,
			       ( gid_t )bstatp->bs_gid);
		if ( rval ) {
			mlog( MLOG_VERBOSE,
			      "WARNING: unable to set owner and group "
			      "of %s: %s\n",
			      path,
			      strerror( errno ));
		}

		rval = close( fd );
		if ( rval ) {
			mlog( MLOG_NORMAL,
			      "WARNING: unable to close "
			      "%s: %s\n",
			      path,
			      strerror( errno ));
		}
	}

	return ok;
}

static bool_t
restore_spec(content_t *contentp, filehdr_t *fhdrp, intgen_t *statp, char *path)
{
	scontext_t *scontextp =
			    ( scontext_t * )contentp->c_strategyp->cs_contextp;
	bstat_t *bstatp = &fhdrp->fh_stat;
	subtree_t *subtreep = scontextp->sc_subtreep;
	bool_t inter = scontextp->sc_inter;
	bool_t toc =
		contentp->c_strategyp->cs_flags & CONTENT_STRATEGY_FLAGS_TOC;
	bstat_t *statbufp = &fhdrp->fh_stat;
	struct utimbuf utimbuf;
	char *printstr;
	intgen_t rval;

	*statp = 0;

	switch ( statbufp->bs_mode & S_IFMT ) {
	case S_IFBLK:
		printstr = "block special file";
		break;
	case S_IFCHR:
		printstr = "char special file";
		break;
	case S_IFIFO:
		printstr = "named pipe";
		break;
	case S_IFNAM:
		printstr = "XENIX named pipe";
		break;
	default:
		assert( 0 );
		return BOOL_FALSE;
	/* not yet implemented
	case S_IFUUID:
	case S_IFMNT:
	 */
	}


	if ( ! path ) {
		if ( ! subtreep && ! inter ) {
			mlog( MLOG_VERBOSE,
			      "WARNING: discarding %s ino %llu: no links\n",
			      printstr,
			      fhdrp->fh_stat.bs_ino );
		}
		return BOOL_TRUE;
	}

	if ( ! toc ) {
		mlog( MLOG_TRACE,
		      "restoring %s ino %llu %s\n",
		      printstr,
		      fhdrp->fh_stat.bs_ino,
		      path );
	} else {
		mlog( MLOG_NORMAL | MLOG_STDOUT,
		      "%s\n",
		      path );
	}

	if ( ! toc ) {
		bool_t ok;
		char *reasonstr;

		ok = content_overwrite_ok( contentp->c_strategyp,
					   path,
					   bstatp->bs_ctime.tv_sec,
					   bstatp->bs_mtime.tv_sec,
					   &reasonstr );
		if ( ok ) {
			/* unlink the node in case it already exists
			 */
			rval = unlink( path );
			if ( rval && errno != ENOENT ) {
				mlog( MLOG_VERBOSE,
				      "WARNING: unable to unlink "
				      "current file prior to creating "
				      "%s ino %llu %s: %s: discarding\n",
				      printstr,
				      fhdrp->fh_stat.bs_ino,
				      path,
				      strerror( errno ));
				return BOOL_TRUE;
			}

			/* create the node
			 */
			rval = mknod( path,
				      ( mode_t )statbufp->bs_mode,
				      ( dev_t )statbufp->bs_rdev );
			if ( rval && rval != EEXIST ) {
				mlog( MLOG_VERBOSE,
				      "WARNING: unable to create %s "
				      "ino %llu %s: %s: discarding\n",
				      printstr,
				      fhdrp->fh_stat.bs_ino,
				      path,
				      strerror( errno ));
				return BOOL_TRUE;
			}

			/* set the owner and group
			 */
			rval = chown( path,
				      ( uid_t )statbufp->bs_uid,
				      ( gid_t )statbufp->bs_gid );
			if ( rval ) {
				mlog( MLOG_VERBOSE,
				      "WARNING: unable to set owner and group of %s %s: "
				      "%s\n",
				      printstr,
				      path,
				      strerror( errno ));
			}

			/* set the permissions/mode
			 */
			rval = chmod( path, ( mode_t )fhdrp->fh_stat.bs_mode );
			if ( rval ) {
				mlog( MLOG_VERBOSE,
				      "WARNING: unable to set mode "
				      "of %s: %s\n",
				      path,
				      strerror( errno ));
			}

			/* set the access and modification times
			 */
			utimbuf.actime = ( time_t )statbufp->bs_atime.tv_sec;
			utimbuf.modtime = ( time_t )statbufp->bs_mtime.tv_sec;
			rval = utime( path, &utimbuf );
			if ( rval ) {
				mlog( MLOG_VERBOSE,
				      "WARNING: unable to set access and modification "
				      "times of %s: %s\n",
				      path,
				      strerror( errno ));
			}
		} else {
			mlog( MLOG_NORMAL,
			      "NOT: restoring %s: %s\n",
			      path,
			      reasonstr );
		}
	}

	return BOOL_TRUE;
}

static bool_t
restore_symlink( content_t *contentp,
		 filehdr_t *fhdrp,
		 intgen_t *statp,
		 char *path,
		 bool_t ehcs )
{
	scontext_t *scontextp =
			    ( scontext_t * )contentp->c_strategyp->cs_contextp;
	context_t *contextp = ( context_t * )contentp->c_contextp;
	bstat_t *bstatp = &fhdrp->fh_stat;
	media_t *mediap = contentp->c_mediap;
	media_ops_t *mop = mediap->m_opsp;
	extenthdr_t ehdr;
	intgen_t nread;
	bool_t toc =
		contentp->c_strategyp->cs_flags & CONTENT_STRATEGY_FLAGS_TOC;
	subtree_t *subtreep = scontextp->sc_subtreep;
	bool_t inter = scontextp->sc_inter;
	intgen_t rval;

	*statp = 0;

	if ( ! path ) {
		if ( ! subtreep && ! inter ) {
			mlog( MLOG_VERBOSE,
			      "WARNING: discarding symbolic link ino %llu: "
			      "no links\n",
			      fhdrp->fh_stat.bs_ino );
		}
	} else {
		if ( ! toc ) {
			mlog( MLOG_TRACE,
			      "restoring symbolic link ino %llu %s\n",
			      fhdrp->fh_stat.bs_ino,
			      path );
		} else {
			mlog( MLOG_NORMAL | MLOG_STDOUT,
			      "%s\n",
			      path );
		}
	}

	/* read the extent header
	 */
	rval = read_extenthdr( mediap, &ehdr, ehcs );
	if ( rval ) {
		*statp = rval;
		return BOOL_FALSE;
	}

	/* symlinks always have one extent
	 */
	assert( ehdr.eh_type == EXTENTHDR_TYPE_DATA );

	/* read the link path extent
	 */
	assert( ehdr.eh_sz <= ( off64_t )contextp->cc_symlinkbufsz );
	nread = read_buf( contextp->cc_symlinkbufp,
			  ( size_t )ehdr.eh_sz,
			  ( void * )mediap,
			  ( rfp_t )mop->mo_read,
			  ( rrbfp_t )mop->mo_return_read_buf,
			  &rval );
	if ( rval ) {
		return rval;
	}
	assert( ( off64_t )nread == ehdr.eh_sz );
	assert( strlen( contextp->cc_symlinkbufp )
		<
		contextp->cc_symlinkbufsz );

	if ( ! toc && path ) {
		bool_t ok;
		char *reasonstr;

		ok = content_overwrite_ok( contentp->c_strategyp,
					   path,
					   bstatp->bs_ctime.tv_sec,
					   bstatp->bs_mtime.tv_sec,
					   &reasonstr );
		if ( ok ) {
			/* create the symbolic link
			 */
			rval = unlink( path );
			if ( rval && errno != ENOENT ) {
				mlog( MLOG_VERBOSE,
				      "WARNING: unable to unlink "
				      "current file prior to creating "
				      "symlink ino %llu %s: %s: discarding\n",
				      fhdrp->fh_stat.bs_ino,
				      path,
				      strerror( errno ));
				return BOOL_TRUE;
			}
			rval = symlink( contextp->cc_symlinkbufp, path );
			if ( rval ) {
				mlog( MLOG_VERBOSE,
				      "WARNING: unable to create "
				      "symlink ino %llu %s: %s: discarding\n",
				      fhdrp->fh_stat.bs_ino,
				      path,
				      strerror( errno ));
				return BOOL_TRUE;
			}

			/* set the owner and group
			 */
			rval = lchown( path,
				       ( uid_t )fhdrp->fh_stat.bs_uid,
				       ( gid_t )fhdrp->fh_stat.bs_gid );
			if ( rval ) {
				mlog( MLOG_VERBOSE,
				      "WARNING: unable to set owner and group "
				      "of %s: %s\n",
				      path,
				      strerror( errno ));
			}

			/* NOTE: no way to set times or mode for sym links!
			 */
		} else {
			mlog( MLOG_NORMAL,
			      "NOT: restoring %s: %s\n",
			      path,
			      reasonstr );
		}
	}

	return BOOL_TRUE;
}

static intgen_t
read_filehdr( media_t *mediap, filehdr_t *fhdrp, bool_t fhcs )
{
	media_ops_t *mop = mediap->m_opsp;
	intgen_t nread;
#ifdef FILEHDR_CHECKSUM
	register u_int32_t *sump = ( u_int32_t * )fhdrp;
	register u_int32_t *endp = ( u_int32_t * )( fhdrp + 1 );
	register u_int32_t sum;
#endif /* FILEHDR_CHECKSUM */
	intgen_t rval;

	nread = read_buf( ( char * )fhdrp,
			  sizeof( *fhdrp ),
			  ( void * )mediap,
			  ( rfp_t )mop->mo_read,
			  ( rrbfp_t )mop->mo_return_read_buf,
			  &rval );
	if ( rval ) {
		return rval;
	}
	assert( ( size_t )nread == sizeof( *fhdrp ));

	mlog( MLOG_NITTY,
	      "read file hdr off %lld flags 0x%x ino %llu mode 0x%08x\n",
	      fhdrp->fh_offset,
	      fhdrp->fh_flags,
	      fhdrp->fh_stat.bs_ino,
	      fhdrp->fh_stat.bs_mode );

#ifdef FILEHDR_CHECKSUM
	if ( fhcs ) {
		if ( ! ( fhdrp->fh_flags & FILEHDR_FLAGS_CHECKSUM )) {
			mlog( MLOG_NORMAL,
			      "WARNING: corrupt file header\n" );
			return MEDIA_ERROR_CORRUPTION;
		}
		for ( sum = 0 ; sump < endp ; sum += *sump++ ) ;
		if ( sum ) {
			mlog( MLOG_NORMAL,
			      "WARNING: bad file header checksum\n" );
			return MEDIA_ERROR_CORRUPTION;
		}
	}
#endif /* FILEHDR_CHECKSUM */

	return 0;
}

static intgen_t
read_extenthdr( media_t *mediap, extenthdr_t *ehdrp, bool_t ehcs )
{
	media_ops_t *mop = mediap->m_opsp;
	intgen_t nread;
#ifdef EXTENTHDR_CHECKSUM
	register u_int32_t *sump = ( u_int32_t * )ehdrp;
	register u_int32_t *endp = ( u_int32_t * )( ehdrp + 1 );
	register u_int32_t sum;
#endif /* EXTENTHDR_CHECKSUM */
	intgen_t rval;

	nread = read_buf( ( char * )ehdrp,
			  sizeof( *ehdrp ),
			  ( void * )mediap,
			  ( rfp_t )mop->mo_read,
			  ( rrbfp_t )mop->mo_return_read_buf,
			  &rval );
	if ( rval ) {
		return rval;
	}
	assert( ( size_t )nread == sizeof( *ehdrp ));

	mlog( MLOG_NITTY,
	      "read extent hdr size %lld offset %lld type %d flags %08x\n",
	      ehdrp->eh_sz,
	      ehdrp->eh_offset,
	      ehdrp->eh_type,
	      ehdrp->eh_flags );

#ifdef EXTENTHDR_CHECKSUM
	if ( ehcs ) {
		if ( ! ( ehdrp->eh_flags & EXTENTHDR_FLAGS_CHECKSUM )) {
			mlog( MLOG_NORMAL,
			      "WARNING: corrupt extent header\n" );
			return MEDIA_ERROR_CORRUPTION;
		}
		for ( sum = 0 ; sump < endp ; sum += *sump++ ) ;
		if ( sum ) {
			mlog( MLOG_NORMAL,
			      "WARNING: bad extent header checksum\n" );
			return MEDIA_ERROR_CORRUPTION;
		}
	}
#endif /* EXTENTHDR_CHECKSUM */

	return 0;
}

static intgen_t
read_dirent( media_t *mediap,
	     direnthdr_t *dhdrp,
	     size_t direntbufsz,
	     bool_t dhcs )
{
	media_ops_t *mop = mediap->m_opsp;
	intgen_t nread;
#ifdef DIRENTHDR_CHECKSUM
	register u_int32_t *sump = ( u_int32_t * )dhdrp;
	register u_int32_t *endp = ( u_int32_t * )( dhdrp + 1 );
	register u_int32_t sum;
#endif /* DIRENTHDR_CHECKSUM */
	intgen_t rval;

	/* read the head of the dirent
	 */
	nread = read_buf( ( char * )dhdrp,
			  sizeof( direnthdr_t ),
			  ( void * )mediap,
			  ( rfp_t )mop->mo_read,
			  ( rrbfp_t )
			  mop->mo_return_read_buf,
			  &rval );
	if ( rval ) {
		return rval;
	}
	assert( ( size_t )nread == sizeof( direnthdr_t ));

#ifdef DENTGEN
	mlog( MLOG_NITTY,
	      "read dirent hdr ino %llu gen %u size %u\n",
	      dhdrp->dh_ino,
	      ( size_t )dhdrp->dh_gen,
	      ( size_t )dhdrp->dh_sz );
#else /* DENTGEN */
	mlog( MLOG_NITTY,
	      "read dirent hdr ino %llu size %u\n",
	      dhdrp->dh_ino,
	      ( size_t )dhdrp->dh_sz );
#endif /* DENTGEN */

#ifdef DIRENTHDR_CHECKSUM
	if ( dhcs ) {
		if ( dhdrp->dh_sz == 0 ) {
			mlog( MLOG_NORMAL,
			      "WARNING: corrupt directory entry header\n" );
			return MEDIA_ERROR_CORRUPTION;
		}
		for ( sum = 0 ; sump < endp ; sum += *sump++ ) ;
		if ( sum ) {
			mlog( MLOG_NORMAL,
			      "WARNING: bad directory entry header checksum\n" );
			return MEDIA_ERROR_CORRUPTION;
		}
	}
#endif /* DIRENTHDR_CHECKSUM */

	/* if null, return
	 */
	if ( dhdrp->dh_ino == 0 ) {
		assert( ( size_t )dhdrp->dh_sz == sizeof( direnthdr_t ));
		return 0;
	}

	/* read the remainder of the dirent.
	 */
	assert( ( size_t )dhdrp->dh_sz <= direntbufsz );
	assert( ( size_t )dhdrp->dh_sz >= sizeof( direnthdr_t ));
	assert( ! ( ( size_t )dhdrp->dh_sz & ( DIRENTHDR_ALIGN - 1 )));
	if ( ( size_t )dhdrp->dh_sz > sizeof( direnthdr_t )) {
		size_t remsz = ( size_t )dhdrp->dh_sz - sizeof( direnthdr_t );
		nread = read_buf( ( char * )( dhdrp + 1 ),
				  remsz,
				  ( void * )mediap,
				  ( rfp_t )mop->mo_read,
				  ( rrbfp_t )
				  mop->mo_return_read_buf,
				  &rval );
		if ( rval ) {
			return rval;
		}
		assert( ( size_t ) nread == remsz );
	}

	return 0;
}

static intgen_t
discard_padding( size_t sz, media_t *mediap )
{
	media_ops_t *mop = mediap->m_opsp;
	intgen_t nread;
	intgen_t rval;

	nread = read_buf( 0,
			  sz,
			  ( void * )mediap,
			  ( rfp_t )mop->mo_read,
			  ( rrbfp_t )mop->mo_return_read_buf,
			  &rval );
	if ( rval ) {
		return rval;
	}
	assert( ( size_t )nread == sz );

	return 0;
}

static bool_t
restore_extent( filehdr_t *fhdrp,
		extenthdr_t *ehdrp,
		int fd,
		char *path,
		media_t *mediap,
		off64_t *bytesreadp,
		intgen_t *statp )
{
	media_ops_t *mop = mediap->m_opsp;
	off64_t off = ehdrp->eh_offset;
	off64_t sz = ehdrp->eh_sz;
	off64_t new_off;

	*bytesreadp = 0;

	if ( fd != -1 ) {
		assert( path );
		/* seek to the beginning of the extent.
		 * must be on a basic fs blksz boundary.
		 */
		assert( ( off & ( off64_t )( BBSIZE - 1 )) == 0 );
		new_off = lseek64(  fd, off, SEEK_SET );
		if ( new_off < 0 ) {
			mlog( MLOG_NORMAL,
			      "attempt to seek %s to %lld failed: %s\n",
			      path,
			      off,
			      strerror( errno ));
			return BOOL_FALSE;
		}
		assert( new_off == off );
	}
		
	/* decide if direct I/O can be used.
	 */

	/* move from media to fs.
	 */
	while ( sz ) {
		char *bufp;
		size_t req_bufsz;	/* requested bufsz */
		size_t sup_bufsz;	/* supplied bufsz */
		intgen_t nwritten;
		intgen_t stat;
		size_t ntowrite;

		req_bufsz = ( size_t )min( ( off64_t )INTGENMAX, sz );
		bufp = ( * mop->mo_read )(mediap, req_bufsz, &sup_bufsz, &stat);
		if ( stat ) {
			mlog( MLOG_NORMAL,
			      "attempt to read %u bytes failed: %s\n",
			      req_bufsz,
			      stat == MEDIA_ERROR_ABORT
			      ?
			      "device error"
			      :
			      stat == MEDIA_ERROR_EOF
			      ?
			      "end of media file"
			      :
			      "end of media" );
			*statp = stat;
			return BOOL_FALSE;
		}
		if ( off >= fhdrp->fh_stat.bs_size ) {
			assert( off == fhdrp->fh_stat.bs_size );
			ntowrite = 0;
		} else if ((off64_t)sup_bufsz > fhdrp->fh_stat.bs_size - off ) {
			ntowrite = ( size_t )( fhdrp->fh_stat.bs_size - off );
		} else {
			ntowrite = sup_bufsz;
		}
		assert( ntowrite <= INTGENMAX );
		if ( ntowrite > 0 ) {
			*bytesreadp += ( off64_t )ntowrite;
			if ( fd != -1 ) {
				/* new */
				size_t tries;
				size_t remaining;
				intgen_t rval;
				off64_t tmp_off;

				rval = 0; /* for lint */
				for ( nwritten = 0,
				      tries = 0,
				      remaining = ntowrite,
				      tmp_off = off
				      ;
				      nwritten < ( intgen_t )ntowrite
				      &&
				      tries < WRITE_TRIES_MAX
				      ;
				      nwritten += rval,
				      tries++,
				      remaining -= ( size_t )rval,
				      tmp_off += ( off64_t )rval ) {
					assert( remaining <= INTGENMAX );
					rval = write( fd, bufp, remaining );
					if ( rval < 0 ) {
						nwritten = rval;
						break;
					}
					assert( ( size_t )rval <= remaining );
					if ( ( size_t )rval < remaining ) {
						mlog( MLOG_NORMAL,
						      "WARNING: attempt to "
						      "write %u bytes to %s at "
						      "offset %lld failed: "
						      "only %d bytes written\n",
						      remaining,
						      path,
						      tmp_off,
						      rval );
					}
				}
				/* old
				nwritten = write( fd, bufp, ntowrite );
				*/
			} else {
				nwritten = ( intgen_t )ntowrite;
			}
		} else {
			nwritten = 0;
		}
		( * mop->mo_return_read_buf )( mediap, bufp, sup_bufsz );
		if ( nwritten < 0 ) {
			mlog( MLOG_NORMAL,
			      "attempt to write %u bytes to %s "
			      "at offset %lld failed: %s\n",
			      ntowrite,
			      path,
			      off,
			      strerror( errno ));
			return BOOL_FALSE;
		}
		if ( ( size_t )nwritten != ntowrite ) {
			assert( ( size_t )nwritten < ntowrite );
			mlog( MLOG_NORMAL,
			      "attempt to write %u bytes to %s at "
			      "offset %lld failed: only %d bytes written\n",
			      ntowrite,
			      path,
			      off,
			      nwritten );
			return BOOL_FALSE;
		}
		sz -= ( off64_t )sup_bufsz;
		off += ( off64_t )nwritten;
	}

	return BOOL_TRUE;
}

static char *
ehdr_typestr( int32_t type )
{
	switch ( type ) {
	case EXTENTHDR_TYPE_LAST:
		return "LAST";
	case EXTENTHDR_TYPE_ALIGN:
		return "ALIGN";
	case EXTENTHDR_TYPE_DATA:
		return "DATA";
	default:
		return "?";
	}
}

static void
hkd_abort_cleanup( void *arg1, void *arg2 )
{
	scontext_t *scontextp = ( scontext_t * )arg1;
	state_t *statep = scontextp->sc_statep;
	char *statepath = scontextp->sc_statepath;
	char *hkdir = scontextp->sc_hkdir;
	size_t streamix;
	size_t streamcnt;
	intgen_t rval;

	/* see if we were interrupted. if not, can remove
	 */
	if ( statep ) {
		for ( streamcnt = statep->s_streamcnt, streamix = 0
		      ;
		      streamix < streamcnt
		      ;
		      streamix++ ) {
			if ( statep->s_stream[ streamix ].ss_interrupted ) {
				return;
			}
		}
	}
	
	if ( statepath ) {
		rval = unlink( statepath );
		if ( rval && errno != ENOENT ) {
			mlog( MLOG_NORMAL,
			      " unable to remove state file %s: %s\n",
			      statepath,
			      strerror( errno ));
		}
	}
	if ( hkdir ) {
		rval = rmdir( hkdir );
		if ( rval && errno != ENOENT ) {
			mlog( MLOG_NORMAL,
			      " unable to remove directory %s: %s\n",
			      hkdir,
			      strerror( errno ));
		}
	}
}
