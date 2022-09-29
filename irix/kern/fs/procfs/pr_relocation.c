/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995-1997 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident  "$Id: pr_relocation.c,v 1.8 1997/12/19 21:27:15 beck Exp $"

#include "pr_relocation.h"
#include "procfs_private.h"
#include "os/proc/cell/pproc_migrate.h"

extern void phfree(struct pollhead *);

/*
 * prnode relocation
 *
 * Some explanation is deserved because a number of (callout) routines
 * are involved here (in time-ordered calling sequence):
 *	prnode_proc_emigrate_start
 *		is called by the pproc relocation code during the
 *		source_prepare step to commence relocation on any procfs
 *		vnodes associated with a migrating process. It is
 *		responsible for adding any such to the object manifest.
 *	prnode_proc_immigrate_start
 *		is called by the pproc relocation code during the
 *		target_prepare step to read descriptions of any procfs
 *		vnodes from the object manifest and create embryonic
 *		vnode servers.
 *	prnode_proc_emigrate
 *		is called by the pproc relocation code during the
 *		source_bag step to package any physical prnodes.
 *	prnode_bhv_source_bag
 *		is called from the cfs behavior relocation code (itself
 *		called by KORE) to package a physical prnode.
 *		In fact, there'e nothing to do because the prnode
 *		is owned by the proc and has been bagged already.
 *	prnode_proc_immigrate
 *		is called by the pproc relocation code during the
 *		traget_unbag step to instantiate any target prnode.
 *	prnode_bhv_target_unbag
 *		is called from the cfs behavior relocation code to
 *		instantiate the target prnode. Again, there's nothing to do.
 *	prnode_proc_emigrate_end
 *		is called by the pproc relocation code during the
 *		source_end step. The destruction of any prnodes
 *		is deffered until ...
 *	prnode_bhv_source_end
 *		is called from the cfs behavior relocation code to
 *		destroy a source prnode.
 *	prnode_bhv_{source,target}_abort
 *		are called in failure cases - XXX to panic.
 */


/*
 * Internal utility routines:
 ****************************
 */

/*
 * Count the number of linked prnodes.
 */
PRIVATE int
prnode_list_count(
	prnode_t	*pnp)
{
	int	i;

	/*
	 * While counting, take an additional vnode reference.
	 */
	for (i = 0; pnp != NULL; pnp = pnp->pr_next, i++)
		VN_HOLD(pnp->pr_vnode);
	return i;
}

/*
 * Put into manifest vnodes associated with linked prnodes.
 */
PRIVATE void
prnode_list_mft_put(
	obj_manifest_t	*mftp,
	prnode_t	**pnpp)
{
	prnode_t	*pnp = *pnpp;
	int		error;

	/*
	 * Call KORE to relocate each vnode.
	 * Note that once an object is registered with KORE, the source_bag
	 * and source_end callouts will be made at the appropriate time.
	 * In the case of prnodes, however, there's little or nothing
	 * to do because prnodes are relocated along with the proc.
	 */
	while (pnp != NULL) {
		error = obj_mft_put(mftp, (void *) pnp->pr_vnode,
				    SVC_CFS, OBJ_RMODE_SERVER);
		ASSERT_NONZERO(error);
		pnp = pnp->pr_next;
	}
}

/*
 * Get from manifest a given number of vnodes and relink prnodes.
 */
PRIVATE void
prnode_list_mft_get(
	obj_manifest_t	*mftp,
#ifdef PRCLEAR
	proc_t		*p,
#endif
	prnode_t	**pnpp,
	int		npn)
{
	int		i;
	int		error;
	prnode_t	*pnp;
	vnode_t		*vp;
	bhv_desc_t	*bdp;

	/*
	 * Call KORE to instantiate the embryonic vnodes and plug them
	 * into the appropriate embryonic proc object hierarchy.
	 * Note that once registered with KORE, it will make the target_unbag
	 * callout at the appropriate time.
	 */
	for (i = npn; i > 0; i--) {
		error = obj_mft_get(mftp, (void **) &vp);
		ASSERT_NONZERO(error);

		pnp = prnode_alloc();
		*pnpp = pnp;
		pnpp = &pnp->pr_next;
		pnp->pr_vnode = vp;
#ifdef PRCLEAR
		pnp->pr_listproc = p;
#endif

		/*
		 * Insert the embryonic prnode behavior into vnode bhv chain.
		 */
		bdp = PRNODETOBHV(pnp);
		bhv_desc_init(bdp, pnp, vp, &prvnodeops);
		(void) vn_bhv_insert(VN_BHV_HEAD(vp), bdp);

	}
}

/*
 * Stuff details of prnodes in list into a bag.
 */
