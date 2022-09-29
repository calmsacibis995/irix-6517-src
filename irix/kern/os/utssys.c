/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 3.103 $"

#include <limits.h>
#include <sys/types.h>
#include <ksys/as.h>
#include <sys/capability.h>
#include <sys/conf.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <ksys/vfile.h>
#include <ksys/fdt.h>
#include <sys/kabi.h>
#include <sys/kmem.h>
#include <sys/param.h>
#include <sys/prctl.h>
#include <sys/proc.h>
#include <sys/sat.h>
#include <sys/sema.h>
#include <ksys/vsession.h>
#include <sys/signal.h>
#include <sys/statvfs.h>
#include <sys/systm.h>
#include <sys/uadmin.h>
#include <sys/uio.h>
#include <sys/ustat.h>
#include <sys/utssys.h>
#include <sys/var.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/vsocket.h>
#include <sys/fsid.h>
#include <ksys/vproc.h>
#include <ksys/vhost.h>
#include <sys/xlate.h>
#include <fs/procfs/prdata.h>

#include "os/proc/pproc_private.h"    /* XXX bogus */

struct unamea;
extern int uname(struct unamea *, rval_t *);

static int ustat_to_irix5(void *, int, xlate_info_t *);
static int ustat_to_irix5_o32(void *, int, xlate_info_t *);
#if (_MIPS_SIM == _ABIO32)
static int ustat_to_irix5_n32(void *, int, xlate_info_t *);
#endif
#if (_MIPS_SIM == _ABI64)
static int irix5_to_f_anonid(enum xlate_mode, void *, int, xlate_info_t *);
#endif

/*
 * utssys()
 */
static int uts_fusers(char *, int, char *, rval_t *);

struct utssysa {
	union {
		char *cbuf;
		struct ustat *ubuf;
	} ub;
	union {
		sysarg_t	dev;		/* for USTAT */
		sysarg_t	flags;		/* for FUSERS */
	} un;
	sysarg_t	type;
	char	*outbp;			/* for FUSERS */
};

int
utssys(struct utssysa *uap, rval_t *rvp)
{
	struct vfs *vfsp;
	struct statvfs stf;
	struct ustat ust;
	int error, kern_abi;

	/*
	 * In a 32 bit kernel, we don't match either O32 or N32, so
	 * set the kern abi to 64 so that we'll always translate.
	 * in  64 bit kernel, we match both N32 and 64 callers, so
	 * set both for the kernel abi.
	 */
#if (_MIPS_SIM == _ABIO32)
	kern_abi = ABI_IRIX5_64;
#else
	kern_abi = (ABI_IRIX5_64 | ABI_IRIX5_N32);
#endif

	switch (uap->type) {
	case UTS_UNAME:			/* same as uname(2) */
		return uname((struct unamea *)uap, rvp);

	/* case 1 was umask */

	case UTS_USTAT:			/* ustat */
		vfsp = vfs_busydev(uap->un.dev, VFS_FSTYPE_ANY);
		if (vfsp == NULL)
			return EINVAL;
		VFS_STATVFS(vfsp, &stf, NULL, error);
		vfs_unbusy(vfsp);
		if (error)
			return error;

		ust.f_tfree = (daddr_t)stf.f_bfree;
		ust.f_tinode = (ino_t)stf.f_ffree;
		bcopy(stf.f_fstr, ust.f_fname, sizeof(ust.f_fname));
		bcopy(stf.f_fstr + sizeof(ust.f_fname), ust.f_fpack,
		      sizeof(ust.f_fpack));
		return xlate_copyout(&ust, uap->ub.ubuf, sizeof(ust),
				     ustat_to_irix5, get_current_abi(),
				     kern_abi, 1);

	case UTS_FUSERS:
		return uts_fusers(uap->ub.cbuf, uap->un.flags, uap->outbp, rvp);

	default:
		return EINVAL;
	}
}

