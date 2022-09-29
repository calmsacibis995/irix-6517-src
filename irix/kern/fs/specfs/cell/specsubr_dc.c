/**************************************************************************
 *									  *
 *	 	Copyright (C) 1986-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded	instructions, statements, and computer programs	 contain  *
 *  unpublished	 proprietary  information of Silicon Graphics, Inc., and  *
 *  are	protected by Federal copyright law.  They  may	not be disclosed  *
 *  to	third  parties	or copied or duplicated	in any form, in	whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.9 $"

#include <sys/types.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <ksys/ddmap.h>
#include <sys/errno.h>
#include <ksys/vfile.h>
#include <ksys/hwg.h>
#include <sys/kabi.h>
#include <sys/kmem.h>
#include <sys/open.h>
#include <sys/param.h>
#include <sys/sema.h>
#include <sys/stream.h>
#include <sys/strsubr.h>
#include <sys/strmp.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/termios.h>
#include <sys/time.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/ddi.h>
#include <sys/major.h>
#include <sys/alenlist.h>
#include <sys/pfdat.h>                  /* VMKLUDGE ZZZ */
#include <sys/vnode_private.h>          /* VMKLUDGE ZZZ */
#include <ksys/cred.h>
#include <fs/specfs/spec_lsnode.h>
#include "invk_spec_ds_stubs.h"
#include "I_spec_dc_stubs.h"



/*
 * "spec_vnimport()"
 *
 * Currently called out of:
 *	cfs_vnimport()		 to import a reference to a specfs vnode.
 *	cfs_vn_source_retarget() to create a specfs connection
 *				 for a dsvn becoming a dcvn.
 *
 * Use the contents of the "spec_handle_t" in order
 * to create a local spec vnode/snode that will "attach"
 * back to this cell's common vnode/snode.
 */
vnode_t *
spec_vnimport(	vnode_t		*vp,
		struct cred	*credp,
		int		opencnt)
{
	int		error;
	bhv_desc_t	*bdp;
	lsnode_t	*lsp;
	vnode_t		*nvp;
	vnode_t		*rvp;


	SPEC_STATS(spec_vnimport);

#ifdef	JTK_DEBUG
	printf("spec_vnimport[%d]: vp/0x%x dev(%d/%d, %d) opencnt/%d\n",
				cellid(), vp,
				major(vp->v_rdev), emajor(vp->v_rdev),
				minor(vp->v_rdev), opencnt);
#endif	/* JTK_DEBUG */

	/*
	 * 1st: Create/attach to a "local" vnode/snode pair on this cell.
	 */
	rvp = spec_vp(vp, vp->v_rdev, vp->v_type, credp);

	lsp = VP_TO_LSP(rvp);
					/* Make sure things are   */
					/* what we think they are */
	ASSERT((lsp->ls_flag & SCOMMON) == 0);

	if (SPECFS_IS_LOCAL_DEVICE(rvp)) {

		/*
		 * We're "importing" a device that needs to run entirely
		 * on the "local" cell, since we know the device was
		 * originally opened on the "source" cell (otherwise we
		 * wouldn't be importing it), we need to now open the
		 * device on the "target" cell.
		 *
		 * Would be "nice" to have had the original "mode" flags..
		 */
		bdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(rvp), &spec_vnodeops);

		ASSERT(bdp);		/* Gotta be there!		*/

		nvp = rvp;		/* Preset the answer		*/

		while (opencnt--) {
			error = spec_open(bdp, &nvp, FREAD|FWRITE, credp);

			ASSERT(nvp == rvp);	/* Clones can't happen here! */

			if (error) {
				panic("spec_vnimport: spec_open failed!");
			}
		}
	}

	return rvp;
}


/*
 * "spec_dc_attach" is called through the spec_ls2dc_ops vector.
 */
STATIC void
spec_dc_attach(
	dev_t		dev,			/* in */
	vtype_t		type,			/* in */
	spec_handle_t	*handle,		/* out */
	spec_handle_t	*cvp_handle,		/* out */
	long		*gen,			/* out */
	long		*size,			/* out */
	struct stdata	**stream)		/* out */
{
	service_t	svc;
	spec_handle_t	target_handle;
	spec_handle_t	local_stream;
	/* REFERENCED */
	int		msgerr;

	SPEC_STATS(spec_dc_attach);

	/*
	 * Manufacture a "handle" that will lead to the specfs
	 * service running on the "appropriate" cell.
	 *
	 * It was spec_attach() that figured out which cell to
	 * use, and passed that information in as part of the
	 * "target" handle structure.
	 */
	SERVICE_MAKE(svc, handle->sh_obj.h_service.s_cell, SVC_SPECFS);

	SPEC_HANDLE_MAKE(target_handle, svc, NULL, 0, VNON);

	local_stream.sh_obj.h_objid = NULL;

#ifdef	JTK_DEBUG
	printf("spec_dc_attach[%d]: dev/0x%x(%d/%d, %d) type/%d\n",
				cellid(), dev,
				major(dev), emajor(dev), minor(dev), type);
#endif	/* JTK_DEBUG */

	msgerr = invk_spec_ds_attach(SPEC_HANDLE_TO_SERVICE(target_handle),
					dev, type, handle, cvp_handle,
				gen, size, &local_stream);
	ASSERT(!msgerr);

#ifdef	JTK_DEBUG_LVL_2
	printf("spec_dc_attach.r: handle/0x%x = %d/%d/0x%x\n",
					handle,
					handle->sh_obj.h_service.s_cell,
					handle->sh_obj.h_service.s_svcnum,
					handle->sh_obj.h_objid);
	printf("spec_dc_attach.r: cvp_handle.h_objid/0x%x = 0x%x\n",
					cvp_handle,
					cvp_handle->sh_obj.h_objid);
	printf("spec_dc_attach.r: gen/0x%x = %d\n", gen, *gen);
	printf("spec_dc_attach.r: size/0x%x = %d\n", size, *size);
	printf("spec_dc_attach.r: stream/0x%x = 0x%x\n",
					stream,
					local_stream.sh_obj.h_objid);
#endif	/* JTK_DEBUG_LVL_2 */

