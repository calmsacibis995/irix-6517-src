#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/dump/RCS/inomap.c,v 1.17 1998/05/20 15:16:28 cwf Exp $"

#include <sys/types.h>
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
#include <string.h>
#include <assert.h>

#include "types.h"
#include "jdm.h"
#include "util.h"
#include "mlog.h"
#include "global.h"
#include "drive.h"
#include "media.h"
#include "content.h"
#include "content_inode.h"
#include "inomap.h"
#ifdef EXTATTR
#include "hsmapi.h"
#endif /* EXTATTR */

#ifdef BANYAN
#define MACROBITS
#endif /* BANYAN */

/* structure definitions used locally ****************************************/

#define BSTATBUFLEN	pgsz
	/* length (in bstat_t's) of buf passed to bigstat_iter
	 */

#define GETDENTBUFSZ	pgsz
	/* size (in bytes) of buf passed to diriter (when not recursive)
	 */

/* declarations of externally defined global symbols *************************/

extern bool_t preemptchk( int );
extern size_t pgsz;
#ifdef EXTATTR
extern hsm_fs_ctxt_t *hsm_fs_ctxtp;
#endif /* EXTATTR */


/* forward declarations of locally defined static functions ******************/

/* inomap construction callbacks
 */
static void cb_context( bool_t last,
			time_t,
			bool_t,
			time_t,
			size_t,
			drange_t *,
			size_t,
			startpt_t *,
			size_t,
			char *,
			size_t );
static void cb_postmortem( void );
static intgen_t cb_add( void *, jdm_fshandle_t *, intgen_t, xfs_bstat_t * );
static bool_t cb_inoinresumerange( ino64_t );
static bool_t cb_inoresumed( ino64_t );
static intgen_t cb_prune( void *, jdm_fshandle_t *, intgen_t,  xfs_bstat_t * );
static intgen_t cb_count_in_subtreelist( void *,
					 jdm_fshandle_t *,
					 intgen_t,
					 xfs_bstat_t *,
					 char * );
static intgen_t cb_count_needed_children( void *,
					  jdm_fshandle_t *,
					  intgen_t,
					  xfs_bstat_t *,
					  char * );
static void gdrecursearray_init( void );
static void gdrecursearray_free( void );
static intgen_t cb_cond_del( void *,
			     jdm_fshandle_t *,
			     intgen_t,
			     xfs_bstat_t *,
			     char * );
static intgen_t cb_del( void *,
			jdm_fshandle_t *,
			intgen_t,
			xfs_bstat_t *,
			char * );
static void cb_accuminit_sz( void );
static void cb_accuminit_ctx( void * );
static intgen_t cb_accumulate( void *,
			       jdm_fshandle_t *,
			       intgen_t,
			       xfs_bstat_t * );
static void cb_spinit( void );
static intgen_t cb_startpt( void *,
			    jdm_fshandle_t *,
			    intgen_t,
			    xfs_bstat_t * );
static off64_t quantity2offset( jdm_fshandle_t *, xfs_bstat_t *, off64_t );
static off64_t estimate_dump_space( xfs_bstat_t * );

/* inomap primitives
 */
static void map_init( void );
static void map_add( ino64_t ino, intgen_t );
static intgen_t map_getset( ino64_t, intgen_t, bool_t );
static intgen_t map_get( ino64_t );
static intgen_t map_set( ino64_t ino, intgen_t );

/* subtree abstraction
 */
static void subtreelist_add( ino64_t );
static intgen_t subtreelist_parse_cb( void *,
				      jdm_fshandle_t *,
				      intgen_t fsfd,
				      xfs_bstat_t *,
				      char * );
static bool_t subtreelist_parse( jdm_fshandle_t *,
				 intgen_t,
				 xfs_bstat_t *,
				 char *[],
				 ix_t );
static void subtreelist_free( void );
static bool_t subtreelist_contains( ino64_t );

/* multiply linked (mln) abstraction
 */
static void mln_init( void );
static nlink_t mln_register( ino64_t, nlink_t );
static void mln_free( void );


/* definition of locally defined global variables ****************************/


/* definition of locally defined static variables *****************************/

static ix_t *inomap_statphasep;
static ix_t *inomap_statpassp;
static size64_t *inomap_statdonep;

/* definition of locally defined global functions ****************************/

/* inomap_build - build an in-core image of the inode map for the
 * specified file system. identify startpoints in the non-dir inodes,
 * such that the total dump media required is divided into startptcnt segments.
 */
