#ifndef	_NUMA_REPLATTR_H
#define	_NUMA_REPLATTR_H

/******************************************************************************
 * replattr.h
 *	Describes the replcation attribute structure that gets associated
 *	with each vnode which has replication capability. 
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 ******************************************************************************/

#ident "$Revision: 1.5 $"

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)

#include <sys/types.h>
#include <sys/sema.h>
#include <sys/pvnode.h>
#include "repl_policy.h"

/*
 * Various States for Replication data structure.
 */
typedef enum  {
	REPLSTATE_INVALID,      /* Invalid state. Should never be here 	*/
	REPLSTATE_RDY,          /* State: ready to replicate            */
	REPLSTATE_TRANS,        /* State: In transition..being shot down*/
	REPLSTATE_BLOCK         /* State: Blocked, No more replicas	*/
} replstate_t;

#define	REPLOP_SUCCESS	0
#define	REPLOP_FAILED	1


/*
 * Replication attribute data structure.
 * One of these would be attached to each pager object which can have
 * replicated pages (only vnode at this time)
 */ 

typedef struct replattr_s {
	/*
	 * multi-reader single writer.
	 * Held in read mode while replicating.
	 * Held in write mode while shooting down.
	 * Gaurds the state transitions of replstate field.
	 */
	mrlock_t	replwait;

	/* State of replication */
	replstate_t    	replstate;	

	/* 
	 * Number of replicated pages create as part of this vnode
	 *
	 * This is bumped up/down while holding memlock. 
	 * This count is changed mostly while changing vnode's
	 * page count (v_pgcnt), which is also gaurded by
	 * memlock. 
	 */
	int	     	replpgcnt;		

	/* Replication policy attachment position */
	/* 
	 * Embed the data structure instead of creating one more level.
	 * repl_policy_t is a generic policy related data structure
	 * providing the accessor methods for the specific policy.
	 */
	repl_policy_t	policy;

	/* Embed this behavior's descriptor */
	bhv_desc_t	replbhv;

} replattr_t;

/* State transition macros */
#define REPLSTATE_MKRDY(p)      ((p)->replstate = REPLSTATE_RDY)
#define REPLSTATE_MKTRANS(p)    ((p)->replstate = REPLSTATE_TRANS)
#define REPLSTATE_MKBLOCK(p)   	((p)->replstate = REPLSTATE_BLOCK)

#define	REPLPOLICY_GET(replattr)	(&(replattr)->policy)
#define	REPLPOLICY_SET(replattr, val)	((replattr)->policy = (val))

#define BHV_TO_REPL(bdp)		((replattr_t *)BHV_PDATA(bdp))

/*
 * interface to shoot down replicated pages.
 * Tries to shoot down all replicated pages in the range "start:end"
 * In general, the range is expected to be the entire range of pages
 * for which a replica may exist, since all pages on a specified vnode
 * can get replicated.
 */
extern int page_replica_shoot(vnode_t *, replattr_t *, pgno_t, pgno_t);


/* Mutex and Macos to sleep while waiting to lock all ptes attached 
 * to a pfdat.
 * This is needed to shoot down ptes associated with a page.
 * We define an array of semaphores, and use a hash algorithm to pick
 * the one to sleep on.
 *
 * (Instead, we could have one per node, and sleep on that)
 *
 * Wakeup is done via timeout mechanism.
 */

#define REPL_SEMAS     16
#define REPL_SEMAMASK   (REPL_SEMAS - 1)
#define REPLSEMA(p)     &pgrepl_sema[(((__psunsigned_t)(p)>>3) & REPL_SEMAMASK)]
extern sema_t pgrepl_sema[REPL_SEMAS];

/*
 * Replication specific statistics.
 * To Do:
 *	Decide which of these need to be kept as part of statstics exported
 * 	to user level, and NUMAize them.
 */
typedef struct repl_page_stats_s {
	int	replshootdn;
	int	replpgshotdn;
} repl_page_stats_t;

typedef struct repl_vnode_stats_s {
	int	replobjs;
} repl_vnode_stats_t;

typedef struct repl_stat_s {
	repl_page_stats_t	page_stat;
	repl_policy_stats_t	policy_stat;
	repl_vnode_stats_t	vnode_stat;
} repl_stats_t;

#define	REPL_PAGE_STATS		(repl_stat.page_stat)
#define	REPL_VN_STATS		(repl_stat.vnode_stat)
	
extern repl_stats_t	repl_stat;

extern replattr_t *replattr_creat(void);
extern void replattr_free(replattr_t *);

/*
 * Functions doing state transition.
 */
/*
 * Hold replication attribute in ready state. Done, by getting an 
 * extra access count on mrlock.
 */
extern int  replattr_state_hold_rdy(replattr_t *);

/*
 * release the access count taken on the mrlock.
 */
extern void replattr_state_release_rdy(replattr_t *);

/*
 * Move the replication attribute to transient state.
 * grabs mrlock in exclusive mode to do this.
 */
extern int  replattr_state_trans(replattr_t *);

/*
 * Move replication attribute to blocked state.
 * Expects the mrlock to be held in update mode.
 * Releases the mrlock.
 */
extern void replattr_state_block(replattr_t *);

/*
 * Move replattr from blocked/transient to ready state
 */
extern void replattr_state_mkrdy(replattr_t *);


/*
 * Given a vnode, get the replication attribute.
 */

extern replattr_t *replattr_lookup(vnode_t *);


/*
 * replication vnode ops initialization routine.
 */

extern void repl_vninit(void);
#endif /* C || C++ */
#endif /* _NUMA_PREPLATTR_H */
