#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/dump/RCS/content_inode.c,v 1.62 1995/08/22 14:51:11 tap Exp $"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/uuid.h>
#include <time.h>
#include <sys/fs/xfs_itable.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/dirent.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "types.h"
#include "path.h"
#include "jdm.h"
#include "util.h"
#include "mlog.h"
#include "getopt.h"
#include "stream.h"
#include "cldmgr.h"
#include "global.h"
#include "drive.h"
#include "media.h"
#include "content.h"
#include "content_inode.h"
#include "inomap.h"
#include "var.h"
#include "inventory.h"
#include "cleanup.h"

#define LEVEL_DEFAULT	0
#define LEVEL_MIN	0
#define LEVEL_MAX	9

#define SUBTREE_MAX	100
#define PGALIGNTHRESH	8


/* content_inode.c - inode-style content strategy
 */


/* structure definitions used locally ****************************************/

/* strategy context
 */
struct scontext {
	inv_idbtoken_t sc_inv_idbtoken;
	inv_sestoken_t sc_inv_sestoken;
	inv_stmtoken_t *sc_inv_stmtokenp; /* array of stream tokens */
	bool_t *sc_inv_interruptedp; /* per-stream "interrupted" flags */
	bool_t sc_last;
	time_t sc_lasttime;
	bool_t sc_resume;
	time_t sc_resumetime;
	size_t sc_resumerangecnt;
	drange_t *sc_resumerangep;
	bool_t sc_inv_update;
	intgen_t sc_level;
	char ** sc_subtreep;
	intgen_t sc_subtreecnt;
	jdm_fshandle_t *sc_fshandlep;
	int sc_fsfd;
	xfs_bstat_t *sc_rootxfsstatp;
};

typedef struct scontext scontext_t;


/* context structure to hang from the generic content_t's contextp
 */
typedef enum { STATE_NONE, STATE_DIR, STATE_NONDIR } statstate_t;

struct context {
	jdm_fshandle_t *cc_fshandlep;	/* needed by jdm_open() */
	intgen_t cc_fsfd;		/* needed by bigstat_... */
	filehdr_t *cc_filehdrp;		/* heads each dumped file */
	extenthdr_t *cc_extenthdrp;	/* heads each file extent */
	void *cc_inomap_state_contextp;	/* to speed inomap iteration */
	char *cc_getdents64bufp;	/* buffer for getdents64() syscall */
	size_t cc_getdents64bufsz;
	char *cc_mdirentbufp;		/* buffer for on-media dirent */
	size_t cc_mdirentbufsz;
	char *cc_readlinkbufp;		/* buffers for readlink() */
	size_t cc_readlinkbufsz;
	off64_t cc_mfilesz;		/* total bytes dumped to media file */
	size_t cc_markscommitted;	/* number of marks committed in mfile */
	scontext_t *cc_scontextp;	/* pointer to strategy-specific cntxt */
	statstate_t cc_statstate;
	u_int64_t cc_statdirdone;
	u_int64_t cc_statnondirdone;
	ino64_t cc_statlast;
	u_int64_t cc_statdircnt;
	u_int64_t cc_statnondircnt;
	u_int64_t cc_datasz;
	u_int64_t cc_datadumped;
};

typedef struct context context_t;

/* extent group context, used by dump_file()
 */
#define BMAP_LEN	20	/* make much larger; set small for testing */

struct extent_group_context {
	getbmap_t eg_bmap[ BMAP_LEN ];
	getbmap_t *eg_nextbmapp;	/* ptr to the next extent to dump */
	getbmap_t *eg_endbmapp;		/* to detect extent exhaustion */
	intgen_t eg_fd;			/* file desc. */
	intgen_t eg_bmapix;		/* debug info only, not used */
	intgen_t eg_gbmcnt;		/* debug, counts getbmap calls for ino */
};

typedef struct extent_group_context extent_group_context_t;


/* minimum getdents( ) buffer size
 */
#define GETDENTSBUF_SZ_MIN	( 2 * PGSZ )


/* declarations of externally defined global symbols *************************/

extern void usage( void );
extern char *homedir;
extern bool_t miniroot;


/* forward declarations of locally defined static functions ******************/

/* strategy functions
 */
static intgen_t s_match( int, char *[], media_strategy_t * );
static bool_t s_create( content_strategy_t *, int, char *[] );
static bool_t s_init( content_strategy_t *, int, char *[] );
static size_t s_stat( content_strategy_t *, char [][ CONTENT_STATSZ ], size_t );
static void s_complete( content_strategy_t * );

/* manager operators
 */
static bool_t create( content_t * );
static bool_t init( content_t * );
static intgen_t begin_stream( content_t *, size_t );

/* file dumpers
 */
static intgen_t dump_dir( void *,
			  jdm_fshandle_t *,
			  intgen_t,
			  xfs_bstat_t * );
static intgen_t dump_file( void *,
			   jdm_fshandle_t *,
			   intgen_t,
			   xfs_bstat_t * );
static intgen_t dump_file_reg( content_t *, jdm_fshandle_t *, xfs_bstat_t * );
static intgen_t dump_file_spec( content_t *, jdm_fshandle_t *, xfs_bstat_t * );
static intgen_t dump_filehdr( content_t *contentp,
			      xfs_bstat_t *,
			      off64_t,
			      intgen_t );
static intgen_t dump_extenthdr( content_t *,
				int32_t,
				int32_t,
				off64_t,
				off64_t );
static intgen_t dump_dirent( content_t *,
			     xfs_bstat_t *,
			     ino64_t,
			     u_int32_t,
			     char *,
			     size_t );
static intgen_t init_extent_group_context( jdm_fshandle_t *,
					   xfs_bstat_t *,
					   extent_group_context_t * );
static void cleanup_extent_group_context( extent_group_context_t * );
static intgen_t dump_extent_group( content_t *,
				   xfs_bstat_t *,
				   extent_group_context_t *,
				   off64_t,
				   off64_t,
				   bool_t,
				   off64_t *,
				   off64_t *,
				   bool_t * );
static void dump_session_inv( content_t * );
static intgen_t write_pad( content_t *, size_t );

static void mark_callback( void *, media_mark_t *, bool_t );

static void inv_cleanup( void *, void * );


/* definition of locally defined global variables ****************************/

/* strategy-specific context
 */
scontext_t scontext_inode = {
	INV_TOKEN_NULL,	/* sc_inv_idbtoken */
	INV_TOKEN_NULL,	/* sc_inv_sestoken */
	0,		/* sc_inv_stmtokenp */
	0,		/* sc_inv_interruptedp */
	BOOL_FALSE,	/* sc_last */
	0,		/* sc_lasttime */
	BOOL_FALSE,	/* sc_resume */
	0,		/* sc_resumetime */
	0,		/* sc_resumerangecnt */
	0,		/* sc_resumerangep */
	BOOL_FALSE,	/* sc_inv_update */
	0,		/* sc_level */
	0,		/* sc_subtreep */
	0,		/* sc_subtreecnt */
	0,		/* sc_fshandlep */
	-1,		/* sc_fsfd */
	0,		/* sc_rootxfsstatp */
};

/* content strategy. referenced by content.c
 */