/* ARGSUSED */
bool_t
inomap_build( jdm_fshandle_t *fshandlep,
	      intgen_t fsfd,
	      xfs_bstat_t *rootstatp,
	      bool_t last,
	      time_t lasttime,
	      bool_t resume,
	      time_t resumetime,
	      size_t resumerangecnt,
	      drange_t *resumerangep,
	      char *subtreebuf[],
	      ix_t subtreecnt,
	      startpt_t *startptp,
	      size_t startptcnt,
	      ix_t *statphasep,
	      ix_t *statpassp,
	      size64_t statcnt,
	      size64_t *statdonep )
{
	xfs_bstat_t *bstatbufp;
	size_t bstatbuflen;
	char *getdentbufp;
	bool_t pruneneeded;
	bool_t rescanneeded;
	ix_t scancnt;
	intgen_t stat;
	void *inomap_state_contextp;
	intgen_t rval;

	/* copy stat ptrs
	 */
	inomap_statphasep = statphasep;
	inomap_statpassp = statpassp;
	inomap_statdonep = statdonep;

	/* allocate a bulkstat buf
	 */
	bstatbuflen = BSTATBUFLEN;
	bstatbufp = ( xfs_bstat_t * )memalign( pgsz,
					       bstatbuflen
					       *
					       sizeof( xfs_bstat_t ));
	assert( bstatbufp );

	/* allocate a getdent buf
	 */
	getdentbufp = ( char * )memalign( pgsz, GETDENTBUFSZ );
	assert( getdentbufp );

	/* parse the subtree list, if any subtrees specified.
	 * this will be used during the tree pruning phase.
	 */
	if ( subtreecnt ) {
		mlog( MLOG_VERBOSE | MLOG_INOMAP,
		      "ino map phase 1: "
		      "parsing subtree selections\n" );
		if ( ! subtreelist_parse( fshandlep,
					  fsfd,
					  rootstatp,
					  subtreebuf,
					  subtreecnt )) {
			free( ( void * )bstatbufp );
			free( ( void * )getdentbufp );
			return BOOL_FALSE;
		}
	} else {
		mlog( MLOG_VERBOSE | MLOG_INOMAP,
		      "ino map phase 1: "
		      "skipping (no subtrees specified)\n" );
	}
	
	if ( preemptchk( PREEMPT_FULL )) {
		free( ( void * )bstatbufp );
		free( ( void * )getdentbufp );
		return BOOL_FALSE;
	}

	/* initialize the callback context
	 */
	cb_context( last,
		    lasttime,
		    resume,
		    resumetime,
		    resumerangecnt,
		    resumerangep,
		    subtreecnt,
		    startptp,
		    startptcnt,
		    getdentbufp,
		    GETDENTBUFSZ );

	/* construct the ino map, based on the last dump time, resumed
	 * dump info, and subtree list. place all unchanged directories
	 * in the "needed for children" state (MAP_DIR_SUPPRT). these will be
	 * dumped even though they have not changed. a later pass will move
	 * some of these to "not dumped", such that only those necessary
	 * to represent the minimal tree containing only changes will remain.
	 * the bigstat iterator is used here, along with a inomap constructor
	 * callback. set a flag if any ino not put in a dump state. This will
	 * be used to decide if any pruning can be done.
	 */
	mlog( MLOG_VERBOSE | MLOG_INOMAP,
	      "ino map phase 2: "
	      "constructing initial dump list\n" );
	*inomap_statdonep = 0;
	*inomap_statphasep = 2;
	pruneneeded = BOOL_FALSE;
	stat = 0;
	cb_accuminit_sz( );
	rval = bigstat_iter( fshandlep,
			     fsfd,
			     BIGSTAT_ITER_ALL,
			     ( ino64_t )0,
			     cb_add,
			     ( void * )&pruneneeded,
			     &stat,
			     preemptchk,
			     bstatbufp,
			     bstatbuflen );
	*inomap_statphasep = 0;
	if ( rval ) {
		free( ( void * )bstatbufp );
		free( ( void * )getdentbufp );
		return BOOL_FALSE;
	}

	if ( preemptchk( PREEMPT_FULL )) {
		free( ( void * )bstatbufp );
		free( ( void * )getdentbufp );
		return BOOL_FALSE;
	}

	/* prune subtrees not called for in the subtree list, and
	 * directories unchanged since the last dump and containing
	 * no children needing dumping. Each time the latter pruning
	 * occurs at least once, repeat.
	 */
	if ( pruneneeded || subtreecnt > 0 ) {
		/* setup the list of multiply-linked pruned nodes
		 */
		mln_init( );
		gdrecursearray_init( );

		scancnt = 0;
		rescanneeded = BOOL_FALSE; /* not needed, keeps lint happy */
		do {
			scancnt++;
			if ( scancnt <= 1 ) {
				mlog( MLOG_VERBOSE | MLOG_INOMAP,
				      "ino map phase 3: "
				      "pruning unneeded subtrees\n" );
			} else {
				mlog( MLOG_VERBOSE | MLOG_INOMAP,
				      "ino map phase 3: "
				      "pass %d\n",
				      scancnt );
			}
			*inomap_statdonep = 0;
			*inomap_statpassp = scancnt;
			*inomap_statphasep = 3;
			rescanneeded = BOOL_FALSE;
			stat = 0;
			rval = bigstat_iter( fshandlep,
					     fsfd,
					     BIGSTAT_ITER_DIR,
					     ( ino64_t )0,
					     cb_prune,
					     ( void * )&rescanneeded,
					     &stat,
					     preemptchk,
					     bstatbufp,
					     bstatbuflen );
			*inomap_statphasep = 0;
			*inomap_statpassp = 0;
			if ( rval ) {
				gdrecursearray_free( );
				free( ( void * )bstatbufp );
				free( ( void * )getdentbufp );
				return BOOL_FALSE;
			}

			if ( preemptchk( PREEMPT_FULL )) {
				gdrecursearray_free( );
				free( ( void * )bstatbufp );
				free( ( void * )getdentbufp );
				return BOOL_FALSE;
			}

		} while ( rescanneeded );

		gdrecursearray_free( );
		mln_free( );

		cb_postmortem( );

	} else {
		mlog( MLOG_VERBOSE | MLOG_INOMAP,
		      "ino map phase 3: "
		      "skipping (no pruning necessary)\n" );
	}

	/* free the subtree list memory
	 */
	if ( subtreecnt ) {
		subtreelist_free( );
	}

	/* allocate a map iterator to allow us to skip inos that have
	 * been pruned from the map.
	 */
	inomap_state_contextp = inomap_state_getcontext( );

	cb_accuminit_ctx( inomap_state_contextp );

	/* calculate the total dump space needed for non-directories.
	 * no needed if no pruning was done, since already done during
	 * phase 2.
	 */
	if ( pruneneeded || subtreecnt > 0 ) {
		cb_accuminit_sz( );

		mlog( MLOG_VERBOSE | MLOG_INOMAP,
		      "ino map phase 4: "
		      "estimating dump size\n" );
		*inomap_statdonep = 0;
		*inomap_statphasep = 4;
		stat = 0;
		rval = bigstat_iter( fshandlep,
				     fsfd,
				     BIGSTAT_ITER_NONDIR,
				     ( ino64_t )0,
				     cb_accumulate,
				     0,
				     &stat,
				     preemptchk,
				     bstatbufp,
				     bstatbuflen );
		*inomap_statphasep = 0;
		if ( rval ) {
			inomap_state_freecontext( inomap_state_contextp );
			free( ( void * )bstatbufp );
			free( ( void * )getdentbufp );
			return BOOL_FALSE;
		}

		if ( preemptchk( PREEMPT_FULL )) {
			inomap_state_freecontext( inomap_state_contextp );
			free( ( void * )bstatbufp );
			free( ( void * )getdentbufp );
			return BOOL_FALSE;
		}

		inomap_state_postaccum( inomap_state_contextp );
	} else {
		mlog( MLOG_VERBOSE | MLOG_INOMAP,
		      "ino map phase 4: "
		      "skipping (size estimated in phase 2)\n" );
	}

	/* initialize the callback context for startpoint calculation
	 */
	cb_spinit( );

	/* identify dump stream startpoints
	 */
	if ( startptcnt > 1 ) {
		mlog( MLOG_VERBOSE | MLOG_INOMAP,
		      "ino map phase 5: "
		      "identifying stream starting points\n" );
	} else {
		mlog( MLOG_VERBOSE | MLOG_INOMAP,
		      "ino map phase 5: "
		      "skipping (only one dump stream)\n" );
	}
	stat = 0;
	*inomap_statdonep = 0;
	*inomap_statphasep = 5;
	rval = bigstat_iter( fshandlep,
			     fsfd,
			     BIGSTAT_ITER_NONDIR,
			     ( ino64_t )0,
			     cb_startpt,
			     0,
			     &stat,
			     preemptchk,
			     bstatbufp,
			     bstatbuflen );
	*inomap_statphasep = 0;
	
	inomap_state_postaccum( inomap_state_contextp );

	inomap_state_freecontext( inomap_state_contextp );

	if ( rval ) {
		free( ( void * )bstatbufp );
		free( ( void * )getdentbufp );
		return BOOL_FALSE;
	}

	if ( startptcnt > 1 ) {
		ix_t startptix;
		for ( startptix = 0 ; startptix < startptcnt ; startptix++ ) {
			startpt_t *p;
			startpt_t *ep;

			p = &startptp[ startptix ];
			if ( startptix == startptcnt - 1 ) {
				ep = 0;
			} else {
				ep = &startptp[ startptix + 1 ];
			}
			assert( ! p->sp_flags );
			mlog( MLOG_VERBOSE | MLOG_INOMAP,
			      "stream %u: ino %llu offset %lld to ",
			      startptix,
			      p->sp_ino,
			      p->sp_offset );
			if ( ! ep ) {
				mlog( MLOG_VERBOSE | MLOG_BARE | MLOG_INOMAP,
				      "end\n" );
			} else {
				mlog( MLOG_VERBOSE |  MLOG_BARE | MLOG_INOMAP,
				      "ino %llu offset %lld\n",
				      ep->sp_ino,
				      ep->sp_offset );
			}
		}
	}

	free( ( void * )bstatbufp );
	free( ( void * )getdentbufp );
	mlog( MLOG_VERBOSE | MLOG_INOMAP,
	      "ino map construction complete\n" );
	return BOOL_TRUE;
}

void
inomap_skip( ino64_t ino )
{
	intgen_t oldstate;

	oldstate = map_get( ino );
	if ( oldstate == MAP_NDR_CHANGE) {
		( void )map_set( ino, MAP_NDR_NOCHNG );
	}

	if ( oldstate == MAP_DIR_CHANGE
	     ||
	     oldstate == MAP_DIR_SUPPRT ) {
		( void )map_set( ino, MAP_DIR_NOCHNG );
	}
}


/* definition of locally defined static functions ****************************/

/* callback context and operators - inomap_build makes extensive use
 * of iterators. below are the callbacks given to these iterators.
 */
