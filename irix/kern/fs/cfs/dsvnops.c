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
#ident	"$Id: dsvnops.c,v 1.40 1997/09/29 03:06:57 steiner Exp $"

/*
 * Server-side vnode ops for the Cell File System.
 */
#include "dvn.h"
#include "invk_dcvn_stubs.h"
#include <sys/pvnode.h>
#include <sys/fs_subr.h>
#include <sys/pfdat.h>
#include <sys/pathname.h>

static void 	dsvn_tsif_recall(void *, tks_ch_t, tk_set_t, tk_disp_t);
static void 	dsvn_tsif_recalled(void *, tk_set_t, tk_set_t);
static tks_iter_t vnode_change_iterator(void *, tks_ch_t, tk_set_t, va_list);
static tks_iter_t dsvn_page_cache_operation_iterator(void*, tks_ch_t, tk_set_t, va_list);

tks_ifstate_t dsvn_tserver_iface = {
	        dsvn_tsif_recall,
		dsvn_tsif_recalled,
		NULL,			/* tsif_idle */
};

/*
 * Internal routine to teardown an installed dsvn.
 */
void
dsvn_teardown(
        dsvn_t		*dsp,
	bhv_desc_t	*bdp)
{
	/*
	 * - remove from hash table
	 * - remove from behavior chain
	 * - free data structures
	 */
	DSSTAT(dsvn_teardown);
	DVN_KTRACE4(DVN_TRACE_RECYCLE, "dsvn_teardown", dsp, "bdp", bdp);
	dsvn_handle_remove(dsp);
	vn_bhv_remove(VN_BHV_HEAD(BHV_TO_VNODE(bdp)), bdp);
	dsvn_destroy(dsp);
}

/*
 * Read vnode op.
 */
int
dsvn_read(
	bhv_desc_t	*bdp,
	uio_t		*uiop,
	int		ioflag,
	cred_t		*credp,
	flid_t		*fl)
{
	dsvn_t		*dsp = BHV_TO_DSVN(bdp);
	int		error;
	bhv_desc_t	*nbdp;

	DSSTAT(dsvn_read);
	PV_NEXT(bdp, nbdp, vop_read);

	PVOP_READ(nbdp, uiop, ioflag, credp, fl, error);
	/* Cause the read attribute tokens to be revoked */
	if (!error) {
		tkc_acquire1(dsp->dsv_tclient, DVN_TIMES_SHARED_WRITE_1);
		tkc_release1 (dsp->dsv_tclient, DVN_TIMES_SHARED_WRITE_1);
	}
	DVN_KTRACE4(DVN_TRACE_DSVOP, "dsvn_read", dsp, "error", error);
	return error;
}

/*
 * Write vnode op.
 */
int
dsvn_write(
	bhv_desc_t 	*bdp,
	uio_t		*uiop,
	int		ioflag,
	cred_t		*credp,
	flid_t		*fl)
{
	dsvn_t		*dsp = BHV_TO_DSVN(bdp);
	int		error;
	bhv_desc_t	*nbdp;
	tk_set_t	tkacq;

	PV_NEXT(bdp, nbdp, vop_write);

	DSSTAT(dsvn_write);
	tkc_acquire(dsp->dsv_tclient, DVN_WRITE_TOKENS, &tkacq);
	ASSERT(tkacq == DVN_WRITE_TOKENS);
	PVOP_WRITE(nbdp, uiop, ioflag, credp, fl, error);
	tkc_release (dsp->dsv_tclient, DVN_WRITE_TOKENS);
	DVN_KTRACE4(DVN_TRACE_DSVOP, "dsvn_write", dsp, "error", error);
	return error;
}

/*
 * Getattr vnode op.
 */
int
dsvn_getattr(
	bhv_desc_t 	*bdp,
	vattr_t		*vap,
	int		flags,
	cred_t		*credp)
{
	dsvn_t		*dsp = BHV_TO_DSVN(bdp);
	int		error;
	bhv_desc_t	*nbdp;
	int		mask;
	tk_set_t	tokens_needed = TK_NULLSET, tkacq;

	/*
	 * First, figure out what tokens we need depending on what
	 * is being read.
	 */
	DSSTAT(dsvn_getattr);

	mask = vap->va_mask;

	/*
	 * We don't need the DVN_ATTR_READ or DVN_EXTENT_READ
	 * tokens since no client can change the field associated
	 * with these tokens. Clients only change the times holding
	 * the DVN_TIMES_SHARED_WRITE.
	 */

	if ((mask & AT_TIMES) != 0)
		tokens_needed = TK_ADD_SET(tokens_needed, DVN_TIMES_READ);

	PV_NEXT(bdp, nbdp, vop_getattr);

	if (tokens_needed) {
		tkc_acquire(dsp->dsv_tclient, tokens_needed, &tkacq);
		ASSERT(tkacq == tokens_needed);
	}
	PVOP_GETATTR(nbdp, vap, flags, credp, error);
	if (tokens_needed)
		tkc_release (dsp->dsv_tclient, tokens_needed);
	DVN_KTRACE6(DVN_TRACE_DSVOP, "dsvn_getattr", dsp,
		"tokens_needed", tokens_needed, "error", error);
	return error;
}

