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

#ident	"$Revision: 1.8 $"

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
#include <sys/alenlist.h>
#include <ksys/cred.h>
#include <fs/cfs/cfs.h>
#include <fs/specfs/spec_csnode.h>
#include "invk_spec_dc_stubs.h"
#include "I_spec_ds_stubs.h"



extern void
spec_cs_attach(
	dev_t		dev,
	vtype_t		type,
	spec_handle_t	*handle,
	spec_handle_t	*cvp_handle,
	long		*gen,
	long		*size,
	struct stdata	**stream);

extern void
spec_cs_clone(
	spec_handle_t	*handle,
	struct stdata	*stp,
	spec_handle_t	*ovp_handle,
	int		flag,
	struct stdata	**stream);

extern int
spec_cs_open_hndl(
	spec_handle_t	*handle,
	long		gen,
	dev_t		*newdev,
	long		*cs_size,
	struct stdata	**stp,
	mode_t		flag,
	struct cred	*credp);

extern int
spec_cs_read_hndl(
	spec_handle_t	*handle,
	long		gen,
	vnode_t		*vp,
	struct uio	*uiop,
	int		ioflag,
	struct cred	*credp,
	struct flid	*fl);

extern int
spec_cs_write_hndl(
	spec_handle_t	*handle,
	long		gen,
	vnode_t		*vp,
	struct uio	*uiop,
	int		ioflag,
	struct cred	*credp,
	struct flid	*fl);

extern int
spec_cs_ioctl_hndl(
	spec_handle_t	*handle,
	long		gen,
	int		cmd,
	void		*arg,
	int		mode,
	struct cred	*credp,
	int		*rvalp,
	struct vopbd	*vbd);

extern int
spec_cs_strgetmsg_hndl(
	spec_handle_t	*handle,
	struct strbuf	*mctl,
	struct strbuf	*mdata,
	unsigned char	*prip,
	int		*flagsp,
	int		fmode,
	union rval	*rvp);

extern int
spec_cs_strputmsg_hndl(
	spec_handle_t	*handle,
	struct strbuf	*mctl,
	struct strbuf	*mdata,
	unsigned char	pri,
	int		flag,
	int		fmode);

extern int
spec_cs_close_hndl(
	spec_handle_t	*handle,
	int		flag,
	lastclose_t	lastclose,
	struct cred	*credp);

extern int
spec_cs_cmp_gen(
	spec_handle_t	*handle,
	long		lsp_gen);

extern int
spec_cs_get_gen(
	spec_handle_t	*handle);

extern int
spec_cs_get_opencnt(
	spec_handle_t	*handle);

extern daddr_t
spec_cs_get_size(
	spec_handle_t	*handle);

extern int
spec_cs_ismounted(
	spec_handle_t	*handle);

extern void
spec_cs_mountedflag(
	spec_handle_t	*handle,
	int		set);

extern void
spec_cs_teardown_subr(
	spec_handle_t	*handle);

extern void
spec_cs_strategy_subr(
	spec_handle_t	*handle,
	struct buf	*bp);

extern int
spec_cs_poll_hndl(
	spec_handle_t	*handle,
	long		gen,
	short		events,
	int		anyyet,
	short		*reventsp,
	struct pollhead	**phpp,
	unsigned int	*genp);

extern int
spec_cs_bmap_subr(
	spec_handle_t	*handle,
	off_t		offset,
	ssize_t		count,
	int		rw,
	struct cred	*credp,
	struct bmapval	*bmap,
	int		*nbmap);

extern int
spec_cs_addmap_subr(
	spec_handle_t	*handle,
	vaddmap_t	op,
	vhandl_t	*vt,
	off_t		off,
	size_t		len,
	mprot_t		prot,
	struct cred	*credp,
	pgno_t		*pgno);

extern int
spec_cs_delmap_subr(
	spec_handle_t	*handle,
	vhandl_t	*vt,
	size_t		len,
	struct cred	*credp);




/* ARGSUSED */
void
I_spec_ds_attach(
	dev_t		dev,			/* in */
	vtype_t		type,			/* in */
	spec_handle_t	*handle,		/* out */
	spec_handle_t	*cvp_handle,		/* out */
	long		*gen,			/* out */
	long		*size,			/* out */
	spec_handle_t	*stream)		/* out */
{
	struct stdata	*local_stream;


	SPEC_STATS(I_spec_ds_attach);

#ifdef	JTK_DEBUG
	printf("I_spec_ds_attach[%d]: dev/0x%x(%d/%d, %d) type/%d\n",
				cellid(), dev,
				major(dev), emajor(dev), minor(dev), type);
#endif	/* JTK_DEBUG */

	local_stream   = NULL;

	spec_cs_attach(dev, type, handle, cvp_handle,
					gen, size, &local_stream);

#ifdef	JTK_DEBUG_LVL_2
	printf("I_spec_ds_attach.r: handle/0x%x = %d/%d/0x%x\n",
					handle,
					handle->sh_obj.h_service.s_cell,
					handle->sh_obj.h_service.s_svcnum,
					handle->sh_obj.h_objid);
	printf("I_spec_ds_attach.r: cvp_handle/0x%x = 0x%x\n",
					cvp_handle,
					cvp_handle->sh_obj.h_objid);
	printf("I_spec_ds_attach.r: gen/0x%x = %d\n", gen, *gen);
	printf("I_spec_ds_attach.r: size/0x%x = %d\n", size, *size);
	printf("I_spec_ds_attach.r: stream/0x%x = 0x%x\n",
						stream, local_stream);
#endif	/* JTK_DEBUG_LVL_2 */

	stream->sh_obj.h_objid = (void *)local_stream;
}


