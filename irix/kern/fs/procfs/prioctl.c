/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*#ident	"@(#)uts-comm:fs/procfs/prioctl.c	1.6"*/

#ident	"$Revision: 1.241 $"

#include <sys/types.h>
#include <sys/acct.h>
#include <ksys/as.h>
#include <ksys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/extacct.h>
#include <sys/fault.h>
#include <ksys/vfile.h>
#include <sys/kmem.h>
#include <sys/sema.h>
#include <sys/kabi.h>
#include <sys/ksignal.h>
#include <sys/kthread.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/proc.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/timers.h>
#include <sys/vnode.h>
#include <sys/watch.h>
#include <sys/atomic_ops.h>
#include <sys/xlate.h>
#include <sys/runq.h>
#include <ksys/vop_backdoor.h>
#include "prdata.h"
#include "procfs.h"
#include "prsystm.h"
#include "priface.h"
#include <ksys/vproc.h>
#include <ksys/exception.h>
#include <ksys/prioctl.h>
#include <ksys/fdt.h>
#ifdef CKPT
#include <sys/ckpt.h>
#endif
#include "procfs_n32.h"
#ifdef R10000
#include <sys/hwperftypes.h>
#include <sys/hwperfmacros.h>
#include <sys/capability.h>

typedef struct pioc_hwarg {
	void *arg;
	void *auxp;
	int *rvalp;
} pioc_hwarg_t;

int piocenevctrsex(uthread_t *ut, void *hwargs);
int piocrelevctrs(uthread_t *ut, void *rvalp);
int piocgetevctrs(uthread_t *ut, void *hwargs);
int piocgetevctrl(uthread_t *ut, void *hwargs);
int piocsetevctrl(uthread_t *ut, void *hwargs);
int piocsaveccntrs(uthread_t *ut, void *rv);
#endif

#include "os/proc/pproc_private.h"
#include "ksys/vsession.h"

#ifdef	DEBUG
#define	STATIC
#else	/* ! DEBUG */
#define	STATIC	static
#endif	/* ! DEBUG */

#ifdef SN
#include <sys/SN/hwcntrs.h>
#include <sys/pfdat.h>
#include <sys/lpage.h>
#endif

#ifdef _MTEXT_VFS
#include "os/as/as_private.h"
#include <sys/mtext.h>
#endif /* _MTEXT_VFS */

#define cmd	        CTX.pc_cmd
#define target_abi      CTX.pc_tabi
#define pcbuf           CTX.pc_buf

#if (_MIPS_SIM == _ABIO32) 
#define PRSTATUS_SIZE	sizeof(irix5_n32_prstatus_t)
#define KERN_ABI        CTX.pc_kabi
#elif (_MIPS_SIM == _ABIN32)
#define PRSTATUS_SIZE	sizeof(irix5_n32_prstatus_t)
#define KERN_ABI        ABI_IRIX5_N32
#else
#define PRSTATUS_SIZE	sizeof(prstatus_t)
#define KERN_ABI        ABI_IRIX5_64
#endif

static int      prioctl_prepare(pioc_ctx_t *, struct prnode *, caddr_t, int);
static void     prioctl_finish(pioc_ctx_t *);
static int      prioctl_thread(pioc_ctx_t *, uthread_t *, struct prnode *, 
			       caddr_t, struct cred *, int *,
			       struct vopbd *vbds, int);

static int      prioctl_setflags(proc_t *, ulong_t);
static int      prioctl_resetflags(proc_t *, ulong_t);
static prrun_t *prioctl_prrun(pioc_ctx_t *, caddr_t);
static int      pioccfault(uthread_t *ut, void *ignored);

int		prsetrun(uthread_t *, void *);
int             prsetrun_opts(uthread_t *, prrun_t *, int, int, int);
void            prsetrun_proc(proc_t *, prrun_t *);
void		prsetrun_final(uthread_t *);
int		prchkrun(uthread_t *, void *);
int		prchkrun_step(uthread_t *, void *);
int		prchkrun_sub(uthread_t *);
extern int	prstatus_to_32(void *, int, xlate_info_t *);
extern int	irix5_to_prio(enum xlate_mode, void *, int, xlate_info_t *);

/*
 * A little chicanery to get compiler-generated max-buffer size
 */
typedef union prioctl_maxbuf {
	gregset_t	gregset;
	fpregset_t	fpregset;
	prstatus_t	prstatus;
	prusage_t	prusage;
	irix5_n32_prstatus_t	irix5_n32_prstatus;
	prpsinfo_t	prpsinfo;
	prrun_t		prrun;
	timespec_t	timespec[MAX_PROCTIMER];
	irix5_64_gregset_t	irix5_64_gregset;
	irix5_64_fpregset_t	irix5_64_fpregset;
	prthreadctl_t	prthreadctl;
} prioctl_maxbuf_t;


/*
 * Special structure for PIOCRUN interaction with prsetrun
 */
typedef struct prsetrun_s {
        prrun_t *prsr_prrun;    /* Address of associated PIOCRUN options. */
} prsetrun_t;


/*
 * This routine to set debug flags (PIOCSET ioctl) is a separate routine
 * mainly because we don't want to all off the right margin of our source
 * windows.
 */
int
prioctl_setflags(
	proc_t *p,
        ulong_t dbgflags)
{
	int 		n;
	proc_proxy_t	*px = &p->p_proxy;

	/*
         * Do anything involving PR_JOINTSTOP or PR_JOINTPOLL with the
         * uscan lock held for update to make sure we have a consistent
         * set of flags and are sure that the proxy flags are set
         * consistently with p_dbgflags, except for the fact that these
         * should not be on in the proxy if there is only one thread
         * so as to preserve the simple case of dostop when the more
         * complicated case is not needed.
	 */
	if (dbgflags & (PR_JOINTSTOP|PR_JOINTPOLL))
	        uscan_update(px);

	/*
         * Now set the proper flags requiring p_lock.
	 */
	if (dbgflags & (PR_FORK|PR_RLC|PR_KLC|
			PR_JOINTSTOP|PR_JOINTPOLL|PR_CKF)) {
	        n = p_lock(p);

		/*
                 * Enforce restriction that PR_JOINTPOLL requires PR_JOINSTOP.
		 */
		if ((dbgflags & PR_JOINTPOLL) &&
		    (dbgflags & PR_JOINTSTOP) == 0 &&
		    (p->p_dbgflags & KPRJSTOP) == 0) {
		        p_unlock(p, n);
			uscan_unlock(px);
			return (EINVAL);
		}

		/*
		 * Now do the flags and give up the proc lock.
		 */
		if (dbgflags & PR_FORK)
		        p->p_dbgflags |= KPRIFORK;
		if (dbgflags & PR_RLC)
		        p->p_dbgflags |= KPRRLC;
		if (dbgflags & PR_KLC)
		        p->p_dbgflags |= KPRKLC;
		if (dbgflags & PR_JOINTSTOP)
		        p->p_dbgflags |=  KPRJSTOP;
		if (dbgflags & PR_JOINTPOLL)
		        p->p_dbgflags |=  KPRJPOLL;
		if (dbgflags & PR_CKF)
			p->p_dbgflags |= KPRCKF;
		p_unlock(p, n);
	}

	/*
         * Propage changes involving PR_JOINTSTOP and PR_JOINTPOLL to the
         * local proxy.  Of course this is where, they will have to be
         * propagated to other proxies when they exist.  Also, give up the
         * uscan lock for update, gotten at the top.
	 */
	if (dbgflags & (PR_JOINTSTOP|PR_JOINTPOLL)) {
	        if (px->prxy_utshare != NULL) {
 		        if (dbgflags & PR_JOINTSTOP)
		                prxy_flagset(px, PRXY_JSTOP);
			if (dbgflags & PR_JOINTPOLL)
		                prxy_flagset(px, PRXY_JPOLL);
		}
		uscan_unlock(px);
	}

	return (0);
}

/*
 * This routine to reset debug flags (PIOCRESET ioctl) is a separate routine
 * mainly because we don't want to all off the right margin of our source
 * windows.
 */
int
prioctl_resetflags(
	proc_t *p,
        ulong_t dbgflags)
{
	int 		n;
	proc_proxy_t	*px = &p->p_proxy;

	/*
         * Do anything involving PR_JOINTSTOP or PR_JOINTPOLL with the
         * uscan lock held for update to make sure we have a consistent
         * set of flags and are sure that the proxy flags are set
         * consistently with p_dbgflags.
	 */
	if (dbgflags & (PR_JOINTSTOP|PR_JOINTPOLL))
	        uscan_update(px);

	/*
         * Now clear the proper flags requiring p_lock.
	 */
	if (dbgflags & (PR_FORK|PR_RLC|PR_KLC|
			PR_JOINTSTOP|PR_JOINTPOLL)) {

		/*
                 * Enforce restriction that PR_JOINTPOLL requires PR_JOINSTOP.
		 */
	        n = p_lock(p);
		if ((dbgflags & PR_JOINTSTOP) &&
		    (dbgflags & PR_JOINTPOLL) == 0 && 
		    (p->p_dbgflags & PR_JOINTPOLL) == 0){
		        p_unlock(p, n);
			uscan_unlock(px);
			return (EINVAL);
		}

		/*
		 * Now do the flags and give up the proc lock.
		 */
		if (dbgflags & PR_FORK)
		        p->p_dbgflags &= ~KPRIFORK;
		if (dbgflags & PR_RLC)
		        p->p_dbgflags &= ~KPRRLC;
		if (dbgflags & PR_KLC)
		        p->p_dbgflags &= ~KPRKLC;
		if (dbgflags & PR_JOINTSTOP)
		        p->p_dbgflags &=  ~KPRJSTOP;
		if (dbgflags & PR_JOINTPOLL)
		        p->p_dbgflags &=  ~KPRJPOLL;
		p_unlock(p, n);
	}

	/*
         * Propage changes involving PR_JOINTSTOP and PR_JOINTPOLL to the
         * local proxy.  Of course this is where, they will have to be
         * propagated to other proxies when they exist.  Also, give up the
         * uscan lock for update, gotten at the top.
	 */
	if (dbgflags & (PR_JOINTSTOP|PR_JOINTPOLL)) {
	        if (dbgflags & PR_JOINTSTOP)
		        prxy_flagclr(px, PRXY_JSTOP);
		if (dbgflags & PR_JOINTPOLL)
		        prxy_flagclr(px, PRXY_JPOLL);
		uscan_unlock(px);
	}
	return (0);
}

/*
 * subroutines moved out of the large switch
 * in prioctl() to save stack space.
 */
STATIC int
prioctl_piocthread(
	proc_t		*p,
	pioc_ctx_t	*ctx,
	caddr_t		cmaddr,
	int		flag,
	struct prnode	*pnp,
	struct cred	*cr,
	int		*rvalp,
        struct vopbd	*vbds,
	int		*gotofallout)

