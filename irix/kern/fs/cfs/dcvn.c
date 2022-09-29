/**************************************************************************
 *									  *
 *		 Copyright (C) 1995-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident	"$Id: dcvn.c,v 1.11 1997/09/03 22:42:56 rcc Exp $"

/*
 * Client-side vnode message interfaces for the Cell File System.
 */
#include "dvn.h"
#include "invk_dsvn_stubs.h"
#include "I_dcvn_stubs.h"
#include <sys/fs_subr.h>
#include <fs/cell/cxfs_intfc.h>

/*
 * Called from server via a message when the server is broadcasting
 * vnode changes.
 */
void
I_dcvn_vnode_change(
        cfs_handle_t	*handlep,
        vchange_t	cmd,
	__psint_t	val,
	int		*error)
{
	dcvn_t		*dcp;

	/*
	 * Map from handle to dcvn_t, getting a reference to the
	 * associated vnode.
	 */
	DCSTAT(I_dcvn_vnode_change);
	dcp = dcvn_handle_lookup(handlep);
	if (dcp != NULL) {
		fs_vnode_change(DCVN_TO_BHV(dcp), cmd, val);
		VN_RELE(DCVN_TO_VNODE(dcp));		/* ref. taken by dcvn_handle_lookup */
		*error = 0;
	} else {
		/*
		 * Same races are possible here as in I_dcvn_recall.
		 */
		*error = EINVAL;
	}
	DVN_KTRACE8(DVN_TRACE_DCRPC, "I_dcvn_vnode_change", dcp, "error", *error,
		"cmd", cmd, "val", val);
}

/*
 * Called from server via a message when the server is broadcasting
 * page flush/invalidate requests.
 */

void
I_dcvn_page_cache_operation(
        cfs_handle_t	*handlep,
	int		opcode,
	off_t		first,
	off_t		last,
	uint64_t	flags,
	int		fiopt,
	int		*error)
{
	dcvn_t		*dcp;
	vnode_t		*vp;

	DCSTAT(I_dcvn_page_cache_op);
	/*
	 * Map from handle to dcvn_t, getting a reference to the
	 * associated vnode.
	 */
	dcp = dcvn_handle_lookup(handlep);

	if (dcp != NULL) {
		vp = DCVN_TO_VNODE(dcp);
		*error = 0;
		if (vp->v_pgcnt) {
			int		do_remapf;

			do_remapf = ((fiopt == FI_REMAPF || fiopt == FI_REMAPF_LOCKED)
				&& VN_MAPPED(vp));
	
			if (do_remapf && opcode != CFS_TOSS)
				remapf(vp, 0, 1);

			if (opcode == CFS_FLUSHINVAL)
				vnode_flushinval_pages(vp, first, last);
			else if (opcode == CFS_FLUSH)
				vnode_flush_pages(vp, first, last, flags);
			else if (opcode == CFS_TOSS)
				vnode_tosspages(vp, first, last);
			else if (opcode == CFS_INVALFREE)
				vnode_invalfree_pages(vp, first);

			if (do_remapf && opcode == CFS_TOSS)
				remapf(vp, 0, 0);

		}

		/*
		 * If the client has no pages in the page cache, return the
		 * PCACHE token. This will make the server stop sending requests.
		 */
		if (vp->v_pgcnt == 0 && dcp->dcv_empty_pcache_cnt++ >= DSV_EMPTY_PCACHE_RECALL_THRESHOLD) 
			tkc_recall(dcp->dcv_tclient, DVN_PCACHE_READ, TK_DISP_CLIENT_ALL);

		VN_RELE(vp);		/* ref. taken by dcvn_handle_lookup */
	} else {
		/*
		 * Same races are possible here as in I_dcvn_recall.
		 */
		*error = EINVAL;
	}
	DVN_KTRACE8(DVN_TRACE_DCRPC, "I_dcvn_page_cache_operation", dcp, "error", *error,
		"first", first, "last", last);
}

void
dcvn_count_recall(
	tk_set_t recset)
{
	if (TK_GET_CLASS(recset, DVN_EXIST_NUM)) DCSTAT(DVN_EXIST_recall);
	if (TK_GET_CLASS(recset, DVN_PCACHE_NUM)) DCSTAT(DVN_PCACHE_recall);
	if (TK_GET_CLASS(recset, DVN_PSEARCH_NUM)) DCSTAT(DVN_PSEARCH_recall);
	if (TK_GET_CLASS(recset, DVN_NAME_NUM)) DCSTAT(DVN_NAME_recall);
	if (TK_GET_CLASS(recset, DVN_ATTR_NUM)) DCSTAT(DVN_ATTR_recall);
	if (TK_GET_CLASS(recset, DVN_TIMES_NUM)) DCSTAT(DVN_TIMES_recall);
	if (TK_GET_CLASS(recset, DVN_EXTENT_NUM)) DCSTAT(DVN_EXTENT_recall);
	if (TK_GET_CLASS(recset, DVN_BIT_NUM)) DCSTAT(DVN_BIT_recall);
	if (TK_GET_CLASS(recset, DVN_DIT_NUM)) DCSTAT(DVN_DIT_recall);
}
/*
 * Called from server via a message to recall tokens.
 */