	*stream   = (struct stdata *)local_stream.sh_obj.h_objid;
}


/*
 * "spec_dc_clone" is called through the spec_ls2dc_ops vector.
 */
STATIC void
spec_dc_clone(
	spec_handle_t	*handle,		/* in */
	struct stdata	*stp,			/* in */
	spec_handle_t	*ovp_handle,		/* in */
	int		flag,			/* in */
	struct stdata	**stream)		/* out */
{
	spec_handle_t	local_stp;
	spec_handle_t	local_stream;
	/* REFERENCED */
	int		msgerr;


	SPEC_STATS(spec_dc_clone);

	local_stp.sh_obj.h_objid = (void *)stp;

	local_stream.sh_obj.h_objid = NULL;

#ifdef	JTK_DEBUG_LVL_2
	printf("spec_dc_clone: handle/0x%x = %d/%d/0x%x\n",
					handle,
					handle->sh_obj.h_service.s_cell,
					handle->sh_obj.h_service.s_svcnum,
					handle->sh_obj.h_objid);
	printf("spec_dc_clone: stp/0x%x=0x%x\n", &local_stp, stp);
	printf("spec_dc_clone: ovp_handle/0x%x=0x%x\n",
					ovp_handle,
					ovp_handle->sh_obj.h_objid);
	printf("spec_dc_clone: flag/0x%x\n", flag);
#endif	/* JTK_DEBUG_LVL_2 */

	msgerr = invk_spec_ds_clone(SPEC_HANDLE_TO_SERVICE(*handle),
				handle, &local_stp, ovp_handle, flag, &local_stream);
	ASSERT(!msgerr);

#ifdef	JTK_DEBUG_LVL_2
	printf("spec_dc_clone.r: stream/0x%x = 0x%x\n",
					stream,
					local_stream.sh_obj.h_objid);
#endif	/* JTK_DEBUG_LVL_2 */

	*stream = (struct stdata *)local_stream.sh_obj.h_objid;
}


/*
 * called through the spec_ls2dc_ops vector.
 */
STATIC int
spec_dc_cmp_gen(
	spec_handle_t	*handle,		/* in */
	long		lsp_gen)		/* in */
{
	int		result;


	SPEC_STATS(spec_dc_cmp_gen);

	result = invk_spec_ds_cmp_gen(SPEC_HANDLE_TO_SERVICE(*handle),
						handle, lsp_gen);
	ASSERT(result != ECELLDOWN);

#ifdef	JTK_DEBUG
	if (result == 0) {
		printf("spec_dc_cmp_gen: handle/0x%x = %d/%d/0x%x\n",
					handle,
					handle->sh_obj.h_service.s_cell,
					handle->sh_obj.h_service.s_svcnum,
					handle->sh_obj.h_objid);
		printf("spec_dc_cmp_gen: lsp_gen/%d\n", lsp_gen);
		printf("spec_dc_cmp_gen.r: result/%d\n", result);
	}
#endif	/* JTK_DEBUG */

	return result;
}


/*
 * called through the spec_ls2dc_ops vector.
 */
STATIC int
spec_dc_get_gen(
	spec_handle_t	*handle)		/* in */
{
	int		result;


	SPEC_STATS(spec_dc_get_gen);

	result = invk_spec_ds_get_gen(SPEC_HANDLE_TO_SERVICE(*handle), handle);
	ASSERT(result != ECELLDOWN);

	return result;
}


/*
 * called through the spec_ls2dc_ops vector.
 */
STATIC int
spec_dc_get_opencnt(
	spec_handle_t	*handle)		/* in */
{
	int		result;


	SPEC_STATS(spec_dc_get_opencnt);

	result = invk_spec_ds_get_opencnt(SPEC_HANDLE_TO_SERVICE(*handle),
									handle);
	ASSERT(result != ECELLDOWN);

	return result;
}


/*
 * called through the spec_ls2dc_ops vector.
 */
STATIC daddr_t
spec_dc_get_size(
	spec_handle_t	*handle)		/* in */
{
	daddr_t		result;
	/* REFERENCED */
	int		msgerr;

	SPEC_STATS(spec_dc_get_size);

#ifdef	JTK_DEBUG
	printf("spec_dc_get_size: retaddr/0x%x\n", __return_address);
	printf("spec_dc_get_size: handle/0x%x = %d/%d/0x%x\n",
					handle,
					handle->sh_obj.h_service.s_cell,
					handle->sh_obj.h_service.s_svcnum,
					handle->sh_obj.h_objid);
#endif	/* JTK_DEBUG */

	msgerr = invk_spec_ds_get_size(SPEC_HANDLE_TO_SERVICE(*handle),
					handle, &result);
	ASSERT(!msgerr);

#ifdef	JTK_DEBUG
	printf("spec_dc_get_size.r: result/%d\n", result);
#endif	/* JTK_DEBUG */

	return result;
}


/*
 * called through the spec_ls2dc_ops vector.
 */