/* ARGSUSED */
void
I_spec_ds_clone(
	spec_handle_t	*handle,		/* in */
	spec_handle_t	*stp,			/* in */
	spec_handle_t	*ovp_handle,		/* in */
	int		flag,			/* in */
	spec_handle_t	*stream)		/* out */
{
	struct stdata	*local_stream;
	struct stdata	*local_stp;


	SPEC_STATS(I_spec_ds_clone);

	local_stp = (struct stdata *)stp->sh_obj.h_objid;

#ifdef	JTK_DEBUG_LVL_2
	printf("I_spec_ds_clone: handle/0x%x = %d/%d/0x%x\n",
					handle,
					handle->sh_obj.h_service.s_cell,
					handle->sh_obj.h_service.s_svcnum,
					handle->sh_obj.h_objid);
	printf("I_spec_ds_clone: stp/0x%x=0x%x\n", stp, local_stp);
	printf("I_spec_ds_clone: ovp_handle/0x%x=0x%x\n",
						ovp_handle,
						ovp_handle->sh_obj.h_objid);
	printf("I_spec_ds_clone: flag/0x%x\n", flag);
#endif	/* JTK_DEBUG_LVL_2 */

	local_stream = NULL;

	spec_cs_clone(handle, local_stp, ovp_handle, flag, &local_stream);

#ifdef	JTK_DEBUG_LVL_2
	printf("I_spec_ds_clone.r: stream/0x%x = 0x%x\n",
						stream, local_stream);
#endif	/* JTK_DEBUG_LVL_2 */

	stream->sh_obj.h_objid = (void *)local_stream;
}


/* ARGSUSED */
int
I_spec_ds_open(
	spec_handle_t	*handle,		/* in */
	long		gen,			/* in */
	mode_t		flag,			/* in */
	credid_t	credid,			/* in */
	dev_t		*newdev,		/* in/out */
	long		*cs_size,		/* in/out */
	spec_handle_t	*stp)			/* out */
{
	int		error;
#ifdef	JTK_DEBUG_LVL_2
	dev_t		in_newdev;
#endif	/* JTK_DEBUG_LVL_2 */
	struct cred	*credp;
	struct stdata	*local_stp;


	SPEC_STATS(I_spec_ds_open);

	credp = CREDID_GETCRED(credid);

	local_stp = NULL;

#ifdef	JTK_DEBUG_LVL_2
	in_newdev = *newdev;
#endif	/* JTK_DEBUG_LVL_2 */

	error = spec_cs_open_hndl(handle, gen, newdev, cs_size,
						&local_stp, flag, credp);

#ifdef	JTK_DEBUG_LVL_2
	if (error || (in_newdev != *newdev)) {
		printf("I_spec_ds_open: handle/0x%x = %d/%d/0x%x\n",
					handle,
					handle->sh_obj.h_service.s_cell,
					handle->sh_obj.h_service.s_svcnum,
					handle->sh_obj.h_objid);
		printf("I_spec_ds_open: gen/%d\n", gen);
		printf("I_spec_ds_open: flag/0x%x\n", flag);
		printf("I_spec_ds_open: credp/0x%x\n", credp);
		printf("I_spec_ds_open: in_newdev(%d/%d, %d)\n",
					major(in_newdev),
					emajor(in_newdev), minor(in_newdev));
		printf("I_spec_ds_open.r: newdev/0x%x (%d/%d, %d)\n",
					newdev, major(*newdev),
					emajor(*newdev), minor(*newdev));
		printf("I_spec_ds_open.r: cs_size/0x%x = %d\n",
							cs_size, *cs_size);
		printf("I_spec_ds_open.r: stp/0x%x = 0x%x\n",
						stp, local_stp);
		printf("I_spec_ds_open.r: error/%d\n", error);
	}
#endif	/* JTK_DEBUG_LVL_2 */

	stp->sh_obj.h_objid = (void *)local_stp;

	return error;
}


/* ARGSUSED */
int
I_spec_ds_cmp_gen(
	spec_handle_t	*handle,		/* in */
	long		lsp_gen)		/* in */
{
	int		result;


	SPEC_STATS(I_spec_ds_cmp_gen);

	result = spec_cs_cmp_gen(handle, lsp_gen);

#ifdef	JTK_DEBUG
	if (result == 0) {
		printf("I_spec_ds_cmp_gen: handle/0x%x = %d/%d/0x%x\n",
					handle,
					handle->sh_obj.h_service.s_cell,
					handle->sh_obj.h_service.s_svcnum,
					handle->sh_obj.h_objid);
		printf("I_spec_ds_cmp_gen: lsp_gen/%d\n", lsp_gen);
		printf("I_spec_ds_cmp_gen.r: result/%d\n", result);
	}
#endif	/* JTK_DEBUG */

	return result;
}