void
I_dcvn_recall(
        cfs_handle_t	*handlep,
        tk_set_t	recset,
	tk_disp_t	why)
{
	dcvn_t		*dcp;
	vnode_t		*vp;

	dcvn_count_recall(recset);

	/*
	 * Map from handle to dcvn_t, getting a reference to the
	 * associated vnode.
	 */
	dcp = dcvn_handle_lookup(handlep);
	if (dcp == NULL) {
		/* REFERENCED */
		int	msgerr;

		/*
		 * Two possible races can cause this:
		 *
		 * 1) We returned (or, are returning) the existence token 
		 *    simultaneous with the server trying to recall tokens.
		 *
		 * 2) The recall message from the server raced ahead
		 *    of a message containing the existence token (either
		 *    a lookup reply or an obtain_exist reply).
		 *
		 * Both cases are handled by sending a "not found" msg
		 * to the server.  Note that in case 1) the server must
		 * be prepared to receive a not found msg for a handle
		 * that refers to a vnode that's been recycled.
		 */
		msgerr = invk_dsvn_notfound(CFS_HANDLE_TO_SERVICE(*handlep),
				   handlep, cellid(), recset, why);
		ASSERT(!msgerr);
		return;
	}

	/*
	 * Let CXFS refuse any tokens first. It will return whatever
	 * tokens should be recalled by CFS.
	 */

	if (dcp->dcv_dcxvn)
		recset = cxfs_dcxvn_recall(dcp->dcv_dcxvn, recset, why);

	/*
	 * If recalling the existence token then the file is completely
	 * removed from the name space (zero link count).
	 *
	 * Ideally, we'd like to force inactive/reclaim when we do our
	 * vn_rele but we can only do that if we currently hold the only
	 * vnode reference, and we ensure that no new refs. can be obtained
	 * from vn_get.  If we know inactive processing will get initiated by
	 * our vn_rele then we know all the vnode's tokens will be returned.  
	 * Otherwise, we must refuse to return the existence token.  The
	 * server is expecting some form of reply (return or refuse), 
	 * because that allows the remove operation to complete.
	 * 
	 * In the case where we can't immediately return the existence token,
	 * we guarantee the server that all tokens will be returned when the 
	 * vnode is eventually inactivated.
	 */

	vp = DCVN_TO_VNODE(dcp);
	if (TK_IS_IN_SET(recset, DVN_EXIST_READ)) {
		/*
		 * Make sure the vnode is torn down at inactive time.
		 */
		VN_FLAGSET(vp, VINACTIVE_TEARDOWN);

		/*
		 * If vn_evict succeeds, we know for sure that inactive 
		 * processing will be initiated by our vn_rele.
		 */
		if (vn_evict(vp)) {
			/* REFERENCED */
			int	msgerr;

			/* 
			 * Didn't succeed - send refusal message.
			 * No need to set DVN_LINKZERO because the
			 * VINACTIVE_TEARDOWN flag is set in the vnode.
			 */
			msgerr = invk_dsvn_refuse(DCVN_TO_SERVICE(dcp), 
					 DCVN_TO_OBJID(dcp),
					 cellid(), DVN_EXIST_READ, why);
			ASSERT(!msgerr);
			recset = TK_SUB_SET(recset, DVN_EXIST_READ);
		}
	}

	/* 
	 * If no pages are in the page cache, try to return the DVN_PCACHE_READ
	 * token as well. This could reduce future RPCs.
	 */

	if (vp->v_pgcnt == 0 &&
		(dcp->dcv_empty_pcache_cnt++ >= DSV_EMPTY_PCACHE_RECALL_THRESHOLD)) {
		if (!TK_IS_IN_SET(recset, DVN_PCACHE_READ)) {
			recset = TK_ADD_SET(recset, DVN_PCACHE_READ);
			why = TK_ADD_DISP(why,
				TK_MAKE_DISP(DVN_PCACHE_NUM, TK_CLIENT_INITIATED));
		}
	}

	if (recset != TK_NULLSET)
		tkc_recall(dcp->dcv_tclient, recset, why);

	DVN_KTRACE10(DVN_TRACE_DCRPC, "I_dcvn_recall", dcp, "recset", recset,
		"why", why, "vp", vp, "vp->v_pgcnt", vp->v_pgcnt);
	VN_RELE(vp);		/* ref. taken by dcvn_handle_lookup */
}

/*
 * The following is called to atomically update the time in the
 * vattr within dcvn. The int should be one of AT_ATIME, AT_CTIME,
 * or AT_MTIME.
 */

void
cfs_dcvn_set_times(
	dcvn_t *dcp,
	timespec_t *stimep,
	int flag)
{
	timespec_t	*dtimep;
	lock_t		*lp;
	int		s, ds_flag = 0;

	DCSTAT(cfs_dcvn_set_times);
	ASSERT(dcp && stimep && flag);
	lp = &dcp->dcv_lock;

	switch(flag) {
	case AT_ATIME:
		dtimep = &dcp->dcv_vattr.va_atime;
		ds_flag = DVN_ATIME_MOD;
		break;

	case AT_MTIME:
		dtimep = &dcp->dcv_vattr.va_mtime;
		ds_flag = DVN_MTIME_MOD;
		break;

	case AT_CTIME:
		dtimep = &dcp->dcv_vattr.va_ctime;
		ds_flag = DVN_CTIME_MOD;
		break;

	default:
		printf("cfs_dcvn_set_times: bad flag %d\n", flag);
		goto out;
	}
	s = mutex_spinlock(lp);
	dcp->dcv_flags |= ds_flag;
	dtimep->tv_sec = stimep->tv_sec;
	dtimep->tv_nsec = stimep->tv_nsec;
	mutex_spinunlock(lp, s);
out:
	DVN_KTRACE8(DVN_TRACE_DCRPC, "cfs_dcvn_set_times", dcp, "stimep", stimep,
		"ds_flag", ds_flag, "flag", flag);
}
