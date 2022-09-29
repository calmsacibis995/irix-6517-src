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
#ident	"$Id: dcvnops.c,v 1.90 1997/09/05 15:55:27 mostek Exp $"

/*
 * Client-side vnode ops for the Cell File System.
 */
#include <sys/fs_subr.h>
#include <sys/vnode.h>
#include <sys/vnode_private.h>
#include <sys/buf.h>
#include <sys/fcntl.h>
#include <sys/flock.h>
#include <sys/pathname.h>
#include <sys/uthread.h>
#include <limits.h>
#include <ksys/vop_backdoor.h>
#include <sys/pfdat.h>
#include <sys/alenlist.h>
#include <sys/numa.h>
#include <sys/pmo.h>
#include <sys/mmci.h>
#include "dvn.h"
#include "dvfs.h"
#include "cfs_relocation.h"
#include "invk_dsvn_stubs.h"
#include <fs/cell/cxfs_intfc.h>
#include <sys/mac_label.h>
#include <sys/acl.h>
#include <fs/specfs/spec_lsnode.h>


static void	dcvn_teardown(dcvn_t *);
static int 	dcvn_page_read(bhv_desc_t *, uio_t *, int, cred_t *, flid_t *);
static int	dcvn_create_int(bhv_desc_t *, char *, vattr_t *, int,
				int, vnode_t **, cred_t *);
static void 	dcvn_tcif_obtain(void *, tk_set_t, tk_set_t, tk_disp_t,
				 tk_set_t *);
static void 	dcvn_tcif_return(tkc_state_t, void *, tk_set_t, tk_set_t,
				 tk_disp_t);

tkc_ifstate_t dcvn_tclient_iface = {
	dcvn_tcif_obtain,
	dcvn_tcif_return
};

extern external_alenlist_t uio_to_plist (uio_t *, int, int *, struct iovec *,
	int *);
extern void plist_free (int, int, struct iovec *);
void dc_putobjects(tk_set_t, vattr_t *, vattr_t *);

/*
 * Internal routine to copy a specific page back to server.
 * ZZZ - should this be a generic VOP routine?
 * ZZZ - will other file systems use it??
 */
/* ARGSUSED */
int
dcvn_update_page_flags(
        dcvn_t		*dcp,
	int		maxcount,
	int		flags)
{
	int		s;
	int		unchained=0;
	int		reqcount;
	int		count;
	vnode_t		*vp;
	pfd_t		*pfd;
	pfn_t		pfnlist[DSVN_PFN_LIST_SIZE];

	vp = DCVN_TO_VNODE(dcp);

again:
        /*
	 * Build a list of dirty pages. Note that P_BAD pages are simply 
	 * freed - no need to tell the server about them.
	 */
	reqcount = MIN(DSVN_PFN_LIST_SIZE, maxcount);
	count=0;	
	s = VN_LOCK(vp);
	while ( (pfd = vp->v_dpages) && count < reqcount) {
		vp->v_dpages = pfd->pf_vchain;
		pageflags(pfd, P_DQUEUE, 0);

		ASSERT(pfd->pf_vp == vp);
		ASSERT(pfd->pf_flags & P_DIRTY);
		unchained++;

		if (pfd->pf_flags&P_BAD) {
			VN_UNLOCK(vp, s);
			pagefree(pfd);
			s = VN_LOCK(vp);
		} else {
			pfnlist[count] = pfdattopfn(pfd);
			count++;
		}
	}
	VN_UNLOCK(vp, s);

	/*
	 * Now pass the list of dirty pages to the server
	 */
	if (count) {
		/* REFERENCED */
		int	msgerr;

		maxcount -= count;
		msgerr = invk_dsvn_update_page_flags (DCVN_TO_SERVICE(dcp), 
			DCVN_TO_OBJID(dcp), pfnlist, count);
		ASSERT(!msgerr);
	}

	/*
	 * Drop the extra reference to the page. This must be done
	 * AFTER the server updates the page dirty status.
	 */
	for (count--; count >= 0; count--)
		pagefree(pfntopfdat(pfnlist[count]));

	if (maxcount && vp->v_dpages)
		goto again;

	return(unchained);
}


void
dcvn_count_return(
	tk_set_t retset)
{
	if (TK_GET_CLASS(retset, DVN_EXIST_NUM)) DCSTAT(DVN_EXIST_return);
	if (TK_GET_CLASS(retset, DVN_PCACHE_NUM)) DCSTAT(DVN_PCACHE_return);
	if (TK_GET_CLASS(retset, DVN_PSEARCH_NUM)) DCSTAT(DVN_PSEARCH_return);
	if (TK_GET_CLASS(retset, DVN_NAME_NUM)) DCSTAT(DVN_NAME_return);
	if (TK_GET_CLASS(retset, DVN_ATTR_NUM)) DCSTAT(DVN_ATTR_return);
	if (TK_GET_CLASS(retset, DVN_TIMES_NUM)) DCSTAT(DVN_TIMES_return);
	if (TK_GET_CLASS(retset, DVN_EXTENT_NUM)) DCSTAT(DVN_EXTENT_return);
	if (TK_GET_CLASS(retset, DVN_BIT_NUM)) DCSTAT(DVN_BIT_return);
	if (TK_GET_CLASS(retset, DVN_DIT_NUM)) DCSTAT(DVN_DIT_return);
}

/*
 * Internal routine to return tokens.
 */
void
dcvn_return(
        dcvn_t		*dcp,
	tk_set_t	retset,
	tk_set_t	unknownset,
	tk_disp_t	why)
{
	vnode_t		*vp;
	/* REFERENCED */
	int		msgerr;
	ds_times_t	times;
	vattr_t		*dvap;

	vp = DCVN_TO_VNODE(dcp);
	if (retset == TK_NULLSET && unknownset == TK_NULLSET)
		return;

	dcvn_count_return(retset);
	/*
	 * Shutoff access to the page cache by setting VREMAPPING.
	 * If returning page cache  or existance tokens, flush all the
	 * pages back to the server.
	 */
	if (TK_IS_IN_SET(retset, DVN_PSEARCH_READ))
		VN_FLAGSET(vp, VREMAPPING);

	if (TK_IS_IN_SET(retset, TK_ADD_SET(DVN_EXIST_READ, DVN_PCACHE_READ))) {
		if(VN_MAPPED(vp))
			remapf(vp, 0, 1);
		vnode_flushinval_pages(vp, 0, LONG_MAX);
	}

	if (dcp->dcv_dcxvn)
		cxfs_dcxvn_return(dcp->dcv_dcxvn, retset, unknownset, why);

	/*
	 * Return our times if our shared write tokens is being revoked.
	 */

	if (TK_IS_IN_SET(retset, DVN_TIMES_SHARED_WRITE)) {
		/*
		 * No need to lock the dcvn here since noone can modify
		 * the flag or times after the tkc module calls us
		 * since everyone has release the shared write token.
		 */
		dvap = &dcp->dcv_vattr;
		times.ds_flags = 0;
		if (dcp->dcv_flags & DVN_ATIME_MOD) {
			times.ds_flags |= AT_ATIME;
			times.ds_atime.tv_sec = dvap->va_atime.tv_sec;
			times.ds_atime.tv_nsec = dvap->va_atime.tv_nsec;
		}
		if (dcp->dcv_flags & DVN_MTIME_MOD) {
			times.ds_flags |= AT_MTIME;
			times.ds_mtime.tv_sec = dvap->va_mtime.tv_sec;
			times.ds_mtime.tv_nsec = dvap->va_mtime.tv_nsec;
		}
		if (dcp->dcv_flags & DVN_CTIME_MOD) {
			times.ds_flags |= AT_CTIME;
			times.ds_ctime.tv_sec = dvap->va_ctime.tv_sec;
			times.ds_ctime.tv_nsec = dvap->va_ctime.tv_nsec;
		}
	}

	/*
	 * Before returning the existence token, remove the dcvn from
	 * hash table.  Otherwise, there's a possibility the server will
	 * reuse the handle and we could attempt to enter a duplicate entry.
	 */
	if (TK_IS_IN_SET(retset, DVN_EXIST_READ)) {
		dcvn_handle_remove(dcp);
		dcvfs_dcvn_delete(dcp, dcp->dcv_dcvfs);
	}

	msgerr = invk_dsvn_return(DCVN_TO_SERVICE(dcp), DCVN_TO_OBJID(dcp),
			 cellid(), retset, unknownset, why, &times);
	ASSERT(!msgerr);
	tkc_returned(dcp->dcv_tclient, retset, TK_NULLSET);
}

/*
 * Internal routine to teardown an installed dcvn.
 */
void
dcvn_teardown(
	dcvn_t		*dcp)
{
	tk_set_t	retset;
	tk_disp_t	why;

	DCSTAT(dcvn_teardown);
	/*
	 * Return tokens.
	 *
	 * Before returning the existence token, remove the dcvn from
	 * hash table.  Otherwise, there's a possibility the server will
	 * reuse the handle and we could attempt to enter a duplicate entry.
	 */
	tkc_returning(dcp->dcv_tclient, TK_SET_ALL, &retset, &why, 0); 
	DVN_KTRACE4(DVN_TRACE_RECYCLE, "dcvn_teardown", dcp, "retset", retset);

	/*
	 * If the existence token has already been set then the VEVICT
	 * flag must be set.
	 */
	ASSERT(!(retset & DVN_EXIST_READ) ? 
	       (DCVN_TO_VNODE(dcp)->v_flag & VEVICT) : 1);

	if (retset != TK_NULLSET)
		dcvn_return(dcp, retset, TK_NULLSET, why);

	/*
	 * Teardown token client and other data structures.
	 */
	vn_bhv_remove(VN_BHV_HEAD(DCVN_TO_VNODE(dcp)), DCVN_TO_BHV(dcp));
	dcvn_destroy(dcp);
}

/*
 * Open vnode op.
 */
