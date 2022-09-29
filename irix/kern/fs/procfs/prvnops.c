/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*#ident	"@(#)uts-comm:fs/procfs/prvnops.c	1.15"*/
#ident	"$Revision: 1.159 $"

#include <sys/types.h>
#include <ksys/cred.h>
#include <sys/debug.h>
#include <sys/dirent.h>
#include <sys/fpu.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <ksys/vfile.h>
#include <sys/fs_subr.h>
#include <sys/kabi.h>
#include <sys/kmem.h>
#include <sys/ksignal.h>
#include <sys/param.h>
#include <sys/poll.h>
#include <ksys/vproc.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/unistd.h>
#include <sys/var.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/sat.h>
#include <sys/mac_label.h>
#include <sys/capability.h>
#include <sys/pathname.h>
#include <string.h>
#include <ksys/pid.h>
#include <sys/ddi.h>
#include <sys/prctl.h>
#include "prdata.h"
#include "prsystm.h"
#include "procfs.h"
#include "procfs_private.h"
#include "os/proc/pproc_private.h"
#ifdef  CELL_IRIX
#include "fs/cfs/cfs_relocation.h"
#endif
#define STATIC static

static int prpoll(bhv_desc_t *, short, int, short *, struct pollhead **,
		  unsigned int *);
static vnode_t *prget(pid_t, prnodetype_t);

STATIC int maxprfree = 50;
#ifdef PRCLEAR
int prnode_pidmismatch_cnt;
#endif

int prprint;

#ifdef	CELL_IRIX
/*
 * Get a free prnode from freelist or allocate anew.
 */
prnode_t *
prnode_alloc(void)
{
	prnode_t	*pnp;
	int		s;

	s = mutex_spinlock(&procfs_lock);
	if (procfs_freelist) {
		pnp = procfs_freelist;
		procfs_freelist = pnp->pr_free;
		ASSERT(procfs_nfree > 0);
		procfs_nfree--;
		procfs_refcnt++;
		mutex_spinunlock(&procfs_lock, s);
	} else {
		procfs_refcnt++;
		mutex_spinunlock(&procfs_lock, s);
		pnp = (prnode_t *) kmem_zalloc(sizeof(prnode_t), KM_SLEEP);
		mutex_init(&pnp->pr_lock, MUTEX_DEFAULT, "prnode");
	}

	return pnp;
}

/*
 * Free prnode to freelist or deallocate.
 */
void
prnode_free(
	prnode_t	*pnp)
{
	int		s;

	s = mutex_spinlock(&procfs_lock);
	ASSERT(procfs_refcnt > 0);
	procfs_refcnt--;
	if (procfs_nfree >= maxprfree) {
		mutex_spinunlock(&procfs_lock, s);
		mutex_destroy(&pnp->pr_lock);
		kmem_free(pnp, sizeof(prnode_t));
	} else {
		pnp->pr_free = procfs_freelist;
		procfs_freelist = pnp;
		procfs_nfree++;
		mutex_spinunlock(&procfs_lock, s);
	}
}
#endif	/* CELL_IRIX */

/* ARGSUSED */
static int
propen(
	bhv_desc_t *bdp,
	struct vnode **vpp,
	register mode_t flag,
	struct cred *cr)
{
	struct vnode *vp = BHV_TO_VNODE(bdp);
	struct prnode *pnp = BHVTOPRNODE(bdp);
	struct proc *p;
	int error = 0;
	int s;
	int anywriter = 0;
	struct pr_tracemasks *pr_masks = NULL;

	/*
	 * If the process exists, lock it and the entire /proc mechanism now.
	 * Otherwise we have a race condition with prclose().
	 */
	if (error = prlock(pnp, ZYES, vp->v_type == VDIR ? PRNULLOK : PRNONULL))
		return error;

	if ((p = pnp->pr_proc) != NULL) {
		ASSERT(vp->v_type == VREG);

		/* Allocate pr_masks before we grab siglock, in case we
		 * sleep.
		 * Don't allocate a tracemask struct if the process
		 * has already started to exit. This is to avoid
		 * races with exit freeing this structure - since
		 * we have asked for zombies (ZYES).
		 * It is the client process's responsability to
		 * free the p_trmasks structure when it exits.
		 *
		 * Note that we don't grab siglock to check the
		 * p_stat or p_flag - since we have prlock'd
		 * the process, either those attributes will already
		 * be set, or will be unable to be set as long
		 * as we have the process referenced via prlock.
		 */
		if (pnp->pr_type == PR_PROCDIR && p->p_trmasks == NULL &&
		    p->p_stat != SZOMB)
			pr_masks = kmem_zone_zalloc(procfs_trzone, KM_SLEEP);

		s = p_lock(p);
		if (p->p_flag & STRC) {
			error = ENOENT;
			goto out;
		}

		/*
		 * Fail if the process had or has privilege and caller
		 * does not.
		 */
		if (   (pnp->pr_type != PR_PSINFO)
		    && (p->p_flag & SPRPROTECT)
		    && !cap_able_cred(cr, CAP_PROC_MGT)) {
			error = EAGAIN;
			goto out;
		}
	}
	if (pnp->pr_pflags & PRINVAL) {
		error = EAGAIN;
		goto out;
	}

