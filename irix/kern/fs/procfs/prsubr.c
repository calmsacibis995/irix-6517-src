/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*#ident	"@(#)uts-comm:fs/procfs/prsubr.c	1.9"*/
#ident	"$Revision: 1.114 $"

#include <sys/types.h>
#include <ksys/as.h>
#include <sys/atomic_ops.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/ksignal.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/poll.h>
#include <ksys/pid.h>
#include <ksys/vproc.h>
#include <sys/proc.h>
#include <sys/runq.h>
#include <sys/sema.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/var.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/prctl.h>
#include <os/as/as_private.h> /* XXX */
#include "os/proc/pproc_private.h"

#include "prdata.h"
#include "prsystm.h"
#include "procfs.h"
#include "procfs_private.h"
#ifdef _SHAREII
# include <sys/shareIIstubs.h>
#endif /* _SHAREII */

/*
 * These are defined in the generic kernel because sv_wait wants to
 * take the address of proctracewait while avoiding deadlock, therefore
 * proctracewait can't be stubbed, and proctracelock tags along.
 */
extern sv_t	proctracewait;	/* sleepers waiting for proc to stop */
extern mutex_t	proctracelock;	/* associated lock held during vsema of above */

static void prpollwakeup_any(prnode_t *pnp, short event);

/*
 * Called from procfree() or exit() when a traced process is removed
 * from the process table.  Releases the traced process's reference
 * on p->p_trace.
 */
void
prexit(struct proc *p)
{
	prnode_t *pnp;

	if (p->p_trmasks) {
		kmem_zone_free(procfs_trzone, p->p_trmasks);
		p->p_trmasks = NULL;
	}

#ifdef _SHAREII
	/*
	 * tell ShareII the process is *finally* gone
	 */
	SHR_PROCDESTROY(p);
#endif /* _SHAREII */

	if (!p->p_trace)
		return;

	pnp = p->p_trace;

	/* Release the process's hold on the vnode. This may be the
	 * last reference, in which case VOP_INACTIVE will be
	 * called to free the prnode.
	 *
	 * If the node is PRINVAL - then we released the reference
	 * in prinvalidate.
	 */
	if (!(pnp->pr_pflags & PRINVAL)) {
		ASSERT(p->p_trace->pr_vnode != NULL);
		VN_RELE(p->p_trace->pr_vnode);
	}
}

/*
 * Propagates debugging information from parent to child.
 */
void
prfork(
        uthread_t *puth,
        uthread_t *cuth)
{
        struct proc *pp = UT_TO_PROC(puth);
        struct proc *cp = UT_TO_PROC(cuth);

        if (pp->p_dbgflags & KPRIFORK) {

                /* Inherit the debugging attributes from parent. */
                cp->p_flag |= (pp->p_flag & (SPROCTR));
                cp->p_sigtrace = pp->p_sigtrace;
                cp->p_dbgflags = pp->p_dbgflags;
                cuth->ut_flags |= (puth->ut_flags & (UT_STEP)); /* XXX */

                cp->p_trmasks = kmem_zone_alloc(procfs_trzone, KM_SLEEP);
                ASSERT(pp->p_trmasks != NULL);
                bcopy(pp->p_trmasks, cp->p_trmasks,
                                                sizeof(struct pr_tracemasks));
                /* These are zeroed, not inherited. */
                cp->p_trmasks->pr_error = 0;
                cp->p_trmasks->pr_rval1 = 0;
                cp->p_trmasks->pr_rval2 = 0;
        } else {
#ifdef do_not_bother_cause_cp_is_zallocd
                /* clear out tracing flags */
                sigemptyset(&cp->p_sigtrace);
                /*
                 * if the inherit flag wasn't set, kill all the
                 * tracing flags:
                 */
                cp->p_dbgflags = 0;
#endif
                ASSERT(sigisempty(&cp->p_sigtrace));
                ASSERT(cp->p_dbgflags == 0);
        }
}

/* Called to wake up pollers of processes */
void
prpollwakeup(prnode_t *pnp, short event)
{
	ASSERT(pnp->pr_tpid == current_pid());
        prpollwakeup_any(pnp, event);
}

void
prpollwakeup_any(prnode_t *pnp, short event)
{
	if (pnp->pr_pollhead != NULL)
		pollwakeup(pnp->pr_pollhead, event);
}

/*
 * Ensure that the underlying object file is readable.
 */
int
prisreadable(
	register proc_t *p,
	register struct cred *cr)
{
	register int error = 0;
	vnode_t *vp;

	/* Protect p_exec against races with exec */
	mrlock(&p->p_who, MR_ACCESS, PZERO);
	if ((vp = p->p_exec) != NULL)
		VOP_ACCESS(vp, VREAD, cr, error);
	mrunlock(&p->p_who);
	return error;
}

/*
 * Called from hooks in exec-related code when a traced process
 * attempts to exec(2) a setuid/setgid program or an unreadable
 * file.  Rather than fail the exec we invalidate the associated
 * /proc vnode so that subsequent attempts to use it will fail.
 *
 * Old (invalid) vnodes are retained on a linked list (rooted at
 * p_trace in the process table entry) until last close.
 *
 * A controlling process must re-open the /proc file in order to
 * regain control.
 */