{
#define	CTX	(*ctx)
	int             dir;
	int		error = 0;
	int             set;
	pioc_ctx_t      ctx2;
	prthreadctl_t   *ptp;
	uthread_t	*utp;
	uthread_t       *utfound = NULL;


	*gotofallout = 0;

	ptp = (prthreadctl_t *)pcbuf;

	if (cmaddr == NULL) {
		return EINVAL;
	} else if (COPYIN_XLATE(cmaddr, ptp, sizeof(prthreadctl_t),
					irix5_to_prthreadctl,
					target_abi, 1)) {
		return EFAULT;
	}

	/*
	 * Pick out directtion and set codes and check them for 
	 * validity.
	 */
	dir = ptp->pt_flags & PTF_DIR;
	set = ptp->pt_flags & PTF_SET;

	if (   dir > PTFD_MAX
	    || set > PTFS_MAX
	    || ((set | dir) != ptp->pt_flags)) { 
		return EINVAL;
	}

	/*
	 * Get the lock to go through the threads.  When the set
	 * is PTFS_EVENTS, we need the lock for update so that
	 * the same event is not picked up twice.  Setting
	 * UT_EVENTR only reuires the uthread lock but resetting
	 * it requires the uscan lock for update as well.
	 */
	if (set == PTFS_EVENTS)
		uscan_update(&p->p_proxy);
	else
		uscan_access(&p->p_proxy);

	/*
	 * Scan for the designated threads.  We may break out when
	 * the thread id is equal or record the best match while
	 * going through the entire list if equality is not OK
	 * or not encountered.
	 */
	uscan_forloop(&p->p_proxy, utp) {
		if (set == PTFS_STOPPED && !isstopped(utp))
			continue;

		if (set == PTFS_EVENTS && !(utp->ut_flags & UT_EVENTPR))
			continue;

		if (utp->ut_id == ptp->pt_tid && dir <= PTFD_GEQ) {
			utfound = utp;
			break;
		}

		if (utp->ut_id > ptp->pt_tid && dir >= PTFD_GEQ) {
			if (utfound == NULL || utfound->ut_id > utp->ut_id)
				utfound = utp;
		}
	}

	/*
	 * Reset event bit if PTFS_EVENTS was specified.
	 */
	if (set == PTFS_EVENTS && utfound != NULL) {
		int s = ut_lock(utfound);
		utfound->ut_flags &= ~UT_EVENTPR;
		ut_unlock(utfound, s);
	}

	/*
	 * Give up the lock and return ENOENT if a matching thread
	 * was not found.
	 */
	uscan_unlock(&p->p_proxy);

	if (utfound == NULL)
		return ENOENT;

	ptp->pt_tid = utfound->ut_id;

	if (copyout(&ptp->pt_tid, cmaddr, sizeof(tid_t)))
		return EFAULT;

	/*
	 * Now do the operation on the specified thread.
	 */
	ctx2.pc_cmd = ptp->pt_cmd;

	if (error = prioctl_prepare(&ctx2, pnp, ptp->pt_data, flag))
		return error;

	if ((ctx2.pc_attr & (PIOCA_THREAD | PIOCA_MTHREAD)) == 0)
		return EINVAL;

	error = prioctl_thread(&ctx2, utfound, pnp, ptp->pt_data, 
						       cr, rvalp, vbds, 0);

	prioctl_finish(&ctx2);

	*gotofallout = 1;

	return error;

#undef	CTX
}


STATIC int
prioctl_piocwstop(
	proc_t		*p,
	pioc_ctx_t	*ctx,
	caddr_t		cmaddr,
	struct prnode	*pnp,
	int		*gotofallout)
{
#define	CTX	(*ctx)
	int		error = 0;
	uthread_t	*ut;


	*gotofallout = 0;

	VPROC_PRWSTOP_THREADS(PROC_TO_VPROC(p), pnp, error);

	if (error) {
		*gotofallout = 1;	/* prunlock already done */

		return error;
	}

	p = pnp->pr_proc;

	ut = prchoosethread(p);

	if (cmaddr) {
		/*
		 * Return process status information.
		 */
		if (pnp->pr_pflags & PRINVAL)
			error = EAGAIN;
		else if (ut == NULL)
			error = ENOENT;
		else {
			error = prgetstatus(ut, 1, (void *)pcbuf);

			if (   error == 0
			    && xlate_copyout(pcbuf, cmaddr,
						  PRSTATUS_SIZE,
						  prstatus_to_32,
						  target_abi,
						  KERN_ABI, 1)) {
				error = EFAULT;
			}
		}
	}

	return error;

#undef	CTX
}

STATIC int
prioctl_piocrun(
	proc_t		*p,
	pioc_ctx_t	*ctx,
	caddr_t		cmaddr,
	struct prnode	*pnp)
{
	int		error = 0;
	int		ss;
	proc_proxy_t	*px;
	prrun_t		*prrun;
	prsetrun_t	prsr;


	prrun = prioctl_prrun(ctx, cmaddr);

	if (prrun == NULL)
		return EFAULT;
	
	px = &p->p_proxy;

	error = uthread_apply(px, UT_ID_NULL, prchkrun, NULL);

	if (error == 0 && (prrun->pr_flags & PRSTEP))
		error = uthread_apply(px, UT_ID_NULL, prchkrun_step, prrun);

	if (error == 0)
		prsetrun_proc(pnp->pr_proc, prrun);

	if (error == 0) {
		/*
		 * If all the threads are in a state suitable
		 * for PIOCRUN, then we may need to wait for just
		 * a little bit longer so that the js fields get 
		 * into a sane state. No problem in holding prlock
		 * for such a short duration.
		 */
		ss = prxy_stoplock(px);

		while (px->prxy_flags & PRXY_JSARMED) {
			jstop_wait(px, ss);
			ss = prxy_stoplock(px);
		}

		/*
		 * Any new threads created after this will not
		 * be marked to stop (see uthread_create)
		 */
		prxy_nflagclr(px, PRXY_JSTOPPED);

		prxy_stopunlock(px, ss);

		bzero(&prsr, sizeof(prsr));

		prsr.prsr_prrun = prrun;

		error = uthread_apply(px, UT_ID_NULL, prsetrun, &prsr);

		ASSERT(error == 0);
	}

	return error;
}


STATIC int
prioctl_piocgtrace(
	proc_t		*p,
	caddr_t		cmaddr)
{
	int		error = 0;
	int		n;
	k_sigset_t	ksigs;
	sigset_t	usigs;


	n = p_lock(p);

	ksigs = p->p_sigtrace;

	sigktou(&ksigs, &usigs);

	p_unlock(p, n);

	if (copyout(&usigs, cmaddr, sizeof usigs))
		error = EFAULT;

	return error;
}


STATIC int
prioctl_piocstrace(
	proc_t		*p,
	caddr_t		cmaddr)
{
	int		n;
	k_sigset_t	ksigs;
	sigset_t	usigs;


	if (copyin(cmaddr, &usigs, sizeof usigs))
		return EFAULT;

	sigutok(&usigs, &ksigs);

	sigdelset(&ksigs, SIGKILL);

	n = p_lock(p);

	ASSERT(p->p_trmasks != NULL);

	if (   !sigisempty(&ksigs)
	    || p->p_trmasks->pr_systrap
	    || !prisempty(&p->p_trmasks->pr_fltmask)) {
		p->p_flag |= SPROCTR;
	} else {
		p->p_flag &= ~SPROCTR;
	}

	p->p_sigtrace = ksigs;

	p_unlock(p, n);

	return 0;
}


STATIC int
prioctl_piockill(
	proc_t		*p,
	caddr_t		cmaddr,
	struct cred	*cr)
{
	int	error = 0;
	int	signo;


	if (copyin(cmaddr, (caddr_t) &signo, sizeof(int)))
		error = EFAULT;
	else if (signo <= 0 || signo >= NSIG)
		error = EINVAL;
	else {
		k_siginfo_t info;

		bzero(&info, sizeof(info));

		info.si_signo = signo;
		info.si_code  = SI_USER;
		info.si_pid   = current_pid();
		info.si_uid   = cr->cr_ruid;

		VPROC_SENDSIG(PROC_TO_VPROC(p), signo, SIG_HAVPERM,
						0, NULL, &info, error);
	}

	return error;
}


STATIC int
prioctl_piocunkill(
	proc_t		*p,
	caddr_t		cmaddr,
	uthread_t	*ut)
{
	int		error = 0;
	int		n;
	int		signo;
	sigqueue_t	*infop;


	if (copyin(cmaddr, (caddr_t) &signo, sizeof(int)))
		error = EFAULT;
	else if (signo <= 0 || signo >= NSIG || signo == SIGKILL)
		error = EINVAL;
	else {
		sigvec_t *sigvp = &p->p_sigvec;
		
		sigvec_lock(sigvp);

		n = ut_lock(ut);

		if (   (ut->ut_flags & UT_PTHREAD) == 0
		    && sigismember(&ut->ut_sigpend.s_sig, signo)) {

			sigdelset(&ut->ut_sigpend.s_sig, signo);

			infop = sigdeq(&ut->ut_sigpend, signo, sigvp);
		} else {
			sigdelset(&sigvp->sv_sigpend.s_sig, signo);

			infop = sigdeq(&sigvp->sv_sigpend, signo, sigvp);
		}
		
		/*
		 * Dequeue and discard first (if any) siginfo
		 * for the designated signal.  sigdeq() will
		 * repost the signal bit to ut->ut_sig if any
		 * siginfos for this signal remain.
		 */
		ut_unlock(ut, n);

		sigvec_unlock(sigvp);
		
		if (infop)
			sigqueue_free(infop);
	}

	return error;
}


STATIC int
prioctl_piocnice(
	proc_t		*p,
	caddr_t		cmaddr,
	struct cred	*cr,
	int		*rvalp)
{
	int	error = 0;
	int	n;
	vproc_t	*vpr;

	if (copyin(cmaddr, &n, sizeof(int)))
		return EFAULT;

	vpr = VPROC_LOOKUP_STATE(p->p_pid, ZYES);
	ASSERT(vpr != NULL);

	VPROC_SET_NICE(vpr, &n, cr, VNICE_INCR, error);

	VPROC_RELE(vpr);

	if (!error)
		*rvalp = n - NZERO;

	return error;
}


STATIC int
prioctl_piocgentry_exit(
	proc_t		*p,
	pioc_ctx_t	*ctx,
	caddr_t		cmaddr)
{
#define	CTX	(*ctx)
	int		n;
	sysset_t	prmask;

			/* copying them needs to be atomic */
	n = p_lock(p);

	ASSERT(p->p_trmasks != NULL);

	if (cmd == PIOCGENTRY) {
		prassignset(&prmask, &p->p_trmasks->pr_entrymask);
	} else {
		prassignset(&prmask, &p->p_trmasks->pr_exitmask);
	}

	p_unlock(p, n);

	if (copyout((caddr_t)&prmask, cmaddr, sizeof(prmask)))
		return EFAULT;

	return 0;

#undef	CTX
}


STATIC int
prioctl_piocsentry_exit(
	proc_t		*p,
	pioc_ctx_t	*ctx,
	caddr_t		cmaddr)
{
#define	CTX	(*ctx)
	int		emptymask;
	int		n;
	sysset_t	prmask;

	if (copyin(cmaddr, (caddr_t)&prmask, sizeof(prmask)))
		return EFAULT;

	emptymask = prisempty(&prmask);

	n = p_lock(p);

	ASSERT(p->p_trmasks != NULL);

	if (cmd == PIOCSENTRY) {
		if (emptymask)
			emptymask = prisempty(&p->p_trmasks->pr_exitmask);

		prassignset(&p->p_trmasks->pr_entrymask, &prmask);
	} else {
		if (emptymask)
			emptymask = prisempty(&p->p_trmasks->pr_entrymask);

		prassignset(&p->p_trmasks->pr_exitmask, &prmask);
	}

	if (!emptymask) {
		/*
		 * set stop-on-entry/exit bit mask
		 */
		p->p_trmasks->pr_systrap = 1;
		p->p_flag |= SPROCTR;
	} else {
		/*
		 * clear stop-on-entry/exit bit mask
		 */
		p->p_trmasks->pr_systrap = 0;

		if (  sigisempty(&p->p_sigtrace)
		    && prisempty(&p->p_trmasks->pr_fltmask))
			p->p_flag &= ~SPROCTR;
	}

	p_unlock(p, n);

	return 0;

#undef	CTX
}


