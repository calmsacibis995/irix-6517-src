/*
 * ckpt_procfs.c
 *
 * 	Implements procfs functionality for4 checkpoint restart.
 *
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
 */

#ident "$Revision: 1.189 $"

#include <sys/types.h>
#include <ksys/as.h>
#include <os/as/as_private.h> /* XXX */
#include <ksys/cred.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/debug.h>
#include <sys/kucontext.h>
#include <ksys/vproc.h>
#include <sys/proc.h>
#include "os/proc/pproc_private.h"	/* XXX bogus */
#include <sys/vnode.h>
#include <sys/vfs.h>
#include <ksys/vfile.h>
#include <ksys/vsession.h>
#include <sys/procfs.h>
#include <ksys/prioctl.h>
#include <sys/ckpt.h>
#include <sys/ckpt_procfs.h>
#include <sys/errno.h>
#include <sys/ksignal.h>
#include <sys/kmem.h>
#include <sys/prctl.h>
#include <sys/signal.h>
#include <sys/sat.h>
#include <sys/mode.h>
#include <sys/time.h>
#include <ksys/exception.h>
#include <ksys/fdt.h>
#include <fs/procfs/prdata.h>
#include <fs/procfs/priface.h>
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
#include <fs/procfs/procfs_n32.h>
#endif
#include <sys/hwperfmacros.h>
#include <sys/numa.h>
#include <sys/vsocket.h>
#include <sys/extacct.h>
#include <sys/acct.h>
#include <os/numa/pmo_ns.h>
#include <os/numa/pm.h>
#include <os/numa/mld.h>
#include <os/numa/pmo_list.h>
#include <os/numa/mldset.h>
#include <sys/resource.h>
#include <sys/buf.h>
#include <sys/schedctl.h>

#include <sys/cmn_err.h>

int
ckpt_prioctl_attr(int cmdval) 
{
	switch (cmdval) {
	case PIOCCKPTMSTAT:
	case PIOCCKPTGETMAP:
	case PIOCCKPTFORKMEM:
	case PIOCCKPTPUSEMA:
	case PIOCCKPTPMOGETNEXT:
	case PIOCCKPTPMGETALL:
	case PIOCCKPTPMINFO:
	case PIOCCKPTMLDINFO:
	case PIOCCKPTMLDLINKINFO:
	case PIOCCKPTMLDSETINFO:
	case PIOCCKPTMLDPLACEINFO:
	case PIOCCKPTRAFFOPEN:
	case PIOCCKPTSHM:
	case PIOCCKPTGETITMRS:
	case PIOCCKPTPMPGINFO:
		return (PIOCA_CKPT | PIOCA_VALIDMT);

	case PIOCCKPTSETRITMR:
		return (PIOCA_CKPT | PIOCA_VALIDMT | PIOCA_WRITE);

	case PIOCCKPTGETCTX:
	case PIOCCKPTGETSI:
	case PIOCCKPTUTINFO:
		return (PIOCA_CKPT | PIOCA_VALIDT);

	case PIOCCKPTSETCTX:
	case PIOCCKPTSSIG:
	case PIOCCKPTABORT:
	case PIOCCKPTSTOP:
	case PIOCCKPTSIGTHRD:
		return (PIOCA_CKPT | PIOCA_VALIDT | PIOCA_WRITE);

	case PIOCCKPTFSTAT:
	case PIOCCKPTFSWAP:
	case PIOCCKPTOPEN:
	case PIOCCKPTOPENCWD:
	case PIOCCKPTOPENRT:
	case PIOCCKPTCHROOT:
	case PIOCCKPTDUP:
	case PIOCCKPTSCHED:
		return (PIOCA_CKPT | PIOCA_VALIDP);

	case PIOCCKPTPSINFO:
	case PIOCCKPTUSAGE:
		return (PIOCA_CKPT | PIOCA_VALIDP | PIOCA_PSINFO | PIOCA_ZOMBIE);

	case PIOCCKPTSETPI:
	case PIOCCKPTQSIG:
	case PIOCCKPTMARK:
		return (PIOCA_CKPT | PIOCA_VALIDP | PIOCA_WRITE);

	}
	return (0);
}
#undef target_abi	/* from procfs_private!!! */
#undef cmd

static int
ckpt_supported_abi(int target_abi)
{
#if _MIPS_SIM == _ABI64
	return (ABI_IS_IRIX5_64(target_abi));
#else
	return (ABI_IS_IRIX5_N32(target_abi));
#endif
}

/*
 * ckpt_prfstat - retrieve file info about procs file descriptor
 */
static int
ckpt_prfstat(proc_t *p, caddr_t cmaddr)
{
	extern vnodeops_t prvnodeops;
	ckpt_statbuf_t	sbuf;
	struct vfile *fp;
	struct vattr vattr;
	vnode_t *vp;
	struct vfssw *vswp;
	int i, fd, error = 0;

	fd = fuword(cmaddr);
	if (fd < 0) {
		if (fd == -1)
			return EFAULT;
		else
			return EBADF;
 	}
	ASSERT(fd < 10000);

	bzero(&sbuf, sizeof(sbuf));

	/*
	 * Starting at index 'fd' get a reference to the process's
	 * next open file.  Note that the fp ref prevents the vnode
	 * from going away.
	 */
	fp = fdt_getref_next(p, fd, &i);
	if (fp == NULL)
		return ENOENT;

	sbuf.ckpt_fd = i;
	sbuf.ckpt_fflags = fp->vf_flag+FOPEN;

	if (VF_IS_VNODE(fp)) {
		VFILE_GETOFFSET(fp, &sbuf.ckpt_offset);
		vp = VF_TO_VNODE(fp);
		if(vp->v_fbhv && (vp->v_fops == &prvnodeops)) {
			/*
			 * avoid deadlocking via prlock
			 */
			strcpy(sbuf.ckpt_fstype, "proc");
			goto out0;
		}
		/*
		 * Read the file mode
		 */
		vattr.va_mask = AT_TYPE|AT_MODE|AT_SIZE|AT_FSID|
			AT_RDEV|AT_NODEID|AT_ATIME|AT_MTIME;

		VOP_GETATTR(vp, &vattr, 0, get_current_cred(), error);
		if (error)
			goto out;

		if (vp->v_vfsp) {
			vswp = &vfssw[vp->v_vfsp->vfs_fstype];
			if (vswp->vsw_name && *vswp->vsw_name)
				strcpy(sbuf.ckpt_fstype, vswp->vsw_name);
		}
	} else {
		sbuf.ckpt_offset = (off_t)0;
		bzero(&vattr, sizeof(vattr));
		vattr.va_type = VSOCK;
	} 

	sbuf.ckpt_mode = VTTOIF(vattr.va_type) | vattr.va_mode;
	sbuf.ckpt_size = vattr.va_size;
	sbuf.ckpt_dev  = vattr.va_fsid;
	sbuf.ckpt_rdev  = vattr.va_rdev;
	sbuf.ckpt_ino  = vattr.va_nodeid;
	sbuf.ckpt_atime = vattr.va_atime;
	sbuf.ckpt_mtime = vattr.va_mtime;
	sbuf.ckpt_fid  = (caddr_t)fp;

 out0:
	if (copyout((caddr_t)&sbuf, cmaddr, sizeof(sbuf)))
		error = EFAULT;

 out:
	VFILE_REF_RELEASE(fp);
	return error;
}

static int
ckpt_prfswap(proc_t *p, caddr_t cmaddr)
{
	ckpt_fswap_t fswap;
	struct vfile *srcfp;
	struct vfile *targfp;

	if (copyin(cmaddr, (caddr_t)&fswap, sizeof(fswap)))
		return (EFAULT);

	srcfp = fdt_getref(curprocp, fswap.srcfd);
	if (srcfp == NULL)
		return (EBADF);

	targfp = fdt_swapref(p, fswap.targfd, srcfp);
	if (targfp == NULL) {
		VFILE_REF_RELEASE(srcfp);
		return (EBADF);
	}
	VFILE_REF_RELEASE(targfp);

	return (0);
}

static int
ckpt_memstat(uthread_t *ut, caddr_t cmaddr)
{
	ckpt_mstat_arg_t ckpt_mstat;
	vnode_t *vp;
	int err;
	vasid_t vasid;
	as_getvaddrattr_t asattr;

	if (copyin(cmaddr, (caddr_t)&ckpt_mstat, sizeof(ckpt_mstat)))
		return EFAULT;

	if (as_lookup(ut->ut_asid, &vasid))
		return ENOENT;
	VAS_LOCK(vasid, AS_SHARED);

	if (VAS_GETVADDRATTR(vasid, AS_VNODE|AS_LOADVNODE, 
						ckpt_mstat.vaddr, &asattr)) {
		VAS_UNLOCK(vasid);
		as_rele(vasid);
		return EINVAL;
	}
	if ((vp = asattr.as_vp) == NULL) {
		/* vp will be null if we loaded region using loadreg(). */
		if((vp = asattr.as_loadvp) == NULL) {
			VAS_UNLOCK(vasid);
			as_rele(vasid);
			return ENOENT;
		}
	}
	/*
	 * aspacelock keeps vnode from going away
	 */
	err = xcstat(vp, ckpt_mstat.sbp, ckpt_mstat.statvers, 0,
							get_current_cred());

	VAS_UNLOCK(vasid);
	as_rele(vasid);
	return (err);
}