/* ARGSUSED */
STATIC int
dcvn_open(
	bhv_desc_t 	*bdp,
	vnode_t		**vpp,
	mode_t		mode,
	cred_t		*credp)
{
	dcvn_t		*dcp = BHV_TO_DCVN(bdp);
	dcvn_t		*newdcp;
	cfs_handle_t	handle;
	tk_set_t	obtset;
	enum vtype	vtype;
	dev_t		rdev;
	int		flags, try_cnt, error;


	DCSTAT(dcvn_open);
	/*
	 * Send an open RPC to the server.
	 *
	 * Note that for the case of the server returning a different vnode,
	 * multiple threads racing to obtain an existence token, or a thread
	 * obtaining racing a thread returning an existence token, requires
	 * that this code loop, checking a hash table and contacting the
	 * server.  Backoff each time through the loop.
	 *
	 * XXX Consider optimizing this routine for underlying file systems
	 * that are guaranteed _not_ to return a different vnode.
	 */
	try_cnt = 0;
	newdcp = NULL;
	do {
		/* REFERENCED */
		int	msgerr;

		/*
		 * Send the RPC.
		 */
		DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_open", dcp, "try_cnt", try_cnt);
		msgerr = invk_dsvn_open(DCVN_TO_SERVICE(dcp),
				DCVN_TO_OBJID(dcp), 
				mode, CRED_GETID(credp), cellid(), &handle,
				&obtset, &vtype, &rdev, &flags, &error);
		ASSERT(!msgerr);
		if (error)
			break;

		if (CFS_HANDLE_IS_NULL(handle)) {
			/*
			 * Server didn't exchange vnodes - all done.
			 */
			if (newdcp != NULL) {
				spinlock_destroy((&newdcp->dcv_lock));
				kmem_zone_free(dcvn_zone, newdcp);
			}
			return 0;
		}

		/*
		 * Now we're in the case where the server changed the
		 * vnode on us.
		 *
		 * Map from a handle to a new dcvn.  If ERESTART is
		 * returned then we're racing and must retry.
		 *
		 * Note: this call can change the value of newdcp.
		 */
		error = dcvn_handle_to_dcvn(&handle, obtset, vtype, rdev,
					    dcp->dcv_dcvfs, flags,
					    0, (char *)0, 0, &newdcp);

		ASSERT(!error || error == ERESTART);
		if (!error)
			break;

		/* backoff to give others a chance to run */
		cell_backoff(++try_cnt);

		obtset = TK_NULLSET;		/* reset */
	} while (1);

	if (!error) {
		ASSERT(newdcp != NULL);
		*vpp = DCVN_TO_VNODE(newdcp);
		VN_RELE(BHV_TO_VNODE(bdp));	/* release ref to old vp */
	} else if (newdcp != NULL) {
		spinlock_destroy((&newdcp->dcv_lock));
		kmem_zone_free(dcvn_zone, newdcp);
	}

	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_open", newdcp, "error", error);
	return error;
}

/*
 * Close vnode op.
 */
STATIC int
dcvn_close(
	bhv_desc_t	*bdp,
	int		flag,
	lastclose_t	lastclose,
	cred_t		*credp)
{
	dcvn_t		*dcp = BHV_TO_DCVN(bdp);
	int		error;
	/* REFERENCED */
	int		msgerr;

	DCSTAT(dcvn_close);

	msgerr = invk_dsvn_close(DCVN_TO_SERVICE(dcp), DCVN_TO_OBJID(dcp), 
			flag, lastclose, CRED_GETID(credp), &error);
	ASSERT(!msgerr);
	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_close", dcp, "error", error);
	return error;
}

/*
 * Read vnode op.
 */
STATIC int
dcvn_read(
	bhv_desc_t	*bdp,
	uio_t		*uiop,
	int		ioflag,
	cred_t		*credp,
	flid_t		*fl)
{
	dcvn_t		*dcp = BHV_TO_DCVN(bdp);
	int		error;
	off_t		uio_offset;
	short		uio_sigpipe;
	ssize_t		uio_resid;
	/* REFERENCED */
	external_alenlist_t	dcalist;
	/* REFERENCED */
	int		dcalen;
	/* REFERENCED */
	struct iovec	saved_iov[16];
	/* REFERENCED */
	vnode_t		*vp;
	/* REFERENCED */
	int		msgerr;

	DCSTAT(dcvn_read);

	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_read", dcp, "uiop", uiop);
	if (uiop->uio_segflg == UIO_NOSPACE && !(ioflag&IO_DIRECT))
		return(dcvn_page_read(bdp, uiop, ioflag, credp, fl));

	uiop->uio_pmp = NULL;
	ASSERT(uiop->uio_pbuf == NULL);
	ASSERT(credp->cr_mac == NULL);
	ASSERT(uiop->uio_iovcnt <= 16);

	vp = BHV_TO_VNODE(bdp);
#ifdef NOTYET
	if (!IS_KSEG0(uiop->uio_iov[0].iov_base)
	     && ((vp->v_type) == VREG || (vp->v_type == VDIR))) {
		dcalist = uio_to_plist (uiop, B_READ, &dcalen, saved_iov, 
			&error);
		if (dcalist == NULL) {
			return(error);
		}

		msgerr = invk_dsvn_list_read(DCVN_TO_SERVICE(dcp),
				DCVN_TO_OBJID(dcp), 
				uiop, uiop->uio_iov, uiop->uio_iovcnt, 
				dcalist,
				(dcalen/sizeof(struct external_alenlist)),
				ioflag, CRED_GETID(credp), fl, 
				&uio_offset, &uio_sigpipe, &uio_resid, &error);
		ASSERT(!msgerr);

		if ((uiop->uio_segflg != UIO_SYSSPACE) &&
		    (uiop->uio_segflg != UIO_NOSPACE)) {
			plist_free (uiop->uio_iovcnt, B_READ|B_PHYS, 
				saved_iov);
		}
		kmem_free (dcalist, dcalen);

	} else {
#endif
		msgerr = invk_dsvn_read(DCVN_TO_SERVICE(dcp),
				DCVN_TO_OBJID(dcp),
				uiop, uiop->uio_iov, uiop->uio_iovcnt, ioflag, 
				CRED_GETID(credp),
				fl, &uio_offset, &uio_sigpipe, &uio_resid,
				&error);
		ASSERT(!msgerr);
#ifdef NOTYET
	}
#endif
	
	uiop->uio_offset = uio_offset;
	uiop->uio_sigpipe = uio_sigpipe;
	uiop->uio_resid = uio_resid;

	return error;
}


/*
 * Read vnode op.
 */
STATIC int
dcvn_page_read(
	bhv_desc_t	*bdp,
	uio_t		*uiop,
	int		ioflag,
	cred_t		*credp,
	flid_t		*fl)
{
	dcvn_t		*dcp = BHV_TO_DCVN(bdp);
	tk_set_t	tkacq;
	int		error = 0;
	pfd_t		*pfd, *tpfd;
	vnode_t		*vp;
	pfn_t		paddr=0;
	uint		pagekey;
	uint		vrgen;
	int		flags;
	int		already_imported;
	pgno_t		lpn;
	/* REFERENCED */
	size_t		pagesize;
	/* REFERENCED */
	int		msgerr;

	DCSTAT(dcvn_page_read);
	vp = BHV_TO_VNODE(bdp);
	lpn = btoc(uiop->uio_offset);
	uiop->uio_sigpipe = 0;
	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_page_read", dcp, "lpn", lpn);

	/*
	 * If VREMAPPING is set, the pages may exist but were not seen by VM.
	 * Wait for the remap operation to finish & check the page cache again - 
	 * the page may be found.
	 */
	if (vp->v_pgcnt && vp->v_flag&VREMAPPING) {
		tkc_acquire (dcp->dcv_tclient, DVN_PCACHE_PSEARCH_READ, &tkacq);
		ASSERT (DVN_PCACHE_PSEARCH_READ == tkacq);
		pfd = pfind(vp, lpn, 0);
		tkc_release (dcp->dcv_tclient, DVN_PCACHE_PSEARCH_READ);
		if (pfd) {
			uiop->uio_resid = 0;
			goto done;
		}
	}

	/* 
	 * We should look at the policy module & decide whether to
	 * use large pages.
	 */
	pagekey = VKEY(vp) + lpn;
	pagesize = NBPP;

	/*
	 * ZZZ - probably cant do this without large page support on
	 * export/import.
	 */
	uiop->uio_pmp = NULL;		/* ZZZ - not yet */
	if (uiop->uio_pmp)
		tpfd = PM_PAGEALLOC(uiop->uio_pmp, pagekey, VM_MVOK, &pagesize, (caddr_t)lpn);
	else
		tpfd = pagealloc_rr(pagekey, VM_MVOK);

	/*
	 * If we cant get any memory, call setsxbrk. Then return to VM. It will
	 * probably just reissue the read request.
	 */
	if (tpfd == NULL) {
		setsxbrk();
		uiop->uio_resid = 0;
		goto done;
	}

	dcp->dcv_empty_pcache_cnt = 0;
	tkc_acquire (dcp->dcv_tclient, DVN_PCACHE_PSEARCH_READ, &tkacq);
	ASSERT (DVN_PCACHE_PSEARCH_READ == tkacq);
	vrgen = dcp->dcv_vrgen;

	pfd = pinsert_try(tpfd, (void*)vp, lpn);
	if (pfd != tpfd) {
		pagefree(pfd);
		tkc_release (dcp->dcv_tclient, DVN_PCACHE_PSEARCH_READ);
		uiop->uio_resid = 0;
		goto done;
	}
	paddr = pfdattophys(pfd);
	tkc_release (dcp->dcv_tclient, DVN_PCACHE_PSEARCH_READ);

	msgerr = invk_dsvn_page_read(DCVN_TO_SERVICE(dcp), DCVN_TO_OBJID(dcp), 
	       uiop, ioflag, CRED_GETID(credp), fl, paddr, cellid(), 
	       &paddr, &flags, &uiop->uio_resid, &already_imported, &error);
	ASSERT(!msgerr);


	tkc_acquire (dcp->dcv_tclient, DVN_PCACHE_PSEARCH_READ, &tkacq);
	ASSERT (DVN_PCACHE_PSEARCH_READ == tkacq);
	if (error || uiop->uio_resid == NBPP) {
		ASSERT(paddr == 0);
		pagebad(pfd);
		if (error == EAGAIN)
			error = 0;
	} else if (paddr) {
		/*
		 * Import a page from the server & discard the page we assigned.
		 */ 
		tpfd = import_page(paddr, SERVICE_TO_CELL(DCVN_TO_SERVICE(dcp)), 
				already_imported);
		if (tpfd) {
			ASSERT(pfdattopfn(tpfd) == paddr && tpfd->pf_use >0);
			if (flags&P_HOLE) 
				tpfd->pf_flags |= P_HOLE;
			if (dcp->dcv_vrgen != vrgen)
				tpfd->pf_flags |= P_BAD;
			pfd = preplace(pfd, tpfd);
		} else {
			pagebad(pfd);
		}
	} else {
		/*
		 * Page assigned by client has been exported to the server. Update
		 * any page status returned from the server.
		 */
		if (flags&P_HOLE) 
			pageflags(pfd, P_HOLE, 1);
	}

	pagedone(pfd);
	pagefree(pfd);
	tkc_release (dcp->dcv_tclient, DVN_PCACHE_PSEARCH_READ);

done:
	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_page_read", dcp, "error", error);
	return error;
}

/*
 * Write vnode op.
 */