/*
 * Determine the ways in which processes are using a named file or mounted
 * file system (path).  Normally return 0 with rvp->rval1 set to the number of
 * processes found to be using it.  For each of these, fill a f_user_t to
 * describe the process and its usage.  When successful, copy this list
 * of structures to the user supplied buffer (outbp).
 *
 * In error cases, clean up and return the appropriate errno.
 */
static int lookupanon(char *, vnode_t **, vsock_t **);
extern int uipc_vsock_from_addr(vnode_t *, vsock_t **);
extern int in_vsock_from_addr(vsock_t **vsop, struct fid *);
static int dofusers(vnode_t *, int, char *, rval_t *);
static int dosusers(vsock_t *, int, char *, rval_t *);

static int
uts_fusers(
	char *path,
	int flags,
	char *outbp,
	rval_t *rvp)
{
	vnode_t *fvp = NULL;
	vsock_t *vsop = NULL;
	int error;

	if (error = (flags & F_ANONYMOUS) ?
		    lookupanon(path, &fvp, &vsop) :
		    lookupname(path, UIO_USERSPACE, FOLLOW, NULLVPP, &fvp, NULL)) {
		return error;
	}
	if (flags & F_ANONYMOUS) {
		ASSERT(vsop);
		error = dosusers(vsop, flags, outbp, rvp);
		vsocket_release(vsop);
		return error;
	}
	ASSERT(fvp);
	if (fvp->v_type == VSOCK) {
		/*
		 * if we have a socket vnode, we need some help from the
		 * AF_UNIX code to find the vsocket structure
		 */
		error = uipc_vsock_from_addr(fvp, &vsop);
		VN_RELE(fvp);
		if (error) {
			return error;
		}
		ASSERT(vsop);
		error = dosusers(vsop, flags, outbp, rvp);
		vsocket_release(vsop);
		return error;
	}
	error = dofusers(fvp, flags, outbp, rvp);
	VN_RELE(fvp);
	return error;
}

/*
 * Lookup an anonymous file by its filesystem type name and file identifier.
 */
/*ARGSUSED2*/
static int
lookupanon(
	char *path,
	vnode_t **vpp,
	vsock_t **vsop)
{
	f_anonid_t fa;
	struct vfssw *vsw;
	u_short len;
	struct fid *fidp;
	int error;
	int socket;

	if (COPYIN_XLATE(path, &fa, sizeof fa,
			 irix5_to_f_anonid, get_current_abi(), 1))
		return EFAULT;
	socket = (!strncmp(fa.fa_fsid, FSID_SOCKET, 6));
	vsw = vfs_getvfssw(fa.fa_fsid);
	if ((vsw == 0) && !socket)
		return ENOPKG;
	if (copyin(&fa.fa_fid->fid_len, &len, sizeof len))
		return EFAULT;
	fidp = kmem_alloc(sizeof *fidp - MAXFIDSZ + len, KM_SLEEP);
	fidp->fid_len = len;
	if (copyin(fa.fa_fid->fid_data, fidp->fid_data, len)) {
		error = EFAULT;
	} else {
		if (!socket)
		    error = (*vsw->vsw_vfsops->vfs_vget)
				((bhv_desc_t *)NULL, vpp, fidp);
		else
		    error = in_vsock_from_addr(vsop, fidp);
	}
	freefid(fidp);
	return error;
}

typedef struct fuserarg {
	vnode_t	*fvp;		/* vnode of file whose users we're seeking */
	int	contained;	/* search for all files contained in a vfs */
	int	pcnt;		/* count of processes found using fvp */
	f_user_t *fuentry;	/* next found-user entry to allocate */
	int	maxpcnt;	/* max # processes we return info about */
} fuserarg_t;

static int fuserscan(proc_t *, void *, int);
static int suserscan(proc_t *, void *, int);