STATIC int
spec_dc_ismounted(
	spec_handle_t	*handle)		/* in */
{
	int		result;


	SPEC_STATS(spec_dc_ismounted);

#ifdef	JTK_DEBUG
	printf("spec_dc_ismounted: retaddr/0x%x\n", __return_address);
	printf("spec_dc_ismounted: handle/0x%x = %d/%d/0x%x\n",
					handle,
					handle->sh_obj.h_service.s_cell,
					handle->sh_obj.h_service.s_svcnum,
					handle->sh_obj.h_objid);
#endif	/* JTK_DEBUG */

	result = invk_spec_ds_ismounted(SPEC_HANDLE_TO_SERVICE(*handle),
								handle);
	ASSERT(result != ECELLDOWN);

#ifdef	JTK_DEBUG
	printf("spec_dc_ismounted.r: result/%d\n", result);
#endif	/* JTK_DEBUG */

	return result;
}


/*
 * called through the spec_ls2dc_ops vector.
 */
STATIC void
spec_dc_mountedflag(
	spec_handle_t	*handle,		/* in */
	int		set)			/* in */
{
	/* REFERENCED */
	int	msgerr;

	SPEC_STATS(spec_dc_mountedflag);

#ifdef	JTK_DEBUG
	printf("spec_dc_mountedflag: retaddr/0x%x\n", __return_address);
	printf("spec_dc_mountedflag: handle/0x%x = %d/%d/0x%x\n",
					handle,
					handle->sh_obj.h_service.s_cell,
					handle->sh_obj.h_service.s_svcnum,
					handle->sh_obj.h_objid);
#endif	/* JTK_DEBUG */

	msgerr = invk_spec_ds_mountedflag(SPEC_HANDLE_TO_SERVICE(*handle),
					handle, set);
	ASSERT(!msgerr);
}


/*
 * called through the spec_ls2dc_ops vector.
 */
/* ARGSUSED */
int
spec_dc_poll_hndl(
	spec_handle_t	*handle,		/* in */
	long		gen,			/* in */
	short		events,			/* in */
	int		anyyet,			/* in */
	short		*reventsp,		/* out */
	struct pollhead	**phpp,			/* out */
	unsigned int	*genp)			/* out */
{
	int		error;
	spec_handle_t	local_phpp;

	SPEC_STATS(spec_dc_poll_hndl);

	local_phpp.sh_obj.h_objid = (void *)*phpp;

#ifdef	JTK_POLL_DEBUG
	printf("spec_dc_poll_hndl: handle/0x%x = %d/%d/0x%x\n",
					handle,
					handle->sh_obj.h_service.s_cell,
					handle->sh_obj.h_service.s_svcnum,
					handle->sh_obj.h_objid);
	printf("spec_dc_poll_hndl: gen/%d\n", gen);
	printf("spec_dc_poll_hndl: events/0x%x\n", events);
	printf("spec_dc_poll_hndl: anyyet/0x%x\n", anyyet);
#endif	/* JTK_POLL_DEBUG */

	error = invk_spec_ds_poll(SPEC_HANDLE_TO_SERVICE(*handle), handle, gen,
					events, anyyet, reventsp, &local_phpp,
					genp);
	ASSERT(error != ECELLDOWN);

#ifdef	JTK_POLL_DEBUG
	printf("spec_dc_poll_hndl.r: reventsp/0x%x = 0x%x\n",
						reventsp, *reventsp);
	printf("spec_dc_poll_hndl.r: phpp/0x%x = 0x%x\n",
					phpp, local_phpp.sh_obj.h_objid);
	printf("spec_dc_poll_hndl.r: genp/0x%x = %d\n", genp, *genp);
	printf("spec_dc_poll_hndl.r: error/%d\n", error);
#endif	/* JTK_POLL_DEBUG */

	*phpp = (struct pollhead *)local_phpp.sh_obj.h_objid;

	return error;
}


/*
 * called through the spec_ls2dc_ops vector.
 */
/* ARGSUSED */
int
spec_dc_bmap_subr(
	spec_handle_t	*handle,		/* in */
	off_t		offset,			/* in */
	ssize_t		count,			/* in */
	int		rw,			/* in */
	struct cred	*credp,			/* in */
	struct bmapval	*bmap,			/* out */
	int		*nbmap)			/* out */
{
	int		error;


	SPEC_STATS(spec_dc_bmap_subr);

#ifdef	JTK_DEBUG
	printf("spec_dc_bmap_subr: handle/0x%x = %d/%d/0x%x\n",
					handle,
					handle->sh_obj.h_service.s_cell,
					handle->sh_obj.h_service.s_svcnum,
					handle->sh_obj.h_objid);
	printf("spec_dc_bmap_subr: offset/%d\n", offset);
	printf("spec_dc_bmap_subr: count/%d\n", count);
	printf("spec_dc_bmap_subr: rw/0x%x\n", rw);
	printf("spec_dc_bmap_subr: credp/0x%x\n", credp);
#endif	/* JTK_DEBUG */

	error = invk_spec_ds_bmap(SPEC_HANDLE_TO_SERVICE(*handle), handle,
				  offset, count, rw, CRED_GETID(credp),
				  bmap, nbmap);
	ASSERT(error != ECELLDOWN);

#ifdef	JTK_DEBUG
	printf("spec_dc_bmap_subr.r: bmap/0x%x = %0x%x\n", bmap, *bmap);
	printf("spec_dc_bmap_subr.r: nbmap/0x%x = %d\n", nbmap, *nbmap);
	printf("spec_dc_bmap_subr.r: error/%d\n", error);
#endif	/* JTK_DEBUG */

	return error;
}


/*
 * called through the spec_ls2dc_ops vector.
 */