content_strategy_t content_strategy_inode = {
	-1,		/* cs_id */
	0,		/* cs_flags */
	s_match,	/* cs_match */
	s_create,	/* cs_create */
	s_init,		/* cs_init */
	0,		/* cs_stat */
	s_complete,	/* cs_complete */
	0,		/* cs_contentp */
	0,		/* cs_contentcnt */
	( void * )&scontext_inode,/* cs_contextp */
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
 */
/*ARGSUSED*/
static intgen_t
s_match( int argc, char *argv[], media_strategy_t *msp )
{
	content_strategy_t *csp = &content_strategy_inode;
	content_t *contentp = csp->cs_contentp[ 0 ];
	content_hdr_t *cwhdrp = contentp->c_writehdrp;

	/* sanity check on initialization
	 */
	assert( csp->cs_id >= 0 );

	if ( ! strcmp( cwhdrp->ch_fstype, "xfs" )) {
		return 1;
	} else {
		return 0;
	}
}

/* strategy init - initializes the strategy, and each manager
 */
/*ARGSUSED*/
static bool_t
s_create( content_strategy_t *csp, int argc, char *argv[] )
{
	jdm_fshandle_t *fshandlep;
	int fsfd;
	intgen_t level;
	bool_t level_found;
	bool_t last;
	time_t lasttime;
	uuid_t lastid;
	intgen_t lastlevel;
	bool_t resume_req;
	bool_t resume;
	time_t resumetime;
	uuid_t resumeid;
	drange_t *resumerangep;
	size_t resumerangecnt;
	intgen_t subtreeix;
	intgen_t subtreecnt;
	char **subtreep;
	int c;
	scontext_t *scontextp = ( scontext_t * )csp->cs_contextp;
	content_t *contentp = csp->cs_contentp[ 0 ];
	content_hdr_t *cwhdrp = contentp->c_writehdrp;
	global_hdr_t *gwhdrp = contentp->c_gwritehdrp;
	stat64_t rootstat;
	xfs_bstat_t *rootxfsstatp;
	u_int32_t uuidstat;
	u_char_t *string_uuid;
	bool_t inv_update;
	inv_idbtoken_t inv_idbt;
	inv_session_t *inv_sessionp;
	bool_t interrupted;
	bool_t partial;
	bool_t equalfound;
	size_t streamix;
	size_t streamcnt = csp->cs_contentcnt;
	int rval;
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
	assert( DIRENTHDR_SZ % DIRENTHDR_ALIGN == 0 );

	/* create a subtree array, to be populated from command line
	 * subtree option arguments from the command line. only needed during
	 * inomap calculation; will be freed at the end of this function.
	 */
	subtreep = ( char ** )calloc( SUBTREE_MAX, sizeof( char * ));
	assert( subtreep );

	/* scan cmdline for dump level
	 */
	level = LEVEL_DEFAULT;
	level_found = BOOL_FALSE;
	inv_update = BOOL_TRUE;
	subtreeix = 0;
	resume_req = BOOL_FALSE;
	optind = 1;
	opterr = 0;
	while ( ( c = getopt( argc, argv, GETOPT_CMDSTRING )) != EOF ) {
		switch ( c ) {
		case GETOPT_LEVEL:
			if ( level_found ) {
				mlog( MLOG_NORMAL,
				      "too many -%c arguments: "
				      "\"-%c %d\" already given\n",
				      optopt,
				      optopt,
				      level );
				usage( );
				return BOOL_FALSE;
			}
			level_found = BOOL_TRUE;
			if ( ! optarg || optarg[ 0 ] == '-' ) {
				mlog( MLOG_NORMAL,
				      "-%c argument missing\n",
				      optopt );
				usage( );
				return BOOL_FALSE;
			}
			level = atoi( optarg );
			if ( level < LEVEL_MIN || level > LEVEL_MAX ) {
				mlog( MLOG_NORMAL,
				      "-%c argument must be "
				      "between %d and %d\n",
				      optopt,
				      LEVEL_MIN,
				      LEVEL_MAX );
				usage( );
				return BOOL_FALSE;
			}
			break;
		case GETOPT_SUBTREE:
			if ( subtreeix >= SUBTREE_MAX ) {
				mlog( MLOG_NORMAL,
				      "too many -%c arguments: "
				      "maximum is %d\n",
				      optopt,
				      SUBTREE_MAX );
				usage( );
				return BOOL_FALSE;
			}
			if ( ! optarg || optarg[ 0 ] == '-' ) {
				mlog( MLOG_NORMAL,
				      "-%c argument missing\n",
				      optopt );
				usage( );
				return BOOL_FALSE;
			}
			if ( optarg[ 0 ] == '/' ) {
				mlog( MLOG_NORMAL,
				      "-%c argument (subtree) "
				      "must be a relative pathname\n",
				      optopt );
				usage( );
				return BOOL_FALSE;
			}
			subtreep[ subtreeix ] = optarg;
			subtreeix++;
			break;
		case GETOPT_RESUME:
			resume_req = BOOL_TRUE;
			break;
		case GETOPT_NOINVUPDATE:
			inv_update = BOOL_FALSE;
			break;
		}
	}

	/* miniroot precludes inventory update
	 */
	if ( miniroot ) {
		inv_update = BOOL_FALSE;
	}

	/* now the subtree count is known
	 */
	subtreecnt = subtreeix;

	/* place the inv_update decision in the strategy context,
	 * to be used by s_init
	  */
	scontextp->sc_inv_update = inv_update;
	scontextp->sc_level = level;
	scontextp->sc_subtreep = subtreep;
	scontextp->sc_subtreecnt = subtreecnt;

	/* point out that deltas (resume or incremental) don't make
	 * sense if the inventory is not to be used
	 */
	if ( ! inv_update ) {
		if ( resume_req ) {
			mlog( MLOG_NORMAL,
			      "use of the -%c option "
			      "(inhibit inventory update) "
			      "precludes the use of the -%c option "
			      "(resume previous dump)\n",
			      GETOPT_NOINVUPDATE,
			      GETOPT_RESUME );
			return BOOL_FALSE;
		}
		if ( level > 0 ) {
			mlog( MLOG_NORMAL,
			      "use of the -%c option "
			      "(inhibit inventory update) "
			      "precludes the use of the -%c option "
			      "(incremental dump)\n",
			      GETOPT_NOINVUPDATE,
			      GETOPT_LEVEL );
			return BOOL_FALSE;
		}
	}

	/* create my /var directory if it doesn't already exist
	 */
	if ( ! miniroot ) {
		var_create( );
	}

	/* open the dump inventory. first create a cleanup handler,
	 * to close the inventory on exit.
	 */
	if ( inv_update ) {
		( void )cleanup_register( inv_cleanup, ( void * )csp, 0 );
		inv_idbt = inv_open( ( inv_predicate_t )INV_BY_UUID,
				     INV_SEARCH_N_MOD,
				     ( void * )&cwhdrp->ch_fsid );
		if ( inv_idbt == INV_TOKEN_NULL ) {
			return BOOL_FALSE;
		}
	} else {
		inv_idbt = INV_TOKEN_NULL;
	}
	scontextp->sc_inv_idbtoken = inv_idbt;

	/* if inventory update requested, get two session descriptors from
	 * the inventory: one for the last dump at this level, and one for
	 * the last dump at a lower level. the former will be used to check
	 * if the last dump at this level was prematurely terminated; if so,
	 * for those inos already dumped then, we will look only for changes
	 * since that dump.  * the latter will give us a change date for all
	 * other inos.
	 */

	/* look for the most recent dump at a level less than
	 * the level of this dump, which was not partial or
	 * interrupted.
	 */
	inv_sessionp = 0;
	if ( inv_update && level > LEVEL_MIN ) {
		ok = inv_lastsession_level_lessthan( inv_idbt,
						     ( u_char_t )level,
						     &inv_sessionp );
		if ( ! ok ) {
			return BOOL_FALSE;
		}

		if ( inv_sessionp ) {
			size_t strix;
			size_t strcnt;
			inv_stream_t *bsp;

			lasttime = inv_sessionp->s_time;
			lastid = inv_sessionp->s_sesid;
			lastlevel = ( intgen_t )inv_sessionp->s_level;
			last = BOOL_TRUE;
			partial = inv_sessionp->s_ispartial;
			interrupted = BOOL_FALSE;
			strcnt =  ( size_t )inv_sessionp->s_nstreams;
			for ( strix = 0 ; strix < strcnt ; strix++ ) {
				bsp = &inv_sessionp->s_streams[ strix ];
				if ( bsp->st_interrupted ) {
					interrupted = BOOL_TRUE;
					break;
				}
			}
			free( ( void * )inv_sessionp->s_streams );
			free( ( void * )inv_sessionp );
			inv_sessionp = 0;
			if ( partial ) {
				mlog( MLOG_NORMAL,
				      "most recent base dump"
				      " (level %d begun %s)"
				      " was partial"
				      ": aborting"
				      "\n",
				      lastlevel,
				      ctimennl( &lasttime ));
				return BOOL_FALSE;
			}
			if ( interrupted ) {
				mlog( MLOG_NORMAL,
				      "most recent base dump"
				      " (level %d begun %s)"
				      " was interrupted"
				      ": aborting"
				      "\n",
				      lastlevel,
				      ctimennl( &lasttime ));
				return BOOL_FALSE;
			}
		} else {
			mlog( MLOG_NORMAL,
			      "cannot find earlier dump "
			      "to base level %d increment upon\n",
			      level );
			return BOOL_FALSE;
		}
	} else {
		lasttime = 0;
		uuid_create_nil( &lastid, &uuidstat );
		lastlevel = 0;
		last = BOOL_FALSE;
	}
	scontextp->sc_last = last;
	scontextp->sc_lasttime = lasttime;

	/* get info about most recent dump at the level of this dump.
	 * if it occurred before the most recent dump at a lesser level,
	 * moot. if however it occurred after, and was interrupted, and
	 * the operator has specified the resume (-R) option, extract
	 * info to allow us avoid re-dumping files dumped in the interrupted
	 * dump unless they have changed since then.
	 */
	assert( inv_sessionp == 0 );
	if ( inv_update ) {
		ok = inv_lastsession_level_equalto( inv_idbt,
						    ( u_char_t )level,
						    &inv_sessionp );
		if ( ! ok ) {
			return BOOL_FALSE;
		}
	}

	interrupted = BOOL_FALSE;
	partial = BOOL_FALSE;
	equalfound = BOOL_FALSE;
	if ( inv_update && inv_sessionp && inv_sessionp->s_time > lasttime ) {
		size_t streamix;

		partial = inv_sessionp->s_ispartial;
		resumetime = inv_sessionp->s_time;
		equalfound = BOOL_TRUE;
		resumeid =  inv_sessionp->s_sesid;
		resumerangecnt =  ( size_t )inv_sessionp->s_nstreams;
		resumerangep = ( drange_t * )calloc( resumerangecnt,
						      sizeof( drange_t ));
		assert( resumerangep );
		for ( streamix = 0 ; streamix < resumerangecnt ; streamix++ ) {
			inv_stream_t *bsp;
			inv_stream_t *esp;
			drange_t *p = &resumerangep[ streamix ];
			bsp = &inv_sessionp->s_streams[ streamix ];
			esp = ( streamix < resumerangecnt - 1 )
			      ?
			      bsp + 1
			      :
			      0;
			if ( bsp->st_interrupted ) {
				interrupted = BOOL_TRUE;
				p->dr_begin.sp_ino = bsp->st_endino;
				p->dr_begin.sp_offset = bsp->st_endino_off;
				mlog( MLOG_DEBUG,
				      "resume range ino %llu:%lld to ",
				      p->dr_begin.sp_ino,
				      p->dr_begin.sp_offset );
				if ( esp ) {
					p->dr_end.sp_ino = esp->st_startino;
					p->dr_end.sp_offset =
							esp->st_startino_off;
					mlog( MLOG_DEBUG | MLOG_CONTINUE,
					      "%llu:%lld\n",
					      p->dr_end.sp_ino,
					      p->dr_end.sp_offset );
				} else {
					p->dr_end.sp_flags = STARTPT_FLAGS_END;	
					mlog( MLOG_DEBUG | MLOG_CONTINUE,
					      "end\n" );
				}
			} else {
				/* set the range start pt's END flag to
				 * indicate the range was not interrupted.
				 */
				p->dr_begin.sp_flags = STARTPT_FLAGS_END;
			}
		}
	} else {
		resumetime = 0;
		uuid_create_nil( &resumeid, &uuidstat );
		resumerangecnt = 0;
		resumerangep = 0;
	}

	if ( inv_sessionp ) {
		free( ( void * )inv_sessionp->s_streams );
		free( ( void * )inv_sessionp );
		inv_sessionp = 0;
	}


	/* reconcile the resume state
	 */
	if ( interrupted ) {
		if ( resume_req ) {
			resume = BOOL_TRUE;
		} else {
			mlog( MLOG_NORMAL,
			      "WARNING: most recent level %d dump "
			      "was interrupted, "
			      "but not resuming that dump since "
			      "resume (-R) option not specified\n",
			      level );
			resume = BOOL_FALSE;
		}
	} else {
		resume = BOOL_FALSE;
		if ( resumerangep ) {
			free( ( void * )resumerangep );
			resumerangep = 0;
		}
		if ( resume_req ) {
			if ( equalfound ) {
				mlog( MLOG_NORMAL,
				      "resume (-R) option inappropriate: "
				      "most recent level %d dump (%s) was "
				      "not interrupted\n",
				      level,
				      ctimennl( &lasttime ));
			} else {
				mlog( MLOG_NORMAL,
				      "resume (-R) option inappropriate: "
				      "no previous level %d dump found\n" );
			}
			return BOOL_FALSE;
		}
	}

	/* place the resume range info in the strategy context;
	 * will be used by dump_file.
	 */
	scontextp->sc_resume = resume;
	scontextp->sc_resumetime = resumetime;
	scontextp->sc_resumerangecnt = resumerangecnt;
	scontextp->sc_resumerangep = resumerangep;

	/* announce the dump characteristics
	 */
	if ( ! resume ) {
		if ( ! last ) {
			assert( level == LEVEL_MIN );
			mlog( MLOG_VERBOSE,
			      "level %d dump of %s:%s\n",
			      level,
			      gwhdrp->gh_hostname,
			      cwhdrp->ch_mntpnt );
		} else {
			mlog( MLOG_VERBOSE,
			      "level %d incremental dump of %s:%s"
			      " based on level %d dump begun %s\n",
			      level,
			      gwhdrp->gh_hostname,
			      cwhdrp->ch_mntpnt,
			      lastlevel,
			      ctimennl( &lasttime ));
		}
	} else {
		if ( ! last ) {
			mlog( MLOG_VERBOSE,
			      "resuming level %d dump of %s:%s begun %s\n",
			      level,
			      gwhdrp->gh_hostname,
			      cwhdrp->ch_mntpnt,
			      ctimennl( &resumetime ));
		} else {
			mlog( MLOG_VERBOSE,
			      "resuming level %d incremental dump of %s:%s"
			      " based on level %d dump begun %s\n",
			      level,
			      gwhdrp->gh_hostname,
			      cwhdrp->ch_mntpnt,
			      lastlevel,
			      ctimennl( &lasttime ));
		}
	}

	/* announce the dump time
	 */
	mlog( MLOG_VERBOSE,
	      "dump date: %s\n",
	      ctimennl( &gwhdrp->gh_timestamp ));

	/* display the session UUID
	 */
	uuid_to_string( &gwhdrp->gh_dumpid, (char **)&string_uuid, &uuidstat );
	assert( uuidstat == uuid_s_ok );
	mlog( MLOG_VERBOSE,
	      "session id: %s\n",
	      string_uuid );
	free( ( void * )string_uuid );

	/* display the session label
	 */
	mlog( MLOG_VERBOSE,
	      "session label: \"%s\"\n",
	      gwhdrp->gh_dumplabel );

	/* get a file descriptor for the file system. any file
	 * contained in the file system will do; use the mntpnt.
	 * needed by bigstat.
	 */
	fsfd = open( cwhdrp->ch_mntpnt, O_RDONLY );
	if ( fsfd < 0 ) {
		mlog( MLOG_NORMAL,
		      "unable to open %s: %s\n",
		      cwhdrp->ch_mntpnt,
		      strerror( errno ));
		return BOOL_FALSE;
	}
	scontextp->sc_fsfd = fsfd;

	/* figure out the ino for the root directory of the fs
	 */
	rval = fstat64( fsfd, &rootstat );
	if ( rval ) {
		mlog( MLOG_NORMAL,
		      "could not stat %s\n",
		      cwhdrp->ch_mntpnt );
		return BOOL_FALSE;
	}

	/* translate the stat64_t into a xfs_bstat64_t,
	 * since that is what is expected by inomap_build()
	 */
	rootxfsstatp = ( xfs_bstat_t * )calloc( 1, sizeof( xfs_bstat_t ));
	assert( rootxfsstatp );
	stat64_to_xfsbstat( rootxfsstatp, &rootstat );
	scontextp->sc_rootxfsstatp = rootxfsstatp;
	
	/* alloc a file system handle, to be used with the jdm_open()
	 * functions.
	 */
	fshandlep = jdm_getfshandle( cwhdrp->ch_mntpnt );
	if ( ! fshandlep ) {
		mlog( MLOG_NORMAL,
		      "unable to construct a file system handle for %s: %s\n",
		      cwhdrp->ch_mntpnt,
		      strerror( errno ));
		return BOOL_FALSE;
	}
	scontextp->sc_fshandlep = fshandlep;

	/* ask var to ask inomap to skip files under var if var is in
	 * the fs being dumped
	 */
	if ( ! miniroot ) {
		var_skip( &cwhdrp->ch_fsid, inomap_skip );
	}

	/* give each content manager its operators, patch in content-specific
	 * header info, including startpoints, end startpoints, allocate a
	 * context_t, inomap_state_context_t, filehdr_t, extenthdr_t, and
	 * alternate dirent buffer for each, and call each manager's init
	 * operator.
	 */
	for ( streamix = 0 ; streamix < streamcnt ; streamix++ ) {
		content_t *contentp;
		content_hdr_t *cwhdrp;	/* hides instance in outer scope */
		content_inode_hdr_t *scwhdrp;
		context_t *contextp;

		contentp = csp->cs_contentp[ streamix ];
		contentp->c_opsp = &content_ops;

		cwhdrp = contentp->c_writehdrp;
		scwhdrp = ( content_inode_hdr_t * )
			  ( void * )
			  cwhdrp->ch_specific;

		/* populate the strategy-specific content header
		 */
		/*LINTED*/
		assert( sizeof( cwhdrp->ch_specific ) >= sizeof( *scwhdrp ));
		scwhdrp->cih_dumpattr = 0;
		scwhdrp->cih_mediafiletype = CIH_MEDIAFILETYPE_DATA;
		scwhdrp->cih_level = level;
		if ( last ) {
			scwhdrp->cih_dumpattr |= CIH_DUMPATTR_INCREMENTAL;
		}
		scwhdrp->cih_last_time = lasttime;
		scwhdrp->cih_last_id = lastid;
		if ( resume ) {
			scwhdrp->cih_dumpattr |= CIH_DUMPATTR_RESUME;
		}
		scwhdrp->cih_resume_time = resumetime;
		scwhdrp->cih_resume_id = resumeid;
		scwhdrp->cih_rootino = rootxfsstatp->bs_ino;

		/* populate the strategy-specific stream context
		 */
		contextp = ( context_t * )calloc( 1, sizeof( context_t ));
		assert( contextp );
		contentp->c_contextp = ( void * )contextp;
		contextp->cc_fshandlep = fshandlep;
		contextp->cc_fsfd = fsfd;
		contextp->cc_filehdrp =
				( filehdr_t * )calloc( 1, sizeof( filehdr_t ));
		assert( contextp->cc_filehdrp );
		contextp->cc_extenthdrp =
			    ( extenthdr_t * )calloc( 1, sizeof( extenthdr_t ));
		assert( contextp->cc_extenthdrp );
		contextp->cc_scontextp = scontextp;

		/* allocate a buffer for the getdents64 sys call.
		 */
		contextp->cc_getdents64bufsz = sizeof( dirent64_t )
					       +
					       MAXNAMELEN;
		if ( contextp->cc_getdents64bufsz < GETDENTSBUF_SZ_MIN ) {
			contextp->cc_getdents64bufsz = GETDENTSBUF_SZ_MIN;
		}

		contextp->cc_getdents64bufp =
			   ( char * ) calloc( 1, contextp->cc_getdents64bufsz );
		assert( contextp->cc_getdents64bufp );

		/* allocate a buffer for translating the dirent64_t above
		 * into an on-media directory entry.
		 */
		contextp->cc_mdirentbufsz = sizeof( direnthdr_t  )
					    +
					    MAXNAMELEN
					    +
					    DIRENTHDR_ALIGN;
		contextp->cc_mdirentbufp =
			   ( char * ) calloc( 1, contextp->cc_mdirentbufsz );
		assert( contextp->cc_mdirentbufp );

		/* allocate a buffer for the readlink() sys call.
		 */
		contextp->cc_readlinkbufsz = MAXPATHLEN + SYMLINK_ALIGN;
		contextp->cc_readlinkbufp =
			   ( char * ) calloc( 1, contextp->cc_readlinkbufsz );
		assert( contextp->cc_readlinkbufp );

		/* init per-stream stat stat display state
		 */
		contextp->cc_statstate = ( statstate_t )STATE_NONE;

		/* call the content manager's create and init operators.
		 */
		ok = ( * contentp->c_opsp->co_create )( contentp );
		if ( ! ok ) {
			return BOOL_FALSE;
		}
	}

	/* allow status display, now that per-stream stat state ready
	 */
	csp->cs_stat = s_stat;

	return BOOL_TRUE;
}

static bool_t
s_init( content_strategy_t *csp, int argc, char *argv[] )
{
	scontext_t *scontextp = ( scontext_t * )csp->cs_contextp;
	content_t *contentp = csp->cs_contentp[ 0 ];
	global_hdr_t *gwhdrp = contentp->c_gwritehdrp;
	content_hdr_t *cwhdrp = contentp->c_writehdrp;
	intgen_t fsfd = scontextp->sc_fsfd;
	jdm_fshandle_t *fshandlep = scontextp->sc_fshandlep;
	bool_t inv_update = scontextp->sc_inv_update;
	bool_t last = scontextp->sc_last;
	time_t lasttime = scontextp->sc_lasttime;
	bool_t resume = scontextp->sc_resume;
	time_t resumetime = scontextp->sc_resumetime;
	size_t resumerangecnt = scontextp->sc_resumerangecnt;
	drange_t *resumerangep = scontextp->sc_resumerangep;
	bool_t level = scontextp->sc_level;
	char **subtreep = scontextp->sc_subtreep;
	bool_t subtreecnt = scontextp->sc_subtreecnt;
	inv_idbtoken_t inv_idbt = scontextp->sc_inv_idbtoken;
	char *qmntpnt;
	char *qfsdevice;
	inv_sestoken_t inv_st;
	size_t streamcnt = csp->cs_contentcnt;
	size_t streamix;
	xfs_bstat_t *rootxfsstatp = scontextp->sc_rootxfsstatp;
	startpt_t *startptp;
	bool_t ok;

	/* allocate storage for the stream startpoints, and build inomap.
	 * inomap_build() also fills in the start points. storage only needed
	 * until the startpoints are copied into each streams header. will
	 * be freed at the end of this function.
	 */
	startptp = ( startpt_t * )calloc( csp->cs_contentcnt,
					  sizeof( startpt_t ));
	assert( startptp );
	ok = inomap_build( fshandlep,
			   fsfd,
			   rootxfsstatp,
			   last,
			   lasttime,
			   resume,
			   resumetime,
			   resumerangecnt,
			   resumerangep,
			   subtreep,
			   subtreecnt,
			   startptp,
			   streamcnt );
	if ( ! ok ) {
		return BOOL_FALSE;
	}
	
	for ( streamix = 0 ; streamix < streamcnt ; streamix++ ) {
		content_t *contentp = csp->cs_contentp[ streamix ];
		context_t *contextp = ( context_t * )contentp->c_contextp;
		content_hdr_t *cwhdrp;	/* hides instance in outer scope */
		content_inode_hdr_t *scwhdrp;

		cwhdrp = contentp->c_writehdrp;
		scwhdrp = ( content_inode_hdr_t * )
			  ( void * )
			  cwhdrp->ch_specific;
		scwhdrp->cih_startpt = startptp[ streamix ];
		if ( streamix < streamcnt - 1 ) {
			scwhdrp->cih_endpt = startptp[ streamix + 1 ];
		} else {
			scwhdrp->cih_endpt.sp_flags = STARTPT_FLAGS_END;
		}
		contextp->cc_inomap_state_contextp = inomap_state_getcontext( );
		inomap_writehdr( scwhdrp );
	}

	free( ( void * )startptp );
	free( ( void * )subtreep );
	free( ( void * )rootxfsstatp );

	/* if inventory update requested, open an inventory session.
	 */
	if ( inv_update ) {

		/* open an inventory write session. the mount point and
		 * device path arguments must actually be qualified with
		 * the hostname.
		 */
		qmntpnt = ( char * )calloc( 1, strlen( gwhdrp->gh_hostname )
					       +
					       1
					       +
					       strlen( cwhdrp->ch_mntpnt )
					       +
					       1 );
		assert( qmntpnt );
		( void )sprintf( qmntpnt,
				 "%s:%s",
				 gwhdrp->gh_hostname,
				 cwhdrp->ch_mntpnt );
		qfsdevice = ( char * )calloc( 1, strlen( gwhdrp->gh_hostname )
					       +
					       1
					       +
					       strlen( cwhdrp->ch_fsdevice )
					       +
					       1 );
		assert( qfsdevice );
		( void )sprintf( qfsdevice,
				 "%s:%s",
				 gwhdrp->gh_hostname,
				 cwhdrp->ch_fsdevice );

		/* hold tty signals while creating a new inventory session
		 */
		( void )sighold( SIGINT );
		( void )sighold( SIGQUIT );
		( void )sighold( SIGHUP );

		inv_st = inv_writesession_open( inv_idbt,
						&cwhdrp->ch_fsid,
						&gwhdrp->gh_dumpid,
						gwhdrp->gh_dumplabel,
						subtreecnt ? BOOL_TRUE
							   : BOOL_FALSE,
						resume,
						( u_char_t )level,
						streamcnt,
						gwhdrp->gh_timestamp,
						qmntpnt,
						qfsdevice );
		scontextp->sc_inv_sestoken = inv_st;
		if( inv_st == INV_TOKEN_NULL ) {
			ok = BOOL_FALSE;
		} else {

			/* mark each stream as interrupted - will be changed
			 * when the stream is completely dumped
			 */
			scontextp->sc_inv_interruptedp =
			  ( bool_t * )calloc( streamcnt, sizeof( bool_t ));
			assert( scontextp->sc_inv_interruptedp );

			/* open an inventory stream for each stream
			 */
			scontextp->sc_inv_stmtokenp =
			  ( inv_stmtoken_t * )calloc( streamcnt,
						      sizeof( inv_stmtoken_t ));
			assert( scontextp->sc_inv_stmtokenp );

			ok = BOOL_TRUE;
			for ( streamix = 0 ; streamix < streamcnt ; streamix++ ) {
				scontext_t *scontextp = ( scontext_t * )csp->cs_contextp;
				content_t *contentp = csp->cs_contentp[ 0 ];
				media_t *mediap = contentp->c_mediap;
				drive_t *drivep = mediap->m_drivep;
				inv_stmtoken_t *inv_stmtp;
				char *drvpath;

				scontextp->sc_inv_interruptedp[ streamix ] =
								    BOOL_TRUE;
				if ( strcmp( drivep->d_pathname, "stdio" )) {
					drvpath = path_reltoabs( drivep->d_pathname,
								  homedir );
				} else {
					drvpath = drivep->d_pathname;
				}
				inv_stmtp = scontextp->sc_inv_stmtokenp
					    +
					    streamix;
				*inv_stmtp = inv_stream_open( inv_st,
							      drvpath );
				if ( strcmp( drivep->d_pathname, "stdio" )) {
					free( ( void * )drvpath );
				}
				if ( *inv_stmtp == INV_TOKEN_NULL ) {
					ok = BOOL_FALSE;
					break;
				}
			}
		}

		( void )sigrelse( SIGINT );
		( void )sigrelse( SIGQUIT );
		( void )sigrelse( SIGHUP );
	} else {
		scontextp->sc_inv_sestoken = INV_TOKEN_NULL;
		scontextp->sc_inv_stmtokenp = 0;
		ok = BOOL_TRUE;
	}

	if ( ! ok ) {
		return BOOL_FALSE;
	}

	for ( streamix = 0 ; streamix < streamcnt ; streamix++ ) {
		content_t *contentp;

		contentp = csp->cs_contentp[ streamix ];

		ok = ( * contentp->c_opsp->co_init )( contentp );
		if ( ! ok ) {
			return BOOL_FALSE;
		}
	}

	return BOOL_TRUE;
}

static size_t
s_stat( content_strategy_t *csp,
	char statln[ ][ CONTENT_STATSZ ],
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
		percent = ( double )contextp->cc_datadumped
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
		sprintf( statln[ 0 ],
			 "status: %lld/%lld files dumped, "
			 "%.2lf percent complete, "
			 "%u seconds elapsed\n",
			 contextp->cc_statnondirdone,
			 contextp->cc_statnondircnt,
			 percent,
			 elapsed );
		assert( strlen( statln[ 0 ] ) < CONTENT_STATSZ );
		return 1;
	default:
		return 0;
	}
}