STATIC int
dcvn_write(
	bhv_desc_t 	*bdp,
	uio_t		*uiop,
	int		ioflag,
	cred_t		*credp,
	flid_t		*fl)
{
	dcvn_t		*dcp = BHV_TO_DCVN(bdp);
	int		error;
	/* REFERENCED */
	int		msgerr;
	off_t		uio_offset;
	short		uio_sigpipe;
	ssize_t		uio_resid;
#ifdef NOTYET
	vnode_t		*vp;
	external_alenlist_t	dcalist;
	int		dcalen;
	struct iovec	saved_iov[16];
#endif

	DCSTAT(dcvn_write);
	uiop->uio_pmp = NULL;
	ASSERT(uiop->uio_pbuf == NULL);
	ASSERT(credp->cr_mac == NULL);
	ASSERT(uiop->uio_iovcnt <= 16);

	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_write", dcp, "uiop", uiop);
#ifdef NOTYET
	vp = BHV_TO_VNODE(bdp);
	if (uiop->uio_segflg != UIO_NOSPACE &&
	     !IS_KSEG0(uiop->uio_iov[0].iov_base)
	     && ((vp->v_type) == VREG || (vp->v_type == VDIR))) {
		dcalist = uio_to_plist (uiop, B_WRITE, &dcalen, saved_iov,
			&error);
		if (dcalist == NULL) {
			return(error);
		}
	
		msgerr = invk_dsvn_list_write(DCVN_TO_SERVICE(dcp),
			DCVN_TO_OBJID(dcp), 
			uiop, uiop->uio_iov, uiop->uio_iovcnt, 
			dcalist, (dcalen/sizeof(struct external_alenlist)),
			ioflag, CRED_GETID(credp), fl, 
			&uio_offset, &uio_sigpipe, &uio_resid, &error);
		ASSERT(!msgerr);

		if (uiop->uio_segflg != UIO_SYSSPACE) {
			plist_free (uiop->uio_iovcnt, B_WRITE|B_PHYS, 
				saved_iov);
		}
		kmem_free (dcalist, dcalen);

	} else {
#endif
		msgerr = invk_dsvn_write(DCVN_TO_SERVICE(dcp),
			DCVN_TO_OBJID(dcp), 
			uiop, uiop->uio_iov, uiop->uio_iovcnt, ioflag,
			CRED_GETID(credp), fl, &uio_offset, &uio_sigpipe,
			&uio_resid, &error);
		ASSERT(!msgerr);
#ifdef NOTYET
	}
#endif


	uiop->uio_offset = uio_offset;
	uiop->uio_sigpipe = uio_sigpipe;
	uiop->uio_resid = uio_resid;
	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_write", dcp, "error", error);

	return error;
}

/*
 * Ioctl vnode op.
 */
STATIC int
dcvn_ioctl(
	bhv_desc_t 	*bdp,
	int 		cmd,
	void 		*arg,
	int 		flag,
	cred_t 		*credp,
	int 		*rvalp,
	vopbd_t         *vbds)
        
{
	dcvn_t		*dcp = BHV_TO_DCVN(bdp);
	service_t       svc = DCVN_TO_SERVICE(dcp);
	cfs_handle_t	vbds_vnh;
	int		error;
	/* REFERENCED */
	int		msgerr;

	DCSTAT(dcvn_ioctl);
	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_ioctl", dcp, "cmd", cmd);
	msgerr = invk_dsvn_ioctl(svc, DCVN_TO_OBJID(dcp), 
			cmd, arg, flag, CRED_GETID(credp), rvalp, vbds,
			&vbds_vnh, &error);
	ASSERT(!msgerr);
	if (error)
		goto out;

	switch (vbds->vopbd_req) {
	case VOPBDR_NIL:
		break;

	case VOPBDR_ASSIGN: {
	        vnode_t *xvp = NULL;
		vopbd_assign_t *vbda = &vbds->vopbd_parm.vopbdp_assign;

		while (1) {
			cfs_vnimport(&vbds_vnh, &xvp);
			if (xvp)
				break;
			msgerr = invk_dsvn_vnode_reexport(svc, 
					 (objid_t) vbda->vopbda_vp,
					 vbda->vopbda_vnum, &vbds_vnh);
			ASSERT(!msgerr);
		}
		msgerr = invk_dsvn_vnode_unref(svc, (objid_t) vbda->vopbda_vp, 
				      vbda->vopbda_vnum);
		ASSERT(!msgerr);
		vbda->vopbda_vp = xvp;
		break;
	}

	default:
	        panic("unsupported cross-cell operation");
	}

out:
	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_ioctl", dcp, "error", error);
	return error;
}


void dcvn_checkattr(dcvn_t *dcp)
{
	vattr_t *vap;

	ASSERT(dcp);
	if (tkc_hold(dcp->dcv_tclient, DVN_ATTR_READ) == DVN_ATTR_READ) {
		vap = &dcp->dcv_vattr;
		ASSERT((int)vap->va_type > 0 && (int)vap->va_type < 20);
		tkc_release(dcp->dcv_tclient, DVN_ATTR_READ);
	}
}
/*
 * Getattr vnode op.
 */
/* ARGSUSED */
STATIC int
dcvn_getattr(
	bhv_desc_t 	*bdp,
	vattr_t		*vap,
	int		flags,
	cred_t		*credp)
{
	dcvn_t		*dcp = BHV_TO_DCVN(bdp);
	vattr_t		*dvap;
	int		error;
	/* REFERENCED */
	int		msgerr;
	int		mask;
	tk_set_t	tokens_needed = TK_NULLSET, tkacq;

	DCSTAT(dcvn_getattr);
	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_getattr", dcp, "vfs", dcp->dcv_dcvfs);
	ASSERT(dcp && dcp->dcv_dcvfs);

	if (DC_FS_ATTR_CACHE(dcp->dcv_dcvfs)) {

		/*
		 * First, figure out what tokens we need depending on what
		 * is being read.
		 */

		mask = vap->va_mask;
		if ((mask & AT_ATTR_PROTECTED) != 0)
			tokens_needed = TK_ADD_SET(tokens_needed, DVN_ATTR_READ);
		if ((mask & AT_TIMES) != 0)
			tokens_needed = TK_ADD_SET(tokens_needed, DVN_TIMES_READ);
		if ((mask & AT_EXTENT_PROTECTED) != 0)
			tokens_needed = TK_ADD_SET(tokens_needed, DVN_EXTENT_READ);

		tkc_acquire (dcp->dcv_tclient, tokens_needed, &tkacq);
		ASSERT(tkacq == tokens_needed);
		if (error = dcp->dcv_error)
			goto release_tkc;

		dcvn_checkattr(dcp);
		dvap = &dcp->dcv_vattr;

		vap->va_size = dvap->va_size;
		if (vap->va_mask == AT_SIZE)
			goto release_tkc;

		vap->va_fsid = dvap->va_fsid;
		vap->va_nodeid = dvap->va_nodeid;
		vap->va_nlink = dvap->va_nlink;

		/*
		 * Quick exit for non-stat callers
		 */
		if ((vap->va_mask & ~(AT_SIZE|AT_FSID|AT_NODEID|AT_NLINK)) == 0)
			goto release_tkc;

		vap->va_type = dvap->va_type;
		vap->va_mode = dvap->va_mode;
		vap->va_uid = dvap->va_uid;
		vap->va_gid = dvap->va_gid;
		vap->va_projid = dvap->va_projid;

		if ((dvap->va_type == VCHR) || (dvap->va_type == VBLK))
			vap->va_rdev = dvap->va_rdev;
		else
			vap->va_rdev = 0;

		vap->va_atime.tv_sec = dvap->va_atime.tv_sec;
		vap->va_atime.tv_nsec = dvap->va_atime.tv_nsec;
		vap->va_mtime.tv_sec = dvap->va_mtime.tv_sec;
		vap->va_mtime.tv_nsec = dvap->va_mtime.tv_nsec;
		vap->va_ctime.tv_sec = dvap->va_ctime.tv_sec;
		vap->va_ctime.tv_nsec = dvap->va_ctime.tv_nsec;

                vap->va_blksize = dvap->va_blksize;
		vap->va_nblocks = dvap->va_nblocks;

		/*
		 * Exit for stat callers.  See if any of the rest of the fields
		 * to be filled in are needed.
		 */
		if ((vap->va_mask &
		     (AT_XFLAGS|AT_EXTSIZE|AT_NEXTENTS|AT_ANEXTENTS|
		      AT_GENCOUNT|AT_VCODE)) == 0)
			goto release_tkc;

		/*
		 * convert di_flags to xflags
		 */

		vap->va_xflags = dvap->va_xflags;
		vap->va_extsize = dvap->va_extsize;
		vap->va_nextents = dvap->va_nextents;
		vap->va_anextents = dvap->va_anextents;
		vap->va_gencount = dvap->va_gencount;
		vap->va_vcode = dvap->va_vcode;
release_tkc:
		tkc_release (dcp->dcv_tclient, tokens_needed);
	} else {
		msgerr = invk_dsvn_getattr(DCVN_TO_SERVICE(dcp),
					DCVN_TO_OBJID(dcp), 
					vap, flags, CRED_GETID(credp), &error);
		ASSERT(!msgerr);
	}
	DVN_KTRACE8(DVN_TRACE_DCVOP, "dcvn_getattr", dcp,
		"tokens", tokens_needed, "mask", mask, "error", error);
	return error;
}

/*
 * Setattr vnode op.
 */
STATIC int
dcvn_setattr(
	bhv_desc_t 	*bdp,
	vattr_t		*vap,
	int		flags,
	cred_t		*credp)
{
	dcvn_t		*dcp = BHV_TO_DCVN(bdp);
	int		error;
	/* REFERENCED */
	int		msgerr;

	DCSTAT(dcvn_setattr);

	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_setattr", dcp, "vap", vap);
	msgerr = invk_dsvn_setattr(DCVN_TO_SERVICE(dcp), DCVN_TO_OBJID(dcp), 
			  vap, flags, CRED_GETID(credp), &error);
	ASSERT(!msgerr);
	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_setattr", dcp, "error", error);
	return error;
}

/*
 * Access vnode op.
 */