static int
ckpt_propen_cwd(proc_t *p, int *rvalp)
{
	vnode_t *vp = prxy_to_thread(&p->p_proxy)->ut_cdir;
	int error;

	if (vp == NULL)
		return (ENOENT);

	VN_HOLD(vp);
	if (error = ckpt_vp_open(vp, O_RDONLY, -1, rvalp))
		VN_RELE(vp);

	return (error);
}

static int
ckpt_propen_root(proc_t *p, int *rvalp)
{
	vnode_t *vp = prxy_to_thread(&p->p_proxy)->ut_rdir;
	int error;

	if (vp == NULL)
		return (ENOENT);

	VN_HOLD(vp);
	if (error = ckpt_vp_open(vp, O_RDONLY, -1, rvalp))
		VN_RELE(vp);

	return (error);
}

static void chdirec(vnode_t *, proc_t *);

static int
ckpt_chroot(uthread_t *ut, caddr_t cmaddr)
{
	vnode_t *vp;
	int error;

	if (!_CAP_ABLE(CAP_CHROOT))
		return EPERM;

	if (error = lookupname(cmaddr, UIO_USERSPACE, FOLLOW, NULLVPP, &vp, NULL))
		return error;

	if (vp->v_type != VDIR) {
		VN_RELE(vp);
		return ENOTDIR;
	}

	VOP_ACCESS(vp, VEXEC, ut->ut_cred, error);
	if (error) {
		VN_RELE(vp);
		return error;
	}

	chdirec(vp, UT_TO_PROC(ut));
	return 0;
}

void
chdirec(vnode_t *vp, proc_t *p)
{
	vnode_t **vpp = &prxy_to_thread(&p->p_proxy)->ut_rdir;

	/*
	 * Hold vp for the shared block so that if this process goes away
	 * before others have synced, there is still a reference to it 
	 */
	if (p->p_proxy.prxy_shmask & PR_SDIR) {
		VN_HOLD(vp);

		if (*vpp) {
			/*
			 * If ut_rdir was shared, also back shared block's ref
			 */
			ASSERT((*vpp)->v_count >= 2);
			VN_RELE(*vpp);
		}

		/*
		 * Add to all shared processes (if we're sharing dirs)
		 */
		if (IS_SPROC(&p->p_proxy)) {
			shaddr_t *sa = p->p_shaddr;
			sa->s_rdir = vp;
			setshdsync(sa, p, PR_SDIR, UT_UPDDIR);
		} else {
			ASSERT(p->p_proxy.prxy_utshare);
			p->p_proxy.prxy_utshare->ps_rdir = vp;
			setpsync(&p->p_proxy, UT_UPDDIR);
		}
	}

	if (*vpp)
		VN_RELE(*vpp);
	*vpp = vp;
}

static int
ckpt_propen(proc_t *p, caddr_t cmaddr, int *rvalp)
{
	ckpt_open_t openbuf;
	struct vfile *srcfp;
	vnode_t *vp;
	struct vattr vattr;
	u_short mode_saved;
	int error;

	if (copyin(cmaddr, (caddr_t)&openbuf, sizeof(openbuf)))
		return EFAULT;

	if (openbuf.ckpt_fd < 0)
		return EBADF;

	openbuf.ckpt_oflag -= FOPEN;

	/* 
	 * Get a file struct reference and hold it throughout to
	 * make sure the file struct doesn't go away due to close.
	 * Note that fp ref prevents the vnode from going away.
	 */
	if ((srcfp = fdt_getref(p, openbuf.ckpt_fd)) == NULL)
		return EBADF;

	if (!VF_IS_VNODE(srcfp)) {
		error = ENOTSUP;
		goto out;
	}
	vp = VF_TO_VNODE(srcfp);

	/*
	 * change file mode to allow CPR access
	 */
	vattr.va_mask = AT_MODE;
	VOP_GETATTR(vp, &vattr, 0, get_current_cred(), error);
	if (error)
		goto out;

	mode_saved = vattr.va_mode;

	if (!(vattr.va_mode & openbuf.ckpt_mode_req)) {
		vattr.va_mode |= openbuf.ckpt_mode_req;

		VOP_SETATTR(vp, &vattr, 0, get_current_cred(), error);
		if (error)
			goto out;
	}
	/*
	 * Get a vp ref to donate to ckpt_vp_open.
	 */
	VN_HOLD(vp);
	if (error = ckpt_vp_open(vp, openbuf.ckpt_oflag, srcfp->vf_ckpt, rvalp)) {
		VN_RELE(vp);
		goto out;
	}

	/*
	 * restore the original file mode back
	 */
	if (mode_saved != vattr.va_mode) {
		vattr.va_mode = mode_saved;

		VOP_SETATTR(vp, &vattr, 0, get_current_cred(), error);
		if (error) {
			VN_RELE(vp);
			goto out;
		}
	}

out:
	VFILE_REF_RELEASE(srcfp);
	return error;
}

static int
ckpt_dup(proc_t *p, caddr_t cmaddr, int *rvalp)
{
	int srcfd;
	struct vfile *srcfp;
	int fd;
	int error;

	srcfd = fuword(cmaddr);
	if (srcfd < 0) {
		if (srcfd == -1)
			return EFAULT;
		else
			return EBADF;
	}

	if ((srcfp = fdt_getref(p, srcfd)) == NULL)
		return EBADF;

	if (error = fdt_dup(0, srcfp, &fd)) {
		VFILE_REF_RELEASE(srcfp);
		_SAT_DUP(srcfd, 0, error);
		return error;
	}
	ASSERT(srcfp->vf_count >= 2);

	VFILE_REF_RELEASE(srcfp);
	*rvalp = fd;
	_SAT_DUP(srcfd, fd, 0);
	return 0;
}
/*
 * getmap
 */
static int
ckpt_getmap(uthread_t *ut, caddr_t cmaddr, int target_abi, int *rvalp)
{
	prmap_sgi_arg_t argbuf;
	vasid_t vasid;
	as_getattrin_t asin;
	as_getattr_t asattr;
	int error;

	if (copyin(cmaddr, (caddr_t)&argbuf, sizeof argbuf))
		return EFAULT;

        if (!ckpt_supported_abi(target_abi))
		return ENOSYS;
	/*
	 * this buffer might be very large - so we really don't want to
	 * create a copy in the kernel
	 * Note that by doing the useracc, its safe for us to act on
	 * ourselves (since GETATTR does a copyout).
	 */
	if (error = useracc(argbuf.pr_vaddr, argbuf.pr_size, B_READ, NULL))
		return error;

	if (as_lookup(ut->ut_asid, &vasid) == 0) {

		asin.as_ckptgetmap.as_n = argbuf.pr_size / sizeof(ckpt_getmap_t);
		asin.as_ckptgetmap.as_udest = argbuf.pr_vaddr;
		error = VAS_GETATTR(vasid, AS_CKPTGETMAP, &asin, &asattr);
		as_rele(vasid);

		*rvalp = asattr.as_nckptmaps;
	}
	unuseracc(argbuf.pr_vaddr, argbuf.pr_size, B_READ);
	return error;
}

static int
ckpt_schedmode(proc_t *p, caddr_t cmaddr)
{
	int schedmode;
	uthread_t *ut = prchoosethread(p);
	int error;
	__int64_t rval;

	if (copyin(cmaddr, &schedmode, sizeof(schedmode)))
		return EFAULT;

	if (schedmode < SGS_FREE || schedmode > SGS_GANG)
		return EINVAL;

	VPROC_SCHEDMODE(UT_TO_VPROC(ut), schedmode, &rval, &error);

	return (error);
}

/*
 * ckpt_savecontext
 *
 * Copied from irix5_64_savecontext in os/sig.c and modified to operate on a
 * different process!
 * Will move this to os/sig.c and integrate with irix5_64_savecontext
 * once all the issues have been worked out!
 */