/* ARGSUSED */
int
I_spec_ds_read(
	spec_handle_t	*handle,		/* in */
	long		gen,			/* in */
	struct uio	*uiop,			/* in */
	iovec_t		*iovp,			/* in */
	size_t		iov_len,		/* in */
	int		ioflag,			/* in */
	credid_t	credid,			/* in */
	struct flid	*fl,			/* in */
	off_t		*uio_offset,		/* out */
	short		*uio_sigpipe,		/* out */
	ssize_t		*uio_resid)		/* out */
{
	int		error;
	struct cred	*credp;


	SPEC_STATS(I_spec_ds_read);

	credp = CREDID_GETCRED(credid);

	uiop->uio_iov = iovp;

	error = spec_cs_read_hndl(handle, gen, NULL, uiop, ioflag, credp, fl);

#ifdef	JTK_DEBUG
	if (error) {
		printf("I_spec_ds_read: handle/0x%x = %d/%d/0x%x\n",
					handle,
					handle->sh_obj.h_service.s_cell,
					handle->sh_obj.h_service.s_svcnum,
					handle->sh_obj.h_objid);
		printf("I_spec_ds_read: gen/%d\n", gen);
		printf("I_spec_ds_read: uiop/0x%x\n", uiop);
		printf("I_spec_ds_read: ioflag/0x%x\n", ioflag);
		printf("I_spec_ds_read: credp/0x%x\n", credp);
		printf("I_spec_ds_read: fl/0x%x\n", fl);
		printf("I_spec_ds_read.r: uio_offset/0x%x = %d\n",
						uio_offset, uiop->uio_offset);
		printf("I_spec_ds_read.r: uio_resid/0x%x = %d\n",
						uio_resid, uiop->uio_resid);
		printf("I_spec_ds_read.r: uio_sigpipe/0x%x = 0x%x\n",
						uio_sigpipe, uiop->uio_sigpipe);
		printf("I_spec_ds_read.r: error/%d\n", error);
	}
#endif	/* JTK_DEBUG */

	*uio_offset  = uiop->uio_offset;
	*uio_sigpipe = uiop->uio_sigpipe;
	*uio_resid   = uiop->uio_resid;

	return error;
}


/* ARGSUSED */
int
I_spec_ds_list_read(
	spec_handle_t	*handle,		/* in */
	long		gen,			/* in */
	uio_t		*uiop,			/* in */
	iovec_t		*iovp,			/* in */
	size_t		iov_len,		/* in */
	external_alenlist_t alist,		/* in */
	size_t		alist_len,		/* in */
	int		ioflag,			/* in */
	credid_t	credid,			/* in */
	flid_t		*fl,			/* in */
	off_t		*uio_offset,		/* out */
	short		*uio_sigpipe,		/* out */
	ssize_t		*uio_resid)		/* out */
{
	extern void plist_to_uio(external_alenlist_t, uio_t*, struct iovec *);
	extern void kvaddr_free(int, struct iovec *);

	int		error;
	struct cred	*credp;
	struct iovec	saved_iov[16];


	SPEC_STATS(I_spec_ds_list_read);

	credp = CREDID_GETCRED(credid);

	uiop->uio_iov = iovp;

	plist_to_uio(alist, uiop, saved_iov);

	uiop->uio_segflg = UIO_SYSSPACE;

	error = spec_cs_read_hndl(handle, gen, NULL, uiop, ioflag, credp, fl);

#ifdef	JTK_DEBUG
	if (error) {
		printf("I_spec_ds_list_read: handle/0x%x = %d/%d/0x%x\n",
					handle,
					handle->sh_obj.h_service.s_cell,
					handle->sh_obj.h_service.s_svcnum,
					handle->sh_obj.h_objid);
		printf("I_spec_ds_list_read: gen/%d\n", gen);
		printf("I_spec_ds_list_read: uiop/0x%x\n", uiop);
		printf("I_spec_ds_list_read: ioflag/0x%x\n", ioflag);
		printf("I_spec_ds_list_read: credp/0x%x\n", credp);
		printf("I_spec_ds_list_read: fl/0x%x\n", fl);
		printf("I_spec_ds_list_read.r: uio_offset/0x%x = %d\n",
						uio_offset, uiop->uio_offset);
		printf("I_spec_ds_list_read.r: uio_resid/0x%x = %d\n",
						uio_resid, uiop->uio_resid);
		printf("I_spec_ds_list_read.r: uio_sigpipe/0x%x = 0x%x\n",
						uio_sigpipe, uiop->uio_sigpipe);
		printf("I_spec_ds_list_read.r: error/%d\n", error);
	}
#endif	/* JTK_DEBUG */

	kvaddr_free(uiop->uio_iovcnt, saved_iov);

	*uio_offset  = uiop->uio_offset;
	*uio_sigpipe = uiop->uio_sigpipe;
	*uio_resid   = uiop->uio_resid;

	return error;
}


