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
#ident	"$Id: dsvn.c,v 1.78 1997/09/23 20:14:55 jh Exp $"

/*
 * Server-side vnode message interfaces for the Cell File System.
 */
#include <sys/fs_subr.h>
#include <sys/buf.h>
#include <sys/fcntl.h>
#include <sys/flock.h>
#include <sys/pathname.h>
#include <sys/pvnode.h>
#include <sys/uthread.h>
#include <ksys/vop_backdoor.h>
#include <ksys/cred.h>
#include <sys/idbgentry.h>
#include <sys/pfdat.h>
#include <sys/vnode_private.h>
#include <sys/alenlist.h>
#include <fs/cell/cxfs_intfc.h>
#include <fs/cell/cfs_intfc.h>
#include "dvn.h"
#include "I_dsvn_stubs.h"


extern void plist_to_uio (external_alenlist_t, uio_t*, struct iovec *);
extern void kvaddr_free (int, struct iovec *);
int ds_getobjects(dsvn_t *, vattr_t *, char **, size_t *, int *,
	tk_set_t, tk_set_t, tk_set_t, void **, bhv_desc_t *);
void ds_update_times(dsvn_t *, bhv_desc_t *, vnode_t *, ds_times_t *);
int  ds_timespeccmp(timespec_t *, timespec_t *);

void
I_dsvn_open(
        objid_t		objid,
        int		mode,
	credid_t	credid,
	cell_t		client,
	cfs_handle_t	*handlep,	/* out */
	tk_set_t	*obtset,	/* out */
        enum vtype	*vtype,		/* out */
	dev_t		*rdev,		/* out */
	int		*flags,		/* out */
	int		*error)		/* out */
{
	bhv_desc_t	*bdp, *nbdp;
	vnode_t		*vp, *newvp;
	tk_set_t	refset, hadset;
	dsvn_t		*dsp;
	cred_t		*credp;
	/* REFERENCED */
	int		dummy;

	DSSTAT(I_dsvn_open);
	bdp = OBJID_TO_BHV(objid);
	vp = BHV_TO_VNODE(bdp);

	credp = CREDID_GETCRED(credid);
	/*
	 * If PVOP_OPEN returns a new vnode it will have vn_rele'd our
	 * vnode reference.  This is a bad thing because the client-side
	 * is holding an existence token.  Hence, take an additional 
	 * reference now that PVOP_OPEN may consume; if it doesn't then
	 * we vn_rele it below.
	 */
	VN_HOLD(vp);

	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp)); /* prevent behavior insert/remove */

	newvp = vp;
	PV_NEXT(bdp, nbdp, vop_open);
	PVOP_OPEN(nbdp, &newvp, mode, credp, *error);
	if (*error) {
		VN_RELE(vp);			/* release ref. taken above */
		goto out;
	}

	/*
	 * Account for fact that the returned vnode may be different
	 * than the one passed in.  The logic in this case is very
	 * similar to the lookup case.
	 */
	if (newvp == vp) {
		VN_RELE(vp);			/* release ref. taken above */
		CFS_HANDLE_MAKE_NULL(*handlep);	/* indicate no vnode change */
	} else {

		/*
		 * - Get a ds.
		 * - Get an existence token.
		 * - Set handle, type, rdev, flags out args.
		 */
		dsp = dsvn_vnode_to_dsvn(newvp);

		/*
		 * Try to get the existence token.
		 *
		 * If we can't because the client is
		 * racing with itself (obtaining or returning), then it will
		 * retry.  In that case the client uses the returned
		 * handle as a hint. Also, we must undo the open from
		 * above. 
		 */

		tks_obtain(dsp->dsv_tserver, client, DVN_EXIST_READ,
			obtset, &refset, &hadset);

		DSVN_HANDLE_MAKE(*handlep, dsp);
		*vtype = newvp->v_type;
		*rdev = newvp->v_rdev;

		/*
		 * Get flags for client.  Must always return associated 
		 * flags when returning an existence token.
		 */
		DSVN_CLIENT_FLAGS(dsp, newvp, *flags);

		/*
		 * Account for fact that each existence token holds a
		 * vnode reference.  Also, undo open if didn't get it.
		 */
		if (!(*obtset & DVN_EXIST_READ)) {
			PVOP_CLOSE(nbdp, mode, L_TRUE, credp, dummy);
			VN_RELE(newvp);
		}
	}
 out:
	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));
	DVN_KTRACE8(DVN_TRACE_DSRPC, "I_dsvn_open", objid, "error", *error,
		"newvp", newvp, "vp", vp);
	if (credp)
		crfree(credp);
}

void
I_dsvn_close(
        objid_t		objid,
	int		flag,
	lastclose_t	lastclose,
	credid_t	credid,
	int		*error)
{
	bhv_desc_t	*bdp, *nbdp;
	vnode_t		*vp;
	cred_t		*credp;

	DSSTAT(I_dsvn_close);
	bdp = OBJID_TO_BHV(objid);
	vp = BHV_TO_VNODE(bdp);
	credp = CREDID_GETCRED(credid);

	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp)); /* prevent behavior insert/remove */

	PV_NEXT(bdp, nbdp, vop_close);
	PVOP_CLOSE(nbdp, flag, lastclose, credp, *error);

	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));
	if (credp)
		crfree(credp);
	DVN_KTRACE8(DVN_TRACE_DSRPC, "I_dsvn_close", objid, "error", *error,
		"flag", flag, "lastclose", lastclose);
}

/*ARGSUSED*/
void
I_dsvn_read(
        objid_t 	objid,
	uio_t		*uiop,
	iovec_t		*iovp,
	size_t		iov_len,
	int		ioflag,
	credid_t	credid,
	flid_t		*fl,
	off_t		*uio_offset,
	short		*uio_sigpipe,
	ssize_t		*uio_resid,
	int		*error)
{
	dsvn_t		*dsp;
	bhv_desc_t	*bdp, *nbdp;
	vnode_t		*vp;
	cred_t		*credp;

	DSSTAT(I_dsvn_read);
	uiop->uio_iov = iovp;
	if(uiop->uio_segflg == UIO_SYSSPACE)
		uiop->uio_segflg = UIO_USERSPACE;

	bdp = OBJID_TO_BHV(objid);
	vp = BHV_TO_VNODE(bdp);
	credp = CREDID_GETCRED(credid);
	dsp = BHV_TO_DSVN(bdp);


	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp)); /* prevent behavior insert/remove */
	PV_NEXT(bdp, nbdp, vop_read);
	PVOP_READ(nbdp, uiop, ioflag, credp, fl, *error);
	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));

	if (credp)
		crfree(credp);
	*uio_offset = uiop->uio_offset;
	*uio_sigpipe = uiop->uio_sigpipe;
	*uio_resid = uiop->uio_resid;
	/* Cause the read attribute tokens to be revoked */
	if (!*error) {
		tkc_acquire1(dsp->dsv_tclient, DVN_TIMES_SHARED_WRITE_1);
		tkc_release1 (dsp->dsv_tclient, DVN_TIMES_SHARED_WRITE_1);
	}
	DVN_KTRACE6(DVN_TRACE_DSRPC, "I_dsvn_read", dsp, "resid", *uio_resid,
		"error", *error);
}

/*ARGSUSED*/
void
I_dsvn_page_read(
        objid_t 	objid,
	uio_t		*uiop,
	int		ioflag,
	credid_t	credid,
	flid_t		*fl,
	pfn_t		client_pfn,
	cell_t		client_cell,
	pfn_t		*server_pfn,
	int		*pf_flags,
	ssize_t		*uio_resid,
	int		*already_exported,
	int		*error)
{
	bhv_desc_t	*bdp, *nbdp;
	vnode_t		*vp;
	iovec_t		iov;
	pfd_t		*pfd;
	cred_t		*credp=NULL;
	dsvn_t		*dsp;
	pgno_t		lpn = btoc(uiop->uio_offset);

	DSSTAT(I_dsvn_page_read);
	ASSERT (uiop->uio_segflg == UIO_NOSPACE && uiop->uio_resid == NBPP);

	uiop->uio_iov = &iov;
	iov.iov_base = (caddr_t) 0xdeadbeefL;
	iov.iov_len = NBPP;

	bdp = OBJID_TO_BHV(objid);
	dsp = BHV_TO_DSVN(bdp);
	vp = BHV_TO_VNODE(bdp);
	*error = 0;

	/*
	 * If the data is already in the page cache, just export it
	 * back to the client. 
	 */
export_again:

	*server_pfn = 0;
	if (vp->v_flag & VREMAPPING)
		pfd = NULL;
	else
		pfd = pfind((void*)vp,  lpn, VM_ATTACH); 
	if (pfd) {
		if (!(pfd->pf_flags & P_DONE))
			pagewait(pfd);
		if (pfd->pf_flags & P_BAD) {
			pagefree(pfd);
			goto export_again;
		}

		*server_pfn = export_page(pfd, client_cell, already_exported);
		*pf_flags = pfd->pf_flags;
		pagefree(pfd);
		*uio_resid = (*error == EAGAIN) ? uiop->uio_resid : 0;
		*error = 0;
		goto done;
	} else if (*error == EAGAIN) {
		goto done;
	}

	credp = CREDID_GETCRED(credid);

	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp)); /* prevent behavior insert/remove */
	PV_NEXT(bdp, nbdp, vop_read);
	PVOP_READ(nbdp, uiop, ioflag, credp, fl, *error);
	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));

	/*
	 * The data should now be in the page cache.
	 * Try again to find it and export it back to the client.
	 */
	if (*error == 0 && uiop->uio_resid != NBPP) { 
		*error = EAGAIN;
		goto export_again;
	}

	/*
	 * Attempt to read beyond EOI. No page assigned.
	 */
	*uio_resid = uiop->uio_resid;

done:
	DVN_KTRACE6(DVN_TRACE_DSRPC, "I_dsvn_page_read", dsp,
		 "resid", *uio_resid, "error", *error);
	if (credp)
		crfree(credp);

	/* Cause the read attribute tokens to be revoked */
	if (!*error) {
		tkc_acquire1(dsp->dsv_tclient, DVN_TIMES_SHARED_WRITE_1);
		tkc_release1 (dsp->dsv_tclient, DVN_TIMES_SHARED_WRITE_1);
	}
}

/*ARGSUSED*/
void
I_dsvn_write(
        objid_t 	objid,
	uio_t		*uiop,
	iovec_t		*iovp,
	size_t		iov_len,
	int		ioflag,
	credid_t	credid,
	flid_t		*fl,
	off_t		*uio_offset,
	short		*uio_sigpipe,
	ssize_t		*uio_resid,
	int		*error)
{
	bhv_desc_t	*bdp, *nbdp;
	vnode_t		*vp;
	cred_t		*credp;
	dsvn_t		*dsp;
	tk_set_t	tkacq;

	DSSTAT(I_dsvn_write);
	uiop->uio_iov = iovp;
	if(uiop->uio_segflg == UIO_SYSSPACE)
		uiop->uio_segflg = UIO_USERSPACE;

	bdp = OBJID_TO_BHV(objid);
	vp = BHV_TO_VNODE(bdp);
	credp = CREDID_GETCRED(credid);

	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp)); /* prevent behavior insert/remove */

	PV_NEXT(bdp, nbdp, vop_write);
	dsp = BHV_TO_DSVN(bdp);
	tkc_acquire(dsp->dsv_tclient, DVN_WRITE_TOKENS, &tkacq);
	ASSERT(tkacq == DVN_WRITE_TOKENS);
	PVOP_WRITE(nbdp, uiop, ioflag, credp, fl, *error);
	tkc_release (dsp->dsv_tclient, DVN_WRITE_TOKENS);

	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));

	*uio_offset = uiop->uio_offset;
	*uio_sigpipe = uiop->uio_sigpipe;
	*uio_resid = uiop->uio_resid;

	if (credp)
		crfree(credp);
	DVN_KTRACE6(DVN_TRACE_DSRPC, "I_dsvn_write", dsp,
		"resid", *uio_resid, "error", *error);
}