void
prinvalidate(register proc_t *p)
{
	prnode_t *pnp = p->p_trace;
	proc_proxy_t *prxy = &p->p_proxy;
	uthread_t *ut;
	int s;
	int tracewait = 0;

	ASSERT(prxy->prxy_nthreads == 1 
	       && prxy_to_thread(prxy)->ut_next == NULL);

	/*
	 * Invalidate the currently active /proc vnode.
	 */
	if (pnp != NULL) {
		mutex_lock(&pnp->pr_lock, PINOD);
		ASSERT(!(pnp->pr_pflags & PRINVAL));
		pnp->pr_pflags |= PRINVAL;
		mutex_unlock(&pnp->pr_lock);
	}

	/*
	 * If any tracing flags are in effect and the vnode is open for
	 * writing then set the requested-stop and run-on-last-close flags.
	 * Otherwise, clear all tracing flags and wake up any tracers.
	 */
	uscan_update(prxy);
	s = p_lock(p);
	for (; pnp != NULL; pnp = pnp->pr_next) {
		if (pnp->pr_writers) {	/* some writer exists */
			break;
		}
	}

	if ((p->p_flag & SPROCTR) && pnp != NULL) {
	  	uscan_forloop(prxy, ut) {
			ut_nested_lock(ut);
			ut->ut_flags |= UT_PRSTOPX;
			ut_nested_unlock(ut);
		}
		p->p_dbgflags |= KPRRLC;
	} else {
		tracewait = prletrun(p);
	}
	p_unlock(p, s);
	uscan_unlock(prxy);

	if (tracewait)
	        prawake();

	/* Release traced process's reference on the p_trace
	 * vnode - this may result in VOP_INACTIVE being called,
	 * and the vnode being freed.
	 */
	 if (p->p_trace) {
		 ASSERT(p->p_trace->pr_vnode != NULL);
		 VN_RELE(p->p_trace->pr_vnode);
	}
}

/*
 * Releases a process to run at prinvalidate time or at last close when
 * run on last-close is in effect.
 *
 * The caller must have locked the target process and have done a uscan_update
 * on the associated proxy.
 *
 * The value returned is non-zero if the caller should call prawake.
 */
int
prletrun(
	struct proc	*p)
{
	int 		tracewait = 0;
	proc_proxy_t 	*prxy = &p->p_proxy;
	uthread_t 	*ut;
        int     	s;

	ASSERT(p_islocked(p));
	ASSERT(uscan_update_held(prxy));

	if (p->p_trmasks) {
	        /* syscalls */
	        premptyset(&p->p_trmasks->pr_entrymask);
		premptyset(&p->p_trmasks->pr_exitmask);
		/* faults */
		premptyset(&p->p_trmasks->pr_fltmask);
		p->p_trmasks->pr_systrap = 0;
	}
	premptyset(&p->p_sigtrace);			/* signals */
	p->p_flag &= ~(SPROCTR|SCKPT);
	uscan_forloop(prxy, ut) {
	        ut_nested_lock(ut);
		tracewait |= ut->ut_flags & UT_TRWAIT;
		ut->ut_flags &= ~(UT_PRSTOPBITS|UT_TRWAIT|UT_HOLDJS|UT_EVENTPR);
		if (ut->ut_whystop != 0 &&
		    ut->ut_whystop != JOBCONTROL &&
		    ut->ut_flags & UT_STOP)
		        unstop(ut);
		ut_nested_unlock(ut);
	}
	s = prxy_stoplock(prxy);
	tracewait |= (prxy->prxy_flags & PRXY_PWAIT);
	prxy_nflagclr(prxy, PRXY_JSARMED | PRXY_PWAIT | PRXY_PSTOP | 
				PRXY_JPOLL | PRXY_JSTOP | PRXY_JSTOPPED);
	prxy->prxy_jscount = 0;
	prxy_stopunlock(prxy, s);
	p->p_dbgflags = 0;
	return (tracewait);
}

/*
 * /proc fault handler interface.
 * If the trap is not intrinsically fatal, /proc may wish to stop
 * the process and cancel the fault.
 */