/* ARGSUSED */
int
I_spec_ds_write(
	spec_handle_t	*handle,		/* in */
	long		gen,			/* in */
	struct uio	*uiop,			/* in */
	iovec_t		*iovp,			/* in */
	size_t		iov_len,		/* in */
	int		ioflag,			/* in */
	credid_t	credid,			/* in */
	struct flid	*fl,			/* in */
	off_t		*uio_offset,		/* out */
	short		*uio_sigpipe,		/* out */
	ssize_t		*uio_resid)		/* out */
{
	int		error;
	struct cred	*credp;


	SPEC_STATS(I_spec_ds_write);

	credp = CREDID_GETCRED(credid);

	uiop->uio_iov = iovp;

	error = spec_cs_write_hndl(handle, gen, NULL, uiop, ioflag, credp, fl);

#ifdef	JTK_DEBUG
	if (error) {
		printf("I_spec_ds_write: handle/0x%x = %d/%d/0x%x\n",
					handle,
					handle->sh_obj.h_service.s_cell,
					handle->sh_obj.h_service.s_svcnum,
					handle->sh_obj.h_objid);
		printf("I_spec_ds_write: gen/%d\n", gen);
		printf("I_spec_ds_write: uiop/0x%x\n", uiop);
		printf("I_spec_ds_write: ioflag/0x%x\n", ioflag);
		printf("I_spec_ds_write: credp/0x%x\n", credp);
		printf("I_spec_ds_write: fl/0x%x\n", fl);
		printf("I_spec_ds_write.r: uio_offset/0x%x = %d\n",
						uio_offset, uiop->uio_offset);
		printf("I_spec_ds_write.r: uio_resid/0x%x = %d\n",
						uio_resid, uiop->uio_resid);
		printf("I_spec_ds_write.r: uio_sigpipe/0x%x = 0x%x\n",
						uio_sigpipe, uiop->uio_sigpipe);
		printf("I_spec_ds_write.r: error/%d\n", error);
	}
#endif	/* JTK_DEBUG */

	*uio_offset  = uiop->uio_offset;
	*uio_sigpipe = uiop->uio_sigpipe;
	*uio_resid   = uiop->uio_resid;

	return error;
}


/* ARGSUSED */
int
I_spec_ds_list_write(
	spec_handle_t	*handle,		/* in */
	long		gen,			/* in */
	struct uio	*uiop,			/* in */
	iovec_t		*iovp,			/* in */
	size_t		iov_len,		/* in */
	external_alenlist_t alist,		/* in */
	size_t		alist_len,		/* in */
	int		ioflag,			/* in */
	credid_t	credid,			/* in */
	struct flid	*fl,			/* in */
	off_t		*uio_offset,		/* out */
	short		*uio_sigpipe,		/* out */
	ssize_t		*uio_resid)		/* out */
{
	extern void plist_to_uio(external_alenlist_t, uio_t*, struct iovec *);
	extern void kvaddr_free(int, struct iovec *);

	int		error;
	struct cred	*credp;
	struct iovec	saved_iov[16];


	SPEC_STATS(I_spec_ds_write);

	credp = CREDID_GETCRED(credid);

	uiop->uio_iov = iovp;

	plist_to_uio(alist, uiop, saved_iov);

	uiop->uio_segflg = UIO_SYSSPACE;

	error = spec_cs_write_hndl(handle, gen, NULL, uiop, ioflag, credp, fl);

#ifdef	JTK_DEBUG
	if (error) {
		printf("I_spec_ds_list_write: handle/0x%x = %d/%d/0x%x\n",
					handle,
					handle->sh_obj.h_service.s_cell,
					handle->sh_obj.h_service.s_svcnum,
					handle->sh_obj.h_objid);
		printf("I_spec_ds_list_write: gen/%d\n", gen);
		printf("I_spec_ds_list_write: uiop/0x%x\n", uiop);
		printf("I_spec_ds_list_write: ioflag/0x%x\n", ioflag);
		printf("I_spec_ds_list_write: credp/0x%x\n", credp);
		printf("I_spec_ds_list_write: fl/0x%x\n", fl);
		printf("I_spec_ds_list_write.r: uio_offset/0x%x = %d\n",
						uio_offset, uiop->uio_offset);
		printf("I_spec_ds_list_write.r: uio_resid/0x%x = %d\n",
						uio_resid, uiop->uio_resid);
		printf("I_spec_ds_list_write.r: uio_sigpipe/0x%x = 0x%x\n",
						uio_sigpipe, uiop->uio_sigpipe);
		printf("I_spec_ds_list_write.r: error/%d\n", error);
	}
#endif	/* JTK_DEBUG */

	kvaddr_free(uiop->uio_iovcnt, saved_iov);

	*uio_offset  = uiop->uio_offset;
	*uio_sigpipe = uiop->uio_sigpipe;
	*uio_resid   = uiop->uio_resid;

	return error;
}