static bool_t cb_last;		/* set by cb_context() */
static time_t cb_lasttime;	/* set by cb_context() */
static bool_t cb_resume;	/* set by cb_context() */
static time_t cb_resumetime;	/* set by cb_context() */
static size_t cb_resumerangecnt;/* set by cb_context() */
static drange_t *cb_resumerangep;/* set by cb_context() */
static ix_t cb_subtreecnt;	/* set by cb_context() */
static void *cb_inomap_state_contextp;
static startpt_t *cb_startptp;	/* set by cb_context() */
static size_t cb_startptcnt;	/* set by cb_context() */
static size_t cb_startptix;	/* set by cb_spinit(), incr. by cb_startpt */
static off64_t cb_datasz;	/* set by cb_context() */
static off64_t cb_accum;	/* set by cb_context(), cb_spinit() */
static off64_t cb_incr;		/* set by cb_spinit(), used by cb_startpt() */
static off64_t cb_target;	/* set by cb_spinit(), used by cb_startpt() */
static off64_t cb_dircnt;	/* number of dirs CHANGED or PRUNE */
static off64_t cb_nondircnt;	/* number of non-dirs CHANGED */
static char *cb_getdentbufp;
static size_t cb_getdentbufsz;
static size_t cb_maxrecursionlevel;

/* cb_context - initializes the call back context for the add and prune
 * phases of inomap_build().
 */
static void
cb_context( bool_t last,
	    time_t lasttime,
	    bool_t resume,
	    time_t resumetime,
	    size_t resumerangecnt,
	    drange_t *resumerangep,
	    ix_t subtreecnt,
	    startpt_t *startptp,
	    size_t startptcnt,
	    char *getdentbufp,
	    size_t getdentbufsz )
{
	cb_last = last;
	cb_lasttime = lasttime;
	cb_resume = resume;
	cb_resumetime = resumetime;
	cb_resumerangecnt = resumerangecnt;
	cb_resumerangep = resumerangep;
	cb_subtreecnt = subtreecnt;
	cb_startptp = startptp;
	cb_startptcnt = startptcnt;
	cb_accum = 0;
	cb_dircnt = 0;
	cb_nondircnt = 0;
	cb_getdentbufp = getdentbufp;
	cb_getdentbufsz = getdentbufsz;
	cb_maxrecursionlevel = 0;

	map_init( );
}

static void
cb_postmortem( void )
{
	mlog( MLOG_DEBUG | MLOG_NOTE | MLOG_INOMAP,
	      "maximum subtree pruning recursion depth: %u\n",
	      cb_maxrecursionlevel );
}

/* cb_add - called for all inodes in the file system. checks
 * mod and create times to decide if should be dumped. sets all
 * unmodified directories to be dumped for supprt. notes if any
 * files or directories have not been modified.
 */
/* ARGSUSED */
static intgen_t
cb_add( void *arg1,
	jdm_fshandle_t *fshandlep,
	intgen_t fsfd,
	xfs_bstat_t *statp )
{
	register time_t mtime = statp->bs_mtime.tv_sec;
	register time_t ctime = statp->bs_ctime.tv_sec;
	register time_t ltime = max( mtime, ctime );
	register mode_t mode = statp->bs_mode & S_IFMT;
	ino64_t ino = statp->bs_ino;
	bool_t changed;
	bool_t resumed;

	( *inomap_statdonep )++;

	/* skip if no links
	 */
	if ( statp->bs_nlink == 0 ) {
		return 0;
	}

	/* if no portion of this ino is in the resume range,
	 * then only dump it if it has changed since the interrupted
	 * dump.
	 *
	 * otherwise, if some or all of this ino is in the resume range,
	 * and has changed since the base dump upon which the original
	 * increment was based, dump it if it has changed since that
	 * original base dump.
	 */
	if ( cb_resume && ! cb_inoinresumerange( ino )) {
		if ( ltime >= cb_resumetime ) {
			changed = BOOL_TRUE;
		} else {
			changed = BOOL_FALSE;
		}
	} else if ( cb_last ) {
		if ( ltime >= cb_lasttime ) {
			changed = BOOL_TRUE;
		} else {
			changed = BOOL_FALSE;
		}
	} else {
		changed = BOOL_TRUE;
	}

	/* this is redundant: make sure any ino partially dumped
	 * is completed.
	 */
	if ( cb_resume && cb_inoresumed( ino )) {
		resumed = BOOL_TRUE;
	} else {
		resumed = BOOL_FALSE;
	}

	if ( changed ) {
		if ( mode == S_IFDIR ) {
			map_add( ino, MAP_DIR_CHANGE );
			cb_dircnt++;
		} else {
			map_add( ino, MAP_NDR_CHANGE );
			cb_nondircnt++;
			cb_datasz += estimate_dump_space( statp );
		}
	} else if ( resumed ) {
		assert( mode != S_IFDIR );
		assert( changed );
	} else {
		if ( mode == S_IFDIR ) {
			register bool_t *pruneneededp = ( bool_t * )arg1;
			*pruneneededp = BOOL_TRUE;
			map_add( ino, MAP_DIR_SUPPRT );
			cb_dircnt++;
		} else {
			map_add( ino, MAP_NDR_NOCHNG );
		}
	}

	return 0;
}

static bool_t
cb_inoinresumerange( ino64_t ino )
{
	register size_t streamix;

	for ( streamix = 0 ; streamix < cb_resumerangecnt ; streamix++ ) {
		register drange_t *rp = &cb_resumerangep[ streamix ];
		if ( ! ( rp->dr_begin.sp_flags & STARTPT_FLAGS_END )
		     &&
		     ino >= rp->dr_begin.sp_ino
		     &&
		     ( ( rp->dr_end.sp_flags & STARTPT_FLAGS_END )
		       ||
		       ino < rp->dr_end.sp_ino
		       ||
		       ( ino == rp->dr_end.sp_ino
			 &&
			 rp->dr_end.sp_offset != 0 ))) {
			return BOOL_TRUE;
		}
	}

	return BOOL_FALSE;
}

static bool_t
cb_inoresumed( ino64_t ino )
{
	size_t streamix;

	for ( streamix = 0 ; streamix < cb_resumerangecnt ; streamix++ ) {
		drange_t *rp = &cb_resumerangep[ streamix ];
		if ( ! ( rp->dr_begin.sp_flags & STARTPT_FLAGS_END )
		     &&
		     ino == rp->dr_begin.sp_ino
		     &&
		     rp->dr_begin.sp_offset != 0 ) {
			return BOOL_TRUE;
		}
	}

	return BOOL_FALSE;
}

/* cb_prune -  does subtree and incremental pruning.
 * calls cb_cond_del() to do dirty work on subtrees.
 */
static intgen_t
cb_prune( void *arg1,
	  jdm_fshandle_t *fshandlep,
	  intgen_t fsfd,
	  xfs_bstat_t *statp )
{
	register bool_t *rescanneededp = ( bool_t * )arg1;
	ino64_t ino = statp->bs_ino;

	assert( ( statp->bs_mode & S_IFMT ) == S_IFDIR );

	if ( cb_subtreecnt > 0 ) {
		if ( subtreelist_contains( ino )) {
			intgen_t n = 0;
			intgen_t cbrval = 0;
			( void )diriter( fshandlep,
					 fsfd,
					 statp,
					 cb_count_in_subtreelist,
					 ( void * )&n,
					 &cbrval,
					 cb_getdentbufp,
					 cb_getdentbufsz );
			if ( n > 0 ) {
				( void )diriter( fshandlep,
						 fsfd,
						 statp,
						 cb_cond_del,
						 0,
						 &cbrval,
						 cb_getdentbufp,
						 cb_getdentbufsz );
			}
		}
	}
	if ( map_get( ino ) == MAP_DIR_SUPPRT ) {
		intgen_t n = 0;
		intgen_t cbrval = 0;
		( void )diriter( fshandlep,
				 fsfd,
				 statp,
				 cb_count_needed_children,
				 ( void * )&n,
				 &cbrval,
				 cb_getdentbufp,
				 cb_getdentbufsz );
		if ( n == 0 ) {
			( void )map_set( ino, MAP_DIR_NOCHNG );
			cb_dircnt--;
			( *rescanneededp )++;
		}
	}

	( *inomap_statdonep )++;

	return 0;
}

/* cb_count_in_subtreelist - used by cb_prune() to look for possible
 * subtree pruning.
 */