/*ARGSUSED*/
void
I_dsvn_list_read(
        objid_t 	objid,
	uio_t		*uiop,
	iovec_t		*iovp,
	size_t		iov_len,
	external_alenlist_t alist,
	size_t		alist_len,
	int		ioflag,
	credid_t	credid,
	flid_t		*fl,
	off_t		*uio_offset,
	short		*uio_sigpipe,
	ssize_t		*uio_resid,
	int		*error)
{
	bhv_desc_t	*bdp, *nbdp;
	vnode_t		*vp;
	struct iovec	saved_iov[16];
	cred_t		*credp;
	dsvn_t		*dsp;

	DSSTAT(I_dsvn_list_read);

	uiop->uio_iov = iovp;
	plist_to_uio (alist, uiop, saved_iov);
	uiop->uio_segflg = UIO_SYSSPACE;

	bdp = OBJID_TO_BHV(objid);
	vp = BHV_TO_VNODE(bdp);
	credp = CREDID_GETCRED(credid);

	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp)); /* prevent behavior insert/remove */

	PV_NEXT(bdp, nbdp, vop_read);
	dsp = BHV_TO_DSVN(bdp);
	PVOP_READ(nbdp, uiop, ioflag, credp, fl, *error);

	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));

	kvaddr_free(uiop->uio_iovcnt, saved_iov);

	*uio_offset = uiop->uio_offset;
	*uio_sigpipe = uiop->uio_sigpipe;
	*uio_resid = uiop->uio_resid;

	if (credp)
		crfree(credp);
	/* Cause the read attribute tokens to be revoked */
	if (!*error) {
		tkc_acquire1(dsp->dsv_tclient, DVN_TIMES_SHARED_WRITE_1);
		tkc_release1 (dsp->dsv_tclient, DVN_TIMES_SHARED_WRITE_1);
	}
	DVN_KTRACE6(DVN_TRACE_DSRPC, "I_dsvn_list_read", dsp,
			"resid", *uio_resid, "error", *error);
}

/*ARGSUSED*/
void
I_dsvn_list_write(
        objid_t 	objid,
	uio_t		*uiop,
	iovec_t		*iovp,
	size_t		iov_len,
	external_alenlist_t alist,
	size_t		alist_len,
	int		ioflag,
	credid_t	credid,
	flid_t		*fl,
	off_t		*uio_offset,
	short		*uio_sigpipe,
	ssize_t		*uio_resid,
	int		*error)
{
	dsvn_t		*dsp;
	bhv_desc_t	*bdp, *nbdp;
	vnode_t		*vp;
	cred_t		*credp;
	struct iovec	saved_iov[16];
	tk_set_t	tkacq;

	DSSTAT(I_dsvn_list_write);
	uiop->uio_iov = iovp;
	plist_to_uio (alist, uiop, saved_iov);
	uiop->uio_segflg = UIO_SYSSPACE;

	bdp = OBJID_TO_BHV(objid);
	vp = BHV_TO_VNODE(bdp);
	credp = CREDID_GETCRED(credid);

	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp)); /* prevent behavior insert/remove */

	PV_NEXT(bdp, nbdp, vop_write);
	dsp = BHV_TO_DSVN(bdp);
	tkc_acquire(dsp->dsv_tclient, DVN_WRITE_TOKENS, &tkacq);
	ASSERT(tkacq == DVN_WRITE_TOKENS);
	PVOP_WRITE(nbdp, uiop, ioflag, credp, fl, *error);
	tkc_release (dsp->dsv_tclient, DVN_WRITE_TOKENS);

	kvaddr_free(uiop->uio_iovcnt, saved_iov);

	*uio_offset = uiop->uio_offset;
	*uio_sigpipe = uiop->uio_sigpipe;
	*uio_resid = uiop->uio_resid;
	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));

	if (credp)
		crfree(credp);
	DVN_KTRACE6(DVN_TRACE_DSRPC, "I_dsvn_list_write", dsp,
			"resid", *uio_resid, "error", *error);
}

void
I_dsvn_ioctl(
        objid_t 	objid,
	int 		cmd,
	void 		*arg,
	int 		flag,
	credid_t	credid,
	int 		*rvalp,
        vopbd_t         *vbds,
	cfs_handle_t    *vbds_vnh,
	int		*error)
{
	bhv_desc_t	*bdp, *nbdp;
	vnode_t		*vp;
	cred_t		*credp;
	dsvn_t		*dsp;
	tk_set_t	tkacq;

	DSSTAT(I_dsvn_ioctl);
	bdp = OBJID_TO_BHV(objid);
	vp = BHV_TO_VNODE(bdp);
	credp = CREDID_GETCRED(credid);
	dsp = BHV_TO_DSVN(bdp);

	vbds->vopbd_req = VOPBDR_NIL;

	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp)); /* prevent behavior insert/remove */

	/*
	 * For now, assume the worst case that all ioctls change
	 * everything.
	 */
	tkc_acquire(dsp->dsv_tclient, DVN_WRITE_TOKENS, &tkacq);
	ASSERT(tkacq == DVN_WRITE_TOKENS);
	PV_NEXT(bdp, nbdp, vop_ioctl);
	PVOP_IOCTL(nbdp, cmd, arg, flag, credp, rvalp, vbds, *error);
	tkc_release (dsp->dsv_tclient, DVN_WRITE_TOKENS);

	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));

	if (credp)
		crfree(credp);

	DVN_KTRACE6(DVN_TRACE_DSRPC, "I_dsvn_ioctl", BHV_TO_DSVN(bdp),
		"rval", rvalp ? *rvalp : 0, "error", *error);

	if (*error)
		return;

	switch (vbds->vopbd_req) {
	case VOPBDR_NIL:
		break;

	case VOPBDR_ASSIGN: {
	        vnode_t *xvp = vbds->vopbd_parm.vopbdp_assign.vopbda_vp;

	        vbds->vopbd_parm.vopbdp_assign.vopbda_vnum = xvp->v_number;
		cfs_vnexport(xvp, vbds_vnh);
		break;
	}

	default:
	        panic("unsupported cross-cell operation");
	}

}


void
I_dsvn_strgetmsg(
        objid_t 	objid,
	struct strbuf	*mctl,
	struct strbuf	*mdata,
	unsigned char	*prip,
	int		*flagsp,
	int		fmode,
	union rval	*rvp,
	int		*errorp)
{
	DSSTAT(I_dsvn_strgetmsg);
	dsvn_strgetmsg(OBJID_TO_BHV(objid),
			      mctl, mdata, prip, flagsp, fmode, rvp, errorp);
	DVN_KTRACE4(DVN_TRACE_DSRPC, "I_dsvn_strgetmsg", OBJID_TO_BHV(objid),
		"error", *errorp);
}


void
I_dsvn_strputmsg(
        objid_t 	objid,
	struct strbuf	*mctl,
	struct strbuf	*mdata,
	unsigned char	pri,
	int		flag,
	int		fmode,
	int		*errorp)
{
	DSSTAT(I_dsvn_strputmsg);
	dsvn_strputmsg(OBJID_TO_BHV(objid),
			      mctl, mdata, pri, flag, fmode, errorp);
	DVN_KTRACE4(DVN_TRACE_DSRPC, "I_dsvn_strgetmsg", OBJID_TO_BHV(objid),
			"error", *errorp);
}


/*
 * The following should only get called for file systems which
 * aren't being cached on the client.
 *
 * So no need to get tokens.
 */
void
I_dsvn_getattr(
        objid_t 	objid,
	vattr_t		*vap,
	int		flags,
	credid_t	credid,
	int		*error)
{
	bhv_desc_t	*bdp, *nbdp;
	vnode_t		*vp;
	cred_t		*credp;

	DSSTAT(I_dsvn_getattr);
	bdp = OBJID_TO_BHV(objid);
	vp = BHV_TO_VNODE(bdp);
	credp = CREDID_GETCRED(credid);

	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp)); /* prevent behavior insert/remove */

	PV_NEXT(bdp, nbdp, vop_getattr);
	PVOP_GETATTR(nbdp, vap, flags, credp, *error);

	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));

	if (credp)
		crfree(credp);
	DVN_KTRACE4(DVN_TRACE_DSRPC, "I_dsvn_getattr", BHV_TO_DSVN(bdp),
		"error", *error);
}

void
I_dsvn_setattr(
        objid_t 	objid,
	vattr_t		*vap,
	int		flags,
	credid_t	credid,
	int		*error)
{
	bhv_desc_t	*bdp, *nbdp;
	vnode_t		*vp;
	cred_t		*credp;
	dsvn_t		*dsp;
	int		mask;
	tk_set_t	tokens_needed = TK_NULLSET, tkacq;

	DSSTAT(I_dsvn_setattr);

	bdp = OBJID_TO_BHV(objid);
	vp = BHV_TO_VNODE(bdp);
	credp = CREDID_GETCRED(credid);
	ASSERT(vap && vp && bdp && objid);

	/*
	 * First, figure out what tokens we need depending on what
	 * is being set.
	 */

	mask = vap->va_mask;
	if ((mask & AT_ATTR_PROTECTED) != 0)
		tokens_needed = TK_ADD_SET(tokens_needed, DVN_ATTR_WRITE);
	if ((mask & (AT_UPDTIMES|AT_TIMES)) != 0)
		tokens_needed = TK_ADD_SET(tokens_needed, DVN_TIMES_WRITE);
	if ((mask & AT_EXTENT_PROTECTED) != 0)
		tokens_needed = TK_ADD_SET(tokens_needed, DVN_EXTENT_WRITE);

	/*
	 * If we are about to change some attribute and we don't already
	 * need the times_write token, get the times_shared_write
	 * token since changing an attribute changes a time.
	 */

	if ((tokens_needed != TK_NULLSET) &&
		!TK_IS_IN_SET(tokens_needed, DVN_TIMES_WRITE)) {
		tokens_needed = TK_ADD_SET(tokens_needed,
					DVN_TIMES_SHARED_WRITE);
	}

	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp)); /* prevent behavior insert/remove */

	PV_NEXT(bdp, nbdp, vop_setattr);
	dsp = BHV_TO_DSVN(bdp);
	if (tokens_needed) {
		tkc_acquire(dsp->dsv_tclient, tokens_needed, &tkacq);
		ASSERT(tkacq == tokens_needed);
	}
	PVOP_SETATTR(nbdp, vap, flags, credp, *error);
	if (tokens_needed)
		tkc_release (dsp->dsv_tclient, tokens_needed);

	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));

	if (credp)
		crfree(credp);
	DVN_KTRACE6(DVN_TRACE_DSRPC, "I_dsvn_setattr", dsp, "mask", vap->va_mask,
		"error", *error);
}