/*
 * Setattr vnode op.
 */
int
dsvn_setattr(
	bhv_desc_t 	*bdp,
	vattr_t		*vap,
	int		flags,
	cred_t		*credp)
{
	dsvn_t		*dsp = BHV_TO_DSVN(bdp);
	int		error;
	bhv_desc_t	*nbdp;
	int		mask;
	tk_set_t	tokens_needed = TK_NULLSET, tkacq;

	/*
	 * First, figure out what tokens we need depending on what
	 * is being set.
	 */
	DSSTAT(dsvn_setattr);

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

	PV_NEXT(bdp, nbdp, vop_setattr);

	if (tokens_needed) {
		tkc_acquire(dsp->dsv_tclient, tokens_needed, &tkacq);
		ASSERT(tkacq == tokens_needed);
	}
	PVOP_SETATTR(nbdp, vap, flags, credp, error);
	if (tokens_needed)
		tkc_release (dsp->dsv_tclient, tokens_needed);
	DVN_KTRACE6(DVN_TRACE_DSVOP, "dsvn_setattr", dsp,
		"tokens_needed", tokens_needed, "error", error);
	return error;
}

/*
 * Create vnode op.
 */
int
dsvn_create(
	bhv_desc_t	*dirbdp,
	char		*name,
	vattr_t		*vap,
	int		flags,
	int		I_mode,
	vnode_t		**vpp,
	cred_t		*credp)
{
	dsvn_t		*dsp = BHV_TO_DSVN(dirbdp);
	int		error;
	bhv_desc_t	*nbdp;
	int 		truncate = 0;
	tk_set_t	tkacq;

	DSSTAT(dsvn_create);

	tkc_acquire(dsp->dsv_tclient, DVN_WRITE_TOKENS, &tkacq);
	ASSERT(tkacq == DVN_WRITE_TOKENS);

	PV_NEXT(dirbdp, nbdp, vop_create);
	PVOP_CREATE(nbdp, name, vap, flags, I_mode, vpp, credp, error);

	tkc_release (dsp->dsv_tclient, DVN_WRITE_TOKENS);
	DVN_KTRACE6(DVN_TRACE_DSVOP, "dsvn_setattr", dsp, "error", error,
		"trunc", truncate);
	return error;
}

/*
 * Remove vnode op.
 */
int
dsvn_remove(
	bhv_desc_t 	*dirbdp,
	char		*name,
	cred_t		*credp)
{
	dsvn_t		*dsp = BHV_TO_DSVN(dirbdp);
	int		error;
	bhv_desc_t	*nbdp;
	tk_set_t	tkacq;
	DSSTAT(dsvn_remove);

	PV_NEXT(dirbdp, nbdp, vop_remove);
	/*
	 * the read attribute token of the file being removed
	 * is revoked in VOP_LINK_REMOVED.
	 */
	tkc_acquire(dsp->dsv_tclient, DVN_WRITE_TOKENS, &tkacq);
	ASSERT(tkacq == DVN_WRITE_TOKENS);

	PVOP_REMOVE(nbdp, name, credp, error);
	tkc_release (dsp->dsv_tclient, DVN_WRITE_TOKENS);
	DVN_KTRACE4(DVN_TRACE_DSVOP, "dsvn_remove", dsp, "error", error);
	return error;
}

/*
 * Link vnode op.
 */
int
dsvn_link(
	bhv_desc_t 	*tdirbdp,
	vnode_t		*svp,
	char		*tname,
	cred_t		*credp)
{
	dsvn_t		*dsp = BHV_TO_DSVN(tdirbdp), *file_dsp;
	int		error;
	bhv_desc_t	*nbdp, *file_bdp;
	tk_set_t	tkacq;

	DSSTAT(dsvn_link);
	PV_NEXT(tdirbdp, nbdp, vop_link);

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

	PVOP_LINK(nbdp, svp, tname, credp, error);

	if (file_dsp && (file_dsp != dsp)) {
		tkc_release(file_dsp->dsv_tclient, DVN_ATTR_MOD_TOKENS);
	}
	tkc_release (dsp->dsv_tclient, DVN_WRITE_TOKENS);
	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(svp));
	DVN_KTRACE6(DVN_TRACE_DSVOP, "dsvn_link", dsp, "file_dsp",
		file_dsp, "error", error);
	return error;
}