STATIC int
dcvn_access(
	bhv_desc_t 	*bdp,
	int		mode,
	cred_t		*credp)
{
	dcvn_t		*dcp = BHV_TO_DCVN(bdp);
	int		error;
	vattr_t		*vap;
	vnode_t 	*vp = BHV_TO_VNODE(bdp);
	mode_t 		orgmode = mode;

	DCSTAT(dcvn_access);

	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_access", dcp, "vfs", dcp->dcv_dcvfs);
	if (DC_FS_ATTR_CACHE(dcp->dcv_dcvfs)) {
		tkc_acquire1 (dcp->dcv_tclient, DVN_ATTR_READ_1);

		if (error = dcp->dcv_error)
			goto done;

		/*
		 * Verify that the MAC policy allows the requested access.
		 * JIMJIM cache mac label into dcvn? New token?
		 */
		if (error = _MAC_VACCESS(vp, credp, mode))
			goto done;
		
		if ((mode & VWRITE) && !WRITEALLOWED(vp, credp)) {
			error = EROFS;
			goto done;
		}

		/*
		 * If there's an Access Control List it's used instead of
		 * the mode bits.
		 * JIMJIM cache of ACL's in the dcvn? New token?
		 */
		if ((error = _ACL_VACCESS(vp, mode, credp)) != -1)
			goto done;
		else
			error = 0;

		vap = &dcp->dcv_vattr;
		dcvn_checkattr(dcp);
		if (credp->cr_uid != vap->va_uid) {
			mode >>= 3;
			if (!groupmember((gid_t)vap->va_gid, credp))
				mode >>= 3;
		}
		if ((vap->va_mode & mode) == mode)
			goto done;

		if (((orgmode & VWRITE) && !_CAP_CRABLE(credp, CAP_DAC_WRITE)) ||
		    ((orgmode & VREAD) && !_CAP_CRABLE(credp, CAP_DAC_READ_SEARCH)) ||
		    ((orgmode & VEXEC) && !_CAP_CRABLE(credp, CAP_DAC_EXECUTE))) {
#ifdef	NOISE
			cmn_err(CE_NOTE, "Ick: mode=%o, orgmode=%o", mode, orgmode);
#endif	/* NOISE */
			error = EACCES;
			goto done;
		}

	done:
		tkc_release1 (dcp->dcv_tclient, DVN_ATTR_READ_1);
	} else { /* FS doesn't support attr caching so ship it */
		/* REFERENCED */
		int	msgerr;

		msgerr = invk_dsvn_access(DCVN_TO_SERVICE(dcp),
				DCVN_TO_OBJID(dcp), 
				mode, CRED_GETID(credp), &error);
		ASSERT(!msgerr);
	}
	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_access", dcp, "error", error);
	return error;
}

/*
 * Lookup vnode op.
 */
/* ARGSUSED */
STATIC int
dcvn_lookup(
	bhv_desc_t	*dirbdp,
	char		*name,
	vnode_t		**vpp,
	pathname_t	*pnp,
	int		inflags,
	vnode_t		*rdir, 
	cred_t		*credp)
{
	dcvn_t		*dcp = BHV_TO_DCVN(dirbdp);
	vnode_t		*dirvp = BHV_TO_VNODE(dirbdp);
	dcvn_t		*newdcp;
	tk_set_t	obtset;
	enum vtype	vtype;
	dev_t		rdev;
	int		flags, try_cnt, error;
	int		cxfs_flags;
	char 		*cxfs_buff;
	size_t		cxfs_count;
	/* REFERENCED */
	int		msgerr;

	DCSTAT(dcvn_lookup);
	if (dirvp->v_type != VDIR)
		return ENOTDIR;

	/*
	 * If/when the dnlc is cellularized then we can do a dnlc
	 * lookup on the client-side.  Make sure that hits in the dnlc
	 * still do a VEXEC access check.
	 */

	/*
	 * Allocate a dcvn in advance.  When we begin caching attributes this
	 * will allow the lookup RPC to deposit attributes in their final
	 * destination.  The call to vn_alloc occurs in dcvn_newdc_setup.
	 */
	newdcp = kmem_zone_alloc(dcvn_zone, KM_SLEEP);
	spinlock_init(&newdcp->dcv_lock, "dcv_lock");

	/*
	 * Send a lookup RPC to the server.
	 *
	 * Note that multiple threads racing to obtain an existence token,
	 * or a thread obtaining racing a thread returning an existence token,
	 * requires that this code loop, checking a hash table and contacting
	 * the server.  Backoff each time through the loop.
	 */
	try_cnt = 0;
	do {
		DVN_KTRACE6(DVN_TRACE_DCVOP, "dcvn_lookup", dcp, "try_cnt", try_cnt,
			"newdcp", newdcp);
		cxfs_count = 0; cxfs_buff = NULL; cxfs_flags = 0;
		/*
		 * Send the RPC.
		 */
		msgerr = invk_dsvn_lookup(DCVN_TO_SERVICE(dcp),
				 DCVN_TO_OBJID(dcp), 
				 name, pnp->pn_complen+1,  
				 inflags, CRED_GETID(credp), cellid(),
				 &newdcp->dcv_handle, &obtset, 
				 &vtype, &rdev, &flags,
				 &newdcp->dcv_vattr, &cxfs_flags,
				 &cxfs_buff, &cxfs_count, &error);
		ASSERT(!msgerr);
		if (error)
			break;

		/*
		 * Map from a handle to a new dcvn.  If ERESTART is
		 * returned then we're racing and must retry.
		 *
		 * Note: this call can change the value of newdcp.
		 */
		error = dcvn_handle_to_dcvn(&newdcp->dcv_handle, obtset, 
					    vtype, rdev, dcp->dcv_dcvfs,
					    flags, cxfs_flags, cxfs_buff,
					    cxfs_count, &newdcp);

		ASSERT(!error || error == ERESTART);
		if (!error)
			break;

		if (cxfs_count) {
			ASSERT(cxfs_buff);
			kmem_free((caddr_t)cxfs_buff, cxfs_count);
		}

		/* backoff to give others a chance to run */
		cell_backoff(++try_cnt);

		obtset = TK_NULLSET;		/* reset */
	} while (1);

	ASSERT(newdcp != NULL);

	if (!error) {
		vnode_t	*vp;


		*vpp = vp = DCVN_TO_VNODE(newdcp);

		/*
		 * If the vnode is a special device give
		 * specfs a chance to insert its behavior
		 */
		if (vp->v_type == VBLK || vp->v_type == VCHR) {
			vnode_t	*nvp;

#ifdef  SPECFS_DEBUG
			printf("dcvn_lookup[%d]: vp/0x%x{%d}, rdev(%d/%d, %d)\n",
						cellid(), vp, vp->v_count,
						major(vp->v_rdev),
						emajor(vp->v_rdev),
						minor(vp->v_rdev));
#endif  /* SPECFS_DEBUG */

			nvp = spec_vp(vp, vp->v_rdev, vp->v_type, credp);

			ASSERT(nvp);
			ASSERT(nvp == vp);
			ASSERT(nvp->v_count > 1);

				/* Release xtra ref taken by spec */
			VN_RELE(nvp);

			*vpp = nvp;
		}

	} else {
		ASSERT(!cxfs_count && !cxfs_flags);
		spinlock_destroy(&newdcp->dcv_lock);
		kmem_zone_free(dcvn_zone, newdcp);
	}

	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_lookup", newdcp, "error", error);

	return error;
}

/*
 * Internal routine used by create and mkdir.
 */
STATIC int
dcvn_create_int(
	bhv_desc_t	*dirbdp,
	char		*name,
	vattr_t		*vap,
	int		inflags,
	int		I_mode,
	vnode_t		**vpp,
	cred_t		*credp)
{
	dcvn_t		*dcp = BHV_TO_DCVN(dirbdp);
	dcvn_t		*newdcp;
	tk_set_t	obtset;
	int		flags, try_cnt, error;
	int		cxfs_flags;
	char 		*cxfs_buff;
	size_t		cxfs_count;
	/* REFERENCED */
	int		msgerr;

	DCSTAT(dcvn_create_int);
	if (*vpp) {
		VN_RELE(*vpp);
		*vpp = NULL;
	}
	/*
	 * Allocate a dcvn in advance.  When we begin caching attributes this
	 * will allow the create RPC to deposit attributes in their final
	 * destination.  The call to vn_alloc occurs in dcvn_newdc_setup.
	 */
	newdcp = kmem_zone_alloc(dcvn_zone, KM_SLEEP);
	spinlock_init(&newdcp->dcv_lock, "dcv_lock");

	/*
	 * Send a create RPC to the server.
	 *
	 * Note that a thread doing a lookup, or a thread returning the
	 * existence token, racing against us creating, requires that this
	 * code loop, checking a hash table and contacting the server.  
	 * Backoff each time through the loop.
	 *
	 * Note also that attempting to create a file that already exists
	 * can succeed (if the VEXCL flag isn't set), but we might already
	 * have the existence token for the file.
	 */
	try_cnt = 0;
	do {
		DVN_KTRACE6(DVN_TRACE_DCVOP, "dcvn_create_int", dcp,
			"try_cnt", try_cnt, "newdcp", newdcp);
		/*
		 * Send the RPC.
		 */
		cxfs_count = 0; cxfs_buff = NULL; cxfs_flags = 0;
		msgerr = invk_dsvn_create(DCVN_TO_SERVICE(dcp),
				 DCVN_TO_OBJID(dcp), 
				 name, strlen(name)+1, vap, inflags, 
				 I_mode, CRED_GETID(credp), cellid(),
				 &newdcp->dcv_handle, &obtset, &flags,
				 &newdcp->dcv_vattr, &cxfs_flags,
				 &cxfs_buff, &cxfs_count, &error);
		ASSERT(!msgerr);
		if (error)
			break;

		/*
		 * Map from a handle to a new dcvn.  If ERESTART is
		 * returned then we're racing and must retry.
		 *
		 * Note: this call can change the value of newdcp.
		 */
		error = dcvn_handle_to_dcvn(&newdcp->dcv_handle, obtset, 
					    vap->va_type, vap->va_rdev, 
					    dcp->dcv_dcvfs, flags, cxfs_flags,
					    cxfs_buff, cxfs_count, &newdcp);

		ASSERT(!error || error == ERESTART);
		if (!error)
			break;

		if (cxfs_count) {
			ASSERT(cxfs_buff);
			kmem_free((caddr_t)cxfs_buff, cxfs_count);
		}

		/* backoff to give others a chance to run */
		cell_backoff(++try_cnt);

		obtset = TK_NULLSET;		/* reset */
	} while (1);

	ASSERT(newdcp != NULL);

	if (!error) {
		vnode_t	*vp;


		*vpp = vp = DCVN_TO_VNODE(newdcp);

		/*
		 * If the vnode is a special device give
		 * specfs a chance to insert its behavior
		 */
		if (vp->v_type == VBLK || vp->v_type == VCHR) {
			vnode_t	*nvp;

#ifdef  SPECFS_DEBUG
			printf("dcvn_create[%d]: vp/0x%x{%d}, rdev(%d/%d, %d)\n",
						cellid(), vp, vp->v_count,
						major(vp->v_rdev),
						emajor(vp->v_rdev),
						minor(vp->v_rdev));
#endif  /* SPECFS_DEBUG */

			nvp = spec_vp(vp, vp->v_rdev, vp->v_type, credp);

			ASSERT(nvp);
			ASSERT(nvp == vp);
			ASSERT(nvp->v_count > 1);

				/* Release xtra ref taken by spec */
			VN_RELE(nvp);

			*vpp = nvp;
		}

	} else {
		spinlock_destroy(&newdcp->dcv_lock);
		kmem_zone_free(dcvn_zone, newdcp);
	}

	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_create_int", newdcp, "error", error);

	return error;
}

/*
 * Create vnode op.
 */