/* ARGSUSED */
int
I_spec_ds_ioctl(
	spec_handle_t	*handle,		/* in */
	long		gen,			/* in */
	int		cmd,			/* in */
	void		*arg,			/* in */
	int		mode,			/* in */
	credid_t	credid,			/* in */
	int		*rvalp,			/* out */
	struct vopbd	*vbd)			/* out */
{
	int		error;
	struct cred	*credp;


	SPEC_STATS(I_spec_ds_ioctl);

	credp = CREDID_GETCRED(credid);

	vbd->vopbd_req = VOPBDR_NIL;

	error = spec_cs_ioctl_hndl(handle, gen,
					cmd, arg, mode, credp, rvalp, vbd);

#ifdef	JTK_DEBUG_LVL_2
	if (error) {
		printf("I_spec_ds_ioctl: handle/0x%x = %d/%d/0x%x\n",
					handle,
					handle->sh_obj.h_service.s_cell,
					handle->sh_obj.h_service.s_svcnum,
					handle->sh_obj.h_objid);
		printf("I_spec_ds_ioctl: gen/%d\n", gen);
		printf("I_spec_ds_ioctl: cmd/0x%x\n", cmd);
		printf("I_spec_ds_ioctl: arg/0x%x\n", arg);
		printf("I_spec_ds_ioctl: mode/0x%x\n", mode);
		printf("I_spec_ds_ioctl: credp/0x%x\n", credp);
		printf("I_spec_ds_ioctl.r: rvalp/0x%x = %d\n", rvalp, *rvalp);
		printf("I_spec_ds_ioctl.r: vbd/0x%x\n", vbd);
		printf("I_spec_ds_ioctl.r: error/%d\n", error);
	}
#endif	/* JTK_DEBUG_LVL_2 */

	return error;
}


int
I_spec_ds_strgetmsg(
        spec_handle_t 	*handle,
	struct strbuf	*mctl,
	struct strbuf	*mdata,
	unsigned char	*prip,
	int		*flagsp,
	int		fmode,
	union rval	*rvp)
{
	int	error;


	SPEC_STATS(I_spec_ds_strgetmsg);

	error = spec_cs_strgetmsg_hndl(handle,
					mctl, mdata, prip, flagsp, fmode, rvp);
#ifdef	JTK_DEBUG
/*	if (error) {	*/
		printf("I_spec_ds_strgetmsg: handle/0x%x = %d/%d/0x%x\n",
					handle,
					handle->sh_obj.h_service.s_cell,
					handle->sh_obj.h_service.s_svcnum,
					handle->sh_obj.h_objid);
		printf("I_spec_ds_strgetmsg: mctl/0x%x\n", mctl);
		printf("I_spec_ds_strgetmsg: mdata/0x%x\n", mdata);
		printf("I_spec_ds_strgetmsg: prip/0x%x\n", prip);
		printf("I_spec_ds_strgetmsg: flagsp/0x%x\n", flagsp);
		printf("I_spec_ds_strgetmsg: fmode/0x%x\n", fmode);
		printf("I_spec_ds_strgetmsg: rvp/0x%x\n", rvp);
		printf("I_spec_ds_strgetmsg.r: error/%d\n", error);
/*	}	*/
#endif	/* JTK_DEBUG */

	return error;
}


int
I_spec_ds_strputmsg(
        spec_handle_t 	*handle,
	struct strbuf	*mctl,
	struct strbuf	*mdata,
	unsigned char	pri,
	int		flag,
	int		fmode)
{
	int	error;


	SPEC_STATS(I_spec_ds_strputmsg);

	error = spec_cs_strputmsg_hndl(handle, mctl, mdata, pri, flag, fmode);

#ifdef	JTK_DEBUG
/*	if (error) {	*/
		printf("I_spec_ds_strputmsg: handle/0x%x = %d/%d/0x%x\n",
					handle,
					handle->sh_obj.h_service.s_cell,
					handle->sh_obj.h_service.s_svcnum,
					handle->sh_obj.h_objid);
		printf("I_spec_ds_strputmsg: mctl/0x%x\n", mctl);
		printf("I_spec_ds_strputmsg: mdata/0x%x\n", mdata);
		printf("I_spec_ds_strputmsg: pri/0x%x\n", pri);
		printf("I_spec_ds_strputmsg: flag/0x%x\n", flag);
		printf("I_spec_ds_strputmsg: fmode/0x%x\n", fmode);
		printf("I_spec_ds_strputmsg.r: error/%d\n", error);
/*	}	*/
#endif	/* JTK_DEBUG */

	return error;
}