/*
 * Access should only get here for file systems
 * that don't have attributes cached on the client.
 * So, no need to get tokens.
 */
void
I_dsvn_access(
        objid_t 	objid,
	int		mode,
	credid_t	credid,
	int		*error)
{
	bhv_desc_t	*bdp, *nbdp;
	vnode_t		*vp;
	cred_t		*credp;

	DSSTAT(I_dsvn_access);
	bdp = OBJID_TO_BHV(objid);
	vp = BHV_TO_VNODE(bdp);
	credp = CREDID_GETCRED(credid);

	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp)); /* prevent behavior insert/remove */

	PV_NEXT(bdp, nbdp, vop_access);
	PVOP_ACCESS(nbdp, mode, credp, *error);

	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));

	if (credp)
		crfree(credp);
	DVN_KTRACE6(DVN_TRACE_DSRPC, "I_dsvn_access", BHV_TO_DSVN(bdp),
		"mode", mode, "error", *error);
}

/* ARGSUSED */
void
I_dsvn_lookup(
        objid_t 	objid,
	char		*name,
        size_t          name_len,
	int		inflags,
	credid_t	credid,
	cell_t		client,
	cfs_handle_t	*handlep,	/* out */
	tk_set_t	*obtset,	/* out */
        enum vtype	*vtype,		/* out */
	dev_t		*rdev,		/* out */
	int		*flags,		/* out */
	vattr_t		*vattr,		/* out */
	int		*cxfs_flags,	/* out */
	char 		**cxfs_buff,	/* out */
	size_t		*cxfs_count,	/* out */
	int		*error,		/* out */
	void		**bufdesc)
{
	bhv_desc_t	*bdp, *nbdp;
	dsvn_t		*dsp;
	tk_set_t	refset, hadset;
	vnode_t		*vp, *newvp = NULL;
	vnode_t		*rdir = NULL;
        pathname_t      pn;
	cred_t		*credp;

	DSSTAT(I_dsvn_lookup);
	*cxfs_count = 0;
	*cxfs_flags = 0;
	*bufdesc = NULL;
	bdp = OBJID_TO_BHV(objid);
	vp = BHV_TO_VNODE(bdp);
	credp = CREDID_GETCRED(credid);
        pn_setcomponent(&pn, name, name_len-1);

	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp)); /* prevent behavior insert/remove */

	PV_NEXT(bdp, nbdp, vop_lookup);
	PVOP_LOOKUP(nbdp, name, &newvp, &pn, inflags, rdir, 
		    credp, *error);

	if (*error == 0) {

		/*
		 * Support traversing from cellular to non-cellular file sys.
		 * Note that lookuppn() supports ".." in the opposite 
		 * direction.
		 */
		if (newvp->v_vfsmountedhere != NULL) {
			vnode_t *savevp = newvp;

			VN_HOLD(savevp);  
			if ((*error = traverse(&newvp)) != 0) {
				if (credp)
					crfree(credp);
				VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));
				VN_RELE(savevp);  /* from lookup */
				VN_RELE(savevp);  /* from vn_hold */
				return;
			}

			/*
			 * If cells are supported, then undo the effects
			 * of the traverse.  It's the responsibility of
			 * the client side to bring in the mount point.
			 */
			VN_RELE(newvp);  /* from traverse */
			newvp = savevp;
		}

		/*
		 * - Get a ds.
		 * - Get an existence token.
		 * - Set handle, type, rdev, flags out args.
		 */
		dsp = dsvn_vnode_to_dsvn(newvp);

		/*
		 * Try to get a set of tokens including the existence token.
		 *
		 * If we can't get the existence token
		 * because the client is racing with itself
		 * (obtaining or returning), then it will retry. In that
		 * case the client uses the returned handle as a hint.
		 */
		tks_obtain_conditional(dsp->dsv_tserver, client, DVN_EXIST_READ,
			DVN_OPTIMISTIC_TOKENS, obtset, &refset, &hadset);

		DSVN_HANDLE_MAKE(*handlep, dsp);
		*vtype = newvp->v_type;
		*rdev = newvp->v_rdev;

		/*
		 * Get flags for client.  Must always return associated 
		 * flags when returning an existence token.
		 */
		DSVN_CLIENT_FLAGS(dsp, newvp, *flags);


		/*
		 * Account for fact that each existence token holds a
		 * vnode reference.
		 */
		if (!(TK_IS_IN_SET(*obtset, DVN_EXIST_READ))) {
			ASSERT(*obtset == TK_NULLSET);
			VN_RELE(newvp);
		} else {
			*error = ds_getobjects(dsp, vattr, cxfs_buff,
				cxfs_count, cxfs_flags, *obtset, refset,
				hadset, bufdesc, DSVN_TO_BHV(dsp));

			/*
			 * If we had an error getting any objects,
			 * return any recently obtained tokens.
			 */

			if (*error && (*obtset != TK_NULLSET)) {
				tks_return(dsp->dsv_tserver, client, *obtset,
					TK_NULLSET, TK_NULLSET,
					TK_DISP_CLIENT_ALL);
				*obtset = TK_NULLSET;
			}
		}
	}

	if (credp)
		crfree(credp);
	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));
	DVN_KTRACE10(DVN_TRACE_DSRPC, "I_dsvn_lookup", dsp, "obtset", *obtset,
		"vtype", *vtype, "newvp", newvp, "error", *error);
}

/* ARGSUSED */
void
I_dsvn_lookup_done(
	char 		*cxfs_buff,
	size_t		cxfs_count,
	void		*bufdesc)
{
	DSSTAT(I_dsvn_lookup_done);
	DVN_KTRACE2(DVN_TRACE_DSRPC, "I_dsvn_lookup_done", bufdesc);
	if (bufdesc)
		cxfs_dsxvn_obtain_done(cxfs_buff, cxfs_count, bufdesc);
}

/*
 * Used by both create and mkdir.
 */
/* ARGSUSED */
void
I_dsvn_create(
        objid_t 	objid,
	char		*name,
        size_t          name_len,
	vattr_t		*vap,
	int		inflags,
	int		I_mode,
	credid_t	credid,
	cell_t		client,
	cfs_handle_t	*handlep,	/* out */
	tk_set_t	*obtset,	/* out */
	int		*flags,		/* out */
	vattr_t		*vattr,		/* out */
	int		*cxfs_flags,	/* out */
	char 		**cxfs_buff,	/* out */
	size_t		*cxfs_count,	/* out */
	int		*error,		/* out */
	void		**bufdesc)
{
	bhv_desc_t	*bdp, *nbdp;
	dsvn_t		*dsp;
	tk_set_t	refset, hadset;
	vnode_t		*vp, *newvp = NULL;
	cred_t		*credp;
	tk_set_t	tkacq;

	DSSTAT(I_dsvn_create);
	*cxfs_count = 0;
	*cxfs_flags = 0;
	*bufdesc = NULL;

	bdp = OBJID_TO_BHV(objid);
	dsp = BHV_TO_DSVN(bdp);
	vp = BHV_TO_VNODE(bdp);
	credp = CREDID_GETCRED(credid);

	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp)); /* prevent behavior insert/remove */

	/*
	 * Get the "write tokens" since we are about to write the directory.
	 */

	tkc_acquire(dsp->dsv_tclient, DVN_WRITE_TOKENS, &tkacq);
	ASSERT(tkacq == DVN_WRITE_TOKENS);

	/*
	 * Invoke Create or Mkdir depending on the type.
	 */
	if (vap->va_type == VDIR) {
		PV_NEXT(bdp, nbdp, vop_mkdir);
		PVOP_MKDIR(nbdp, name, vap, &newvp, credp, *error);
	} else {
		PV_NEXT(bdp, nbdp, vop_create);
		PVOP_CREATE(nbdp, name, vap, inflags, I_mode, &newvp, credp, 
			    *error);
	}
	tkc_release (dsp->dsv_tclient, DVN_WRITE_TOKENS);

	if (*error == 0) {

		/*
		 * - Get a ds.
		 * - Try to get token(s).
		 * - Set handle and flags out args.
		 *
		 * It's possible we can't get an existence token because, e.g.,
		 * - a lookup raced in after we created the file but before
		 *   we called tks_obtain, or
		 * - the file already existed, but the VEXCL flag was
		 *   set meaning the vop_create can succeed
		 *
		 * In this case, the client uses the returned handle as a hint.
		 */
		dsp = dsvn_vnode_to_dsvn(newvp);

		tks_obtain_conditional(dsp->dsv_tserver, client, DVN_EXIST_READ,
			DVN_OPTIMISTIC_TOKENS, obtset, &refset, &hadset);

		DSVN_HANDLE_MAKE(*handlep, dsp);

		/*
		 * Get flags for client.  Must always return associated 
		 * flags when returning an existence token.
		 */
		DSVN_CLIENT_FLAGS(dsp, newvp, *flags);


		/*
		 * Account for fact that each existence token holds a
		 * vnode reference.
		 */
		if (!(TK_IS_IN_SET(*obtset, DVN_EXIST_READ))) {
			ASSERT(*obtset == TK_NULLSET);
			VN_RELE(newvp);
		} else {
			*error = ds_getobjects(dsp, vattr, cxfs_buff,
				cxfs_count, cxfs_flags, *obtset, refset,
				hadset, bufdesc, DSVN_TO_BHV(dsp));

			/*
			 * If we had an error getting any objects,
			 * return any recently obtained tokens.
			 */

			if (*error && (*obtset != TK_NULLSET)) {
				tks_return(dsp->dsv_tserver, client, *obtset,
					TK_NULLSET, TK_NULLSET,
					TK_DISP_CLIENT_ALL);
				*obtset = TK_NULLSET;
			}
		}
	}

	if (credp)
		crfree(credp);

	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));
	DVN_KTRACE10(DVN_TRACE_DSRPC, "I_dsvn_create", dsp, "obtset", *obtset,
		"flags", *flags, "newvp", newvp, "error", *error);
}

/* ARGSUSED */
void
I_dsvn_create_done(
	char 		*cxfs_buff,
	size_t		cxfs_count,
	void		*bufdesc)
{
	DSSTAT(I_dsvn_create_done);
	DVN_KTRACE2(DVN_TRACE_DSRPC, "I_dsvn_create_done", bufdesc);
	if (bufdesc)
		cxfs_dsxvn_obtain_done(cxfs_buff, cxfs_count, bufdesc);
}

/* ARGSUSED */
void
I_dsvn_remove(
        objid_t 	objid,
	char		*name,
        size_t          name_len,
	credid_t	credid,
	int		*error)
{
	bhv_desc_t	*bdp, *nbdp;
	vnode_t		*vp;
	cred_t		*credp;
	dsvn_t		*dsp;
	tk_set_t	tkacq;

	DSSTAT(I_dsvn_remove);
	bdp = OBJID_TO_BHV(objid);
	vp = BHV_TO_VNODE(bdp);
	credp = CREDID_GETCRED(credid);

	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp)); /* prevent behavior insert/remove */

	PV_NEXT(bdp, nbdp, vop_remove);
	dsp = BHV_TO_DSVN(bdp);
	/*
	 * the read attribute token of the file being removed
	 * is revoked in VOP_LINK_REMOVED.
	 */
	tkc_acquire(dsp->dsv_tclient, DVN_WRITE_TOKENS, &tkacq);
	ASSERT(tkacq == DVN_WRITE_TOKENS);
	PVOP_REMOVE(nbdp, name, credp, *error);
	tkc_release (dsp->dsv_tclient, DVN_WRITE_TOKENS);

	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));

	if (credp)
		crfree(credp);
	DVN_KTRACE4(DVN_TRACE_DSRPC, "I_dsvn_remove", dsp, "error", *error);
}