void
prfault(
	register uthread_t *ut,
	register uint code,
	int *sigp)
{
	register proc_t *p = UT_TO_PROC(ut);
	register int sig = *sigp;
	register int dostop = 0;
	register int what = 0;
	fltset_t faults;
	int s;

	/*
	 * read fltmask atomically (faults is > 1 word)
	 */
	s = p_lock(p);
	ASSERT(p->p_trmasks != NULL);
	faults = p->p_trmasks->pr_fltmask;
	p_unlock(p, s);
	
	switch(code) {
	case SEXC_CPU:
	case EXC_II:
		if (prismember(&faults, FLTILL)) {
			what = FLTILL;
			dostop = 1;
		}
		break;
	case EXC_OV:
		if (prismember(&faults, FLTIOVF)) {
			what = FLTIOVF;
			dostop = 1;
		}
		break;
	case EXC_BREAK:
		switch (ut->ut_code) {
		case BRK_USERBP:
			what = FLTBPT;
			break;
		case BRK_SSTEPBP:
			what = FLTTRACE;
			break;
		case BRK_RANGE:		/* range error check */
		case BRK_MULOVF:	/* multiply overflow detected */
		case BRK_OVERFLOW:	/* overflow check */
			what = FLTIOVF;
			break;
		case BRK_DIVZERO:	/* divide by zero check */
			what = FLTIZDIV;
			break;
		}
		if (what && prismember(&faults, what))
			dostop = 1;
		break;
	case EXC_WADE:
	case EXC_RADE:
	case EXC_DBE:
	case EXC_IBE:
		if (prismember(&faults, FLTACCESS)) {
			what = FLTACCESS;
			dostop = 1;
		}
		break;
	case EXC_RMISS:
	case EXC_WMISS:
	case SEXC_SEGV:
		if (ut->ut_code == FAULTEDSTACK) {
			if (prismember(&faults, FLTSTACK)) {
				what = FLTSTACK;
				dostop = 1;
			}
		} else {
			if (prismember(&faults, FLTBOUNDS)) {
				what = FLTBOUNDS;
				dostop = 1;
			}
		}
		break;
	case SEXC_PAGEIN:
		if (prismember(&faults, FLTPAGE)) {
			what = FLTPAGE;
			dostop = 1;
		}
		break;
	case SEXC_WATCH:
		if (ut->ut_code == FAULTEDWATCH) {
			if (prismember(&faults, FLTWATCH)) {
				what = FLTWATCH;
				dostop = 1;
			}
		} else if (ut->ut_code == FAULTEDKWATCH) {
			if (prismember(&faults, FLTKWATCH)) {
				what = FLTKWATCH;
				dostop = 1;
			}
		} else if (ut->ut_code == FAULTEDSCWATCH) {
			if (prismember(&faults, FLTSCWATCH)) {
				what = FLTSCWATCH;
				dostop = 1;
			}
		}
		break;
	}

	if (dostop) {
		s = ut_lock(ut);
		ut->ut_flags &= ~UT_CLRHALT;
		ut_unlock(ut, s);

		stop(ut, FAULTED, what, what != FLTTRACE);

		if ((ut->ut_flags & UT_CLRHALT) || !(p->p_flag & SPROCTR))
			sig = 0;
	}
	*sigp = sig;
}

/*
 * Lock a process.  Until prunlock() is performed, this
 * (a) serializes use of /proc/<pid> and (b) prevents the process
 * from running again if it is not a system process.
 * Returns 0 on success, a non-zero error number on failure.
 *
 * 'zdisp' is ZYES if we want to return zombies, ZNO if not.
 * 
 * if 'nonull' is non-zero, then return error if  VPROC_LOOKUP fails.
 */
int
prlock(
	prnode_t *pnp,
	int zdisp,
	int nonull)
{
	vproc_t *vpr;
	pid_t pid;

	if (mutex_mine(&pnp->pr_lock)) {
		/* allow nested prlock/prunlock */
		pnp->pr_nested++;
		return 0;
	}

	mutex_lock(&pnp->pr_lock, PINOD);

	if ((pid = pnp->pr_tpid) == -1) {
		ASSERT(pnp->pr_proc == NULL);

		if (nonull) {
			mutex_unlock(&pnp->pr_lock);
			return ENOENT;
		}
		return 0;
	}

	if ((vpr = VPROC_LOOKUP_STATE(pid, zdisp)) == NULL) {
		/*
		 * Process exited under us.
		 * If the caller will accept null pr_proc
		 * then we should not return an error.
		 */
		ASSERT(zdisp == ZNO || pnp->pr_proc == NULL);

		if (nonull) {
			mutex_unlock(&pnp->pr_lock);
			return ENOENT;
		}

		/* The only time the pr_proc field is guaranteed
		 * to be valid is when the prnode is locked via
		 * prlock. Since the vproc lookup failed, we
		 * set pr_proc to null.
		 */
		pnp->pr_proc = NULL;
	} else {
		/* Set pnp->pr_proc, since we have reference on
		 * the process.
		 */
		prnode_t *tpnp;
		int s;

		VPROC_GET_PROC(vpr, &pnp->pr_proc);
                VPROC_ASSERT_LOCAL(vpr);
		ASSERT(pnp->pr_proc);
		uscan_hold(&pnp->pr_proc->p_proxy);
		/*
		 * Proc may hav exited and pid re-used.  Make
		 * sure that pnp is in trace list
		 */
		s = p_lock(pnp->pr_proc);
		tpnp = (pnp->pr_type == PR_PROCDIR)? 
				pnp->pr_proc->p_trace :
				pnp->pr_proc->p_pstrace;
		for (; tpnp; tpnp = tpnp->pr_next) {
			if (tpnp == pnp) {
				/*
				 * It's a match
				 */
				break;
			}
		}
		p_unlock(pnp->pr_proc, s);
		if (tpnp == NULL) {
			/*
			 * Wrong process
			 */
			uscan_rele(&pnp->pr_proc->p_proxy);
			VPROC_RELE(vpr);
			if (nonull) {
				mutex_unlock(&pnp->pr_lock);
				return ENOENT;
			}
			pnp->pr_proc = NULL;
		}
	}
	return 0;
}

/*
 * Undo prlock.
 */
void
prunlock(struct prnode *pnp)
{
	proc_t *p;

	if (pnp->pr_nested) {
		/* allow nested prlock/prunlock */
		ASSERT(mutex_mine(&pnp->pr_lock));
		pnp->pr_nested--;
		return;
	}

	/*
	 * Clear pr_proc - the only time it is valid to reference
	 * this field is when the node is locked via prlock.
	 */
	if (p = pnp->pr_proc) {
		pnp->pr_proc = NULL;
		uscan_rele(&p->p_proxy);
		VPROC_RELE(PROC_TO_VPROC(p));
	}

	mutex_unlock(&pnp->pr_lock);

}