/* ARGSUSED */
static intgen_t
cb_count_in_subtreelist( void *arg1,
			 jdm_fshandle_t *fshandlep,
			 intgen_t fsfd,
			 xfs_bstat_t *statp,
			 char *namep )
{
	if ( subtreelist_contains( statp->bs_ino )) {
		intgen_t *np = ( intgen_t * )arg1;
		( *np )++;
	}

	return 0;
}

/* ARGSUSED */
static intgen_t
cb_count_needed_children( void *arg1,
			  jdm_fshandle_t *fshandlep,
			  intgen_t fsfd,
			  xfs_bstat_t *statp,
			  char *namep )
{
	intgen_t state = map_get( statp->bs_ino );
	
	if ( state != MAP_INO_UNUSED
	     &&
	     state != MAP_DIR_NOCHNG
	     &&
	     state != MAP_NDR_NOCHNG ) {
		register intgen_t *np = ( intgen_t * )arg1;
		( *np )++;
	}

	return 0;
}

/* cb_cond_del - usd by cb_prune to check and do subtree pruning
 */
/* ARGSUSED */
static intgen_t
cb_cond_del( void *arg1,
	     jdm_fshandle_t *fshandlep,
	     intgen_t fsfd,
	     xfs_bstat_t *statp,
	     char *namep )
{
	ino64_t ino = statp->bs_ino;

	if ( ! subtreelist_contains( ino )) {
		cb_del( 0, fshandlep, fsfd, statp, namep );
	}

	return 0;
}

#define GDRECURSEDEPTHMAX	32

static char *gdrecursearray[ GDRECURSEDEPTHMAX ];

static void
gdrecursearray_init( void )
{
	ix_t level;

	for ( level = 0 ; level < GDRECURSEDEPTHMAX ; level++ ) {
		gdrecursearray[ level ] = 0;
	}
}

static void
gdrecursearray_free( void )
{
	ix_t level;

	for ( level = 0 ; level < GDRECURSEDEPTHMAX ; level++ ) {
		if ( gdrecursearray[ level ] ) {
			free( ( void * )gdrecursearray[ level ] );
			gdrecursearray[ level ] = 0;
		}
	}
}

/* cb_del - used by cb_cond_del() to actually delete a subtree.
 * recursive.
 */
/* ARGSUSED */
static intgen_t
cb_del( void *arg1,
	jdm_fshandle_t *fshandlep,
	intgen_t fsfd,
	xfs_bstat_t *statp,
	char *namep )
{
	ino64_t ino = statp->bs_ino;
	intgen_t oldstate;
	register size_t recursion_level = ( size_t )arg1;

	if ( recursion_level >= GDRECURSEDEPTHMAX ) {
		mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_INOMAP,
		      "subtree pruning recursion depth exceeds max (%d): "
		      "some unselected subtrees may be included in dump\n",
		      GDRECURSEDEPTHMAX );
		return 0;
	}

	if ( cb_maxrecursionlevel < recursion_level ) {
		cb_maxrecursionlevel = recursion_level;
	}

	oldstate = MAP_INO_UNUSED;

	if ( ( statp->bs_mode & S_IFMT ) == S_IFDIR ) {
		intgen_t cbrval = 0;
		oldstate = map_set( ino, MAP_DIR_NOCHNG );
		if ( ! gdrecursearray[ recursion_level ] ) {
			char *getdentbufp;
			getdentbufp = ( char * )memalign( pgsz, GETDENTBUFSZ );
			assert( getdentbufp );
			gdrecursearray[ recursion_level ] = getdentbufp;
		}
		( void )diriter( fshandlep,
				 fsfd,
				 statp,
				 cb_del,
				 ( void * )( recursion_level + 1 ),
				 &cbrval,
				 gdrecursearray[ recursion_level ],
				 GETDENTBUFSZ );
		mlog( MLOG_DEBUG | MLOG_INOMAP,
		      "pruning dir ino %llu\n",
		      ino );
	} else if ( statp->bs_nlink <= 1 ) {
		mlog( MLOG_DEBUG | MLOG_INOMAP,
		      "pruning non-dir ino %llu\n",
		      ino );
		oldstate = map_set( ino, MAP_NDR_NOCHNG );
	} else if ( mln_register( ino, statp->bs_nlink ) == 0 ) {
		mlog( MLOG_DEBUG | MLOG_INOMAP,
		      "pruning non-dir ino %llu\n",
		      ino );
		oldstate = map_set( ino, MAP_NDR_NOCHNG );
	}

	if ( oldstate == MAP_DIR_CHANGE || oldstate == MAP_DIR_SUPPRT ){
		cb_dircnt--;
	} else if ( oldstate == MAP_NDR_CHANGE ) {
		cb_nondircnt--;
	}

	return 0;
}

static void
cb_accuminit_sz( void )
{
	cb_datasz = 0;
}

static void
cb_accuminit_ctx( void *inomap_state_contextp )
{
	cb_inomap_state_contextp = inomap_state_contextp;
}

/* used by inomap_build() to add up the dump space needed for
 * all non-directory files.
 */
/* ARGSUSED */
static intgen_t
cb_accumulate( void *arg1,
	       jdm_fshandle_t *fshandlep,
	       intgen_t fsfd,
	       xfs_bstat_t *statp )
{
	register intgen_t state;

	( *inomap_statdonep )++;

	/* skip if no links
	 */
	if ( statp->bs_nlink == 0 ) {
		return 0;
	}

	state = inomap_state( cb_inomap_state_contextp, statp->bs_ino );
	if ( state == MAP_NDR_CHANGE ) {
		cb_datasz += estimate_dump_space( statp );
	}

	return 0;
}

/* cb_spinit - initializes context for the startpoint calculation phase of
 * inomap_build. cb_startptix is the index of the next startpoint to
 * record. cb_incr is the dump space distance between each startpoint.
 * cb_target is the target accum value for the next startpoint.
 * cb_accum accumulates the dump space.
 */
static void
cb_spinit( void )
{
	cb_startptix = 0;
	cb_incr = cb_datasz / ( off64_t )cb_startptcnt;
	cb_target = 0; /* so first ino will push us over the edge */
	cb_accum = 0;
}

/* cb_startpt - called for each non-directory inode. accumulates the
 * require dump space, and notes startpoints. encodes a heuristic for
 * selecting startpoints. decides for each file whether to include it
 * in the current stream, start a new stream beginning with that file,
 * or split the file between the old and new streams. in the case of
 * a split decision, always split at a BBSIZE boundary.
 */
#define TOO_SHY		1000000	/* max accept. accum short of target */
#define TOO_BOLD	1000000	/* max accept. accum beyond target */

typedef enum {
	HOLD,	/* don't change */
	BUMP,	/* set a new start point and put this file after it */
	SPLIT,	/* set a new start point and split this file across it */
	YELL	/* impossible condition; complain */
} action_t;