	/*
	 * Maintain a count of opens for write.  Allow exactly one
	 * O_RDWR|O_EXCL request and fail subsequent ones (even for
	 * the super-user).  Once an exclusive open is in effect,
	 * fail subsequent attempts to open for writing (except for
	 * the super-user).
	 */
	if (flag & FWRITE) {
		if (pnp->pr_type == PR_PSINFO) {
			error = EACCES;
			goto out;
		}
		if (flag & FEXCL) {
			if (pnp->pr_writers > 0) {
				error = EBUSY;	/* XXX EACCES in irix */
				goto out;
			}
			pnp->pr_pflags |= PREXCL;
		} else if (pnp->pr_pflags & PREXCL) {
			ASSERT(pnp->pr_writers > 0);
			if (!cap_able_cred(cr, CAP_DAC_WRITE)) {
				error = EACCES;	/* XXX EBUSY in svr4 */
				goto out;
			}
		}
		pnp->pr_writers++;
		anywriter++;
	}

	/*
	 * Keep a count of opens so that we can identify the last close.
	 * The vnode reference count (v_count) is unsuitable for this
	 * because references are generated by other operations in
	 * addition to open and close.
	 */
	pnp->pr_opens++;
	if (p != NULL) {
		int firstopen = !(p->p_flag & SPROPEN);

		ASSERT(p->p_trace == pnp || p->p_pstrace == pnp);
		if (pnp->pr_type == PR_PROCDIR) {
			if (p->p_trmasks == NULL) {
				ASSERT(pr_masks != NULL || p->p_stat == SZOMB);
				ASSERT(firstopen);

				/* if SZOMB, pr_masks will be null */
				p->p_trmasks = pr_masks;
			} else {
				ASSERT(pr_masks == NULL);
			}
			p->p_flag |= SPROPEN;
		}

		if (p->p_stat != SZOMB) {
			/*
			 * on first open, set up dbgflags
			 * dbx /debug expects KPRIFORK in that it expects that
			 * the child will stop on fork/exec.
			 * -XXX Somebody else could have raced us here and
			 * we are nolonger the firstopener.
			 */
			if (firstopen && anywriter) {
				/* by default, inherit RLC on fork,
				 * but not KLC
				 */
				p->p_dbgflags |= (KPRRLC | KPRIFORK);
			}
		}
	}

out:
	if (p != NULL)
		p_unlock(p, s);
	if (error && pr_masks)
		kmem_zone_free(procfs_trzone, pr_masks);
	prunlock(pnp);
	return error;
}

static int
prchkfatal(uthread_t *ut, void *arg)
{
	sigvec_t *sigvp = (sigvec_t *)arg;
	kthread_t *kt = UT_TO_KT(ut);
	int s;

	s = ut_lock(ut);
	if (isfatalsig(ut, sigvp)) {
		if (kt->k_wchan == (caddr_t)&ut->ut_pblock.s_wait)
			setrun(kt);
	}
	ut_unlock(ut, s);
	return (0);
}

/* ARGSUSED */
static int
prclose(
	bhv_desc_t *physbdp,
	int flag,
	lastclose_t lastclose,
	struct cred *cr)
{
	register struct prnode *pnp = BHVTOPRNODE(physbdp);
	register struct proc *p;
	prnode_t *tpnp;
	int error, s;
        int dokill = 0;
	int checkfatal = 0;
	/*
	 * There is nothing to do until the last close
	 * of the file table entry.
	 */
	if (lastclose == L_FALSE)
		return 0;

	/*
	 * If the process exists, lock it and the entire /proc mechanism now.
	 * Otherwise we have a race condition with propen().
	 */
	if (error = prlock(pnp, ZYES, PRNULLOK))
		return error;