/* ARGSUSED */
int
I_spec_ds_close(
	spec_handle_t	*handle,		/* in */
	int		flag,			/* in */
	lastclose_t	lastclose,		/* in */
	credid_t	credid)			/* in */
{
	int		error;
	struct cred	*credp;

	SPEC_STATS(I_spec_ds_close);

	credp = CREDID_GETCRED(credid);

	error = spec_cs_close_hndl(handle, flag, lastclose, credp);

#ifdef	JTK_DEBUG
	if (error) {
		printf("I_spec_ds_close: handle/0x%x = %d/%d/0x%x\n",
					handle,
					handle->sh_obj.h_service.s_cell,
					handle->sh_obj.h_service.s_svcnum,
					handle->sh_obj.h_objid);
		printf("I_spec_ds_close: flag/0x%x\n", flag);
		printf("I_spec_ds_close: lastclose/%d\n", lastclose);
		printf("I_spec_ds_close: credp/0x%x\n", credp);
		printf("I_spec_ds_close.r: error/%d\n", error);
	}
#endif	/* JTK_DEBUG*/

	return error;
}


/* ARGSUSED */
int
I_spec_ds_get_gen(
	spec_handle_t	*handle)		/* in */
{
	int		result;


	SPEC_STATS(I_spec_ds_get_gen);

	result = spec_cs_get_gen(handle);

	return result;
}


/* ARGSUSED */
int
I_spec_ds_get_opencnt(
	spec_handle_t	*handle)		/* in */
{
	int		result;


	SPEC_STATS(I_spec_ds_get_opencnt);

	result = spec_cs_get_opencnt(handle);

	return result;
}


/* ARGSUSED */
void
I_spec_ds_get_size(
	spec_handle_t	*handle,		/* in */
	daddr_t		*result)		/* out */
{
	SPEC_STATS(I_spec_ds_get_size);

#ifdef	JTK_DEBUG
	printf("I_spec_ds_get_size: handle/0x%x = %d/%d/0x%x\n",
					handle,
					handle->sh_obj.h_service.s_cell,
					handle->sh_obj.h_service.s_svcnum,
					handle->sh_obj.h_objid);
#endif	/* JTK_DEBUG */

	*result = spec_cs_get_size(handle);

#ifdef	JTK_DEBUG
	printf("I_spec_ds_get_size.r: result/0x%x=%d\n", result, *result);
#endif	/* JTK_DEBUG */
}


/* ARGSUSED */
int
I_spec_ds_ismounted(
	spec_handle_t	*handle)		/* in */
{
	int		result;


	SPEC_STATS(I_spec_ds_ismounted);

#ifdef	JTK_DEBUG
	printf("I_spec_ds_ismounted: handle/0x%x = %d/%d/0x%x\n",
					handle,
					handle->sh_obj.h_service.s_cell,
					handle->sh_obj.h_service.s_svcnum,
					handle->sh_obj.h_objid);
#endif	/* JTK_DEBUG */

	result = spec_cs_ismounted(handle);

#ifdef	JTK_DEBUG
	printf("I_spec_ds_ismounted.r: result/%d\n", result);
#endif	/* JTK_DEBUG */

	return result;
}


/* ARGSUSED */
void
I_spec_ds_mountedflag(
	spec_handle_t	*handle,		/* in */
	int		set)			/* in */
{
	SPEC_STATS(I_spec_ds_mountedflag);

#ifdef	JTK_DEBUG
	printf("I_spec_ds_mountedflag: handle/0x%x = %d/%d/0x%x\n",
					handle,
					handle->sh_obj.h_service.s_cell,
					handle->sh_obj.h_service.s_svcnum,
					handle->sh_obj.h_objid);
#endif	/* JTK_DEBUG */

	spec_cs_mountedflag(handle, set);
}


/* ARGSUSED */
void
I_spec_ds_teardown(
	spec_handle_t	*handle)		/* in */
{
	SPEC_STATS(I_spec_ds_teardown);

	spec_cs_teardown_subr(handle);
}


int
I_spec_ds_poll(
	spec_handle_t	*handle,		/* in */
	long		gen,			/* in */
	short		events,			/* in */
	int		anyyet,			/* in */
	short		*reventsp,		/* out */
	spec_handle_t	*phpp,			/* out */
	unsigned int	*genp)			/* out */
{
	int		error;
	struct pollhead	*local_phpp;

	SPEC_STATS(I_spec_ds_poll);

	local_phpp = (struct pollhead *)phpp->sh_obj.h_objid;

#ifdef	JTK_POLL_DEBUG
	printf("spec_dc_poll_hndl: handle/0x%x = %d/%d/0x%x\n",
					handle,
					handle->sh_obj.h_service.s_cell,
					handle->sh_obj.h_service.s_svcnum,
					handle->sh_obj.h_objid);
	printf("I_spec_ds_poll: gen/%d\n", gen);
	printf("I_spec_ds_poll: events/0x%x\n", events);
	printf("I_spec_ds_poll: anyyet/0x%x\n", anyyet);
#endif	/* JTK_POLL_DEBUG */

retry:

	error = spec_cs_poll_hndl(handle, gen,
				  events, anyyet, reventsp, &local_phpp, genp);

#ifdef	JTK_POLL_DEBUG
	printf("I_spec_ds_poll.r: reventsp/0x%x = 0x%x\n",
						reventsp, *reventsp);
	printf("I_spec_ds_poll.r: phpp/0x%x = 0x%x\n",
						phpp, local_phpp);
	printf("I_spec_ds_poll.r: genp/0x%x = %d\n", genp, *genp);
	printf("I_spec_ds_poll.r: error/%d\n", error);
#endif	/* JTK_POLL_DEBUG */

	/*
	 * In the distributed case, add this event to the distributed
	 * poll list on the server.
	 * Return a NULL phpp to the client, as "phpp" is a cell-local
	 * address.
	 */
	if (   error == 0
	    && (!anyyet && (*reventsp == 0) && local_phpp) ) {
		if (distributed_polladd(local_phpp, events, *genp) != 0)
			goto retry;
#ifdef	JTK_POLL_DEBUG
	printf("I_spec_ds_poll: phpp/0x%x added to server poll head\n",
								local_phpp);
#endif	/* JTK_POLL_DEBUG */
	}

	phpp->sh_obj.h_objid = NULL;
	
	return error;
}