/* ARGSUSED */
STATIC int
prioctl_piocset(
	proc_t		*p,
	pioc_ctx_t	*ctx,
	caddr_t		cmaddr)
{
#define	CTX	(*ctx)
	ulong_t	dbgflags;


#if _MIPS_SIM == _ABI64
	if (!ABI_IS_IRIX5_64(target_abi)) {
		app32_ulong_t	tmp;

		if (copyin(cmaddr, &tmp, sizeof(tmp)))
			return EFAULT;

		dbgflags = tmp;
	} else
#endif
	if (copyin(cmaddr, &dbgflags, sizeof(dbgflags)))
		return EFAULT;

	return prioctl_setflags(p, dbgflags);

#undef	CTX
}


/* ARGSUSED */
STATIC int
prioctl_piocreset(
	proc_t		*p,
	pioc_ctx_t	*ctx,
	caddr_t		cmaddr)
{
#define	CTX	(*ctx)
	ulong_t	dbgflags;


#if _MIPS_SIM == _ABI64
	if (!ABI_IS_IRIX5_64(target_abi)) {
		app32_ulong_t	tmp;

		if (copyin(cmaddr, &tmp, sizeof(tmp)))
			return EFAULT;

		dbgflags = tmp;
	} else
#endif
	if (copyin(cmaddr, &dbgflags, sizeof(dbgflags)))
		return EFAULT;

	return prioctl_resetflags(p, dbgflags);

#undef	CTX
}


STATIC int
prioctl_piocgetptimer(
	proc_t		*p,
	pioc_ctx_t	*ctx,
	caddr_t		cmaddr)
{
#define	CTX	(*ctx)
	uint	size;


	size = priface_sizeof_timespec_t() * MAX_PROCTIMER;

	(void)priface_prgetptimer(p, pcbuf);

	if (copyout((caddr_t)pcbuf, cmaddr, size))
		return EFAULT;

	return 0;

#undef	CTX
}


STATIC int
prioctl_piocaction(
	proc_t		*p,
	pioc_ctx_t	*ctx,
	caddr_t		cmaddr)
{
#define	CTX	(*ctx)
	union {
		sigaction_t sigact;
		irix5_sigaction_t i5_sigact;
	} sigact;
	register u_int	sig;
	register u_int	bufsize;


	bufsize = priface_sizeof_sigaction_t();

	for (sig = 1; sig < NSIG; sig++) {

		priface_prgetaction(p, sig, &sigact);

		if (priface_sigaction_copyout( (caddr_t)&sigact, cmaddr))
			return EFAULT;

		cmaddr += bufsize;
	}

	return 0;

#undef	CTX
}


STATIC int
prioctl_piocgfault(
	proc_t		*p,
	caddr_t		cmaddr)
{
	int		n;
	fltset_t	fltmask;

	ASSERT(p->p_trmasks != NULL);

	n = p_lock(p);

	prassignset(&fltmask, &p->p_trmasks->pr_fltmask);

	p_unlock(p, n);

	if (copyout((caddr_t) &fltmask, cmaddr, sizeof(fltmask)))
		return EFAULT;
	
	return 0;
}


STATIC int
prioctl_piocsfault(
	proc_t		*p,
	caddr_t		cmaddr)
{
	int		n;
	fltset_t	fltmask;

	if (copyin(cmaddr, (caddr_t) &fltmask, sizeof(fltmask)))
		return EFAULT;

	ASSERT(p->p_trmasks != NULL);

	n = p_lock(p);

	if (!prisempty(&fltmask))
		p->p_flag |= SPROCTR;
	else if (   sigisempty(&p->p_sigtrace)
		 && p->p_trmasks->pr_systrap == 0) {
		p->p_flag &= ~SPROCTR;
	}

	prassignset(&p->p_trmasks->pr_fltmask, &fltmask);

	p_unlock(p, n);

	return 0;
}


STATIC int
prioctl_pioccred(
	proc_t		*p,
	caddr_t		cmaddr)
{
	prcred_t	prcred;
	struct cred	*cp;


	cp = pcred_access(p);

	prcred.pr_euid    = cp->cr_uid;
	prcred.pr_ruid    = cp->cr_ruid;
	prcred.pr_suid    = cp->cr_suid;
	prcred.pr_egid    = cp->cr_gid;
	prcred.pr_rgid    = cp->cr_rgid;
	prcred.pr_sgid    = cp->cr_sgid;
	prcred.pr_ngroups = cp->cr_ngroups;

	pcred_unaccess(p);

	if (copyout((caddr_t) &prcred, cmaddr, sizeof(prcred)))
		return EFAULT;
	
	return 0;
}


prioctl_piocgroups(
	proc_t		*p,
	caddr_t		cmaddr)
{
	int		error = 0;
	int		n;
	struct cred	*cp;


	cp = pcred_access(p);

	n = MAX(cp->cr_ngroups, 1);

	if (copyout((caddr_t) &cp->cr_groups[0], cmaddr,
				    n * sizeof(cp->cr_groups[0]))) {
		error = EFAULT;
	}

	pcred_unaccess(p);

	return error;
}


STATIC int
prioctl_piocusage(
	proc_t		*p,
	pioc_ctx_t	*ctx,
	caddr_t		cmaddr)
{
#define	CTX	(*ctx)
	priface_prgetusage(p, pcbuf);

	if (priface_prusage_copyout((caddr_t) pcbuf, cmaddr))
		return EFAULT;
	
	return 0;

#undef	CTX
}


prioctl_piocacinfo(
	proc_t		*p,
	caddr_t		cmaddr)
{
	struct pracinfo		AI;
	struct proc_acct_s	acct;
	struct rusage		ru;
	vpagg_t			*vpag;
	

	/*
	 * All of these fields should have the same size regardless 
	 * of the user's ABI or kernel pointer size.
	 */
	AI.pr_version = PROC_ACCT_VERSION;
	AI.pr_flag    = 0;
	AI.pr_nice    = p->p_proxy.prxy_sched.prs_nice;
	AI.pr_sched   = IS_SPROC(&p->p_proxy) ? p->p_shaddr->s_sched : 0;
	AI.pr_btime   = p->p_start;
	AI.pr_etime   = lbolt - p->p_ticks;
	
	/*
	 * Pull out session-oriented info if available
	 */
	VPROC_GETVPAGG(PROC_TO_VPROC(p), &vpag);

	if (vpag) {
		AI.pr_prid = VPAG_GETPRID(vpag);
		AI.pr_ash  = VPAG_GETPAGGID(vpag);
	} else {
		AI.pr_prid = PRID_NONE;
		AI.pr_ash  = ASH_NONE;
	}

	VPROC_GET_ACCT_TIMERS(PROC_TO_VPROC(p), &AI.pr_timers);

	if (p->p_acflag & AFORK)
		AI.pr_flag |= AIF_FORK;
	if (p->p_acflag & ASU)
		AI.pr_flag |= AIF_SU;

	VPROC_GETRUSAGE(PROC_TO_VPROC(p), VRUSAGE_SELF, &ru);

	AI.pr_counts.ac_swaps  = (accum_t) ru.ru_nswap;
	AI.pr_counts.ac_br     = (accum_t) ru.ru_inblock;
	AI.pr_counts.ac_bw     = (accum_t) ru.ru_oublock;

	VPROC_GET_EXTACCT(PROC_TO_VPROC(p), &acct);

	AI.pr_counts.ac_mem    = (accum_t) acct.pr_mem;
	AI.pr_counts.ac_chr    = (accum_t) acct.pr_bread;
	AI.pr_counts.ac_chw    = (accum_t) acct.pr_bwrit;
	AI.pr_counts.ac_syscr  = (accum_t) acct.pr_syscr;
	AI.pr_counts.ac_syscw  = (accum_t) acct.pr_syscw;

	if (copyout((caddr_t) &AI, cmaddr, sizeof(struct pracinfo)))
		return EFAULT;
	
	return 0;
}


#ifdef	R10000

STATIC int
prioctl_piocenevctrs(
	proc_t		*p,
	caddr_t		cmaddr,
	int		*rvalp)
{
	int			error = 0;
	hwperf_profevctrarg_t 	*enable_evctr_args;
	pioc_hwarg_t		hwa;


	/*
	 * Acquire and start using the hardware counters.	
	 * This call is for normal usage of the counters
	 * and so make sure that the preload values for
	 * the counters end up being 0.
	 */
	if (! IS_R10000()) {
		*rvalp = -1;
		return ENODEV;
	}		       

	if (!priface_hwpcntrs_accessible(p)) {
		*rvalp = -1;
		return EPERM;
	}

	enable_evctr_args = kmem_alloc(sizeof(hwperf_profevctrarg_t),
								KM_SLEEP);
	if (copyin((void *)cmaddr, (void *)enable_evctr_args,
					   sizeof(hwperf_profevctrarg_t))) {
		error = EFAULT;
		*rvalp = -1;
	} else {
		hwa.arg   = (void *)enable_evctr_args;
		hwa.auxp  = (void *)NULL;
		hwa.rvalp = rvalp;

		error = uthread_apply(&p->p_proxy, UT_ID_NULL,
					piocenevctrsex, (void *)&hwa);
		if (error)
			(void) uthread_apply(&p->p_proxy, UT_ID_NULL,
						 piocrelevctrs, (void *)rvalp);
	}

	kmem_free(enable_evctr_args, sizeof(hwperf_profevctrarg_t));

	return error;
}


STATIC int
prioctl_piocenevctrsex(
	proc_t		*p,
	caddr_t		cmaddr,
	int		*rvalp)
{
	int			error = 0;
	hwperf_profevctrargex_t	args_ex;
	hwperf_profevctrarg_t 	*enable_evctr_args;
	hwperf_profevctraux_t 	*enable_evctr_aux;
	pioc_hwarg_t		hwa;


	/*
	 * Acquire and start using the hardware counters.
	 * Extended version.
	 * This allows caller to re-establish generation number
	 * and counter values via hwperf_profevctraux_t struct.
	 */
	if (! IS_R10000()) {
		*rvalp = -1;
		return ENODEV;
	}		       

	if (!priface_hwpcntrs_accessible(p)) {
		*rvalp = -1;
		return EPERM;
	}

	enable_evctr_args = kmem_alloc(sizeof(hwperf_profevctrarg_t),
								KM_SLEEP);
	enable_evctr_aux = kmem_alloc(sizeof(hwperf_profevctraux_t),
								KM_SLEEP);

	if (   copyin(cmaddr, (void *)&args_ex,
					   sizeof(hwperf_profevctrargex_t))
	    || copyin(args_ex.hwp_args, (void *)enable_evctr_args,
					   sizeof(hwperf_profevctrarg_t))
	    || copyin(args_ex.hwp_aux, (void *)enable_evctr_aux,
					   sizeof(hwperf_profevctraux_t))) {
		error = EFAULT;
		*rvalp = -1;
	} else {

		hwa.arg   = (void *)enable_evctr_args;
		hwa.auxp  = (void *)enable_evctr_aux;
		hwa.rvalp = rvalp;

		error = uthread_apply(&p->p_proxy, UT_ID_NULL,
						piocenevctrsex, &hwa);
		if (error)
			(void) uthread_apply(&p->p_proxy, UT_ID_NULL,
						 piocrelevctrs, (void *)rvalp);
	}

	kmem_free(enable_evctr_args, sizeof(hwperf_profevctrarg_t));
	kmem_free(enable_evctr_aux, sizeof(hwperf_profevctraux_t));

	return error;
}


