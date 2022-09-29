#if  defined(NUMA_REPLICATION) 

/*****************************************************************************
 * repl_vnodeops.c
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
 ****************************************************************************/

#ident "$Revision: 1.26 $"

#include <limits.h>
#include <sys/types.h>
#include <sys/immu.h>	
#include <sys/cmn_err.h>
#include <sys/pfdat.h>
#include <sys/vnode.h>
#include <sys/sysmacros.h>		/* offtoc */
#include <sys/systm.h>			/* splhi */
#include <sys/pda.h>
#include <sys/proc.h>
#include <ksys/vfile.h>			/* FWRITE */
#include <sys/kmem.h>
#include <ksys/rmap.h>
#include <sys/debug.h>
#include <sys/idbgentry.h>
#include <sys/errno.h>			/* ETXTBSY 	*/
#include <sys/ktrace.h>
#include <sys/fs_subr.h>		/* fs_nosys 	*/
#include <sys/sysmp.h>			/* numainfo 	*/
#include <sys/numa.h>
#include <sys/cred.h>
#include <ksys/behavior.h>
#include "pmo_base.h"
#include "replattr.h"
#include "pmo_list.h"
#include "pmo_ns.h"
#include "pm.h"
#include "repldebug.h"

#define	SUCCESS		0

/*ARGSUSED*/
static int
repl_open(bhv_desc_t *bdp, vnode_t **vpp, mode_t mode, cred_t *credp)
{
	int		error;
	vnode_t		*vp = BHV_TO_VNODE(bdp);
	repl_policy_t	*rp;
	bhv_desc_t	*nbdp;
	
	/*
	 * Invoke the open routine in the next behavior.
	 */
	PV_NEXT(bdp, nbdp, vop_open);
	PVOP_OPEN(nbdp, vpp, mode, credp, error);

	if (error)
		return error;

		
	if (mode & FWRITE){
		/*
		 * Check if it's possible to disable the replication policy
		 * to allow opening in write mode.
		 */
		if (!VN_ISNONREPLICABLE(vp)) {
			if (repl_policy_disable(bdp) == REPLOP_SUCCESS){
				int s;
				s = VN_LOCK(vp);
				VN_SETNONREPLICABLE(vp);
				VN_CLRREPLICABLE(vp);
				VN_UNLOCK(vp, s);
				return error;
			} else {
				return ETXTBSY;
			}
		}
	}

	rp = REPLPOLICY_GET(BHV_TO_REPL(bdp));

	if (rp->update) {
		/* Update replication policy as number of mappers increase */
		(*rp->update)(bdp);
	}

	return error;
}


/*
 * repl_inactive gets called when the reference count for the vnode 
 * goes to zero. 
 * We come here only if the replication layer has been interposed
 * Just check if the vnode was marked "Nonreplicable" at some point of
 * time. If so, this's the time to turn it off, and mark it back as
 * replicable.
 */
static int
repl_inactive(bhv_desc_t *bdp, cred_t *credp)
{
	vnode_t		*vp = BHV_TO_VNODE(bdp);
	bhv_desc_t	*nbdp;
	int		s;
	int		cache;

	/*
	 * Invoke the inactive routine in the next behavior.
	 */
	PV_NEXT(bdp, nbdp, vop_inactive);
	PVOP_INACTIVE(nbdp, credp, cache);

	if (VN_ISNONREPLICABLE(vp)){
		s = VN_LOCK(vp);
		VN_CLRNONREPLICABLE(vp);
		VN_SETREPLICABLE(vp);
		/* 
		 * vnode was marked non-replicable. Since the replication
		 * layer got introducded, it should have happened during
		 * a replica shootdown. Since the last user of the vnode
		 * is going away, it's possible to reenable replication
		 * on this vnode.
		 */
		VN_UNLOCK(vp, s);
		repl_policy_enable(BHV_TO_REPL(bdp));
	}

	/*
	 * If the underlying behavior is gone, we're going too.
	 */
	if (cache == VN_INACTIVE_NOCACHE) {
		/*
		 * Clear the replication flags in the vnode.
		 */
		VN_FLAGCLR(vp, VREPLICABLE);
	
		/*
		 * Pull our behavior from the vnode.  We don't need
		 * additional locking, because this is the inactive
		 * routine where access to this vnode is single threaded.
		 */
		vn_bhv_remove(VN_BHV_HEAD(vp), bdp);

		repl_policy_teardown(BHV_TO_REPL(bdp));
	}
	return cache;
}