/* ARGSUSED */
int
spec_dc_open_hndl(
	spec_handle_t	*handle,		/* in */
	int		gen,			/* in */
	dev_t		*newdev,		/* in/out */
	long		*cs_size,		/* in/out */
	struct stdata	**stp,			/* out */
	mode_t		flag,			/* in */
	struct cred	*credp)			/* in */
{
	int		error;
#ifdef	JTK_DEBUG_LVL_2
	dev_t		in_newdev;
#endif	/* JTK_DEBUG_LVL_2 */
	spec_handle_t	local_stp;


	SPEC_STATS(spec_dc_open_hndl);

	local_stp.sh_obj.h_objid = NULL;

#ifdef	JTK_DEBUG_LVL_2
	in_newdev = *newdev;
#endif	/* JTK_DEBUG_LVL_2 */

	error = invk_spec_ds_open(SPEC_HANDLE_TO_SERVICE(*handle), handle, gen,
						flag, CRED_GETID(credp),
						newdev, cs_size, &local_stp);
	ASSERT(error != ECELLDOWN);

#ifdef	JTK_DEBUG_LVL_2
	if (error || (in_newdev != *newdev)) {
		printf("spec_dc_open_hndl: handle/0x%x = %d/%d/0x%x\n",
					handle,
					handle->sh_obj.h_service.s_cell,
					handle->sh_obj.h_service.s_svcnum,
					handle->sh_obj.h_objid);
		printf("spec_dc_open_hndl: gen/%d\n", gen);
		printf("spec_dc_open_hndl: in_newdev(%d/%d, %d)\n",
					major(in_newdev),
					emajor(in_newdev), minor(in_newdev));
		printf("spec_dc_open_hndl: flag/0x%x\n", flag);
		printf("spec_dc_open_hndl: credp/0x%x\n", credp);
		printf("spec_dc_open_hndl.r: newdev/0x%x (%d/%d, %d)\n",
					newdev, major(*newdev),
					emajor(*newdev), minor(*newdev));
		printf("spec_dc_open_hndl.r: cs_size/0x%x = %d\n",
						cs_size, *cs_size);
		printf("spec_dc_open_hndl.r: stp/0x%x = 0x%x\n",
						stp, local_stp.sh_obj.h_objid);
		printf("spec_dc_open_hndl.r: error/%d\n", error);
	}
#endif	/* JTK_DEBUG_LVL_2 */

	*stp = (struct stdata *)local_stp.sh_obj.h_objid;

	return error;
}


/*
 * called through the spec_ls2dc_ops vector.
 */
/* ARGSUSED */
int
spec_dc_read_hndl(
	spec_handle_t	*handle,		/* in */
	int		gen,			/* in */
	vnode_t		*vp,			/* in */
	struct uio	*uiop,			/* in */
	int		ioflag,			/* in */
	struct cred	*credp,			/* in */
	struct flid	*fl)			/* in */
{
	extern external_alenlist_t uio_to_plist(uio_t *, int, int *,
						struct iovec *, int *);
	extern void plist_free(int, int, struct iovec *);

	/* REFERENCED */
	int			dcalen;
	int			error;
	/* REFERENCED */
	external_alenlist_t	dcalist;
	off_t			uio_offset;
	short			uio_sigpipe;
	ssize_t			uio_resid;
	/* REFERENCED */
	struct iovec		saved_iov[16];


	SPEC_STATS(spec_dc_read_hndl);

	uiop->uio_pmp = NULL;

	ASSERT(uiop->uio_pbuf   == NULL);
	ASSERT(credp->cr_mac    == NULL);
	ASSERT(uiop->uio_iovcnt <= 16);

#ifdef	NOTYET
	if (  ! IS_KSEG0(uiop->uio_iov[0].iov_base)
	    && ((vp->v_type) == VREG || (vp->v_type == VDIR))) {

		dcalist = uio_to_plist(uiop, B_READ, &dcalen,
							saved_iov, &error);
		if (dcalist == NULL)
			return error;

		error = invk_spec_ds_list_read(SPEC_HANDLE_TO_SERVICE(*handle),
				handle, gen,
				uiop, uiop->uio_iov, uiop->uio_iovcnt, 
				dcalist,
				(dcalen / sizeof(struct external_alenlist)),
				ioflag, CRED_GETID(credp), fl,
				&uio_offset, &uio_sigpipe, &uio_resid);
		ASSERT(error != ECELLDOWN);

		if (   (uiop->uio_segflg != UIO_SYSSPACE)
		    && (uiop->uio_segflg != UIO_NOSPACE)) {

			plist_free(uiop->uio_iovcnt, B_READ|B_PHYS, saved_iov);
		}

		kmem_free (dcalist, dcalen);
	} else
#endif	/* NOTYET */
	{
		error = invk_spec_ds_read(SPEC_HANDLE_TO_SERVICE(*handle),
					handle, gen,
					uiop, uiop->uio_iov, uiop->uio_iovcnt,
					ioflag, CRED_GETID(credp), fl,
					&uio_offset, &uio_sigpipe, &uio_resid);
		ASSERT(error != ECELLDOWN);

	}

#ifdef	JTK_DEBUG
	if (error) {
		printf("spec_dc_read_hndl: handle/0x%x = %d/%d/0x%x\n",
					handle,
					handle->sh_obj.h_service.s_cell,
					handle->sh_obj.h_service.s_svcnum,
					handle->sh_obj.h_objid);
		printf("spec_dc_read_hndl: gen/%d\n", gen);
		printf("spec_dc_read_hndl: uiop/0x%x\n", uiop);
		printf("spec_dc_read_hndl: ioflag/0x%x\n", ioflag);
		printf("spec_dc_read_hndl: credp/0x%x\n", credp);
		printf("spec_dc_read_hndl: fl/0x%x\n", fl);
		printf("spec_dc_read_hndl.r: uio_offset/%d\n", uio_offset);
		printf("spec_dc_read_hndl.r: uio_resid/%d\n", uio_resid);
		printf("spec_dc_read_hndl.r: uio_sigpipe/0x%x\n",
								uio_sigpipe);
		printf("spec_dc_read_hndl.r: error/%d\n", error);
	}
#endif	/* JTK_DEBUG */

