#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/common/RCS/inomap.c,v 1.27 1995/10/24 06:56:51 ack Exp $"

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
#include <time.h>
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


/* structure definitions used locally ****************************************/


/* declarations of externally defined global symbols *************************/


/* forward declarations of locally defined static functions ******************/

/* inomap construction callbacks
 */
static void cb_context( bool_t last,
			time_t,
			bool_t,
			time_t,
			size_t,
			drange_t *,
			intgen_t,
			startpt_t *,
			size_t );
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
static void cb_accuminit( void * );
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
				 intgen_t );
static void subtreelist_free( void );
static bool_t subtreelist_contains( ino64_t );

/* multiply linked (mln) abstraction
 */
static void mln_init( void );
static nlink_t mln_register( ino64_t, nlink_t );
static void mln_free( void );


/* definition of locally defined global variables ****************************/


/* definition of locally defined static variables *****************************/


/* definition of locally defined global functions ****************************/

/* inomap_build - build an in-core image of the inode map for the
 * specified file system. identify startpoints in the non-dir inodes,
 * such that the total dump media required is divided into startptcnt segments.
 */
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
	      intgen_t subtreecnt,
	      startpt_t *startptp,
	      size_t startptcnt )
{
	bool_t pruneneeded;
	bool_t rescanneeded;
	intgen_t scancnt;
	intgen_t stat;
	void *inomap_state_contextp;
	intgen_t rval;

	/* parse the subtree list, if any subtrees specified.
	 * this will be used during the tree pruning phase.
	 */
	if ( subtreecnt ) {
		mlog( MLOG_VERBOSE,
		      "ino map phase 1: "
		      "parsing subtree selections\n" );
		if ( ! subtreelist_parse( fshandlep,
					  fsfd,
					  rootstatp,
					  subtreebuf,
					  subtreecnt )) {
			return BOOL_FALSE;
		}
	} else {
		mlog( MLOG_VERBOSE,
		      "ino map phase 1: "
		      "skipping (no subtrees specified)\n" );
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
		    startptcnt );

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
	mlog( MLOG_VERBOSE,
	      "ino map phase 2: "
	      "constructing initial dump list\n" );
	pruneneeded = BOOL_FALSE;
	stat = 0;
	rval = bigstat_iter( fshandlep,
			     fsfd,
			     BIGSTAT_ITER_ALL,
			     ( ino64_t )0,
			     cb_add,
			     ( void * )&pruneneeded,
			     &stat );
	if ( rval ) {
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

		scancnt = 0;
		rescanneeded = BOOL_FALSE; /* not needed, keeps lint happy */
		do {
			scancnt++;
			if ( scancnt <= 1 ) {
				mlog( MLOG_VERBOSE,
				      "ino map phase 3: "
				      "pruning unneeded subtrees\n" );
			} else {
				mlog( MLOG_VERBOSE,
				      "ino map phase 3: "
				      "pass %d\n",
				      scancnt );
			}
			rescanneeded = BOOL_FALSE;
			stat = 0;
			rval = bigstat_iter( fshandlep,
					     fsfd,
					     BIGSTAT_ITER_DIR,
					     ( ino64_t )0,
					     cb_prune,
					     ( void * )&rescanneeded,
					     &stat );
			if ( rval ) {
				return BOOL_FALSE;
			}
		} while ( rescanneeded );

		mln_free( );
	} else {
		mlog( MLOG_VERBOSE,
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

	/* make this available to the callbacks for accum and startpts
	 */
	cb_accuminit( inomap_state_contextp );

	/* calculate the total dump space needed for non-directories
	 */
	mlog( MLOG_VERBOSE,
	      "ino map phase 4: "
	      "estimating dump size\n" );
	stat = 0;
	rval = bigstat_iter( fshandlep,
			     fsfd,
			     BIGSTAT_ITER_NONDIR,
			     ( ino64_t )0,
			     cb_accumulate,
			     0,
			     &stat );
	if ( rval ) {
		inomap_state_freecontext( inomap_state_contextp );
		return BOOL_FALSE;
	}

	/* initialize the callback context for startpoint calculation
	 */
	cb_spinit( );

	/* identify dump stream startpoints
	 */
	if ( startptcnt > 1 ) {
		mlog( MLOG_VERBOSE,
		      "ino map phase 5: "
		      "identifying stream starting points\n" );
	} else {
		mlog( MLOG_VERBOSE,
		      "ino map phase 5: "
		      "skipping (only one dump stream)\n" );
	}
	stat = 0;
	rval = bigstat_iter( fshandlep,
			     fsfd,
			     BIGSTAT_ITER_NONDIR,
			     ( ino64_t )0,
			     cb_startpt,
			     0,
			     &stat );
	if ( rval ) {
		inomap_state_freecontext( inomap_state_contextp );
		return BOOL_FALSE;
	}
	
	inomap_state_freecontext( inomap_state_contextp );

	mlog( MLOG_VERBOSE,
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
static intgen_t cb_subtreecnt;	/* set by cb_context() */
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
	    intgen_t subtreecnt,
	    startpt_t *startptp,
	    size_t startptcnt )
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

	map_init( );
}

/* cb_add - called for all inodes in the file system. checks
 * mod and create times to decide if should be dumped. sets all
 * unmodified directories to be dumped for supprt. notes if any
 * files or directories have not been modified.
 */
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
					 &cbrval );
			if ( n > 0 ) {
				( void )diriter( fshandlep,
						 fsfd,
						 statp,
						 cb_cond_del,
						 0,
						 &cbrval );
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
				 &cbrval );
		if ( n == 0 ) {
			( void )map_set( ino, MAP_DIR_NOCHNG );
			cb_dircnt--;
			( *rescanneededp )++;
		}
	}

	return 0;
}