STATIC int
prioctl_piocrelevctrs(
	proc_t		*p,
	int		*rvalp)
{
	int	error = 0;


	if (! IS_R10000()) {
		*rvalp = -1;
		return ENODEV;
	}		       

	if (!priface_hwpcntrs_accessible(p)) {
		*rvalp = -1;
		return EPERM;
	} else {
		*rvalp = 0;

		error = uthread_apply(&p->p_proxy, UT_ID_NULL,
						piocrelevctrs, (void *)rvalp);
	}

	return error;
}


STATIC int
prioctl_piocgetevctrs(
	proc_t		*p,
	caddr_t		cmaddr,
	int		*rvalp)
{
	int	error = 0;


	/*
	 * Read the counters, but don't
	 * read all the prusage information.
	 */
	if (! IS_R10000()) {
		*rvalp = -1;
		return ENODEV;
	}		       

	if (p->p_proxy.prxy_cpumon) {
		hwperf_cntr_t	cntrs;
		pioc_hwarg_t	hwa;

		error = hwperf_get_counters(p->p_proxy.prxy_cpumon,
							&cntrs, rvalp);

		hwa.arg   = (void *)&cntrs;
		hwa.rvalp = rvalp;

		*rvalp = -1;

		error = uthread_apply(&p->p_proxy, UT_ID_NULL,
						piocgetevctrs, (void *)&hwa);

		if (!error && copyout((void *)&cntrs, (void *)cmaddr,
					      sizeof(hwperf_cntr_t))) {
			error = EFAULT;
			*rvalp = -1;
		}
	} else {
		error = ESRCH;
		*rvalp = -1;
	}
	
	return error;
}


STATIC int
prioctl_piocgetprevctrs(
	int		*rvalp)
{
	if (! IS_R10000()) {
		*rvalp = -1;
		return ENODEV;
	}		       

	*rvalp = -1;

	return ENOSYS;
}


STATIC int
prioctl_piocgetevctrl(
	proc_t		*p,
	caddr_t		cmaddr,
	int		*rvalp)
{
	int			error = 0;
	hwperf_eventctrl_t 	evctrl;
	pioc_hwarg_t		hwa;


	/* 
	 * Get all the information about which
	 * events are being traced and the execution
	 * level for each event.
	 */
	if (! IS_R10000()) {
		*rvalp = -1;
		return ENODEV;
	}		       

	hwa.arg   = (void *)&evctrl;
	hwa.auxp  = (void *)NULL;
	hwa.rvalp = rvalp;

	error = uthread_apply(&p->p_proxy, UT_ID_NULL,
					piocgetevctrl, (void *)&hwa);

	if (!error && copyout((void *)&evctrl, (void *)cmaddr,
					sizeof(hwperf_eventctrl_t))) {
		*rvalp = -1;
		return EFAULT;
	}

	return error;
}


STATIC int
prioctl_piocgetevctrlex(
	proc_t		*p,
	caddr_t		cmaddr,
	int		*rvalp)
{
	int			error = 0;
	hwperf_getctrlex_arg_t	args_ex;
	hwperf_eventctrl_t 	evctrl;
	hwperf_eventctrlaux_t 	evctrl_aux;
	pioc_hwarg_t		hwa;

	/* 
	 * Get all the information about which
	 * events are being traced and the execution
	 * level for each event.  Extended version.
	 */
	if (! IS_R10000()) {
		*rvalp = -1;
		return ENODEV;
	}		       

	if (copyin((void *)cmaddr, (void *)&args_ex,
					sizeof(hwperf_getctrlex_arg_t))) {
		*rvalp = -1;
		return EFAULT;
	}

	hwa.arg   = (void *)&evctrl;
	hwa.auxp  = (void *)&evctrl_aux;
	hwa.rvalp = rvalp;

	error = uthread_apply(&p->p_proxy, UT_ID_NULL,
					piocgetevctrl, (void *)&hwa);

	if (error) {
		*rvalp = -1;
		return error;
	}

	if (copyout((void *)&evctrl, (void *)args_ex.hwp_evctrl,
					sizeof(hwperf_eventctrl_t))) {
		*rvalp = -1;
		return EFAULT;
	}

	if (copyout((void *)&evctrl_aux, (void *)args_ex.hwp_evctrlaux,
					sizeof(hwperf_eventctrlaux_t))) {
		*rvalp = -1;
		return EFAULT;
	}

	return error;
}


STATIC int
prioctl_piocsetevctrl(
	proc_t		*p,
	caddr_t		cmaddr,
	int		*rvalp)
{
	int			error = 0;
	hwperf_profevctrarg_t 	*evctr_args;
	pioc_hwarg_t		hwa;

	/* 
	 * After acquiring the counters, change
	 * all/some of the events being tracked and/or the
	 * mode the event is being counted in.
	 */
	if (! IS_R10000()) {
		*rvalp = -1;
		return ENODEV;
	}		       

	if (!priface_hwpcntrs_accessible(p)) {
		*rvalp = -1;
		return EPERM;
	}

	evctr_args = kmem_alloc(sizeof(hwperf_profevctrarg_t), KM_SLEEP);

	if (copyin(cmaddr, evctr_args, sizeof(hwperf_profevctrarg_t))) {
		error = EFAULT;
		*rvalp = -1;
	} else {
		hwa.arg   = (void *)evctr_args;
		hwa.rvalp = rvalp;
		
		error = uthread_apply(&p->p_proxy, UT_ID_NULL,
						piocsetevctrl, (void *)&hwa);
	}

	kmem_free(evctr_args, sizeof(hwperf_profevctrarg_t));

	return error;
}


STATIC int
prioctl_piocsaveccntrs(
	proc_t		*p,
	int		*rvalp)
{
	/*
	 * Set a flag in the child's cpu_mon_t
	 * so that the parent always gets the 
	 * child's counters when the child exits.
	 */
	if (! IS_R10000()) {
		*rvalp = -1;
		return ENODEV;
	}		       

	if (!priface_hwpcntrs_accessible(p)) {
		*rvalp = -1;
		return EPERM;
	}

	return uthread_apply(&p->p_proxy, UT_ID_NULL,
					piocsaveccntrs, (void *)rvalp);
}
#endif	/* R10000 */

#ifdef	SN0

STATIC int
prioctl_piocgetsn0refcntrs(
	pioc_ctx_t	*ctx,
	caddr_t		cmaddr,
	uthread_t	*ut)
{
#define	CTX	(*ctx)
	int			error = 0;
	sn0_refcnt_args_t	args;
	sn0_refcnt_buf_t*	outbuf;
	sn0_refcnt_buf_t	*temp_bufp;
	vasid_t			vasid;
	as_getrangeattrin_t	asattrin;
	pgcnt_t			npgs, i;
	pfn_t			pfn;
	size_t			pgsz;
	uvaddr_t		vaddr;

	
	if (!migr_refcnt_enabled())
		return EINVAL;

#if _MIPS_SIM == _ABI64
	if (!ABI_IS_IRIX5_64(target_abi)) {
		sn0_refcnt_args_32_t	temp_args;

		if (copyin((void *)cmaddr, (void *)&temp_args,
					   sizeof(sn0_refcnt_args_32_t))) {
			return EFAULT;
		}

		args.len   = (long)temp_args.len;
		args.vaddr = temp_args.vaddr;
		args.buf   = (sn0_refcnt_buf_t *)(__psint_t)temp_args.buf;
	} else
#endif
	if (copyin(cmaddr, &args, sizeof(sn0_refcnt_args_t)))
		return EFAULT;
	
	npgs = bytes_to_refcntpages(args.len);

	vaddr = (uvaddr_t)align_to_refcntpage(args.vaddr);

	outbuf = args.buf;

	if (as_lookup(ut->ut_asid, &vasid))
		return EINVAL;

	asattrin.as_vtop.as_ppfn  = &pfn;
	asattrin.as_vtop.as_ppgsz = &pgsz;
	asattrin.as_vtop.as_step  = 0;
	asattrin.as_vtop.as_shift = 0;
	asattrin.as_vtop.as_bits  = 0;

	temp_bufp = kmem_alloc(sizeof(sn0_refcnt_buf_t), KM_SLEEP);

	for (i = 0; i < npgs; i++, vaddr += NBPREFCNTP, outbuf++) {

		pfn = 0;

		temp_bufp->paddr = 0;

		error = VAS_GETRANGEATTR(vasid, vaddr, NBPREFCNTP,
						 AS_GET_VTOP, &asattrin, NULL);
		if (error)
			break;
		
		if (pfn) {
			temp_bufp->paddr = ctob(pfn)
					 + refcntpage_offset(vaddr);

			temp_bufp->cnodeid   = PFN_TO_CNODEID(pfn);
			temp_bufp->page_size = pgsz;

			migr_refcnt_read(temp_bufp);
		} else {
			temp_bufp->paddr     = 0;
			temp_bufp->cnodeid   = CNODEID_NONE;
			temp_bufp->page_size = 0;

			bzero(&temp_bufp->refcnt_set,
						sizeof(sn0_refcnt_set_t));
		}	

		if (copyout(temp_bufp, outbuf, sizeof(sn0_refcnt_buf_t))) {
			error = EFAULT;
			break;
		}
	}

	kmem_free(temp_bufp, sizeof(sn0_refcnt_buf_t));

	as_rele(vasid);

	return error;

#undef	CTX
}


STATIC int
prioctl_piocgetsn0extrefcntrs(
	pioc_ctx_t	*ctx,
	caddr_t		cmaddr,
	uthread_t	*ut)
{
#define	CTX	(*ctx)
	int			error = 0;
	sn0_refcnt_args_t	args;
	sn0_refcnt_buf_t*	outbuf;
	sn0_refcnt_buf_t	*temp_bufp;
	vasid_t			vasid;
	as_getrangeattrin_t	asattrin;
	pgcnt_t			npgs, i;
	pfn_t			pfn;
	size_t			pgsz;
	uvaddr_t		vaddr;
	
	if (!migr_refcnt_enabled())
		return EINVAL;

#if _MIPS_SIM == _ABI64
	if (!ABI_IS_IRIX5_64(target_abi)) {
		sn0_refcnt_args_32_t	temp_args;

		if (copyin((void *)cmaddr, (void *)&temp_args,
			   sizeof(sn0_refcnt_args_32_t))) {
			return EFAULT;
		}

		args.len   = (long)temp_args.len;
		args.vaddr = temp_args.vaddr;
		args.buf   = (sn0_refcnt_buf_t *)(__psint_t)temp_args.buf;
	} else
#endif
	if (copyin(cmaddr, &args, sizeof(sn0_refcnt_args_t)))
		return EFAULT;
	
	npgs = bytes_to_refcntpages(args.len);

	vaddr = (uvaddr_t)align_to_refcntpage(args.vaddr);

	outbuf = args.buf;

	if (as_lookup(ut->ut_asid, &vasid))
		return EINVAL;

	asattrin.as_vtop.as_ppfn  = &pfn;
	asattrin.as_vtop.as_ppgsz = &pgsz;
	asattrin.as_vtop.as_step  = 0;
	asattrin.as_vtop.as_shift = 0;
	asattrin.as_vtop.as_bits  = 0;

	temp_bufp = kmem_alloc(sizeof(sn0_refcnt_buf_t), KM_SLEEP);

	for (i = 0; i < npgs; i++, vaddr += NBPREFCNTP, outbuf++) {

		pfn = 0;

		temp_bufp->paddr = 0;

		error = VAS_GETRANGEATTR(vasid, vaddr, NBPREFCNTP,
					 AS_GET_VTOP, &asattrin, NULL);
		if (error)
			break;
		
		if (pfn) {
			temp_bufp->paddr = ctob(pfn)
					 + refcntpage_offset(vaddr);

			temp_bufp->cnodeid   = PFN_TO_CNODEID(pfn);
			temp_bufp->page_size = pgsz;

			migr_refcnt_read_extended(temp_bufp);
		} else {
			temp_bufp->paddr     = 0;
			temp_bufp->cnodeid   = CNODEID_NONE;
			temp_bufp->page_size = 0;

			bzero(&temp_bufp->refcnt_set,
						sizeof(sn0_refcnt_set_t));
		}	

		if (copyout(temp_bufp, outbuf, sizeof(sn0_refcnt_buf_t))) {
			error = EFAULT;
			break;
		}
	}

	kmem_free(temp_bufp, sizeof(sn0_refcnt_buf_t));

	as_rele(vasid);

	return error;

#undef	CTX
}
#endif	/* SN0 */