/* ARGSUSED */
void
I_dsvn_link(
        objid_t 	tdir_objid,
	objid_t		svp_objid,
	char		*tname,
	size_t          tname_len,
	credid_t	credid,
	int		*error)
{
	bhv_desc_t	*bdp, *nbdp, *file_bdp;
	vnode_t		*vp, *svp;
	cred_t		*credp;
	dsvn_t		*dsp, *file_dsp;
	tk_set_t	tkacq;

	DSSTAT(I_dsvn_link);
	bdp = OBJID_TO_BHV(tdir_objid);
	vp = BHV_TO_VNODE(bdp);
	credp = CREDID_GETCRED(credid);
	dsp = BHV_TO_DSVN(bdp);

	svp = BHV_TO_VNODE(OBJID_TO_BHV(svp_objid));

	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp)); /* prevent behavior insert/remove */

	/*
	 * We need to potentialy get the write attribute token
	 * on the vnode since the link count is going up.
	 *
	 * Lock the behavior so that it doens't change from non-export
	 * to exported since there is a window where the wrong 
	 * link count could go out in the vattr.
	 */

	VN_BHV_READ_LOCK(VN_BHV_HEAD(svp));
	file_bdp = dsvn_vnode_to_dsbhv(svp);
	if (file_bdp)
		file_dsp = BHV_TO_DSVN(file_bdp);
	else
		file_dsp = NULL;

	tkc_acquire(dsp->dsv_tclient, DVN_WRITE_TOKENS, &tkacq);
	ASSERT(tkacq == DVN_WRITE_TOKENS);
	if (file_dsp && (file_dsp != dsp)) {
		tkc_acquire(file_dsp->dsv_tclient, DVN_ATTR_MOD_TOKENS,
				&tkacq);
		ASSERT(tkacq == DVN_ATTR_MOD_TOKENS);
	}

	PV_NEXT(bdp, nbdp, vop_link);
	PVOP_LINK(nbdp, svp, tname, credp, *error);

	if (file_dsp && (file_dsp != dsp)) {
		tkc_release(file_dsp->dsv_tclient, DVN_ATTR_MOD_TOKENS);
	}
	tkc_release (dsp->dsv_tclient, DVN_WRITE_TOKENS);

	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(svp));
	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));

	if (credp)
		crfree(credp);
	DVN_KTRACE8(DVN_TRACE_DSRPC, "I_dsvn_link", dsp,
		"file_dsp", file_dsp, "vp", vp, "error", *error);
}

/* ARGSUSED */
void
I_dsvn_rename(
        objid_t 	objid,
	char		*sname,
        size_t          sname_len,
	cfs_handle_t	*tdir_handlep,	/* handle for target dir */
	char		*tname,
        size_t          tname_len,
	credid_t	credid,
	int		*error)
{
	bhv_desc_t	*bdp, *nbdp, *tdsbdp;
	vnode_t		*vp, *tdir_vp;
	pathname_t	tpn;
	cred_t		*credp;
	dsvn_t		*sdsp, *tdsp = NULL;
	tk_set_t	tkacq;
	int		same;

	DSSTAT(I_dsvn_rename);
	bdp = OBJID_TO_BHV(objid);
	vp = BHV_TO_VNODE(bdp);
	pn_setcomponent(&tpn, tname, tname_len-1);

	/*
	 * Map from tdir_handle to a vnode.
	 */
	cfs_vnimport(tdir_handlep, &tdir_vp);
	if (tdir_vp == NULL) {
		*error = EMIGRATED;	/* raced with migration */
		return;
	}
	credp = CREDID_GETCRED(credid);

	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp)); /* prevent behavior insert/remove */

	sdsp = BHV_TO_DSVN(bdp);
	tkc_acquire(sdsp->dsv_tclient, DVN_WRITE_TOKENS, &tkacq);
	ASSERT(tkacq == DVN_WRITE_TOKENS);

	if (vn_cmp(vp, tdir_vp)) {
		same = 1;
	} else {
			/* prevent behavior insert/remove */
		same = 0;
		VN_BHV_READ_LOCK(VN_BHV_HEAD(tdir_vp));
		tdsbdp = dsvn_vnode_to_dsbhv(tdir_vp);
		if (tdsbdp && (tdsbdp != bdp)) {
			tdsp = BHV_TO_DSVN(tdsbdp);
			tkc_acquire(tdsp->dsv_tclient, DVN_WRITE_TOKENS, &tkacq);
			ASSERT(tkacq == DVN_WRITE_TOKENS);
		}
	}

	PV_NEXT(bdp, nbdp, vop_rename);
	PVOP_RENAME(nbdp, sname, tdir_vp, tname, &tpn, credp, *error);

	if (!same) {
		if (tdsp)
			tkc_release(tdsp->dsv_tclient, DVN_WRITE_TOKENS);
		VN_BHV_READ_UNLOCK(VN_BHV_HEAD(tdir_vp));
	}

	tkc_release (sdsp->dsv_tclient, DVN_WRITE_TOKENS);
	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));

	VN_RELE(tdir_vp);		/* ref from cfs_vnimport */

	if (credp)
		crfree(credp);
	DVN_KTRACE8(DVN_TRACE_DSRPC, "I_dsvn_rename", sdsp,
		"tdsp", tdsp, "same", same, "error", *error);
}

/* ARGSUSED */
void
I_dsvn_rmdir(
        objid_t 	objid,
	char		*name,
	size_t          name_len,
	cfs_handle_t	*cdir_handlep,	/* handle for current dir */
	credid_t	credid,
	int		*error)
{
	bhv_desc_t	*bdp, *nbdp;
	vnode_t		*vp, *cdir_vp;
	cred_t		*credp;
	dsvn_t		*dsp;
	tk_set_t	tkacq;

	DSSTAT(I_dsvn_rmdir);
	bdp = OBJID_TO_BHV(objid);
	vp = BHV_TO_VNODE(bdp);

	/*
	 * Map from cdir_handle to a vnode.
	 */
	cfs_vnimport(cdir_handlep, &cdir_vp);
	if (cdir_vp == NULL) {
		*error = EMIGRATED;	/* raced with migration */
		return;
	}
	credp = CREDID_GETCRED(credid);

	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp)); /* prevent behavior insert/remove */

	PV_NEXT(bdp, nbdp, vop_rmdir);
	dsp = BHV_TO_DSVN(bdp);
	tkc_acquire(dsp->dsv_tclient, DVN_WRITE_TOKENS, &tkacq);
	ASSERT(tkacq == DVN_WRITE_TOKENS);
	PVOP_RMDIR(nbdp, name, cdir_vp, credp, *error);
	tkc_release (dsp->dsv_tclient, DVN_WRITE_TOKENS);

	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));

	VN_RELE(cdir_vp);		/* ref from cfs_vnimport */

	if (credp)
		crfree(credp);
	DVN_KTRACE4(DVN_TRACE_DSRPC, "I_dsvn_rmdir", dsp, "error", *error);
}

/*ARGSUSED*/
void
I_dsvn_readdir(
        objid_t 	objid,
	uio_t		*uiop,
	iovec_t		*iovp,
	size_t		iov_len,
	credid_t	credid,
	int		*eofp,
	off_t		*uio_offset,
	short		*uio_sigpipe,
	ssize_t		*uio_resid,
	int		*error)
{
	bhv_desc_t	*bdp, *nbdp;
	vnode_t		*vp;
	cred_t		*credp;
	dsvn_t		*dsp;

	DSSTAT(I_dsvn_readdir);
	uiop->uio_iov = iovp;
	if(uiop->uio_segflg == UIO_SYSSPACE)
		uiop->uio_segflg = UIO_USERSPACE;

	bdp = OBJID_TO_BHV(objid);
	vp = BHV_TO_VNODE(bdp);
	credp = CREDID_GETCRED(credid);

	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp)); /* prevent behavior insert/remove */

	PV_NEXT(bdp, nbdp, vop_readdir);
	dsp = BHV_TO_DSVN(bdp);
	PVOP_READDIR(nbdp, uiop, credp, eofp, *error);

	*uio_offset = uiop->uio_offset;
	*uio_sigpipe = uiop->uio_sigpipe;
	*uio_resid = uiop->uio_resid;
	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));

	if (credp)
		crfree(credp);

	if (!*error) {
		tkc_acquire1(dsp->dsv_tclient, DVN_TIMES_SHARED_WRITE_1);
		tkc_release1(dsp->dsv_tclient, DVN_TIMES_SHARED_WRITE_1);
	}

	DVN_KTRACE8(DVN_TRACE_DSRPC, "I_dsvn_readdir", dsp, "uio_offset", *uio_offset,
		"uio_resid", *uio_resid, "error", *error);
}

/* ARGSUSED */
void
I_dsvn_symlink(
        objid_t 	objid,
	char	       	*link_name,
	size_t          link_name_len,
	vattr_t		*vap,
	char		*target_path,
	size_t          target_path_len,
	credid_t	credid,
	int		*error)
{
	bhv_desc_t	*bdp, *nbdp;
	vnode_t		*vp;
	cred_t		*credp;
	dsvn_t		*dsp;
	tk_set_t	tkacq;

	DSSTAT(I_dsvn_symlink);

	bdp = OBJID_TO_BHV(objid);
	vp = BHV_TO_VNODE(bdp);
	credp = CREDID_GETCRED(credid);

	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp)); /* prevent behavior insert/remove */

	PV_NEXT(bdp, nbdp, vop_symlink);
	dsp = BHV_TO_DSVN(bdp);
	tkc_acquire(dsp->dsv_tclient, DVN_WRITE_TOKENS, &tkacq);
	ASSERT(tkacq == DVN_WRITE_TOKENS);
	PVOP_SYMLINK(nbdp, link_name, vap, target_path, credp, *error);
	tkc_release(dsp->dsv_tclient, DVN_WRITE_TOKENS);

	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));

	if (credp)
		crfree(credp);

	DVN_KTRACE4(DVN_TRACE_DSRPC, "I_dsvn_symlink", dsp, "error", *error);
}