/* cb_count_in_subtreelist - used by cb_prune() to look for possible
 * subtree pruning.
 */
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
		intgen_t *np = ( intgen_t * )arg1;
		( *np )++;
	}

	return 0;
}

/* cb_cond_del - usd by cb_prune to check and do subtree pruning
 */
static intgen_t
cb_cond_del( void *arg1,
	     jdm_fshandle_t *fshandlep,
	     intgen_t fsfd,
	     xfs_bstat_t *statp,
	     char *namep )
{
	ino64_t ino = statp->bs_ino;

	if ( ! subtreelist_contains( ino )) {
		cb_del( arg1, fshandlep, fsfd, statp, namep );
	}

	return 0;
}

/* cb_del - used by cb_cond_del() to actually delete a subtree.
 * recursive.
 */
static intgen_t
cb_del( void *arg1,
	jdm_fshandle_t *fshandlep,
	intgen_t fsfd,
	xfs_bstat_t *statp,
	char *namep )
{
	ino64_t ino = statp->bs_ino;
	intgen_t oldstate;

	oldstate = MAP_INO_UNUSED;

	if ( ( statp->bs_mode & S_IFMT ) == S_IFDIR ) {
		intgen_t cbrval = 0;
		oldstate = map_set( ino, MAP_DIR_NOCHNG );
		( void )diriter( fshandlep,
				 fsfd,
				 statp,
				 cb_del,
				 0,
				 &cbrval );
		mlog( MLOG_DEBUG,
		      "pruning dir ino %llu\n",
		      ino );
	} else if ( statp->bs_nlink <= 1 ) {
		mlog( MLOG_DEBUG,
		      "pruning non-dir ino %llu\n",
		      ino );
		oldstate = map_set( ino, MAP_NDR_NOCHNG );
	} else if ( mln_register( ino, statp->bs_nlink ) == 0 ) {
		mlog( MLOG_DEBUG,
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
cb_accuminit( void *inomap_state_contextp )
{
	cb_inomap_state_contextp = inomap_state_contextp;
	cb_datasz = 0;
}

/* used by inomap_build() to add up the dump space needed for
 * all non-directory files.
 */
static intgen_t
cb_accumulate( void *arg1,
	       jdm_fshandle_t *fshandlep,
	       intgen_t fsfd,
	       xfs_bstat_t *statp )
{
	register intgen_t state;

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

static intgen_t
cb_startpt( void *arg1,
	    jdm_fshandle_t *fshandlep,
	    intgen_t fsfd,
	    xfs_bstat_t *statp )
{
	register intgen_t state;

	off64_t estimate = estimate_dump_space( statp );
	off64_t old_accum = cb_accum;
	off64_t qty;	/* amount of a SPLIT file to skip */
	action_t action;

	/* skip if not in inomap or not a non-dir
	 */
	state = inomap_state( cb_inomap_state_contextp, statp->bs_ino );
	if ( state != MAP_NDR_CHANGE ) {
		return 0;
	}

	assert( cb_startptix < cb_startptcnt );

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

static void
map_init( void )
{
	assert( sizeof( hnk_t ) == HNKSZ );

	roothnkp = 0;
	hnkcnt = 0;
	segcnt = 0;
}

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
};

typedef struct inomap_state_context inomap_state_context_t;

void *
inomap_state_getcontext( void )
{
	inomap_state_context_t *cp;
	cp = ( inomap_state_context_t * )
				calloc( 1, sizeof( inomap_state_context_t ));
	assert( cp );

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
		cp->currhnkp = roothnkp;
		cp->currsegp = roothnkp->seg;
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

	SEG_GET_BITS( cp->currsegp, ino, state );
	return state;
}

void
inomap_state_freecontext( void *p )
{
	free( p );
}

void
inomap_writehdr( content_inode_hdr_t *scwhdrp )
{
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

intgen_t
inomap_dump( content_t *contentp )
{
	hnk_t *hnkp;

	/* use write_buf to dump the hunks
	 */
	for ( hnkp = roothnkp ; hnkp != 0 ; hnkp = hnkp->nextp ) {
		intgen_t rval;
		media_t *mediap = contentp->c_mediap;
		media_ops_t *mop = mediap->m_opsp;

		rval = write_buf( ( char * )hnkp,
				  sizeof( *hnkp ),
				  ( void * )mediap,
				  ( gwbfp_t )mop->mo_get_write_buf,
				  ( wfp_t )mop->mo_write );
		if ( rval ) {
			return rval;
		}
	}

	return 0;
}

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
				SEG_GET_BITS( segp, ino, st );
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

intgen_t
inomap_restore( content_t *contentp )
{
	content_inode_hdr_t *scrhdrp;
	media_t *mediap = contentp->c_mediap;
	media_ops_t *mop = mediap->m_opsp;
	hnk_t *hnkp;
	intgen_t stat;
	intgen_t rval;

	scrhdrp = ( content_inode_hdr_t * )contentp->c_readhdrp->ch_specific;

	/* first update the inomap info in the content header
	 */
	hnkcnt = scrhdrp->cih_inomap_hnkcnt;

	/* there is no inomap to restore
	 */
	if ( hnkcnt == 0 ) {
		return 0;
	}

	segcnt = scrhdrp->cih_inomap_segcnt;
	last_ino_added = scrhdrp->cih_inomap_lastino;

	/* allocate space for the hunks
	 */
	assert( hnkcnt * sizeof( hnk_t ) <= INT32MAX );
	roothnkp = ( hnk_t * )calloc( ( size_t )hnkcnt, sizeof( hnk_t ));
	assert( roothnkp );
	
	/* read them from the media
	 */
	rval = read_buf( ( char * )roothnkp,
			  sizeof( hnk_t ) * ( size_t )hnkcnt,
			  ( void * )mediap,
			  ( rfp_t )mop->mo_read,
			  ( rrbfp_t )mop->mo_return_read_buf,
			  &stat );
	if ( stat ) {
		return stat;
	}
	assert( ( size_t )rval == sizeof( hnk_t ) * ( size_t )hnkcnt );

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
	
	return 0;
}

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
		   intgen_t subtreecnt )
{
	intgen_t subtreeix;

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
				 &cbrval );
		if ( cbrval != 1 ) {
			mlog( MLOG_NORMAL,
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
			 &cbrval );
	return cbrval;
}

/* uses the extent map to figure the first offset in the file
 * with qty real (non-hole) bytes behind it
 */
#define BMAP_LEN 20	/* make larger; small now for stress-testing */

static off64_t
quantity2offset( jdm_fshandle_t *fshandlep, xfs_bstat_t *statp, off64_t qty )
{
	intgen_t fd;
	getbmap_t bmap[ BMAP_LEN ];
	off64_t offset;
	off64_t offset_next;
	off64_t qty_accum;

	offset = 0;
	offset_next = 0;
	qty_accum = 0;
	bmap[ 0 ].bmv_offset = 0;
	bmap[ 0 ].bmv_length = -1;
	bmap[ 0 ].bmv_count = BMAP_LEN;
	bmap[ 0 ].bmv_entries = -1;
	fd = jdm_open( fshandlep, statp, O_RDONLY );
	if ( fd < 0 ) {
		mlog( MLOG_NORMAL,
		      "could not open ino %llu to read extent map: %s\n",
		      statp->bs_ino,
		      strerror( errno ));
		return 0;
	}

	for ( ; ; ) {
		intgen_t eix;
		intgen_t rval;

		rval = fcntl( fd, F_GETBMAP, bmap );
		if ( rval ) {
			mlog( MLOG_NORMAL,
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
			getbmap_t *bmapp = &bmap[ eix ];
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

	/* impossible to get here
	 */
	/*NOTREACHED*/
	assert( 0 );
	( void )close( fd );
	return 0;
}


static off64_t
estimate_dump_space( xfs_bstat_t *statp )
{
	switch ( statp->bs_mode & S_IFMT ) {
	case S_IFREG:
		/* very rough: must improve this
		 */
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
		mlog( MLOG_NORMAL,
		      "unknown inode type: ino=%llu, mode=0x%04x 0%06o\n",
		      statp->bs_ino,
		      statp->bs_mode,
		      statp->bs_mode );
		return 0;
	}
}