/*
 * Rename vnode op.
 */
int
dsvn_rename(
	bhv_desc_t	*sdirbdp,
	char		*sname,
	vnode_t		*tdir_vp,
	char		*tname,
	pathname_t	*tpnp,
	cred_t		*credp)
{
	dsvn_t		*sdsp = BHV_TO_DSVN(sdirbdp), *tdsp = NULL;
	int		error, same;
	bhv_desc_t	*nbdp, *tdsbdp;
	tk_set_t	tkacq;

	DSSTAT(dsvn_rename);
	PV_NEXT(sdirbdp, nbdp, vop_rename);

	tkc_acquire(sdsp->dsv_tclient, DVN_WRITE_TOKENS, &tkacq);
	ASSERT(tkacq == DVN_WRITE_TOKENS);

	if (vn_cmp(BHV_TO_VNODE(sdirbdp), tdir_vp)) {
		same = 1;
	} else {
			/* prevent behavior insert/remove */
		same = 0;
		VN_BHV_READ_LOCK(VN_BHV_HEAD(tdir_vp));
		tdsbdp = dsvn_vnode_to_dsbhv(tdir_vp);
		if (tdsbdp) {
			tdsp = BHV_TO_DSVN(tdsbdp);
			tkc_acquire(tdsp->dsv_tclient, DVN_WRITE_TOKENS, &tkacq);
			ASSERT(tkacq == DVN_WRITE_TOKENS);
		}
	}
	PVOP_RENAME(nbdp, sname, tdir_vp, tname, tpnp, credp, error);

	if (!same) {
		if (tdsp)
			tkc_release(tdsp->dsv_tclient, DVN_WRITE_TOKENS);
		VN_BHV_READ_UNLOCK(VN_BHV_HEAD(tdir_vp));
	}

	tkc_release (sdsp->dsv_tclient, DVN_WRITE_TOKENS);
	DVN_KTRACE8(DVN_TRACE_DSVOP, "dsvn_rename", sdsp,
		"tdsp", tdsp, "same", same, "error", error);
	return error;
}

/*
 * Mkdir vnode op.
 */
int
dsvn_mkdir(
	bhv_desc_t	*dirbdp,
	char		*name,
	vattr_t		*vap,
	vnode_t		**vpp,
	cred_t		*credp)
{
	dsvn_t		*dsp = BHV_TO_DSVN(dirbdp);
	int		error;
	bhv_desc_t	*nbdp;
	tk_set_t	tkacq;

	DSSTAT(dsvn_mkdir);
	PV_NEXT(dirbdp, nbdp, vop_mkdir);

	tkc_acquire(dsp->dsv_tclient, DVN_WRITE_TOKENS, &tkacq);
	ASSERT(tkacq == DVN_WRITE_TOKENS);
	PVOP_MKDIR(nbdp, name, vap, vpp, credp, error);
	tkc_release (dsp->dsv_tclient, DVN_WRITE_TOKENS);
	DVN_KTRACE4(DVN_TRACE_DSVOP, "dsvn_mkdir", dsp, "error", error);
	return error;
}

/*
 * Rmdir vnode op.
 */
int
dsvn_rmdir(
	bhv_desc_t	*dirbdp,
	char		*name,
	vnode_t		*cdir_vp,	/* current dir */
	cred_t		*credp)
{
	dsvn_t		*dsp = BHV_TO_DSVN(dirbdp);
	int		error;
	bhv_desc_t	*nbdp;
	tk_set_t	tkacq;

	DSSTAT(dsvn_rmdir);
	PV_NEXT(dirbdp, nbdp, vop_rmdir);

	tkc_acquire(dsp->dsv_tclient, DVN_WRITE_TOKENS, &tkacq);
	ASSERT(tkacq == DVN_WRITE_TOKENS);
	PVOP_RMDIR(nbdp, name, cdir_vp, credp, error);
	tkc_release (dsp->dsv_tclient, DVN_WRITE_TOKENS);
	DVN_KTRACE4(DVN_TRACE_DSVOP, "dsvn_rmdir", dsp, "error", error);
	return error;
}

/*
 * Readdir vnode op.
 */
int
dsvn_readdir(
	bhv_desc_t	*dirbdp,
	uio_t		*uiop,
	cred_t		*credp,
	int		*eofp)
{
	dsvn_t		*dsp = BHV_TO_DSVN(dirbdp);
	int		error;
	bhv_desc_t	*nbdp;

	DSSTAT(dsvn_readdir);
	PV_NEXT(dirbdp, nbdp, vop_readdir);

	PVOP_READDIR(nbdp, uiop, credp, eofp, error);
	if (!error) {
		tkc_acquire1(dsp->dsv_tclient, DVN_TIMES_SHARED_WRITE_1);
		tkc_release1 (dsp->dsv_tclient, DVN_TIMES_SHARED_WRITE_1);
	}
	DVN_KTRACE4(DVN_TRACE_DSVOP, "dsvn_readdir", dsp, "error", error);
	return error;
}