	/*
	 * When the last reference to a writable file descriptor goes
	 * away, decrement the count of writers.  When the count of
	 * writers drops to zero, clear the "exclusive-use" flag.
	 * Anomaly: we can't distinguish multiple writers enough to
	 * tell which one originally requested exclusive-use, so once
	 * exclusive-use is set it stays on until all writers have
	 * gone away.
	 */
	ASSERT(pnp->pr_opens > 0);
	--pnp->pr_opens;
	if ((flag & FWRITE) && --pnp->pr_writers == 0)
		pnp->pr_pflags &= ~PREXCL;

	/*
	 * If there is no process, there is nothing more to do.
	 */
	if ((p = pnp->pr_proc) == NULL)
		goto out;

	if (pnp->pr_type == PR_PSINFO) {
		prunlock(pnp);
		return 0;
	}

	/*
	 * On last close of all writable file descriptors,
	 * perform run-on-last-close logic.
	 */
	uscan_update(&p->p_proxy);
	s = p_lock(p);
	for (tpnp = p->p_trace; tpnp != NULL; tpnp = tpnp->pr_next) {
		if (tpnp->pr_writers) {
			goto unsiglck;
		}
	}
	if (p->p_stat == SZOMB)
		goto zombie;

	/*
	 * Check for fatalsigs?
	 */
	checkfatal = p->p_dbgflags & KPRCKF;
	p->p_dbgflags &= ~KPRCKF;
	/*
	 * Reset the tracing flags.
	 * Since the process doesn't need to be stopped we can't just reset
	 * the SSTEP bit -- if the caller closes without resetting this, the
	 * target process will die.
	 */
	if (p->p_dbgflags & (KPRKLC|KPRRLC)) {
		ASSERT(p->p_trmasks != NULL);
		dokill = p->p_dbgflags & KPRKLC;
		(void) prletrun(p);
	}

	/*
	 * On last close of all /proc file descriptors,
	 * reset the process's proc-open flag.
	 */
zombie:
	for (tpnp = p->p_trace; ; tpnp = tpnp->pr_next) {
		if (tpnp == NULL) {
			p->p_flag &= ~SPROPEN;
			break;
		}
		if (tpnp->pr_opens)
			break;
	}
unsiglck:
	p_unlock(p, s);
	uscan_unlock(&p->p_proxy);
	if (dokill)
	        (void) sigtopid(p->p_pid, SIGKILL, SIG_HAVPERM, 0,
				(cred_t *)NULL, (k_siginfo_t *)NULL);

	if (checkfatal)
 		uthread_apply(&p->p_proxy, UT_ID_NULL, prchkfatal, &p->p_sigvec);
out:
	prunlock(pnp);

	return 0;
}

/* ARGSUSED */
static int
prread(
	bhv_desc_t *bdp,
	struct uio *uiop,
	int ioflag,
	struct cred *fcr,
	struct flid *fl)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	register prnode_t *pnp = BHVTOPRNODE(bdp);
	register proc_t *p;
	int error;

	if (pnp->pr_type == PR_PSINFO)
		return EPERM;
	if (vp->v_type == VDIR)
		error = EISDIR;
	else if ((error = prlock(pnp, ZNO, PRNONULL)) == 0) {
		p = pnp->pr_proc;
		if (pnp->pr_pflags & PRINVAL)
			error = EAGAIN;
		else if (   (p->p_flag & SPRPROTECT)
			 && !cap_able_cred(fcr, CAP_PROC_MGT))
			/*
			 * Fail if the process had or has privilege and
			 * caller does not.
			 */
			 error = EAGAIN;
		else {
			/*
			 * If two regions are adjacent to each other,
			 * dbx could try to do a read that spans across
			 * them. We would get a short read if we hit up
			 * against the end of a region. Hence we just
			 * call prusrio() again. uiop should have
			 * been updated. We do not return the error
			 * from the second prusrio(), but just the previous
			 * short read.
			 */
			if (!(error = prusrio(pnp->pr_proc,
					      prchoosethread(pnp->pr_proc),
					      UIO_READ, uiop, 0)) &&
			     uiop->uio_resid) {
				do
					error = prusrio(pnp->pr_proc,
					      		prchoosethread(pnp->pr_proc),
							UIO_READ,
							uiop, 0);
				while (!error && uiop->uio_resid);
				error = 0;
			}
		}
		prunlock(pnp);
	}
	return error;
}