/*
 * If a controlling process is sleeping in prwstop()
 * waiting for us to wake up, wake it.
 */
void
prawake(void)
{
	mutex_lock(&proctracelock, 0);
	sv_broadcast(&proctracewait);
	mutex_unlock(&proctracelock);
}

static int pr_wake_count;
#ifdef DEBUG
static int pr_wake_skip;
#endif

/* ARGSUSED */
void
pr_async_wake(async_vec_t *asv)
{
	ASSERT(pr_wake_count == 1);
	pr_wake_count = 0;
	prawake();
}

/*
 * Wake up controlling process waiting in prwstop().
 * This is called by those who cannot afford to sleep.
 */
void
prasyncwake(void)
{
	async_vec_t asv;

	/*
	 * If there's already a request to wake up a tracing process,
	 * don't bother.  If locking ever moves from global to the
	 * targetted processes, we won't be able to do this optimazation.
	 */
	if (compare_and_swap_int(&pr_wake_count, 0, 1) == 0) {
#ifdef DEBUG
		atomicAddInt(&pr_wake_skip, 1);
#endif
		return;
	}

	asv.async_func = pr_async_wake;

	(void) async_call(&asv);
}

/*
 * This god help us reads the current instruction
 */
int
prstatusio(proc_t *p, void *where, void *cur_pc)
{
	uio_t uio;
	iovec_t iov;

	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = (off_t)cur_pc;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_fmode = 0;
	uio.uio_limit = 0;
	uio.uio_resid = iov.iov_len = 4;
	uio.uio_pmp = NULL;
	uio.uio_pio = 0;
	uio.uio_readiolog = 0;
	uio.uio_writeiolog = 0;
	uio.uio_pbuf = 0;
	iov.iov_base = where;

	return prusrio(p, prchoosethread(p), UIO_READ, &uio, 0);
}

/*
 * Count the number of attr segments in this process's address space.
 * NB: caller must acquire and release p->p_aspacelock.
 */
int
prnsegs(vasid_t vasid)
{
	as_getattr_t asattr;

	VAS_GETATTR(vasid, AS_NATTRS, NULL, &asattr);
	return asattr.as_nattrs;
}

/*
 * Determine whether a set is empty.
 */
int
setisempty(register __uint32_t *sp, register unsigned n)
{
	while (n--)
		if (*sp++)
			return 0;
	return 1;
}

#define PRWSTOP_UTHREAD_OK(ut)   					\
        (isstopped(ut) ||						\
	 (((ut)->ut_flags & UT_STOP) && ((ut)->ut_flags & UT_CKPT))) 

/*
 * Wait for a uthread to stop.
 * We have prnode (and process) prlocked locked when we enter.
 * Returns:
 *	0	if process now stopped
 *	EAGAIN	if process now invalid
 *	ENOENT	if process went away
 *	EINTR	if process was interrupted by a signal
 */
int
prwstop_ut(uthread_t *ut, struct prnode *pnp)
{
#if DEBUG
	proc_t *p = UT_TO_PROC(ut);
#endif
	int s;

	/*
	 * If process is not stopped, unlock the process and wait
	 * for it to stop/exit.  Holding proctracelock will prevent
	 * a race condition with the cvsema in the stop/exit code
	 * and the sv_wait_sig below.
	 * In the case of a requested stop (UT_PRSTOP) we will also
	 * be awoken by target process going to sleep.
	 */
again:
	mutex_lock(&proctracelock, 0);
	s = ut_lock(ut);

	while (!PRWSTOP_UTHREAD_OK(ut)) {
	        ut->ut_flags |= UT_TRWAIT;
		ut_unlock(ut, s);
		prunlock(pnp);
		if (sv_wait_sig(&proctracewait, 0, &proctracelock, 0) == -1)
			return (EINTR);

		/* here if either signaled via sv_signal OR received a 
		 * cancelled signal/job control
		 */
		if (pnp->pr_pflags & PRINVAL)
			return (EAGAIN);
		if (prlock(pnp, ZNO, PRNONULL))
			return (ENOENT);

		goto again;
	}
	
	/*
	 * Pull any FP register info.
	 * Do this with siglck set so process cannot just proceed.
	 */
	if (UT_TO_KT(ut)->k_flags & KT_SLEEP)
		checkfp(ut, 0);
	/*
	 * Now we know for sure that the process is definitely stopped.
	 */
	ut_unlock(ut, s);
	mutex_unlock(&proctracelock);
	
	/*
	 * Wakeup anybody polling/selecting us.
	 */
	ASSERT(p->p_trace && p->p_trace == pnp);
	if (pnp->pr_pollhead)
		pollwakeup(pnp->pr_pollhead,
				POLLIN|POLLPRI|POLLOUT|POLLRDNORM|POLLRDBAND);

	/*
	 * Fix for possible PIOCSTOP while someone else is PIOCWSTOP.
	 */
	prawake();

	return (0);
}

/*
 * Wait for a uthread to stop.
 * We have prnode (and process) prlocked locked when we enter.
 * Returns:
 *	0	if process now stopped
 *	EAGAIN	if process now invalid
 *	ENOENT	if process went away
 *	EINTR	if process was interrupted by a signal
 */