/*
 * Symlink vnode op.
 */
int
dsvn_symlink(
	bhv_desc_t	*dirbdp,
	char	       	*link_name,
	vattr_t		*vap,
	char		*target_path,
	cred_t		*credp)
{
	int		error;
	bhv_desc_t	*nbdp;
	dsvn_t		*dsp = BHV_TO_DSVN(dirbdp);
	tk_set_t	tkacq;

	DSSTAT(dsvn_symlink);
	PV_NEXT(dirbdp, nbdp, vop_symlink);

	tkc_acquire(dsp->dsv_tclient, DVN_WRITE_TOKENS, &tkacq);
	ASSERT(tkacq == DVN_WRITE_TOKENS);
	PVOP_SYMLINK(nbdp, link_name, vap, target_path, credp, error);
	tkc_release (dsp->dsv_tclient, DVN_WRITE_TOKENS);
	DVN_KTRACE4(DVN_TRACE_DSVOP, "dsvn_symlink", dsp, "error", error);
	return error;
}

/*
 * Readlink vnode op.
 */
int
dsvn_readlink(
	bhv_desc_t 	*bdp,
	uio_t		*uiop,
	cred_t		*credp)
{
	int		error;
	bhv_desc_t	*nbdp;
	dsvn_t		*dsp = BHV_TO_DSVN(bdp);

	DSSTAT(dsvn_readlink);
	PV_NEXT(bdp, nbdp, vop_readlink);

	PVOP_READLINK(nbdp, uiop, credp, error);
	if (!error) {
		tkc_acquire1(dsp->dsv_tclient, DVN_TIMES_SHARED_WRITE_1);
		tkc_release1 (dsp->dsv_tclient, DVN_TIMES_SHARED_WRITE_1);
	}
	DVN_KTRACE4(DVN_TRACE_DSVOP, "dsvn_readlink", dsp, "error", error);
	return error;
}

/*
 * Inactive vnode op.
 */
/* ARGSUSED */
int
dsvn_inactive(
	bhv_desc_t	*bdp,
	cred_t		*credp)
{
	dsvn_t		*dsp = BHV_TO_DSVN(bdp);
	vnode_t         *vp = BHV_TO_VNODE(bdp);
	bhv_desc_t	*nbdp;
	int		cache;
	
	DSSTAT(dsvn_inactive);
	/*
	 * Call the behavior below.  Special handling for autofs which
         * has non-VSHARE vnodes.
	 */
	PV_NEXT(bdp, nbdp, vop_inactive);
	if ((vp->v_flag & (VSHARE | 
			   VINACTIVE_TEARDOWN)) == VINACTIVE_TEARDOWN) {
  		dsvn_teardown(dsp, bdp);
		PVOP_INACTIVE(nbdp, credp, cache);
		ASSERT(cache == VN_INACTIVE_NOCACHE);
		DVN_KTRACE4(DVN_TRACE_DSVOP, "dsvn_inactive_NOCACHE", dsp,
				"vflag", vp->v_flag);
		return (cache);
	}
	PVOP_INACTIVE(nbdp, credp, cache);

	/*
	 * If link count on the file is zero (and since the reference
	 * count is also 0) then the file will never be accessed again.
	 * So, tear down the dsvn behavior now.  Also tear down if
	 * the layer below tore down (indicated by VN_INACTIVE_NOCACHE).
	 *
	 * Note that tearing down now due to a zero link count _must_
	 * be done because XFS can reuse vnodes/inodes without VOP_RECLAIM
	 * being called.
	 */
	if ((dsp->dsv_flags & DVN_LINKZERO) || cache == VN_INACTIVE_NOCACHE)
		dsvn_teardown(dsp, bdp);

	DVN_KTRACE6(DVN_TRACE_DSVOP, "dsvn_inactive_CACHE", dsp,
		"flags", dsp->dsv_flags, "cache", cache);

	return cache;
}

/*
 * Allocstore vnode op.
 */
int
dsvn_allocstore(
	bhv_desc_t	*bdp,
	off_t		offset,
	size_t		count,
	cred_t		*credp)
{
	int		error;
	bhv_desc_t	*nbdp;
	dsvn_t		*dsp = BHV_TO_DSVN(bdp);

	DSSTAT(dsvn_allocstore);
	PV_NEXT(bdp, nbdp, vop_allocstore);
	tkc_acquire1(dsp->dsv_tclient, DVN_EXTENT_WRITE_1);

	PVOP_ALLOCSTORE(nbdp, offset, count, credp, error);
	tkc_release1 (dsp->dsv_tclient, DVN_EXTENT_WRITE_1);
	DVN_KTRACE8(DVN_TRACE_DSVOP, "dsvn_allocstore", dsp, "offset", offset,
		"count", count, "error", error);
	return error;
}