static void
s_complete( content_strategy_t *csp )
{
}

/* create - do content stream initialization pass 1
 */
static bool_t
create( content_t *contentp )
{
	return BOOL_TRUE;
}

/* init - do content stream initialization pass 2
 */
static bool_t
init( content_t *contentp )
{
	context_t *contextp = ( context_t * )contentp->c_contextp;
	content_hdr_t *cwhdrp = contentp->c_writehdrp;
	content_inode_hdr_t *scwhdrp =
				( content_inode_hdr_t * )cwhdrp->ch_specific;

	contextp->cc_datadumped = 0;
	contextp->cc_datasz = scwhdrp->cih_inomap_datasz;
	return BOOL_TRUE;
}

/* media marks consist of a media layer opaque cookie and a startpoint.
 * the media layer requires that it be passed a pointer to a media_mark_t.
 * we tack on content-specific baggage (startpt_t). this works because we
 * allocate and free mark_t's here.
 */
struct mark {
	media_mark_t mm;
	startpt_t startpt;
};

typedef struct mark mark_t;

static void
mark_set( content_t *contentp, ino64_t ino, off64_t offset, int32_t flags )
{
	media_t *mediap = contentp->c_mediap;
	media_ops_t *mop = mediap->m_opsp;
	mark_t *markp = ( mark_t * )calloc( 1, sizeof( mark_t ));
	assert( markp );

	if ( flags & STARTPT_FLAGS_NULL ) {
		mlog( MLOG_DEBUG,
		      "setting media NULL mark\n" );
	} else if ( flags & STARTPT_FLAGS_END ) {
		mlog( MLOG_DEBUG,
		      "setting media END mark\n" );
	} else {
		mlog( MLOG_DEBUG,
		      "setting media mark"
		      " for ino %llu offset %lld\n",
		      ino,
		      offset );
	}
		 
	markp->startpt.sp_ino = ino;
	markp->startpt.sp_offset = offset;
	markp->startpt.sp_flags = flags;
	( * mop->mo_set_mark )( mediap,
				mark_callback,
				contentp,
				( media_mark_t * )markp );
}

static void
mark_callback( void *p, media_mark_t *mmp, bool_t committed )
{
	/* this is really a content_t
	 */
	content_t *contentp = ( content_t * )p;

	/* this is really a mark_t, allocated by mark_set()
	 */
	mark_t *markp = ( mark_t * )mmp;

	content_inode_hdr_t *scwhdrp = ( content_inode_hdr_t * )
				       contentp->c_writehdrp->ch_specific;

	media_t *mediap = contentp->c_mediap;

	if ( committed ) {
		/* bump the per-mfile mark committed count
		 */
		context_t *contextp = ( context_t * )contentp->c_contextp;
		contextp->cc_markscommitted++;

		/* copy the mark into the write header: this establishes the
		 * starting point should we need to retry the non-dir portion
		 * of the dump
		 */

		/* log the mark commit
		 */
		if ( markp->startpt.sp_flags & STARTPT_FLAGS_NULL ) {
			mlog( MLOG_DEBUG,
			      "media NULL mark committed"
			      " in media file %d\n",
			      mediap->m_writehdrp->mh_dumpfileix );
			scwhdrp->cih_startpt.sp_flags |= STARTPT_FLAGS_NULL;
		} else if ( markp->startpt.sp_flags & STARTPT_FLAGS_END ) {
			mlog( MLOG_DEBUG,
			      "media END mark committed"
			      " in media file %d\n",
			      mediap->m_writehdrp->mh_dumpfileix );
			if ( scwhdrp->cih_endpt.sp_flags & STARTPT_FLAGS_END ) {
				scwhdrp->cih_startpt.sp_ino++;
				scwhdrp->cih_startpt.sp_offset = 0;
			} else {
				scwhdrp->cih_startpt = scwhdrp->cih_endpt;
			}
			scwhdrp->cih_startpt.sp_flags |= STARTPT_FLAGS_END;
		} else {
			mlog( MLOG_DEBUG,
			      "media mark committed"
			      " for ino %llu offset %lld"
			      " in media file %d\n",
			      markp->startpt.sp_ino,
			      markp->startpt.sp_offset,
			      mediap->m_writehdrp->mh_dumpfileix );
			scwhdrp->cih_startpt = markp->startpt;
		}
	} else {
		/* note the mark was not committed
		 */
		if ( markp->startpt.sp_flags & STARTPT_FLAGS_NULL ) {
			mlog( MLOG_DEBUG,
			      "media NULL mark -NOT- committed\n" );
		} else if ( markp->startpt.sp_flags & STARTPT_FLAGS_END ) {
			mlog( MLOG_DEBUG,
			      "media END mark -NOT- committed\n" );
		} else {
			mlog( MLOG_DEBUG,
			      "media mark -NOT- committed"
			      " for ino %llu offset %lld\n",
			      markp->startpt.sp_ino,
			      markp->startpt.sp_offset );
		}
	}

	/* get rid of this mark (it was allocated by mark_set())
	 */
	free( ( void * )markp );
}