	uiop->uio_offset  = uio_offset;
	uiop->uio_sigpipe = uio_sigpipe;
	uiop->uio_resid   = uio_resid;

	return error;
}


/*
 * called through the spec_ls2dc_ops vector.
 */
/* ARGSUSED */
int
spec_dc_write_hndl(
	spec_handle_t	*handle,		/* in */
	int		gen,			/* in */
	vnode_t		*vp,			/* in */
	struct uio	*uiop,			/* in */
	int		ioflag,			/* in */
	struct cred	*credp,			/* in */
	struct flid	*fl)			/* in */
{
	extern external_alenlist_t uio_to_plist(uio_t *, int, int *,
						struct iovec *, int *);
	extern void plist_free(int, int, struct iovec *);

	int			error;
	/* REFERENCED */
	int			dcalen;
	/* REFERENCED */
	external_alenlist_t	dcalist;
	off_t			uio_offset;
	short			uio_sigpipe;
	ssize_t			uio_resid;
	/* REFERENCED */
	struct iovec		saved_iov[16];


	SPEC_STATS(spec_dc_write_hndl);

	uiop->uio_pmp = NULL;

	ASSERT(uiop->uio_pbuf   == NULL);
	ASSERT(credp->cr_mac    == NULL);
	ASSERT(uiop->uio_iovcnt <= 16);

#ifdef	NOTYET
	if (  ! IS_KSEG0(uiop->uio_iov[0].iov_base)
	    && ((vp->v_type) == VREG || (vp->v_type == VDIR))) {

		dcalist = uio_to_plist(uiop, B_WRITE, &dcalen,
						saved_iov, &error);
		if (dcalist == NULL)
			return error;
	
		error = invk_spec_ds_list_write(SPEC_HANDLE_TO_SERVICE(*handle),
				handle, gen,
				uiop, uiop->uio_iov, uiop->uio_iovcnt, 
				dcalist,
				(dcalen / sizeof(struct external_alenlist)),
				ioflag, CRED_GETID(credp), fl, 
				&uio_offset, &uio_sigpipe, &uio_resid);
		ASSERT(error != ECELLDOWN);

		if (   (uiop->uio_segflg != UIO_SYSSPACE)
		    && (uiop->uio_segflg != UIO_NOSPACE)) {

			plist_free(uiop->uio_iovcnt, B_WRITE|B_PHYS, saved_iov);
		}

		kmem_free(dcalist, dcalen);
	} else
#endif	/* NOTYET */
	{

		error = invk_spec_ds_write(SPEC_HANDLE_TO_SERVICE(*handle),
					handle, gen,
					uiop, uiop->uio_iov, uiop->uio_iovcnt,
					ioflag, CRED_GETID(credp), fl,
					&uio_offset, &uio_sigpipe, &uio_resid);
		ASSERT(error != ECELLDOWN);

	}

#ifdef	JTK_DEBUG
	if (error) {
		printf("spec_dc_write_hndl: handle/0x%x = %d/%d/0x%x\n",
					handle,
					handle->sh_obj.h_service.s_cell,
					handle->sh_obj.h_service.s_svcnum,
					handle->sh_obj.h_objid);
		printf("spec_dc_write_hndl: gen/%d\n", gen);
		printf("spec_dc_write_hndl: uiop/0x%x\n", uiop);
		printf("spec_dc_write_hndl: ioflag/0x%x\n", ioflag);
		printf("spec_dc_write_hndl: credp/0x%x\n", credp);
		printf("spec_dc_write_hndl: fl/0x%x\n", fl);
		printf("spec_dc_write_hndl.r: uio_offset/%d\n", uio_offset);
		printf("spec_dc_write_hndl.r: uio_resid/%d\n", uio_resid);
		printf("spec_dc_write_hndl.r: uio_sigpipe/0x%x\n",
								uio_sigpipe);
		printf("spec_dc_write_hndl.r: error/%d\n", error);
	}
#endif	/* JTK_DEBUG */

	uiop->uio_offset  = uio_offset;
	uiop->uio_sigpipe = uio_sigpipe;
	uiop->uio_resid   = uio_resid;

	return error;
}


/*
 * called through the spec_ls2dc_ops vector.
 */
/* ARGSUSED */
int
spec_dc_ioctl_hndl(
	spec_handle_t	*handle,		/* in */
	int		gen,			/* in */
	int		cmd,			/* in */
	void		*arg,			/* in */
	int		mode,			/* in */
	struct cred	*credp,			/* in */
	int		*rvalp,			/* out */
	struct vopbd	*vbd)			/* out */
{
	int		error;


	SPEC_STATS(spec_dc_ioctl_hndl);

	error = invk_spec_ds_ioctl(SPEC_HANDLE_TO_SERVICE(*handle), handle,
					gen, cmd, arg, mode, CRED_GETID(credp),
					rvalp, vbd);
	ASSERT(error != ECELLDOWN);

#ifdef	JTK_DEBUG_LVL_2
	if (error) {
		printf("spec_dc_ioctl_hndl: handle/0x%x = %d/%d/0x%x\n",
					handle,
					handle->sh_obj.h_service.s_cell,
					handle->sh_obj.h_service.s_svcnum,
					handle->sh_obj.h_objid);
		printf("spec_dc_ioctl_hndl: gen/%d\n", gen);
		printf("spec_dc_ioctl_hndl: cmd/0x%x\n", cmd);
		printf("spec_dc_ioctl_hndl: arg/0x%x\n", arg);
		printf("spec_dc_ioctl_hndl: mode/0x%x\n", mode);
		printf("spec_dc_ioctl_hndl: credp/0x%x\n", credp);
		printf("spec_dc_ioctl_hndl.r: rvalp/0x%x = %d\n",
								rvalp, *rvalp);
		printf("spec_dc_ioctl_hndl.r: vbd/0x%x\n", vbd);
		printf("spec_dc_ioctl_hndl.r: error/%d\n", error);
	}
#endif	/* JTK_DEBUG_LVL_2 */