/*
 * Fcntl vnode op.
 */
int
dsvn_fcntl(
	bhv_desc_t	*bdp,
	int		cmd,
	void		*arg,
	int		flags,
	off_t		offset,
	cred_t		*credp,
	rval_t		*rvalp)
{
	int		error;
	bhv_desc_t	*nbdp;
	dsvn_t		*dsp = BHV_TO_DSVN(bdp);
	tk_set_t	tkacq;

	DSSTAT(dsvn_fcntl);
	PV_NEXT(bdp, nbdp, vop_fcntl);
	/*
	 * For now, assume the worst case that the fcntl is changing everything.
	 */
	tkc_acquire(dsp->dsv_tclient, DVN_WRITE_TOKENS, &tkacq);
	ASSERT(tkacq == DVN_WRITE_TOKENS);
	PVOP_FCNTL(nbdp, cmd, arg, flags, offset, credp, rvalp, error);
	tkc_release (dsp->dsv_tclient, DVN_WRITE_TOKENS);
	DVN_KTRACE4(DVN_TRACE_DSVOP, "dsvn_fcntl", dsp, "error", error);
	return error;
}
/*
 * Reclaim vnode op.
 */
/* ARGSUSED */
int
dsvn_reclaim(
	bhv_desc_t	*bdp,
	int		flag)
{
	dsvn_t		*dsp = BHV_TO_DSVN(bdp);
	bhv_desc_t	*nbdp;
	int		error;
	
	DSSTAT(dsvn_reclaim);
	/*
	 * Call to next behavior first.  If it succeeds, teardown here too.
	 */
	PV_NEXT(bdp, nbdp, vop_reclaim);
	PVOP_RECLAIM(nbdp, flag, error);
	DVN_KTRACE6(DVN_TRACE_DSVOP, "dsvn_reclaim", dsp, "flag", flag,
		"error", error);
	if (error)
		return error;

	dsvn_teardown(dsp, bdp);

	return 0;
}

/*
 * Link removed vnode op.
 */
/* ARGSUSED */
void
dsvn_link_removed(
	bhv_desc_t	*bdp,		/* file with the link removed */
	vnode_t		*dvp,		/* parent vnode */
	int		linkzero)	/* is the link count now zero? */
{
	dsvn_t		*dsp = BHV_TO_DSVN(bdp);
	tk_set_t	refset;
	tk_set_t	tkacq;

	DSSTAT(dsvn_link_removed);
	/*
	 * If the link count for the file is now zero, set the dsvn's
	 * 'linkzero' flag.  This ensures that any client obtaining an
	 * existence token for the file (e.g., obtain_exist) will return
	 * the existence token when its vnode goes inactive.  This is
	 * needed so that the server-side vnode will also go inactive
	 * and the disk space may be reclaimed.
	 *
	 * Must set the flag prior to revoking the existence tokens in
	 * order to maintain the protocol associated with DSVN_CLIENT_FLAGS.
	 *
	 * Also, if the link count is now zero, must attempt to revoke all 
	 * existence tokens.  Each client will either return its existence 
	 * token, or refuse to return it if it has the vnode referenced, 
	 * but in that case it will return the existence token when its 
	 * vnode goes inactive.
	 * 
	 * If/when name and attribute caching are implemented, we also
	 * will need to revoke name and attribute tokens.
	 */

	DVN_KTRACE4(DVN_TRACE_DSVOP, "dsvn_link_removed", dsp,
		"linkzero", linkzero);

	if (linkzero) {
		DSVN_FLAGSET(dsp, DVN_LINKZERO);
		
		/*
		 * Doing a recall with the 'refset' arg being non-null
		 * causes it to be done synchronously.  This is important
		 * because we want to synchronously recall and provide
		 * clients with the DVN_LINKZERO flag.
		 */
		tks_recall(dsp->dsv_tserver, 
			DVN_ALL_CLIENT_TOKENS, &refset); 
	} else {
		/*
		 * get the attribute write token since the link count has
		 * changed.
		 */
		tkc_acquire(dsp->dsv_tclient, DVN_ATTR_MOD_TOKENS, &tkacq);
		ASSERT(tkacq == DVN_ATTR_MOD_TOKENS);
		tkc_release(dsp->dsv_tclient, DVN_ATTR_MOD_TOKENS);
	}
	
	/*
	 * No need to call to next behavior because we're the only 
	 * behavior interested in this VOP.
	 */
}


/*
 * Internal routines for sending a distributed page cache operation
 * to all clients that have pages of the vode cached on the client.
 */