STATIC int
dcvn_create(
	bhv_desc_t	*dirbdp,
	char		*name,
	vattr_t		*vap,
	int		flags,
	int		I_mode,
	vnode_t		**vpp,
	cred_t		*credp)
{
	return dcvn_create_int(dirbdp, name, vap, flags, I_mode, vpp, credp);
}

/*
 * Mkdir vnode op.
 */
/* ARGSUSED */
STATIC int
dcvn_mkdir(
	bhv_desc_t	*dirbdp,
	char		*name,
	vattr_t		*vap,
	vnode_t		**vpp,
	cred_t		*credp)
{
	return dcvn_create_int(dirbdp, name, vap, 0, 0, vpp, credp);
}

/*
 * Remove vnode op.
 */
STATIC int
dcvn_remove(
	bhv_desc_t 	*dirbdp,
	char		*name,
	cred_t		*credp)
{
	dcvn_t		*dcp = BHV_TO_DCVN(dirbdp);
	int		error;
	/* REFERENCED */
	int		msgerr;

	DCSTAT(dcvn_remove);
	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_remove", dcp, "name", name);
	msgerr = invk_dsvn_remove(DCVN_TO_SERVICE(dcp), DCVN_TO_OBJID(dcp), 
			 name, strlen(name)+1, CRED_GETID(credp), &error);
	ASSERT(!msgerr);
	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_remove", dcp, "error", error);
	return error;
}

/*
 * Link vnode op.
 */
STATIC int
dcvn_link(
	bhv_desc_t 	*tdirbdp,
	vnode_t		*svp,
	char		*tname,
	cred_t		*credp)
{
	dcvn_t		*dcp = BHV_TO_DCVN(tdirbdp);
	objid_t		objid;
	int		error;
	/* REFERENCED */
	int		msgerr;

	/* 
	 * Invoke the target directory, passing the objid of the vnode
	 * to link to as an argument.
	 *
	 * We know the svp's base behavior is a DCVN because we're guaranteed
	 * that both the directory and svp are in the same file system.
	 */
	DCSTAT(dcvn_link);
	objid = DCVN_TO_OBJID(VNODE_TO_DCVN(svp)); 
	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_link", dcp, "objid", objid);

	msgerr = invk_dsvn_link(DCVN_TO_SERVICE(dcp), DCVN_TO_OBJID(dcp), 
		       objid, tname, strlen(tname)+1, 
		       CRED_GETID(credp), &error);
	ASSERT(!msgerr);
	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_link", dcp, "error", error);
	return error;
}

/*
 * Rename vnode op.
 */
STATIC int
dcvn_rename(
	bhv_desc_t	*sdirbdp,
	char		*sname,
	vnode_t		*tdir_vp,
	char		*tname,
	pathname_t	*tpnp,
	cred_t		*credp)
{
	dcvn_t		*dcp = BHV_TO_DCVN(sdirbdp);
	cfs_handle_t	tdir_handle;
	int		try_cnt, error;
	/* REFERENCED */
	int		msgerr;

	DCSTAT(dcvn_rename);

	/*
	 * Map tdir_vp to a handle.  There's no guarantee that the object
	 * won't migrate prior to the handle being imported via cfs_vnimport.
	 */
	try_cnt = 0;
 again:
	cfs_vnexport(tdir_vp, &tdir_handle);
	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_rename", dcp, "tdir_vp", tdir_vp);

	msgerr = invk_dsvn_rename(DCVN_TO_SERVICE(dcp), DCVN_TO_OBJID(dcp), 
			 sname, strlen(sname)+1, &tdir_handle, 
			 tname, tpnp->pn_complen+1,
			 CRED_GETID(credp), &error);
	ASSERT(!msgerr);
	if (error == EMIGRATED) {
		/* 
		 * The target directory must have migrated since
		 * we called cfs_vnexport.  Retry.
		 */
		cell_backoff(++try_cnt);
		goto again;
	}
	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_rename", dcp, "error", error);
	return error;
}

/*
 * Rmdir vnode op.
 */
STATIC int
dcvn_rmdir(
	bhv_desc_t	*dirbdp,
	char		*name,
	vnode_t		*cdir_vp,	/* current dir */
	cred_t		*credp)
{
	dcvn_t		*dcp = BHV_TO_DCVN(dirbdp);
	cfs_handle_t	cdir_handle;
	int		try_cnt, error;
	/* REFERENCED */
	int		msgerr;


	DCSTAT(dcvn_rmdir);
	/*
	 * Map cdir_vp to a handle.  There's no guarantee that the object
	 * won't migrate prior to the handle being imported via cfs_vnimport.
	 */
	try_cnt = 0;
 again:
	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_rmdir", dcp, "try_cnt", try_cnt);
	cfs_vnexport(cdir_vp, &cdir_handle);

	msgerr = invk_dsvn_rmdir(DCVN_TO_SERVICE(dcp), DCVN_TO_OBJID(dcp), 
			name, strlen(name)+1, &cdir_handle, CRED_GETID(credp),
			&error);
	ASSERT(!msgerr);
	if (error == EMIGRATED) {
		/* 
		 * The current working directory must have migrated since
		 * we called cfs_vnexport.  Retry.
		 */
		cell_backoff(++try_cnt);
		goto again;
	}
	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_rmdir", dcp, "error", error);
	return error;
}

/*
 * Readdir vnode op.
 */
STATIC int
dcvn_readdir(
	bhv_desc_t	*dirbdp,
	uio_t		*uiop,
	cred_t		*credp,
	int		*eofp)
{
	dcvn_t		*dcp = BHV_TO_DCVN(dirbdp);
	int		error;
	off_t		uio_offset;
	short		uio_sigpipe;
	ssize_t		uio_resid;
	/* REFERENCED */
	int		msgerr;

	DCSTAT(dcvn_readdir);
	uiop->uio_pmp = NULL;
	ASSERT(uiop->uio_pbuf == NULL);
	ASSERT(credp->cr_mac == NULL);
	
	msgerr = invk_dsvn_readdir(DCVN_TO_SERVICE(dcp), DCVN_TO_OBJID(dcp), 
			  uiop, uiop->uio_iov, uiop->uio_iovcnt,
			  CRED_GETID(credp), eofp, &uio_offset, &uio_sigpipe,
			  &uio_resid, &error);
	ASSERT(!msgerr);

	uiop->uio_offset = uio_offset;
	uiop->uio_sigpipe = uio_sigpipe;
	uiop->uio_resid = uio_resid;

	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_readdir", dcp, "error", error);
	return error;
}

/*
 * Symlink vnode op.
 */
STATIC int
dcvn_symlink(
	bhv_desc_t	*dirbdp,
	char	       	*link_name,
	vattr_t		*vap,
	char		*target_path,
	cred_t		*credp)
{
	dcvn_t		*dcp = BHV_TO_DCVN(dirbdp);
	int		error;
	/* REFERENCED */
	int		msgerr;

	DCSTAT(dcvn_symlink);

	msgerr = invk_dsvn_symlink(DCVN_TO_SERVICE(dcp), DCVN_TO_OBJID(dcp), 
			  link_name, strlen(link_name)+1, vap, 
			  target_path, strlen(target_path)+1, 
			  CRED_GETID(credp), &error);
	ASSERT(!msgerr);
	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_symlink", dcp, "error", error);
	return error;
}

/*
 * Readlink vnode op.
 */
STATIC int
dcvn_readlink(
	bhv_desc_t 	*bdp,
	uio_t		*uiop,
	cred_t		*credp)
{
	dcvn_t		*dcp = BHV_TO_DCVN(bdp);
	int		error;
	off_t		uio_offset;
	short		uio_sigpipe;
	ssize_t		uio_resid;
	/* REFERENCED */
	int		msgerr;

	DCSTAT(dcvn_readlink);
	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_readlink", dcp, "uiop", uiop);
	uiop->uio_pmp = NULL;
	ASSERT(uiop->uio_pbuf == NULL);
	ASSERT(credp->cr_mac == NULL);
	
	msgerr = invk_dsvn_readlink(DCVN_TO_SERVICE(dcp), DCVN_TO_OBJID(dcp), 
			   uiop, uiop->uio_iov, uiop->uio_iovcnt,
			   CRED_GETID(credp),
			   &uio_offset, &uio_sigpipe, &uio_resid, &error);
	ASSERT(!msgerr);

	uiop->uio_offset = uio_offset;
	uiop->uio_sigpipe = uio_sigpipe;
	uiop->uio_resid = uio_resid;

	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_readlink", dcp, "error", error);
	return error;
}

/*
 * Fsync vnode op.
 */
STATIC int
dcvn_fsync(
	bhv_desc_t 	*bdp,
	int		flag,
	cred_t		*credp,
	off_t		start,
	off_t		stop)
{
	dcvn_t		*dcp = BHV_TO_DCVN(bdp);
	int		error;
	/* REFERENCED */
	int		msgerr;

	DCSTAT(dcvn_fsync);
	msgerr = invk_dsvn_fsync(DCVN_TO_SERVICE(dcp), DCVN_TO_OBJID(dcp), 
			flag, CRED_GETID(credp), start, stop, &error);
	ASSERT(!msgerr);
	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_fsync", dcp, "error", error);
	return error;
}

/*
 * Inactive vnode op.
 */
/* ARGSUSED */
STATIC int
dcvn_inactive(
	bhv_desc_t 	*bdp,
	cred_t		*credp)
{
	dcvn_t *dcp;
	/*
	 * If the VINACTIVE_TEARDOWN flag is set then we want to do 
	 * teardown now.
	 */
	DCSTAT(dcvn_inactive);
	dcp = BHV_TO_DCVN(bdp);
	if (BHV_TO_VNODE(bdp)->v_flag & VINACTIVE_TEARDOWN) {
		DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_inactive", dcp, "NOCACHE",
			BHV_TO_VNODE(bdp)->v_flag);
		dcvn_teardown(dcp);
		return VN_INACTIVE_NOCACHE;
	}

	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_inactive", dcp, "CACHE", 
		BHV_TO_VNODE(bdp)->v_flag);
	return VN_INACTIVE_CACHE;
}

/*
 * Fid vnode op.
 */
/* ARGSUSED */
STATIC int
dcvn_fid(
	bhv_desc_t 	*bdp,
	fid_t		**fidpp)
{
	dcvn_t		*dcp = BHV_TO_DCVN(bdp);
	fid_t		*fidp;
	int		error;
	/* REFERENCED */
	int		msgerr;

	DCSTAT(dcvn_fid);
	fidp = (fid_t *)kmem_alloc(sizeof(fid_t), KM_SLEEP);

	msgerr = invk_dsvn_fid(DCVN_TO_SERVICE(dcp), DCVN_TO_OBJID(dcp), 
		      fidp, &error);
	ASSERT(!msgerr);
	if (error)
		kmem_free((caddr_t)fidp, sizeof(fid_t));
	else
		*fidpp = fidp;

	DVN_KTRACE6(DVN_TRACE_DCVOP, "dcvn_fid", dcp, "error", error,
		"fidp", fidp);
	return error;
}