/*
 * reclaim all data associated with vnode replication layer.
 */
static int
repl_reclaim(bhv_desc_t *bdp, int flag)
{
	int		error;
	bhv_desc_t	*nbdp;
	vnode_t		*vp;

	/*
	 * Invoke the reclaim routine in the next behavior.
	 */
	PV_NEXT(bdp, nbdp, vop_reclaim);
	PVOP_RECLAIM(nbdp, flag, error);

	if (error)
		return error;

	vp = BHV_TO_VNODE(bdp);

	/*
	 * Clear the replication flags in the vnode.
	 */
	VN_FLAGCLR(vp, VREPLICABLE);
	
	/*
	 * Pull our behavior from the vnode.  We don't need
	 * additional locking, because this is the reclaim
	 * routine where access to this vnode is single threaded.
	 */
	vn_bhv_remove(VN_BHV_HEAD(vp), bdp);

	repl_policy_teardown(BHV_TO_REPL(bdp));
	return 0;
}


vnodeops_t	repl_vnodeops;
vnodeops_t	*vreplops = &repl_vnodeops;


int	numa_page_replication_enable	= 0; /* XXX turn off */

/*
 * Replication layer interposer.
 *	Interpose the replication layer if not alreay done.
 *	<< Need to come up with a routine to check if the replication
 *	layer is already interposed.
 */
void
repl_interpose(vnode_t *vp, char *policy_name)
{
	replattr_t	*replattr;
	bhv_desc_t	*bdp;
	repl_policy_t	*rp;

	if((numa_page_replication_enable == 0) || (numnodes == 1))
		return;

	panic("repl behavior doesn't work");  /* XXX BHV_SYNCH not on */

	/*
	 * Check if the replication layer is already interposed.
	 * If not, do it now.  We check without the lock since
	 * a read of a single word is atomic anyway.
	 */

	if (vp->v_flag & (VNONREPLICABLE | VREPLICABLE)) {
		return;
	}


	replattr = repl_policy_setup(policy_name);

	if (!replattr)
		return;
	bdp = &replattr->replbhv;

	/*
	 * Initialize the behavior descriptor.
	 */
	bhv_desc_init(bdp, replattr, vp, vreplops);

	/*
	 * Insert, checking if any other process raced with us.
	 */
	VN_BHV_WRITE_LOCK(VN_BHV_HEAD(vp));
	if (vn_bhv_insert(VN_BHV_HEAD(vp), bdp)) {
		/* 
		 * Insertion Failed. Some one has already interposed.
		 * return.  Assert must be done prior to unlocking to 
		 * avoid race with deletion.
		 */
		ASSERT(VN_ISREPLICABLE(vp)); 
		VN_BHV_WRITE_UNLOCK(VN_BHV_HEAD(vp));

		repl_policy_teardown(replattr);
#ifdef DEBUG1
		cmn_err(CE_NOTE, "Failed to interpose onto 0x%x", vp);
#endif
		return;
	}
	/* Set flags before we release the lock */
	VN_FLAGSET(vp, VREPLICABLE);

	VN_BHV_WRITE_UNLOCK(VN_BHV_HEAD(vp));

	rp = REPLPOLICY_GET(replattr);
	if (rp->update) {
		(*rp->update)(bdp);
	}

	return;
}

/*
 * Dismantle the replication layer attached to this vnode.
 */
void
repl_dispose(vnode_t *vp)
{

	bhv_desc_t	*bdp;

	if (!VN_ISREPLICABLE(vp)) {
#ifdef	DEBUG
		VN_BHV_READ_LOCK(VN_BHV_HEAD(vp));
		ASSERT(vn_bhv_lookup(VN_BHV_HEAD(vp), vreplops) == 0);
		VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));
