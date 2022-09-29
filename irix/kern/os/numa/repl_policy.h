#ifndef __NUMA_REPL_POLICY_H__
#define __NUMA_REPL_POLICY_H__


/******************************************************************************
 * repl_policy.h
 *	Describes replication policy data structure that gets attached
 *	to replication attribute in a vnode.
 *
 *	Replication policy data structure provides a mechanism to attach
 *	replication policy to a vnode. Primarily policy implementation
 *	expects a set of function pointers to be attached as part of
 *	replication policy, and these are invoked at appropriate times.
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

#ident "$Revision: 1.8 $"

#include <sys/pmo.h>
#include "pmo_list.h"
struct pm;
struct pfdat;
/*
 * repl_policy_t
 *	Replication policy data structure.
 *
 *	Replication policy data structure defines the data structure/access
 *	mechanisms needed to implement replication policy on a pager
 *	object. (vnode in our case).  This data structure is attached
 *	to replication attribute structure one of which is associated with
 *	a vnode capable of having replicated pages.
 */
typedef struct repl_policy_s {

	/*
	 * Function pointer to get a matching page.
	 * get the page that's most suitable for the caller, given
	 * the policy module pm, and the pfd at hand. 
	 * Finds the most suitable page. If no such page exists, and
	 * it's possible to create a new replica do so.
	 */ 
	struct pfdat* 	(*getpage)(bhv_desc_t *, struct pm *, struct pfdat *, int);

	/*
	 * Function pointer to update the policy
	 * Provides a chance to policy implementation module to update
	 * its replication policy for the vnode. 
	 * This routine is called whenever a vnode capable of having
	 * replicated pages is opened.
	 */
	void		(*update)(bhv_desc_t *);

	/*
	 * Function pointer to free any private data structure allocate by
	 * the policy implementation.
	 */

	void		(*free)(struct repl_policy_s *);

	/*
	 * Private data pointer. Policy modules attach their private data
	 * here. 
	 */
	void		*policy_pvt;

#ifdef	DEBUG
	int		repl_debug_level;
#endif
} repl_policy_t;

/*
 * Given a vnode, get the pointer to the replication policy associated
 * with the vnode.
 */
#define	policy_pvt_get(rp, datatype)	((datatype *)(rp)->policy_pvt)
#define	policy_pvt_set(rp, val)		((rp)->policy_pvt = (void *)(val))

/*
 * Replication policy Associated with a process.
 */

/*
 * Function to initialize the replication policy related data structures.
 */
extern void repl_policy_init(void);

/*
 * Default replication policy initialization 
 */
extern void repl_policy_default_init(void);

#define	repl_policy_debuglvl_set(rp, lvl)	((rp)->repl_debug_level = (lvl))

#ifdef	DEBUG
#define	REPL_POLICY_DEBUG_LEVEL(rp)	(rp->repl_debug_level)
#else
#define	REPL_POLICY_DEBUG_LEVEL(rp)	(0)
#endif
/*
 * Policy Statistics
 */
typedef struct repl_policy_default_stat {
	int	rp_setup;
	int	rp_teardn;
	int	rp_getpage;
	int	rp_affupdate;
	int	rp_affmatch;
	int	rp_affnonode;
} repl_policy_stats_t;

#define	REPL_POLICY_STAT	(repl_stat.policy_stat)

struct replattr_s;
extern struct replattr_s *repl_policy_setup(char *);
extern void repl_policy_teardown(struct replattr_s *);
extern void repl_policy_enable(struct replattr_s *);
extern int repl_policy_disable(bhv_desc_t *);

#endif /* __NUMA_REPL_POLICY_H__ */