int
prwstop_proc(proc_t *p, struct prnode *pnp)
{
        proc_proxy_t *px = &p->p_proxy;
        uthread_t *ut;
	int s;
	int ok;

	/*
	 * If process is not stopped, unlock the process and wait
	 * for it to stop/exit.  Holding proctracelock will prevent
	 * a race condition with the cvsema in the stop/exit code
	 * and the sv_wait_sig below.
	 * In the case of a requested stop (UT_PRSTOP) we will also
	 * be awoken by target process going to sleep.
	 */
	while (1) {
	        mutex_lock(&proctracelock, 0);
		uscan_access(px);
		if (px->prxy_utshare) {
			prxy_flagset(px, PRXY_PWAIT);
			ok = 1;
			uscan_forloop(px, ut) {
		        	s = ut_lock(ut);
				ok = PRWSTOP_UTHREAD_OK(ut);
				ut_unlock(ut, s);
				if (!ok)
					break;
			}
			if (ok) {
				/*
				 * The stoplock is not strictly needed,
				 * except that jsacct might someday
				 * clear jsarmed before setting jstopped.
				 */
				s = prxy_stoplock(px);
				if (px->prxy_flags & PRXY_JSARMED)
					ok = 0;
				prxy_stopunlock(px, s);
				if (ok)
					prxy_flagclr(px, PRXY_PWAIT);
			}
		}
		else {
		        ut = prxy_to_thread(px);
			s = ut_lock(ut);
			ok = PRWSTOP_UTHREAD_OK(ut);
			if (!ok)
				ut->ut_flags |= UT_TRWAIT; 
			ut_unlock(ut, s);
		}
		uscan_unlock(px);
		if (ok)
			break;
		prunlock(pnp);
		if (sv_wait_sig(&proctracewait, 0, &proctracelock, 0) == -1)
			return (EINTR);

		/* here if either signaled via sv_signal OR received a 
		 * cancelled signal/job control
		 */
		if (pnp->pr_pflags & PRINVAL)
			return (EAGAIN);
		if (prlock(pnp, ZNO, PRNONULL))
			return (ENOENT);
		p = pnp->pr_proc;
		px = &p->p_proxy;		
	}

	mutex_unlock(&proctracelock);
	
	/*
	 * Pull any FP register info.
	 * Do this with ut_lock held so threads cannot just proceed.
	 */
	uscan_access(px);
        uscan_forloop(px, ut) {
	        s = ut_lock(ut);
		if (UT_TO_KT(ut)->k_flags & KT_SLEEP)
		  	checkfp(ut, 0);
		ut_unlock(ut, s);
	}
	uscan_unlock(px);
	
	/*
	 * Wakeup anybody polling/selecting us.
	 */
	ASSERT(p->p_trace && p->p_trace == pnp);
	if (pnp->pr_pollhead)
		pollwakeup(pnp->pr_pollhead,
				POLLIN|POLLPRI|POLLOUT|POLLRDNORM|POLLRDBAND);

	/*
	 * Fix for possible PIOCSTOP while someone else is PIOCWSTOP.
	 */
	prawake();

	return (0);
}

/*
 * Find or construct a vnode for the given process.
 */
vnode_t *
prbuild(
        struct proc *p, 
        int is_procdir)
{
	register prnode_t *pnp;
	register vnode_t *vp;
	register vnode_t *pvp;
	prnode_t *tpnp;
	vmap_t vmap;
	int s;

	/*
	 * Pre-allocate, since we don't want to sleep while
	 * we fiddle with the data structures.
	 */
	vp = vn_alloc(procfs_vfs, VREG, 0);

	/* We grab a reference for the traced process
	 * now - since the traced process does operations on
	 * this vnode (prpollwakeup, prinvalidate) it really 
	 * should have a reference of its own.
	 */
	if (is_procdir)
		VN_HOLD(vp);
again:
	s = p_lock(p);
	if ((tpnp = (is_procdir ? p->p_trace : p->p_pstrace)) &&
	    ((tpnp->pr_pflags & PRINVAL) == 0)) {

		/*
		 * Because of weaknesses in the prinactive logic, a process
                 * which is being reaped may not have its prnode information
                 * cleaned up properly.  To defend against this, the following
                 * hack checks situations in which this may be happening 
                 * and avoids a panic.  See pv 554233.
		 */
		pvp = tpnp->pr_vnode;
		if (pvp != NULL)
			VMAP(pvp, vmap);
		p_unlock(p, s);
		if (vmap.v_number == 0)
			pvp = NULL;
		if (pvp == NULL || vn_get(pvp, &vmap, 0) > 0) {
			/* Remove traced process' reference */
			if (is_procdir)
				VN_RELE(vp);

			vn_free(vp);
			return pvp;
		}
		goto again;
	}

	/*
	 * New /proc vnode required; allocate it and fill in all the fields.
	 */
	nested_spinlock(&procfs_lock);
	if (procfs_freelist) {
		pnp = procfs_freelist;
		procfs_freelist = pnp->pr_free;
		ASSERT(procfs_nfree > 0);
		procfs_nfree--;
		procfs_refcnt++;
		nested_spinunlock(&procfs_lock);
	} else {
		nested_spinunlock(&procfs_lock);
		p_unlock(p, s);
		/* 
		 * we must init mutex here - a kernel with metered mutexes
		 * can sleep, and it's very bad to sleep with siglck
		 * The naming info isn't all that important.
		 */
		pnp = (prnode_t *)kmem_zalloc(sizeof(prnode_t), KM_SLEEP);
		mutex_init(&pnp->pr_lock, MUTEX_DEFAULT, "prnode");

		/*
		 * We might have slept in the allocation routine.
		 * loop around to try again.
		 */
		s = mutex_spinlock(&procfs_lock);
		pnp->pr_free = procfs_freelist;
		procfs_freelist = pnp;
		procfs_nfree++;
		mutex_spinunlock(&procfs_lock, s);
		goto again;
	}
	VN_FLAGSET(vp, VNOSWAP | VINACTIVE_TEARDOWN);
	bhv_desc_init(PRNODETOBHV(pnp), pnp, vp, &prvnodeops);
	vn_bhv_insert_initial(VN_BHV_HEAD(vp), PRNODETOBHV(pnp));
	pnp->pr_vnode = vp;
	if (is_procdir)
		pnp->pr_mode = 0600;	/* R/W only by owner */
	else
		pnp->pr_mode = 0444;
	pnp->pr_type = is_procdir ? PR_PROCDIR : PR_PSINFO;
	pnp->pr_tpid = p->p_pid;

	/* pr_proc is only valid when the node is locked (via prlock)
	 * Since we are not locking this node, set it to NULL.
	 */
	pnp->pr_proc = NULL;
	pnp->pr_free = NULL;
	pnp->pr_opens = 0;
	pnp->pr_writers = 0;
	pnp->pr_pflags = 0;
#ifdef PRCLEAR
	pnp->pr_listproc = p;
	p->p_prnode_refcnt++;
#endif
	pnp->pr_nested = 0;
	/*
	 * If there is a left-over invalid /proc vnode, link it to
	 * the new vnode.
	 */
	if (is_procdir) {
#ifdef DEBUG
		if (p->p_trace != NULL) {
			ASSERT(p->p_trace->pr_pflags & PRINVAL);
		}
#endif
		pnp->pr_next = p->p_trace;
		p->p_trace = pnp;
	} else {
		ASSERT(p->p_pstrace == NULL);
		pnp->pr_next = p->p_pstrace;
		p->p_pstrace = pnp;
	}
	p_unlock(p, s);

	return vp;
}