/* ARGSUSED */
static int
prwrite(
	bhv_desc_t *bdp,
	struct uio *uiop,
	int ioflag,
	struct cred *fcr,
	struct flid *fl)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	register prnode_t *pnp = BHVTOPRNODE(bdp);
	register proc_t *p;
	int error;

	if (pnp->pr_type == PR_PSINFO)
		return EPERM;
	if (vp->v_type == VDIR)
		error = EISDIR;
	else if ((error = prlock(pnp, ZNO, PRNONULL)) == 0) {
		p = pnp->pr_proc;
		if (pnp->pr_pflags & PRINVAL)
			error = EAGAIN;
		else if (   (p->p_flag & SPRPROTECT)
			 && !cap_able_cred(fcr, CAP_PROC_MGT))
			/*
			 * Fail if the process had or has privilege and
			 * caller does not.
			 */
			 error = EAGAIN;
		else
			error = prusrio(pnp->pr_proc,
					prchoosethread(pnp->pr_proc),
					UIO_WRITE, uiop, 0);
		prunlock(pnp);
	}
	return error;
}

/* prioctl in prioctl.c */

/* ARGSUSED */
static int
prgetattr(
	register bhv_desc_t *bdp,
	register vattr_t *vap,
	int flags,
	struct cred *cr)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	register prnode_t *pnp = BHVTOPRNODE(bdp);
	register proc_t *p;
	int error;
	timespec_t start;
	extern timespec_t boottime;
	extern timespec_t forktime;
	extern mac_label *mac_low_high_lp;
	auto pgcnt_t size, rss;

	/*
	 * Return all the attributes.  Should be refined so that it
	 * returns only those asked for.
	 *
	 * Most of this is complete fakery anyway.
	 */
	if (pnp->pr_pflags & PRINVAL)
		return EAGAIN;
	if (vp->v_type == VDIR) {
		vap->va_uid = 0;
		vap->va_gid = 0;
		vap->va_nlink = 2;
		if (pnp->pr_type == PR_PROCDIR) {
			vap->va_nodeid = PRROOTINO;
			vap->va_size = (v.v_proc + 3) * PRSDSIZE;
		} else {
			vap->va_nodeid = PRPINFOINO;
			vap->va_size = (v.v_proc + 2) * PRSDSIZE;
		}
		nanotime_syscall(&vap->va_atime);
		vap->va_mtime = forktime;	/* XXX svr4 uses hrestime */
		vap->va_ctime = boottime;	/* XXX svr4 uses hrestime */
	} else {
		cred_t *pcr;

		if (error = prlock(pnp, ZYES, PRNONULL))
			return error;
		p = pnp->pr_proc;
		pcr = pcred_access(p);
		vap->va_uid = pcr->cr_uid;
		vap->va_gid = pcr->cr_gid;
		pcred_unaccess(p);
		vap->va_nlink = 1;
		vap->va_nodeid = ptoi(p->p_pid);

		/* prgetattr is called in a path where the process may have
		 * been prlocked with the ZYES flag, hence the prxy_threads
		 * field may be invalid.
		 */
		if (p->p_stat == SZOMB)
			size = 0;
		else
			as_getassize(prxy_to_thread(&p->p_proxy)->ut_asid,
					&size, &rss);
		vap->va_size = ctob(size);

		start.tv_sec = p->p_start; /* XXX svr4 uses hrestime */
		start.tv_nsec = 0;
		prunlock(pnp);
		vap->va_atime = vap->va_mtime = vap->va_ctime = start;
	}

	vap->va_type = vp->v_type;
	vap->va_mode = pnp->pr_mode;
	vap->va_fsid = procfs_info.info_dev;
	vap->va_rdev = 0;
	vap->va_blksize = 1024;
	vap->va_nblocks = btod(vap->va_size);
	vap->va_vcode = 0;
	vap->va_xflags = 0;
	vap->va_extsize = 0;
	vap->va_nextents = 0;
	vap->va_anextents = 0;
	vap->va_projid = 0;
	vap->va_gencount = 0;
	return 0;
}

/* ARGSUSED */
static int
praccess(
	register bhv_desc_t *bdp,
	register int mode,
	register struct cred *cr)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	register prnode_t *pnp = BHVTOPRNODE(bdp);
	register proc_t *p;
	register int error;
	struct cred *pcr;

	if (pnp->pr_pflags & PRINVAL)
		return EAGAIN;
	if ((mode & VWRITE) && (vp->v_vfsp->vfs_flag & VFS_RDONLY))
		return EROFS;

	if (vp->v_type == VREG && pnp->pr_type != PR_PSINFO) {
		if (error = prlock(pnp, ZNO, PRNONULL))
			return error;
		p = pnp->pr_proc;

		/*
		 * MAC access check.
		 * Since the label of the target is required, the process
		 * must be locked.
		 */
		pcr = pcred_access(p);
		if (_MAC_ACCESS(pcr->cr_mac, cr, mode))
			error = EACCES;
		/*
		 * Do the permission bit check for non-privileged processes.
		 */
		else if (cr->cr_uid != pcr->cr_ruid
		    || cr->cr_uid != pcr->cr_suid
		    || cr->cr_gid != pcr->cr_rgid
		    || cr->cr_gid != pcr->cr_sgid) {
			if (!cap_able_cred(cr, CAP_DAC_OVERRIDE)) {
				error = EACCES;
			}
		}
		pcred_unaccess(p);

		if (!error)
			error = prisreadable(p, cr);
		prunlock(pnp);
		if (error)
			return error;
	}
	/*
	 * Do the permission bit check for non-privileged processes.
	 */
	if ((pnp->pr_mode & mode) != mode &&
	    !cap_able_cred(cr, CAP_DAC_OVERRIDE))
		return EACCES;
	return 0;
}