/* begin - called by stream process to invoke the dump stream
 */
static intgen_t
begin_stream( content_t *contentp, size_t vmsize )
{
	media_t *mediap = contentp->c_mediap;
	drive_t *drivep = mediap->m_drivep;
	media_ops_t *mop = mediap->m_opsp;
	content_hdr_t *cwhdrp = contentp->c_writehdrp;
	content_inode_hdr_t *scwhdrp= ( content_inode_hdr_t * )
				      ( void * )
				      cwhdrp->ch_specific;
	media_hdr_t *mwhdrp = mediap->m_writehdrp;
	context_t *contextp = ( context_t * )contentp->c_contextp;
	scontext_t *scontextp = contextp->cc_scontextp;
	size_t streamix = ( size_t )drivep->d_index;
	inv_stmtoken_t inv_stmt;
	bool_t ran_out_of_media;
	bool_t all_nondirs_committed;
	bool_t empty_mediafile;
	intgen_t retrycnt;
	ino64_t firstino;
	uuid_t mediaid;
	char medialabel[ GLOBAL_HDR_STRING_SZ ];
	intgen_t stat;
	time_t elapsed;
	intgen_t rval;

	/* this strategy always places a inomap and dirdump at the
	 * beginning of every media file.
	 */
	scwhdrp->cih_dumpattr |= CIH_DUMPATTR_INOMAP | CIH_DUMPATTR_DIRDUMP;

	/* if so compiled (see -D...CHECKSUM in Makefile),
	 * flag the checksums available
	 */
#ifdef FILEHDR_CHECKSUM
	scwhdrp->cih_dumpattr |= CIH_DUMPATTR_FILEHDR_CHECKSUM;
#endif /* FILEHDR_CHECKSUM */
#ifdef EXTENTHDR_CHECKSUM
	scwhdrp->cih_dumpattr |= CIH_DUMPATTR_EXTENTHDR_CHECKSUM;
#endif /* EXTENTHDR_CHECKSUM */
#ifdef DIRENTHDR_CHECKSUM
	scwhdrp->cih_dumpattr |= CIH_DUMPATTR_DIRENTHDR_CHECKSUM;
#endif /* DIRENTHDR_CHECKSUM */
#ifdef DENTGEN
	scwhdrp->cih_dumpattr |= CIH_DUMPATTR_DIRENTHDR_GEN;
#endif /* DENTGEN */
	
	/* save the first ino and offset.
	 * allows detection of nondirs committed to media,
	 * and used for operator message display
	firstoffset = scwhdrp->cih_startpt.sp_offset;
	 */
	firstino = scwhdrp->cih_startpt.sp_ino;

	/* extract the inventory stream token
	 */
	if ( scontextp->sc_inv_stmtokenp ) {
		inv_stmt = scontextp->sc_inv_stmtokenp[ streamix ];
	} else {
		inv_stmt = INV_TOKEN_NULL;
	}

	/* used to decide if the session inventory can be dumped.
	 */
	ran_out_of_media = BOOL_FALSE;

	/* used to decide if any non-dirs not yet on media
	 */
	all_nondirs_committed = BOOL_FALSE;

	/* used to report non-dir dump progress
	 */
	contextp->cc_statnondircnt = scwhdrp->cih_inomap_nondircnt;
	contextp->cc_statnondirdone = 0;
	contextp->cc_statlast = 0;
	
	/* used to detect generation of an empty media file;
	 * contains at most an inomap and dirdump and null file hdr.
	 */
	empty_mediafile = BOOL_FALSE;

	/* tell the media layer to begin a stream
	 */
	rval = ( * mop->mo_begin_stream )( mediap );
	if ( rval ) {
		if ( rval == MEDIA_ERROR_ABORT ) {
			return STREAM_EXIT_ABORT;
		} else {
			return STREAM_EXIT_CORE;
		}
	}

	/* loop, dumping media files, until the entire stream is dumped.
	 * each time we hit EOM/EOF, repeat the inomap and directory dump.
	 * dump the non-dirs beginning with the current startpoint.
	 * The current startpoint will be updated each time a media mark
	 * is committed.
	 */
	for ( retrycnt = 0 ; ; retrycnt++  ) {
		ino64_t startino;
		intgen_t mediafileix;
		bool_t stop_requested;
		bool_t all_dirs_committed;
		bool_t all_nondirs_sent;
		off64_t startoffset;
		off64_t ncommitted;
		bool_t done;

		/* used to decide whether or not to go back for more.
		 */
		stop_requested = BOOL_FALSE;

		/* used to decide if the media file contains all
		 * of the inomap and dirdump.
		 */
		all_dirs_committed = BOOL_FALSE;

		/* used to decide if all non-dirs were sent (not necessarily
		 * committed)
		 */
		all_nondirs_sent = BOOL_FALSE;

		/* always clear the NULL flag from the stream startpoint
		 * before beginning the media file. allows detection
		 * of null file hdr commit.
		 */
		scwhdrp->cih_startpt.sp_flags &= ~STARTPT_FLAGS_NULL;

		/* used to decide if the null file header was fully
		 * committed to media.
		null_committed = BOOL_FALSE;
		 */

		/* save the original start points, to be given to
		 * the inventory at the end of each media file.
		startflags = scwhdrp->cih_startpt.sp_flags;
		 */
		startino = scwhdrp->cih_startpt.sp_ino;
		startoffset = scwhdrp->cih_startpt.sp_offset;

		/* modify the write header if this is the first retry
		 */
		if ( retrycnt == 1 ) {
			scwhdrp->cih_dumpattr |= CIH_DUMPATTR_RETRY;
		}

		/* set the accumulated file size to zero.
		 * this will be monitored by dump_file() to decide
		 * if the current dump file is too long. if so,
		 * it will set a startpoint and spoof an EOF.
		 * this will cause a new dump file to be started,
		 * beginning at the startpoint.
		 */
		contextp->cc_mfilesz = 0;

		/* tell the media manager to begin a new file.
		 * this dumps the write hdr as a side-effect.
		 */
		mlog( MLOG_VERBOSE,
		      "beginning media file\n" );
		rval = ( * mop->mo_begin_write )( mediap );
		if ( rval == MEDIA_ERROR_ABORT ) {
			return STREAM_EXIT_ABORT;
		}
		if ( rval == MEDIA_ERROR_CORE ) {
			return STREAM_EXIT_CORE;
		}
		if ( rval == MEDIA_ERROR_TIMEOUT ) {
			mlog( MLOG_VERBOSE,
			      "media change timeout will be treated as "
			      "an interruption: can resume later\n" );
			ran_out_of_media = BOOL_TRUE;
			break;
		}
		if ( rval == MEDIA_ERROR_STOP ) {
			mlog( MLOG_VERBOSE,
			      "media change decline will be treated as "
			      "an interruption: can resume later\n" );
			ran_out_of_media = BOOL_TRUE;
			break;
		}
		if ( rval != 0 ) {
			assert( 0 );
			return STREAM_EXIT_CORE;
		}
		mlog( MLOG_VERBOSE,
		      "media file %u (media %u, file %u)\n",
		      mwhdrp->mh_dumpfileix,
		      mwhdrp->mh_mediaix,
		      mwhdrp->mh_mediafileix );

		/* initialize the count marks committed in the media file.
		 * will be bumped by mark_callback().
		 */
		contextp->cc_markscommitted = 0;

		/* extract the index of this media file within the media object.
		 * will be recorded in the inventory.
		 */
		mediaid = mwhdrp->mh_mediaid;
		strcpy( medialabel, mwhdrp->mh_medialabel );
		mediafileix = ( intgen_t )mwhdrp->mh_mediafileix;

		/* first dump the inomap
		 */
		mlog( MLOG_VERBOSE,
		      "dumping ino map\n" );
		rval = inomap_dump( contentp );
		if ( rval == MEDIA_ERROR_ABORT ) {
			return STREAM_EXIT_ABORT;
		}
		if ( rval == MEDIA_ERROR_CORE ) {
			return STREAM_EXIT_CORE;
		}
		if ( rval ) {
			goto decision_more;
		}

		/* now dump the directories. use the bigstat iterator
		 * capability to call my dump_dir function
		 * for each directory in the bitmap.
		 */
		mlog( MLOG_VERBOSE,
		      "dumping directories\n" );
		stat = 0;
		contextp->cc_statdirdone = 0;
		contextp->cc_statdircnt = scwhdrp->cih_inomap_dircnt;
		contextp->cc_statstate = ( statstate_t )STATE_DIR;
		rval = bigstat_iter( contextp->cc_fshandlep,
				     contextp->cc_fsfd,
				     BIGSTAT_ITER_DIR,
				     ( ino64_t )0,
				     dump_dir,
				     ( void * )contentp,
				     &stat );
		if ( rval ) {
			return STREAM_EXIT_CORE;
		}
		if ( stat == MEDIA_ERROR_ABORT ) {
			return STREAM_EXIT_ABORT;
		}
		if ( stat == MEDIA_ERROR_CORE ) {
			return STREAM_EXIT_CORE;
		}
		if ( stat == -2 ) {
			/* operator-requested interrupt during
			 * directory dump
			 */
			stop_requested = BOOL_TRUE;
			goto decision_more;
		}
		if ( stat ) {
			goto decision_more;
		}

		/* finally, dump the non-directory files beginning with this
		 * stream's startpoint. Note that dump_file will set one or
		 * more media marks; the callback will update the hdr's
		 * startpoint; thus each time a demarcated portion of a
		 * non-directory file is fully committed to media,
		 * the starting point for the next media file will be advanced.
		 */
		if ( ! all_nondirs_committed ) {
			if ( streamix == 0
			     &&
			     startino == firstino
			     &&
			     startoffset == 0
			     &&
			     ! ( scwhdrp->cih_dumpattr & CIH_DUMPATTR_RESUME)) {
				mlog( MLOG_VERBOSE,
				      "dumping non-directory files\n" );
			} else {
				mlog( MLOG_VERBOSE,
				      "dumping non-directory files\n" );
			}
			contextp->cc_statstate = ( statstate_t )STATE_NONDIR;
			rval = bigstat_iter( contextp->cc_fshandlep,
					     contextp->cc_fsfd,
					     BIGSTAT_ITER_NONDIR,
					     scwhdrp->cih_startpt.sp_ino,
					     dump_file,
					     ( void * )contentp,
					     &stat );
			if ( rval ) {
				return STREAM_EXIT_CORE;
			}
			if ( stat == MEDIA_ERROR_ABORT ) {
				return STREAM_EXIT_ABORT;
			}
			if ( stat == MEDIA_ERROR_CORE ) {
				return STREAM_EXIT_CORE;
			}
			if ( stat == -2 ) {
				/* operator-requested interrupt during
				 * directory dump
				 */
				stop_requested = BOOL_TRUE;
				goto decision_more;
			}
			if ( stat && stat != -1 ) {
				goto decision_more;
			}
		}

		/* if we got here, all files were sent without hitting
		 * the end of the current media object, or hitting the
		 * media file size limit. send the special END mark.
		 * this is only send at the end of the last media file in the
		 * dump session. if it actually gets committed to media,
		 * we know the last data written to media all made it.
		 * we won't know if this mark is committed to media until
		 * we attempt to end the write stream.
		 */
		all_nondirs_sent = BOOL_TRUE;
		mark_set( contentp,
			  INO64MAX,
			  OFF64MAX,
			  STARTPT_FLAGS_END );

decision_more:

		/* write a null file hdr, to let restore recognize
		 * the end of the media file.  the flags indicate
		 * whether or not this is intended to be the last
		 * media file in the stream.
		 */
		rval = dump_filehdr( contentp,
				     0,
				     0,
				     all_nondirs_sent
				     ?
				     FILEHDR_FLAGS_NULL | FILEHDR_FLAGS_END
				     :
				     FILEHDR_FLAGS_NULL );
		if ( rval == MEDIA_ERROR_ABORT ) {
			return STREAM_EXIT_ABORT;
		}
		if ( rval == MEDIA_ERROR_CORE ) {
			return STREAM_EXIT_CORE;
		}

		/* send a mark to detect if the null file header made it.
		 */
		mark_set( contentp,
			  INO64MAX,
			  OFF64MAX,
			  all_nondirs_sent
			  ?
			  STARTPT_FLAGS_NULL | STARTPT_FLAGS_END
			  :
			  STARTPT_FLAGS_NULL );

		/* tell the media manager to end the media file.
		 * this is done before the inventory update, to
		 * see how much was actually committed to media.
		 */
		mlog( MLOG_VERBOSE,
		      "ending media file\n" );
		rval = ( * mop->mo_end_write )( mediap, &ncommitted );
		if ( rval == MEDIA_ERROR_ABORT ) {
			return STREAM_EXIT_ABORT;
		}
		if ( rval == MEDIA_ERROR_CORE ) {
			return STREAM_EXIT_CORE;
		}
		mlog( MLOG_VERBOSE,
		      "media file size %lld bytes\n",
		      ncommitted );

		/* if at least one mark committed, we know all of
		 * the inomap and dirdump was committed.
		 */
		all_dirs_committed = ( contextp->cc_markscommitted > 0 );

		/* at this point we can check the new start point
		 * to determine if all nondirs have been committed.
		 * if this flag was already set, then this is a
		 * inomap and dirdump-only media file.
		 */
		if ( scwhdrp->cih_startpt.sp_flags & STARTPT_FLAGS_END ) {
			if ( all_nondirs_committed ) {
				empty_mediafile = BOOL_TRUE;
			}
			all_nondirs_committed = BOOL_TRUE;
		}

		/*
		if ( scwhdrp->cih_startpt.sp_flags & STARTPT_FLAGS_NULL ) {
			null_committed = BOOL_TRUE;
		}
		*/

		/* we are done if all nondirs have been committed.
		 * it is not necessary for the null file header to have
		 * been committed.
		 */
		done = all_nondirs_committed;

		/* tell the inventory about the media file
		 */
		if ( inv_stmt != INV_TOKEN_NULL ) {
			bool_t ok;

			if ( ! all_dirs_committed ) {
				mlog( MLOG_DEBUG,
				      "giving inventory "
				      "partial dirdump media file\n" );
			} else if ( done && empty_mediafile ) {
				mlog( MLOG_DEBUG,
				      "giving inventory "
				      "empty last media file: "
				      "%llu:%lld\n",
				       startino,
				       startoffset );
			} else if ( empty_mediafile ) {
				mlog( MLOG_DEBUG,
				      "giving inventory "
				      "empty media file: "
				      "%llu:%lld\n",
				       startino,
				       startoffset );
			} else if ( done ) {
				mlog( MLOG_DEBUG,
				      "giving inventory "
				      "last media file: "
				      "%llu:%lld\n",
				       startino,
				       startoffset );
			} else {
				mlog( MLOG_DEBUG,
				      "giving inventory "
				      "media file: "
				      "%llu:%lld - %llu:%lld\n",
				       startino,
				       startoffset,
				       scwhdrp->cih_startpt.sp_ino,
				       scwhdrp->cih_startpt.sp_offset );
			}
			assert( mediafileix >= 0 );

			ok = inv_put_mediafile( inv_stmt,
						&mediaid,
						medialabel,
						( u_intgen_t )mediafileix,
						startino,
						startoffset,
						scwhdrp->cih_startpt.sp_ino,
						scwhdrp->cih_startpt.sp_offset,
						ncommitted,
					        all_dirs_committed
						&&
						! empty_mediafile,
						BOOL_FALSE );
			if ( ! ok ) {
				mlog( MLOG_NORMAL,
				      "inventory media file put failed\n" );
			}
			if ( done ) {
				scontextp->sc_inv_interruptedp[ streamix ] =
								    BOOL_FALSE;
				    /* so inv_end_stream will know
				     */
			}
		}

		/* don't go back for more if done or stop was requested
		 */
		if ( done || stop_requested ) {
			break;
		}
	}

	/* dump the session inventory here, if the drive
	 * supports multiple media files. don't bother if
	 * we ran out of media
	 */
	if ( ! ran_out_of_media
	     &&
	     ( drivep->d_capabilities & DRIVE_CAP_FILES )) {
		dump_session_inv( contentp );
	}

	/* tell the media layer to end the stream
	 */
	( * mop->mo_end_stream )( mediap );

	elapsed = time( 0 ) - contentp->c_gwritehdrp->gh_timestamp;

#ifdef MULTISTREAM
	mlog( MLOG_VERBOSE,
	      "stream %d complete: %u seconds elapsed\n",
	      streamix,
	      elapsed );
#else /* MULTISTREAM */
	mlog( MLOG_VERBOSE,
	      "dump complete: %u seconds elapsed\n",
	      elapsed );
#endif /* MULTISTREAM */

	return STREAM_EXIT_SUCCESS;
}