static int
dofusers(
	vnode_t *fvp,
	int flags,
	char *outbp,
	rval_t *rvp)
{
	size_t fubufsize;
	f_user_t *fubuf;	/* accumulate results here */
	fuserarg_t fuarg;
	int error = 0;

	fubufsize = v.v_proc * sizeof(f_user_t);
	fubuf = kmem_alloc(fubufsize, KM_SLEEP);
	fuarg.maxpcnt = v.v_proc;
	fuarg.fvp = fvp;
	fuarg.contained = (flags & F_CONTAINED) != 0;
	fuarg.fuentry = fubuf;
	fuarg.pcnt = 0;
	if (fuarg.contained && !(fvp->v_flag & VROOT)) {
		error = EINVAL;
	} else if (fvp->v_count > 1) {
		procscan(fuserscan, &fuarg);
		if (copyout(fubuf, outbp, fuarg.pcnt * sizeof(f_user_t)))
			error = EFAULT;
	}
	kmem_free(fubuf, fubufsize);
	rvp->r_val1 = fuarg.pcnt;
	return error;
}

static int
dosusers(
	vsock_t *vsop,
	int flags,
	char *outbp,
	rval_t *rvp)
{
	size_t fubufsize;
	f_user_t *fubuf;        /* accumulate results here */
	fuserarg_t fuarg;
	int error = 0;

	fubufsize = v.v_proc * sizeof(f_user_t);
	fubuf = kmem_alloc(fubufsize, KM_SLEEP);
	fuarg.maxpcnt = v.v_proc;
	fuarg.fvp = (struct vnode *)vsop;
	fuarg.contained = (flags & F_CONTAINED) != 0;
	fuarg.fuentry = fubuf;
	fuarg.pcnt = 0;
	procscan(suserscan, &fuarg);
	if (copyout(fubuf, outbp, fuarg.pcnt * sizeof(f_user_t)))
		error = EFAULT;
	kmem_free(fubuf, fubufsize);
	rvp->r_val1 = fuarg.pcnt;
	return error;
}

int	fdt_suser(proc_t *, vsock_t *);

/*ARGSUSED3*/
static int
suserscan(proc_t *p, void *arg, int mode)
{
	fuserarg_t *ap;
	int flags;
	f_user_t *fu;
	vproc_t *vpr;

	if (!p)
		return 0;

	/* Grab a reference on our proc so it cannot exit. */
	if ((vpr = VPROC_LOOKUP(p->p_pid)) == NULL)
		return 0;

	ap = (fuserarg_t *)arg;

	/*
	 * Stop the scan if we have already exceeded the number of procs
	 * we can hold information about.
	 */
	if (ap->maxpcnt == ap->pcnt)
		return 1;

	flags = 0;
	if (fdt_suser(p, (vsock_t *)ap->fvp) != 0)
		flags |= F_OPEN;
	if (flags) {
		fu = ap->fuentry++;
		fu->fu_pid = p->p_pid;
		fu->fu_flags = flags;
		fu->fu_uid = p->p_cred->cr_uid;
		ap->pcnt++;
	}
	VPROC_RELE(vpr);
	return 0;
}