PRIVATE void
prnode_list_bag(
	obj_bag_t	bag,
	prnode_t	**pnpp)
{
	prnode_t	*pnp = *pnpp;
	prnode_bag_t	prnode_bag;
	int		error;

	while (pnp != NULL) {
		prnode_bag.pr_tpid      = pnp->pr_tpid;
		prnode_bag.pr_type      = pnp->pr_type;
		prnode_bag.pr_mode      = pnp->pr_mode;
		prnode_bag.pr_opens     = pnp->pr_opens;
		prnode_bag.pr_writers   = pnp->pr_writers;
		prnode_bag.pr_pflags    = pnp->pr_pflags;
	
		error = obj_bag_put(bag, OBJ_TAG_CFS_PRNODE,
				    (void *) &prnode_bag, sizeof(prnode_bag_t));
		ASSERT_NONZERO(error);

		pnp = pnp->pr_next;
	}
}

/*
 * Instantiate prnodes in a list with details from a bag.
 */
PRIVATE void
prnode_list_unbag(
	obj_bag_t	bag,
	prnode_t	**pnpp)
{
	prnode_t	*pnp = *pnpp;
	prnode_bag_t	prnode_bag;
	int		error;

	while (pnp != NULL) {
		/*
		 * Extract the expected data item from the bag.
		 */
		obj_bag_get_here(bag, OBJ_TAG_CFS_PRNODE,
				 &prnode_bag, sizeof(prnode_bag_t), error);
		ASSERT_NONZERO(error);

		pnp->pr_proc      = NULL;
		pnp->pr_pollhead  = NULL;		/* polls restart */
		pnp->pr_tpid   	  = prnode_bag.pr_tpid;
		pnp->pr_type   	  = prnode_bag.pr_type;
		pnp->pr_mode   	  = prnode_bag.pr_mode;
		pnp->pr_opens  	  = prnode_bag.pr_opens;
		pnp->pr_writers	  = prnode_bag.pr_writers;
		pnp->pr_pflags 	  = prnode_bag.pr_pflags;
	
		pnp = pnp->pr_next;
	}
}

/*
 * Hooks called from proc migration to deal with any attached prnodes.
 *********************************************************************
 */

int
prnode_proc_emigrate_start(
	proc_t		*p,		/* IN proc */
	obj_manifest_t	*mftp)		/* IN/OUT object manifest */
{
	prnode_info_t	prnode_info;
	int		error;
	int		spl;

	spl = p_lock(p);

	/*
	 * Determine what procfs vnodes we have attached to the
	 * the migrating process. Put a summary item in the manifest.
	 * This summary is a count of the prnodes - valid and invalid.
	 */
	prnode_info.ntrace   = prnode_list_count(p->p_trace);
	prnode_info.npstrace = prnode_list_count(p->p_pstrace);
	error = obj_bag_put(obj_mft_src_info(mftp), OBJ_TAG_CFS_PRNODE_INFO,
			    (void *) &prnode_info, sizeof(prnode_info));
	ASSERT_NONZERO(error);

	/*
	 * Put prnode/vnode lists into object manifest.
	 */
	prnode_list_mft_put(mftp, &p->p_trace);
	prnode_list_mft_put(mftp, &p->p_pstrace);

	p_unlock(p, spl);

	return 0;
}

int
prnode_proc_immigrate_start(
	proc_t		*p,		/* IN proc */
	obj_manifest_t	*mftp)		/* IN object manifest */
{
	prnode_info_t	prnode_info;
	int		error;

	/*
	 * Fetch the item from the manifest summarizing what, if any,
	 * procfs vnode are to be relocated.
	 */
	obj_bag_get_here(obj_mft_src_info(mftp), OBJ_TAG_CFS_PRNODE_INFO,
			 (void *) &prnode_info, sizeof(prnode_info), error);
	ASSERT_NONZERO(error);

	/*
	 * Re-create lists of embryonic prnodes/vnodes.
	 * No locking deemed necssary at this point.
	 */
#ifdef PRCLEAR
	prnode_list_mft_get(mftp, p, &p->p_trace, prnode_info.ntrace);
	prnode_list_mft_get(mftp, p, &p->p_pstrace, prnode_info.npstrace);
#else
	prnode_list_mft_get(mftp, &p->p_trace, prnode_info.ntrace);
	prnode_list_mft_get(mftp, &p->p_pstrace, prnode_info.npstrace);
#endif

	return 0;
}

void
prnode_proc_emigrate(
	proc_t		*p,		/* IN proc */
	obj_bag_t	bag)		/* IN object bag */
{
	int		spl;

	spl = p_lock(p);		/* XXX - necessary? */

	/*
	 * Package up prnodes.
	 */
	prnode_list_bag(bag, &p->p_trace);
	prnode_list_bag(bag, &p->p_pstrace);

	p_unlock(p, spl);

}