/* ARGSUSED */
STATIC int
prlookup(
	bhv_desc_t *pbdp,
	register char *comp,
	vnode_t **vpp,
	pathname_t *pnp,
	int flags,
	vnode_t *rdir,
	cred_t *cr)
{
	vnode_t *dp = BHV_TO_VNODE(pbdp);
	prnode_t *dpp = BHVTOPRNODE(pbdp);
	register int n;

	if (comp[0] == 0 || strcmp(comp, ".") == 0 ||
	    (strcmp(comp, "..") == 0 && dp != procfs_pinfo.pr_vnode)) {
		VN_HOLD(dp);
		*vpp = dp;
		return 0;
	}
	if (strcmp(comp, "..") == 0) {
		struct vnode *vp = procfs_root.pr_vnode;
		VN_HOLD(vp);
		*vpp = vp;
		return 0;
	}
	if (strcmp(comp, "pinfo") == 0 && dp != procfs_pinfo.pr_vnode) {
		struct vnode *vp = procfs_pinfo.pr_vnode;
		VN_HOLD(vp);
		*vpp = vp;
		return 0;
	}

	if (comp[0] == '.')	/* Oh?  Why? */
		comp++;
	n = 0;
	while (*comp) {
		if (*comp < '0' || *comp > '9')
			return ENOENT;
		n = 10 * n + *comp++ - '0';
	}
	*vpp = prget(n, dpp->pr_type);
	return (*vpp == NULL) ? ENOENT : 0;
}

static vnode_t *
prget(
	pid_t pid,
        prnodetype_t type)
{
        vproc_t *vpr;
        vnode_t *vp;

        vpr = VPROC_LOOKUP_STATE(pid, ZYES);
        if (vpr == NULL)
		return (NULL);
	VPROC_PRNODE(vpr, type == PR_PROCDIR, &vp);
        VPROC_RELE(vpr);
	return (vp);
}