/*
 * Fid2 vnode op.
 */
STATIC int
dcvn_fid2(
	bhv_desc_t	*bdp,
	fid_t		*fidp)
{
	dcvn_t		*dcp = BHV_TO_DCVN(bdp);
	int		error;
	/* REFERENCED */
	int		msgerr;

	DCSTAT(dcvn_fid2);
	msgerr = invk_dsvn_fid2(DCVN_TO_SERVICE(dcp), DCVN_TO_OBJID(dcp), 
		       fidp, &error);
	ASSERT(!msgerr);
	DVN_KTRACE6(DVN_TRACE_DCVOP, "dcvn_fid2", dcp, "error", error,
		"fidp", fidp);
	return error;
}

/*
 * Rwlock vnode op.
 */
STATIC void
dcvn_rwlock(
	bhv_desc_t	*bdp,
	vrwlock_t	locktype)
{
	dcvn_t		*dcp = BHV_TO_DCVN(bdp);
	/* REFERENCED */
	int		msgerr;

	DCSTAT(dcvn_rwlock);
	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_rwlock", dcp, "locktype", locktype);
	msgerr = invk_dsvn_rwlock(DCVN_TO_SERVICE(dcp), DCVN_TO_OBJID(dcp), 
			 locktype);
	ASSERT(!msgerr);
}

/*
 * Rwunlock vnode op.
 */
STATIC void
dcvn_rwunlock(
	bhv_desc_t	*bdp,
	vrwlock_t	locktype)
{
	dcvn_t		*dcp = BHV_TO_DCVN(bdp);
	/* REFERENCED */
	int		msgerr;

	DCSTAT(dcvn_rwunlock);

	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_rwunlock", dcp, "locktype", locktype);
	msgerr = invk_dsvn_rwunlock(DCVN_TO_SERVICE(dcp), DCVN_TO_OBJID(dcp), 
			   locktype);
	ASSERT(!msgerr);
}

/*
 * Seek vnode op.
 */
STATIC int
dcvn_seek(
	bhv_desc_t	*bdp,
	off_t		old_offset,
	off_t		*new_offsetp)
{
	dcvn_t		*dcp = BHV_TO_DCVN(bdp);
	int		error;
	/* REFERENCED */
	int		msgerr;

	DCSTAT(dcvn_seek);
	msgerr = invk_dsvn_seek(DCVN_TO_SERVICE(dcp), DCVN_TO_OBJID(dcp), 
		       old_offset, new_offsetp, &error);
	ASSERT(!msgerr);
	DVN_KTRACE8(DVN_TRACE_DCVOP, "dcvn_seek", dcp, "old_offset", old_offset,
		"new_offsetp", new_offsetp, "error", error);
	return error;
}

/*
 * Seek vnode op.
 */
STATIC int
dcvn_cmp(
	bhv_desc_t	*bdp,
	vnode_t		*vp2)
{
	dcvn_t		*dcp = BHV_TO_DCVN(bdp);
	cfs_handle_t	vp2_handle;
	int		equal, try_cnt;
	int		error;
	/* REFERENCED */
	int		msgerr;

	ASSERT((BHV_TO_VNODE(bdp)->v_flag & VDOCMP) || 
	       (vp2->v_flag & VDOCMP));

	DCSTAT(dcvn_cmp);
	/*
	 * Map vp2 to a handle.  This handle isn't protected by
	 * an existence token so there's no guarantee that the object
	 * won't migrate prior to the handle being imported via cfs_vnimport.
	 */
	try_cnt = 0;
 again:
	DVN_KTRACE6(DVN_TRACE_DCVOP, "dcvn_cmp", dcp, "vp2", vp2,
		"try_cnt", try_cnt);
	cfs_vnexport(vp2, &vp2_handle);

	msgerr = invk_dsvn_cmp(DCVN_TO_SERVICE(dcp), DCVN_TO_OBJID(dcp), 
		      &vp2_handle, &equal, &error);
	ASSERT(!msgerr);
	if (error == EMIGRATED) {
		/* 
		 * vp2 must have migrated since we called cfs_vnexport.  
		 * Retry.
		 */
		cell_backoff(++try_cnt);
		goto again;
	}
	ASSERT(error == 0);

	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_cmp", dcp, "equal", equal);
	return equal;
}

/*
 * Frlock vnode op.
 */
STATIC int
dcvn_frlock(
	bhv_desc_t	*bdp,
	int		cmd,
	flock_t		*flockp,
	int		flag,
	off_t		offset,
	vrwlock_t       vrwlock,
	cred_t		*credp)
{
	dcvn_t		*dcp = BHV_TO_DCVN(bdp);
	int		error;
	/* REFERENCED */
	int		msgerr;

	DCSTAT(dcvn_frlock);
	msgerr = invk_dsvn_frlock(DCVN_TO_SERVICE(dcp), DCVN_TO_OBJID(dcp), 
			 cmd, flockp, flag, offset, vrwlock, CRED_GETID(credp), 
			 &error);
	ASSERT(!msgerr);
	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_frlock", dcp, "error", error);
	return error;
}

/*
 * Bmap vnode op.
 */
STATIC int
dcvn_bmap(
	bhv_desc_t	*bdp,
	off_t		offset,
	ssize_t		count,
	int		flags,
	cred_t		*credp,
	struct bmapval	*bmapp,
	int		*nbmaps)
{
	dcvn_t		*dcp = BHV_TO_DCVN(bdp);
	int		error;
	/* REFERENCED */
	int		msgerr;

	DCSTAT(dcvn_bmap);
	msgerr = invk_dsvn_bmap(DCVN_TO_SERVICE(dcp), DCVN_TO_OBJID(dcp), 
		       offset, count, flags, CRED_GETID(credp), bmapp,
		       nbmaps, &error);
	ASSERT(!msgerr);
	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_bmap", dcp, "error", error);
	return error;
}

/*
 * Map vnode op.
 */
STATIC int
dcvn_map(
	bhv_desc_t	*bdp,
	off_t		offset,
	size_t		len,
	mprot_t		prot,
	uint		inflags,
	cred_t		*credp,
	vnode_t		**vpp)		/* only need to set if vnode changed */
{
	dcvn_t		*dcp = BHV_TO_DCVN(bdp);
	dcvn_t		*newdcp;
	cfs_handle_t	handle;
	tk_set_t	obtset;
	enum vtype	vtype;
	dev_t		rdev;
	int		flags, try_cnt, error;

	/*
	 * Send a map RPC to the server.
	 *
	 * Note that for the case of the server returning a different vnode,
	 * multiple threads racing to obtain an existence token, or a thread
	 * obtaining racing a thread returning an existence token, requires
	 * that this code loop, checking a hash table and contacting the
	 * server.  Backoff each time through the loop.
	 *
	 * XXX Consider optimizing this routine for underlying file systems
	 * that are guaranteed _not_ to return a different vnode.
	 */
	DCSTAT(dcvn_map);
	try_cnt = 0;
	newdcp = NULL;
	do {
		/* REFERENCED */
		int	msgerr;
		DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_map", dcp, "try_cnt", try_cnt);

		/*
		 * Send the RPC.
		 */
		msgerr = invk_dsvn_map(DCVN_TO_SERVICE(dcp), DCVN_TO_OBJID(dcp),
			      offset, len, prot, inflags, CRED_GETID(credp),
			      cellid(), &handle, &obtset, &vtype, &rdev,
			      &flags, &error);
		ASSERT(!msgerr);
		if (error)
			break;

		if (CFS_HANDLE_IS_NULL(handle)) {
			/*
			 * Server didn't exchange vnodes - all done.
			 */
			if (newdcp != NULL) {
				spinlock_destroy(&newdcp->dcv_lock);
				kmem_zone_free(dcvn_zone, newdcp);
			}
			return 0;
		}

		/*
		 * CXFS should never return a new vnode for this
		 * vop. If it is changed to do so, this code must be
		 * modified to do something similar to lookup.
		 */

		ASSERT(!dcp->dcv_dcxvn);

		/*
		 * Now we're in the case where the server changed the
		 * vnode on us.
		 *
		 * Map from a handle to a new dcvn.  If ERESTART is
		 * returned then we're racing and must retry.
		 *
		 * Note: this call can change the value of newdcp.
		 */
		error = dcvn_handle_to_dcvn(&handle, obtset, vtype, rdev,
					    dcp->dcv_dcvfs, flags,
					    0, (char *)0, 0, &newdcp);

		ASSERT(!error || error == ERESTART);
		if (!error)
			break;

		/* backoff to give others a chance to run */
		cell_backoff(++try_cnt);

		obtset = TK_NULLSET;		/* reset */
	} while (1);

	if (!error) {
		ASSERT(newdcp != NULL);
		*vpp = DCVN_TO_VNODE(newdcp);
	} else if (newdcp != NULL) {
		spinlock_destroy(&newdcp->dcv_lock);
		kmem_zone_free(dcvn_zone, newdcp);
	}

	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_map", newdcp, "error", error);
	return error;
}

/*
 * Addmap vnode op.
 *
 * To avoid an AB/BA lock ordering problem, the implementation of this
 * routine must NOT take a lock that is also taken by any dcvn code that
 * calls copyin/copyout.  The reason is that VOP_ADDMAP is called with
 * the address space lock held, and copyin/copyout could fault requiring 
 * that the address space lock be taken.
 */
/* ARGSUSED */
STATIC int
dcvn_addmap(
	bhv_desc_t	*bdp,
	vaddmap_t	op,
	struct __vhandl_s	*vt,
	pgno_t		*pgno,
	off_t		offset,
	size_t		len,
	mprot_t		prot,
	cred_t		*credp)
{
	dcvn_t		*dcp = BHV_TO_DCVN(bdp);
	int		error;
	pgno_t		temp;
	/* REFERENCED */
	int		msgerr;

	DCSTAT(dcvn_addmap);
	if (pgno == NULL)
		pgno = &temp;
	msgerr = invk_dsvn_addmap(DCVN_TO_SERVICE(dcp), DCVN_TO_OBJID(dcp), 
			 op, vt, pgno, offset, len, prot, CRED_GETID(credp),
			 &error);
	ASSERT(!msgerr);
	return error;
}

/*
 * Delmap vnode op.
 */
/* ARGSUSED */
STATIC int
dcvn_delmap(
	bhv_desc_t	*bdp,
	struct __vhandl_s	*vt,
	size_t		len,
	cred_t		*credp)
{
	dcvn_t		*dcp = BHV_TO_DCVN(bdp);
	int		error;
	/* REFERENCED */
	int		msgerr;

	DCSTAT(dcvn_delmap);
	msgerr = invk_dsvn_delmap(DCVN_TO_SERVICE(dcp), DCVN_TO_OBJID(dcp), 
			 vt, len, CRED_GETID(credp), &error);
	ASSERT(!msgerr);
	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_delmap", dcp, "error", error);
	return error;
}