void
dsvn_page_cache_operation(
	int		opcode,
	dsvn_t		*dsp,
	off_t		first,
	off_t		last,
	int		flags,
	int		fiopt)
{
	/* REFERENCED */
	tks_iter_t	result;

	/*
	 * If doing a REMAPPED LOCKED operation, we must acquire
	 * the DVN_PSEARCH token. This keeps clients from finding
	 * pages in their page cache until the entire operation 
	 * is complete.
	 */
	if (fiopt == FI_REMAPF_LOCKED) {
		tkc_acquire1 (dsp->dsv_tclient, DVN_PSEARCH_WRITE_1);
		ASSERT(dsp->dsv_vrgen_flags == 0);
		dsp->dsv_vrgen_flags |= VRGEN_ACTIVE;
	}

	/*
	 * if there are no pages in the page cache, skip the operation
	 * on clients.
	 */
	if (DSVN_TO_VNODE(dsp)->v_pgcnt && last > 0) {
		result = tks_iterate(dsp->dsv_tserver,
			DVN_PCACHE_READ, 0, dsvn_page_cache_operation_iterator,
			opcode, first, last, flags, fiopt);

		ASSERT(result == TKS_CONTINUE);
	}
	DVN_KTRACE8(DVN_TRACE_MISC, "dsvn_pg_ca_ops", dsp, "opcode", opcode, 
		"pgcnt", DSVN_TO_VNODE(dsp)->v_pgcnt, "last", last);
}


/* ARGSUSED */
tks_iter_t
dsvn_page_cache_operation_iterator(
        void            *dsobj,
        tks_ch_t        client,
        tk_set_t        tokens_owned,
        va_list         args)
{
        dsvn_t          *dsp = (dsvn_t *)dsobj;
        service_t       service;
        cfs_handle_t    handle;
	int		opcode;
        off_t	        first;
        off_t	        last;
        int             flags;
        int             fiopt;
	int		error;
	/* REFERENCED */
	int		msgerr;


	DSSTAT(dsvn_page_cache_op_it);
        opcode = va_arg(args, off_t);
        first = va_arg(args, off_t);
        last = va_arg(args, off_t);
        flags = va_arg(args, int);
        fiopt = va_arg(args, int);

        ASSERT(client != cellid());

        /*
         * Contact the client to do the operation.
	 */
        SERVICE_MAKE(service, (cell_t)client, SVC_CFS);
        DSVN_HANDLE_MAKE(handle, dsp);
        msgerr = invk_dcvn_page_cache_operation(service, &handle, 
			opcode, first, last, flags, fiopt, &error);
	ASSERT(!msgerr);

	DVN_KTRACE8(DVN_TRACE_MISC, "dsvn_pg_ca_ops_iter", dsp,
		"client", client, "tokens_owned", tokens_owned,
		"error", error);
        if (error)
                return TKS_RETRY;
        else
                return TKS_CONTINUE;
}


void
dsvn_release_pcache_token(
	dsvn_t	*dsp,
	int	fiopt)
{
	DSSTAT(dsvn_rel_pcache_tok);

	if (fiopt == FI_REMAPF_LOCKED) {
		ASSERT(dsp->dsv_vrgen_flags == VRGEN_ACTIVE);
		dsp->dsv_vrgen_flags = 0;
		tkc_release1 (dsp->dsv_tclient, DVN_PSEARCH_WRITE_1);
	}
}


/*
 * vnode pcache layer for vnode_tosspages.
 */
void
dsvn_tosspages(
        bhv_desc_t	*bdp,
	off_t		first,
	off_t		last,
	int		fiopt)
{
	bhv_desc_t	*nbdp;
	dsvn_t          *dsp = BHV_TO_DSVN(bdp);

	DSSTAT(dsvn_tosspages);

	DVN_KTRACE8(DVN_TRACE_DSVOP, "dsvn_tosspages", dsp, "first", first, 
		"last", last, "fiopt", fiopt);

	dsvn_page_cache_operation(CFS_TOSS, dsp, first, last, 0, fiopt);

	PV_NEXT(bdp, nbdp, vop_tosspages);
	PVOP_TOSS_PAGES(nbdp, first, last, fiopt);

	dsvn_release_pcache_token (dsp, fiopt);
}

/*
 * vnode pcache layer for vnode_flushinval_pages.
 */
void
dsvn_flushinval_pages(
        bhv_desc_t	*bdp,
	off_t		first,
	off_t		last,
	int		fiopt)
{
	bhv_desc_t	*nbdp;
	dsvn_t          *dsp = BHV_TO_DSVN(bdp);

	dsvn_page_cache_operation(CFS_FLUSHINVAL, dsp, first, last, 0, fiopt);

	PV_NEXT(bdp, nbdp, vop_flushinval_pages);
	PVOP_FLUSHINVAL_PAGES(nbdp, first, last, fiopt);

	dsvn_release_pcache_token (dsp, fiopt);
}