/* ARGSUSED */
static int
prreaddir(
	bhv_desc_t *bdp,
	register uio_t *uiop,
	cred_t *fcr,
	int *eofp)
{
#define PDIR_RECLEN		(round(sizeof(dirent64_t) - 1 + PNSIZ + 1))
#define PIDBUFCNT		20

	/* dirbuf holds PIDBUFCNT dirent structure */
	void *dirbuf[(PDIR_RECLEN * PIDBUFCNT) / sizeof (void *)];
	pid_t pidbuf[PIDBUFCNT];
        pidord_t ordbuf[PIDBUFCNT];
	int reclen, xent;
        pidord_t ord;
	int oresid;
	off_t off;
	prnode_t *pnp = BHVTOPRNODE(bdp);
	size_t bufcnt = 0;
	ino_t d_ino;
	char d_name[PNSIZ+1];
	int (*dirent_func)(void *, ino_t, off_t, char *);
	int target_abi;

	target_abi = GETDENTS_ABI(get_current_abi(), uiop);
	switch (target_abi) {
	case ABI_IRIX5_64:
	case ABI_IRIX5_N32:
		dirent_func = fs_fmtdirent;
		break;

	case ABI_IRIX5:
		dirent_func = irix5_fs_fmtdirent;
		break;
	}

	if (uiop->uio_offset < 0 || uiop->uio_resid <= 0)
		return ENOENT;
	if (eofp)
		*eofp = 0;
	oresid = uiop->uio_resid;

	if (pnp->pr_type == PR_PROCDIR)
		xent = 3;
	else xent = 2;

	/*
	 * Loop until user's request is satisfied or until all processes
	 * have been examined.
	 */
	for (; uiop->uio_resid > 0; uiop->uio_offset = off + 1) {

		if ((off = uiop->uio_offset) == 0) {	/* "." */
			if (pnp->pr_type == PR_PROCDIR)
				d_ino = PRROOTINO;
			else
				d_ino = PRPINFOINO;
			d_name[0] = '.';
			d_name[1] = '\0';
			reclen = (*dirent_func)(dirbuf, d_ino,
					1, d_name);
		} else if (off == 1) { /* ".." */
			d_ino = PRROOTINO;
			d_name[0] = '.';
			d_name[1] = '.';
			d_name[2] = '\0';
			reclen = (*dirent_func)(dirbuf, d_ino,
					2, d_name);
		} else if (off == 2 && pnp->pr_type == PR_PROCDIR) {
			d_ino = PRPINFOINO;
			d_name[0] = 'p';
			d_name[1] = 'i';
			d_name[2] = 'n';
			d_name[3] = 'f';
			d_name[4] = 'o';
			d_name[5] = '\0';
			reclen = (*dirent_func)(dirbuf, d_ino,
					3, d_name);
		} else {
			caddr_t bp;
			int j, k;

			/*
			 * Stop when entire pid table has been examined.
			 */
			ord = off - xent;
			bufcnt = PIDBUFCNT;

			if (pid_getlist(&ord, &bufcnt, pidbuf, ordbuf)) {
			        if (eofp)
					*eofp = 1;
			  	break;
			}

			reclen = 0;
			bp = (caddr_t) dirbuf;

			for (j = 0; j < bufcnt; j++) {
				pid_t pid;
				int trec;

				pid = pidbuf[j];
				d_ino = ptoi(pid);

				for (k = PNSIZ-1; k >= 0; k--) {
					d_name[k] = pid % 10 + '0';
					pid /= 10;
				}
				d_name[PNSIZ] = '\0';
				trec = (*dirent_func)(bp, d_ino,
					ordbuf[j] + xent, d_name);
				reclen += trec;
                                if (reclen > uiop->uio_resid) {
					reclen -= trec;
					if (j == 0)
					        ord = off - xent;
					else
					        ord = ordbuf[j - 1];
					break;
				}
				bp += trec;
			}
			/* 'ord' is set by pid_getlist, and represents
			 * the next possible pid that a subsequent call
			 * to pid_getlist should return.
			 * We subtract 1 here, since the top of this loop
			 * adds 1 back on.
			 */
			off = ord + xent - 1;
		}

		if (reclen == 0 || reclen > uiop->uio_resid) {

			/*
			 * Error if no entries have been returned yet.
			 */
			if (uiop->uio_resid == oresid)
				return EINVAL;

			break;
		}
		/*
		 * uiomove() updates both resid and offset by the same
		 * amount.  But we want offset to change in increments
		 * of PRSDSIZE, which is different from the number of bytes
		 * being returned to the user.  So we set uio_offset
		 * separately, ignoring what uiomove() does.
		 */
		if (reclen > 0 && uiomove(dirbuf, reclen, UIO_READ, uiop))
			return EFAULT;

	}

	return 0;

#undef PIDBUFCNT
#undef PDIR_RECLEN
}

/* ARGSUSED */
STATIC int
prfsync(
	bhv_desc_t *bdp,
	int uio,
	cred_t *cr,
	off_t start,
	off_t stop)
{
	return 0;
}

/* ARGSUSED */
static int
prinactive(
	register bhv_desc_t *bdp,
	cred_t *cr)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	register prnode_t *pnp;
	prnode_t *tpnp;
	/*REFERENCED*/
	int error;
	proc_t *p;
	int s;

	if (vp->v_type == VDIR) {
		/*
		 * This only happens when we unmount procfs.
		 * Just pull the procfs behavior from the
		 * vnode and return.
		 */
		vn_bhv_remove(VN_BHV_HEAD(vp), bdp);
		return VN_INACTIVE_NOCACHE;
	}

	pnp = BHVTOPRNODE(bdp);
	ASSERT(pnp->pr_opens == 0);

	error = prlock(pnp, ZYES, PRNULLOK);
	ASSERT(error == 0);