/*
 * Allocstore vnode op.
 */
STATIC int
dcvn_allocstore(
	bhv_desc_t	*bdp,
	off_t		offset,
	size_t		count,
	cred_t		*credp)
{
	dcvn_t		*dcp = BHV_TO_DCVN(bdp);
	int		error;
	/* REFERENCED */
	int		msgerr;

	DCSTAT(dcvn_allocstore);
	msgerr = invk_dsvn_allocstore(DCVN_TO_SERVICE(dcp), DCVN_TO_OBJID(dcp), 
			     offset, count, CRED_GETID(credp), &error);
	ASSERT(!msgerr);
	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_allocstore", dcp, "error", error);
	return error;
}

/*
 * Fcntl vnode op.
 */
STATIC int
dcvn_fcntl(
	bhv_desc_t	*bdp,
	int		cmd,
	void		*arg,
	int		flags,
	off_t		offset,
	cred_t		*credp,
	rval_t		*rvalp)
{
	dcvn_t		*dcp = BHV_TO_DCVN(bdp);
	int		error;
	/* REFERENCED */
	int		msgerr;

	DCSTAT(dcvn_fcntl);
	msgerr = invk_dsvn_fcntl(DCVN_TO_SERVICE(dcp), DCVN_TO_OBJID(dcp), 
			cmd, arg, flags, offset, CRED_GETID(credp), rvalp,
			&error);
	ASSERT(!msgerr);
	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_fcntl", dcp, "error", error);
	return error;
}

/*
 * Reclaim vnode op.
 */
/* ARGSUSED */
STATIC int
dcvn_reclaim(
	bhv_desc_t	*bdp,
	int		flag)
{
	DCSTAT(dcvn_reclaim);
	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_reclaim", BHV_TO_DCVN(bdp), "flag", flag);
	dcvn_teardown(BHV_TO_DCVN(bdp));
	return 0;
}

/*
 * attr get vnode op.
 */
STATIC int
dcvn_attr_get(
	bhv_desc_t 	*bdp, 
	char 		*name, 
	char 		*value, 
	int 		*valuelenp, 
	int 		flags,
	struct cred 	*cred)
{
	dcvn_t		*dcp = BHV_TO_DCVN(bdp);
	int		error;
	char		*rvalue;
	size_t		rlen = 0;
	/* REFERENCED */
	int		msgerr;

	DCSTAT(dcvn_attr_get);
	rvalue = value;
	msgerr = invk_dsvn_attr_get(DCVN_TO_SERVICE(dcp), DCVN_TO_OBJID(dcp), 
			   name, strlen(name)+1, &rvalue, &rlen, 
			   *valuelenp, flags, CRED_GETID(cred), &error);
	ASSERT(!msgerr);
	if (error == 0) {
		*valuelenp = rlen;
		if (rvalue != value) {
			bcopy(rvalue, value, rlen);
			kmem_free(rvalue, rlen);
		}
	}

	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_attr_get", dcp, "error", error);
	return error;
}

/*
 * attr set vnode op.
 */
STATIC int
dcvn_attr_set(
	bhv_desc_t 	*bdp, 
	char 		*name, 
	char 		*value, 
	int 		valuelen, 
	int 		flags,
	struct cred 	*cred)
{
	dcvn_t		*dcp = BHV_TO_DCVN(bdp);
	int		error;
	/* REFERENCED */
	int		msgerr;

	DCSTAT(dcvn_attr_set);
	msgerr = invk_dsvn_attr_set(DCVN_TO_SERVICE(dcp), DCVN_TO_OBJID(dcp), 
			   name, strlen(name)+1, 
			   value, valuelen, flags, CRED_GETID(cred), &error);
	ASSERT(!msgerr);
	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_attr_set", dcp, "error", error);
	return error;
}

/*
 * attr remove vnode op.
 */
STATIC int
dcvn_attr_remove(
	bhv_desc_t 	*bdp, 
	char 		*name, 
	int 		flags, 
	struct cred 	*cred)
{
	dcvn_t		*dcp = BHV_TO_DCVN(bdp);
	int		error;
	/* REFERENCED */
	int		msgerr;

	DCSTAT(dcvn_attr_remove);
	msgerr = invk_dsvn_attr_remove(DCVN_TO_SERVICE(dcp),
				DCVN_TO_OBJID(dcp), 
				name, strlen(name)+1, flags, CRED_GETID(cred),
				&error);
	ASSERT(!msgerr);
	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_attr_remove", dcp, "error", error);
	return error;
}

/*
 * Cover vnode op.
 */
STATIC int
dcvn_cover(
        bhv_desc_t      *bdp,
        struct mounta   *uap,
	char 		*attrs,
	struct cred 	*cred)
{
	cfs_handle_t	rdir_handle, cdir_handle;
	dcvn_t		*dcp = BHV_TO_DCVN(bdp);
	int		error;
	/* REFERENCED */
	int		msgerr;

	DCSTAT(dcvn_cover);
	/* XXX handle mntupdate case */
	/* 
	 * purge this vnode from our cache XXX R2
	 */

	/* export rdir and cdir */
	if (curuthread->ut_rdir)
		cfs_vnexport(curuthread->ut_rdir, &rdir_handle);
	cfs_vnexport(curuthread->ut_cdir, &cdir_handle);

	/* 
	 * function ship the cover to the dir server.
	 * Hence, all mounts covering this vnode are single-threaded.
	 */
	msgerr = invk_dsvn_cover(DCVN_TO_SERVICE(dcp), DCVN_TO_OBJID(dcp), uap, 
			attrs, CRED_GETID(cred),
			(curuthread->ut_rdir ? &rdir_handle : NULL), 
			&cdir_handle, &error);
	ASSERT(!msgerr);
	/* the new mount and its root vnode are imported lazily */
	VN_RELE(BHV_TO_VNODE(bdp));
	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_cover", dcp, "error", error);
	return(error);
}

/*
 * Vnode_change vnode op.
 */
/* ARGSUSED */
STATIC void
dcvn_vnode_change(
	bhv_desc_t	*bdp,
	vchange_t	cmd,
	__psint_t	val)
{
	dcvn_t		*dcp = BHV_TO_DCVN(bdp);
	/* REFERENCED */
	int		msgerr;

	DCSTAT(dcvn_vnode_change);

	switch (cmd) {
	case VCHANGE_FLAGS_FRLOCKS:
	case VCHANGE_FLAGS_ENF_LOCKING:
		/*
		 * Send change to server, which will serialize and broadcast 
		 * it to all clients.
		 */
		msgerr = invk_dsvn_vnode_change(DCVN_TO_SERVICE(dcp), 
				       DCVN_TO_OBJID(dcp), cmd, val);
		ASSERT(!msgerr);
		break;

	default:
		break;
	}
	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_vnode_change", dcp, "cmd", cmd);
}

STATIC int
dcvn_poll(
	bhv_desc_t	*bdp,
	short 		events,
	int 		anyyet,
	short 		*reventsp,
	struct pollhead **phpp,
	unsigned int 	*genp)
{
	dcvn_t		*dcp = BHV_TO_DCVN(bdp);
	int		error;
	/* REFERENCED */
	int		msgerr;

	DCSTAT(dcvn_poll);
	msgerr = invk_dsvn_poll(DCVN_TO_SERVICE(dcp), DCVN_TO_OBJID(dcp), 
			 events, anyyet, reventsp, phpp, genp, &error);
	ASSERT(!msgerr);
	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_poll", dcp, "error", error);
	return error;
}

STATIC int
dcvn_strgetmsg(
	bhv_desc_t	*bdp,
	struct strbuf	*mctl,
	struct strbuf	*mdata,
	unsigned char	*prip,
	int		*flagsp,
	int		fmode,
	union rval	*rvp)
{
	int             error;
	/* REFERENCED */
	int             msgerr;
	dcvn_t		*dcp = BHV_TO_DCVN(bdp);

	DCSTAT(dcvn_strgetmsg);
	msgerr = invk_dsvn_strgetmsg(DCVN_TO_SERVICE(dcp), DCVN_TO_OBJID(dcp), 
			      mctl, mdata, prip, flagsp, fmode, rvp, &error);
	ASSERT(!msgerr);
	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_strgetmsg", dcp, "error", error);
        return error;
}

STATIC int
dcvn_strputmsg(
	bhv_desc_t	*bdp,
	struct strbuf	*mctl,
	struct strbuf	*mdata,
	unsigned char	pri,
	int		flag,
	int		fmode)
{
	int             error;
	/* REFERENCED */
	int             msgerr;
	dcvn_t		*dcp = BHV_TO_DCVN(bdp);

	DCSTAT(dcvn_strputmsg);
	msgerr = invk_dsvn_strputmsg(DCVN_TO_SERVICE(dcp), DCVN_TO_OBJID(dcp), 
			      mctl, mdata, pri, flag, fmode, &error);
	ASSERT(!msgerr);
	DVN_KTRACE4(DVN_TRACE_DCVOP, "dcvn_strgetmsg", dcp, "error", error);
        return error;
}

/* client-side ops */
vnodeops_t cfs_vnodeops = {
	BHV_IDENTITY_INIT(VN_BHV_CFS,VNODE_POSITION_CFS_DC),
	dcvn_open,
	dcvn_close,
	dcvn_read,
	dcvn_write,
	dcvn_ioctl,
	(vop_setfl_t)fs_noerr,
	dcvn_getattr,
	dcvn_setattr,
	dcvn_access,
	dcvn_lookup,
	dcvn_create,
	dcvn_remove,
	dcvn_link,
	dcvn_rename,
	dcvn_mkdir,
	dcvn_rmdir,
	dcvn_readdir,
	dcvn_symlink,
	dcvn_readlink,
	dcvn_fsync,
	dcvn_inactive,
	dcvn_fid,
	dcvn_fid2,
	dcvn_rwlock,
	dcvn_rwunlock,
	dcvn_seek,
	dcvn_cmp,
	dcvn_frlock,
	(vop_realvp_t)fs_nosys,
	dcvn_bmap,
	(vop_strategy_t)fs_noval,
	dcvn_map,
	dcvn_addmap,
	dcvn_delmap,
	dcvn_poll,
	(vop_dump_t)fs_nosys,
	fs_pathconf,
	dcvn_allocstore,
	dcvn_fcntl,
	dcvn_reclaim,
	dcvn_attr_get,
	dcvn_attr_set,
	dcvn_attr_remove,
	(vop_attr_list_t)fs_nosys,
	dcvn_cover,	
	(vop_link_removed_t)fs_noval,
	dcvn_vnode_change,
	fs_tosspages,
	fs_flushinval_pages,
	fs_flush_pages,
	fs_invalfree_pages,
	fs_pages_sethole,
	(vop_commit_t)fs_nosys,
	(vop_readbuf_t)fs_nosys,
	dcvn_strgetmsg,
	dcvn_strputmsg,
};