	return error;
}


/*
 * called through the spec_ls2dc_ops vector.
 */
/* ARGSUSED */
int
spec_dc_strgetmsg_hndl(
	spec_handle_t	*handle,
	struct strbuf	*mctl,
	struct strbuf	*mdata,
	unsigned char	*prip,
	int		*flagsp,
	int		fmode,
	union rval	*rvp)
{
	int		error;

	SPEC_STATS(spec_dc_strgetmsg_hndl);

	error = invk_spec_ds_strgetmsg(SPEC_HANDLE_TO_SERVICE(*handle), handle,
					mctl, mdata, prip, flagsp, fmode, rvp);
	ASSERT(error != ECELLDOWN);
#ifdef	JTK_DEBUG
/*	if (error) {	*/
		printf("spec_dc_strgetmsg_hndl: handle/0x%x = %d/%d/0x%x\n",
					handle,
					handle->sh_obj.h_service.s_cell,
					handle->sh_obj.h_service.s_svcnum,
					handle->sh_obj.h_objid);
		printf("spec_dc_strgetmsg_hndl: mctl/0x%x\n", mctl);
		printf("spec_dc_strgetmsg_hndl: mdata/0x%x\n", mdata);
		printf("spec_dc_strgetmsg_hndl: prip/0x%x\n", prip);
		printf("spec_dc_strgetmsg_hndl: flagsp/0x%x\n", flagsp);
		printf("spec_dc_strgetmsg_hndl: fmode/0x%x\n", fmode);
		printf("spec_dc_strgetmsg_hndl: rvp/0x%x\n", rvp);
		printf("spec_dc_strgetmsg_hndl.r: error/%d\n", error);
/*	}	*/
#endif	/* JTK_DEBUG */

	return error;
}


/*
 * called through the spec_ls2dc_ops vector.
 */
/* ARGSUSED */
int
spec_dc_strputmsg_hndl(
	spec_handle_t	*handle,
	struct strbuf	*mctl,
	struct strbuf	*mdata,
	unsigned char	pri,
	int		flag,
	int		fmode)
{
	int		error;

	SPEC_STATS(spec_dc_strputmsg_hndl);

	error = invk_spec_ds_strputmsg(SPEC_HANDLE_TO_SERVICE(*handle), handle,
					mctl, mdata, pri, flag, fmode);
	ASSERT(error != ECELLDOWN);
#ifdef	JTK_DEBUG
/*	if (error) {	*/
		printf("spec_dc_strputmsg_hndl: handle/0x%x = %d/%d/0x%x\n",
					handle,
					handle->sh_obj.h_service.s_cell,
					handle->sh_obj.h_service.s_svcnum,
					handle->sh_obj.h_objid);
		printf("spec_dc_strputmsg_hndl: mctl/0x%x\n", mctl);
		printf("spec_dc_strputmsg_hndl: mdata/0x%x\n", mdata);
		printf("spec_dc_strputmsg_hndl: pri/0x%x\n", pri);
		printf("spec_dc_strputmsg_hndl: flag/0x%x\n", flag);
		printf("spec_dc_strputmsg_hndl: fmode/0x%x\n", fmode);
		printf("spec_dc_strputmsg_hndl.r: error/%d\n", error);
/*	}	*/
#endif	/* JTK_DEBUG */

	return error;
}



/*
 * called through the spec_ls2dc_ops vector.
 */
/* ARGSUSED */
void
spec_dc_strategy_subr(
	spec_handle_t	*handle,		/* in */
	struct buf	*bp)			/* in */
{
	int			i;
	/* REFERENCED */
	int			error;
	int			entries;
	int			exlist_len;
	/* REFERENCED */
	int			msgerr;
	alenaddr_t		tmpaddr;
	external_alenlist_t	exlist, nexlist;
	service_t		svc;
	spec_handle_t		bp_handle;

	SPEC_STATS(spec_dc_strategy_subr);

#ifdef	JTK_DEBUG
	printf("spec_dc_strategy_subr:[%d] handle/0x%x = %d/%d/0x%x\n",
					cellid(), handle,
					handle->sh_obj.h_service.s_cell,
					handle->sh_obj.h_service.s_svcnum,
					handle->sh_obj.h_objid);
	printf("spec_dc_strategy_subr:[%d] bp/0x%x\n", cellid(), bp);
#endif	/* JTK_DEBUG */

	SERVICE_MAKE(svc, cellid(), SVC_SPECFS);	/* My return address */

	SPEC_HANDLE_MAKE(bp_handle, svc, bp, 0, VNON);

	ASSERT(bp->b_alenlist == NULL);

	bp->b_alenlist = buf_to_alenlist(NULL, bp, 0);

	ASSERT(bp->b_alenlist);

	entries = alenlist_size(bp->b_alenlist);