/*ARGSUSED*/
void
I_dsvn_readlink(
        objid_t 	objid,
	uio_t		*uiop,
	iovec_t		*iovp,
	size_t		iov_len,
	credid_t	credid,
	off_t		*uio_offset,
	short		*uio_sigpipe,
	ssize_t		*uio_resid,
	int		*error)
{
	bhv_desc_t	*bdp, *nbdp;
	vnode_t		*vp;
	cred_t		*credp;
	dsvn_t		*dsp;

	DSSTAT(I_dsvn_readlink);
	uiop->uio_iov = iovp;
	if(uiop->uio_segflg == UIO_SYSSPACE)
		uiop->uio_segflg = UIO_USERSPACE;

	bdp = OBJID_TO_BHV(objid);
	vp = BHV_TO_VNODE(bdp);
	credp = CREDID_GETCRED(credid);

	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp)); /* prevent behavior insert/remove */

	dsp = BHV_TO_DSVN(bdp);
	PV_NEXT(bdp, nbdp, vop_readlink);
	PVOP_READLINK(nbdp, uiop, credp, *error);

	*uio_offset = uiop->uio_offset;
	*uio_sigpipe = uiop->uio_sigpipe;
	*uio_resid = uiop->uio_resid;
	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));

	if (credp)
		crfree(credp);

	if (!*error) {
		tkc_acquire1(dsp->dsv_tclient, DVN_TIMES_SHARED_WRITE_1);
		tkc_release1 (dsp->dsv_tclient, DVN_TIMES_SHARED_WRITE_1);
	}

	DVN_KTRACE8(DVN_TRACE_DSRPC, "I_dsvn_readlink", dsp,
		"uio_offset", *uio_offset, "uio_resid", *uio_resid,
		"error", *error);
}

void
I_dsvn_fsync(
        objid_t 	objid,
	int		flag,
	credid_t	credid,
	off_t		start,
	off_t		stop,
	int		*error)
{
	bhv_desc_t	*bdp, *nbdp;
	vnode_t		*vp;
	cred_t		*credp;

	DSSTAT(I_dsvn_fsync);

	bdp = OBJID_TO_BHV(objid);
	vp = BHV_TO_VNODE(bdp);
	credp = CREDID_GETCRED(credid);

	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp)); /* prevent behavior insert/remove */

	PV_NEXT(bdp, nbdp, vop_fsync);
	PVOP_FSYNC(nbdp, flag, credp, start, stop, *error);

	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));

	if (credp)
		crfree(credp);

	DVN_KTRACE10(DVN_TRACE_DSRPC, "I_dsvn_fsync", BHV_TO_DSVN(bdp),
		"flag", flag, "start", start, "stop", stop, "error", *error);
}

void
I_dsvn_fid(
        objid_t 	objid,
	fid_t		*fidp,
	int		*error)
{
	bhv_desc_t	*bdp, *nbdp;
	vnode_t		*vp;
	fid_t		*local_fidp;

	DSSTAT(I_dsvn_fid);

	bdp = OBJID_TO_BHV(objid);
	vp = BHV_TO_VNODE(bdp);

	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp)); /* prevent behavior insert/remove */

	PV_NEXT(bdp, nbdp, vop_fid);
	PVOP_FID(nbdp, &local_fidp, *error);

	/*
	 * Eventually, the RPC system will be doing the copy for us,
	 * but it doesn't yet...
	 */
	if (*error == 0) {
		*fidp = *local_fidp;
		freefid(local_fidp);
	}

	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));
	DVN_KTRACE6(DVN_TRACE_DSRPC, "I_dsvn_fid", BHV_TO_DSVN(bdp),
		"fidp", fidp, "error", *error);
}

void
I_dsvn_fid2(
        objid_t 	objid,
	fid_t		*fidp,
	int		*error)
{
	bhv_desc_t	*bdp, *nbdp;
	vnode_t		*vp;

	DSSTAT(I_dsvn_fid2);
	bdp = OBJID_TO_BHV(objid);
	vp = BHV_TO_VNODE(bdp);

	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp)); /* prevent behavior insert/remove */

	PV_NEXT(bdp, nbdp, vop_fid2);
	PVOP_FID2(nbdp, fidp, *error);

	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));
	DVN_KTRACE6(DVN_TRACE_DSRPC, "I_dsvn_fid2", BHV_TO_DSVN(bdp),
		"fidp", fidp, "error", *error);
}

void
I_dsvn_rwlock(
        objid_t 	objid,
	vrwlock_t	locktype)
{
	bhv_desc_t	*bdp, *nbdp;
	vnode_t		*vp;

	DSSTAT(I_dsvn_rwlock);
	bdp = OBJID_TO_BHV(objid);
	vp = BHV_TO_VNODE(bdp);

	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp)); /* prevent behavior insert/remove */

	PV_NEXT(bdp, nbdp, vop_rwlock);
	PVOP_RWLOCK(nbdp, locktype);

	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));
	DVN_KTRACE6(DVN_TRACE_DSRPC, "I_dsvn_rwlock", BHV_TO_DSVN(bdp),
		"locktype", locktype, "vp", vp);
}

void
I_dsvn_rwunlock(
        objid_t 	objid,
	vrwlock_t	locktype)
{
	bhv_desc_t	*bdp, *nbdp;
	vnode_t		*vp;

	DSSTAT(I_dsvn_rwunlock);
	bdp = OBJID_TO_BHV(objid);
	vp = BHV_TO_VNODE(bdp);

	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp)); /* prevent behavior insert/remove */

	PV_NEXT(bdp, nbdp, vop_rwunlock);
	PVOP_RWUNLOCK(nbdp, locktype);

	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));
	DVN_KTRACE6(DVN_TRACE_DSRPC, "I_dsvn_rwunlock", BHV_TO_DSVN(bdp),
		"locktype", locktype, "vp", vp);
}

void
I_dsvn_seek(
        objid_t 	objid,
	off_t		old_offset,
	off_t		*new_offsetp,
	int		*error)
{
	bhv_desc_t	*bdp, *nbdp;
	vnode_t		*vp;

	DSSTAT(I_dsvn_seek);
	bdp = OBJID_TO_BHV(objid);
	vp = BHV_TO_VNODE(bdp);

	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp)); /* prevent behavior insert/remove */

	PV_NEXT(bdp, nbdp, vop_seek);
	PVOP_SEEK(nbdp, old_offset, new_offsetp, *error);

	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));
	DVN_KTRACE10(DVN_TRACE_DSRPC, "I_dsvn_seek", BHV_TO_DSVN(bdp),
		"old_offset", old_offset, "new_offset", *new_offsetp,
		"vp", vp, "error", *error);
}

void
I_dsvn_cmp(
        objid_t 	objid,
	cfs_handle_t	*vp2_handlep,	/* handle for vp2 */
	int		*equal,
	int		*error)
{
	bhv_desc_t	*bdp, *nbdp;
	vnode_t		*vp, *vp2;

	DSSTAT(I_dsvn_cmp);
	bdp = OBJID_TO_BHV(objid);
	vp = BHV_TO_VNODE(bdp);

	/*
	 * Map from vp2_handle to a vnode.
	 */
	cfs_vnimport(vp2_handlep, &vp2);
	if (vp2 == NULL) {
		*error = EMIGRATED;	/* raced with migration */
		goto out;
	}
	ASSERT((vp->v_flag & VDOCMP) || (vp2->v_flag & VDOCMP));

	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp)); /* prevent behavior insert/remove */

	PV_NEXT(bdp, nbdp, vop_cmp);
	PVOP_CMP(nbdp, vp2, *equal);

	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));

	VN_RELE(vp2);			/* ref from cfs_vnimport */
	*error = 0;

out:
	DVN_KTRACE6(DVN_TRACE_DSRPC, "I_dsvn_cmp", BHV_TO_DSVN(bdp),
		"equal", *equal, "error", *error);
}

void
I_dsvn_frlock(
        objid_t 	objid,
	int		cmd,
	flock_t		*flockp,
	int		flag,
	off_t		offset,
	vrwlock_t       vrwlock,
	credid_t	credid,
	int		*error)
{
	bhv_desc_t	*bdp, *nbdp;
	vnode_t		*vp;
	cred_t		*credp;

	DSSTAT(I_dsvn_frlock);
	bdp = OBJID_TO_BHV(objid);
	vp = BHV_TO_VNODE(bdp);
	credp = CREDID_GETCRED(credid);

	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp)); /* prevent behavior insert/remove */

	PV_NEXT(bdp, nbdp, vop_frlock);
	PVOP_FRLOCK(nbdp, cmd, flockp, flag, offset, vrwlock, credp, *error);

	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));

	if (credp)
		crfree(credp);
	DVN_KTRACE6(DVN_TRACE_DSRPC, "I_dsvn_frlock", BHV_TO_DSVN(bdp),
		"cmd", cmd, "error", *error);
}

void
I_dsvn_bmap(
        objid_t 	objid,
	off_t		offset,
	ssize_t		count,
	int		flags,
	credid_t	credid,
	struct bmapval	*bmapp,
	int		*nbmaps,
	int		*error)
{
	bhv_desc_t	*bdp, *nbdp;
	vnode_t		*vp;
	cred_t		*credp;

	DSSTAT(I_dsvn_bmap);
	bdp = OBJID_TO_BHV(objid);
	vp = BHV_TO_VNODE(bdp);
	credp = CREDID_GETCRED(credid);

	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp)); /* prevent behavior insert/remove */

	PV_NEXT(bdp, nbdp, vop_bmap);
	PVOP_BMAP(nbdp, offset, count, flags, credp, bmapp, nbmaps, *error);

	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));

	if (credp)
		crfree(credp);
	DVN_KTRACE6(DVN_TRACE_DSRPC, "I_dsvn_bmap", BHV_TO_DSVN(bdp), "nbmaps", *nbmaps,
		"error", *error);
}

/* ARGSUSED */
void
I_dsvn_map(
        objid_t 	objid,
	off_t		offset,
	size_t		len,
	mprot_t		prot,
	uint		inflags,
	credid_t	credid,
	cell_t		client,
	cfs_handle_t	*handlep,
	tk_set_t	*obtset,
	enum vtype	*vtype,
	dev_t		*rdev,
	int		*flags,
	int		*error)
{
	bhv_desc_t	*bdp, *nbdp;
	vnode_t		*vp, *newvp;
	tk_set_t	refset, hadset;
	dsvn_t		*dsp;
	cred_t		*credp;
	DSSTAT(I_dsvn_map);

	bdp = OBJID_TO_BHV(objid);
	vp = BHV_TO_VNODE(bdp);
	credp = CREDID_GETCRED(credid);

	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp)); /* prevent behavior insert/remove */

	newvp = vp;
	PV_NEXT(bdp, nbdp, vop_map);
	PVOP_MAP(nbdp, offset, len, prot, inflags, credp, &newvp, *error);
	if (*error)
		goto out;


	if (credp)
		crfree(credp);
	/*
	 * Account for fact that the returned vnode may be different
	 * than the one passed in.  The logic in this case is very
	 * similar to the lookup case.
	 */
	if (newvp == vp) {
		CFS_HANDLE_MAKE_NULL(*handlep);	/* indicate no vnode change */
	} else {

		/*
		 * - Get a ds.
		 * - Get an existence token.
		 * - Set handle, type, rdev, flags out args.
		 */
		dsp = dsvn_vnode_to_dsvn(newvp);

		/*
		 * Try to get an existence token.
		 *
		 * If we can't because the client is racing with itself
		 * (obtaining or returning), then it will retry.  In that
		 * case the client uses the returned handle as a hint.  
		 */
		tks_obtain(dsp->dsv_tserver, client, DVN_EXIST_READ,
			   obtset, &refset, &hadset);

		DSVN_HANDLE_MAKE(*handlep, dsp);
		*vtype = newvp->v_type;
		*rdev = newvp->v_rdev;

		/*
		 * Get flags for client.  Must always return associated 
		 * flags when returning an existence token.
		 */
		DSVN_CLIENT_FLAGS(dsp, newvp, *flags);

		/*
		 * Account for fact that each existence token holds a
		 * vnode reference. 
		 */
		if (!(*obtset & DVN_EXIST_READ))
			VN_RELE(newvp);
	}
 out:
	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));
	DVN_KTRACE10(DVN_TRACE_DSRPC, "I_dsvn_map", dsp, "obtset", *obtset,
		"vp", vp, "newvp", newvp, "error", *error);
}