STATIC int
prioctl_piocgetnode(
	proc_t		*p,
	caddr_t		cmaddr,
	struct cred	*cr)
{
	int		error = 0;
	prinodeinfo_t	pi;
	vattr_t		va;
	vnode_t		*vp;

	if (copyin((void *)cmaddr, (void *)&pi, sizeof(pi)))
		return EFAULT;

	if (error = fdt_lookup_fdinproc(p, pi.pi_fd, &vp))
		return error;

	ASSERT(vp);

	va.va_mask = AT_FSID|AT_NODEID|AT_GENCOUNT;

	VOP_GETATTR(vp, &va, 0, cr, error);

	VN_RELE(vp);

	if (error)
		return error;

	pi.pi_dev  = va.va_fsid;
	pi.pi_inum = va.va_nodeid;
	pi.pi_gen  = va.va_gencount;

	if (copyout((void *)&pi, (void *)cmaddr, sizeof(pi)))
		return EFAULT;
	
	return 0;
}


/*
 * Control operations (lots).
 */
int
prioctl(
	bhv_desc_t *bdp,
	int cmdarg,
	void *arg,
	int flag,
	struct cred *cr,
	int *rvalp,
        struct vopbd *vbds)
{
	int n;
	register caddr_t cmaddr = (caddr_t)arg;
	register proc_t *p;
	register uthread_t *ut;
	vnode_t *vp = BHV_TO_VNODE(bdp);
	struct prnode *pnp = BHVTOPRNODE(bdp);
	int error;
	int attr;
	pioc_ctx_t ctx;
#define CTX     ctx

	if (vp->v_type == VDIR)
		return EINVAL;

	/*
	 * Fail all ioctls if a security violation has occurred.
	 * This test alone is not sufficient; it must be repeated
	 * after any sleep(), including the sleep() in prlock().
	 */
	if (pnp->pr_pflags & PRINVAL)
		return EAGAIN;

	/*
         * Prepare to handle this request.
	 */
	ctx.pc_cmd = cmdarg;
	error = prioctl_prepare(&ctx, pnp, cmaddr, flag);
	if (error) {
		return (error);
        }
	attr = ctx.pc_attr;


	error = prlock(pnp, attr & PIOCA_ZOMBIE ? ZYES : ZNO, PRNONULL);
	if (error) {
		prioctl_finish(&ctx);

		return error;
	}

        /*
	 * Fail if a security violation invalidated the vnode while
	 * we were waiting.
	 */
	if (pnp->pr_pflags & PRINVAL) {
		error = EAGAIN;
		goto done;
	}
        
	p = pnp->pr_proc;

	/*
	 * Fail if the process had or has privilege and caller does not.
	 */
	if (   (pnp->pr_type != PR_PSINFO)
	    && (p->p_flag & SPRPROTECT)
	    && !cap_able_cred(cr, CAP_PROC_MGT)) {
		error = EAGAIN;
		goto done;
	}

	/*
	 * XXX	Does this thread get locked?  In prlock?
	 * XXX	Virtualize proc/thread
	 */
	ut = prchoosethread(pnp->pr_proc);

	/*
         * In the following section, we deal with a number of situations
         * in whcih the procfs ioctl will not be dealt with in the 
         * main prioctl switch immediately below this if-else chain.
         *
         * Of particular note are those situations in which we take 
         * an ioctl nominally directed at a process and treat it as
         * one directed at one of threads within that process.  
         *
         * For ioctls marked PIOCA_MTHREAD, we select one thread
         * from the process and call prioctl_thread to perform the
         * ioctl as if it were directed at that thread, with a
         * parameter to prioctl_thread indicating that this originally
         * directed at the process, thus allow prioctl_thread to
         * slightly vary the handling on this basis.  The ioctls
         * marked with PIOCA_MTHREAD are those in which our interest
         * in allowing old non-thread-aware tools to keep working in
         * some fashion outweighs the problems in randomly selecting
         * a thread and threby giving incomplete information.
         *
         * For ioctls without PIOCA_PROC set, we call prioctl_thread
         * only if the process in question consists of a single
         * thread.  This allows us to treat the request unambiguously
         * as one designating the only thread.  If there are multiple
         * threads, such ioctls return EINVAL since they are inherently
         * ambiguous when issued directly.  Note that ioctls for 
         * which both PIOCA_PROC and PIOCA_THREAD are set are handled
         * in the main switch statement below.  Only when they are
         * issued as the target of a PIOCTHREAD is the thread-specific
         * handling performed.
	 */
	if ((attr & PIOCA_VALID) == 0) {
		error = EINVAL;
		goto done;
	}
	else if (attr & PIOCA_MTHREAD) {
	        ASSERT((attr & PIOCA_PROC) == 0);
		error = prioctl_thread(&ctx, ut, pnp, cmaddr, 
				       cr, rvalp, vbds, 1);
		prioctl_finish(&ctx);

		return error;
	}
	else if ((attr & PIOCA_PROC) == 0) {
	        ASSERT(attr & PIOCA_THREAD);
	        if (ut->ut_next == NULL) {
			error = prioctl_thread(&ctx, ut, pnp, cmaddr, 
					       cr, rvalp, vbds, 1);
			prioctl_finish(&ctx);

			return error;
		}
		else {
		  	error = EINVAL;
			goto done;
		}
	}
#ifdef	CKPT
	else if (attr & PIOCA_CKPT) {
		error = ckpt_prioctl(cmd, pnp, cmaddr, cr,
				     flag, target_abi, rvalp);
		goto done;
	}
#endif
        
	switch (cmd) {
#if     DEBUG
	default:
		panic("bad prioctl handling");
#endif 
        case PIOCTHREAD:		/* issue ioctl on specified thread. */
	{
		int	gotofallout = 0;

		error = prioctl_piocthread(p, &ctx, cmaddr, flag,
						pnp, cr, rvalp, vbds,
						&gotofallout);
		if (gotofallout)
			goto fallout;
		break;
	}
	/*
	 * For PIOC{STOP,WSTOP} directed at a process, all threads are stopped.
         * When a prstatus address is given, the thread-specific data within 
         * it, will reflect a thread selected the kernel.  The user should 
         * not depend on thread-specific data within the prstatus.  It will
         * not be reliable.
	 */
	case PIOCSTOP:		/* stop process thread(s) from running */
	{

	        VPROC_PRSTOP_THREADS(PROC_TO_VPROC(p));
		/* FALLTHROUGH */
	}

	case PIOCWSTOP:		/* wait for process thread(s) to stop */
	{
		int	gotofallout = 0;

		error = prioctl_piocwstop(p, &ctx, cmaddr, pnp, &gotofallout);

		if (gotofallout)
			goto fallout;
		break;
	}

	case PIOCRUN:		/* make process runnable */
		error = prioctl_piocrun(p, &ctx, cmaddr, pnp);
		break;

	case PIOCGTRACE:	/* get signal trace mask */
		error = prioctl_piocgtrace(p, cmaddr);
		break;

	case PIOCSTRACE:	/* set signal trace mask */
		error = prioctl_piocstrace(p, cmaddr);
		break;

	case PIOCKILL:		/* send signal */
		error = prioctl_piockill(p, cmaddr, cr);
		break;

	case PIOCUNKILL:	/* delete a signal */
		error = prioctl_piocunkill(p, cmaddr, ut);
		break;

	case PIOCNICE:		/* set nice priority */
		error = prioctl_piocnice(p, cmaddr, cr, rvalp);
		break;

	case PIOCGENTRY:	/* get syscall entry bit mask */
	case PIOCGEXIT:		/* get syscall exit bit mask */
		error = prioctl_piocgentry_exit(p, &ctx, cmaddr);
		break;

	case PIOCSENTRY:	/* set syscall entry bit mask */
	case PIOCSEXIT:		/* set syscall exit bit mask */
		error = prioctl_piocsentry_exit(p, &ctx, cmaddr);
		break;

	case PIOCSRLC:		/* set running on last /proc close */
		n = p_lock(p);
		p->p_dbgflags |= KPRRLC;
		p_unlock(p, n);
		break;

	case PIOCRRLC:		/* reset run-on-last-close flag */
		n = p_lock(p);
		p->p_dbgflags &= ~KPRRLC;
		p_unlock(p, n);
		break;

	case PIOCSFORK:		/* set inherit-on-fork flag */
		n = p_lock(p);
		p->p_dbgflags |= KPRIFORK;
		p_unlock(p, n);
		break;

	case PIOCRFORK:		/* reset inherit-on-fork flag */
		n = p_lock(p);
		p->p_dbgflags &= ~KPRIFORK;
		p_unlock(p, n);
		break;

        case PIOCSET:
		error = prioctl_piocset(p, &ctx, cmaddr);
		break;

        case PIOCRESET: 
		error = prioctl_piocreset(p, &ctx, cmaddr);
		break;

	case PIOCGETPTIMER:	/* get process timers */
		error = prioctl_piocgetptimer(p, &ctx, cmaddr);
		break;

	case PIOCMAXSIG:	/* get maximum signal number */
		n = MAXSIG;

		if (copyout(&n, cmaddr, sizeof(int)))
			error = EFAULT;
		break;

	case PIOCACTION:	/* get signal action structures */
		error = prioctl_piocaction(p, &ctx, cmaddr);
		break;

	case PIOCGFAULT:	/* get mask of traced faults */
		error = prioctl_piocgfault(p, cmaddr);
		break;

	case PIOCSFAULT:	/* set mask of traced faults */
		error = prioctl_piocsfault(p, cmaddr);
		break;

	case PIOCCFAULT:	/* clear current fault */
		error = uthread_apply(&p->p_proxy, UT_ID_NULL,
				      pioccfault, NULL);
		break;

	case PIOCCRED:		/* get process credentials */
		error = prioctl_pioccred(p, cmaddr);
		break;

	case PIOCGROUPS:	/* get supplementary groups */
		error = prioctl_piocgroups(p, cmaddr);
		break;

        case PIOCUSAGE:
		error = prioctl_piocusage(p, &ctx, cmaddr);
		break;

	case PIOCACINFO:
		error = prioctl_piocacinfo(p, cmaddr);
		break;

#ifdef R10000
	case PIOCENEVCTRS:
		error = prioctl_piocenevctrs(p, cmaddr, rvalp);
		break;

	case PIOCENEVCTRSEX:
		error = prioctl_piocenevctrsex(p, cmaddr, rvalp);
		break;

	case PIOCRELEVCTRS:
		error = prioctl_piocrelevctrs(p, rvalp);
		break;

	case PIOCGETEVCTRS:
		error = prioctl_piocgetevctrs(p, cmaddr, rvalp);
		break;

	case PIOCGETPREVCTRS:
		error = prioctl_piocgetprevctrs(rvalp);
		break;

	case PIOCGETEVCTRL:
		error = prioctl_piocgetevctrl(p, cmaddr, rvalp);
		break;

	case PIOCGETEVCTRLEX:
		error = prioctl_piocgetevctrlex(p, cmaddr, rvalp);
		break;

	case PIOCSETEVCTRL:
		error = prioctl_piocsetevctrl(p, cmaddr, rvalp);
		break;

	case PIOCSAVECCNTRS:
		error = prioctl_piocsaveccntrs(p, rvalp);
		break;

#else /* !R10000 */

	case PIOCENEVCTRS:
	case PIOCENEVCTRSEX:
	case PIOCRELEVCTRS:
	case PIOCGETEVCTRS:
	case PIOCGETPREVCTRS:
	case PIOCGETEVCTRL:
	case PIOCGETEVCTRLEX:
	case PIOCSETEVCTRL:
	case PIOCSAVECCNTRS:
		error = ENOTSUP;
		*rvalp = -1;
		break;

#endif /* !R10000 */

#ifdef SN0
	case PIOCGETSN0REFCNTRS:
		error = prioctl_piocgetsn0refcntrs(&ctx, cmaddr, ut);
		break;

	case PIOCGETSN0EXTREFCNTRS:
		error = prioctl_piocgetsn0extrefcntrs(&ctx, cmaddr, ut);
		break;
#endif /* SN0 */

	case PIOCGETINODE:
		error = prioctl_piocgetnode(p, cmaddr, cr);
		break;

	case PIOCPTIMERS:	/* enable/disable process timers */
	{
		if (copyin(cmaddr, (caddr_t) &n, sizeof(int))) {
			error = EFAULT;
			break;
		}
		/* XXX currently unimplemented */
		break;
	}

	}
done:
	prunlock(pnp);
fallout:
	prioctl_finish(&ctx);