static int
ckpt_savecontext(
#if _MIPS_SIM == _ABI64
	irix5_64_ucontext_t	*ucp,
#else
	irix5_n32_ucontext_t	*ucp,
#endif
	k_sigset_t		*mask,
	uthread_t		*ut,
	vasid_t			vasid,
	int			target_abi)
{
	proc_proxy_t *prxy = ut->ut_pproxy;
	int error;
	as_getasattr_t asattr;

	ucp->uc_flags = UC_ALL;
	ucp->uc_link = NULL;	/* We're not coming back! */

	/* save signal mask */
	sigktou(mask, &ucp->uc_sigmask);

	if (!(prxy->prxy_ssflags & SS_ONSTACK)) {
		/* lowest stack address */
		VAS_GETASATTR(vasid, AS_STKBASE|AS_STKSIZE, &asattr);
		ucp->uc_stack.ss_sp = (app64_ptr_t)asattr.as_stkbase;
        	ucp->uc_stack.ss_size = asattr.as_stksize;
        	ucp->uc_stack.ss_flags = 0;
	} else {
		ucp->uc_stack.ss_sp = (app64_ptr_t)prxy->prxy_sigsp;
		/*
		 * if someone mixes BSD sigstack with this then they'll get
		 * wrong result, but that's OK since BSD doesn't have the
		 * concept of alternate stack size anyway.
		 */
        	ucp->uc_stack.ss_size = prxy->prxy_spsize;
                ucp->uc_stack.ss_flags |= SS_ONSTACK;
	}

	/*
	 * Save machine context.
	 */
	priface_prgetregs(ut, &ucp->uc_mcontext.gregs[0]);
	ucp->uc_mcontext.gregs[CTX_SR] = ut->ut_exception->u_eframe.ef_sr;

	ASSERT(prhasfp());

	error = priface_prgetfpregs(ut, &ucp->uc_mcontext.fpregs);

	return error;
}