#endif	/* DEBUG */
		return;
	}

	/*
	 * Use the behavior chain lock to make the lookup
	 * and removal of the replication behavior atomic.
	 */
	VN_BHV_WRITE_LOCK(VN_BHV_HEAD(vp));

	bdp = vn_bhv_lookup(VN_BHV_HEAD(vp), vreplops);
	if (bdp) {
		VN_FLAGCLR(vp, VREPLICABLE);

		vn_bhv_remove(VN_BHV_HEAD(vp), bdp);
		VN_BHV_WRITE_UNLOCK(VN_BHV_HEAD(vp));

		repl_policy_teardown(BHV_TO_REPL(bdp));
		return;
	}
	VN_BHV_WRITE_UNLOCK(VN_BHV_HEAD(vp));
	return;
}
	

/*
 * repl_pfind:
 *	Find the page on the specified vnode.
 *	This routine gets a pointer to the pfdat found on the hash chain
 *	by system page find routine, and decides to go about checking
 *	if this is the right page to be returned to the process. 
 *	
 * 	Primarily this routine is just responsible for making sure that
 *	the vnode is in the right state for creating replicas, and 
 *	if so, invokes appropriate policy interface to create replicas.
 */
/*ARGSUSED*/
pfd_t *
repl_pfind(
	vnode_t 	*vp, 
	pgno_t 		pgno, 
	int 		acquire, 
	void 		*pmodule,
	pfd_t		*pfd)
{
	pfd_t	   	*newpfd;
	bhv_desc_t	*bdp;
	replattr_t     	*replattr;
	repl_policy_t	*rp;
	pm_t		*pm = (pm_t *)pmodule;

	/*
	 * If caller is not planning to attach the page or if the page
	 * is not ready for replication (P_DONE not set),
	 */
	if ((!(acquire & VM_ATTACH)) || !(pfd->pf_flags & P_DONE))
		return pfd;

	/*
	 * Now, the fact that this interface is called indicates the vnode
	 * has been marked replicable. But a second thread could just
	 * be in the process of shooting down replicated pages, and
	 * removing the interposition. So, watch for this.
	 * replicate.
	 */

	bdp = vn_bhv_lookup(VN_BHV_HEAD(vp), vreplops);
	ASSERT(bdp);

	replattr = BHV_TO_REPL(bdp);
	ASSERT(replattr);
	rp = REPLPOLICY_GET(replattr);

	/*
	 * Check the state of vnode replication attribute. If the replication 
	 * attribute is not "READY", then it's not a good time
	 * for creating replicas. If it's, then do whatever to hold it
	 * in that state.
	 */
	if (replattr_state_hold_rdy(replattr) != REPLOP_SUCCESS){
		/* 
		 * Something preventing us from replicating. return 
		 * current page.
		 */
		return pfd;
	}

	/* Apply replication method associated with this proces to
	 * get a new replicated page.
	 */
	ASSERT(rp->getpage);
	newpfd = (rp->getpage)(bdp, pm, pfd, acquire);

	if (pfd != newpfd){
		/* replication method returned a new page. Release
		 * reference on the one we got from previous pfind.
		 */
		pagefree(pfd);
	} else {
		/* Nothing else to do. Page we got earlier seems to be
		 * the right page for whatever reason replication policy
		 * module believs in. So, just return that.
		 */
	}
	replattr_state_release_rdy(replattr);

	ASSERT(newpfd);

	return newpfd;

}

/*
 * Given a vnode, lookup your pvnode, and hence the attribute 
 * structure.
 */ 
replattr_t *
replattr_lookup(vnode_t *vp)
{
	bhv_desc_t	*bdp;

	bdp = vn_bhv_lookup(VN_BHV_HEAD(vp), (void *)vreplops);
	ASSERT(bdp);

	return BHV_TO_REPL(bdp);
}

void
repl_vninit()
{
	/*
	 * Initialization needed as part of replication vnode operations.
	 * we just initialize our vnode operations.
	 */

	bcopy((caddr_t)vn_passthrup, (caddr_t)vreplops, sizeof(vnodeops_t));

	vreplops->vn_position.bi_position = VNODE_POSITION_REPL;
	vreplops->vop_open = repl_open;
	vreplops->vop_inactive = repl_inactive;
	vreplops->vop_reclaim  = repl_reclaim;

}

#endif /* NUMA_REPLICATION */