	ASSERT(entries > 0);

#ifdef	JTK_DEBUG
	if (bp->b_flags & B_PAGEIO) {
		printf("spec_dc_strategy_subr:[%d] B_PAGEIO bp->b_page/0x%x\n",
							cellid(), bp->b_page);
	} else if (bp->b_flags & B_MAPPED) {
		printf(
		    "spec_dc_strategy_subr:[%d] B_MAPPED bp->b_dmaaddr/0x%x\n",
							cellid(),
							bp->b_dmaaddr);
	} else {
		printf("spec_dc_strategy_subr:[%d] bp->b_un.b_addr/0x%x\n",
							cellid(),
							bp->b_un.b_addr);
	}

	printf("spec_dc_strategy_subr:[%d] alenlist/0x%x entries/%d\n",
							cellid(),
							bp->b_alenlist, entries);
#endif	/* JTK_DEBUG */

	exlist_len = entries * sizeof(struct external_alenlist);

	nexlist = exlist = (external_alenlist_t)kmem_alloc(exlist_len, KM_SLEEP);

	error = alenlist_cursor_init(bp->b_alenlist, 0, NULL);

	ASSERT(error == ALENLIST_SUCCESS);

	i = 0;

	while (alenlist_get(bp->b_alenlist, NULL, 0,
						&tmpaddr, &nexlist->len, 0)
							== ALENLIST_SUCCESS) {
		nexlist->addr = PHYS_TO_K0(tmpaddr);

#ifdef	JTK_DEBUG
		printf("spec_dc_strategy_subr:[%d]   %2d - 0x%x - %d\n",
			cellid(), i, nexlist->addr, nexlist->len);
#endif	/* JTK_DEBUG */

		nexlist++;
		i++;
	}

	ASSERT(i == entries);

	msgerr = invk_spec_ds_strategy(SPEC_HANDLE_TO_SERVICE(*handle),
							handle, &bp_handle,
							bp, exlist, entries);
	ASSERT(!msgerr);

	kmem_free(exlist, exlist_len);

	/*
	 * Keep our "reference" to bp->b_alenlist,
	 * it'll be dropped later in I_spec_dc_strategy().
	 */
}


/* ARGSUSED */
void
I_spec_dc_strategy(
	spec_handle_t		*handle,		/* in */
	spec_handle_t		*bp_handle,		/* in */
	struct buf		*nbp)			/* in */
{
	int		i;
	struct buf	*bp;


	SPEC_STATS(I_spec_dc_strategy);

#ifdef	JTK_DEBUG
	printf("I_spec_dc_strategy:[%d] handle/0x%x = %d/%d/0x%x\n",
					cellid(),
					handle,
					handle->sh_obj.h_service.s_cell,
					handle->sh_obj.h_service.s_svcnum,
					handle->sh_obj.h_objid);
	printf("I_spec_dc_strategy:[%d] bp_hndl/0x%x\n",
					cellid(), bp_handle->sh_obj.h_objid);
	printf("I_spec_dc_strategy:[%d] nbp/0x%x\n", cellid(), nbp);
	printf("I_spec_dc_strategy:[%d] dev/0x%x(%d/%d, %d)\n",
					cellid(),
					nbp->b_edev, major(nbp->b_edev),
					emajor(nbp->b_edev), minor(nbp->b_edev));
#endif	/* JTK_DEBUG */

	bp = (struct buf *)bp_handle->sh_obj.h_objid;

	ASSERT(nbp->b_edev    == bp->b_edev);
	ASSERT(nbp->b_offset  == bp->b_offset);
	ASSERT(nbp->b_bcount  == bp->b_bcount);
	ASSERT(nbp->b_bufsize == bp->b_bufsize);
	ASSERT(nbp->b_blkno   == bp->b_blkno);

#ifdef	JTK_DEBUG
	printf("I_spec_dc_strategy:[%d] alist/0x%x\n", cellid(), bp->b_alenlist);
	printf("I_spec_dc_strategy:[%d] alist_len/%d\n",
					cellid(), alenlist_size(bp->b_alenlist));

	{
		int		error;
		alenaddr_t	tmpaddr;
		size_t		tmplen;


		error = alenlist_cursor_init(bp->b_alenlist, 0, NULL);

		ASSERT(error == ALENLIST_SUCCESS);

		i = 0;

		while (alenlist_get(bp->b_alenlist, NULL, 0,
						&tmpaddr, &tmplen, 0)
							== ALENLIST_SUCCESS) {

			printf("I_spec_dc_strategy:[%d]   %2d - 0x%x - %d\n",
						cellid(),
						i, PHYS_TO_K0(tmpaddr), tmplen);
			i++;
		}
	}
#endif	/* JTK_DEBUG */

	/*
	 * Selectively update some fields in the "original" bp.
	 */
	bp->b_flags    |= (nbp->b_flags & B_ERROR);
	bp->b_error	= nbp->b_error;
	bp->b_resid	= nbp->b_resid;
	bp->b_remain	= nbp->b_remain;

	alenlist_destroy(bp->b_alenlist);

	bp->b_alenlist = NULL;

	iodone(bp);			/* Turn it loose	*/
}


/*
 * called through the spec_ls2dc_ops vector.
 */
