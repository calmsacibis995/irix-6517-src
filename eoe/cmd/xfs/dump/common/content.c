#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/common/RCS/content.c,v 1.18 1995/08/22 14:49:43 tap Exp $"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/uuid.h>
#include <time.h>
#include <sys/fs/xfs_itable.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "path.h"
#include "types.h"
#include "jdm.h"
#include "util.h"
#include "mlog.h"
#include "dlog.h"
#include "getopt.h"
#include "stream.h"
#include "fs.h"
#include "global.h"
#include "drive.h"
#include "media.h"
#include "content.h"

#define FS_DEFAULT	"xfs"
#define OVERWRITE_PROMPT_TIMEOUT	60 /* seconds */

/* content.c - selects and initializes a content strategy
 */


/* structure definitions used locally ****************************************/


/* declarations of externally defined global symbols *************************/

extern void usage( void );
extern char *homedir;

/* declare all content strategies here
 */
extern content_strategy_t content_strategy_inode;


/* forward declarations of locally defined static functions ******************/

static content_t *content_alloc( media_t *,
				 char *
#ifdef DUMP
				 ,char *,
				 char *,
				 uuid_t *
#endif /* DUMP */
					);

/* definition of locally defined global variables ****************************/


/* definition of locally defined static variables *****************************/

/* content strategy array - ordered by precedence
 */
static content_strategy_t *strategyp[] = {
	&content_strategy_inode,
};


/* definition of locally defined global functions ****************************/

/* content_create - select and initialize a content strategy, 
 * and create and initialize content managers for each stream.
 */