/*
 * vnode pcache layer for vnode_flush_pages.
 */
int
dsvn_flush_pages(
        bhv_desc_t	*bdp,
	off_t		first,
	off_t		last,
	uint64_t	flags,
	int		fiopt)
{
	bhv_desc_t	*nbdp;
	dsvn_t          *dsp = BHV_TO_DSVN(bdp);
	int		ret;

	DSSTAT(dsvn_flush_pages);
	dsvn_page_cache_operation(CFS_FLUSH, dsp, first, last, flags, fiopt);

	PV_NEXT(bdp, nbdp, vop_flush_pages);
	PVOP_FLUSH_PAGES(nbdp, first, last, flags, fiopt, ret);

	dsvn_release_pcache_token (dsp, fiopt);

	DVN_KTRACE10(DVN_TRACE_DSVOP, "dsvn_flush_pages", dsp, "first", first, 
		"last", last, "fiopt", fiopt, "ret", ret);

	return(ret);
}

/*
 * vnode pcache layer for vnode_invalfree_pages.
 */
void
dsvn_invalfree_pages(
        bhv_desc_t	*bdp,
	off_t		filesize)
{
	bhv_desc_t	*nbdp;
	dsvn_t          *dsp = BHV_TO_DSVN(bdp);

	DSSTAT(dsvn_invalfree_pages);
	dsvn_page_cache_operation(CFS_INVALFREE, dsp, 0, filesize, 0, FI_NONE);

	PV_NEXT(bdp, nbdp, vop_invalfree_pages);
	PVOP_INVALFREE_PAGES(nbdp, filesize);

	dsvn_release_pcache_token (dsp, FI_NONE);
	DVN_KTRACE4(DVN_TRACE_DSVOP, "dsvn_invalfree_pages", dsp,
		"filesize", filesize);
}


/*
 * vnode pcache layer for vnode_pages_sethole.
 */
void
dsvn_pages_sethole(
        bhv_desc_t	*bdp,
	pfd_t		*pfd,
	int		cnt,
	int		doremap,
	off_t		remapoffset)
{
	bhv_desc_t	*nbdp;
	dsvn_t          *dsp = BHV_TO_DSVN(bdp);
	off_t		offset;

	DSSTAT(dsvn_pages_sethole);
	offset = ctob(pfd->pf_pageno);
	dsvn_page_cache_operation(CFS_FLUSHINVAL, dsp, offset, offset+NBPP*(cnt-1), 0, FI_NONE);

	PV_NEXT(bdp, nbdp, vop_pages_sethole);
	PVOP_PAGES_SETHOLE(nbdp, pfd, cnt, doremap, remapoffset);

	dsvn_release_pcache_token (dsp, FI_NONE);
	DVN_KTRACE8(DVN_TRACE_DSVOP, "dsvn_pages_sethole", dsp, "offset", offset, 
		"cnt", cnt, "doremap", doremap);

}

/*
 * Vnode_change vnode op.  Also called by I_dsvn_vnode_change.
 */
/* ARGSUSED */
void
dsvn_vnode_change(
	bhv_desc_t	*bdp,	
	vchange_t	cmd,
        __psint_t	val)
{
	dsvn_t		*dsp = BHV_TO_DSVN(bdp);
	/* REFERENCED */
	tks_iter_t 	result;
	tk_set_t 	tkacq;

	DSSTAT(dsvn_vnode_change);
	DVN_KTRACE4(DVN_TRACE_DSVOP, "dsvn_vnode_change", dsp, "cmd", cmd);

	/*
	 * If this is a truncate, let the token module do the work
	 * by getting the "truncate" tokens in write mode.
	 */

	if (cmd == VCHANGE_FLAGS_TRUNCATED) {
		tkc_acquire(dsp->dsv_tclient, DVN_TRUNCATE_TOKENS, &tkacq);
		ASSERT(tkacq == DVN_TRUNCATE_TOKENS);
		tkc_release (dsp->dsv_tclient, DVN_TRUNCATE_TOKENS);
		return;
	}

	/*
	 * Serialize all changes.
	 *
	 * Note:  be careful with this lock.  For example, xfs_setattr
	 * calls VOP_VNODE_CHANGE with its inode lock held, implying
	 * there's an ordering of inode lock->dsvn mutex.  So, to 
	 * avoid deadlock the invk_dcvn_vnode_change callback to the 
	 * client must _never_ result in a VOP that could possibly take 
	 * the inode lock.
	 */
	mutex_lock(&dsp->dsv_mutex, PRIBIO);

	/*
	 * Apply the change locally first and then send to all the
	 * clients.
	 */
	fs_vnode_change(bdp, cmd, val);
	
	result = tks_iterate(dsp->dsv_tserver,
			     DVN_EXIST_READ, 0, vnode_change_iterator,
			     cmd, val);

	/* result is the last value returned from vnode_change_iterator */
	ASSERT(result == TKS_CONTINUE);

	mutex_unlock(&dsp->dsv_mutex);
}