static int
ckpt_getcontext(uthread_t *ut, caddr_t cmaddr, int target_abi)
{
	ckpt_ctxt_t	ctxt;
	int		error;
	proc_t		*p = UT_TO_PROC(ut);
	vasid_t		vasid;

        if (!ckpt_supported_abi(target_abi))
		return ENOSYS;

	if (as_lookup(ut->ut_asid, &vasid))
		return ENOENT;

	VAS_LOCK(vasid, AS_SHARED);
	/*
	 * If UT_SIGSUSPEND is set, then restore signal mask from ut_suspmask
	 * because we will be restarting a sigsuspend syscall (which moves
	 * *ut_sighold to ut_suspmask, then replaces *ut_sighold).
	 */
#if _MIPS_SIM == _ABI64
	error = ckpt_savecontext((irix5_64_ucontext_t *)&ctxt.cc_ctxt,
#else
	error = ckpt_savecontext((irix5_n32_ucontext_t *)&ctxt.cc_ctxt,
#endif
			ut->ut_flags & UT_SIGSUSPEND ? &ut->ut_suspmask :
							ut->ut_sighold,
			ut, vasid, target_abi);
	VAS_UNLOCK(vasid);
	as_rele(vasid);
	if (error)
		return error;

	ctxt.cc_abi = ut->ut_pproxy->prxy_abi;
	ctxt.cc_fpflags = ut->ut_pproxy->prxy_fp.pfp_fpflags;

	if (copyout(&ctxt, cmaddr, sizeof(ctxt)))
		return EFAULT;

	/* Make a note that the process has (probably) been checkpointed.  */
	/* This is primarily of use to accounting software, which will use */
	/* this info as advice that it may find additional acct records    */
	/* for the same process. The choice of doing this here as opposed  */
	/* to one of the other ckpt_get* calls is mostly arbitrary; the    */
	/* only condition is that this should *not* be called on restart.  */
	p->p_acflag |= ACKPT;

	return 0;
}

/*
 * ckpt_restorecontext
 *
 * Copied from irix5_64_restorecontext in os/sig.c and modified to operate on a
 * different process!
 * Will move this to os/sig.c and integrate with irix5_64_restorecontext
 * once all the issues have been worked out!
 */
static int
ckpt_restorecontext(
#if _MIPS_SIM == _ABI64
	irix5_64_ucontext_t	*ucp,
#else
	irix5_n32_ucontext_t	*ucp,
#endif
	uthread_t		*ut,
	int			target_abi)
{
	int error = 0;
	vasid_t vasid;
	as_setattr_t setattr;
	proc_proxy_t	*prxy = ut->ut_pproxy;

	if (as_lookup(ut->ut_asid, &vasid))
		return ENOENT;

	if (ucp->uc_flags & UC_STACK) {
		if (ucp->uc_stack.ss_flags & SS_ONSTACK) {
			prxy->prxy_sigsp = (caddr_t)ucp->uc_stack.ss_sp;
			prxy->prxy_spsize = (__int32_t)ucp->uc_stack.ss_size;
			prxy->prxy_ssflags = (__int32_t)ucp->uc_stack.ss_flags;

			 /* prxy_spsize and prxy_siglb remain 0 for
			  * sigstack case
			  */
			if (prxy->prxy_spsize)
			    prxy->prxy_siglb =
				(caddr_t)prxy->prxy_sigsp - prxy->prxy_spsize;
		} else {
			prxy->prxy_ssflags &= ~SS_ONSTACK;
			setattr.as_op = AS_SET_STKBASE;
			setattr.as_set_stkbase =
				(uvaddr_t)ctob(btoct(ucp->uc_stack.ss_sp));
			VAS_SETATTR(vasid, 0, &setattr);
			setattr.as_op = AS_SET_STKSIZE;
			setattr.as_set_stksize = btoc(ucp->uc_stack.ss_size);
			VAS_SETATTR(vasid, 0, &setattr);
		}
	}
	if (ucp->uc_flags & UC_CPU) {
		/*
		 * Restore machine context.
		 */
		priface_prsetregs(ut, &ucp->uc_mcontext.gregs[0]);
		ut->ut_exception->u_eframe.ef_sr =
					ucp->uc_mcontext.gregs[CTX_SR];
	}
	if (ucp->uc_flags & UC_MAU)
		error = priface_prsetfpregs(ut, &ucp->uc_mcontext.fpregs);

	if (ucp->uc_flags & UC_SIGMASK) {
		register int s;

		s = ut_lock(ut);
		sigutok(&ucp->uc_sigmask, ut->ut_sighold);
		sigdiffset(ut->ut_sighold, &cantmask);
		ut_unlock(ut, s);
	}

	as_rele(vasid);

	return error;
}

struct set_pmap_arg {
	vasid_t	vasid;
	as_setattr_t ast;
};

static int
ckpt_set_pmap(uthread_t *ut, void *arg)
{
	vasid_t vasid;
	as_setattr_t *astp = (as_setattr_t *)arg;
	int error;

	if (as_lookup(ut->ut_asid, &vasid)) {
		uscan_unlock(ut->ut_pproxy);
	        return ENOENT;
 	}
	error = VAS_SETATTR(vasid, 0, astp);

	as_rele(vasid);

	return (error);
}

static int
ckpt_setcontext(uthread_t *ut, caddr_t cmaddr, int target_abi)
{
	ckpt_ctxt_t ctxt;
	/* REFERENCED */
	char fpflags;
	irix5_64_greg_t sr;
	eframe_t *ep;
	int error;
	int temp_abi;

        if (!ckpt_supported_abi(target_abi))
		return ENOSYS;

	if (copyin(cmaddr, &ctxt, sizeof(ctxt)))
		return EFAULT;
#if _MIPS_SIM == _ABI64
	if (error = ckpt_restorecontext((irix5_64_ucontext_t *)&ctxt.cc_ctxt, ut, target_abi))
#else
	if (error = ckpt_restorecontext((irix5_n32_ucontext_t *)&ctxt.cc_ctxt, ut, target_abi))
#endif
		return error;

 
#ifdef _MIPS3_ADDRSPACE
	if (ABI_IS_64BIT(ut->ut_pproxy->prxy_abi) != ABI_IS_64BIT(ctxt.cc_abi)) {
		as_setattr_t ast;

		switch (ctxt.cc_abi) {
		case ABI_IRIX5:
		case ABI_IRIX5_64:
		case ABI_IRIX5_N32:
			break;
		default:
			return ENOEXEC;
		}
		temp_abi = (ctxt.cc_abi == ABI_IRIX5_N32)?
					ABI_IRIX5 : ctxt.cc_abi;

		uscan_update(ut->ut_pproxy);

		ut->ut_pproxy->prxy_syscall = &syscallsw[temp_abi];
		ut->ut_pproxy->prxy_abi = ctxt.cc_abi;
 
		uscan_unlock(ut->ut_pproxy);

		ast.as_op = AS_SET_PMAP;
		ast.as_pmap_abi = ctxt.cc_abi;

		error = uthread_apply(ut->ut_pproxy,
					UT_ID_NULL,
					ckpt_set_pmap,
					&ast);
		if (error)
		        return (error);
	}
#else
	/*
	 * Update the1 abi, but no pmap switching..there is only 1 format
	 */
	temp_abi = (ctxt.cc_abi == ABI_IRIX5_N32)? ABI_IRIX5 : ctxt.cc_abi;

	uscan_update(ut->ut_pproxy);

	ut->ut_pproxy->prxy_syscall = &syscallsw[temp_abi];
	ut->ut_pproxy->prxy_abi = ctxt.cc_abi;
 
	uscan_unlock(ut->ut_pproxy);
#endif
	fpflags = ut->ut_pproxy->prxy_fp.pfp_fpflags = ctxt.cc_fpflags;

	/*
	 * Update the status register
	 */
	sr = ctxt.cc_ctxt.uc_mcontext.gregs[CTX_SR];

	ep = &ut->ut_exception->u_eframe;
	ep->ef_sr &= ~(SR_UX|SR_UXADDR);
	ep->ef_sr |= (sr & (SR_UX|SR_UXADDR));

#if R10000 || TFP
	ep->ef_sr &= ~SR_XX;
	ep->ef_sr |= (sr & SR_XX);
#endif
	if (ABI_IS(ABI_IRIX5_64|ABI_IRIX5_N32, ut->ut_pproxy->prxy_abi)) {
		ep->ef_sr |= SR_FR;
		ut->ut_pproxy->prxy_fp.pfp_fpflags |= P_FP_FR;
	} else {
		ep->ef_sr &= ~SR_FR;
		ut->ut_pproxy->prxy_fp.pfp_fpflags &= ~P_FP_FR;
	}
#if TFP
	if (ut->ut_pproxy->prxy_fp.pfp_fpflags & P_FP_IMPRECISE_EXCP)
		ep->ef_sr &= ~SR_DM;
	else
		ep->ef_sr |= SR_DM;

	if (fpflags & P_FP_SMM)
		ep->ef_config |= CONFIG_SMM;
	else
		ep->ef_config &= ~CONFIG_SMM;
#endif
	return 0;
}

/*
 * Get a threads misc info
 */
static int
ckpt_getutinfo(uthread_t *ut, caddr_t cmaddr, int target_abi)
{
	ckpt_uti_t uti;

        if (!ckpt_supported_abi(target_abi))
		return ENOSYS;

	uti.uti_flags = 0;
	if (ut->ut_flags & UT_PTHREAD)
		uti.uti_flags |= CKPT_UTI_PTHREAD;
	if (ut->ut_flags & UT_BLKONENTRY)
		uti.uti_flags |= CKPT_UTI_BLKONENTRY;
	/*
	 * if uthread has non-default job, theqn let cpr know
	 */
	if (ut->ut_pproxy->prxy_sched.prs_job != ut->ut_job)
		uti.uti_flags |= CKPT_UTI_SCHED;

	uti.uti_whystop = ut->ut_whystop;
	uti.uti_whatstop = ut->ut_whatstop;
	blockcnt(ut, &uti.uti_blkcnt, &uti.uti_isblocked);
	uthread_sched_getscheduler(ut, &uti.uti_sched);

	if (copyout(&uti, cmaddr, sizeof(ckpt_uti_t)))
		return EFAULT;

	return (0);
}

/*
 * Get a process's misc info
 */
static int
ckpt_getpsinfo(proc_t *p, caddr_t cmaddr, int target_abi)
{
	uthread_t *ut = prchoosethread(p);
	vproc_t *vproc;
	ckpt_psi_t ps;
	vasid_t vasid;
	as_getasattr_t asattr;
	cred_t *cr;
	int i;

        if (!ckpt_supported_abi(target_abi))
		return ENOSYS;

	ps.ps_execid = (caddr_t)p->p_exec;
	ps.ps_acflag = p->p_acflag;
	ps.ps_state = p->p_stat;
	if ((p->p_stat == SZOMB)&&(p->p_proxy.prxy_nthreads == 0))
		ps.ps_ckptflag = CKPT_PROC_EXIT;
	ps.ps_sigtramp = (caddr_t)p->p_proxy.prxy_sigtramp;
	ps.ps_unblkonexecpid = p->p_unblkonexecpid;
	ps.ps_exitsig = p->p_exitsig;
	ps.ps_shaddr = p->p_shaddr;
	ps.ps_shmask = p->p_proxy.prxy_shmask;

	if (ut) {
		ps.ps_mem = ut->ut_acct.ua_mem;
		ps.ps_ioch = ut->ut_acct.ua_ioch;
		ps.ps_cmask = ut->ut_cmask;
		bcopy((caddr_t)&ut->ut_pproxy->prxy_ru,
				(caddr_t)&ps.ps_ru, sizeof(struct rusage));
		bcopy((caddr_t)&UT_TO_KT(ut)->k_timers, (caddr_t)&ps.ps_timers,
				sizeof(ktimerpkg_t));
                ps.ps_mldlink = (caddr_t)UT_TO_KT(ut)->k_mldlink;
		vproc = UT_TO_VPROC(ut);
		for (i = 0; i < RLIM_NLIMITS; i++) {
			VPROC_GETRLIMIT(vproc, i, &ps.ps_rlimit[i]);
		}

	} else {
		ASSERT(ps.ps_state == SZOMB);
		ps.ps_mem = 0;
		ps.ps_ioch = 0;
		ps.ps_cmask = 0;
		bzero((caddr_t)&ps.ps_ru, sizeof(struct rusage));
		bzero((caddr_t)&ps.ps_timers, sizeof(ktimerpkg_t));
                ps.ps_mldlink = (caddr_t)0;
		bzero((caddr_t)ps.ps_rlimit, sizeof(ps.ps_rlimit));
	}
	ps.ps_shrefcnt = 0;
	ps.ps_shflags = 0;

	bcopy((caddr_t)&p->p_cru, (caddr_t)&ps.ps_cru, sizeof(struct rusage));
	/*
	 * Get zombies exit status
	 */
	if (ps.ps_state == SZOMB) {
		vproc_t *pvpr;
		proc_t *ppr;

		if ((pvpr = VPROC_LOOKUP(p->p_ppid)) == NULL)
			return (ENOENT);

		VPROC_GET_PROC(pvpr, &ppr);
		if (getxstat(ppr, p->p_pid, &ps.ps_xstat)) {
			VPROC_RELE(pvpr);
			return (ESRCH);
		}
		VPROC_RELE(pvpr);
	}
	/*
	 * If it's an sproc, update some fields from possibly more
	 * current values from p_shaddr
	 */
	if (ps.ps_state != SZOMB && IS_SPROC(&p->p_proxy)) {
		shaddr_t *sa = p->p_shaddr;
		int s;

		ps.ps_shrefcnt = sa->s_refcnt;
		if (sa->s_master == p->p_pid)
			ps.ps_shflags |= CKPT_PS_MASTER;

		if (p->p_proxy.prxy_shmask & PR_SID) {
			cr = pcred_access(p);
			s = mutex_spinlock(&sa->s_rupdlock);
			if (cr == sa->s_cred)
				ps.ps_shflags |= CKPT_PS_CRED;
			pcred_unaccess(p);
		} else
			s = mutex_spinlock(&sa->s_rupdlock);

		if (p->p_proxy.prxy_shmask & PR_SUMASK)
			ps.ps_cmask = sa->s_cmask;

		if (p->p_proxy.prxy_shmask & PR_SULIMIT)
			ps.ps_rlimit[RLIMIT_FSIZE].rlim_cur = sa->s_limit;

		mutex_spinunlock(&sa->s_rupdlock, s);
	}

	/* get info from AS */
	if (!ut || as_lookup(ut->ut_asid, &vasid)) {
		ps.ps_brkbase = 0;
		ps.ps_brksize = 0;
		ps.ps_stackbase = 0;
		ps.ps_stacksize = 0;
		ps.ps_lock = 0;
	} else {
		VAS_LOCK(vasid, AS_SHARED);
		VAS_GETASATTR(vasid, AS_STKBASE|AS_STKSIZE|AS_BRKBASE|AS_BRKSIZE|AS_LOCKDOWN,
						&asattr);
		VAS_UNLOCK(vasid);
		ps.ps_stackbase = asattr.as_stkbase;
		ps.ps_stacksize = asattr.as_stksize;
		ps.ps_brkbase = asattr.as_brkbase;
		ps.ps_brksize = asattr.as_brksize;
		ps.ps_lock = asattr.as_lockdown;
		as_rele(vasid);
	}

	ps.ps_flags = 0;
	if (USING_HWPERF_COUNTERS(p))
		ps.ps_flags |= CKPT_HWPERF;
	if (p->p_flag & SCEXIT)
		ps.ps_flags |= CKPT_TERMCHILD;
	if (p->p_flag & SCOREPID)
		ps.ps_flags |= CKPT_COREPID;
	if (p->p_flag & SEXECED)
		ps.ps_flags |= CKPT_PMO;

	if (copyout(&ps, cmaddr, sizeof(ckpt_psi_t)))
		return EFAULT;

	return (0);
}
/*
 * Get a process's interval timers
 */
static int
ckpt_getitimers(uthread_t *ut, caddr_t cmaddr, int target_abi)
{
	struct itimerval itimer[ITIMER_MAX];
	int errno;
	int i;

        if (!ckpt_supported_abi(target_abi))
		return ENOSYS;

	/*
	 * Fetch and stop timers.  Caller responsible for restarting timers
	 */
	for (i = 0; i < ITIMER_MAX; i++) {
		if (errno = ckptitimer(i, &itimer[i], ut))
			return (errno);
	}
	if (copyout(itimer, cmaddr, sizeof(itimer)))
		return EFAULT;

	return (0);
}

/*
 * Restart a procs real interval timer
 */
static int
ckpt_setrealitimer(uthread_t *ut, caddr_t cmaddr, int target_abi)
{
	struct itimerval aitv;


        if (!ckpt_supported_abi(target_abi))
		return ENOSYS;

	if (copyin(cmaddr, (caddr_t)&aitv, sizeof(struct itimerval)))
		return EFAULT;

	return (dosetitimer(ITIMER_REAL, &aitv, NULL, ut));
}

/*
 * The process up is stopped at this point.
 * No need to acquire lock but be careful.
 */
static int
ckpt_setpinfo(uthread_t *ut, caddr_t cmaddr, int target_abi)
{
	proc_t *p = UT_TO_PROC(ut);
	ckpt_spi_t spi;
	int spl;

	ASSERT(p == curprocp);

        if (!ckpt_supported_abi(target_abi))
		return ENOSYS;

	if (copyin(cmaddr, (caddr_t)&spi, sizeof(ckpt_spi_t)))
		return EFAULT;

	strcpy(p->p_comm, spi.spi_comm);
	strcpy(p->p_psargs, spi.spi_psargs);

	/* The accum. accounting stuff */
	bcopy((caddr_t)&spi.spi_ru, (caddr_t)&ut->ut_pproxy->prxy_ru,
						sizeof(struct rusage));
        bcopy((caddr_t)&spi.spi_cru, (caddr_t)&p->p_cru, sizeof(struct rusage));
	/*
	 * Copy in the previous timer table which was saved when this process
	 * was in nonactive timer.  We need to restart it in AS_SYS_RUN mode.
	 */
	bcopy((caddr_t)&spi.spi_timers, &UT_TO_KT(ut)->k_timers, sizeof(ktimerpkg_t));
	spl = splhi();
	ktimer_init(UT_TO_KT(ut), AS_SYS_RUN);
	splx(spl);

	p->p_acflag = spi.spi_acflag;
	ut->ut_acct.ua_bread = spi.spi_bread +
				(spi.spi_gbread * (1024LL * 1024LL * 1024LL));
	ut->ut_acct.ua_bwrit = spi.spi_bwrit +
				(spi.spi_gbwrit * (1024LL * 1024LL * 1024LL));
	ut->ut_acct.ua_syscr = spi.spi_syscr;
	ut->ut_acct.ua_syscw = spi.spi_syscw;
	ut->ut_acct.ua_mem = spi.spi_mem;
	ut->ut_acct.ua_ioch = spi.spi_ioch;

	ut->ut_cmask = spi.spi_cmask;
	p->p_exitsig = spi.spi_exitsig;

	/* Make a shadow copy of the acct data so it isn't charged twice */
	shadowacct(p, SHATYPE_PROC);

	if (spi.spi_stat == SZOMB)
		p->p_ticks = lbolt;
	/*
	 * If in share group and updating shared info, set up a sync
	 * for other share members.
	 *
	 * We do this for umask and ulimit only.  id, dirs and brk handled
	 * through existing mechanisms.
	 */
	if ((p->p_proxy.prxy_shmask & (PR_SUMASK|PR_SULIMIT))) {
		uthread_t *ut = prxy_to_thread(&p->p_proxy);

		if (IS_SPROC(&p->p_proxy)) {
			shaddr_t *sa = p->p_shaddr;

			spl = mutex_spinlock(&sa->s_rupdlock);
			if ((p->p_proxy.prxy_shmask & PR_SUMASK) &&
			    (sa->s_cmask != ut->ut_cmask)) {
				sa->s_cmask = ut->ut_cmask;
				setshdsync(sa, p, PR_SUMASK, UT_UPDUMASK);
			}
			if ((p->p_proxy.prxy_shmask & PR_SULIMIT) &&
			    (sa->s_limit != p->p_rlimit[RLIMIT_FSIZE].rlim_cur))
			{
				sa->s_limit =
					p->p_rlimit[RLIMIT_FSIZE].rlim_cur;
				setshdsync(sa, p, PR_SULIMIT, UT_UPDULIMIT);
			}
			if (spi.spi_shflags & CKPT_PS_MASTER)
				sa->s_master = p->p_pid;

			mutex_spinunlock(&sa->s_rupdlock, spl);
		} else {
			pshare_t *ps = p->p_proxy.prxy_utshare;

			ASSERT(p->p_proxy.prxy_shmask & PR_SUMASK);
			if (ps->ps_cmask != ut->ut_cmask) {
				ps->ps_cmask = ut->ut_cmask;
				setpsync(&p->p_proxy, UT_UPDUMASK);
			}
		}
	}
	return (0);
}
/*
 * Get signal info.
 *
 * If csarg.csa_async is set, get proc info...otherwise get uthread info
 */
static int
ckpt_getsiginfo(uthread_t *ut, caddr_t cmaddr, int target_abi, int *rvalp)
{
	vproc_t *vproc = UT_TO_VPROC(ut);
	sigvec_t *sigvp;
	sigqueue_t *sqp;
	int i, count;
#if _MIPS_SIM == _ABI64
	irix5_64_siginfo_t *usip, curinfo;
#else
	irix5_siginfo_t *usip, curinfo;
#endif
	ckpt_siginfo_arg_t csarg;
	int err = 0;
	k_sigset_t pending;
	int cursig;
	int s;

        if (!ckpt_supported_abi(target_abi))
		return ENOSYS;

	if (copyin(cmaddr, &csarg, sizeof(csarg)) < 0)
		return EFAULT;
	/*
	 * Count queued signals
	 */
	if (csarg.csa_async) {
		VPROC_GET_SIGVEC(vproc, VSIGVEC_ACCESS, sigvp);
		sqp = sigvp->sv_sigqueue;
	} else {
		s = ut_lock(ut);
		sqp = ut->ut_sigpend.s_sigqueue;
	}
	for (count = 0; sqp; sqp = sqp->sq_next)
		count++;

	if (csarg.csa_async) {
		VPROC_PUT_SIGVEC(vproc);
	} else
		ut_unlock(ut, s);

	if (csarg.csa_count == -1) {
		*rvalp = count;
		return (0);
	}
	/*
	 * Use the count as a sanity check.
	 */
	if (csarg.csa_count != count)
		return EAGAIN;

	if (count > 0) {
#if _MIPS_SIM == _ABI64
		usip = (irix5_64_siginfo_t *)kmem_alloc(
				count * sizeof(irix5_64_siginfo_t), KM_SLEEP);
#else
		usip = (irix5_siginfo_t *)kmem_alloc(
				count * sizeof(irix5_siginfo_t), KM_SLEEP);
#endif
	} else
		usip = NULL;

	/*
	 * Reaquire lock, get siginfo structs and double check count
	 */
	if (csarg.csa_async) {
		VPROC_GET_SIGVEC(vproc, VSIGVEC_ACCESS, sigvp);

		pending = sigvp->sv_sig;
		cursig = 0;
		bzero(&curinfo, sizeof(curinfo));

		sqp = sigvp->sv_sigqueue;
	} else {
		s = ut_lock(ut);

		pending = ut->ut_sig;
		cursig = ut->ut_cursig;

		if (ut->ut_curinfo)
#if _MIPS_SIM == _ABI64
			irix5_64_siginfoktou(&ut->ut_curinfo->sq_info, &curinfo);
#else
			irix5_siginfoktou(&ut->ut_curinfo->sq_info, &curinfo);
#endif
		else
			bzero(&curinfo, sizeof(curinfo));

		sqp = ut->ut_sigpend.s_sigqueue;
	}

	for (i = 0; sqp && (i < count); sqp = sqp->sq_next, i++)
#if _MIPS_SIM == _ABI64
		irix5_64_siginfoktou(&sqp->sq_info, &usip[i]);
#else
		irix5_siginfoktou(&sqp->sq_info, &usip[i]);
#endif


	if (csarg.csa_async) {
		VPROC_PUT_SIGVEC(vproc);
	} else
		ut_unlock(ut, s);

	if ((sqp != NULL) || (i != count)) {
		/*
		* Things changed!
		*/
		err = EAGAIN;
		goto out;
	}
	if (count) {
#if _MIPS_SIM == _ABI64
		if (copyout(usip, csarg.csa_buffer, count*sizeof(irix5_64_siginfo_t))) {
#else
		if (copyout(usip, csarg.csa_buffer, count*sizeof(irix5_siginfo_t))) {
#endif
			err = EFAULT;
			goto out;
		}
	}
	if (copyout(&curinfo, csarg.csa_curinfo, sizeof(curinfo))) {
		err = EFAULT;
		goto out;
	}
	csarg.csa_cursig = cursig;
	sigktou(&pending, &csarg.csa_pending);
	if (copyout(&csarg, cmaddr, sizeof(csarg))) {
		err = EFAULT;
		goto out;
	}
	*rvalp = count;
out:
	if (usip)
#if _MIPS_SIM == _ABI64
		kmem_free(usip, count * sizeof(irix5_64_siginfo_t));
#else
		kmem_free(usip, count * sizeof(irix5_siginfo_t));
#endif
	return err;
}

static int
ckpt_sigthread(uthread_t *ut, caddr_t cmaddr, int target_abi)
{
	int error;
	ckpt_cursig_arg_t argbuf;
#if _MIPS_SIM == _ABI64
	irix5_64_siginfo_t kern_info;
#else
	irix5_siginfo_t kern_info;
#endif
	k_siginfo_t info;
	extern k_sigset_t jobcontrol;

        if (!ckpt_supported_abi(target_abi))
		return ENOSYS;

	if (copyin(cmaddr, &argbuf, sizeof(argbuf)))
		return EFAULT;

	if ((argbuf.signo <= 0)||(argbuf.signo > NSIG))
		return EINVAL;

	if (argbuf.infop) {
		if (copyin(argbuf.infop, &kern_info, sizeof(kern_info)))
			return EFAULT;
#if _MIPS_SIM == _ABI64
		if (error = irix5_64_siginfoutok(&kern_info, &info))
#else
		if (error = irix5_siginfoutok(&kern_info, &info))
#endif
			return error;

		if (info.si_signo != argbuf.signo)
			return EINVAL;

	}
	if (sigismember(&jobcontrol, argbuf.signo))
		/*
		 * Must send job control to the process
		 */
		return (EINVAL);

	sigtouthread(ut, argbuf.signo, (argbuf.infop)? &info : NULL);

	return (0);
}

static int
ckpt_queuesig(proc_t *p, caddr_t cmaddr, int target_abi)
{
#if _MIPS_SIM == _ABI64
	irix5_64_siginfo_t kern_info;
#else
	irix5_siginfo_t kern_info;
#endif
	k_siginfo_t info;
	int error;

        if (!ckpt_supported_abi(target_abi))
		return ENOSYS;

	if (copyin(cmaddr, &kern_info, sizeof(kern_info)))
		return EFAULT;

#if _MIPS_SIM == _ABI64
	if (error = irix5_64_siginfoutok(&kern_info, &info))
#else
	if (error = irix5_siginfoutok(&kern_info, &info))
#endif
		return error;

	return sigtopid(p->p_pid, info.si_signo, SIG_ISKERN, 0, 0, &info);
}

static int
ckpt_cursig(uthread_t *ut, caddr_t cmaddr, int target_abi)
{
	ckpt_cursig_arg_t argbuf;
#if _MIPS_SIM == _ABI64
	irix5_64_siginfo_t kern_info;
#else
	irix5_siginfo_t kern_info;
#endif
	k_siginfo_t info;
	int error;

        if (!ckpt_supported_abi(target_abi))
		return ENOSYS;

	if (copyin(cmaddr, &argbuf, sizeof(ckpt_cursig_arg_t)))
		return EFAULT;

	if ((argbuf.signo <= 0)||(argbuf.signo > NSIG))
		return EINVAL;

	if (argbuf.infop) {
		if (copyin(argbuf.infop, &kern_info, sizeof(kern_info)))
			return EFAULT;
#if _MIPS_SIM == _ABI64
		if (error = irix5_64_siginfoutok(&kern_info, &info))
#else
		if (error = irix5_siginfoutok(&kern_info, &info))
#endif
			return error;

		if (info.si_signo != argbuf.signo)
			return EINVAL;

	} else {
		info.si_signo = argbuf.signo;
		info.si_code = SI_USER;
		info.si_pid = current_pid();
		info.si_uid = get_current_cred()->cr_ruid;
	}

	sigsetcur(ut, argbuf.signo, &info, 0);

	return 0;

}
/*
 * Abort a thread out of a system call.
 *
 * Note: Should *not* be call with prxy_thrdlock held...can lead to deadlock
 * (e.g. thread is in nsproc())
 */
static int
ckpt_abort(
	uthread_t	*ut,
	prnode_t	*pnp,
	caddr_t		cmaddr,
	int		target_abi,
	int		*rvalp,
	int		*unlocked)
{
	proc_t	 *pp = UT_TO_PROC(ut);
	int s;
	short blkcnt;
	unsigned char isblocked;
	irix5_64_gregset_t gregs;
#if _MIPS_SIM == _ABI64 || _MIPS_SIM == _ABIN32
	prstatus_t status;
#else
	irix5_n32_prstatus_t status;
#endif
	vasid_t vasid;
	int error = 0;
	int wakeum = 1;
	extern void prsetrun_final(uthread_t *);

        if (!ckpt_supported_abi(target_abi))
		return ENOSYS;

	/*
	 * Verify that the current process is going to stop
	 * on our request
	 */
	if ((ut->ut_flags & UT_CKPT) == 0)
		return EINVAL;

	/*
	 * Make sure it's not going to user mode
	 */
	if (error = prwstop_ut(ut, pnp)) {
		/* prunlock done already */
		*unlocked = 1;
		return (error);
	}
	if (ut->ut_syscallno == -1) {
		/*
		 * Not executing a syscall
		 */
		*rvalp = 0;
		return (0);
	}
	s = ut_lock(ut);
	/*
	 * now make sure it's either asleep or stopped
	 * as opposed as to running in the kernel!
	 */
	while (!(ut->ut_flags & UT_STOP) &&
	       !(UT_TO_KT(ut)->k_flags & KT_SLEEP)) {
		ut_unlock(ut, s);
		if (error = prwstop_ut(ut, pnp)) {
			/* prunlock done already */
			*unlocked = 1;
			return (error);
		}
		s = ut_lock(ut);
	}
	if ((ut->ut_flags & UT_STOP) && (ut->ut_whystop == JOBCONTROL)) {
		/*
		 * repost current stop signal
		 */
		int stopsig = ut->ut_whatstop;
#if     (_MIPS_SIM != _ABIO32)
		__uint64_t stopsigbit = sigmask(stopsig);
#endif
		sigbitaddset(&ut->ut_sig, stopsigbit, stopsig);

		*rvalp = STOPABORT;

	} else if (UT_TO_KT(ut)->k_flags & KT_SLEEP) {

		*rvalp = SYSABORT;

	} else {
		/*
		 * syscall completed while we were waiting
		 */
		*rvalp = SYSEND;
		wakeum = 0;
	}
	if (wakeum) {
		/*
		 * Wake up the thread if at all possible
		 */
		thread_interrupt(UT_TO_KT(ut), &s);
	}
	ut_unlock(ut, s);

	do {
		/*
		 * As a precaution, check for signals as we loop just
		 * in case something is wrong and somebody wants to kill
		 * us!
		 */
		if (sigisready() && issig(0, SIG_ALLSIGNALS))
			return (EINTR);

		if (error = prwstop_ut(ut, pnp)) {
			/* prunlock done already */
			*unlocked = 1;
			return (error);
		}
	        blockcnt(ut, &blkcnt, &isblocked);
		/*
		 * Advance the process to the chekpoint stop
		 */
		if (ut->ut_whystop != 0 &&
		    ut->ut_whystop != CHECKPOINT &&
		    !isblocked) {
			s = ut_lock(ut);
			prsetrun_final(ut);
			ut_unlock(ut, s);
		}
	} while (ut->ut_whystop != CHECKPOINT && isblocked == 0);
	/*
	 * One last examination.  Is the process is on it's way to sigtramp?
	 * If it is, this overrides other ones.  Registers should *not* be 
	 * diddled with!
	 */
	if (as_lookup(prxy_to_thread(&pp->p_proxy)->ut_asid, &vasid) == 0) {

		priface_prgetregs(ut, &gregs[0]);
		if ((gregs[CTX_EPC] ==
		     (k_smachreg_t)(__psint_t)pp->p_proxy.prxy_sigtramp) &&
		    (gregs[CTX_T9] ==
		     (k_smachreg_t)(__psint_t)pp->p_proxy.prxy_sigtramp))
			*rvalp = SYSTRAMP;
	
		as_rele(vasid);
	}
	if (cmaddr) {
		if (error = prgetstatus(ut, 0, &status))
			return error;
		if (copyout(&status, cmaddr, sizeof(status)))
			return EFAULT;
	}
	return (0);
}

static int
ckpt_mark(proc_t *pp, caddr_t cmaddr)
{
	int mark;

	mark = fuword(cmaddr);
	if (mark < 0) {
		if (mark == -1)
			return EFAULT;
		else
			return EBADF;
	}
	if (mark)
		p_flagset(pp, SCKPT);
	else
		p_flagclr(pp, SCKPT);

	return (0);
}

static int
ckpt_stop(uthread_t *ut, caddr_t cmaddr)
{
	int stop;

	stop = fuword(cmaddr);
	if (stop < 0) {
		if (stop == -1)
			return EFAULT;
		else
			return EBADF;
	}
	if (stop) {
		ut_flagset(ut, UT_CKPT);
	} else {
		ut_flagclr(ut, UT_CKPT);
	}
	return (0);
}

static void
ckpt_lock(proc_t *p, vasid_t curvasid, vasid_t targvasid)
{
	if (VASID_TO_VAS(curvasid) == VASID_TO_VAS(targvasid)) {
		VAS_LOCK(curvasid, AS_EXCL);
	} else if (current_pid() < p->p_pid) {
		VAS_LOCK(curvasid, AS_EXCL);
		VAS_LOCK(targvasid, AS_SHARED);
	} else {
		VAS_LOCK(targvasid, AS_SHARED);
		VAS_LOCK(curvasid, AS_EXCL);
	}
}

static void
ckpt_unlock(vasid_t curvasid, vasid_t targvasid)
{
	if (VASID_TO_VAS(curvasid) != VASID_TO_VAS(targvasid))
		VAS_UNLOCK(targvasid);
	VAS_UNLOCK(curvasid);
}

static int
ckpt_memfork(proc_t *p, caddr_t cmaddr)
{
	ckpt_forkmem_arg_t arg;
	preg_t *prp, *cprp;
	struct pm *pm;
	vasid_t vasid, curvasid;
	pas_t *pas, *spas;
	ppas_t *ppas;
	int	error;

	if (p == curprocp)
		return EINVAL;

	if (copyin(cmaddr, &arg, sizeof(arg)) < 0)
		return EFAULT;

	if (as_lookup(prxy_to_thread(&p->p_proxy)->ut_asid, &vasid))
		return ENOENT;
	as_lookup_current(&curvasid);
	ckpt_lock(p, curvasid, vasid);

	spas = VASID_TO_PAS(vasid);
	ppas = (ppas_t *)vasid.vas_pasid;

	prp = findfpreg_select(spas, ppas, arg.vaddr, arg.vaddr + arg.len - 1,
							arg.local);
	if (prp == NULL) {
		ckpt_unlock(curvasid, vasid);
		as_rele(vasid);
		return EINVAL;
	}

	/* do some checking first. The addr should be in range.
	 */
	if((arg.vaddr < prp->p_regva)||
			   (pregpgs(prp,arg.vaddr) < btoc(arg.len))){
		ckpt_unlock(curvasid, vasid);
		as_rele(vasid);
		return EINVAL;
	}

	/* now change to current process and attach */
	pas = VASID_TO_PAS(curvasid);
	ppas = (ppas_t *)curvasid.vas_pasid;

	/* Sometimes, separate pregions got merged into
	 * one single region at restart time with mmap(). 
	 * This leads to possible addr collision. So unmap any pregions
	 * that collids with the to be duped preg.
	 */
	cprp = findfpreg_select(pas, ppas, prp->p_regva, 
				prp->p_regva + ctob(prp->p_pglen)-1, 0);
	if(cprp) {
		(void)pas_unmap(pas, ppas, prp->p_regva, ctob(prp->p_pglen), 0);
	}
	reglock(prp->p_reg);
	pm = aspm_getdefaultpm(pas->pas_aspm, MEM_DATA);
	error = dupreg(spas, prp, pas, ppas, prp->p_regva, prp->p_type,
						prp->p_flags, pm, 0,&cprp);
	aspm_releasepm(pm);
	if (error) {
		regrele(prp->p_reg);
		ckpt_unlock(curvasid, vasid);
		as_rele(vasid);
		return ENOMEM;
	}

	if (cprp->p_reg != prp->p_reg)
		regrele(cprp->p_reg);
	regrele(prp->p_reg);
	ckpt_unlock(curvasid, vasid);
	as_rele(vasid);
	return (0);
}

static int
ckpt_getpusema(uthread_t *ut, caddr_t cmaddr, int target_abi, int *rvalp)
{
	ckpt_pusema_arg_t arg;
	int error;

        if (!ckpt_supported_abi(target_abi))
		return ENOSYS;

	if (copyin(cmaddr, (caddr_t)&arg, sizeof(arg)) < 0)
		return EFAULT;

	if (arg.count < 0)
		return EINVAL;

	if (error = usync_object_ckpt(ut, arg.uvaddr, &arg.count, arg.bufaddr))
		return error;

	*rvalp = arg.count;

	return 0;
}

static int
ckpt_pmogetnext(uthread_t *ut, caddr_t cmaddr, int target_abi, int *rvalp)
{
	asid_t asid = ut->ut_asid;
	ckpt_getpmonext_t arg;

        if (!ckpt_supported_abi(target_abi))
		return ENOSYS;

	if (copyin(cmaddr, (caddr_t)&arg, sizeof(arg)) < 0)
		return EFAULT;

	return(pmo_ckpt_pmo_nexthandle((ut == curuthread), asid,
					arg.pmo_handle, arg.pmo_type, rvalp));
}

static int
ckpt_pmogetall(uthread_t *ut, caddr_t cmaddr, int target_abi, int *rvalp)
{
	ckpt_pmgetall_arg_t arg;

        if (!ckpt_supported_abi(target_abi))
		return ENOSYS;

	if (copyin(cmaddr, (caddr_t)&arg, sizeof(arg)) < 0)
		return EFAULT;


	return (pmo_ckpt_pm_getpmhandles(ut->ut_asid,
					arg.ckpt_vrange,
					arg.ckpt_handles,
					rvalp));
}
static int
ckpt_pmoinfo(uthread_t *ut, caddr_t cmaddr, int target_abi)
{
	ckpt_pminfo_arg_t arg;

        if (!ckpt_supported_abi(target_abi))
		return ENOSYS;

	if (copyin(cmaddr, (caddr_t)&arg, sizeof(arg)) < 0)
		return EFAULT;


	return (pmo_ckpt_pm_info(ut->ut_asid,
				 arg.ckpt_pmhandle,
				 arg.ckpt_pminfo,
				 arg.ckpt_pmo));
}

static int
ckpt_mldinfo(uthread_t *ut, caddr_t cmaddr, int target_abi)
{
	ckpt_mldinfo_arg_t arg;
	ckpt_mldinfo_t mldinfo;
	int error;

        if (!ckpt_supported_abi(target_abi))
		return ENOSYS;

	if (copyin(cmaddr, (caddr_t)&arg, sizeof(arg)) < 0)
		return EFAULT;

	error = pmo_ckpt_mld_info(ut->ut_asid,
				  arg.ckpt_mldhandle,
				  &mldinfo.mld_radius,
				  &mldinfo.mld_size,
				  &mldinfo.mld_nodeid,
				  &mldinfo.mld_id);
	if (error)
		return (error);

	if (copyout(&mldinfo, arg.ckpt_mldinfo, sizeof(mldinfo)))
		return (EFAULT);

	return (0);
}

static int
ckpt_mldlinkinfo(uthread_t *ut, caddr_t cmaddr, int target_abi)
{
	ckpt_mldinfo_t mldinfo;

        if (!ckpt_supported_abi(target_abi))
		return ENOSYS;

	pmo_ckpt_mldlink_info(	UT_TO_KT(ut)->k_mldlink,
			  	&mldinfo.mld_radius,
			  	&mldinfo.mld_size,
			  	&mldinfo.mld_nodeid,
			  	&mldinfo.mld_id);

	if (copyout(&mldinfo, cmaddr, sizeof(mldinfo)))
		return (EFAULT);

	return (0);
}


static int
ckpt_mldsetinfo(uthread_t *ut, caddr_t cmaddr, int target_abi, int *rvalp)
{
	ckpt_mldset_info_arg_t arg;

        if (!ckpt_supported_abi(target_abi))
		return ENOSYS;

	if (copyin(cmaddr, (caddr_t)&arg, sizeof(arg)) < 0)
		return EFAULT;

	return (pmo_ckpt_mldset_info(ut->ut_asid,
				  arg.mldset_handle,
				  &arg.mldset_info,
				  rvalp));
}

static int
ckpt_mldset_placement(uthread_t *ut, caddr_t cmaddr, int target_abi, int *rvalp)
{
	mldset_placement_info_t arg;
	int error;

        if (!ckpt_supported_abi(target_abi))
		return ENOSYS;

	if (copyin(cmaddr, &arg, sizeof(arg)) < 0)
		return (EFAULT);

	error = pmo_ckpt_mldset_place_info(ut->ut_asid, &arg, rvalp);
	if (error)
		return (error);

	if (copyout(&arg, cmaddr, sizeof(arg)) < 0)
		return (EFAULT);

	return (0);

}

static int
ckpt_pm_pginfo(uthread_t *ut, caddr_t cmaddr, int target_abi, int *rvalp)
{
	ckpt_pm_pginfo_t arg;
	vasid_t vasid;

        if (!ckpt_supported_abi(target_abi))
		return ENOSYS;

	if (copyin(cmaddr, &arg, sizeof(arg)) < 0)
		return (EFAULT);

	if(as_lookup(ut->ut_asid, &vasid) <0)
		return ESRCH;

	*rvalp = pm_ckpt_get_pginfo(vasid, arg.vrange, arg.pginfo_list);

	if (copyout(&arg, cmaddr, sizeof(arg)) < 0)
		return (EFAULT);

	return (0);

}

/*
 * Open the pathname of he physical node in indicated element of the
 * rafflist
 */
static int
ckpt_mldset_raffopen(uthread_t *ut, caddr_t cmaddr, int target_abi, int *rvalp)
{
	ckpt_raffopen_t raffopen;
	vnode_t *vp;
	int ckpt;
	int error;

        if (!ckpt_supported_abi(target_abi))
		return ENOSYS;

	if (copyin(cmaddr, &raffopen, sizeof(raffopen)))
		return (EFAULT);

	error = pmo_ckpt_mldset_place_ckptinfo(	ut->ut_asid,
						raffopen.mldset_handle,
						raffopen.mldset_element,
						&vp,
						&ckpt);
	if (error)
		return (error);

	if (vp == NULL)
		return (EINVAL);

	/*
	 * Don't need a VN_HOLD(vp); here, 'cause gets held in
	 * pmo_ckpt_mldset_place_ckptinfo
	 */
	if (error = ckpt_vp_open(vp, O_RDONLY, ckpt, rvalp))
		VN_RELE(vp);

	return (error);
}

static int
ckpt_shm(uthread_t *ut, caddr_t cmaddr, int target_abi, int *rvalp)
{
	int count;
	int error;
	ckpt_shm_arg_t arg;
	ckpt_shmlist_t shmlist;

        if (!ckpt_supported_abi(target_abi))
		return ENOSYS;

	VPROC_GET_CKPT(UT_TO_VPROC(ut), CKPT_SHMCNT, &count, error);
	if (error)
		return (error);

	*rvalp = count;

	if (cmaddr == NULL)
		return (0);

	if (copyin(cmaddr, &arg, sizeof(arg)))
		return EFAULT;

	shmlist.count = min(count, arg.count);
	shmlist.list = (int *)kmem_alloc(shmlist.count * sizeof(int), KM_SLEEP);

	VPROC_GET_CKPT(UT_TO_VPROC(ut), CKPT_SHMLIST, &shmlist, error);
	if (error) {
		kmem_free(shmlist.list, shmlist.count * sizeof(int));
		return (error);
	}
	if (copyout(shmlist.list, arg.shmid, shmlist.count * sizeof(int))) {
		kmem_free(shmlist.list, shmlist.count * sizeof(int));
		return EFAULT;
	}
	kmem_free(shmlist.list, shmlist.count * sizeof(int));
	return (0);
}

/* ARGSUSED */
int
ckpt_prioctl(int cmd, prnode_t *pnp, caddr_t cmaddr, cred_t *cr, int flag,
	int target_abi, int *rvalp)
{
	int error = 0;
	proc_t	*p = pnp->pr_proc;
	uthread_t *ut = prchoosethread(p);
	prusage_t info1;

	switch (cmd) {
	default:
		error = EINVAL;
		break;
	case PIOCCKPTPSINFO:
		error = ckpt_getpsinfo(p, cmaddr, target_abi);
		break;
	case PIOCCKPTSETPI:
		error = ckpt_setpinfo(ut, cmaddr, target_abi);
		break;
	case PIOCCKPTUSAGE:
		priface_prgetusage(p, &info1);
		if (priface_prusage_copyout((caddr_t) &info1, cmaddr))
			error = EFAULT;
		break;
	case PIOCCKPTFSTAT:
		error = ckpt_prfstat(p, cmaddr);
		break;
	case PIOCCKPTFSWAP:
		error = ckpt_prfswap(p, cmaddr);
		break;
	case PIOCCKPTOPEN:
		error = ckpt_propen(p, cmaddr, rvalp);
		break;
	case PIOCCKPTOPENCWD:
		error = ckpt_propen_cwd(p, rvalp);
		break;
	case PIOCCKPTOPENRT:
		error = ckpt_propen_root(p, rvalp);
		break;
	case PIOCCKPTCHROOT:
		error = ckpt_chroot(ut, cmaddr);
		break;
	case PIOCCKPTDUP:
		error = ckpt_dup(p, cmaddr, rvalp);
		break;
	case PIOCCKPTQSIG:
		error = ckpt_queuesig(p, cmaddr, target_abi);
		break;
	case PIOCCKPTMARK:
		error = ckpt_mark(p, cmaddr);
		break;
	case PIOCCKPTSCHED:
		error = ckpt_schedmode(p, cmaddr);
		break;
	}
	return error;
}

/* ARGSUSED */
int
ckpt_prioctl_thread(
	pioc_ctx_t	*ctxp,
	uthread_t	*ut,
	prnode_t	*pnp,
	caddr_t		cmaddr,
	cred_t		*cr,
	int		*rvalp,
	int		*error)
{
	proc_t	*p = pnp->pr_proc;
	int unlocked = 0;

#define	cmd		ctxp->pc_cmd
#define	target_abi	ctxp->pc_tabi

	switch (cmd) {

	case PIOCCKPTMSTAT:
		*error = ckpt_memstat(ut, cmaddr);
		break;
	case PIOCCKPTGETMAP:
		*error = ckpt_getmap(ut, cmaddr, target_abi, rvalp);
		break;
	case PIOCCKPTGETCTX:
		*error = ckpt_getcontext(ut, cmaddr, target_abi);
		break;
	case PIOCCKPTSETCTX:
		*error = ckpt_setcontext(ut, cmaddr, target_abi);
		break;
	case PIOCCKPTUTINFO:
		*error = ckpt_getutinfo(ut, cmaddr, target_abi);
		break;
	case PIOCCKPTGETSI:
		*error = ckpt_getsiginfo(ut, cmaddr, target_abi, rvalp);
		break;
	case PIOCCKPTSSIG:
		*error = ckpt_cursig(ut, cmaddr, target_abi);
		break;
	case PIOCCKPTSTOP:
		*error = ckpt_stop(ut, cmaddr);
		break;
	case PIOCCKPTABORT:
		*error = ckpt_abort(ut, pnp, cmaddr, target_abi, rvalp, &unlocked);
		if (unlocked)
			return 1;
		break;
	case PIOCCKPTFORKMEM:
		*error = ckpt_memfork(p, cmaddr);
		break;
	case PIOCCKPTSETRITMR:
		*error = ckpt_setrealitimer(ut, cmaddr, target_abi);
		break;
	case PIOCCKPTGETITMRS:
		*error = ckpt_getitimers(ut, cmaddr, target_abi);
		break;
	case PIOCCKPTPUSEMA:
		*error = ckpt_getpusema(ut, cmaddr, target_abi, rvalp);
		break;
	case PIOCCKPTPMOGETNEXT:
		*error = ckpt_pmogetnext(ut, cmaddr, target_abi, rvalp);
		break;
	case PIOCCKPTPMGETALL:
		*error = ckpt_pmogetall(ut, cmaddr, target_abi, rvalp);
		break;
	case PIOCCKPTPMINFO:
		*error = ckpt_pmoinfo(ut, cmaddr, target_abi);
		break;
	case PIOCCKPTMLDINFO:
		*error = ckpt_mldinfo(ut, cmaddr, target_abi);
		break;
	case PIOCCKPTMLDLINKINFO:
		*error = ckpt_mldlinkinfo(ut, cmaddr, target_abi);
		break;
	case PIOCCKPTMLDSETINFO:
		*error = ckpt_mldsetinfo(ut, cmaddr, target_abi, rvalp);
		break;
	case PIOCCKPTMLDPLACEINFO:
		*error = ckpt_mldset_placement(ut, cmaddr, target_abi, rvalp);
		break;
	case PIOCCKPTRAFFOPEN:
		*error = ckpt_mldset_raffopen(ut, cmaddr, target_abi, rvalp);
		break;
	case PIOCCKPTSHM:
		*error = ckpt_shm(ut, cmaddr, target_abi, rvalp);
		break;
	case PIOCCKPTSIGTHRD:
		*error = ckpt_sigthread(ut, cmaddr, target_abi);
		break;
	case PIOCCKPTPMPGINFO:
		*error = ckpt_pm_pginfo(ut, cmaddr, target_abi, rvalp);
	}
	return (0);
}