#ifdef PRCLEAR
	if ((p = test_and_set_ptr((void **)(&pnp->pr_listproc),NULL)) != NULL) {
		s = p_lock(p);

		 if (pnp->pr_tpid != p->p_pid) {
			/*
			 * for debugging purposes keep track how
			 * often we hit this
			 */
			prnode_pidmismatch_cnt++;

			pnp->pr_next = NULL;
			vn_bhv_remove(VN_BHV_HEAD(vp), bdp);
			p_unlock(p, s);
			goto out;
		}
#else
	if ((p = pnp->pr_proc) != NULL) {

		s = p_lock(p);
		ASSERT(pnp->pr_proc == p);
#endif


		if (pnp->pr_type == PR_PROCDIR)
			tpnp = p->p_trace;
		else
			tpnp = p->p_pstrace;
		ASSERT(tpnp != NULL);

		if (vp == (tpnp->pr_vnode)) {
			if (pnp->pr_type == PR_PROCDIR)
				p->p_trace = pnp->pr_next;
			else
				p->p_pstrace = pnp->pr_next;
		} else {
			while (tpnp->pr_next != pnp) {
				tpnp = tpnp->pr_next;
				ASSERT(tpnp != NULL);
			}
			tpnp->pr_next = pnp->pr_next;
		}

#ifdef PRCLEAR
		p->p_prnode_refcnt--;
#endif

		/*
		 * Remove our behavior from the vnode with the siglock
		 * held, to resolve race with prinvalidate.
		 */
		pnp->pr_next = NULL;
		vn_bhv_remove(VN_BHV_HEAD(vp), bdp);

		p_unlock(p, s);

	} else {
#ifdef PRCLEAR
		/*
		 * prclear has already broken the relationship
		 * between the prnode and the process.
		 * Our only task is now to free the prnode keeping in
		 * mind that we can't reference the process anymore
		 */
#endif
		pnp->pr_next = NULL;
		vn_bhv_remove(VN_BHV_HEAD(vp), bdp);
	}

#ifdef PRCLEAR
out:;
#endif

	if (pnp->pr_pollhead) {
		phfree(pnp->pr_pollhead);
		pnp->pr_pollhead = NULL;
	}

	prunlock(pnp);

	s = mutex_spinlock(&procfs_lock);
	ASSERT(procfs_refcnt > 0);
	procfs_refcnt--;
	pnp->pr_vnode = NULL;
	if (procfs_nfree >= maxprfree) {
		mutex_spinunlock(&procfs_lock, s);
		mutex_destroy(&pnp->pr_lock);
		kmem_free(pnp, sizeof(prnode_t));
	} else {
		pnp->pr_free = procfs_freelist;
		procfs_freelist = pnp;
		procfs_nfree++;
		mutex_spinunlock(&procfs_lock, s);
	}

	/* the inactive_teardown flag must have been set at vn_alloc time */
	ASSERT(vp->v_flag & VINACTIVE_TEARDOWN);

	return VN_INACTIVE_NOCACHE;
}

/* ARGSUSED */
STATIC int
prseek(
	bhv_desc_t *bdp,
	off_t ooff,
	off_t *noffp)
{
	return 0;
}

static int
prpoll(
	bhv_desc_t *bdp,
	register short events,
	int anyyet,
	register short *reventsp,
	struct pollhead **phpp,
	unsigned int *genp)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	prnode_t *pnp = BHVTOPRNODE(bdp);
	proc_t *p;

	if (pnp->pr_type == PR_PSINFO)
		return EPERM;
	/*
	 * Default select/poll for the root of the filesystem.
	 */
	if (vp->v_type == VDIR)
		return fs_poll(bdp, events, anyyet, reventsp, phpp, genp);

	if (!pnp->pr_pollhead) {
		struct pollhead *php = phalloc(KM_SLEEP);
		mutex_lock(&pnp->pr_lock, PINOD);
		if (!pnp->pr_pollhead)
			pnp->pr_pollhead = php;
		else
			phfree(php);
		mutex_unlock(&pnp->pr_lock);
	}
	/*
	 * Always snapshot pollhead generation *before* examining state
	 * since the state isn't guarded by any lock ...
	 */
	*genp = POLLGEN(pnp->pr_pollhead);

	/*
	 * Select/polling for POLLIN/POLLOUT will always return true.
	 */
	if (events & (POLLIN|POLLOUT|POLLRDNORM))
		*reventsp = events;

	/*
	 * poll(2) indicates that POLLIN and POLLPRI are mutually exclusive,
	 * as are POLLHUP and POLLOUT.
	 *	POLLPRI - process has stopped at an event of interest
	 *	POLLHUP - process has terminated
	 *	POLLNVAL - process is a system process or is polling itself
	 */
	if (events & (POLLPRI|POLLRDBAND)) {
		if (prlock(pnp, ZNO, PRNONULL)) {
			*reventsp = (events & ~(POLLPRI | POLLOUT | POLLRDBAND))
					 | POLLHUP;
			return 0;
		}
		if (!anyyet) {
			ASSERT(pnp->pr_pollhead != NULL);
			*phpp = pnp->pr_pollhead;
		}

		p = pnp->pr_proc;
		ASSERT(p->p_stat != SZOMB);

		/*
		 * Processes can poll themselves for (POLLIN | POLLOUT), but
		 * not for POLLPRI.
		 */
		if (p == curprocp)
			*reventsp = (events & ~(POLLPRI|POLLRDBAND)) | POLLNVAL;
		else {
                        proc_proxy_t *px = &p->p_proxy;
		        uthread_t *ut;
			int s;
                        int gotit;

			uscan_access(px);
			if (px->prxy_utshare == NULL) {
			        ut = prxy_to_thread(px);
				s = ut_lock(ut);
				gotit = isstopped(ut);
				ut_unlock(ut, s);
			}
			else if (px->prxy_flags & PRXY_JPOLL) {
			        if ((px->prxy_flags & PRXY_JSARMED) &&
                                    px->prxy_jscount != 0)
                                	gotit = 0;
			        else uscan_forloop(px, ut) {
				        s = ut_lock(ut);
					gotit = isstopped(ut);
					ut_unlock(ut, s);
					if (!gotit)
						break;
				}
			}
			else uscan_forloop(px, ut) {
			        s = ut_lock(ut);
				gotit = ut->ut_flags & UT_EVENTPR;
				ut_unlock(ut, s);
				if (gotit)
				        break;
			}
			uscan_unlock(px);
			if (gotit)
				*reventsp = events & ~(POLLIN|POLLRDNORM);
			else
				*reventsp = events & ~(POLLPRI|POLLRDBAND);
		}
		prunlock(pnp);
	}
	return 0;
}