	return error;

#undef  CTX
}

/* ARGSUSED */
static int 
prioctl_thread(
	pioc_ctx_t	*ctxp,
        uthread_t       *ut,
        struct prnode   *pnp,
        caddr_t         cmaddr,
	struct cred     *cr,
        int             *rvalp,
        struct vopbd    *vbds,
        int             for_proc)
{
        int     	error = 0;
	vasid_t         vasid;
        int     	n;
        int             signo;
	
#define CTX	(*ctxp)

#ifdef CKPT
	if (ctxp->pc_attr & PIOCA_CKPT) {

		if (ckpt_prioctl_thread(ctxp, ut, pnp, cmaddr,
					cr, rvalp, &error) == 0)
			prunlock(pnp);
		/*
		 * else already unlocked
		 */
		return (error);
	}
#endif

        switch (cmd) {
#if     DEBUG
	default:
		panic("bad prioctl handling");
#endif 
	case PIOCGREG:		/* get general registers */
	{
		priface_prgetregs(ut, pcbuf);
		if (priface_gregset_copyout((caddr_t) pcbuf, cmaddr))
			error = EFAULT;
		break;
	}

	case PIOCSREG:		/* set general registers */
	{
		if (!isstopped(ut))
			error = EBUSY;
		else if (priface_gregset_copyin(cmaddr, (caddr_t) pcbuf))
			error = EFAULT;
		else
			priface_prsetregs(ut, pcbuf);
		break;
	}

	case PIOCGFPREG:	/* get floating-point registers */
	{
		if (prhasfp()) {
			if ((error = priface_prgetfpregs(ut, pcbuf)) != 0)
				break;
			if (priface_fpregset_copyout((caddr_t)pcbuf, cmaddr))
				error = EFAULT;
		} else
			error = EINVAL;	/* No FP support */
		break;
	}

	case PIOCSFPREG:	/* set floating-point registers */
	{
		if (!isstopped(ut))
			error = EBUSY;
		else if (prhasfp()) {
			if (priface_fpregset_copyin(cmaddr, (caddr_t) pcbuf))
				error = EFAULT;
			else
				error = priface_prsetfpregs(ut, pcbuf);
		} else
			error = EINVAL;	/* No FP support */
		break;
	}

	case PIOCSTATUS:	/* get process status */
	{
		if ((error = prgetstatus(ut, for_proc, (void *)pcbuf)) != 0)
			break;
		if (xlate_copyout(pcbuf, cmaddr, PRSTATUS_SIZE,
				  prstatus_to_32, target_abi,
				  KERN_ABI, 1))
			error = EFAULT;
		break;
	}

	case PIOCPSINFO:	/* get ps(1) information */
	{
		if ((error = priface_prgetpsinfo(ut, pnp, for_proc, pcbuf))!= 0)
			break;
		if (priface_prpsinfo_copyout((caddr_t)pcbuf, cmaddr))
			error = EFAULT;
		break;
	}

	case PIOCSTOP:		/* stop process thread(s) from running */
	        prstop_ut(ut);
		/* FALLTHROUGH */

	case PIOCWSTOP:		/* wait for process thread(s) to stop */
	{
	        error = prwstop_ut(ut, pnp);

		if (error)
			return (error); /* prunlock already done */

		if (cmaddr) {
			/*
			 * Return process status information.
			 */
			if (pnp->pr_pflags & PRINVAL)
				error = EAGAIN;
			else {
				error = prgetstatus(ut, 0, (void *)pcbuf);
				if (error == 0 &&
				    xlate_copyout(pcbuf, cmaddr,
						  PRSTATUS_SIZE,
						  prstatus_to_32,
						  target_abi,
						  KERN_ABI, 1))
					error = EFAULT;
			}
		}
		break;
	}
	case PIOCRUN:		/* make process runnable */
	{
		prrun_t *prrun = prioctl_prrun(ctxp, cmaddr);
		int s;
			
		if (prrun == NULL) {
		        error = EFAULT;
			break;
		}
		s = ut_lock(ut);
		error = prchkrun_sub(ut);
		if (error == 0 && prrun->pr_flags)
			error = prsetrun_opts(ut, prrun, s, 0, 0);
		if (error == 0) 
                        prsetrun_final(ut);
		ut_unlock(ut, s);
		break;
	}

	case PIOCSSIG:		/* set current signal */
	{
		if (cmaddr) {
			if (copyin(cmaddr, (caddr_t) &signo, sizeof(int))) {
				error = EFAULT;
				break;
			}
			/* Check both limits */
			if (signo < 0 || signo >= NSIG) {
				error = EINVAL;
				break;
			}
		} else {
			/* else cancel current signal */
			signo = 0;
		}

		sigsetcur(ut, signo, NULL, 1);

		n = ut_lock(ut);

		/* Implement svr4 semantics - svr4 setruns sleeping
		 * (and stopped if SIGKILL) procs.
		 */
		if (signo && thread_interruptible(UT_TO_KT(ut)))
			setrun(UT_TO_KT(ut));
		else if (signo == SIGKILL && ut->ut_flags & UT_STOP)
			unstop(ut);

		ut_unlock(ut, n);
		break;
	}

	case PIOCOPENM:		/* open mapped object for reading */
	{
		vnode_t *xvp = NULL;
		caddr_t va;
		as_getvaddrattr_t asattr;

		/* grab address if they passed one */
		if (cmaddr) {
#if _MIPS_SIM == _ABI64
			if (!ABI_IS_IRIX5_64(target_abi)) {
				app32_ptr_t tmp;
				if (copyin(cmaddr, &tmp, sizeof(tmp))) {
					error = EFAULT;
					break;
				}
				va = (caddr_t)(__psint_t)tmp;
			} else
#endif
			if (copyin(cmaddr, &va, sizeof(va))) {
				error = EFAULT;
				break;
			}
		}

		if (as_lookup(ut->ut_asid, &vasid)) {
			error = EINVAL;
			break;
		}
		VAS_LOCK(vasid, AS_SHARED);

		if (cmaddr) {
			if (VAS_GETVADDRATTR(vasid, AS_VNODE|AS_LOADVNODE|AS_FLAGS|AS_CKPTMAP,
							va, &asattr) == 0) {
				if ((asattr.as_flags & AS_PHYS) == 0) {
					xvp = asattr.as_vp;

					/*
			:		 * vp will be null if we loaded region
					 * using loadreg().
					 */
					if (xvp == NULL)
						xvp = asattr.as_loadvp;
#ifdef _VCE_AVOIDANCE
					/* don't use a VCE avoidance vnode */
					if (xvp && xvp->v_type == VNON)
						xvp = NULL;
#endif
				}
			}
		} else {
			/* protect p_exec against races w/exec */
			mrlock(&UT_TO_PROC(ut)->p_who, MR_ACCESS, PZERO);
			xvp = UT_TO_PROC(ut)->p_exec;
		}

#ifdef _MTEXT_VFS
	/* don't use an mtext vnode; use the real one behind it */
	if (xvp != NULL &&
	    IS_MTEXT_VNODE(xvp))
		xvp = mtext_real_vnode(xvp);
#endif /*_MTEXT_VFS */

		if (xvp == NULL) {
			error = EINVAL;
		} else {
			VN_HOLD(xvp);

			VOP_ACCESS(xvp, VREAD, cr, error);
			if (error) {
				VN_RELE(xvp);
			} else {
				vbds->vopbd_req = VOPBDR_ASSIGN;
				vbds->vopbd_parm.vopbdp_assign.vopbda_vp = xvp;
#ifdef CKPT
				vbds->vopbd_parm.vopbdp_assign.vopbda_cpi = 
				        cmaddr ? asattr.as_ckpt.as_ckptinfo : -1;
#endif
			}
		}

		if (!cmaddr) 
			mrunlock(&UT_TO_PROC(ut)->p_who);

		VAS_UNLOCK(vasid);
		as_rele(vasid);
		break;
	}

	case PIOCNMAP:		/* get number of memory mappings */
	{
		if (as_lookup(ut->ut_asid, &vasid)) {
			error = EINVAL;
			break;
		}
		VAS_LOCK(vasid, AS_SHARED);
		n = prnsegs(vasid);
		VAS_UNLOCK(vasid);
		as_rele(vasid);
		if (copyout(&n, cmaddr, sizeof n))
			error = EFAULT;
		break;
	}

	case PIOCMAP:		/* get memory map information */
	{
		/*
		 * Determine the number of segments and allocate storage
		 * to hold the array of prmap structures.  In addition
		 * to the "real" segments we need space for the zero-filled 
		 * entry which terminates the list.
		 */
		if (as_lookup(ut->ut_asid, &vasid)) {
			error = EINVAL;
			break;
		}
		error = prgetmap(cmaddr, target_abi, vasid);
		as_rele(vasid);
		break;
	}

	case PIOCMAP_SGI:	/* get extended memory map information */
	{
		/*
		 * Gather the segment data.  Set the return value equal
		 * to the number of segments written into the user's buffer
		 * minus the NULL end segment.
		 */
		if (as_lookup(ut->ut_asid, &vasid)) {
			error = EINVAL;
			break;
		}
		error = prgetmap_sgi(cmaddr, &n, target_abi, vasid);
		as_rele(vasid);
		*rvalp = n;
		break;
	}

	case PIOCPGD_SGI:	/* get page table information */
	{
		register char *xbuf;
		register int bufsize;

		/*
		 * Determine the number of pages in region and allocate
		 * storage to hold the array of page flags.
		 */
		if (as_lookup(ut->ut_asid, &vasid)) {
			error = EINVAL;
			break;
		}
		VAS_LOCK(vasid, AS_EXCL);
		bufsize = priface_prgetpgd_sgi(cmaddr, vasid, NULL);
		if (bufsize < 0) {
			error = -bufsize;
			VAS_UNLOCK(vasid);
			as_rele(vasid);
			break;
		}
		xbuf = (char *) kmem_zalloc(bufsize, KM_SLEEP);
		priface_prgetpgd_sgi(cmaddr, vasid, xbuf);
		VAS_UNLOCK(vasid);
		as_rele(vasid);
		if (copyout(xbuf, cmaddr, bufsize))
			error = EFAULT;
		kmem_free(xbuf, bufsize);
		break;
	}


	case PIOCNWATCH:
		if (copyout((caddr_t) &maxwatchpoints, cmaddr, sizeof(int)))
			error = EFAULT;
		break;

	case PIOCSWATCH:
	{
		prwatch_t wp;

		if (COPYIN_XLATE(cmaddr, &wp, sizeof(prwatch_t),
				 irix5_to_prwatch, target_abi, 1)) {
			error = EFAULT;
			break;
		}
		if (wp.pr_size == 0) {
			/* disable it */
			error = deletewatch(ut, wp.pr_vaddr);
		} else {
			ulong rmode = 0;
			/*
			 * according to the spec -
			 * ignore types we don't understand
			 */
			if (wp.pr_wflags & MA_WRITE)
				rmode |= W_WRITE;
			if (wp.pr_wflags & MA_READ)
				rmode |= W_READ;
			if (wp.pr_wflags & MA_EXEC)
				rmode |= W_EXEC;
			if (rmode == 0)
				break;
			error = addwatch(ut, wp.pr_vaddr, wp.pr_size, rmode);
		}
		break;
	}

	case PIOCGWATCH:
	{
		error = getwatch(ut, priface_watchcopy, cmaddr, rvalp);

		if (error == 0) {
			union {
				prwatch_t zwp;
				irix5_prwatch_t i5_zwp;
			} zwp;
			register int i;
			register u_int size;

			/* zero out rest */
			bzero(&zwp, sizeof(zwp));
			size = priface_sizeof_prwatch_t();
			cmaddr += *rvalp * size;

			for (i = 0; i < maxwatchpoints - *rvalp; i++) {
				if (copyout((caddr_t) &zwp, cmaddr, size)) {
					error = EFAULT;
					break;
				}
				cmaddr += size;
			}
		}
		break;
	}

	case PIOCTLBMISS:		/* turn utlbmiss counting on/off */
	{
		utas_t *utas;
		int	do_tlb_flush;


		if (copyin(cmaddr, (caddr_t) &signo, sizeof(int))) {
			error = EFAULT;
			break;
		}
		utas = &ut->ut_as;

		/*
		 * For now, only utlbmiss counting can be enabled/disabled.
		 * If any other utlbmiss states become available to user
		 * control, manage cmaddr as a bitmask.
		 */
		if (signo == TLB_STD) {
			n = utas_lock(utas);
			utas->utas_utlbmissswtch &= ~UTLBMISS_COUNT;
			if (utas->utas_utlbmissswtch == 0)
				utas->utas_flag &= ~UTAS_TLBMISS;
			if (utas == &curuthread->ut_as)
				utlbmiss_reset();
			utas_unlock(utas, n);
		} else if (signo == TLB_COUNT) {
			n = utas_lock(utas);
			utas->utas_utlbmissswtch |= UTLBMISS_COUNT;
			utas->utas_flag |= UTAS_TLBMISS;

			/*
			 * If we are switching to a large page
			 * tlbmiss handler, do the flush here.
			 */
			do_tlb_flush = utas->utas_flag & UTAS_LPG_TLBMISS_SWTCH;
			utas->utas_flag &= ~UTAS_LPG_TLBMISS_SWTCH;
			utas_unlock(utas, n);
			if ((utas == &curuthread->ut_as) &&
			    (private.p_utlbmissswtch != UTLBMISS_COUNT))
				utlbmiss_resume(utas);
			if (do_tlb_flush)
				flushtlb_current();
		} else {
			error = EINVAL;
		}
		break;
	}

	case PIOCPTIMERS:
	{
		int set;
		if (copyin(cmaddr, (caddr_t) &set, sizeof(int))) {
			error = EFAULT;
			break;
		}
		/* XXX currently unimplemented */
		break;
	}

	case PIOCGUTID:
	{
		tid_t	tid = ut->ut_id;
    
	        if (copyout((caddr_t) &tid, cmaddr, sizeof(tid))) {
		        error = EFAULT;
			break;
		}
		*rvalp = 1;
		break;
	}

	case PIOCGHOLD:		/* get signal-hold mask */
	{
		k_sigset_t ksigs;
		sigset_t usigs;

		n = ut_lock(ut);
		ksigs = *ut->ut_sighold;
		sigktou(&ksigs, &usigs);
		ut_unlock(ut, n);
		if (copyout(&usigs, cmaddr, sizeof usigs))
			error = EFAULT;
		break;
	}

	case PIOCSHOLD:		/* set signal-hold mask */
	{
		k_sigset_t ksigs;
		sigset_t usigs;
		sigvec_t *sigvp = &UT_TO_PROC(ut)->p_sigvec;

		if (copyin(cmaddr, &usigs, sizeof usigs)) {
			error = EFAULT;
			break;
		}

		sigutok(&usigs, &ksigs);
		sigdiffset(&ksigs, &cantmask);

		sigvec_lock(sigvp);
		n = ut_lock(ut);
		*ut->ut_sighold = ksigs;
		if (thread_interruptible(UT_TO_KT(ut))
		    && fsig(ut, sigvp, SIG_NOSTOPDEFAULT))
		        setrun(UT_TO_KT(ut));

		ut_unlock(ut, n);
		sigvec_unlock(sigvp);
		break;
	}

	case PIOCUNKILL:	/* delete a signal */
	{
		sigqueue_t *infop;

		if (copyin(cmaddr, (caddr_t) &signo, sizeof(int)))
			error = EFAULT;
		else if (signo <= 0 || signo >= NSIG || signo == SIGKILL)
			error = EINVAL;
		else {
			sigvec_t *sigvp = &UT_TO_PROC(ut)->p_sigvec;
			
			sigvec_lock(sigvp);
			n = ut_lock(ut);

			sigdelset(&ut->ut_sigpend.s_sig, signo);
			infop = sigdeq(&ut->ut_sigpend, signo, sigvp);
			
			/*
			 * Dequeue and discard first (if any) siginfo
			 * for the designated signal.  sigdeq() will
			 * repost the signal bit to ut->ut_sig if any
			 * siginfos for this signal remain.
			 */

			ut_unlock(ut, n);
			sigvec_unlock(sigvp);
			
			if (infop)
				sigqueue_free(infop);
		}
		break;
	}

	case PIOCCFAULT:	/* clear current fault */
	{
		error = pioccfault(ut, NULL);
		break;
	}

	case PIOCREAD:	/* read from target address space */
	{
		prio_t	prio;
	
		if (COPYIN_XLATE(cmaddr, &prio, sizeof(prio_t),
				 irix5_to_prio, target_abi, 1)) {
			error = EFAULT;
			break;
		}
		error = prthreadio(pnp->pr_proc, ut, UIO_READ, &prio);
		break;
	}
		

#undef  CTX
	}
	prunlock(pnp);
	return (error);
}