/* ARGSUSED */
static intgen_t
cb_startpt( void *arg1,
	    jdm_fshandle_t *fshandlep,
	    intgen_t fsfd,
	    xfs_bstat_t *statp )
{
	register intgen_t state;

	off64_t estimate;
	off64_t old_accum = cb_accum;
	off64_t qty;	/* amount of a SPLIT file to skip */
	action_t action;

	( *inomap_statdonep )++;

	/* skip if no links
	 */
	if ( statp->bs_nlink == 0 ) {
		return 0;
	}

	/* skip if not in inomap or not a non-dir
	 */
	state = inomap_state( cb_inomap_state_contextp, statp->bs_ino );
	if ( state != MAP_NDR_CHANGE ) {
		return 0;
	}

	assert( cb_startptix < cb_startptcnt );

	estimate = estimate_dump_space( statp );
	cb_accum += estimate;

	/* loop until no new start points found. loop is necessary
	 * to handle the pathological case of a huge file so big it
	 * spans several streams.
	 */
	action = ( action_t )HOLD; /* irrelevant, but demanded by lint */
	do {
		/* decide what to do: hold, bump, or split. there are
		 * 8 valid cases to consider:
		 * 1) accum prior to this file is way too short of the
		 *    target, and accum incl. this file is also shy: HOLD;
		 * 2) accum prior to this file is way too short of the
		 *    target, and accum incl. this file is close to but
		 *    still short of target: HOLD;
		 * 3) accum prior to this file is way too short of the
		 *    target, and accum incl. this file is a little beyond
		 *    the target: HOLD;
		 * 4) accum prior to this file is way too short of the
		 *    target, and accum incl. this file is way beyond
		 *    the target: SPLIT;
		 * 5) accum prior to this file is close to target, and
		 *    accum incl. this file is still short of target: HOLD;
		 * 6) accum prior to this file is close to target, and
		 *    accum incl. this file is a little beyond the target,
		 *    and excluding this file would be less short of target
		 *    than including it would be beyond the target: BUMP;
		 * 7) accum prior to this file is close to target, and
		 *    accum incl. this file is a little beyond the target,
		 *    and including this file would be less beyond target
		 *    than excluding it would be short of target: HOLD;
		 * 8) accum prior to this file is close to target, and
		 *    accum incl. this file is would be way beyond the
		 *    target: HOLD.
		 */
		if ( cb_target - old_accum >= TOO_SHY ) {
			if ( cb_target - cb_accum >= TOO_SHY ) {
				action = ( action_t )HOLD;
			} else if ( cb_accum <= cb_target ) {
				action = ( action_t )HOLD;
			} else if ( cb_accum - cb_target < TOO_BOLD ) {
				action = ( action_t )HOLD;
			} else {
				action = ( action_t )SPLIT;
			}
		} else {
			if ( cb_target - cb_accum >= TOO_SHY ) {
				action = ( action_t )YELL;
			} else if ( cb_accum < cb_target ) {
				action = ( action_t )HOLD;
			} else if ( cb_accum - cb_target < TOO_BOLD ) {
				if ( cb_accum - cb_target >=
						      cb_target - old_accum ) {
					action = ( action_t )BUMP;
				} else {
					action = ( action_t )HOLD;
				}
			} else {
				action = ( action_t )BUMP;
			}
		}

		/* perform the action selected above
		 */
		switch ( action ) {
		case ( action_t )HOLD:
			break;
		case ( action_t )BUMP:
			cb_startptp->sp_ino = statp->bs_ino;
			cb_startptp->sp_offset = 0;
			cb_startptix++;
			cb_startptp++;
			cb_target += cb_incr;
			if ( cb_startptix == cb_startptcnt ) {
				return 1; /* done; abort the iteration */
			}
			break;
		case ( action_t )SPLIT:
			cb_startptp->sp_ino = statp->bs_ino;
			qty = ( cb_target - old_accum )
			      &
			      ~( off64_t )( BBSIZE - 1 );
			cb_startptp->sp_offset =
					quantity2offset( fshandlep,
							 statp,
							 qty );
			cb_startptix++;
			cb_startptp++;
			cb_target += cb_incr;
			if ( cb_startptix == cb_startptcnt ) {
				return 1; /* done; abort the iteration */
			}
			break;
		default:
			assert( 0 );
			return 1;
		}
	} while ( action == ( action_t )BUMP || action == ( action_t )SPLIT );

	return 0;
}

/* map context and operators
 */

/* the inomap is implimented as a linked list of chunks. each chunk contains
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

#ifdef OLDCODE
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
#else /* OLDCODE */
#ifdef MACROBITS
#define SEG_ADD_BITS( segp, ino, state )				\
	{								\
		register ino64_t relino;				\
		register u_int64_t mask;				\
		relino = ino - segp->base;				\
		mask = ( u_int64_t )1 << relino;			\
		switch( state ) {					\
		case 0:							\
			break;						\
		case 1:							\
			segp->lobits |= mask;				\
			break;						\
		case 2:							\
			segp->mebits |= mask;				\
			break;						\
		case 3:							\
			segp->lobits |= mask;				\
			segp->mebits |= mask;				\
			break;						\
		case 4:							\
			segp->hibits |= mask;				\
			break;						\
		case 5:							\
			segp->lobits |= mask;				\
			segp->hibits |= mask;				\
			break;						\
		case 6:							\
			segp->mebits |= mask;				\
			segp->hibits |= mask;				\
			break;						\
		case 7:							\
			segp->lobits |= mask;				\
			segp->mebits |= mask;				\
			segp->hibits |= mask;				\
			break;						\
		}							\
	}

#define SEG_SET_BITS( segp, ino, state )				\
	{								\
		register ino64_t relino;				\
		register u_int64_t mask;				\
		register u_int64_t clrmask;				\
		relino = ino - segp->base;				\
		mask = ( u_int64_t )1 << relino;			\
		clrmask = ~mask;					\
		switch( state ) {					\
		case 0:							\
			segp->lobits &= clrmask;			\
			segp->mebits &= clrmask;			\
			segp->hibits &= clrmask;			\
			break;						\
		case 1:							\
			segp->lobits |= mask;				\
			segp->mebits &= clrmask;			\
			segp->hibits &= clrmask;			\
			break;						\
		case 2:							\
			segp->lobits &= clrmask;			\
			segp->mebits |= mask;				\
			segp->hibits &= clrmask;			\
			break;						\
		case 3:							\
			segp->lobits |= mask;				\
			segp->mebits |= mask;				\
			segp->hibits &= clrmask;			\
			break;						\
		case 4:							\
			segp->lobits &= clrmask;			\
			segp->mebits &= clrmask;			\
			segp->hibits |= mask;				\
			break;						\
		case 5:							\
			segp->lobits |= mask;				\
			segp->mebits &= clrmask;			\
			segp->hibits |= mask;				\
			break;						\
		case 6:							\
			segp->lobits &= clrmask;			\
			segp->mebits |= mask;				\
			segp->hibits |= mask;				\
			break;						\
		case 7:							\
			segp->lobits |= mask;				\
			segp->mebits |= mask;				\
			segp->hibits |= mask;				\
			break;						\
		}							\
	}

#define SEG_GET_BITS( segp, ino, state )				\
	{								\
		register ino64_t relino;				\
		register u_int64_t mask;				\
		relino = ino - segp->base;				\
		mask = ( u_int64_t )1 << relino;			\
		if ( segp->lobits & mask ) {				\
			state = 1;					\
		} else {						\
			state = 0;					\
		}							\
		if ( segp->mebits & mask ) {				\
			state |= 2;					\
		}							\
		if ( segp->hibits & mask ) {				\
			state |= 4;					\
		}							\
	}
#else /* MACROBITS */
static void
SEG_ADD_BITS( seg_t *segp, ino64_t ino, intgen_t state )
{
	register ino64_t relino;
	register u_int64_t mask;
	relino = ino - segp->base;
	mask = ( u_int64_t )1 << relino;
	switch( state ) {
	case 0:
		break;
	case 1:
		segp->lobits |= mask;
		break;
	case 2:
		segp->mebits |= mask;
		break;
	case 3:
		segp->lobits |= mask;
		segp->mebits |= mask;
		break;
	case 4:
		segp->hibits |= mask;
		break;
	case 5:
		segp->lobits |= mask;
		segp->hibits |= mask;
		break;
	case 6:
		segp->mebits |= mask;
		segp->hibits |= mask;
		break;
	case 7:
		segp->lobits |= mask;
		segp->mebits |= mask;
		segp->hibits |= mask;
		break;
	}
}