/* ARGSUSED */
static tks_iter_t
vnode_change_iterator(
	void            *dsobj,
	tks_ch_t        client,
	tk_set_t        tokens_owned,
	va_list         args)
{
	dsvn_t		*dsp = (dsvn_t *)dsobj;
	service_t	service;
	cfs_handle_t	handle;
	vchange_t	cmd;
	__psint_t	val;
	int		error;
	/* REFERENCED */
	int		msgerr;

        ASSERT(TK_IS_IN_SET(DVN_EXIST_READ, tokens_owned));

	DSSTAT(vnode_change_iterator);
	cmd = va_arg(args, vchange_t);
	val = va_arg(args, __psint_t);

	/* local client doesn't hold an existence token */
	ASSERT(client != cellid());

	/*
	 * Contact the client.
	 */
	SERVICE_MAKE(service, (cell_t)client, SVC_CFS);
	DSVN_HANDLE_MAKE(handle, dsp);
	msgerr = invk_dcvn_vnode_change(service, &handle, cmd, val, &error);
	ASSERT(!msgerr);

	DVN_KTRACE6(DVN_TRACE_DSVOP, "vnode_change_iterator", dsp, "cmd", cmd,
		"error", error);

	if (error)
		return TKS_RETRY;
	else
		return TKS_CONTINUE;
}


void
dsvn_strgetmsg(
	bhv_desc_t	*bdp,
	struct strbuf	*mctl,
	struct strbuf	*mdata,
	unsigned char	*prip,
	int		*flagsp,
	int		fmode,
	union rval	*rvp,
	int		*errorp)
{
	DSSTAT(dsvn_strgetmsg);
	*errorp = fs_strgetmsg(bdp, mctl, mdata, prip, flagsp, fmode, rvp);
	DVN_KTRACE4(DVN_TRACE_DSVOP, "dsvn_strgetmsg", bdp, "error", *errorp);
}


void
dsvn_strputmsg(
	bhv_desc_t	*bdp,
	struct strbuf	*mctl,
	struct strbuf	*mdata,
	unsigned char	pri,
	int		flag,
	int		fmode,
	int		*errorp)
{
	DSSTAT(dsvn_strputmsg);
	*errorp = fs_strputmsg(bdp, mctl, mdata, pri, flag, fmode);
	DVN_KTRACE4(DVN_TRACE_DSVOP, "dsvn_strputmsg", bdp, "error", *errorp);
}


/*
 *********************** Token module callouts ******************************
 */

/*
 * Token callout for recalling (aka revoking) a token.
 */
/* ARGSUSED */
static void
dsvn_tsif_recall(
	void		*dsobj,
	tks_ch_t	client,
	tk_set_t	recset,
	tk_disp_t	why)
{
	dsvn_t		*dsp = (dsvn_t *)dsobj;
	service_t	service;
	cfs_handle_t	handle;
	/* REFERENCED */
	int		msgerr;

	DSSTAT(dsvn_tsif_recall);
	DVN_KTRACE6(DVN_TRACE_TOKEN, "dsvn_tsif_recall", dsp,
		"client", client, "recset", recset);
	if (client == cellid()) {
		/* local client */
		tkc_recall(dsp->dsv_tclient, recset, why);
	} else {
		/*
		 * Send a message to recall the tokens.
		 *
		 * Note that recalling the existence token indicates
		 * to the client that the file has a zero link count.  
		 */
		ASSERT((recset & DVN_EXIST_READ) ? 
		       (dsp->dsv_flags & DVN_LINKZERO) : 1);
		SERVICE_MAKE(service, (cell_t)client, SVC_CFS);
		DSVN_HANDLE_MAKE(handle, dsp);

		msgerr = invk_dcvn_recall(service, &handle, recset, why);
		ASSERT(!msgerr);
	}
}

/*
 * Token callout upon completion of a recall.
 */
/* ARGSUSED */
static void
dsvn_tsif_recalled(
        void 		*dsobj, 
	tk_set_t 	recset, 
        tk_set_t 	failset)
{
	DSSTAT(dsvn_tsif_recalled);
	/*
	 * Nothing to do when a token recall has completed.  A key
	 * interesting event for CFS server-side vnodes is that once all
	 * existence tokens have been returned, and the corresponding
	 * vnode references released, the dsvn_reclaim() routine will
	 * be called.
	 */
	return;
}