/*
 * Start a process.
 * XXX	Can't name this routine 'prstart', because lboot would insert
 * XXX	it in the io_start table.
 */
void
pr_start(uthread_t *ut)
{
	int s = ut_lock(ut);

	ut->ut_flags &= ~UT_PRSTOPBITS;

	/*
	 * Caller is doing run-on-last-close: set uthread in motion.
	 */
	if (ut->ut_whystop != 0 &&
	    ut->ut_whystop != JOBCONTROL &&
	    ut->ut_flags & UT_STOP)
		unstop(ut);

	ut_unlock(ut, s);
}

/*
 * Stop a process thread.
 */
void
prstop_ut(register uthread_t *ut)
{
	int s = ut_lock(ut);

	if (!(ut->ut_flags & UT_STOP) || ut->ut_whystop == JOBCONTROL) {
		ut->ut_flags |= UT_PRSTOPX;
	}
	ut_unlock(ut, s);
}

struct uthread_s *
prchoosethread(struct proc *p)
{
	return prxy_to_thread(&p->p_proxy);
}

/*
 * prblocktrace deals with situations in which we are about to 
 * cause the current thread to block and we may have to wake up someone
 * waiting (in procfs) for the current thread or process to either
 * block or stop.  A test in the inline routine ut_block should ensure
 * that we are called relatively infrequently.
 *
 * Note that ut should be the current uthread, it should be locked and
 * we may not block.  This is a very restricted environment.
 */
int
prblocktrace(struct uthread_s *ut)
{	
        int             s, ret = 0;
	extern sv_t	proctracewait;	/* sleepers waiting for proc to stop */
	proc_t 		*cp = UT_TO_PROC(ut);
	proc_proxy_t 	*px = ut->ut_pproxy;
	int		dopollwake = 0;

        ASSERT(ut == curuthread && ut_islocked(ut));

	/*
	 * XXX If you try to wait for a process to 'stop' that
	 * itself if waiting for someone to stop this code
	 * really won't work, because svp == proctracewait.
	 * One would really need to do the sv_broadcast AFTER going to sleep
	 * but BEFORE releasing ut_lock - but we can't grab proctracelock then
	 * (AB-BA) - so we punt for now.
	 */
	if (UT_TO_KT(ut)->k_wchan == (caddr_t)&proctracewait)
	        return (0);

	if (ut->ut_flags & UT_TRWAIT) {
	        ut->ut_flags &= ~UT_TRWAIT;
		ret = 1;
	}
	if (ut->ut_flags & UT_HOLDJS) {
		ASSERT(ut->ut_flags & UT_PRSTOPBITS);
		ASSERT(px->prxy_utshare);
		ASSERT(px->prxy_flags & PRXY_JSARMED);
		ut->ut_flags &= ~UT_HOLDJS;
		s = prxy_stoplock(px);
		dopollwake = jsacct(px, 1, &ret);
		prxy_stopunlock(px, s);
	}
	else if (ut->ut_flags & UT_PRSTOPBITS) {
		/*
		 * We might be here on the 2nd sleep inside the kernel.
		 * The 1st sleep probably took away our UT_HOLDJS. It
		 * is quite possible that a PIOCWSTOP/prpoll process
		 * missed catching us in the isstopped() state. Lets
		 * wake them up again.
		 */
		dopollwake = jsacct(px, 0, &ret);
	}

	if (cp->p_trace && dopollwake && (px->prxy_flags & PRXY_JPOLL))
		prpollwakeup(cp->p_trace, POLLIN|POLLPRI|POLLOUT|POLLRDNORM|
							POLLRDBAND);
	return (ret);
}