static void
SEG_SET_BITS( seg_t *segp, ino64_t ino, intgen_t state )
{
	register ino64_t relino;
	register u_int64_t mask;
	register u_int64_t clrmask;
	relino = ino - segp->base;
	mask = ( u_int64_t )1 << relino;
	clrmask = ~mask;
	switch( state ) {
	case 0:
		segp->lobits &= clrmask;
		segp->mebits &= clrmask;
		segp->hibits &= clrmask;
		break;
	case 1:
		segp->lobits |= mask;
		segp->mebits &= clrmask;
		segp->hibits &= clrmask;
		break;
	case 2:
		segp->lobits &= clrmask;
		segp->mebits |= mask;
		segp->hibits &= clrmask;
		break;
	case 3:
		segp->lobits |= mask;
		segp->mebits |= mask;
		segp->hibits &= clrmask;
		break;
	case 4:
		segp->lobits &= clrmask;
		segp->mebits &= clrmask;
		segp->hibits |= mask;
		break;
	case 5:
		segp->lobits |= mask;
		segp->mebits &= clrmask;
		segp->hibits |= mask;
		break;
	case 6:
		segp->lobits &= clrmask;
		segp->mebits |= mask;
		segp->hibits |= mask;
		break;
	case 7:
		segp->lobits |= mask;
		segp->mebits |= mask;
		segp->hibits |= mask;
		break;
	}
}

static intgen_t
SEG_GET_BITS( seg_t *segp, ino64_t ino )
{
	intgen_t state;
	register ino64_t relino;
	register u_int64_t mask;
	relino = ino - segp->base;
	mask = ( u_int64_t )1 << relino;
	if ( segp->lobits & mask ) {
		state = 1;
	} else {
		state = 0;
	}
	if ( segp->mebits & mask ) {
		state |= 2;
	}
	if ( segp->hibits & mask ) {
		state |= 4;
	}

	return state;
}
#endif /* MACROBITS */
#endif /* OLDCODE */

#define INOPERSEG	( sizeof( (( seg_t * )0 )->lobits ) * NBBY )

#define HNKSZ		( 4 * PGSZ )
#define SEGPERHNK	( ( HNKSZ / sizeof( seg_t )) - 1 )

struct hnk {
	seg_t seg[ SEGPERHNK ];
	ino64_t maxino;
	struct hnk *nextp;
	char pad[sizeof( seg_t ) - sizeof( ino64_t ) - sizeof( struct hnk * )];
};

typedef struct hnk hnk_t;

/* context for inomap construction - initialized by map_init
 */
static u_int64_t hnkcnt;
static u_int64_t segcnt;
static hnk_t *roothnkp;
static hnk_t *tailhnkp;
static seg_t *lastsegp;
static ino64_t last_ino_added;

/*DBGstatic void
showhnk( void )
{
	int segix;
	mlog( MLOG_NORMAL, "roothnkp == 0x%x\n", roothnkp );
	mlog( MLOG_NORMAL, "maxino == %llu\n", roothnkp->maxino );
	mlog( MLOG_NORMAL, "nextp == 0x%x\n", roothnkp->nextp );
	for ( segix = 0 ; segix < 2 ; segix++ ) {
		mlog( MLOG_NORMAL,
		      "seg[%d] %llu 0x%llx 0x%llx 0x%llx\n",
		      segix,
		      roothnkp->seg[ segix ].base,
		      roothnkp->seg[ segix ].lobits,
		      roothnkp->seg[ segix ].mebits,
		      roothnkp->seg[ segix ].hibits );
	}
}*/

static void
map_init( void )
{
	assert( sizeof( hnk_t ) == HNKSZ );

	roothnkp = 0;
	hnkcnt = 0;
	segcnt = 0;
}

#ifdef SIZEEST
u_int64_t
inomap_getsz( void )
{
	return hnkcnt * HNKSZ;
}
#endif /* SIZEEST */

/* called for every ino to be added to the map. assumes the ino in
 * successive calls will be increasing monotonically.
 */
static void
map_add( ino64_t ino, intgen_t state )
{
	hnk_t *newtailp;

	if ( roothnkp == 0 ) {
		roothnkp = ( hnk_t * )calloc( 1, sizeof( hnk_t ));
		assert( roothnkp );
		tailhnkp = roothnkp;
		hnkcnt++;
		lastsegp = &tailhnkp->seg[ 0 ];
		SEG_SET_BASE( lastsegp, ino );
		SEG_ADD_BITS( lastsegp, ino, state );
		tailhnkp->maxino = ino;
		last_ino_added = ino;
		segcnt++;
		return;
	}

	assert( ino > last_ino_added );

	if ( ino >= lastsegp->base + INOPERSEG ) {
		lastsegp++;
		if ( lastsegp >= tailhnkp->seg + SEGPERHNK ) {
			assert( lastsegp == tailhnkp->seg + SEGPERHNK );
			newtailp = ( hnk_t * )calloc( 1, sizeof( hnk_t ));
			assert( newtailp );
			tailhnkp->nextp = newtailp;
			tailhnkp = newtailp;
			hnkcnt++;
			lastsegp = &tailhnkp->seg[ 0 ];
		}
		SEG_SET_BASE( lastsegp, ino );
		segcnt++;
	}

	SEG_ADD_BITS( lastsegp, ino, state );
	tailhnkp->maxino = ino;
	last_ino_added = ino;
}

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
#ifdef MACROBITS
				SEG_GET_BITS( segp, ino, state );
#else /* MACROBITS */
				state = SEG_GET_BITS( segp, ino );
#endif /* MACROBITS */
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
map_get( ino64_t ino )
{
 	return map_getset( ino, MAP_INO_UNUSED, BOOL_FALSE );
}

static intgen_t
map_set( ino64_t ino, intgen_t state )
{
	intgen_t oldstate;

 	oldstate = map_getset( ino, state, BOOL_TRUE );
	return oldstate;
}

/* returns the map state of the specified ino. optimized for monotonically
 * increasing ino argument in successive calls. caller must supply context,
 * since this may be called from several threads.
 */
struct inomap_state_context {
	hnk_t *currhnkp;
	seg_t *currsegp;
	ino64_t last_ino_requested;
	size64_t backupcnt;
};

typedef struct inomap_state_context inomap_state_context_t;

void *
inomap_state_getcontext( void )
{
	inomap_state_context_t *cp;
	cp = ( inomap_state_context_t * )
				calloc( 1, sizeof( inomap_state_context_t ));
	assert( cp );
	assert( roothnkp );

	cp->currhnkp = roothnkp;
	cp->currsegp = roothnkp->seg;

	return ( void * )cp;
}

intgen_t
inomap_state( void *p, ino64_t ino )
{
	register inomap_state_context_t *cp = ( inomap_state_context_t * )p;
	register intgen_t state;

	/* if we go backwards, re-initialize the context
	 */
	if( ino <= cp->last_ino_requested ) {
		assert( roothnkp );
		cp->currhnkp = roothnkp;
		cp->currsegp = roothnkp->seg;
		cp->backupcnt++;
	}
	cp->last_ino_requested = ino;

	if ( cp->currhnkp == 0 ) {
		return MAP_INO_UNUSED;
	}

	if ( ino > last_ino_added ) {
		return MAP_INO_UNUSED;
	}

	while ( ino > cp->currhnkp->maxino ) {
		cp->currhnkp = cp->currhnkp->nextp;
		assert( cp->currhnkp );
		cp->currsegp = cp->currhnkp->seg;
	}
	while ( ino >= cp->currsegp->base + INOPERSEG ) {
		cp->currsegp++;
		if ( cp->currsegp >= cp->currhnkp->seg + SEGPERHNK ) {
			assert( 0 ); /* can't be here! */
			return MAP_INO_UNUSED;
		}
	}

	if ( ino < cp->currsegp->base ) {
		return MAP_INO_UNUSED;
	}

#ifdef MACROBITS
	SEG_GET_BITS( cp->currsegp, ino, state );
#else /* MACROBITS */
	state = SEG_GET_BITS( cp->currsegp, ino );
#endif /* MACROBITS */
	return state;
}

void
inomap_state_postaccum( void *p )
{
	register inomap_state_context_t *cp = ( inomap_state_context_t * )p;

	mlog( MLOG_DEBUG | MLOG_INOMAP,
	      "inomap_state backed up %llu times\n",
	      cp->backupcnt );
	cp->backupcnt = 0;
}