int
I_spec_ds_bmap(
	spec_handle_t	*handle,		/* in */
	off_t		offset,			/* in */
	ssize_t		count,			/* in */
	int		rw,			/* in */
	credid_t	credid,			/* in */
	struct bmapval	*bmap,			/* out */
	int		*nbmap)			/* out */
{
	int		error;
	struct cred	*credp;


	SPEC_STATS(I_spec_ds_bmap);

	credp = CREDID_GETCRED(credid);

#ifdef	JTK_DEBUG
	printf("I_spec_ds_bmap: handle/0x%x = %d/%d/0x%x\n",
					handle,
					handle->sh_obj.h_service.s_cell,
					handle->sh_obj.h_service.s_svcnum,
					handle->sh_obj.h_objid);
	printf("I_spec_ds_bmap: offset/%d\n", offset);
	printf("I_spec_ds_bmap: count/%d\n", count);
	printf("I_spec_ds_bmap: rw/0x%x\n", rw);
	printf("I_spec_ds_bmap: credp/0x%x\n", credp);
#endif	/* JTK_DEBUG */

	error = spec_cs_bmap_subr(handle,
				  offset, count, rw, credp, bmap, nbmap);
#ifdef	JTK_DEBUG
	printf("I_spec_ds_bmap.r: bmap/0x%x = %0x%x\n", bmap, *bmap);
	printf("I_spec_ds_bmap.r: nbmap/0x%x = %d\n", nbmap, *nbmap);
	printf("I_spec_ds_bmap.r: error/%d\n", error);
#endif	/* JTK_DEBUG */

	return error;
}


/* ARGSUSED */
void
I_spec_ds_strategy(
	spec_handle_t		*handle,		/* in */
	spec_handle_t		*bp_handle,		/* in */
	struct buf		*bp,			/* in */
	external_alenlist_t	alist,			/* in */
	size_t			alist_len)		/* in */
{
	int			i;
	/* REFERENCED */
	int			msgerr;
	int			npgs;
	long			plen, soff;
	alenaddr_t		paddr;
	buf_t			*nbp;
	caddr_t			kvaddr;
	pde_t			*pde;
	pgi_t			pdebits;


	SPEC_STATS(I_spec_ds_strategy);

	nbp = getrbuf(KM_SLEEP);

	ASSERT(nbp);

#ifdef	JTK_DEBUG
	printf("I_spec_ds_strategy:[%d] handle/0x%x = %d/%d/0x%x\n",
					cellid(),
					handle,
					handle->sh_obj.h_service.s_cell,
					handle->sh_obj.h_service.s_svcnum,
					handle->sh_obj.h_objid);
	printf("I_spec_ds_strategy:[%d] bp_hndl/0x%x\n",
					cellid(), bp_handle->sh_obj.h_objid);
	printf("I_spec_ds_strategy:[%d] bp/0x%x\n", cellid(), bp);
	printf("I_spec_ds_strategy:[%d] nbp/0x%x\n", cellid(), nbp);
	printf("I_spec_ds_strategy:[%d] dev/0x%x(%d/%d, %d)\n",
					cellid(),
					bp->b_edev, major(bp->b_edev),
					emajor(bp->b_edev), minor(bp->b_edev));
	printf("I_spec_ds_strategy:[%d] alist/0x%x\n", cellid(), alist);
	printf("I_spec_ds_strategy:[%d] alist_len/%d\n", cellid(), alist_len);
#endif	/* JTK_DEBUG */

	nbp->b_flags	= bp->b_flags;
	nbp->b_edev	= bp->b_edev;
	nbp->b_error	= bp->b_error;
	nbp->b_offset	= bp->b_offset;
	nbp->b_bcount	= bp->b_bcount;
	nbp->b_resid	= bp->b_resid;
	nbp->b_remain	= bp->b_remain;
	nbp->b_bufsize	= bp->b_bufsize;
	nbp->b_blkno	= bp->b_blkno;

	ASSERT(nbp->b_flags & B_READ);		/* Paranoia runs deep!	*/

	npgs = 0;

	for (i = 0; i < alist_len; i++) {
#ifdef	JTK_DEBUG
		printf("I_spec_ds_strategy:[%d]   %2d 0x%x - %d\n",
				    cellid(), i, alist[i].addr, alist[i].len);
#endif	/* JTK_DEBUG */
		npgs += max(1, numpages(alist[i].addr, alist[i].len));
	}

	kvaddr = kvalloc(npgs, 0, 0);

	ASSERT(poff(kvaddr) == 0);

	pde = kvtokptbl(kvaddr);

	/*
	 * adjust for starting offset
 	*/
	soff = poff(alist->addr);

	if (soff) {
		kvaddr     += soff;
		alist->addr = pbase(alist->addr);
		alist->len += soff;
	}

#ifdef	JTK_DEBUG
	printf("I_spec_ds_strategy:[%d] npgs/%d soff/%d", cellid(), npgs, soff);
#endif	/* JTK_DEBUG */

	pdebits = PG_VR|PG_G|PG_M|PG_NDREF|PG_SV|pte_cachebits();

	i = npgs;

	while (i) {

		paddr = alist->addr;
		plen  = alist->len;

		while (plen > 0) {

			ASSERT(i);

			pde->pgi = mkpde(pdebits, pnum(paddr));

			pde++;
			i--;

			paddr += NBPP;
			plen  -= NBPP;
		}

		alist++;
	}

	ASSERT(i == 0);

	nbp->b_dmaaddr = kvaddr;
	nbp->b_flags  |= B_MAPPED;

#ifdef	JTK_DEBUG
	printf(" kvaddr/0x%x\n", kvaddr);
#endif	/* JTK_DEBUG */

	spec_cs_strategy_subr(handle, nbp);

#ifdef	JTK_DEBUG
	printf("I_spec_ds_strategy:[%d] 0x%x iowait'ing on nbp/0x%x\n",
							cellid(),
							curthreadp, nbp);
#endif	/* JTK_DEBUG */

	(void)iowait(nbp);		/* Wait for the driver to complete */

	/*
	 * Now we need to notify the thread waiting on the
	 * client cell that the I/O is done.
	 */
	msgerr = invk_spec_dc_strategy(SPEC_HANDLE_TO_SERVICE(*bp_handle),
							handle, bp_handle, nbp);
	ASSERT(!msgerr);

	kvfree(kvaddr, npgs);

	freerbuf(nbp);
}