void
I_dsvn_addmap(
        objid_t 	objid,
	vaddmap_t	op,
	struct __vhandl_s *vt,
	pgno_t		*pgno,
	off_t		offset,
	size_t		len,
	mprot_t		prot,
	credid_t	credid,
	int		*error)
{
	bhv_desc_t	*bdp, *nbdp;
	vnode_t		*vp;
	cred_t		*credp;

	DSSTAT(I_dsvn_addmap);
	bdp = OBJID_TO_BHV(objid);
	vp = BHV_TO_VNODE(bdp);
	credp = CREDID_GETCRED(credid);

	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp)); /* prevent behavior insert/remove */

	PV_NEXT(bdp, nbdp, vop_addmap);
	PVOP_ADDMAP(nbdp, op, vt, pgno, offset, len, prot, credp, *error);

	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));

	if (credp)
		crfree(credp);
	DVN_KTRACE10(DVN_TRACE_DSRPC, "I_dsvn_addmap", BHV_TO_DSVN(bdp),
		"pgno", *pgno, "offset", offset, "len", len, "error", *error);
}

void
I_dsvn_delmap(
        objid_t 	objid,
	struct __vhandl_s	*vt,
	size_t		len,
	credid_t	credid,
	int		*error)
{
	bhv_desc_t	*bdp, *nbdp;
	vnode_t		*vp;
	cred_t		*credp;

	DSSTAT(I_dsvn_delmap);
	bdp = OBJID_TO_BHV(objid);
	vp = BHV_TO_VNODE(bdp);
	credp = CREDID_GETCRED(credid);

	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp)); /* prevent behavior insert/remove */

	PV_NEXT(bdp, nbdp, vop_delmap);
	PVOP_DELMAP(nbdp, vt, len, credp, *error);

	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));

	if (credp)
		crfree(credp);
	DVN_KTRACE6(DVN_TRACE_DSRPC, "I_dsvn_delmap", BHV_TO_DSVN(bdp),
		"len", len, "error", *error);
}

void
I_dsvn_allocstore(
        objid_t 	objid,
	off_t		offset,
	size_t		count,
	credid_t	credid,
	int		*error)
{
	bhv_desc_t	*bdp, *nbdp;
	vnode_t		*vp;
	dsvn_t		*dsp;
	cred_t		*credp;

	DSSTAT(I_dsvn_allocstore);
	bdp = OBJID_TO_BHV(objid);
	vp = BHV_TO_VNODE(bdp);
	credp = CREDID_GETCRED(credid);
	dsp = BHV_TO_DSVN(bdp);

	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp)); /* prevent behavior insert/remove */
	tkc_acquire1(dsp->dsv_tclient, DVN_EXTENT_WRITE_1);

	PV_NEXT(bdp, nbdp, vop_allocstore);
	PVOP_ALLOCSTORE(nbdp, offset, count, credp, *error);

	tkc_release1 (dsp->dsv_tclient, DVN_EXTENT_WRITE_1);
	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));

	if (credp)
		crfree(credp);
	DVN_KTRACE8(DVN_TRACE_DSRPC, "I_dsvn_allocstore", BHV_TO_DSVN(bdp),
		"offset", offset, "count", count, "error", *error);
}

void
I_dsvn_fcntl(
        objid_t 	objid,
	int		cmd,
	void		*arg,
	int		flags,
	off_t		offset,
	credid_t	credid,
	rval_t		*rvalp,
	int		*error)
{
	bhv_desc_t	*bdp, *nbdp;
	vnode_t		*vp;
	cred_t		*credp;
	dsvn_t		*dsp;
	tk_set_t	tkacq;

	DSSTAT(I_dsvn_fcntl);
	bdp = OBJID_TO_BHV(objid);
	vp = BHV_TO_VNODE(bdp);
	credp = CREDID_GETCRED(credid);

	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp)); /* prevent behavior insert/remove */

	PV_NEXT(bdp, nbdp, vop_fcntl);
	dsp = BHV_TO_DSVN(bdp);
	/*
	 * For now, assume the worst case that the fcntl is changing everything.
	 */
	tkc_acquire(dsp->dsv_tclient, DVN_WRITE_TOKENS, &tkacq);
	ASSERT(tkacq == DVN_WRITE_TOKENS);
	PVOP_FCNTL(nbdp, cmd, arg, flags, offset, credp, rvalp, *error);
	tkc_release (dsp->dsv_tclient, DVN_WRITE_TOKENS);

	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));

	if (credp)
		crfree(credp);

	DVN_KTRACE8(DVN_TRACE_DSRPC, "I_dsvn_fcntl", dsp,
		"cmd", cmd, "rval", rvalp->r_off, "error", *error);
}

/* ARGSUSED */
void
I_dsvn_attr_get(
        objid_t 	objid,
	char 		*name, 
        size_t          name_len,
	char 		**value, 
	size_t		*valuelenp, 
	int		inlen, 
	int 		flags,
	credid_t	credid,
	int		*error,
	void		**bdesc)
{
	bhv_desc_t	*bdp, *nbdp;
	vnode_t		*vp;
	int		templen;
	cred_t		*credp;

	DSSTAT(I_dsvn_attr_get);
	templen = inlen;
	bdp = OBJID_TO_BHV(objid);
	vp = BHV_TO_VNODE(bdp);
	credp = CREDID_GETCRED(credid);

	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp)); /* prevent behavior insert/remove */

	PV_NEXT(bdp, nbdp, vop_attr_get);
	ASSERT(*valuelenp == 0);
	*value = kmem_alloc(inlen, KM_SLEEP);
	PVOP_ATTR_GET(nbdp, name, *value, &templen, flags, credp, *error);
	*valuelenp = templen;

	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));

	if (credp)
		crfree(credp);
	DVN_KTRACE8(DVN_TRACE_DSRPC, "I_dsvn_attr_get", BHV_TO_DSVN(bdp),
		"value", *value, "valuelen", *valuelenp, "error", *error);
}

/* ARGSUSED */
void
I_dsvn_attr_get_done(
	char 		*value, 
	size_t		valuelen, 
	void		*bdesc)
{
	DSSTAT(I_dsvn_attr_get_done);
	DVN_KTRACE6(DVN_TRACE_DSRPC, "I_dsvn_attr_get_done", value,
		"valuelen", valuelen, "bdesc", bdesc);
	kmem_free(value, valuelen);
}

/* ARGSUSED */
void
I_dsvn_attr_set(
        objid_t 	objid,
	char 		*name, 
        size_t          name_len,
	char 		*value, 
	size_t 		valulen,
	int 		flags,
	credid_t	credid,
	int		*error)
{
	bhv_desc_t	*bdp, *nbdp;
	vnode_t		*vp;
	cred_t		*credp;

	DSSTAT(I_dsvn_attr_set);
	bdp = OBJID_TO_BHV(objid);
	vp = BHV_TO_VNODE(bdp);
	credp = CREDID_GETCRED(credid);

	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp)); /* prevent behavior insert/remove */

	PV_NEXT(bdp, nbdp, vop_attr_set);
	PVOP_ATTR_SET(nbdp, name, value, valulen, flags, credp, *error);

	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));

	if (credp)
		crfree(credp);
	DVN_KTRACE8(DVN_TRACE_DSRPC, "I_dsvn_attr_set", BHV_TO_DSVN(bdp),
		"value", value, "valulen", valulen, "error", *error);
}

/* ARGSUSED */
void
I_dsvn_attr_remove(
        objid_t 	objid,
	char 		*name, 
        size_t          name_len,
	int 		flags,
	credid_t	credid,
	int		*error)
{
	bhv_desc_t	*bdp, *nbdp;
	vnode_t		*vp;
	cred_t		*credp;

	DSSTAT(I_dsvn_attr_remove);
	bdp = OBJID_TO_BHV(objid);
	vp = BHV_TO_VNODE(bdp);
	credp = CREDID_GETCRED(credid);

	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp)); /* prevent behavior insert/remove */

	PV_NEXT(bdp, nbdp, vop_attr_remove);
	PVOP_ATTR_REMOVE(nbdp, name, flags, credp, *error);

	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));

	if (credp)
		crfree(credp);

	DVN_KTRACE8(DVN_TRACE_DSRPC, "I_dsvn_attr_remove", BHV_TO_DSVN(bdp),
		"name", name, "name_len", name_len, "error", *error);
}

/*
 * A client executing the mount syscall forwards the cover request to the
 * directory server. 
 */
/* ARGSUSED */
void
I_dsvn_cover(
	objid_t		objid,
	struct mounta	*uap,
	char		*attrs,
	credid_t	credid,
	cfs_handle_t	*rdir_hdlp,
	cfs_handle_t	*cdir_hdlp,
	int		*error)
{
        bhv_desc_t      *bdp = OBJID_TO_BHV(objid);
	bhv_desc_t	*nbdp;
	vnode_t         *rdir_save;
	vnode_t         *cdir_save;
	cred_t		*cred;
	DSSTAT(I_dsvn_cover);

	cred = CREDID_GETCRED(credid);
	/* 
	 *  XXX R2 handle vnode name token when we cache vnode on client cell
	 */

	/* 
	 * set the cdir and rdir of the running process to that of the process
	 * that made the system call.
	 * XXX R2
	 */
	rdir_save = curuthread->ut_rdir;
	if (rdir_hdlp) {
		cfs_vnimport(rdir_hdlp, &curuthread->ut_rdir);
	}
	else
		curuthread->ut_rdir = NULL;
	cdir_save = curuthread->ut_cdir;
	cfs_vnimport(cdir_hdlp, &curuthread->ut_cdir);

	/* 
	 * We don't hold the bhv chain lock for VOP_COVER because we don't
	 * want to hold off inserting a new interposer on the covered vnode
	 * (because that can happen from within the VOP_COVER path, in the
	 * distributed case at least), and it's the case that inserting an
	 * interposer on the covered vnode does not require synchronization 
	 * with a VOP_COVER.  XXX But what about relocation of the covered 
	 * vnode?
	 */
	PV_NEXT(bdp, nbdp, vop_cover);
        VN_HOLD(BHV_TO_VNODE(bdp));
	PVOP_COVER(nbdp, uap, attrs, cred, *error);

	if (rdir_hdlp)
		VN_RELE(curuthread->ut_rdir);
	curuthread->ut_rdir = rdir_save;
	VN_RELE(curuthread->ut_cdir);
	curuthread->ut_cdir = cdir_save;

	if (cred)
		crfree(cred);

	DVN_KTRACE4(DVN_TRACE_DSRPC, "I_dsvn_cover", BHV_TO_DSVN(bdp),
		"error", *error);
}