void
inomap_state_freecontext( void *p )
{
	free( p );
}

void
inomap_writehdr( content_inode_hdr_t *scwhdrp )
{
	assert( roothnkp );

	/* update the inomap info in the content header
	 */
	scwhdrp->cih_inomap_hnkcnt = hnkcnt;
	scwhdrp->cih_inomap_segcnt = segcnt;
	scwhdrp->cih_inomap_dircnt = ( u_int64_t )cb_dircnt;
	scwhdrp->cih_inomap_nondircnt = ( u_int64_t )cb_nondircnt;
	scwhdrp->cih_inomap_firstino = roothnkp->seg[ 0 ].base;
	scwhdrp->cih_inomap_lastino = last_ino_added;
	scwhdrp->cih_inomap_datasz = ( u_int64_t )cb_datasz;
}

rv_t
inomap_dump( drive_t *drivep )
{
	hnk_t *hnkp;

	/* use write_buf to dump the hunks
	 */
	for ( hnkp = roothnkp ; hnkp != 0 ; hnkp = hnkp->nextp ) {
		intgen_t rval;
		rv_t rv;
		drive_ops_t *dop = drivep->d_opsp;

		rval = write_buf( ( char * )hnkp,
				  sizeof( *hnkp ),
				  ( void * )drivep,
				  ( gwbfp_t )dop->do_get_write_buf,
				  ( wfp_t )dop->do_write );
		switch ( rval ) {
		case 0:
			rv = RV_OK;
			break;
		case DRIVE_ERROR_MEDIA:
		case DRIVE_ERROR_EOM:
			rv = RV_EOM;
			break;
		case DRIVE_ERROR_EOF:
			rv = RV_EOF;
			break;
		case DRIVE_ERROR_DEVICE:
			rv = RV_DRIVE;
			break;
		case DRIVE_ERROR_CORE:
		default:
			rv = RV_CORE;
			break;
		}
		if ( rv != RV_OK ) {
			return rv;
		}
	}

	return RV_OK;
}

#ifdef NOTUSED
bool_t
inomap_iter_cb( void *contextp,
		intgen_t statemask,
		bool_t ( *funcp )( void *contextp,
				  ino64_t ino,
				  intgen_t state ))
{
	hnk_t *hnkp;

	assert( ! ( statemask & ( 1 << MAP_INO_UNUSED )));

	for ( hnkp = roothnkp ; hnkp != 0 ; hnkp = hnkp->nextp ) {
		seg_t *segp = hnkp->seg;
		seg_t *endsegp = hnkp->seg + SEGPERHNK;
		for ( ; segp < endsegp ; segp++ ) {
			ino64_t ino;
			ino64_t endino;

			if ( hnkp == tailhnkp && segp > lastsegp ) {
				return BOOL_TRUE;
			}
			ino = segp->base;
			endino = segp->base + INOPERSEG;
			for ( ; ino < endino ; ino++ ) {
				intgen_t st;
#ifdef MACROBITS
				SEG_GET_BITS( segp, ino, st );
#else /* MACROBITS */
				st = SEG_GET_BITS( segp, ino );
#endif /* MACROBITS */
				if ( statemask & ( 1 << st )) {
					bool_t ok;
					ok = ( * funcp )( contextp, ino, st );
					if ( ! ok ) {
						return BOOL_FALSE;
					}
				}
			}
		}
	}

	/* should not get here
	 */
	assert( 0 );
	return BOOL_FALSE;
}
#endif /* NOTUSED */

/* mln - the list of ino's pruned but linked to more than one directory.
 * each time one of those is pruned, increment the cnt for that ino in
 * this list. when the seen cnt equals the link count, the ino can
 * be pruned.
 */
struct mln {
	ino64_t ino;
	nlink_t cnt;
};

typedef struct mln mln_t;

#define MLNGRPSZ	PGSZ
#define MLNGRPLEN	( ( PGSZ / sizeof( mln_t )) - 1 )

struct mlngrp {
	mln_t grp[ MLNGRPLEN ];
	struct mlngrp *nextp;
	char pad1[ MLNGRPSZ
		   -
		   MLNGRPLEN * sizeof( mln_t )
		   -
		   sizeof( struct mlngrp * ) ];
};

typedef struct mlngrp mlngrp_t;

static mlngrp_t *mlngrprootp;
static mlngrp_t *mlngrpnextp;
static mln_t *mlnnextp;

static void
mln_init( void )
{
	assert( sizeof( mlngrp_t ) == MLNGRPSZ );

	mlngrprootp = ( mlngrp_t * )calloc( 1, sizeof( mlngrp_t ));
	assert( mlngrprootp );
	mlngrpnextp = mlngrprootp;
	mlnnextp = &mlngrpnextp->grp[ 0 ];
}

/* finds and increments the mln count for the ino.
 * returns nlink minus number of nlink_register calls made so
 * far for this ino, including the current call: hence returns
 * zero if all links seen.
 */
static nlink_t
mln_register( ino64_t ino, nlink_t nlink )
{
	mlngrp_t *grpp;
	mln_t *mlnp;

	/* first see if ino already registered
	 */
	for ( grpp = mlngrprootp ; grpp != 0 ; grpp = grpp->nextp ) {
		for ( mlnp = grpp->grp; mlnp < grpp->grp + MLNGRPLEN; mlnp++ ){
			if ( mlnp == mlnnextp ) {
				mlnnextp->ino = ino;
				mlnnextp->cnt = 0;
				mlnnextp++;
				if ( mlnnextp >= mlngrpnextp->grp + MLNGRPLEN){
					mlngrpnextp->nextp = ( mlngrp_t * )
						 calloc( 1, sizeof( mlngrp_t));
					assert( mlngrpnextp->nextp );
					mlngrpnextp = mlngrpnextp->nextp;
					mlnnextp = &mlngrpnextp->grp[ 0 ];
				}
			}
			if ( mlnp->ino == ino ) {
				mlnp->cnt++;
				assert( nlink >= mlnp->cnt );
				return ( nlink - mlnp->cnt );
			}
		}
	}
	/* should never get here: loops terminated by mlnnextp
	 */
	assert( 0 );
	return 0;
}

static void
mln_free( void )
{
	mlngrp_t *p;

	p = mlngrprootp;
	while ( p ) {
		mlngrp_t *oldp = p;
		p = p->nextp;
		free( ( void * )oldp );
	}
}

/* the subtreelist is simply the ino's of the elements of each of the
 * subtree pathnames in subtreebuf. the list needs to be arranged
 * in a way advantageous for searching.
 */
#define INOGRPSZ	PGSZ
#define INOGRPLEN	(( PGSZ / sizeof( ino64_t )) - 1 )

struct inogrp {
	ino64_t grp[ INOGRPLEN ];
	struct inogrp *nextp;
	char pad[ sizeof( ino64_t ) - sizeof( struct inogrp * ) ];
};

typedef struct inogrp inogrp_t;

static inogrp_t *inogrprootp;
static inogrp_t *nextgrpp;
static ino64_t *nextinop;
static char *currentpath;

static bool_t
subtreelist_parse( jdm_fshandle_t *fshandlep,
		   intgen_t fsfd,
		   xfs_bstat_t *rootstatp,
		   char *subtreebuf[],
		   ix_t subtreecnt )
{
	ix_t subtreeix;

	assert( sizeof( inogrp_t ) == INOGRPSZ );

	/* initialize the list; it will be added to by the
	 * callback;
	 */
	inogrprootp = ( inogrp_t * )calloc( 1, sizeof( inogrp_t ));
	assert( inogrprootp );
	nextgrpp = inogrprootp;
	nextinop = &nextgrpp->grp[ 0 ];

	/* add the root ino to the subtree list
	 */
	subtreelist_add( rootstatp->bs_ino );

	/* do a recursive descent for each subtree specified
	 */
	for ( subtreeix = 0 ; subtreeix < subtreecnt ; subtreeix++ ) {
		intgen_t cbrval = 0;
		currentpath = subtreebuf[ subtreeix ];
		assert( *currentpath != '/' );
		( void )diriter( fshandlep,
				 fsfd,
				 rootstatp,
				 subtreelist_parse_cb,
				 ( void * )currentpath,
				 &cbrval,
				 cb_getdentbufp,
				 cb_getdentbufsz );
		if ( cbrval != 1 ) {
			mlog( MLOG_NORMAL | MLOG_ERROR | MLOG_INOMAP,
			      "%s: %s\n",
			      cbrval == 0 ? "subtree not present"
					  : "invalid subtree specified",
			      currentpath );
			return BOOL_FALSE;
		}
	}

	return BOOL_TRUE;
}