/* ARGSUSED */
int
spec_dc_addmap_subr(
	spec_handle_t	*handle,		/* in */
	vaddmap_t	op,			/* in */
	vhandl_t	*vt,			/* in */
	off_t		off,			/* in */
	size_t		len,			/* in */
	mprot_t		prot,			/* in */
	struct cred	*credp,			/* in */
	pgno_t		*pgno)			/* out */
{
	int		error;


	SPEC_STATS(spec_dc_addmap_subr);


#ifdef	JTK_DEBUG
	printf("spec_dc_addmap_subr: handle/0x%x = %d/%d/0x%x\n",
					handle,
					handle->sh_obj.h_service.s_cell,
					handle->sh_obj.h_service.s_svcnum,
					handle->sh_obj.h_objid);
	printf("spec_dc_addmap_subr: op/0x%x\n", op);
	printf("spec_dc_addmap_subr: vt/0x%x\n", vt);
	printf("spec_dc_addmap_subr: off/%d\n", off);
	printf("spec_dc_addmap_subr: len/%d\n", len);
	printf("spec_dc_addmap_subr: prot/0x%x\n", prot);
	printf("spec_dc_addmap_subr: credp/0x%x\n", credp);
#endif	/* JTK_DEBUG */

	error = invk_spec_ds_addmap(SPEC_HANDLE_TO_SERVICE(*handle), handle,
					op, vt, off, len, prot,
					CRED_GETID(credp), pgno);
	ASSERT(error != ECELLDOWN);

#ifdef	JTK_DEBUG
	printf("spec_dc_addmap_subr.r: pgno/0x%x = 0x%x\n", pgno, *pgno);
	printf("spec_dc_addmap_subr.r: error/%d\n", error);
#endif	/* JTK_DEBUG */

	return error;
}


/*
 * called through the spec_ls2dc_ops vector.
 */
/* ARGSUSED */
int
spec_dc_delmap_subr(
	spec_handle_t	*handle,		/* in */
	vhandl_t	*vt,			/* in */
	size_t		len,			/* in */
	struct cred	*credp)			/* in */
{
	int		error;


	SPEC_STATS(spec_dc_delmap_subr);


#ifdef	JTK_DEBUG
	printf("spec_dc_delmap_subr: handle/0x%x = %d/%d/0x%x\n",
					handle,
					handle->sh_obj.h_service.s_cell,
					handle->sh_obj.h_service.s_svcnum,
					handle->sh_obj.h_objid);
	printf("spec_dc_delmap_subr: vt/0x%x\n", vt);
	printf("spec_dc_delmap_subr: len/%d\n", len);
	printf("spec_dc_delmap_subr: credp/0x%x\n", credp);
#endif	/* JTK_DEBUG */

	error = invk_spec_ds_delmap(SPEC_HANDLE_TO_SERVICE(*handle),
							handle, vt, len,
							CRED_GETID(credp));
	ASSERT(error != ECELLDOWN);

#ifdef	JTK_DEBUG
	printf("spec_dc_delmap_subr.r: error/%d\n", error);
#endif	/* JTK_DEBUG */

	return error;
}


/*
 * called through the spec_ls2dc_ops vector.
 */
/* ARGSUSED */
int
spec_dc_close_hndl(
	spec_handle_t	*handle,		/* in */
	int		flag,			/* in */
	lastclose_t	lastclose,		/* in */
	struct cred	*credp)			/* in */
{
	int		error;


	SPEC_STATS(spec_dc_close_hndl);

	error = invk_spec_ds_close(SPEC_HANDLE_TO_SERVICE(*handle),
						handle, flag, lastclose,
						CRED_GETID(credp));
	ASSERT(error != ECELLDOWN);

#ifdef	JTK_DEBUG
	if (error) {
		printf("spec_dc_close_hndl: handle/0x%x = %d/%d/0x%x\n",
					handle,
					handle->sh_obj.h_service.s_cell,
					handle->sh_obj.h_service.s_svcnum,
					handle->sh_obj.h_objid);
		printf("spec_dc_close_hndl: flag/0x%x\n", flag);
		printf("spec_dc_close_hndl: lastclose/%d\n", lastclose);
		printf("spec_dc_close_hndl: credp/0x%x\n", credp);
		printf("spec_dc_close_hndl.r: error/%d\n", error);
	}
#endif	/* JTK_DEBUG */

	return error;
}


/*
 * called through the spec_ls2dc_ops vector.
 */
STATIC void
spec_dc_teardown_subr(
	spec_handle_t	*handle)		/* in */
{
	/* REFERENCED */
	int	msgerr;

	SPEC_STATS(spec_dc_teardown_subr);

	msgerr = invk_spec_ds_teardown(SPEC_HANDLE_TO_SERVICE(*handle), handle);

	ASSERT(!msgerr);
}




spec_ls_ops_t spec_ls2dc_ops = {
	spec_dc_attach,			/* spec_ops_cs_attach		*/
	spec_dc_cmp_gen,		/* spec_ops_cs_cmp_gen		*/
	spec_dc_get_gen,		/* spec_ops_cs_get_gen		*/
	spec_dc_get_opencnt,		/* spec_ops_cs_get_opencnt	*/
	spec_dc_get_size,		/* spec_ops_cs_get_size		*/
	spec_dc_ismounted,		/* spec_ops_cs_ismounted	*/
	spec_dc_mountedflag,		/* spec_ops_cs_mountedflag	*/
	spec_dc_clone,			/* spec_ops_cs_clone		*/
	spec_dc_bmap_subr,		/* spec_ops_bmap		*/
	spec_dc_open_hndl,		/* spec_ops_open		*/
	spec_dc_read_hndl,		/* spec_ops_read		*/
	spec_dc_write_hndl,		/* spec_ops_write		*/
	spec_dc_ioctl_hndl,		/* spec_ops_ioctl		*/
	spec_dc_strgetmsg_hndl,		/* spec_ops_strgetmsg		*/
	spec_dc_strputmsg_hndl,		/* spec_ops_strputmsg		*/
	spec_dc_poll_hndl,		/* spec_ops_poll		*/
	spec_dc_strategy_subr,		/* spec_ops_strategy		*/
	spec_dc_addmap_subr,		/* spec_ops_addmap		*/
	spec_dc_delmap_subr,		/* spec_ops_delmap		*/
	spec_dc_close_hndl,		/* spec_ops_close		*/
	spec_dc_teardown_subr,		/* spec_ops_teardown		*/
};