static intgen_t
dump_dir( void *arg1,
	  jdm_fshandle_t *fshandlep,
	  intgen_t fsfd,
	  xfs_bstat_t *statp )
{
	content_t *contentp = ( content_t * )arg1;
	context_t *contextp = ( context_t * )contentp->c_contextp;
	void *inomap_state_contextp = contextp->cc_inomap_state_contextp;
	intgen_t state;
	intgen_t fd;
#ifdef GETDENTS64
	dirent64_t *gdp = ( dirent64_t *)contextp->cc_getdents64bufp;
#else /* GETDENTS64 */
	dirent_t *gdp = ( dirent_t *)contextp->cc_getdents64bufp;
#endif /* GETDENTS64 */
	size_t gdsz = contextp->cc_getdents64bufsz;
	intgen_t gdcnt;
	u_int32_t gen;
	intgen_t rval;

	/* no way this can be non-dir, but check anyway
	 */
	assert( ( statp->bs_mode & S_IFMT ) == S_IFDIR );
	if ( ( statp->bs_mode & S_IFMT ) != S_IFDIR ) {
		contextp->cc_statdirdone++;
		return 0;
	}

	/* skip if no links
	 */
	if ( statp->bs_nlink < 1 ) {
		return 0;
	}

	/* see what the inomap says about this ino
	 */
	state = inomap_state( inomap_state_contextp, statp->bs_ino );

	/* skip if not in inomap
	 */
	if ( state == MAP_INO_UNUSED
	     ||
	     state == MAP_DIR_NOCHNG
	     ||
	     state == MAP_NDR_NOCHNG ) {
		if ( state == MAP_NDR_NOCHNG ) {
			mlog( MLOG_DEBUG,
			      "inomap inconsistency ino %llu: "
			      "map says is non-dir but is dir: skipping\n",
			      statp->bs_ino );
		}
		return 0;
	}

	/* note if map says a non-dir
	 */
	if ( state == MAP_NDR_CHANGE ) {
		mlog( MLOG_DEBUG,
		      "inomap inconsistency ino %llu: "
		      "map says non-dir but is dir: skipping\n",
		      statp->bs_ino );
		return 0;
	}

	/* if getdents64() not yet available, and ino occupied more
	 * than 32 bits, warn and skip.
	 */
#ifndef GETDENTS64
	if ( statp->bs_ino > ( ino64_t )INOMAX ) {
		mlog( MLOG_NORMAL,
		      "WARNING: unable to dump dirent: ino %llu too large\n",
		      statp->bs_ino );
		contextp->cc_statdirdone++;
		return 0; /* continue anyway */
	}
#endif /* ! GETDENTS64 */
	
	/* check if the operator has requested to interrupt the dump.
	 */
	if ( cldmgr_stop_requested( )) {
		mlog( MLOG_NORMAL,
		      "dump interrupted prior to directory ino %llu\n",
		      statp->bs_ino );
		contextp->cc_statdirdone++;
		return -2;
	}

	mlog( MLOG_TRACE,
	      "dumping directory ino %llu\n",
	      statp->bs_ino );

	/* open the directory
	 */
	fd = jdm_open( fshandlep, statp, O_RDONLY );
	if ( fd < 0 ) {
		mlog( MLOG_NORMAL,
		      "WARNING: unable to open directory: ino %llu\n",
		      statp->bs_ino );
		contextp->cc_statdirdone++;
		return 0; /* continue anyway */
	}

	/* dump the file header.
	 */
	rval = dump_filehdr( contentp, statp, 0, 0 );
	if ( rval ) {
		close( fd );
		return rval;
	}
	
	/* dump attributes here
	 */

	/* dump dirents - lots of buffering done here, to achieve OS-
	 * independence. if proves to be to much overhead, can streamline.
	 */
	for ( gdcnt = 1, rval = 0 ; rval == 0 ; gdcnt++ ) {
#ifdef GETDENTS64
		dirent64_t *p;
#else /* GETDENTS64 */
		dirent_t *p;
#endif /* GETDENTS64 */
		intgen_t nread;
		register size_t reclen;

#ifdef GETDENTS64
		nread = getdents64( fd, gdp, gdsz );
#else /* GETDENTS64 */
		nread = getdents( fd, gdp, gdsz );
#endif /* GETDENTS64 */
		
		/* negative count indicates something very bad happened;
		 * try to gracefully end this dir.
		 */
		if ( nread < 0 ) {
			mlog( MLOG_NORMAL,
			      "WARNING: unable to read dirents (%d) for "
			      "directory ino %llu: %s\n",
			      gdcnt,
			      statp->bs_ino,
			      strerror( errno ));
			nread = 0; /* pretend we are done */
		}

		/* no more directory entries: break;
		 */
		if ( nread == 0 ) {
			break;
		}

		/* translate and dump each entry: skip "." and ".."
		 * and null entries.
		 */
		for ( p = gdp,
		      reclen = ( size_t )p->d_reclen
		      ;
		      nread > 0
		      ;
		      nread -= ( intgen_t )reclen,
		      assert( nread >= 0 ),
#ifdef GETDENTS64
		      p = ( dirent64_t * )( ( char * )p + reclen ),
#else /* GETDENTS64 */
		      p = ( dirent_t * )( ( char * )p + reclen ),
#endif /* GETDENTS64 */
		      reclen = ( size_t )p->d_reclen ) {
			ino64_t ino;
			register size_t namelen = strlen( p->d_name );
			register size_t nameszmax = ( size_t )reclen
						    -
#ifdef GETDENTS64
						    offsetofmember( dirent64_t,
								    d_name );
#else /* GETDENTS64 */
						    offsetofmember( dirent_t,
								    d_name );
#endif /* GETDENTS64 */

			/* getdents(2) guarantees that the string will
			 * be null-terminated, but the record may have
			 * padding after the null-termination.
			 */
			assert( namelen < nameszmax );

			/* skip "." and ".."
			 */
			if ( *( p->d_name + 0 ) == '.'
			     &&
			     ( *( p->d_name + 1 ) == 0
			       ||
			       ( *( p->d_name + 1 ) == '.'
				 &&
				 *( p->d_name + 2 ) == 0 ))) {
				continue;
			}

#ifdef GETDENTS64
			ino = p->d_ino;
#else /* GETDENTS64 */
			ino = ( ino64_t )p->d_ino;
#endif /* GETDENTS64 */

			if ( ino == 0 ) {
				mlog( MLOG_NORMAL,
				      "WARNING: encountered 0 ino (%s) in "
				      "directory ino %llu: NOT dumping\n",
				      p->d_name,
				      statp->bs_ino );
				continue;
			}

#ifdef DENTGEN
			{
				xfs_bstat_t statbuf;
				intgen_t scrval;
				
				scrval = bigstat_one( fshandlep,
						      fsfd,
#ifdef GETDENTS64
						      p->d_ino,
#else /* GETDENTS64 */
						      ( ino64_t )p->d_ino,
#endif /* GETDENTS64 */
						      &statbuf );
				if ( scrval ) {
					mlog( MLOG_NORMAL,
					      "WARNING: could not stat "
					      "dirent %s ino %llu: %s: "
					      "using null generation count "
					      "in directory entry\n",
					      p->d_name,
					      ( ino64_t )p->d_ino,
					      strerror( errno ));
					gen = 0;
				} else {
					gen = statbuf.bs_gen;
				}
			}
#else /* DENTGEN */
			gen = 0;
#endif /* DENTGEN */

			rval = dump_dirent( contentp,
					    statp,
					    ino,
					    gen,
					    p->d_name,
					    namelen );
			if ( rval ) {
				break;
			}
		}
	}

	/* write a null dirent hdr, unless trouble encountered in the loop
	 */
	if ( ! rval ) {
		rval = dump_dirent( contentp, statp, 0, 0, 0, 0 );
	}

	close( fd );

	/* if an error occurred, just return the error
	 */
	contextp->cc_statdirdone++;
	return rval;
}

/* this function is called by the bigstat iterator for all non-directory
 * files. it passes the buck to file type-specific dump functions.
 * return value is MEDIA_ERROR_EOF if the media file is getting too big,
 * MEDIA_ERROR_(other) if trouble encountered with the media,
 * 0 if file completely dumped, -1 if we've encountered the stream endpt,
 * -2 if operator interrupted the dump.
 */