/*
 * provide some info to tell folks how big a buffer they might
 * need to hold the name of a /proc path and name.
 * We fudge a bit..
 */
/* ARGSUSED */
static int
prpathconf(
	bhv_desc_t	*bdp,
	int		cmd,
	long		*valp,
	struct cred 	*credp)
{
	int error = 0;

	switch (cmd) {
	case _PC_FILESIZEBITS:
		*valp = 64; /* for 64 bit procs */
		break;
	case _PC_NAME_MAX:
		*valp = PNSIZ+1;
		break;
	case _PC_PATH_MAX:
		*valp = 128; /* seems large enough */
		break;
	default:
		error = EINVAL;
		break;
	}
	return error;
}

/*ARGSUSED*/
static int
prattrget(
	bhv_desc_t *bdp,
	char *name,
	char *value,
	int *valuelenp,
	int flags,
	struct cred *cred)
{
	int i;

	if ((i = _MAC_PROC_ATTR_GET(bdp,name,value,valuelenp,flags,cred)) >= 0)
		return i;

	return ENOATTR;
}

vnodeops_t prvnodeops = {
	BHV_IDENTITY_INIT(VN_BHV_PR,VNODE_POSITION_BASE),
	propen,
	prclose,
	prread,
	prwrite,
	prioctl,
	fs_setfl,
	prgetattr,
	(vop_setattr_t)fs_nosys,
	praccess,
	prlookup,
	(vop_create_t)fs_nosys,
	(vop_remove_t)fs_nosys,
	(vop_link_t)fs_nosys,
	(vop_rename_t)fs_nosys,
	(vop_mkdir_t)fs_nosys,
	(vop_rmdir_t)fs_nosys,
	prreaddir,
	(vop_symlink_t)fs_nosys,
	(vop_readlink_t)fs_nosys,
	prfsync,
	prinactive,
	(vop_fid_t)fs_nosys,
	(vop_fid2_t)fs_nosys,
	fs_rwlock,
	fs_rwunlock,
	prseek,
	fs_cmp,
	(vop_frlock_t)fs_nosys,
	(vop_realvp_t)fs_nosys,
	(vop_bmap_t)fs_nosys,
	(vop_strategy_t)fs_noval,
	(vop_map_t)fs_nodev,
	(vop_addmap_t)fs_nosys,
	(vop_delmap_t)fs_nosys,
	prpoll,
	(vop_dump_t)fs_nosys,
	prpathconf,
	(vop_allocstore_t)fs_nosys,
	(vop_fcntl_t)fs_nosys,
	(vop_reclaim_t)fs_noerr,
	prattrget,
	(vop_attr_set_t)fs_nosys,
	(vop_attr_remove_t)fs_nosys,
	(vop_attr_list_t)fs_nosys,
	(vop_cover_t)fs_nosys,
	(vop_link_removed_t)fs_noval,
	fs_vnode_change,
	(vop_ptossvp_t)fs_noval,
	(vop_pflushinvalvp_t)fs_noval,
	(vop_pflushvp_t)fs_nosys,
	(vop_pinvalfree_t)fs_noval,
	(vop_sethole_t)fs_noval,
	(vop_commit_t)fs_nosys,
	(vop_readbuf_t)fs_nosys,
	fs_strgetmsg,
	fs_strputmsg,
};