/*
 * prmulti is called when a program first becomes multi-thread to allow us
 * to make the any necessary changes to deal with the fact of having 
 * multiple threads.  No complementary call is done if the process goes
 * back to having a single thread since the multithread way of doing things
 * should work fine, if not quite as efficiently.
 *
 * We are call with uscan_update held.
 */
void
prmulti(struct proc *p)
{
        int s;
        proc_proxy_t *px = &p->p_proxy;
        uthread_t *ut = prxy_to_thread(px);
	vasid_t		curvasid;
	pas_t		*pas;
	ppas_t		*ppas;
	
	ASSERT(ut == curuthread);
	ASSERT(!(px->prxy_flags & PRXY_PSTOP));
	s = p_lock(p);
	if (p->p_dbgflags & KPRJSTOP)
		prxy_flagset(&p->p_proxy, PRXY_JSTOP);
	if (p->p_dbgflags & KPRJPOLL)
		prxy_flagset(&p->p_proxy, PRXY_JPOLL);
	p_unlock(p, s);

	/*
	 * Text privatization handling stuff now. First mark the pas
	 * as belonging to a pthread.
	 */
	as_lookup_current(&curvasid);
	pas = VASID_TO_PAS(curvasid);
	ppas = (ppas_t *)curvasid.vas_pasid;
	pas_flagset(pas, PAS_PSHARED);

	/*
	 * Now copy over all the private text regions into
	 * the shared list for everyone to share.
	 */
	VAS_LOCK(curvasid, AS_EXCL);
	vungetprivatespace(pas, ppas);
	VAS_UNLOCK(curvasid);
}


#ifdef PRCLEAR
/*
 * break up the relationsship between a process and its prnodes
 * Normally pr_listproc points to it's associated process, when 
 * this pointer is reset to NULL during exit time from pproc_reap()
 * the vnode layer will not reference the process anymore (the initial
 * problem was setting a lock in prvnops.c:prinactive() on a process
 * which already had exited
 */
void
prclear(struct proc *p)
{
	register prnode_t *prn;
	register prnode_t *nprn;	/* elements placed back on linked */
	register prnode_t *p_trace;
	register prnode_t *p_pstrace;

	if (p->p_prnode_refcnt == 0) 
		return;

	if (p_pstrace = p->p_pstrace) {
		p->p_pstrace = NULL;

        	for (prn = p_pstrace ; prn != NULL; prn = prn->pr_next) {
			if (test_and_set_ptr((void **)&prn->pr_listproc,NULL)){
				p->p_prnode_refcnt--;
			} else {
				if (p->p_pstrace == NULL) {
					p->p_pstrace = nprn = prn;
				} else {
					nprn->pr_next = prn;
					nprn          = prn;
				}
			}
                }
		if (p->p_pstrace)
			nprn->pr_next = NULL;
	}

	if (p_trace = p->p_trace) {
		p->p_trace = NULL;
        	for (prn = p_trace ; prn != NULL; prn = prn->pr_next ) {
			if (test_and_set_ptr((void **)&prn->pr_listproc,NULL)){
				p->p_prnode_refcnt--;
			} else {
				if (p->p_trace == NULL) {
					p->p_trace = nprn = prn;
				} else {
					nprn->pr_next = prn;
					nprn          = prn;
				}
			}
                }

		if (p->p_trace)
			nprn->pr_next = NULL;
        }
}
#endif

/*
 * Obtain step right for multi-threaded process.
 *
 * The value returned is one iff only iff the step right was successfully
 * obtained.
 */
int 
prsright_obtain_self(void)
{
        uthread_t *ut = curuthread;
	pshare_t *ps;
	int error;
	int ret = 0;
        int s;

	ASSERT(ut);

	if (ut->ut_flags & (UT_SRIGHT | UT_STEP) != UT_STEP)
		return (1);
	ps = ut->ut_pproxy->prxy_utshare;
	if (ps == NULL)
		return (1);
	while (1) {
		mutex_lock(&ps->ps_stepmutex, PZERO);
		if ((ps->ps_stepflags & PSSTEP_GOTTEN) == 0) {
		        ps->ps_stepflags |= PSSTEP_GOTTEN;
			mutex_unlock(&ps->ps_stepmutex);
			ret = 1;
			break;
		} else {
		        ps->ps_stepflags |= PSSTEP_WAIT;
			error = sv_wait_sig(&ps->ps_stepsv, PZERO+1, 
					    &ps->ps_stepmutex, 0);
			if (error)
				break;
			if (ut->ut_flags & UT_PRSTOPBITS)
				break;
		}
	}
	if (ret) {
                int rls = 0;

		s = ut_lock(ut);
		if ((ut->ut_flags & (UT_SRIGHT | UT_STEP)) == UT_STEP) {
			ut->ut_flags |= UT_SRIGHT;
			ps->ps_steput = ut;
		}
		else 
			rls = 1;
		ut_unlock(ut, s);
		if (rls) 
			prsright_release(ut->ut_pproxy);
	}
	/*
	 * Make sure that we stop if asked - we could be a little more
	 * clever and only do the stop if we have already been counted
	 * as stopped (ACCTJS) while we were sleeping for ps_stepmutex.
	 * If a thread has already gotten the right, it will drop it in
	 * stop.
	 */
	if (ut->ut_flags & UT_PRSTOPBITS)
		stop(ut, REQUESTED, 0, 0);
	return (ret);
}