static void
subtreelist_add( ino64_t ino )
{
	*nextinop++ = ino;
	if ( nextinop >= nextgrpp->grp + INOGRPLEN ) {
		assert( nextinop == nextgrpp->grp + INOGRPLEN );
		nextgrpp->nextp = ( inogrp_t * )calloc( 1, sizeof( inogrp_t ));
		assert( nextgrpp->nextp );
		nextgrpp = nextgrpp->nextp;
		nextinop = &nextgrpp->grp[ 0 ];
	}
}

/* for debugger work only
static void
subtreelist_print( void )
{
	inogrp_t *grpp = inogrprootp;
	ino64_t *inop = &grpp->grp[ 0 ];

	while ( inop != nextinop ) {
		printf( "%llu\n", *inop );
		inop++;
		if ( inop >= grpp->grp + INOGRPLEN ) {
			assert( inop == grpp->grp + INOGRPLEN );
			grpp = grpp->nextp;
			assert( grpp );
			inop = &grpp->grp[ 0 ];
		}
	}
}
 */

static bool_t
subtreelist_contains( ino64_t ino )
{
	inogrp_t *grpp;
	ino64_t *inop;

	for ( grpp = inogrprootp ; grpp != 0 ; grpp = grpp->nextp ) {
		for ( inop = grpp->grp; inop < grpp->grp + INOGRPLEN; inop++ ) {
			if ( inop == nextinop ) {
				return BOOL_FALSE;
			}
			if ( *inop == ino ) {
				return BOOL_TRUE;
			}
		}
	}
	/* should never get here; loops terminated by nextinop
	 */
	assert( 0 );
	return BOOL_FALSE;
}

static void
subtreelist_free( void )
{
	inogrp_t *p;

	p = inogrprootp;
	while ( p ) {
		inogrp_t *oldp = p;
		p = p->nextp;
		free( ( void * )oldp );
	}
}

static intgen_t
subtreelist_parse_cb( void *arg1,
		      jdm_fshandle_t *fshandlep,
		      intgen_t fsfd,
		      xfs_bstat_t *statp,
		      char *name  )
{
	intgen_t cbrval = 0;

	/* arg1 is used to carry the tail of the subtree path
	 */
	char *subpath = ( char * )arg1;

	/* temporarily terminate the subpath at the next slash
	 */
	char *nextslash = strchr( subpath, '/' );
	if ( nextslash ) {
		*nextslash = 0;
	}

	/* if the first element of the subpath doesn't match this
	 * directory entry, try the next entry.
	 */
	if ( strcmp( subpath, name )) {
		if ( nextslash ) {
			*nextslash = '/';
		}
		return 0;
	}

	/* it matches, so add ino to list and continue down the path
	 */
	subtreelist_add( statp->bs_ino );

	/* if we've reached the end of the path, abort the iteration
	 * in a successful way.
	 */
	if ( ! nextslash ) {
		return 1;
	}

	/* if we're not at the end of the path, yet the current
	 * path element is not a directory, complain and abort the
	 * iteration in a way which terminates the application
	 */
	if ( ( statp->bs_mode & S_IFMT ) != S_IFDIR ) {
		*nextslash = '/';
		return 2;
	}

	/* repair the subpath
	 */
	*nextslash = '/';

	/* peel the first element of the subpath and recurse
	 */
	( void )diriter( fshandlep,
			 fsfd,
			 statp,
			 subtreelist_parse_cb,
			 ( void * )( nextslash + 1 ),
			 &cbrval,
			 0,
			 0 );
	return cbrval;
}

/* uses the extent map to figure the first offset in the file
 * with qty real (non-hole) bytes behind it
 */
#define BMAP_LEN	512

static off64_t
quantity2offset( jdm_fshandle_t *fshandlep, xfs_bstat_t *statp, off64_t qty )
{
	intgen_t fd;
	getbmapx_t bmap[ BMAP_LEN ];
	off64_t offset;
	off64_t offset_next;
	off64_t qty_accum;

#ifdef EXTATTR
	/* If GETOPT_DUMPASOFFLINE was specified and the HSM provided an
	 * estimate, then use it.
	 */

	if (hsm_fs_ctxtp) {
		if (HsmEstimateFileOffset(hsm_fs_ctxtp, statp, qty, &offset))
			return offset;
	}
#endif /* EXTATTR */

	offset = 0;
	offset_next = 0;
	qty_accum = 0;
	bmap[ 0 ].bmv_offset = 0;
	bmap[ 0 ].bmv_length = -1;
	bmap[ 0 ].bmv_count = BMAP_LEN;
	bmap[ 0 ].bmv_iflags = BMV_IF_NO_DMAPI_READ;
	bmap[ 0 ].bmv_entries = -1;
	fd = jdm_open( fshandlep, statp, O_RDONLY );
	if ( fd < 0 ) {
		mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_INOMAP,
		      "could not open ino %llu to read extent map: %s\n",
		      statp->bs_ino,
		      strerror( errno ));
		return 0;
	}

	for ( ; ; ) {
		intgen_t eix;
		intgen_t rval;

		rval = fcntl( fd, F_GETBMAPX, bmap );
		if ( rval ) {
			mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_INOMAP,
			      "could not read extent map for ino %llu: %s\n",
			      statp->bs_ino,
			      strerror( errno ));
			( void )close( fd );
			return 0;
		}

		if ( bmap[ 0 ].bmv_entries <= 0 ) {
			assert( bmap[ 0 ].bmv_entries == 0 );
			( void )close( fd );
			return offset_next;
		}

		for ( eix = 1 ; eix <= bmap[ 0 ].bmv_entries ; eix++ ) {
			getbmapx_t *bmapp = &bmap[ eix ];
			off64_t qty_new;
			if ( bmapp->bmv_block == -1 ) {
				continue; /* hole */
			}
			offset = bmapp->bmv_offset * BBSIZE;
			qty_new = qty_accum + bmapp->bmv_length * BBSIZE;
			if ( qty_new >= qty ) {
				( void )close( fd );
				return offset + ( qty - qty_accum );
			}
			offset_next = offset + bmapp->bmv_length * BBSIZE;
			qty_accum = qty_new;
		}
	}
	/* NOTREACHED */
}


static off64_t
estimate_dump_space( xfs_bstat_t *statp )
{
	switch ( statp->bs_mode & S_IFMT ) {
	case S_IFREG:
		/* very rough: must improve this.  If GETOPT_DUMPASOFFLINE was
		 * specified and the HSM provided an estimate, then use it.
		 */
#ifdef EXTATTR
		if (hsm_fs_ctxtp) {
			off64_t	bytes;

			if (HsmEstimateFileSpace(hsm_fs_ctxtp, statp, &bytes))
				return bytes;
		}
#endif	/* EXTATTR */
		return statp->bs_blocks * ( off64_t )statp->bs_blksize;
	case S_IFIFO:
	case S_IFCHR:
	case S_IFDIR:
	case S_IFNAM:
	case S_IFBLK:
	case S_IFSOCK:
	case S_IFLNK:
	/* not yet
	case S_IFUUID:
	*/
		return 0;
	default:
		mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_INOMAP,
		      "unknown inode type: ino=%llu, mode=0x%04x 0%06o\n",
		      statp->bs_ino,
		      statp->bs_mode,
		      statp->bs_mode );
		return 0;
	}
}