void
I_dsvn_vnode_change(
        objid_t 	objid,
	vchange_t	cmd,
	__psint_t	val)
{
	DSSTAT(I_dsvn_vnode_change);
	dsvn_vnode_change(OBJID_TO_BHV(objid), cmd, val);
	DVN_KTRACE6(DVN_TRACE_DSRPC, "I_dsvn_vnode_change",
		BHV_TO_DSVN(OBJID_TO_BHV(objid)), "cmd", cmd, "val", val);
}

/*
 * Token interfaces.
 */

/*
 * Return tokens.
 */
void
I_dsvn_return(
        objid_t 	objid,
	cell_t		client,
        tk_set_t	retset,
        tk_set_t	unknownset,
        tk_disp_t	why,
	ds_times_t	*timesp)
{
	bhv_desc_t	*bdp;
	vnode_t		*vp;
	dsvn_t		*dsp;

	DSSTAT(I_dsvn_return);
	bdp = OBJID_TO_BHV(objid);
	vp = BHV_TO_VNODE(bdp);
	dsp = BHV_TO_DSVN(bdp);

	if (dsp->dsv_dsxvn)
		cxfs_dsxvn_return(dsp->dsv_dsxvn, client, retset,
			unknownset, why);

	tks_return(dsp->dsv_tserver, client, retset, TK_NULLSET,
		unknownset, why);

	/* 
	 * Shared write token should return times.
	 * If the times are greater than the ones in the vnode, set to new times.
	 */

	if (TK_IS_IN_SET(retset, DVN_TIMES_SHARED_WRITE))
		ds_update_times(dsp, bdp, vp, timesp);

	/* 
	 * Existence tokens hold vnode references.
	 */
	if (TK_IS_IN_SET(retset, DVN_EXIST_READ))
		VN_RELE(vp);

	DVN_KTRACE10(DVN_TRACE_TOKEN, "I_dsvn_return", dsp,
		"retset", retset, "unknownset", unknownset,
			"why", why, "client", client);
}

/*
 * Refuse to return tokens.  Response to a recall.
 */
void
I_dsvn_refuse(
        objid_t 	objid,
	cell_t		client,
        tk_set_t	refset,
        tk_disp_t	why)
{
	bhv_desc_t	*bdp = OBJID_TO_BHV(objid);
	DSSTAT(I_dsvn_refuse);

	tks_return(BHV_TO_DSVN(bdp)->dsv_tserver, client, TK_NULLSET, 
		   refset, TK_NULLSET, why);
	DVN_KTRACE8(DVN_TRACE_TOKEN, "I_dsvn_refuse", BHV_TO_DSVN(bdp),
		"refset", refset, "why", why, "client", client);
}

/*
 * Can't return tokens because vnode not found on the client side.  
 * Response to a recall.
 */
void
I_dsvn_notfound(
        cfs_handle_t 	*handlep,
	cell_t		client,
        tk_set_t	notfound_set,
        tk_disp_t	why)
{
	dsvn_t		*dsp;

	DSSTAT(I_dsvn_notfound);
	/*
	 * Note that there's no guarantee the server-side vnode still
	 * exists.  This is because our recall might have crossed in the
	 * mail with a return which caused the vnode to be be reclaimed.  
	 * This recall/return race would also explain why the vnode was
	 * not found on the client.
	 *
	 * If the server-side vnode is found then we call tks_return
	 * with an indication that the recalled tokens were "unknown"
	 * on the client.  This causes the token server module to 
	 * retry the recall until the tokens are returned (or refused).
	 * This retrying is necessary because it's possible the client-side
	 * vnode was not found because the obtain of the existence token
	 * was overtaken by the recall message.
	 */
	dsp = dsvn_handle_lookup(handlep);
	if (dsp != NULL) {
		tks_return(dsp->dsv_tserver, client, TK_NULLSET, 
			   TK_NULLSET, notfound_set, why);

		VN_RELE(DSVN_TO_VNODE(dsp));
	}
	DVN_KTRACE8(DVN_TRACE_TOKEN, "I_dsvn_return", dsp,
		"notfound_set", notfound_set, "why", why, "client", client);
}


/*
 * Obtain tokens for a client.
 */
void
I_dsvn_obtain(
        objid_t 	objid,
	cell_t		client,
        tk_set_t	retset,
	tk_disp_t	why,
        tk_set_t	obtset,
        tk_set_t	*grantset,
        tk_set_t	*already_obtset,
	tk_set_t	*refset,
	vattr_t		*vattr,
	int		*cxfs_flags,
	char 		**cxfs_buff,
	size_t		*cxfs_count,
	int		*errorp,
	void		**bufdesc)
{
	bhv_desc_t	*bdp;
	dsvn_t		*dsp;

	DSSTAT(I_dsvn_obtain);
	ASSERT(!(obtset & DVN_EXIST_READ));

	*errorp = 0;
	*cxfs_count = 0;
	*cxfs_flags = 0;
	*bufdesc = NULL;

	bdp = OBJID_TO_BHV(objid);
	dsp = BHV_TO_DSVN(bdp);

	if (retset != TK_NULLSET) {
		if (dsp->dsv_dsxvn)
			cxfs_dsxvn_return(dsp->dsv_dsxvn, client,
				retset, 0, why);

		tks_return(dsp->dsv_tserver, client, retset,
			TK_NULLSET, TK_NULLSET, why);
	}

	if (obtset != TK_NULLSET) {
		tks_obtain(dsp->dsv_tserver, client, obtset, 
			   grantset, refset, already_obtset);

		*errorp = ds_getobjects(dsp, vattr, cxfs_buff, cxfs_count, cxfs_flags,
			*grantset, *refset, *already_obtset, bufdesc, bdp);

		/*
		 * If we had an error getting any objects, return
		 * any recently obtained tokens.
		 */

		if (*errorp && (*grantset != TK_NULLSET)) {
			printf("I_dsvn_obtain error %d returning %x\n",
				*errorp, *grantset);
			tks_return(dsp->dsv_tserver, client, *grantset,
				TK_NULLSET, TK_NULLSET, TK_DISP_CLIENT_ALL);
			*grantset = TK_NULLSET;
		}
	}

	if (*errorp)
		printf("I_dsvn_obtain: err %d\n", *errorp);

	DVN_KTRACE10(DVN_TRACE_TOKEN, "I_dsvn_obtain", dsp,
			"obtset", obtset, "grant", *grantset,
			"had", *already_obtset, "error", *errorp);
}

/* ARGSUSED */
void
I_dsvn_obtain_done(
	char 		*cxfs_buff,
	size_t		cxfs_count,
	void		*bufdesc)
{
	DSSTAT(I_dsvn_obtain_done);
	DVN_KTRACE2(DVN_TRACE_TOKEN, "I_dsvn_obtain_done", bufdesc);
	if (bufdesc)
		cxfs_dsxvn_obtain_done(cxfs_buff, cxfs_count, bufdesc);
}

/*
 * Given a handle, obtain an existence token for it.
 * Also, return the corresponding vfs handle as an optimization.
 *
 * The caller does NOT guarantee that the vnode/dsvn exists.
 *
 * If an existence is not returned and the DVN_NOTFOUND flag is set, 
 * then the client is racing with the object being torn down or migrating.  
 * If the flag isn't set but an existence token still isn't returned, then 
 * the client is racing with itself.
 */
void
I_dsvn_obtain_exist(
        cfs_handle_t	*handlep,
	cell_t		client,
	tk_set_t	*obtset,	/* out */
        enum vtype	*vtype,		/* out */
	dev_t		*rdev,		/* out */
	int		*flags,		/* out */
	vattr_t		*vattr,		/* out */
	int		*cxfs_flags,	/* out */
	char 		**cxfs_buff,	/* out */
	size_t		*cxfs_count,	/* out */
        cfs_handle_t	*vfs_handlep,	/* out */
	int		*errorp,	/* out */
	void		**bufdesc)
{
	dsvn_t		*dsp;
	vnode_t		*newvp;
	tk_set_t	refset, hadset;

	DSSTAT(I_dsvn_obtain_exist);
	*errorp = 0;

	/*
	 * First find the vnode/dsvn that the handle refers to.
	 * Returns ref'd vp or null.
	 */
	*cxfs_count = 0;
	*cxfs_flags = 0;
	*bufdesc = NULL;

	dsp = dsvn_handle_lookup(handlep);
	if (dsp != NULL) {
		newvp = DSVN_TO_VNODE(dsp);

		/*
		 * Try to get a set of tokens including the existence token.
		 *
		 * If we can't get the existence token
		 * because the client is racing with itself
		 * (obtaining or returning), then it will retry. 
		 */
		tks_obtain_conditional(dsp->dsv_tserver, client, DVN_EXIST_READ,
			DVN_OPTIMISTIC_TOKENS, obtset, &refset, &hadset);

		*vtype = newvp->v_type;
		*rdev = newvp->v_rdev;

		/*
		 * Get flags for client.  Must always return associated 
		 * flags when returning an existence token.
		 */
		DSVN_CLIENT_FLAGS(dsp, newvp, *flags);
		
		/*
		 * Get the vfs handle.
		 */
		cfs_vfsexport(newvp->v_vfsp, vfs_handlep);

		/*
		 * Account for fact that each existence token holds a
		 * vnode reference.
		 */
		if (!(TK_IS_IN_SET(*obtset, DVN_EXIST_READ))) {
			ASSERT(*obtset == TK_NULLSET);
			VN_RELE(newvp);
		} else {
			*errorp = ds_getobjects(dsp, vattr, cxfs_buff,
				cxfs_count, cxfs_flags, *obtset, refset,
				hadset, bufdesc, DSVN_TO_BHV(dsp));

			/*
			 * If we had an error getting any objects,
			 * return any recently obtained tokens.
			 */

			if (*errorp && (*obtset != TK_NULLSET)) {
				tks_return(dsp->dsv_tserver, client, *obtset,
					TK_NULLSET, TK_NULLSET,
					TK_DISP_CLIENT_ALL);
				*obtset = TK_NULLSET;
			}
		}
	} else {
		/*
		 * Object must have been torn down or migrated.
		 */
		*obtset = TK_NULLSET;
		*flags = DVN_NOTFOUND;
	}
	DVN_KTRACE8(DVN_TRACE_TOKEN, "I_dsvn_obtain_exist", dsp,
		"obtset", *obtset, "flags", *flags, "error", *errorp);
}


/* ARGSUSED */
void
I_dsvn_obtain_exist_done(
	char 		*cxfs_buff,
	size_t		cxfs_count,
	void		*bufdesc)
{
	DSSTAT(I_dsvn_obtain_exist_done);
	DVN_KTRACE2(DVN_TRACE_TOKEN, "I_dsvn_obtain_exist_done", bufdesc);
	if (bufdesc)
		cxfs_dsxvn_obtain_done(cxfs_buff, cxfs_count, bufdesc);
}
/*
 * Specials for vop_backdoor functions
 */
/* ARGSUSED */
void
I_dsvn_vnode_unref(
	objid_t		vnaddr,
        vnumber_t       vngen)
{
        vnode_t         *vp = (vnode_t *) vnaddr;

	DSSTAT(I_dsvn_vnode_unref);
	DVN_KTRACE2(DVN_TRACE_DSRPC, "I_dsvn_vnode_unref", vnaddr);
	ASSERT(vp->v_number == vngen);
	VN_RELE(vp);
}