/*
 *********************** Token module callouts ******************************
 */

void
dcvn_count_obt_opti(
	tk_set_t obtset)
{
	if (TK_GET_CLASS(obtset, DVN_EXIST_NUM)) DCSTAT(DVN_EXIST_obt_opti);
	if (TK_GET_CLASS(obtset, DVN_PCACHE_NUM)) DCSTAT(DVN_PCACHE_obt_opti);
	if (TK_GET_CLASS(obtset, DVN_PSEARCH_NUM)) DCSTAT(DVN_PSEARCH_obt_opti);
	if (TK_GET_CLASS(obtset, DVN_NAME_NUM)) DCSTAT(DVN_NAME_obt_opti);
	if (TK_GET_CLASS(obtset, DVN_ATTR_NUM)) DCSTAT(DVN_ATTR_obt_opti);
	if (TK_GET_CLASS(obtset, DVN_TIMES_NUM)) DCSTAT(DVN_TIMES_obt_opti);
	if (TK_GET_CLASS(obtset, DVN_EXTENT_NUM)) DCSTAT(DVN_EXTENT_obt_opti);
	if (TK_GET_CLASS(obtset, DVN_BIT_NUM)) DCSTAT(DVN_BIT_obt_opti);
	if (TK_GET_CLASS(obtset, DVN_DIT_NUM)) DCSTAT(DVN_DIT_obt_opti);
}
void
dcvn_count_obtain(
	tk_set_t obtset)
{
	if (TK_GET_CLASS(obtset, DVN_EXIST_NUM)) DCSTAT(DVN_EXIST_obtain);
	if (TK_GET_CLASS(obtset, DVN_PCACHE_NUM)) DCSTAT(DVN_PCACHE_obtain);
	if (TK_GET_CLASS(obtset, DVN_PSEARCH_NUM)) DCSTAT(DVN_PSEARCH_obtain);
	if (TK_GET_CLASS(obtset, DVN_NAME_NUM)) DCSTAT(DVN_NAME_obtain);
	if (TK_GET_CLASS(obtset, DVN_ATTR_NUM)) DCSTAT(DVN_ATTR_obtain);
	if (TK_GET_CLASS(obtset, DVN_TIMES_NUM)) DCSTAT(DVN_TIMES_obtain);
	if (TK_GET_CLASS(obtset, DVN_EXTENT_NUM)) DCSTAT(DVN_EXTENT_obtain);
	if (TK_GET_CLASS(obtset, DVN_BIT_NUM)) DCSTAT(DVN_BIT_obtain);
	if (TK_GET_CLASS(obtset, DVN_DIT_NUM)) DCSTAT(DVN_DIT_obtain);
}
/*
 * Token client callout for obtaining a token.
 */
/* ARGSUSED */
static void
dcvn_tcif_obtain(
	void		*dcobj,
	tk_set_t	obtset,
	tk_set_t	retset,
	tk_disp_t	why,
	tk_set_t	*refset)	/* out */
{
        dcvn_t		*dcp = (dcvn_t *) dcobj;
	tk_set_t	grantset;
	tk_set_t	already_obtset;
	vattr_t		vattr;
	int 		cxfs_flags;
	char		*cxfs_buff;
	size_t		cxfs_count;
	/* REFERENCED */
	int		msgerr;

	dcvn_count_obtain(obtset);

	cxfs_count = 0; cxfs_flags = 0; cxfs_buff = NULL;

	msgerr = invk_dsvn_obtain(DCVN_TO_SERVICE(dcp), DCVN_TO_OBJID(dcp),
			cellid(), retset, why, obtset, &grantset,
			&already_obtset, refset, &vattr, &cxfs_flags,
			&cxfs_buff, &cxfs_count, &dcp->dcv_error);
	ASSERT(!msgerr);
	
	ASSERT(*refset == TK_NULLSET);

	if (grantset&DVN_PSEARCH_READ) {
		VN_FLAGCLR(DCVN_TO_VNODE(dcp), VREMAPPING);
		dcp->dcv_vrgen++;
	}

	DVN_KTRACE10(DVN_TRACE_TOKEN, "dcvn_tcif_obtain", dcp, "obtset",
			obtset, "grant", grantset, "had", already_obtset,
			"error", dcp->dcv_error);

	if (!dcp->dcv_error) {
		tk_set_t gotset;

		gotset = TK_ADD_SET(grantset, already_obtset);
		dc_putobjects(gotset, &dcp->dcv_vattr, &vattr);

		if (dcp->dcv_dcxvn) {
			cxfs_dcxvn_obtain(dcp->dcv_dcxvn, obtset,
				grantset, already_obtset, *refset,
				cxfs_flags, cxfs_buff, cxfs_count);
		}
	} else if (dcp->dcv_error && cxfs_count) {
		ASSERT(cxfs_buff);
		kmem_free((caddr_t)cxfs_buff, cxfs_count);
	}
}

void
dc_putobjects(
	tk_set_t gotset,
	vattr_t *dvap,
	vattr_t *svap)
{
	int gen_attrs = TK_GET_CLASS(gotset, DVN_ATTR_NUM);
	int ext_attrs = TK_GET_CLASS(gotset, DVN_EXTENT_NUM);
	int times_attrs = TK_GET_CLASS(gotset, DVN_TIMES_NUM);

	ASSERT(dvap && svap);
	if (!gen_attrs && !ext_attrs && !times_attrs)
		return;

	if (times_attrs) {
		DCSTAT(dc_put_times);
		dvap->va_atime.tv_sec = svap->va_atime.tv_sec;
		dvap->va_atime.tv_nsec = svap->va_atime.tv_nsec;
		dvap->va_mtime.tv_sec = svap->va_mtime.tv_sec;
		dvap->va_mtime.tv_nsec = svap->va_mtime.tv_nsec;
		dvap->va_ctime.tv_sec = svap->va_ctime.tv_sec;
		dvap->va_ctime.tv_nsec = svap->va_ctime.tv_nsec;
	}

	if (ext_attrs) {
		DCSTAT(dc_put_ext);
		dvap->va_size = svap->va_size;
		dvap->va_nblocks = svap->va_nblocks;
		dvap->va_extsize = svap->va_extsize;
		dvap->va_nextents = svap->va_nextents;
	}

	if (gen_attrs) {
		DCSTAT(dc_put_gen);
		dvap->va_fsid = svap->va_fsid;
		dvap->va_nodeid = svap->va_nodeid;
		dvap->va_nlink = svap->va_nlink;
		dvap->va_type = svap->va_type;
		dvap->va_mode = svap->va_mode;
		dvap->va_uid = svap->va_uid;
		dvap->va_gid = svap->va_gid;
		dvap->va_projid = svap->va_projid;
		dvap->va_rdev = svap->va_rdev;
		dvap->va_blksize = svap->va_blksize;
		dvap->va_xflags = svap->va_xflags;
		dvap->va_anextents = svap->va_anextents;
		dvap->va_gencount = svap->va_gencount;
		dvap->va_vcode = svap->va_vcode;
	}
}
/*
 * Token client callout for returning a token.
 */
/* ARGSUSED */
static void
dcvn_tcif_return(
	tkc_state_t	tclient,
	void		*dcobj,
	tk_set_t	retset,
	tk_set_t	unknownset,
	tk_disp_t	why)
{
        dcvn_t		*dcp = (dcvn_t *) dcobj;

	DVN_KTRACE10(DVN_TRACE_TOKEN, "dcvn_tcif_return", dcp, "retset", retset,
		"unknownset", unknownset, "why", why, "client", tclient);
	/*
	 * Should only be returning the existence token via this route
	 * if the vnode's VEVICT flag is set.
	 */
	ASSERT((retset & DVN_EXIST_READ) ? 
	       (DCVN_TO_VNODE(dcp)->v_flag & VEVICT) : 1);

	dcvn_return(dcp, retset, unknownset, why);
}

#ifdef NOTYET
external_alenlist_t
uio_to_plist (
	uio_t	 *uiop,
	int	kind,
	int 	*alen,
	struct iovec *iovp,
	int	*rerror)

{
	int	error;
	external_alenlist_t exlist, nexlist;
	int npgs, i, mxpgs, realpgs;
	alenlist_t	alist;
	struct iovec *saved_iovp = iovp;

	/*
	 * compute max list size
	 */
	mxpgs = 0;
	for (i=0; i<uiop->uio_iovcnt; i++) {
		mxpgs += max(1, numpages(uiop->uio_iov[i].iov_base, 
			uiop->uio_iov[i].iov_len));
	}

	i = mxpgs * sizeof(struct external_alenlist);
	nexlist = exlist = (external_alenlist_t)kmem_alloc (i, KM_SLEEP);
	*alen = i;

	realpgs = 0;
	for (i=0; i<uiop->uio_iovcnt; i++) {
		*iovp = uiop->uio_iov[i];
		iovp++;
		if ((uiop->uio_segflg == UIO_SYSSPACE) || 
		    (uiop->uio_segflg == UIO_NOSPACE)) {
			alist = kvaddr_to_alenlist(NULL, 
					uiop->uio_iov[i].iov_base,
					uiop->uio_iov[i].iov_len, 0);
		} else {
			if (uiop->uio_iov[i].iov_len == 0) {
				nexlist->addr = 0;
				nexlist->len = 0;
				nexlist++;
				realpgs += 1;
				continue;
			}
			error = useracc(uiop->uio_iov[i].iov_base,
					uiop->uio_iov[i].iov_len, 
					kind|B_PHYS, NULL);
			if (error) {
				plist_free (i, kind|B_PHYS, saved_iovp);
				*rerror = error;
				kmem_free (exlist, *alen);
				return(NULL);
			}
			alist = uvaddr_to_alenlist(NULL, 
					uiop->uio_iov[i].iov_base,
					uiop->uio_iov[i].iov_len, 0);
		}
		ASSERT(alist);
		error = alenlist_cursor_init(alist, 0, NULL);
		ASSERT(error == ALENLIST_SUCCESS);
		npgs = alenlist_size(alist);

		realpgs += npgs;
	
		while (alenlist_get(alist, NULL, 0, &nexlist->addr, 
			&nexlist->len, 0) == ALENLIST_SUCCESS) {
			nexlist++;
			npgs--;
		}
		ASSERT(npgs == 0);
		alenlist_done(alist);
	}

	return(exlist);
}

void
plist_free (
	int			uio_iovcnt,
	int			flags,
	struct iovec		*iovp)
{
	int i;

	for (i=0; i<uio_iovcnt; i++) {
		if (iovp->iov_len) {
			unuseracc(iovp->iov_base,
			    iovp->iov_len, flags);
		}
		iovp++;
	}
}
#endif