/*
 * Return relevant attributes of individual ioctl commands
 */
static int
prioctl_attr(int cmdval)
{
	switch (cmdval) {
	case PIOCACINFO:
		return (PIOCA_VALIDP | PIOCA_PSINFO);

	case PIOCUSAGE:
	case PIOCTHREAD:
	case PIOCGETPTIMER:
		return (PIOCA_VALIDP | PIOCA_PSINFO | PIOCA_GETBUF);

	case PIOCPSINFO:
		return (PIOCA_VALIDMT | PIOCA_PSINFO | PIOCA_GETBUF | 
			PIOCA_ZOMBIE);

	case PIOCCRED:
		return (PIOCA_VALIDP | PIOCA_PSINFO | PIOCA_ZOMBIE);

	case PIOCSTOP:
		return (PIOCA_VALIDPT | PIOCA_WRITE | PIOCA_CGETBUF | 
			PIOCA_PRGS);

	case PIOCWSTOP:
		return (PIOCA_VALIDPT | PIOCA_CGETBUF | PIOCA_PRGS);

	case PIOCRUN:
		return (PIOCA_VALIDPT | PIOCA_WRITE | PIOCA_GETBUF);

	case PIOCUNKILL:
	case PIOCCFAULT:
		return (PIOCA_VALIDPT | PIOCA_WRITE);

	case PIOCSREG:
	case PIOCSFPREG:
		return (PIOCA_VALIDT | PIOCA_WRITE | PIOCA_GETBUF);

	case PIOCSTATUS:
		return (PIOCA_VALIDMT | PIOCA_GETBUF | PIOCA_PRGS);

	case PIOCGREG:
	case PIOCGFPREG:
		return (PIOCA_VALIDT | PIOCA_GETBUF);

	case PIOCSSIG:
	case PIOCSHOLD:
		return (PIOCA_VALIDT | PIOCA_WRITE);

	case PIOCSWATCH:
	case PIOCTLBMISS:
		return (PIOCA_VALIDMT | PIOCA_WRITE);

	case PIOCNMAP:	
	case PIOCMAP:	
	case PIOCMAP_SGI:
	case PIOCPGD_SGI:
	case PIOCOPENM:
	case PIOCNWATCH:
	case PIOCGWATCH:
	case PIOCREAD:
		return (PIOCA_VALIDMT);

	case PIOCGHOLD:	
	case PIOCGUTID:
		return (PIOCA_VALIDT);

	case PIOCSTRACE:
	case PIOCKILL:
	case PIOCNICE:
	case PIOCSENTRY:
	case PIOCSEXIT:
	case PIOCSRLC:
	case PIOCRRLC:
	case PIOCSFAULT:
	case PIOCSFORK:
	case PIOCRFORK:
	case PIOCSET:
	case PIOCRESET:
		return (PIOCA_VALIDP | PIOCA_WRITE);

	case PIOCGTRACE:
	case PIOCGENTRY:
	case PIOCGEXIT:	
	case PIOCMAXSIG:
	case PIOCACTION:
	case PIOCGFAULT:	
	case PIOCGROUPS:	
	case PIOCENEVCTRS:
	case PIOCENEVCTRSEX:
	case PIOCRELEVCTRS:
	case PIOCGETEVCTRS:
	case PIOCGETPREVCTRS:
	case PIOCGETEVCTRL:
	case PIOCGETEVCTRLEX:
	case PIOCSETEVCTRL:
	case PIOCSAVECCNTRS:
	case PIOCGETSN0REFCNTRS:
	case PIOCGETSN0EXTREFCNTRS:
	case PIOCGETINODE:
	case PIOCPTIMERS:
		return (PIOCA_VALIDP);
	}
#ifdef CKPT
	return (ckpt_prioctl_attr(cmdval));
#else
	return 0;
#endif
}