/* ARGSUSED */
int
I_spec_ds_addmap(
	spec_handle_t	*handle,		/* in */
	vaddmap_t	op,			/* in */
	vhandl_t	*vt,			/* in */
	off_t		off,			/* in */
	size_t		len,			/* in */
	mprot_t		prot,			/* in */
	credid_t	credid,			/* in */
	pgno_t		*pgno)			/* out */
{
	int		error;
	struct cred	*credp;


	SPEC_STATS(I_spec_ds_addmap);

	credp = CREDID_GETCRED(credid);

#ifdef	JTK_DEBUG
	printf("I_spec_ds_addmap: handle/0x%x = %d/%d/0x%x\n",
					handle,
					handle->sh_obj.h_service.s_cell,
					handle->sh_obj.h_service.s_svcnum,
					handle->sh_obj.h_objid);
	printf("I_spec_ds_addmap: op/0x%x\n", op);
	printf("I_spec_ds_addmap: vt/0x%x\n", vt);
	printf("I_spec_ds_addmap: off/%d\n", off);
	printf("I_spec_ds_addmap: len/%d\n", len);
	printf("I_spec_ds_addmap: prot/0x%x\n", prot);
	printf("I_spec_ds_addmap: credp/0x%x\n", credp);
#endif	/* JTK_DEBUG */

	error = spec_cs_addmap_subr(handle,
					op, vt, off, len, prot, credp, pgno);

#ifdef	JTK_DEBUG
	printf("I_spec_ds_addmap.r: pgno/0x%x = 0x%x\n", pgno, *pgno);
	printf("I_spec_ds_addmap.r: error/%d\n", error);
#endif	/* JTK_DEBUG */

	return error;
}


/* ARGSUSED */
int
I_spec_ds_delmap(
	spec_handle_t	*handle,		/* in */
	vhandl_t	*vt,			/* in */
	size_t		len,			/* in */
	credid_t	credid)			/* in */
{
	int		error;
	struct cred	*credp;


	SPEC_STATS(I_spec_ds_delmap);

	credp = CREDID_GETCRED(credid);

#ifdef	JTK_DEBUG
	printf("I_spec_ds_delmap: handle/0x%x = %d/%d/0x%x\n",
					handle,
					handle->sh_obj.h_service.s_cell,
					handle->sh_obj.h_service.s_svcnum,
					handle->sh_obj.h_objid);
	printf("I_spec_ds_delmap: vt/0x%x\n", vt);
	printf("I_spec_ds_delmap: len/%d\n", len);
	printf("I_spec_ds_delmap: credp/0x%x\n", credp);
#endif	/* JTK_DEBUG */

	error = spec_cs_delmap_subr(handle, vt, len, credp);

#ifdef	JTK_DEBUG
	printf("I_spec_ds_delmap.r: error/%d\n", error);
#endif	/* JTK_DEBUG */

	return error;
}