/*ARGSUSED*/
static int
fuserscan(proc_t *p, void *arg, int mode)
{
	fuserarg_t *ap;
	int error;
	int flags;
	f_user_t *fu;
	int s;
	vproc_t *vpr;
	uthread_t *ut;
	vnode_t *vnp;
	vsession_t *vsp;
	vasid_t vasid;
	as_getattr_t asattr;
	as_getattrin_t asin;

	if (!p)
		return 0;

	/*
	 * Grab a reference on our proc so it cannot exit.
	 * we tell VPROC_LOOKUP to ignore zombies and exiting processes
	 * this means that fuser won't tell us about processes that
	 * are hung in exit trying to close a resource ...
	 */
	if ((vpr = VPROC_LOOKUP(p->p_pid)) == NULL)
		return 0;

	ap = (fuserarg_t *)arg;

	/*
	 * Stop the scan if we have already exceeded the number of procs
	 * we can hold information about.
	 */
	if (ap->maxpcnt == ap->pcnt)
		return 1;

#define	IN_USE(vp)	((vp) != NULL && (VN_CMP((vp), ap->fvp) \
			 || ap->contained && (vp)->v_vfsp == ap->fvp->v_vfsp))

	flags = 0;

	uscan_hold(&p->p_proxy);

	ut = prxy_to_thread(&p->p_proxy);	/* any will do? */

	if (IN_USE(ut->ut_cdir))
		flags |= F_CDIR;
	if (IN_USE(ut->ut_rdir))
		flags |= F_RDIR;

	mrlock(&p->p_who, MR_ACCESS, PZERO);
	if (IN_USE(p->p_exec))
		flags |= F_TEXT;
	mrunlock(&p->p_who);

	if (as_lookup(ut->ut_asid, &vasid) == 0) {
		asin.as_inuse.as_vnode = ap->fvp;
		asin.as_inuse.as_contained = ap->contained;
		/* returns EBUSY if vnode is in use */
		if (VAS_GETATTR(vasid, AS_INUSE, &asin, &asattr))
			flags |= F_MAP;

		as_rele(vasid);
	}
	uscan_rele(&p->p_proxy);

	mraccess(&p->p_who);
	if (p->p_sid < 0) {
		mraccunlock(&p->p_who);
	} else {
		vsp = VSESSION_LOOKUP(p->p_sid);
		ASSERT(vsp);
		mraccunlock(&p->p_who);
		VSESSION_CTTY_HOLD(vsp, &vnp, error);
		if (error == 0) {
			if (IN_USE(vnp))
				flags |= F_TTY;
			VSESSION_CTTY_RELE(vsp, vnp);
		}
		VSESSION_RELE(vsp);
	}

#define	PR_IN_USE(pnp)	\
	((pnp) != NULL && \
	 (VN_CMP((pnp)->pr_vnode, ap->fvp) || \
	  ap->contained && (pnp)->pr_vnode->v_vfsp == ap->fvp->v_vfsp))

	s = p_lock(p);
	if (PR_IN_USE(p->p_trace) || PR_IN_USE(p->p_pstrace))
		flags |= F_TRACE;
	p_unlock(p, s);
	if (fdt_fuser(p, ap->fvp, ap->contained) != 0)
		flags |= F_OPEN;

#undef	PR_IN_USE
#undef	IN_USE

	if (flags) {
		fu = ap->fuentry++;
		fu->fu_pid = p->p_pid;
		fu->fu_flags = flags;
		fu->fu_uid = (uid_t) p->p_cred->cr_uid;
		ap->pcnt++;
	}
	VPROC_RELE(vpr);
	return 0;
}

/*
 * administrivia system call
 */

sema_t uasem;
int power_off_flag;	/* set in powerintr(), or uadmin() */

struct uadmina {
	sysarg_t	cmd;
	sysarg_t	fcn;
	sysarg_t	mdep;
};

int
uadmin(struct uadmina *uap)
{
	int error;

	if (!_CAP_ABLE(CAP_SHUTDOWN))
		return EPERM;

	error = psema(&uasem, PZERO+1);
	if (error == -1)
		return EINTR;

	/* record command for sat */
	_SAT_SET_SUBSYSNUM(uap->cmd);

	switch (uap->cmd) {

	case A_SHUTDOWN:
		VHOST_KILLALL(-SIGKILL, 0, current_pid(), 0);
		delay(HZ);	/* allow other procs to exit */

		VHOST_SYNC(SYNC_FSDATA|SYNC_ATTR|SYNC_CLOSE|SYNC_WAIT);
		VFS_MOUNTROOT(rootvfs, ROOT_UNMOUNT, error);
		/* FALL THROUGH */
	case A_REBOOT:
		VHOST_REBOOT(uap->fcn, (char *)uap->mdep);
		/* no return expected */
		break;

	case A_REMOUNT:
		VFS_MOUNTROOT(rootvfs, ROOT_REMOUNT, error);
		break;

	case A_KILLALL: {
		vp_get_attr_t	attr;		/* to lookup caller's sid */
		VPROC_GET_ATTR(curvprocp, VGATTR_SID, &attr);
		VHOST_KILLALL(uap->mdep, uap->fcn,	/* signo, pgrp */
			      current_pid(), attr.va_sid);
		break;
	}

	case A_SETFLAG:
		if(uap->fcn == AD_POWEROFF) {
			power_off_flag = 1;
			break;
		}
		/* else fall through to error */

	default:
		error = EINVAL;
	}
	vsema(&uasem);
	return error;
}