/* ARGSUSED */
void
I_dsvn_vnode_reexport(
	objid_t		vnaddr,
        vnumber_t       vngen,
        cfs_handle_t    *handlep)
{
        vnode_t         *vp = (vnode_t *) vnaddr;
	DSSTAT(I_dsvn_vnode_reexport);


	ASSERT(vp->v_number == vngen);
        ASSERT(vp->v_count > 0);

        cfs_vnexport(vp, handlep);
	DVN_KTRACE4(DVN_TRACE_DSRPC, "I_dsvn_vnode_reexport", vnaddr,
		"vngen", vngen);
}



/*
 * Copy page flags from the client back to the server for pages in 
 * the page cache.
 */
/* ARGSUSED */
void
I_dsvn_update_page_flags(
        objid_t 	objid,
	pfn_t		*pfnlist,
	size_t		count)
{
	pfd_t		*pfd;
	int		i;

	DSSTAT(I_dsvn_update_page_flags);
	for (i=0; i<count; i++) {
		pfd = pfntopfdat(pfnlist[i]);
		ASSERT(pfd->pf_vp == BHV_TO_VNODE(OBJID_TO_BHV(objid)));
	
		pfd_setflags(pfd, P_DIRTY);
		pdinsert(pfd);
	}
	DVN_KTRACE6(DVN_TRACE_DSRPC, "I_dsvn_update_page_flags",
		BHV_TO_VNODE(OBJID_TO_BHV(objid)), "pfnlist", pfnlist,
		"count", count);
}

/*
 * Convert an external list into a uio/kvaddr.  Caller must do kvfree.
 * WARNING:  No VCE_AVOIDANCE stuff here - may be needed!
 */
void
plist_to_uio (
	external_alenlist_t exlist,
	uio_t	*uiop,
	struct iovec *iovp)
{
	pde_t	*pde;
	pgi_t	pdebits;
	caddr_t	kvaddr;
	long	plen, soff;
	alenaddr_t paddr;
	int	i, npgs;

	DSSTAT(plist_to_uio);
	DVN_KTRACE6(DVN_TRACE_MISC, "plist_to_uio", exlist, "uiop", uiop,
		"iovp", iovp);
	for (i=0; i<uiop->uio_iovcnt; i++) {
		npgs = max(1, numpages(uiop->uio_iov[i].iov_base, 
			uiop->uio_iov[i].iov_len));
		if (uiop->uio_iov[i].iov_len == 0) {
			ASSERT(npgs == 1);
			uiop->uio_iov[i].iov_base = 0;
			*iovp = uiop->uio_iov[i];
			iovp++;
			exlist++;
			continue;
		}
		kvaddr = kvalloc(npgs, 0, 0);
		ASSERT(poff(kvaddr) == 0);
		pde = kvtokptbl(kvaddr);

		/*
		 * adjust for starting offset
	 	*/
		soff = poff(exlist->addr);
		if (soff) {
			kvaddr += soff;
			exlist->addr = pbase(exlist->addr);
			exlist->len += soff;
		}

		pdebits =
			PG_VR|PG_G|PG_M|PG_NDREF|PG_SV|pte_cachebits();

		while (npgs) {
			plen = exlist->len;
			paddr = exlist->addr;
			while (plen > 0) {
				ASSERT(npgs);
				pg_setpgi(pde, mkpde(pdebits,pnum(paddr)));
				pde++;
				npgs--;
				paddr += NBPP;
				plen -= NBPP;
			}
			exlist++;
		}
		ASSERT(npgs == 0);
		uiop->uio_iov[i].iov_base = kvaddr;
		*iovp = uiop->uio_iov[i];
		iovp++;
	}

}

/*
 * Free kvaddr created by plist_to_kvaddr based on original
 * iovec.
 */
void
kvaddr_free(
	int	uio_iovcnt,
	iovec_t	*iovp)
{
	int	i, npgs;

	DSSTAT(kvaddr_free);
	for (i=0; i<uio_iovcnt; i++) {
		if (iovp->iov_len) {
			npgs = numpages(iovp->iov_base, 
				iovp->iov_len);
			kvfree(iovp->iov_base, npgs);
		}
		iovp++;
	}
}

void
I_dsvn_poll(
        objid_t		objid,
	short 		events,
	int 		anyyet,
	short 		*reventsp,
	struct pollhead **phpp,
	unsigned int 	*genp,
	int		*rval)
{
	bhv_desc_t	*bdp, *nbdp;
	vnode_t		*vp;

	DSSTAT(I_dsvn_poll);
	bdp = OBJID_TO_BHV(objid);
	vp = BHV_TO_VNODE(bdp);

retry:
	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp)); /* prevent behavior insert/remove */

	PV_NEXT(bdp, nbdp, vop_poll);
	PVOP_POLL(nbdp, events, anyyet, reventsp, phpp, genp, *rval);

	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));

	/*
	 * In the distributed case, add this event to the distributed
	 * poll list on the server.  Return a NULL php to the client.
	 */
	if (!anyyet && (*reventsp == 0) && *phpp) {
		if (distributed_polladd(*phpp, events, *genp) != 0)
			goto retry;
	}
	*phpp = NULL;
	DVN_KTRACE10(DVN_TRACE_DSRPC, "I_dsvn_poll", events, "anyyet", anyyet,
		"revents", *reventsp, "gen", *genp, "rval", *rval);
}

int
ds_getobjects(
	dsvn_t		*dsp,
	vattr_t		*vattr,
	char		**cxfs_buff,
	size_t		*cxfs_count,
	int		*cxfs_flags,
	tk_set_t	obtset,
	tk_set_t	refset,
	tk_set_t	hadset,
	void		**bufdesc,
	bhv_desc_t	*bdp)
{
	int 		error;
	bhv_desc_t	*nbdp;
	int		mask = 0;
	tk_set_t	return_set;
	int		gen_attrs, ext_attrs, times_attrs;

	DSSTAT(ds_getobjects);

	return_set = TK_ADD_SET(obtset, hadset);

	if (return_set == TK_NULLSET)
		goto out;

	gen_attrs = TK_GET_CLASS(return_set, DVN_ATTR_NUM);
	ext_attrs = TK_GET_CLASS(return_set, DVN_EXTENT_NUM);
	times_attrs = TK_GET_CLASS(return_set, DVN_TIMES_NUM);

	error = 0;

	if (dsp->dsv_dsxvn) {
		error = cxfs_dsxvn_obtain(dsp->dsv_dsxvn,
			cxfs_buff, cxfs_count, cxfs_flags, obtset,
			refset, hadset, bufdesc);
	}


	if (!error) {
		if (gen_attrs)
			mask |= AT_ATTR_PROTECTED;
		if (times_attrs)
			mask |= AT_TIMES;
		if (ext_attrs)
			mask |= AT_EXTENT_PROTECTED;

		if (mask) {
			vattr->va_mask = mask;
			PV_NEXT(bdp, nbdp, vop_getattr);
			PVOP_GETATTR(nbdp, vattr, 0, sys_cred, error);
		}
	}

out:

	DVN_KTRACE10(DVN_TRACE_TOKEN, "ds_getobjects", dsp, "obtset",
		obtset, "hadset", hadset, "mask", mask, "error", error);

	return(error);
}

void
ds_update_times(
	dsvn_t 		*dsp,
	bhv_desc_t	*bdp,
	vnode_t		*vp,
	ds_times_t      *timesp)
{
	vattr_t		vattr, *vap = NULL;
	bhv_desc_t	*nbdp;
	int		error = 0;

	DSSTAT(ds_update_times);

	if ((timesp->ds_flags & (AT_ATIME|AT_CTIME|AT_MTIME)) == 0)
		goto out;

	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp)); /* prevent behavior insert/remove */

	vap = &vattr;
	vap->va_mask = AT_TIMES;

	/*
	 * Do all filesystems adhere to no changes in times
	 * if VRWLOCK_WRITE is held?
	 * We not only need to prevent two threads from doing the
	 * following at the same time but we need to make sure there is no
	 * cell doing a setattr/write/... that modifies the times at the same
	 * time we are doing the following. Or We could loose a time update.
	 * JIMJIMJIM
	 * All setattr/writes/... grab the DVN_TIMES_SHARED_WRITE token if
	 * they will possibly modify the times. So, we need to make sure
	 * that our token (the server's) is revoked before any cell can
	 * send us the return RPC that causes the following.
	 */

	PV_NEXT(bdp, nbdp, vop_rwlock);
	PVOP_RWLOCK(nbdp, VRWLOCK_WRITE);

	PV_NEXT(bdp, nbdp, vop_getattr);
	PVOP_GETATTR(nbdp, vap, 0, sys_cred, error);

	/*
	 * getattr can fail. If we can't get attributes,
	 *  forget trying to update the times.
	 *
	 * The file system could have gone belly up.
	 * With /proc, the process could have exitted on us.
	 */

	if(error)
		goto unlock_out;

	/* Now update any times in the upward direction. */

	vap->va_mask = 0;

	if ((timesp->ds_flags & AT_ATIME) &&
		(ds_timespeccmp(&timesp->ds_atime, &vap->va_atime) > 0)) {
		vap->va_mask |= AT_ATIME;
		vap->va_atime.tv_sec = timesp->ds_atime.tv_sec;
		vap->va_atime.tv_nsec = timesp->ds_atime.tv_nsec;
	}

	if ((timesp->ds_flags & AT_MTIME) &&
		(ds_timespeccmp(&timesp->ds_mtime, &vap->va_mtime) > 0)) {
		vap->va_mask |= AT_MTIME;
		vap->va_mtime.tv_sec = timesp->ds_mtime.tv_sec;
		vap->va_mtime.tv_nsec = timesp->ds_mtime.tv_nsec;
	}

	if ((timesp->ds_flags & AT_CTIME) &&
		(ds_timespeccmp(&timesp->ds_ctime, &vap->va_ctime) > 0)) {
		vap->va_mask |= AT_CTIME;
		vap->va_ctime.tv_sec = timesp->ds_ctime.tv_sec;
		vap->va_ctime.tv_nsec = timesp->ds_ctime.tv_nsec;
	}

	if (vap->va_mask) {
		PV_NEXT(bdp, nbdp, vop_setattr);
		PVOP_SETATTR(nbdp, vap, 0, sys_cred, error);

		/* See comment above getattr above */
		if (error)
			goto unlock_out;
	}

unlock_out:

	PV_NEXT(bdp, nbdp, vop_rwunlock);
	PVOP_RWUNLOCK(nbdp, VRWLOCK_WRITE);

	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));

out:

	DVN_KTRACE8(DVN_TRACE_DSRPC, "ds_update_times", dsp, "ds_flags",
		timesp->ds_flags, vap ? "mask" : "NOTIMES",
			vap ? vap->va_mask : 0, "error", error);
}

int
ds_timespeccmp(timespec_t *t1, timespec_t *t2)
{
        time_t  s;
        long    n;

        if (s = t1->tv_sec - t2->tv_sec)
                return(s > 0 ? 1 : -1);	/* Avoid type conflicts with return val */
        if (n = t1->tv_nsec - t2->tv_nsec)
                return(n > 0 ? 1 : -1);	/* Avoid type conflicts with return val */
        return(0);
}