static intgen_t
dump_file( void *arg1,
	   jdm_fshandle_t *fshandlep,
	   intgen_t fsfd,
	   xfs_bstat_t *statp )
{
	content_t *contentp = ( content_t * )arg1;
	context_t *contextp = ( context_t * )contentp->c_contextp;
	content_hdr_t *cwhdrp = contentp->c_writehdrp;
	content_inode_hdr_t *scwhdrp = ( content_inode_hdr_t * )
				       ( void * )
				       cwhdrp->ch_specific;
	startpt_t *startptp = &scwhdrp->cih_startpt;
	startpt_t *endptp = &scwhdrp->cih_endpt;
	intgen_t state;
	intgen_t rval;

	/* skip if no links
	 */
	if ( statp->bs_nlink < 1 ) {
		if ( statp->bs_ino > contextp->cc_statlast ) {
			contextp->cc_statlast = statp->bs_ino;
		}
		return 0;
	}

	/* skip if prior to startpoint
	 */
	if ( statp->bs_ino < startptp->sp_ino ) {
		if ( statp->bs_ino > contextp->cc_statlast ) {
			contextp->cc_statlast = statp->bs_ino;
		}
		return 0;
	}

	/* skip if at or beyond next startpoint. return non-zero to
	 * abort iteration.
	 */
	if ( ! ( endptp->sp_flags & STARTPT_FLAGS_END )) {
		if ( endptp->sp_offset == 0 ) {
			if ( statp->bs_ino >= endptp->sp_ino ) {
				if ( statp->bs_ino > contextp->cc_statlast ) {
					contextp->cc_statlast = statp->bs_ino;
				}
				return -1;
			}
		} else {
			if ( statp->bs_ino > endptp->sp_ino ) {
				if ( statp->bs_ino > contextp->cc_statlast ) {
					contextp->cc_statlast = statp->bs_ino;
				}
				return -1;
			}
		}
	}

	/* see what the inomap says about this ino
	 */
	state = inomap_state( contextp->cc_inomap_state_contextp,
			      statp->bs_ino );

	/* skip if not in inomap
	 */
	if ( state == MAP_INO_UNUSED
	     ||
	     state == MAP_DIR_NOCHNG
	     ||
	     state == MAP_NDR_NOCHNG ) {
		if ( state == MAP_DIR_NOCHNG ) {
			mlog( MLOG_DEBUG,
			      "inomap inconsistency ino %llu: "
			      "MAP_DIR_NOCHNG but is non-dir: skipping\n",
			      statp->bs_ino );
		}
		if ( statp->bs_ino > contextp->cc_statlast ) {
			contextp->cc_statlast = statp->bs_ino;
		}
		return 0;
	}

	/* note if map says a dir
	 */
	if ( state == MAP_DIR_CHANGE || state == MAP_DIR_SUPPRT ) {
		mlog( MLOG_NORMAL,
		      "WARNING: inomap inconsistency ino %llu: "
		      "%s but is now non-dir: NOT dumping\n",
		      statp->bs_ino,
		      state == MAP_DIR_CHANGE
		      ?
		      "map says changed dir"
		      :
		      "map says unchanged dir" );
	}


	/* pass on to specific dump function
	 */
	switch( statp->bs_mode & S_IFMT ) {
	case S_IFREG:
		/* ordinary file
		 */
		rval = dump_file_reg( contentp, fshandlep, statp );
		if ( statp->bs_ino > contextp->cc_statlast ) {
			contextp->cc_statnondirdone++;
			contextp->cc_statlast = statp->bs_ino;
		}
		return rval;
	case S_IFCHR:
	case S_IFBLK:
	case S_IFIFO:
	case S_IFNAM:
	case S_IFLNK:
		/* only need a filehdr_t; no data
		 */
		rval =  dump_file_spec( contentp, fshandlep, statp );
		if ( statp->bs_ino > contextp->cc_statlast ) {
			contextp->cc_statnondirdone++;
			contextp->cc_statlast = statp->bs_ino;
		}
		return rval;
	case S_IFDIR:
		/* no way this can be a dir, but check anyway
		 */
		assert( 0 );
		return 0;
	case S_IFSOCK:
		/* don't dump these
		 */
		if ( statp->bs_ino > contextp->cc_statlast ) {
			contextp->cc_statnondirdone++;
			contextp->cc_statlast = statp->bs_ino;
		}
		return 0;
	default:
		/* don't know how to dump these
		 */
		mlog( MLOG_VERBOSE,
		      "don't know how to dump ino %llu: mode %08x\n",
		      statp->bs_ino,
		      statp->bs_mode );
		if ( statp->bs_ino > contextp->cc_statlast ) {
			contextp->cc_statnondirdone++;
			contextp->cc_statlast = statp->bs_ino;
		}
		return 0;
	/* not yet implemented
	case S_IFMNT:
	 */
	}
}

/* a regular file may be broken into several portions if its size
 * is large. Each portion begins with a filehdr_t and is followed by
 * several extents. each extent begins with an extenthdr_t. returns 0
 * if all extents dumped, MEDIA_ERROR_... on media error, or -2 if
 * operator requested stop.
 */
static intgen_t
dump_file_reg( content_t *contentp,
	       jdm_fshandle_t *fshandlep,
	       xfs_bstat_t *statp )
{
	media_t *mediap = contentp->c_mediap;
	context_t *contextp = ( context_t * )contentp->c_contextp;
	scontext_t *scontextp = contextp->cc_scontextp;
	content_hdr_t *cwhdrp = contentp->c_writehdrp;
	content_inode_hdr_t *scwhdrp = ( content_inode_hdr_t * )
				       ( void * )
				       cwhdrp->ch_specific;
	startpt_t *startptp = &scwhdrp->cih_startpt;
	startpt_t *endptp = &scwhdrp->cih_endpt;
	extent_group_context_t extent_group_context;
	bool_t cmpltflg;
	off64_t offset;
	off64_t stopoffset;
	bool_t sosig;	/* stop offset is significant */
	off64_t maxextentcnt;
	intgen_t rval;

	/* determine the offset within the file where the dump should begin.
	 * it must have been aligned to the basic fs block size by the
	 * startpoint calculations done during strategy initialization.
	 */
	if ( statp->bs_ino == startptp->sp_ino ) {
		offset = startptp->sp_offset;
		assert( ( offset & ( off64_t )( BBSIZE - 1 )) == 0 );
	} else {
		offset = 0;
	}

	/* if this is a resumed dump and the resumption begins somewhere
	 * within this file, and that point is greater than offset set
	 * above, and that file hasn't changed since the resumed dump,
	 * modify offset.
	 */
	if ( scontextp->sc_resume ) {
		drange_t *drangep = scontextp->sc_resumerangep;
		size_t drangecnt = scontextp->sc_resumerangecnt;
		size_t drangeix;

		for ( drangeix = 0 ; drangeix < drangecnt ; drangeix++ ) {
			drange_t *rp = &drangep[ drangeix ];
			if ( statp->bs_ino == rp->dr_begin.sp_ino ) {
				register time_t mtime = statp->bs_mtime.tv_sec;
				register time_t ctime = statp->bs_ctime.tv_sec;
				register time_t ltime = max( mtime, ctime );
				if ( ltime < scontextp->sc_resumetime ) {
					if ( rp->dr_begin.sp_offset > offset ){
						offset =rp->dr_begin.sp_offset;
					}
				}
				break;
			}
		}
		assert( ( offset & ( off64_t )( BBSIZE - 1 )) == 0 );
	}
		
	mlog( MLOG_TRACE,
	      "dumping regular file ino %llu offset %lld size %lld\n",
	      statp->bs_ino,
	      offset,
	      statp->bs_size );

	/* determine the offset within the file where the dump should end.
	 * only significant if this is an inode spanning a startpoint.
	 */
	if ( endptp->sp_flags & STARTPT_FLAGS_END ) {
		sosig = BOOL_FALSE;
		stopoffset = 0;
	} else if ( statp->bs_ino == endptp->sp_ino ) {
		sosig = BOOL_TRUE;
		stopoffset = endptp->sp_offset;
	} else {
		sosig = BOOL_FALSE;
		stopoffset = 0;
	}

	/* SHOULD FILEHDR differentiate the beginning from continuation due
	 * to EOF from continuation due to grouping of extents?
	 */

	/* calculate the maximum extent group size. files larger than this
	 * will be broken into multiple extent groups, each with its own
	 * folehdr_t.
	 */
	maxextentcnt = mediap->m_strategyp->ms_recmarksep;

	/* initialize the extent group context. if fails, just return,
	 * pretending the dump succeeded.
	 */
	rval = init_extent_group_context( fshandlep,
					  statp,
					  &extent_group_context );
	if ( rval ) {
		mlog( MLOG_NORMAL,
		      "WARNING: could not open regular file ino %llu mode 0x%0x8:"
		      " %s: not dumped\n",
		      statp->bs_ino,
		      statp->bs_mode,
		      strerror( errno ));
		return 0;
	}

	/* loop here, dumping marked groups of extents. each extent group
	 * is preceeded by a filehdr_t. this is required so that the
	 * recovery side can identify the fs file at each marked point
	 * in the stream. dump_extent_group returns MEDIA_ERROR
	 * values. it sets by reference offset, bytecnt, and cmpltflg.
	 * it is important to understand that if a fs file is real big,
	 * it will be dumped in parts (extent groups), each beginning with
	 * an identical filehdr_t.
	 */
	cmpltflg = BOOL_FALSE;

	rval = 0;
	for ( ; ; ) {
		off64_t bytecnt = 0;
		off64_t bc;

		/* see if we are done.
		 */
		if ( cmpltflg ) {
			assert( rval == 0 );
			break;
		}

		/* set a mark - important to do this now, before deciding
		 * the media file is to big or the operator asked to
		 * interrupt the dump. this mark, if committed, indicates
		 * the previous fs file / extent group was completely dumped.
		 */
		mark_set( contentp, statp->bs_ino, offset, 0 );

		/* spoof EOF if the media file size is getting too big.
		 * note that the most we can go over is ms_recmarksep.
		 */
		if (contextp->cc_mfilesz >= mediap->m_strategyp->ms_recmfilesz){
			rval = MEDIA_ERROR_EOF;
			break;
		}

		/* check if the operator has requested to interrupt the dump.
		 */
		if ( cldmgr_stop_requested( )) {
			mlog( MLOG_NORMAL,
			      "dump interrupted prior to ino %llu offset %lld\n",
			      statp->bs_ino,
			      offset );
			rval = -2;
			break;
		}

		/* dump the file header
		 */
		mlog( MLOG_DEBUG,
		      "dumping extent group ino %llu offset %lld\n",
		      statp->bs_ino,
		      offset );
		rval = dump_filehdr( contentp, statp, offset, 0 );
		if ( rval ) {
			break;
		}
		bytecnt += sizeof( filehdr_t );

		/* dump attributes here
		 */

		/* dump a group of extents. returns by reference
		 * the offset of the next extent group (to be placed
		 * in the next mark), the total number of bytes written
		 * to media (headers and all), and a flag indicating
		 * all extents have been dumped.
		 */
		bc = 0; /* for lint */
		rval = dump_extent_group( contentp,
					  statp,
					  &extent_group_context,
					  maxextentcnt,
					  stopoffset,
					  sosig,
					  &offset,
					  &bc,
					  &cmpltflg );
		bytecnt += bc;
		contextp->cc_datadumped += ( u_int64_t )bc;
		if ( rval ) {
			break;
		}

		/* dump LAST extent hdr. one of these is placed at the
		 * end of each dumped file. necessary to detect the
		 * end of the file.
		 */
		rval = dump_extenthdr( contentp, EXTENTHDR_TYPE_LAST, 0, 0, 0);
		if ( rval ) {
			break;
		}
		bytecnt += sizeof( extenthdr_t );

		/* update the media file size
		 */
		contextp->cc_mfilesz += bytecnt;

	}

	cleanup_extent_group_context( &extent_group_context );
	return rval;
}

/* dumps character, block, and fifo - special files. no data, just meta-data,
 * all contained within the filehdr_t.
 * also handles symbolic link files: appends a variable-length string after the
 * filehdr_t.
 */
static intgen_t
dump_file_spec( content_t *contentp,
		jdm_fshandle_t *fshandlep,
		xfs_bstat_t *statp )
{
	context_t *contextp = ( context_t * )contentp->c_contextp;
	media_t *mediap = contentp->c_mediap;
	intgen_t rval;

	mlog( MLOG_TRACE,
	      "dumping special file ino %llu mode 0x%04x\n",
	      statp->bs_ino,
	      statp->bs_mode );

	/* set a mark - important to do this now, before deciding
	 * the media file is to big. this mark, if committed,
	 * indicates the previous fs file was completely dumped.
	 */
	mark_set( contentp, statp->bs_ino, 0, 0 );

	/* dump the file header
	 */
	rval = dump_filehdr( contentp, statp, 0, 0 );
	if ( rval ) {
		return rval;
	}

	/* update the media file size
	 */
	contextp->cc_mfilesz += sizeof( filehdr_t );

	/* dump attributes here
	 */

	/* if a symbolic link, also dump the link pathname.
	 * use an extent header to represent the pathname. the
	 * extent sz will always be a multiple of SYMLINK_ALIGN.
	 * the symlink pathname char string will always  be NULL-terminated.
	 */
	if ( ( statp->bs_mode & S_IFMT ) == S_IFLNK ) {
		intgen_t nread;
		size_t extentsz;

		/* read the link path. if error, dump a zero-length
		 * extent. in any case, nread will contain the number of
		 * bytes to dump, and contextp->cc_direntbufp will contain
		 * the bytes.
		 */
		nread = jdm_readlink( fshandlep,
				      statp,
				      contextp->cc_readlinkbufp,
				      contextp->cc_readlinkbufsz );
		if ( nread < 0 ) {
			mlog( MLOG_DEBUG,
			      "could not read symlink ino %llu\n",
			      statp->bs_ino );
			nread = 0;
		}

		/* null-terminate the string
		 */
		assert( ( size_t )nread < contextp->cc_readlinkbufsz );
		contextp->cc_readlinkbufp[ nread ] = 0;

		/* calculate the extent size - be sure to include room
		 * for the null-termination.
		 */
		extentsz = ( ( size_t )nread + 1 + ( SYMLINK_ALIGN - 1 ))
			   &
			   ~ ( SYMLINK_ALIGN - 1 );
		assert( extentsz <= contextp->cc_readlinkbufsz );

		/* dump an extent header
		 */
		rval = dump_extenthdr( contentp,
				       EXTENTHDR_TYPE_DATA,
				       0,
				       0,
				       ( off64_t )extentsz );
		if ( rval ) {
			return rval;
		}

		/* dump the link path extent
		 */
		rval = write_buf( contextp->cc_readlinkbufp,
				  extentsz,
				  ( void * )mediap,
				  ( gwbfp_t )mediap->m_opsp->mo_get_write_buf,
				  ( wfp_t )mediap->m_opsp->mo_write );
		if ( rval ) {
			return rval;
		}
	}

	return 0;
}