/*
 * Determines whether the current thread needs to get a step right
 */
int
prsright_needed(void)
{
        uthread_t *ut = curuthread;
        int s;
        int flags;

	ASSERT(ut);
        s = ut_lock(ut);
	flags = ut->ut_flags & (UT_PTHREAD | UT_STEP | UT_SRIGHT);
	ut_unlock(ut, s);
	return (flags == (UT_PTHREAD | UT_STEP));
}

/*
 * Release step right for multi-threaded process
 */
void
prsright_release(proc_proxy_t *px)
{
	pshare_t *ps = px->prxy_utshare;
	int wake = 0;

	ASSERT(ps);

	mutex_lock(&ps->ps_stepmutex, PZERO);
	ASSERT(ps->ps_stepflags & PSSTEP_GOTTEN);
	wake = ps->ps_stepflags & PSSTEP_WAIT;
	ps->ps_stepflags = 0;
	ps->ps_steput = NULL;
	mutex_unlock(&ps->ps_stepmutex);
	if (wake)
		sv_broadcast(&ps->ps_stepsv);
}

/*
 * Joint stop initiators like prstop_proc and asv_stopall call this, as
 * well as individual threads stopping in dostop and prblocktrace, when
 * they see a joint-stop is in progress and they need to count themselves.
 * If return value is 0, joint stop has not terminated yet.
 * If return value is 1, joint stop has terminated - pwait then indicates
 * whether any PIOCWSTOPer should be woken up.
 * prblocktrace and dostop call this with dec = 0 w/o stoplock to see
 * if they should wake up waiting waiters.
 */
int
jsacct(proc_proxy_t *px, int dec, int *pwait)
{
	int ret = 0;

	if (dec == 0) {
		/*
		 * This allows about-to-sleep/stop threads to enquire
		 * whether wstop/prpoll waiters need to be woken up.
		 */
		if (px->prxy_jscount == 0) {
			ret = 1;
			*pwait |= (prxy_flagclr(px, PRXY_PWAIT) & PRXY_PWAIT);
		}
	} else {
		ASSERT(px->prxy_flags & PRXY_JSARMED);
		ASSERT(px->prxy_jscount > 0);
		if ((px->prxy_jscount -= dec) == 0) {
			ret = 1;
			*pwait |= (px->prxy_flags & PRXY_PWAIT);
			/*
			 * Threads created after PRXY_JSTOPPED is set
			 * will stop (see uthread_create).
			 */
			prxy_nflagset(px, PRXY_JSTOPPED);
			prxy_nflagclr(px, PRXY_JSARMED|PRXY_PWAIT|PRXY_PSTOP);
			jstop_wake(px);
		}
	}
	return(ret);
}


/*
 * Stop an entire process
 */
void
prstop_proc(struct proc *p, int requested)
{
	proc_proxy_t *px = &p->p_proxy;
        uthread_t *ut;
	int s, ss;
        int dec = 0, dowake = 0, pwake = 0, dowork = 0;
	int utflags;

        uscan_update(px);
	if (requested && (px->prxy_utshare == NULL)) {
		/*
		 * This can never be true in the prstopj case.
		 */
		prstop_ut(prxy_to_thread(px));
		uscan_unlock(px);
		return;
	}
	/*
	 * PRXY_PSTOP meaningful only for threads which are already
	 * pthreaded. If the thread is not pthreaded yet, then the
	 * above stop request stops the thread before it can create
	 * another uthread.
	 */
	if (requested) {
		utflags = UT_PRSTOPX;
	} else {
		utflags = UT_PRSTOPJ;
	}

	ss = prxy_stoplock(px);
	if (requested) {
		/*
		 * If someone else has already started joint stop, stop.
		 */
		if ((px->prxy_flags & PRXY_JSARMED) == 0)
			dowork = 1;
	} else {
		/*
		 * Has prletrun reset jsarmed and indicated no stop?
		 */
		if (px->prxy_flags & PRXY_JSARMED)
			dowork = 1;
	}
	if (dowork) {
		ASSERT((px->prxy_flags & PRXY_PSTOP) == 0);
		ASSERT(px->prxy_jscount == 0);
		if (requested)
			prxy_nflagset(px, PRXY_JSARMED|PRXY_PSTOP);
		px->prxy_jscount = px->prxy_nthreads;
		prxy_stopunlock(px, ss);
        	uscan_forloop(px, ut) {
			s = ut_lock(ut);
			if ((ut->ut_flags & UT_STOP) &&
			    (ut->ut_whystop != JOBCONTROL)) {
				dec++;
			} else if (UT_TO_KT(ut)->k_flags & KT_SLEEP) {
				ut->ut_flags |= utflags;
				dec++;
			} else {
		                ut->ut_flags |= (utflags | UT_HOLDJS);
			}
			ut_unlock(ut, s);
		}
		ss = prxy_stoplock(px);
		if (dec)
			dowake = jsacct(px, dec, &pwake);
	}
	prxy_stopunlock(px, ss);
	uscan_unlock(px);
	if (p->p_trace && dowake && (px->prxy_flags & PRXY_JPOLL))
		prpollwakeup_any(p->p_trace, POLLIN|POLLPRI|POLLOUT|POLLRDNORM|
								POLLRDBAND);
	if (pwake)
		prawake();
}