#if (_MIPS_SIM == _ABI64 || _MIPS_SIM == _ABIN32)
static int
ustat_to_irix5(
	void *from,
	int count,
	xlate_info_t *info)
{
	ASSERT(ABI_IS_IRIX5(info->abi));
	return ustat_to_irix5_o32(from, count, info);
}
#endif

#if (_MIPS_SIM == _ABIO32)
static int
ustat_to_irix5(
	void *from,
	int count,
	xlate_info_t *info)
{
	ASSERT(ABI_IS_IRIX5_N32(info->abi) || ABI_IS_IRIX5(info->abi));
	if (ABI_IS_IRIX5_N32(info->abi))
		return ustat_to_irix5_n32(from, count, info);
	else
		return ustat_to_irix5_o32(from, count, info);
}
#endif

/* ARGSUSED */
static int
ustat_to_irix5_o32(
	void *from,
	int count,
	register xlate_info_t *info)
{
	XLATE_COPYOUT_PROLOGUE(ustat, irix5_o32_ustat);
	ASSERT(count == 1);

	target->f_tfree = source->f_tfree;
	target->f_tinode = source->f_tinode;
	if (target->f_tfree  != source->f_tfree ||
	    target->f_tinode != source->f_tinode)
		XLATE_COPYOUT_ERROR(EFBIG);
	bcopy(&source->f_fname, &target->f_fname, sizeof(target->f_fname));
	bcopy(&source->f_fpack, &target->f_fpack, sizeof(target->f_fpack));

	return 0;
}

#if (_MIPS_SIM == _ABIO32)
/* ARGSUSED */
static int
ustat_to_irix5_n32(
	void *from,
	int count,
	register xlate_info_t *info)
{
	XLATE_COPYOUT_PROLOGUE(ustat, irix5_n32_ustat);
	ASSERT(count == 1);

	target->f_tfree = source->f_tfree;
	target->f_tinode = source->f_tinode;
	bcopy(&source->f_fname, &target->f_fname, sizeof(target->f_fname));
	bcopy(&source->f_fpack, &target->f_fpack, sizeof(target->f_fpack));

	return 0;
}
#endif

#if (_MIPS_SIM == _ABI64)

/* ARGSUSED */
static int
irix5_to_f_anonid(
	enum xlate_mode mode,
	void *to,
	int count,
	xlate_info_t *info)
{
	register f_anonid_t *fp;
	register irix5_f_anonid_t *i5_fp;

	ASSERT(info->smallbuf != NULL);
	ASSERT(mode == SETUP_BUFFER || mode == DO_XLATE);

	if (mode == SETUP_BUFFER) {
		ASSERT(info->copybuf == NULL);
		ASSERT(info->copysize == 0);
		if (sizeof(irix5_f_anonid_t) <= info->inbufsize)
			info->copybuf = info->smallbuf;
		else
			info->copybuf = kern_malloc(sizeof(irix5_f_anonid_t));
		info->copysize = sizeof(irix5_f_anonid_t);
		return 0;
	}

	ASSERT(info->copysize == sizeof(irix5_f_anonid_t));
	ASSERT(info->copybuf != NULL);

	fp = to;
	i5_fp = info->copybuf;

	fp->fa_fid = (void *)(__psunsigned_t)i5_fp->fa_fid;
	bcopy(i5_fp->fa_fsid, fp->fa_fsid, FSTYPSZ);

	return 0;
}
#endif	/* _ABI64 */