void
prnode_proc_immigrate(
	proc_t		*p,		/* IN proc */
	obj_bag_t	bag)		/* IN object bag */
{
	/*
	 * Unpackage prnodes.
	 */
	prnode_list_unbag(bag, &p->p_trace);
	prnode_list_unbag(bag, &p->p_pstrace);
}

void
prnode_proc_emigrate_end(
	proc_t		*p)		/* IN proc */
{
	p->p_trace = NULL;
	p->p_pstrace = NULL;

	/* We leave it to the cfs bhv callout to destroy the prnodes. */
	return;
}


/*
 * Callout routines invoked from CFS behavior-dependent code.
 ************************************************************
 */

/*
 * Package up a prnode.
 * This has mostly been done by the proc bagging.
 */
/* ARGSUSED */
int
prnode_bhv_source_bag(
	bhv_desc_t	*bdp,		/* IN bhv descriptor */
	obj_bag_t	bag)		/* IN object bag */
{
	prnode_t	*pnp;
	struct pollhead	*php;

	ASSERT(BHV_IDENTITY(bdp)->bi_id == VN_BHV_PR);
	ASSERT(BHV_IDENTITY(bdp)->bi_position == VNODE_POSITION_BASE);

	DVN_KTRACE4(DVN_TRACE_RELOCATE, "prnode_bhv_source_bag",
		BHV_TO_VNODE(bdp), "bhv", bdp);

	pnp = BHVTOPRNODE(bdp);
	

	/*
	 * Check for pending poll/selects on this prnode.
	 * If so, we must wakeup all interested threads and cause them to
	 * delete their polldata entries from this pollhead.
	 * These threads will retry their VOP_POLL and be sent to the target
	 * cell where they will block till we're done.
	 */
	php = pnp->pr_pollhead;
	if (php != NULL) {
		/*
		 * Restart polls and wait for poll list to emptied.
		 */
		pollrestart(php);
		while (php->ph_list != NULL) {
			delay(1);
		}
		ASSERT(!php->ph_next);

		phfree(php);
		pnp->pr_pollhead = NULL;
        }

	return 0;
}

/*
 * Unbag and re-instantiate a prnode for given vnode from given bag
 * ... but this was done early by the owning proc.
 */
/* ARGSUSED */
int
prnode_bhv_target_unbag(
	vnode_t		*vp,		/* IN vnode */
	obj_bag_t	bag)		/* IN object bag */
{

	DVN_KTRACE2(DVN_TRACE_RELOCATE, "prnode_bhv_target_unbag", vp);

	return 0;
}

/*
 * Relocation complete for a prnode - destroy it.
 * We don't bother about neatly removing from the prnode list
 * rooted in the proc since the proc exists no longer.
 * Nor do we remove the prnode from the behavior chain since
 * it is no longer attached to the vnode.
 */
int
prnode_bhv_source_end(
	bhv_desc_t	*bdp)		/* IN bhv descriptor */
{
	prnode_t	*pnp;
	vnode_t		*vp;

	ASSERT(BHV_IDENTITY(bdp)->bi_id == VN_BHV_PR);
	ASSERT(BHV_IDENTITY(bdp)->bi_position == VNODE_POSITION_BASE);

	vp = BHV_TO_VNODE(bdp);
	DVN_KTRACE4(DVN_TRACE_RELOCATE, "prnode_bhv_source_end", vp,
		    "bhv", bdp);

	pnp = BHVTOPRNODE(bdp);
	
	ASSERT(pnp->pr_pollhead == NULL);
	pnp->pr_next = NULL;
#ifdef PRCLEAR
	pnp->pr_listproc = NULL;
#endif
	prnode_free(pnp);

	VN_RELE(vp);			/* hold taken in source prepare */

	return 0;
}

/*
 * Relocation of given behavior failed - fix it up.
 */
int
prnode_bhv_source_abort(
	bhv_desc_t	*bdp)		/* IN bhv descriptor */
{
	panic("prnode_bhv_source_abort: bdp 0x%x", bdp);
	/* NOTREACHED */
}

/*
 * Relocation of given behavior failed - destroy it.
 */
int
prnode_bhv_target_abort(
	bhv_desc_t	*bdp)		/* IN bhv descriptor */
{
	panic("prnode_bhv_target_abort: bdp 0x%x", bdp);
	/* NOTREACHED */
}

/*
 * Callout table exported to cfs_relocation.
 */
cfs_vn_kore_if_t prnode_obj_bhv_iface = {
	prnode_bhv_source_bag,
	prnode_bhv_target_unbag,
	prnode_bhv_source_end,
	prnode_bhv_source_abort,
	prnode_bhv_target_abort
};