static int
prioctl_prepare(
	pioc_ctx_t	*ctxp,
	struct prnode   *pnp,
        caddr_t         cmaddr,
        int             flag)
{
        int             attr;

	/*
	 * The PIOC_IRIX5_64 and PIOC_IRIX5_32 bits allow 32 bit programs
	 * to perform ioctls on 64 bit or n32 programs by causing the 64bit
	 * or n32 data structures to be returned.
	 */
	if (ctxp->pc_cmd & PIOC_IRIX5_64) {
		if (ABI_IS_IRIX5_64(get_current_abi()))
			return EINVAL;
		/* convert to the normal ioctl but record the fact
		 * that we want the 64bit abi.  Variable target_abi is
		 * used in priface ioctls.
		 */
		ctxp->pc_cmd &= ~PIOC_IRIX5_64;
		ctxp->pc_tabi = ABI_IRIX5_64;
	} else if (ctxp->pc_cmd & PIOC_IRIX5_N32) {
		ctxp->pc_cmd &= ~PIOC_IRIX5_N32;
		ctxp->pc_tabi = ABI_IRIX5_N32;
	} else {
		ctxp->pc_tabi = get_current_abi();
	}

	attr = prioctl_attr(ctxp->pc_cmd);
	ctxp->pc_attr = attr;
	
#if (_MIPS_SIM == _ABIO32)
	if (ABI_IS_IRIX5_64(ctxp->pc_tabi))
		return EINVAL;
	/*
	 * prgetstatus formats the buffer in n32 format only.
	 * tell xlate_copyout that the kernel is o32 so that o32
	 * programs will suffer the least overhead from this xlate stuff.
	 */
	if (attr & PIOCA_PRGS)
		ctxp->pc_kabi = ABI_IRIX5_N32;
	else
		ctxp->pc_kabi = ABI_IRIX5;
#endif

	if (pnp->pr_type == PR_PSINFO) {
	        if ((attr & PIOCA_PSINFO) == 0) {
			        return EACCES;
		}
	}
	if (attr & PIOCA_WRITE) {
		/*
		 * Fail ioctls which are logically "write" requests unless
		 * the user has write permission.
		 */
		if ((flag & FWRITE) == 0)
			return EBADF;
	}
        
	ctxp->pc_buf = NULL;
	if ((attr & PIOCA_GETBUF) || 
	    ((attr & PIOCA_CGETBUF) && (cmaddr != NULL))) {
		ctxp->pc_buf = (caddr_t) kmem_zalloc(sizeof(prioctl_maxbuf_t),
						     KM_SLEEP);
		ASSERT(ctxp->pc_buf != NULL);
	}
	return (0);
}

static void 
prioctl_finish(
	pioc_ctx_t	*ctxp)
{
	if (ctxp->pc_buf)
		(void) kmem_free(ctxp->pc_buf, sizeof(prioctl_maxbuf_t));
}

static prrun_t *
prioctl_prrun(
	pioc_ctx_t	*ctxp,
        caddr_t         cmaddr)
{
        prrun_t         *prrun = (prrun_t *) ctxp->pc_buf;

	if (cmaddr == NULL)
	        prrun->pr_flags = 0;
	else if (COPYIN_XLATE(cmaddr, prrun, sizeof(prrun_t),
			      irix5_to_prrun, ctxp->pc_tabi, 1)) 
	        prrun = NULL;
	return (prrun);
}

/*
 * Check that thread is in a valid state for PIOCRUN
 * This routine is used as a client of uthread_apply.
 */
/*ARGSUSED1*/
int 
prchkrun(uthread_t *ut, void *nada)
{
	int s;
        int err;

	ASSERT(nada == NULL);
	s = ut_lock(ut);
	err = prchkrun_sub(ut);
	ut_unlock(ut, s);
	return (err);
}

/*
 * Check that thread may validly do a step
 * This routine is used as a client of uthread_apply.
 */
int 
prchkrun_step(uthread_t *ut, void *prp_void)
{
	uvaddr_t pc;
	int error;
	as_setrangeattr_t attr;
        prrun_t *prp = (prrun_t *) prp_void;
	vasid_t vasid;

	/*
	 * make sure that where we're going to execute is steppable
	 */
	if (prp->pr_flags & PRSVADDR)
	        pc = prp->pr_vaddr;
	else
	        pc = (uvaddr_t)ut->ut_exception->u_eframe.ef_epc;

	if (as_lookup(ut->ut_asid, &vasid))
	        return EINVAL;
	attr.as_op = AS_MAKE_PRIVATE;
	error = VAS_SETRANGEATTR(vasid, pc, 0, &attr, NULL);
	as_rele(vasid);
	return error;
}

/*
 * Check that thread is in a valid state for PIOCRUN
 * The uthread passed to this routine must be locked.
 */
int 
prchkrun_sub(uthread_t *ut)
{
	if (ut->ut_flags & UT_STOP) {
		if (ut->ut_whystop == 0 || ut->ut_whystop == JOBCONTROL) {
			return EBUSY;
		}
	} else {
		if (!(ut->ut_flags & UT_PRSTOPBITS)) {
			return EBUSY;
		}
	}
	return (0);
}

/*
 * Apply PIOCRUN options and start a single thread
 * This routine is used as the client of uthread_apply when doing the
 * final stage of PIOCRUN on a process.
 */
int
prsetrun(uthread_t *ut, void *prsrp_void)
{
        prsetrun_t *prsrp = (prsetrun_t *) prsrp_void;
	prrun_t *prp = prsrp->prsr_prrun;
        int s;
        int ret = 0;

	s = ut_lock(ut);
	prsetrun_opts(ut, prp, s, 1, 1);
	/* XXX not sure this can ever be FALSE - uscan lock held */
	if ((ut->ut_pproxy->prxy_flags & PRXY_JSTOPPED) == 0)
		prsetrun_final(ut);
	ut_unlock(ut, s);
	return (ret);
}


/*
 * Apply PIOCRUN options.
 *
 * The uthread passed is locked on entry and returned locked, but may be
 * unlocked and relocked by the subroutine.
 */
int
prsetrun_opts(uthread_t *ut, prrun_t *prp, int s, 
	      int no_step_check, int no_proc_opts)
{
	int err = 0;

	ASSERT(UT_TO_PROC(ut)->p_trmasks != NULL);

	/*
	 * perform error checking here before setting anything
	 */
	if (no_step_check == 0 && (prp->pr_flags & PRSTEP)) {
		ut_unlock(ut, s);
	        err = prchkrun_step(ut, prp);
		s = ut_lock(ut);
		if (err)
			return (err);
	}

	/*
	 * Now change state -- no errors from here on down.
	 * Once we run we are no longer stopped on an event of interest.
	 */
	if (prp->pr_flags & PRCSIG) {
		if (ut->ut_whystop == SIGNALLED) {
			cursiginfofree(ut);
			ut->ut_cursig = 0;
		}
	}

	if (prp->pr_flags & PRSHOLD) {
                sigutok(&prp->pr_sighold, ut->ut_sighold);
		sigdiffset(ut->ut_sighold, &cantmask);
	}

	if (prp->pr_flags & PRCFAULT) {       
	        if (ut->ut_whystop == FAULTED)
                        ut->ut_flags |= UT_CLRHALT;
	}

	if (no_proc_opts == 0 && (prp->pr_flags & (PRSTRACE | PRSFAULT))) {
                ut_unlock(ut, s);
		prsetrun_proc(UT_TO_PROC(ut), prp);
		s = ut_lock(ut);
	}

	/*
	 * diddle the return-to-user-land address
	 */
	if (prp->pr_flags & PRSVADDR)
		prsvaddr(ut, prp->pr_vaddr);

	if (prp->pr_flags & PRSTEP)
		ut->ut_flags |= UT_STEP;

	if (prp->pr_flags & PRCSTEP) {
		ut->ut_flags &= ~UT_STEP;
		/*
		 * Clear any indication that we might be going back and
		 * exceuting a kernel installed brk pt. Bug 522347.
		 */
		PCB(pcb_ssi.ssi_cnt) = 0;
		if (ut->ut_flags & UT_SRIGHT) {
			ut->ut_flags &= ~UT_SRIGHT;
			ut_unlock(ut, s);
			prsright_release(ut->ut_pproxy);
			s = ut_lock(ut);
		}
	}

	if (prp->pr_flags & PRSABORT)
		ut->ut_flags |= UT_SYSABORT;

	if (prp->pr_flags & PRSTOP)
		ut->ut_flags |= UT_PRSTOPX;
	return (err);
}

void
prsetrun_final(uthread_t *ut)
{
	ASSERT(prchkrun_sub(ut) == 0);
	if (ut->ut_flags & UT_STOP)
	        unstop(ut);
	else {
	        ASSERT(ut->ut_whystop == 0);
		ut->ut_flags &= ~UT_PRSTOPBITS;
	}
}

/*
 * Apply proc-specific PIOCRUN options
 */
void 
prsetrun_proc(proc_t *p, prrun_t *prp)
{
        int s;

	s = p_lock(p);

	if (prp->pr_flags & PRSTRACE) {
	  	sigutok(&prp->pr_trace, &p->p_sigtrace);
	}

	if (prp->pr_flags & PRSFAULT) {
	  	p->p_trmasks->pr_fltmask = prp->pr_fault;
	}

	if (p->p_trmasks->pr_systrap
	    || !prisempty(&p->p_trmasks->pr_fltmask)
	    || !sigisempty(&p->p_sigtrace))
	  	p->p_flag |= SPROCTR;
	else
	  	p->p_flag &= ~SPROCTR;
	p_unlock(p, s);
}


/* ARGSUSED */
int 
pioccfault(uthread_t *ut, void *trash)
{
        int n;
			
	n = ut_lock(ut);
	if (ut->ut_whystop == FAULTED) 
	          ut->ut_flags |= UT_CLRHALT;
	ut_unlock(ut, n);
	return (0);
}

#ifdef R10000
/*
 * Acquire and start using the hardware counters.	
 */
int
piocenevctrsex(uthread_t *ut, void *hwargs)
{
	pioc_hwarg_t *hwap = (pioc_hwarg_t *)hwargs;

	return hwperf_enable_counters(cpumon_alloc(ut),
				      (hwperf_profevctrarg_t *)hwap->arg,
				      (hwperf_profevctraux_t *)hwap->auxp,
				      HWPERF_PROC,
				      hwap->rvalp);
}

/*
 * Stop using hardware counters and free them up.
 */
int
piocrelevctrs(uthread_t *ut, void *rvp)
{
	int *rvalp = (int *)rvp;
	cpu_mon_t *cmp;

	if (cmp = ut->ut_cpumon) {
		if (*rvalp && *rvalp > cmp->cm_gen)
			*rvalp = cmp->cm_gen;
		(void) hwperf_disable_counters(cmp);
	} else {
		*rvalp = -1;
	}
	return 0;
}

int
piocgetevctrs(uthread_t *ut, void *hwargs)
{
	pioc_hwarg_t *hwap = (pioc_hwarg_t *)hwargs;

	if (!ut->ut_cpumon)
		return ESRCH;

	hwperf_plus_counters(ut->ut_cpumon,
			     (hwperf_cntr_t *)hwap->arg, hwap->rvalp);

	return 0;
}

int
piocgetevctrl(uthread_t *ut, void *hwargs)
{
	pioc_hwarg_t *hwap = (pioc_hwarg_t *)hwargs;

	if (!ut->ut_cpumon)
		return ESRCH;

	return hwperf_control_info(ut->ut_cpumon, 
				   (hwperf_eventctrl_t *)hwap->arg,
				   (hwperf_eventctrlaux_t *)hwap->auxp,
				   hwap->rvalp);
}

int
piocsetevctrl(uthread_t *ut, void *hwargs)
{
	pioc_hwarg_t *hwap = (pioc_hwarg_t *)hwargs;

	if (!ut->ut_cpumon)
		return ESRCH;

	return hwperf_change_control(ut->ut_cpumon, 
				   (hwperf_profevctrarg_t *)hwap->arg,
				   HWPERF_PROC,
				   hwap->rvalp);
}

int
piocsaveccntrs(uthread_t *ut, void *rv)
{
	return hwperf_parent_accumulates(cpumon_alloc(ut), (int *)rv);
}
#endif /* R10000 */