/* contrives the initial state of the extent group context such that
 * dump_extent_group() will fetch some extents from the kernel before it
 * does anything else.
 */
static intgen_t
init_extent_group_context( jdm_fshandle_t *fshandlep,
			   xfs_bstat_t *statp,
			   extent_group_context_t *gcp )
{
	bool_t isrealtime;
	intgen_t oflags;

	isrealtime = ( bool_t )(statp->bs_xflags & XFS_XFLAG_REALTIME );
	oflags = O_RDONLY;
	if ( isrealtime ) {
		oflags |= O_DIRECT;
	}
	( void )memset( ( void * )gcp, 0, sizeof( *gcp ));
	gcp->eg_bmap[ 0 ].bmv_offset = 0;
	gcp->eg_bmap[ 0 ].bmv_length = -1;
	gcp->eg_bmap[ 0 ].bmv_count = BMAP_LEN;
	gcp->eg_nextbmapp = &gcp->eg_bmap[ 1 ];
	gcp->eg_endbmapp = &gcp->eg_bmap[ 1 ];
	gcp->eg_bmapix = 0;
	gcp->eg_gbmcnt = 0;
	gcp->eg_fd = jdm_open( fshandlep, statp, oflags );
	if ( gcp->eg_fd < 0 ) {
		return gcp->eg_fd;
	}
	return 0;
}

static void
cleanup_extent_group_context( extent_group_context_t *gcp )
{
	( void )close( gcp->eg_fd );
}

static intgen_t
dump_extent_group( content_t *contentp,
		   xfs_bstat_t *statp,
		   extent_group_context_t *gcp,
		   off64_t maxcnt,
		   off64_t stopoffset,
		   bool_t sosig,
		   off64_t *nextoffsetp,
		   off64_t *bytecntp,
		   bool_t *cmpltflgp )
{
	media_t *mediap = contentp->c_mediap;
	media_ops_t *mop = mediap->m_opsp;
	bool_t isrealtime = ( bool_t )( statp->bs_xflags
					&
					XFS_XFLAG_REALTIME );
	off64_t nextoffset;
	off64_t bytecnt;	/* accumulates total bytes sent to media */
	intgen_t rval;

	/* dump extents until the recommended extent length is achieved
	 */
	nextoffset = *nextoffsetp;
	bytecnt = 0;
	assert( ( nextoffset & ( BBSIZE - 1 )) == 0 );

	for ( ; ; ) {
		off64_t offset;
		off64_t extsz;

		/* if we've dumped to the stop point return.
		 */
		if ( sosig && nextoffset >= stopoffset ) {
			*nextoffsetp = nextoffset;
			*bytecntp = bytecnt;
			*cmpltflgp = BOOL_TRUE;
			return 0;
		}

		/* if we've dumped the entire file, return
		 */
		if ( nextoffset >= statp->bs_size ) {
			*nextoffsetp = nextoffset;
			*bytecntp = bytecnt;
			*cmpltflgp = BOOL_TRUE;
			return 0;
		}

		/* if we've exceeded the desired per-extent group byte count,
		 * call it quits. we'll be called back for more.
		 */
		if ( bytecnt >= maxcnt ) {
			*nextoffsetp = nextoffset;
			*bytecntp = bytecnt;
			*cmpltflgp = BOOL_FALSE;
			return 0;
		}

		/* if we are not looking at a valid bmap entry,
		 * get one.
		 */
		if ( gcp->eg_nextbmapp >= gcp->eg_endbmapp ) {
			intgen_t entrycnt; /* entries in new bmap */

			assert( gcp->eg_nextbmapp == gcp->eg_endbmapp );

			/* get a new extent block
			 */
			mlog( MLOG_NITTY,
			      "calling getbmap for ino %llu\n",
			      statp->bs_ino );
			rval = fcntl( gcp->eg_fd, F_GETBMAP, gcp->eg_bmap );
			gcp->eg_gbmcnt++;
			entrycnt = gcp->eg_bmap[ 0 ].bmv_entries;
			if ( entrycnt < 0 ) { /* DBG workaround for getbmap bug */
				mlog( MLOG_DEBUG,
				      "WARNING: getbmap %d ino %lld mode 0x%08x "
				      "offset %lld ix %d "
				      "returns negative entry count\n",
				      gcp->eg_gbmcnt,
				      statp->bs_ino,
				      statp->bs_mode,
				      nextoffset,
				      gcp->eg_bmapix );
				*nextoffsetp = nextoffset;
				*bytecntp = bytecnt;
				*cmpltflgp = BOOL_TRUE;
				return 0;
			}
			if ( rval ) {
				mlog( MLOG_NORMAL,
				      "WARNING: getbmap %d ino %lld mode 0x%08x "
				      "offset %lld failed: %s\n",
				      gcp->eg_gbmcnt,
				      statp->bs_ino,
				      statp->bs_mode,
				      nextoffset,
				      strerror( errno ));
			}
			if ( rval ) {
				*nextoffsetp = nextoffset;
				*bytecntp = bytecnt;
				*cmpltflgp = BOOL_TRUE;
				return 0;
			}
			if ( entrycnt <= 0 ) {
				*nextoffsetp = nextoffset;
				*bytecntp = bytecnt;
				*cmpltflgp = BOOL_TRUE;
				return 0;
			}
			gcp->eg_nextbmapp = &gcp->eg_bmap[ 1 ];
			gcp->eg_endbmapp = &gcp->eg_bmap[ entrycnt + 1 ];
		}

		mlog( MLOG_NITTY,
		      "bmap entry %d ix %d block %lld offset %lld length %lld\n",
		      gcp->eg_nextbmapp - &gcp->eg_bmap[ 0 ],
		      gcp->eg_bmapix,
		      gcp->eg_nextbmapp->bmv_block,
		      gcp->eg_nextbmapp->bmv_offset,
		      gcp->eg_nextbmapp->bmv_length );

		/* if the next bmap entry represents a hole, go to the next
		 * one in the bmap, and rescan to check above assumptions.
		 * bump nextoffset to after the hole, if beyond current value.
		 */
		if ( gcp->eg_nextbmapp->bmv_block == -1 ) {
			off64_t tmpoffset;

			mlog( MLOG_NITTY,
			      "hole extent\n" );

			tmpoffset = ( gcp->eg_nextbmapp->bmv_offset
				      +
				      gcp->eg_nextbmapp->bmv_length )
				    *
				    ( off64_t )BBSIZE;
			if ( tmpoffset > nextoffset ) {
				nextoffset = tmpoffset;
			}
			gcp->eg_nextbmapp++;
			gcp->eg_bmapix++;
			continue;
		}

		/* if the next bmap entry has a zero size, go to the next
		 * one in the bmap, and rescan to check above assumptions.
		 */
		if ( gcp->eg_nextbmapp->bmv_length <= 0 ) {
			off64_t tmpoffset;

			mlog( MLOG_NITTY,
			      "non-positive extent\n" );

			tmpoffset = gcp->eg_nextbmapp->bmv_offset
				    *
				    ( off64_t )BBSIZE;
			if ( tmpoffset > nextoffset ) {
				nextoffset = tmpoffset;
			}
			gcp->eg_nextbmapp++;
			gcp->eg_bmapix++;
			continue;
		}

		/* extract the offset and extent size from this
		 * entry
		 */
		offset = gcp->eg_nextbmapp->bmv_offset * ( off64_t )BBSIZE;
		extsz = gcp->eg_nextbmapp->bmv_length * ( off64_t )BBSIZE;

		/* if the new bmap entry begins below the stop offset
		 * but does not contain any data above the current
		 * offset, go to the next one and rescan.
		 */
		if ( ! sosig || offset < stopoffset ) {
			if ( offset + extsz <= nextoffset ) {
				gcp->eg_nextbmapp++;
				gcp->eg_bmapix++;
				continue;
			}
		}
		
		/* if the new bmap entry begins at or above the stop offset,
		 * stop. we are done.
		 */
		if ( sosig && offset >= stopoffset ) {
			*nextoffsetp = nextoffset;
			*bytecntp = bytecnt;
			*cmpltflgp = BOOL_TRUE;
			return 0;
		}
		
		/* if the new entry begins below the range of
		 * interest, modify offset to begin at the
		 * beginning of the range of interest, and shorten
		 * extsz accordingly.
		 */
		if ( offset < nextoffset ) {
			extsz -= nextoffset - offset;
			offset = nextoffset;
		}

		/* if the resultant extent would put us over maxcnt,
		 * shorten it, and round up to the next BBSIZE.
		 */
		if ( bytecnt + sizeof( extenthdr_t ) + extsz > maxcnt ) {
			extsz = maxcnt - bytecnt - sizeof( extenthdr_t );
			extsz = ( extsz + ( off64_t )( BBSIZE - 1 ))
				&
				~( off64_t )( BBSIZE - 1 );
		}

		/* if the shortened extent is too small, return; we'll
		 * pick it up next time around. exception: if the file
		 * size is zero, indicate we are done.
		 */
		if ( extsz <= 0 ) {
			*nextoffsetp = nextoffset;
			*bytecntp = bytecnt;
			if ( statp->bs_size == 0 ) {
			    *cmpltflgp = BOOL_TRUE;
			} else {
			    *cmpltflgp = BOOL_FALSE;
			}
			return 0;
		}

		/* if the resultant extent extends beyond the end of the
		 * file, shorten the extent to the nearest BBSIZE alignment
		 * at or beyond EOF.
		 */
		if ( offset + extsz > statp->bs_size ) {
			extsz = statp->bs_size - offset;
			extsz = ( extsz + ( off64_t )( BBSIZE - 1 ))
				&
				~( off64_t )( BBSIZE - 1 );
		}

		/* I/O performance is better if we align the media write
		 * buffer to a page boundary. do this if the extent is
		 * at least a page in length. Also, necessary for real time
		 * files
		 */
		if ( isrealtime || extsz >= PGALIGNTHRESH * PGSZ ) {
			size_t cnt_to_align;
			cnt_to_align = ( * mop->mo_get_align_cnt )( mediap );
			if ( ( size_t )cnt_to_align < 2*sizeof( extenthdr_t )) {
				cnt_to_align += PGSZ;
			}

			/* account for the DATA header following the alignment
			 */
			cnt_to_align -= sizeof( extenthdr_t );

			rval = dump_extenthdr( contentp,
					       EXTENTHDR_TYPE_ALIGN,
					       0,
					       0,
					       ( off64_t )
					       ( ( size_t )cnt_to_align
						 -
						 sizeof(extenthdr_t)));
			if ( rval ) {
				*nextoffsetp = nextoffset;
				*bytecntp = bytecnt;
				*cmpltflgp = BOOL_TRUE;
				return rval;
			}
			bytecnt += sizeof( extenthdr_t );
			cnt_to_align -= sizeof( extenthdr_t );
			rval = write_pad( contentp, cnt_to_align );
			if ( rval ) {
				*nextoffsetp = nextoffset;
				*bytecntp = bytecnt;
				*cmpltflgp = BOOL_TRUE;
				return rval;
			}
			bytecnt += ( off64_t )cnt_to_align;
		}

		/* adjust the next offset
		 */
		assert( ( offset & ( off64_t )( BBSIZE - 1 )) == 0 );
		assert( ( extsz & ( off64_t )( BBSIZE - 1 )) == 0 );
		nextoffset = offset + extsz;

		/* dump the extent header
		 */
		rval = dump_extenthdr( contentp,
				       EXTENTHDR_TYPE_DATA,
				       0,
				       offset,
				       extsz );
		if ( rval ) {
			*nextoffsetp = nextoffset;
			*bytecntp = bytecnt;
			*cmpltflgp = BOOL_TRUE;
			return rval;
		}
		bytecnt += sizeof( extenthdr_t );

		/* dump the extent. if read fails to return all
		 * asked for, pad out the extent with zeros. necessary
		 * because the extent hdr is already out there!
		 */
		while ( extsz ) {
			off64_t new_off;
			intgen_t nread;
			size_t reqsz;
			size_t actualsz;
			char *bufp;

			reqsz = extsz > ( off64_t )INTGENMAX
				?
				INTGENMAX
				:
				( size_t )extsz;
			bufp = ( * mop->mo_get_write_buf )( mediap,
							    reqsz,
							    &actualsz );
			if ( actualsz > reqsz ) {
				actualsz = reqsz;
			}
			new_off = lseek64( gcp->eg_fd, offset, SEEK_SET );
			if ( new_off == ( off64_t )( -1 )) {
				mlog( MLOG_NORMAL,
				      "can't lseek ino %llu\n",
				      statp->bs_ino );
				nread = 0;
			} else {
				nread = read( gcp->eg_fd, bufp, actualsz );
			}
			if ( nread < 0 ) {
				mlog( MLOG_NORMAL,
				      "can't read ino %llu\n",
				      statp->bs_ino );
				nread = 0;
			}
			assert( ( size_t )nread <= actualsz );
			mlog( MLOG_NITTY,
			      "read ino %llu offset 0x%llx sz %d actual %d\n",
			      statp->bs_ino,
			      offset,
			      actualsz,
			      nread );
			rval = ( * mop->mo_write )( mediap,
						    bufp,
						    ( size_t )nread );
			if ( rval ) {
				*nextoffsetp = nextoffset;
				*bytecntp = bytecnt;
				*cmpltflgp = BOOL_TRUE;
				return rval;
			}
			bytecnt += ( off64_t )nread;
			extsz -= ( off64_t )nread;
			offset += ( off64_t )nread;

			/* if we get a short read, assume we are at the
			 * end of the file; pad out the remainder of the
			 * extent to match the header.
			 */
			if ( ( size_t )nread < actualsz ) {
				while ( extsz ) {
					reqsz = extsz > ( off64_t )INTGENMAX
						?
						INTGENMAX
						:
						( size_t )extsz;
					rval = write_pad( contentp,
							  ( size_t )reqsz );
					if ( rval ) {
						*nextoffsetp = nextoffset;
						*bytecntp = bytecnt;
						*cmpltflgp = BOOL_TRUE;
						return rval;
					}
					bytecnt += ( off64_t )reqsz;
					extsz -= ( off64_t )reqsz;
					offset += ( off64_t )reqsz;
				}
			}
		}

		/* made it! advance to the next extent
		 */
		gcp->eg_nextbmapp++;
		gcp->eg_bmapix++;
	}

	/* no return - infinite loop
	 */
	/*NOTREACHED*/
	assert( 0 );
	return 0;
}