content_strategy_t *
content_create( int argc, char *argv[ ], media_strategy_t *msp )
{
	int c;
#ifdef DUMP
	char *srcname;
#endif /* DUMP */
#ifdef DUMP
	char mntpnt[ GLOBAL_HDR_STRING_SZ ];
	char fsdevice[ GLOBAL_HDR_STRING_SZ ];
	char fstype[ CONTENT_HDR_FSTYPE_SZ ];
	uuid_t fsid;
#endif /* DUMP */
#ifdef RESTORE
	char *mntpnt;
#endif /* RESTORE */
	size_t contentix;
	size_t contentcnt;
	content_t **contentpp;
	intgen_t id;
	content_strategy_t **spp = strategyp;
	content_strategy_t **epp = strategyp + sizeof( strategyp )
					       /
					       sizeof( strategyp[ 0 ] );
	content_strategy_t *chosen_sp;
#ifdef RESTORE
	bool_t toc;
	bool_t cumulative;
	bool_t resume;
	bool_t existing;
	bool_t newer;
	bool_t prompt;
	bool_t changed;
	time_t newertime;
	stat_t statbuf;
#endif /* RESTORE */
bool_t ok;

	/* sanity check asserts
	 */
	assert( sizeof( content_hdr_t ) == CONTENT_HDR_SZ );

	/* scan the command line for a dump label and source
	 * file system name / destination directory
	 */
	optind = 1;
	opterr = 0;
#ifdef RESTORE
	toc = BOOL_FALSE;
	cumulative = BOOL_FALSE;
	resume = BOOL_FALSE;
	existing = BOOL_FALSE;
	newer = BOOL_FALSE;
	prompt = BOOL_FALSE;
	changed = BOOL_FALSE;
#endif /* RESTORE */
	while ( ( c = getopt( argc, argv, GETOPT_CMDSTRING )) != EOF ) {
		switch ( c ) {
#ifdef RESTORE
		case GETOPT_TOC:
			toc = BOOL_TRUE;
			break;
		case GETOPT_CUMULATIVE:
			cumulative = BOOL_TRUE;
			break;
		case GETOPT_RESUME:
			resume = BOOL_TRUE;
			break;
		case GETOPT_EXISTING:
			existing = BOOL_TRUE;
			break;
		case GETOPT_NEWER:
			if ( ! optarg || optarg[ 0 ] == '-' ) {
				mlog( MLOG_NORMAL,
				      "-%c argument missing\n",
				      optopt );
				usage( );
				return 0;
			}
			if ( stat( optarg, &statbuf )) {
				mlog( MLOG_NORMAL,
				      "unable to get status of -%c argument %s:"
				      " %s\n",
				      optopt,
				      optarg,
				      strerror( errno ));
				return 0;
			}
			newer = BOOL_TRUE;
			newertime = statbuf.st_mtime;
			break;
#ifdef NOTYET
		case GETOPT_PROMPT:
			if ( dlog_allowed( )) {
				prompt = BOOL_TRUE;
			}
			break;
#endif /* NOTYET */
		case GETOPT_CHANGED:
			changed = BOOL_TRUE;
			break;
#endif /* RESTORE */
		}
	}

#ifdef RESTORE
	if ( cumulative && toc ) {
		mlog( MLOG_NORMAL,
		      "-%c and -%c option cannot be used together\n",
		      GETOPT_TOC,
		      GETOPT_CUMULATIVE );
		usage( );
		return 0;
	}
	if ( resume && toc ) {
		mlog( MLOG_NORMAL,
		      "-%c and -%c option cannot be used together\n",
		      GETOPT_TOC,
		      GETOPT_RESUME );
		usage( );
		return 0;
	}
#endif /* RESTORE */

	/* the user may specify stdout as the destination / stdin as
	 * the source, by a single dash ('-') with no option letter.
	 * This must appear between all lettered arguments and the
	 * source file system / destination directory  pathname.
	 */
	if ( optind < argc && ! strcmp( argv[ optind ], "-" )) {
		optind++;
	}

	/* DUMP: the last argument must be either the mount point or the
	 * device pathname of the file system to be dumped.
	 * RESTORE: the last argument must be a destination directory
	 */
#ifdef DUMP
	if ( optind >= argc ) {
		mlog( MLOG_NORMAL,
		      "source file system "
		      "not specified\n" );
		usage( );
		return 0;
	}
	srcname = argv[ optind ];
#endif /* DUMP */

#ifdef RESTORE
	if ( ! toc ) {
		stat_t statbuf;
		intgen_t rval;

		if ( optind >= argc ) {
			mlog( MLOG_NORMAL,
			      "destination directory "
			      "not specified\n" );
			usage( );
			return 0;
		}
		mntpnt = path_reltoabs( argv[ optind ], homedir );
		if ( ! mntpnt ) {
			mlog( MLOG_NORMAL,
			      "destination directory %s "
			      "invalid pathname\n" );
			usage( );
			return 0;
		}
		mlog( MLOG_DEBUG,
		      "restore destination path converted from %s to %s\n",
		      argv[ optind ],
		      mntpnt );
		if ( mntpnt[ 0 ] != '/' ) {
			mlog( MLOG_NORMAL,
			      "destination directory %s "
			      "must be an absolute pathname\n",
			      mntpnt );
			usage( );
			return 0;
		}
		rval = lstat( mntpnt, &statbuf );
		if ( rval ) {
			mlog( MLOG_NORMAL,
			      "cannot stat destination directory %s: %s\n",
			      mntpnt,
			      strerror( errno ));
			usage( );
			return 0;
		}
		if ( ( statbuf.st_mode & S_IFMT ) != S_IFDIR ) {
			mlog( MLOG_NORMAL,
			      "specified destination %s is not a directory\n",
			      mntpnt );
			usage( );
			return 0;
		}
	} else {
		mntpnt = ".";
	}
#endif /* RESTORE */

#ifdef DUMP
	/* call a magic function to figure out if the last argument is
	 * a mount point or a device pathname, and retrieve the file
	 * system type, full pathname of the character special device
	 * containing the file system, the latest mount point, and the file
	 * system ID (uuid). returns BOOL_FALSE if the last
	 * argument doesn't look like a file system.
	 */
	if ( ! fs_info( fstype,
			sizeof( fstype ),
			FS_DEFAULT,
			fsdevice,
			sizeof( fsdevice ),
			mntpnt,
			sizeof( mntpnt ),
			&fsid,
			srcname )) {

		mlog( MLOG_NORMAL,
		      "%s does not identify a file system\n",
		      srcname );
		usage( );
		return 0;
	}

	/* verify that the file system is mounted. This must be enhanced
	 * to mount an unmounted file system on a temporary mount point,
	 * if it is not currently mounted.
	 */
	if ( ! fs_mounted( fstype, fsdevice, mntpnt, &fsid )) {
		mlog( MLOG_NORMAL,
		      "%s must be mounted to be dumped\n",
		      srcname );
		return 0;
	}

#endif /* DUMP */

	/* create a content_t array and a content_ts for each media stream.
	 * Initialize each content_t's writehdr and generic portions. these
	 * will be lended to each content strategy during the strategy
	 * match phase, and given to the winning strategy.
	 *
	 * RESTORE: NOTE that the destination is saved in the writehdr mntpnt
	 * field; a slight misnomer, since the restore destination does not
	 * need to be the root of a file system.
	 */
	contentcnt = msp->ms_mediacnt;
	contentpp = ( content_t ** )calloc( contentcnt,
					    sizeof( content_t * ));
	assert( contentpp );
	for ( contentix = 0 ; contentix < contentcnt ; contentix++ ) {
		contentpp[ contentix ] =
				    content_alloc( msp->ms_mediap[ contentix ],
						   mntpnt
#ifdef DUMP
						   , fsdevice,
						   fstype,
						   &fsid
#endif /* DUMP */
							 );
	}

	/* choose the first strategy which claims appropriateness.
	 * if none match, return null. Also, initialize the strategy ID
	 * and pointer to the media strategy. the ID is simply the index
	 * of the strategy in the strategy array. it is placed in the
	 * content_strategy_t as well as the write headers.
	 */
	chosen_sp = 0;
	for ( id = 0 ; spp < epp ; spp++, id++ ) {
		(*spp)->cs_id = id;
		if ( ! chosen_sp ) {
			/* lend the content_t array to the strategy
			 */
			(*spp)->cs_contentp = contentpp;
			(*spp)->cs_msp = msp;
			(*spp)->cs_contentcnt = contentcnt;
			for ( contentix = 0 ; contentix < contentcnt ;
								contentix++ ) {
				content_t *contentp = contentpp[ contentix ];
				contentp->c_strategyp = *spp;
				contentp->c_writehdrp->ch_strategyid = id;
			}
			if ( ( * (*spp)->cs_match )( argc, argv, msp )) {
				chosen_sp = *spp;
			}
		}
	}
#ifdef DUMP
	if ( ! chosen_sp ) {
		mlog( MLOG_NORMAL,
		      "do not know how to dump selected "
		      "source file system's (%s) type (%s)\n",
		      mntpnt,
		      fstype );
#endif /* DUMP */
#ifdef RESTORE
	if ( ! chosen_sp ) {
		mlog( MLOG_NORMAL,
		      "do not know how to restore into selected "
		      "destination file system (%s)\n",
		      mntpnt );
#endif /* RESTORE */
		usage( );
		return 0;
	}

	/* give the content_t array to the chosen strategy
	 */
	for ( contentix = 0 ; contentix < contentcnt ; contentix++ ) {
		content_t *contentp = contentpp[ contentix ];
		contentp->c_strategyp = chosen_sp;
		contentp->c_writehdrp->ch_strategyid = chosen_sp->cs_id;
	}
	
#ifdef RESTORE
	/* set global content strategy flags
	 */
	if ( toc ) {
		chosen_sp->cs_flags |= CONTENT_STRATEGY_FLAGS_TOC;
	}
	if ( cumulative ) {
		chosen_sp->cs_flags |= CONTENT_STRATEGY_FLAGS_CUMULATIVE;
	}
	if ( resume ) {
		chosen_sp->cs_flags |= CONTENT_STRATEGY_FLAGS_RESUME;
	}
	if ( existing ) {
		chosen_sp->cs_flags |= CONTENT_STRATEGY_FLAGS_EXISTING;
	}
	if ( newer ) {
		chosen_sp->cs_flags |= CONTENT_STRATEGY_FLAGS_NEWER;
		chosen_sp->cs_newertime = newertime;
	}
	if ( prompt ) {
		chosen_sp->cs_flags |= CONTENT_STRATEGY_FLAGS_PROMPT;
	}
	if ( changed ) {
		chosen_sp->cs_flags |= CONTENT_STRATEGY_FLAGS_CHANGED;
	}
#endif /* RESTORE */

	/* call strategy-specific create operator.
	 */
	ok = ( * chosen_sp->cs_create )( chosen_sp, argc, argv );
	if ( !ok ) {
		return 0;
	}

	/* return the selected strategy
	 */
	return chosen_sp;
}

bool_t
content_init( content_strategy_t *csp, int argc, char *argv[] )
{
	bool_t ok;

	ok = ( * csp->cs_init )( csp, argc, argv );

	return ok;
}

void
content_complete( content_strategy_t *csp )
{
	( * csp->cs_complete )( csp );
}

#ifdef RESTORE
bool_t
content_overwrite_ok( content_strategy_t *csp,
		      char *path,
		      int32_t ctime,
		      int32_t mtime,
		      char **reasonstrp )
{
	register intgen_t flags = csp->cs_flags;
	stat_t statbuf;

	/* if no overwrite inhibit directives, allow
	 
	if ( ! ( flags
		 &
		 ( CONTENT_STRATEGY_FLAGS_EXISTING
		   |
		   CONTENT_STRATEGY_FLAGS_NEWER
		   |
		   CONTENT_STRATEGY_FLAGS_PROMPT
		   |
		   CONTENT_STRATEGY_FLAGS_CHANGED ))) {
		*reasonstrp = 0;
		return BOOL_TRUE;
	}

	 * if file doesn't exist, allow
	 */
	if ( lstat( path, &statbuf )) {
		*reasonstrp = 0;
		return BOOL_TRUE;
	}

	/* if overwrites absolutely inhibited, disallow
	 */
	if ( flags & CONTENT_STRATEGY_FLAGS_EXISTING ) {
		*reasonstrp = "overwrites inhibited";
		return BOOL_FALSE;
	}

	/* if newer time specified, compare 
	 */
	if ( flags & CONTENT_STRATEGY_FLAGS_NEWER ) {
		if ( ( time_t )ctime < csp->cs_newertime ) {
			*reasonstrp = "too old";
			return BOOL_FALSE;
		}
	}

	/* don't overwrite changed files
	 */
	if ( flags & CONTENT_STRATEGY_FLAGS_CHANGED ) {
		if ( statbuf.st_ctime >= ( time_t )ctime ) {
			*reasonstrp = "existing version is newer";
			return BOOL_FALSE;
		}
	}

	/* prompt to allow overwrite
	 */
	if ( flags & CONTENT_STRATEGY_FLAGS_PROMPT ) {
		char buf[ MAXPATHLEN + 100 ];
		sprintf( buf, "overwrite %s?", path );
		if ( dlog_yesno( buf,
				 "overwriting",
				 "skipping",
				 "skipping",
				 BOOL_FALSE,
				 OVERWRITE_PROMPT_TIMEOUT ) != BOOL_TRUE ) {
			*reasonstrp = "overwrite declined";
			return BOOL_FALSE;
		}
	}

	*reasonstrp = 0;
	return BOOL_TRUE;
}
#endif /* RESTORE */


/* definition of locally defined static functions ****************************/

/* content_alloc - allocate and initialize the generic portions of a content 
 * descriptor and read and write content media headers
 */
static content_t *
content_alloc( media_t *mediap,
	       char *mntpnt
#ifdef DUMP
	       ,char *fsdevice,
	       char *fstype,
	       uuid_t *fsidp
#endif /* DUMP */
			    )
{
	content_t *contentp;
	global_hdr_t *grhdrp;
	global_hdr_t *gwhdrp;
	content_hdr_t *crhdrp;
	content_hdr_t *cwhdrp;
	size_t crhdrsz;
	size_t cwhdrsz;

	contentp = ( content_t * )calloc( 1, sizeof( content_t ));
	assert( contentp );

	grhdrp = 0;
	gwhdrp = 0;
	crhdrp = 0;
	cwhdrp = 0;
	media_get_upper_hdrs( mediap,
			      &grhdrp,
			      ( char ** )&crhdrp,
			      &crhdrsz,
			      &gwhdrp,
			      ( char ** )&cwhdrp,
			      &cwhdrsz );
	assert( grhdrp );
	assert( gwhdrp );
	assert( crhdrp );
	assert( cwhdrp );
	assert( crhdrsz == CONTENT_HDR_SZ );
	assert( cwhdrsz == CONTENT_HDR_SZ );

	contentp->c_greadhdrp = grhdrp;
	contentp->c_gwritehdrp = gwhdrp;
	contentp->c_readhdrp = crhdrp;
	contentp->c_writehdrp = cwhdrp;
	contentp->c_mediap = mediap;

#ifdef RESTORE
	assert( strlen( mntpnt ) < sizeof( cwhdrp->ch_mntpnt ));
#endif /* RESTORE */
	( void )strncpyterm( cwhdrp->ch_mntpnt,
			     mntpnt,
			     sizeof( cwhdrp->ch_mntpnt ));
#ifdef DUMP
	( void )strncpyterm( cwhdrp->ch_fsdevice,
			     fsdevice,
			     sizeof( cwhdrp->ch_fsdevice ));
	( void )strncpyterm( cwhdrp->ch_fstype,
			     fstype,
			     sizeof( cwhdrp->ch_fstype ));
	cwhdrp->ch_fsid = *fsidp;
#endif /* DUMP */

	return contentp;
}