static intgen_t
dump_filehdr( content_t *contentp,
	      xfs_bstat_t *statp,
	      off64_t offset,
	      intgen_t flags )
{
	media_t *mediap = contentp->c_mediap;
	media_ops_t *mop = mediap->m_opsp;
	context_t *contextp = ( context_t * )contentp->c_contextp;
	register filehdr_t *fhdrp = contextp->cc_filehdrp;
#ifdef FILEHDR_CHECKSUM
	register u_int32_t *sump = ( u_int32_t * )fhdrp;
	register u_int32_t *endp = ( u_int32_t * )( fhdrp + 1 );
	register u_int32_t sum;
#endif /* FILEHDR_CHECKSUM */
	intgen_t rval;

	( void )memset( ( void * )fhdrp, 0, sizeof( *fhdrp ));
	if ( statp ) {
		*( ( xfs_bstat_t * )&fhdrp->fh_stat ) = *statp;
	}
	fhdrp->fh_offset = offset;
	fhdrp->fh_flags = flags;

#ifdef FILEHDR_CHECKSUM
	fhdrp->fh_flags |= FILEHDR_FLAGS_CHECKSUM;
	for ( sum = 0 ; sump < endp ; sum += *sump++ ) ;
	fhdrp->fh_checksum = ~sum + 1;
#endif /* FILEHDR_CHECKSUM */

	rval = write_buf( ( char * )fhdrp,
			  sizeof( *fhdrp ),
			  ( void * )mediap,
			  ( gwbfp_t )mop->mo_get_write_buf,
			  ( wfp_t )mop->mo_write );
	
	return rval;
}

static intgen_t
dump_extenthdr( content_t *contentp,
		int32_t type,
		int32_t flags,
		off64_t offset,
		off64_t sz )
{
	media_t *mediap = contentp->c_mediap;
	media_ops_t *mop = mediap->m_opsp;
	context_t *contextp = ( context_t * )contentp->c_contextp;
	register extenthdr_t *ehdrp = contextp->cc_extenthdrp;
#ifdef EXTENTHDR_CHECKSUM
	register u_int32_t *sump = ( u_int32_t * )ehdrp;
	register u_int32_t *endp = ( u_int32_t * )( ehdrp + 1 );
	register u_int32_t sum;
#endif /* EXTENTHDR_CHECKSUM */
	intgen_t rval;

	mlog( MLOG_DEBUG,
	      "dumping extent type = %s offset = %lld size = %lld\n",
	      type == EXTENTHDR_TYPE_LAST
	      ?
	      "LAST"
	      :
	      ( type == EXTENTHDR_TYPE_DATA
		?
		"DATA"
		:
		"ALIGN" ),
	      offset,
	       sz );

	( void )memset( ( void * )ehdrp, 0, sizeof( *ehdrp ));
	ehdrp->eh_type = type;
	ehdrp->eh_flags = flags;
	ehdrp->eh_offset = offset;
	ehdrp->eh_sz = sz;

#ifdef EXTENTHDR_CHECKSUM
	ehdrp->eh_flags |= EXTENTHDR_FLAGS_CHECKSUM;
	for ( sum = 0 ; sump < endp ; sum += *sump++ ) ;
	ehdrp->eh_checksum = ~sum + 1;
#endif /* EXTENTHDR_CHECKSUM */

	rval = write_buf( ( char * )ehdrp,
			  sizeof( *ehdrp ),
			  ( void * )mediap,
			  ( gwbfp_t )mop->mo_get_write_buf,
			  ( wfp_t )mop->mo_write );

	return rval;
}

static intgen_t
dump_dirent( content_t *contentp,
	     xfs_bstat_t *statp,
	     ino64_t ino,
	     u_int32_t gen,
	     char *name,
	     size_t namelen )
{
	media_t *mediap = contentp->c_mediap;
	media_ops_t *mop = mediap->m_opsp;
	context_t *contextp = ( context_t * )contentp->c_contextp;
	direnthdr_t *dhdrp = ( direnthdr_t * )contextp->cc_mdirentbufp;
	size_t direntbufsz = contextp->cc_mdirentbufsz;
	size_t sz;
#ifdef DIRENTHDR_CHECKSUM
	register u_int32_t *sump = ( u_int32_t * )dhdrp;
	register u_int32_t *endp = ( u_int32_t * )( dhdrp + 1 );
	register u_int32_t sum;
#endif /* DIRENTHDR_CHECKSUM */
	intgen_t rval;

	sz = offsetofmember( direnthdr_t, dh_name )
	     +
	     namelen
	     +
	     1;
	sz = ( sz + DIRENTHDR_ALIGN - 1 )
	     &
	     ~( DIRENTHDR_ALIGN - 1 );

	if ( sz > direntbufsz ) {
		mlog( MLOG_NORMAL,
		      "WARNING: unable to dump "
		      "directory %llu entry %s (%llu): "
		      "name too long\n",
		      statp->bs_ino,
		      name,
		      ino );
		return 0;
	}

	assert( sz <= UINT16MAX );
	assert( sz >= DIRENTHDR_SZ );

	memset( ( void * )dhdrp, 0, sz );
	dhdrp->dh_ino = ino;
#ifdef DENTGEN
	dhdrp->dh_sz = ( u_int16_t )sz;
	dhdrp->dh_gen = ( u_int16_t )( gen & DENTGENMASK );
#else /* DENTGEN */
	dhdrp->dh_sz = ( u_int32_t )sz;
	assert( gen == 0 );
#endif /* DENTGEN */

	if ( name ) {
		strcpy( dhdrp->dh_name, name );
	}

#ifdef DIRENTHDR_CHECKSUM
	for ( sum = 0 ; sump < endp ; sum += *sump++ ) ;
	dhdrp->dh_checksum = ~sum + 1;
#endif /* DIRENTHDR_CHECKSUM */

	rval = write_buf( ( char * )dhdrp,
			  sz,
			  ( void * )mediap,
			  ( gwbfp_t )mop->mo_get_write_buf,
			  ( wfp_t )mop->mo_write );
	
	return rval;
}

static void
dump_session_inv( content_t *contentp )
{
	media_t *mediap = contentp->c_mediap;
	drive_t *drivep = mediap->m_drivep;
	size_t streamix = ( size_t )drivep->d_index;
	media_ops_t *mop = mediap->m_opsp;
	media_hdr_t *mwhdrp = mediap->m_writehdrp;
	content_hdr_t *cwhdrp = contentp->c_writehdrp;
	content_inode_hdr_t *scwhdrp= ( content_inode_hdr_t * )
				      ( void * )
				      cwhdrp->ch_specific;
	context_t *contextp = ( context_t * )contentp->c_contextp;
	scontext_t *scontextp = contextp->cc_scontextp;
	inv_sestoken_t inv_st = scontextp->sc_inv_sestoken;
	inv_stmtoken_t inv_stmt;
	char *inv_sbufp;
	size_t inv_sbufsz;
	off64_t ncommitted;
	bool_t ok;
	bool_t done;

	/* if the inventory session token is null, skip
	 */
	if ( inv_st == INV_TOKEN_NULL ) {
		return;
	}

	mlog( MLOG_VERBOSE,
	      "dumping session inventory\n" );

	/* get a buffer from the inventory manager
	 */
	inv_sbufp = 0;
	inv_sbufsz = 0;
	ok = inv_get_sessioninfo( inv_st, ( void * )&inv_sbufp, &inv_sbufsz );
	if ( ! ok ) {
		mlog( MLOG_NORMAL,
		      "WARNING: unable to get session inventory to dump\n" );
		return;
	}
	assert( inv_sbufp );

	/* modify the write header to indicate the media file type.
	 */
	scwhdrp->cih_mediafiletype = CIH_MEDIAFILETYPE_INVENTORY;

	/* loop attempting to write a complete media file,
	 * until we are successful or until the media layer
	 * tells us to give up.
	 */
	for ( done = BOOL_FALSE ; ! done ; ) {
		uuid_t mediaid;
		char medialabel[ GLOBAL_HDR_STRING_SZ ];
		intgen_t mediafileix;

		bool_t partial;
		bool_t complete;
		intgen_t rval;

		mlog( MLOG_VERBOSE,
		      "beginning inventory media file\n" );

		partial = BOOL_FALSE;
		complete = BOOL_FALSE;
		rval = ( * mop->mo_begin_write )( mediap );
		if ( rval ) {
			if ( rval == MEDIA_ERROR_ABORT
			     ||
			     rval == MEDIA_ERROR_CORE
			     ||
			     rval == MEDIA_ERROR_TIMEOUT ) {
				
				done = BOOL_TRUE;
			}
		}

		if ( ! rval ) {
			mlog( MLOG_VERBOSE,
			      "media file %u (media %u, file %u)\n",
			      mwhdrp->mh_dumpfileix,
			      mwhdrp->mh_mediaix,
			      mwhdrp->mh_mediafileix );

			mediaid = mwhdrp->mh_mediaid;
			strcpy( medialabel, mwhdrp->mh_medialabel );
			mediafileix = ( intgen_t )mwhdrp->mh_mediafileix;

			rval = write_buf( inv_sbufp,
					  inv_sbufsz,
					  ( void * )mediap,
					  ( gwbfp_t )mop->mo_get_write_buf,
					  ( wfp_t )mop->mo_write );
			if ( rval ) {
				if ( rval == MEDIA_ERROR_ABORT
				     ||
				     rval == MEDIA_ERROR_CORE ) {
					
					done = BOOL_TRUE;
				} else {
					partial = BOOL_TRUE;
				}
			} else {
				complete = BOOL_TRUE;
				done = BOOL_TRUE;
			}
		}

		mlog( MLOG_VERBOSE,
		      "ending inventory media file\n" );

		rval = ( * mop->mo_end_write )( mediap, &ncommitted );
		if ( rval ) {
			if ( rval == MEDIA_ERROR_CORE
			     ||
			     rval == MEDIA_ERROR_ABORT ) {
				done = BOOL_TRUE;
			} else {
				partial = BOOL_TRUE;
			}
		} else {
			complete = BOOL_TRUE;
		}

		mlog( MLOG_VERBOSE,
		      "inventory media file size %lld bytes\n",
		      ncommitted );

		if ( scontextp->sc_inv_stmtokenp ) {
			inv_stmt = scontextp->sc_inv_stmtokenp[ streamix ];
		} else {
			inv_stmt = INV_TOKEN_NULL;
		}

		if ( inv_stmt != INV_TOKEN_NULL && ( partial || complete )) {
			mlog( MLOG_DEBUG,
			      "giving inventory "
			      "session dump media file\n" );
			ok = inv_put_mediafile( inv_stmt,
						&mediaid,
						medialabel,
						( u_intgen_t )mediafileix,
						(ino64_t )0,
						( off64_t )0,
						(ino64_t )0,
						( off64_t )0,
						ncommitted,
						complete,
						BOOL_TRUE );
			if ( ! ok ) {
				mlog( MLOG_NORMAL,
				      "inventory session media file put failed\n" );
			}
		}
	}
}

static intgen_t
write_pad( content_t *contentp, size_t sz )
{
	media_t *mediap = contentp->c_mediap;
	intgen_t rval;

	rval = write_buf( 0,
			  sz,
			  ( void * )mediap,
			  ( gwbfp_t )mediap->m_opsp->mo_get_write_buf,
			  ( wfp_t )mediap->m_opsp->mo_write );

	return rval;
}

static void
inv_cleanup( void *arg1, void *arg2 )
{
	content_strategy_t *csp = ( content_strategy_t * )arg1;
	scontext_t *scontextp = ( scontext_t * )csp->cs_contextp;
	bool_t ok;

	if ( scontextp->sc_inv_stmtokenp ) {
		size_t streamcnt = csp->cs_contentcnt;
		size_t streamix;
		inv_stmtoken_t *inv_stmtp;
		for ( streamix = 0, inv_stmtp = scontextp->sc_inv_stmtokenp
		      ;
		      streamix < streamcnt
		      ;
		      streamix++, inv_stmtp++ ) {
			bool_t interrupted;
			interrupted = scontextp->sc_inv_interruptedp[streamix];
			if ( *inv_stmtp == INV_TOKEN_NULL ) {
				continue;
			}
			mlog( MLOG_DEBUG,
			      "closing inventory stream %d%s\n",
			      streamix,
			      interrupted ? ": interrupted" : "" );
			ok = inv_stream_close( *inv_stmtp, interrupted );
			assert( ok );
		}
	}

	if ( scontextp->sc_inv_sestoken != INV_TOKEN_NULL ) {
		ok = inv_writesession_close( scontextp->sc_inv_sestoken );
		assert( ok );
	}

	if ( scontextp->sc_inv_idbtoken != INV_TOKEN_NULL ) {
		ok = inv_close( scontextp->sc_inv_idbtoken );
		assert( ok );
	}
}
