/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.42 $"

#include <sys/types.h>
#include <sys/proc.h>
#include <ksys/vproc.h>
#include <sys/cred.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/ksignal.h>
#include <sys/par.h>
#include <sys/prctl.h>
#include <sys/rtmon.h>
#include <sys/vnode.h>
#include <ksys/vsession.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include "pproc.h"
#include "pproc_private.h"

#define tracing(p, sig) \
		(((p)->p_flag & STRC) || sigismember(&(p)->p_sigtrace, sig))

int
pproc_sendsig(
	bhv_desc_t *bhv,
	int sig,
	int flags,
	pid_t sid,
	cred_t *scred,
	k_siginfo_t *info)
{
	proc_t	*p;
	proc_proxy_t	*prxy;

	extern int maxsigq;

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);

	/* Perform validity checks for null signal or zombies */
	if (sig < 1 || sig >= NSIG || p->p_stat == SZOMB)
                return (flags & (SIG_HAVPERM|SIG_ISKERN)) ||
			hasprocperm(p, scred) ? 0 : EPERM;
	
	/* Procfs has its own permission checks, so it calls with
	 * SIG_HAVPERM, but it is still not allowed to kill init
	 */
	if (!(flags & SIG_ISKERN)) {
		if (p->p_pid == INIT_PID && sigismember(&cantmask, sig))
			return EPERM;

		/* If maxsigq is configured, make sure the target procs
		 * queued signals are not maxed-out.
		 *
		 * NOTE: We really only care about this if the signal
		 * is being sent from user-level.
		 */
		if (maxsigq && p->p_sigvec.sv_pending >= maxsigq)
			return EAGAIN;
	}

	if (!(flags & (SIG_HAVPERM|SIG_ISKERN))) {
		if (!hasprocperm(p, scred) && 
		    !(sig == SIGCONT && p->p_sid == sid))
			return EPERM;
	}

	/*
	 * Process has appropriate privilege - post the signal
	 */
	/* Special delivery of interrupt signal when sent to proc.
	 */
	if (sig == SIGPTINTR) {
		
		prxy = &p->p_proxy;
		uscan_access(prxy);
		if (!prxy->prxy_sigthread) {
			uscan_unlock(prxy);
			return (ESRCH);
		}
		sigtouthread(prxy->prxy_sigthread, sig, info);
		uscan_unlock(prxy);
		return 0;
	}
	sigtoproc(p, sig, info);
	return 0;
}

/*
 * post a signal to a process
 *
 * Most of this code should get pushed into os/proc/sig_subr.c,
 * and made available to the ds and dc layers.
 *
 * Note that just because the proc resides on a particular cell, its
 * uthread (or uthreads) isn't necessarily running there, so we have
 * to virtualize the call to deliver the signal to the uthread.
 */
void
sigtoproc(
	struct proc *p,
	int sig,
	k_siginfo_t *info)
{
	vproc_t		*vp;
	__uint64_t	sigbit;
	sigvec_t	*sigvp;
	int		rv;
	sigqueue_t	*sqp;

#ifdef DEBUG
	extern	int signaltrace;
	if (signaltrace == -1 || signaltrace == p->p_pid) {
		cmn_err(CE_CONT,
			"signal %d (0x%x) sent by [%s,0x%x] to pid %d\n",
			sig, sigmask(sig),
			get_current_name(),
			curuthread,
			p->p_pid);
	}
#endif

	sigbit = sigmask(sig);

	/*
	 * Notify rtmond of the signal.  This actually generates more events
	 * than we typically want since this will cause events to be
	 * generated for every process in the system when we often are just
	 * tracing a small handful of processes.  We may need to think about
	 * ways of trying to avoid unneeded events ...
	 */
	/* NB: sig is qual 2 for consistency with TSTAMP_EV_SIGRECV */
	LOG_TSTAMP_EVENT(RTMON_SIGNAL,
		TSTAMP_EV_SIGSEND, p->p_pid, NULL, sig, NULL);

	vp = PROC_TO_VPROC(p);

	if (sig == SIGCONT) {
		VPROC_SIG_THREADS(vp, sig, info);
		return;
	}

	if (sigbitismember(&stopdefault, sigbit, sig)) {
		if (sig != SIGSTOP) {
			/*
			 * Can't send ctty related stop signals to members
			 * of orphaned process groups.
			 */
			int	is_orphaned;
			if (p->p_vpgrp != NULL)
				VPGRP_GETATTR(p->p_vpgrp,
					      NULL, &is_orphaned, NULL);
			if (p->p_vpgrp == NULL || is_orphaned) {
				VPROC_GET_SIGVEC(vp, VSIGVEC_PEEK, sigvp);
				if (!sigbitismember(&sigvp->sv_sigcatch,
								sigbit, sig)
				   && !tracing(p, sig))
					return;
			}
		}
				
		VPROC_SIG_THREADS(vp, sig, info);
		return;
	}

	/* If not threaded or the signal is guaranteed fatal we send
	 * it to all threads.
	 */
	if (!(p->p_proxy.prxy_shmask & PR_THREADS) || sig == SIGKILL) {
		VPROC_SIG_THREADS(vp, sig, info);
		return;
	}

	/*
	 * If the signal is _not_ a job control signal and the process
	 * is multi-threaded, deliver the signal to only one of the
	 * uthreads.  Otherwise, drop through and deliver the signal to
	 * all the threads (the only thread, of course, if this is a
	 * single-threaded process).
	 */
	sqp = NULL;
again:
	VPROC_GET_SIGVEC(vp, VSIGVEC_UPDATE, sigvp);

	/*
	 * If the process is ignoring the signal, or not catching
	 * an ignored signal (and not being traced), no thread's
	 * going to get this, so don't bother trying.
	 */
	if (sigbitismember(&sigvp->sv_sigign, sigbit, sig) ||
	    (sigbitismember(&ignoredefault, sigbit, sig)
	     && !sigbitismember(&sigvp->sv_sigcatch, sigbit, sig)
	     && !tracing(p, sig))) {
		VPROC_PUT_SIGVEC(vp);
		return;
	}

	/*
	 * If a siginfo structure is passed, create a sigqueue
	 * structure and queue it on the sigvecd structure --
	 * but only if the process is interested in siginfos.
	 * The trysig_sigvec routine will pass the siginfo to
	 * the target uthread.
	 */
	if (info && !sqp &&
	    sigbitismember(&sigvp->sv_sainfo, sigbit, sig) &&
	    (!sigbitismember(&sigvp->sv_sigign, sigbit, sig) ||
	     tracing(p, sig))) {
		sqp = sigqueue_alloc(KM_NOSLEEP);
		if (!sqp) {
			VPROC_PUT_SIGVEC(vp);
			sqp = sigqueue_alloc(KM_SLEEP);
			goto again;
		}
		bcopy((caddr_t)info, (caddr_t)&sqp->sq_info,
			sizeof(k_siginfo_t));
		if (sigaddq(&sigvp->sv_sigqueue, sqp, sigvp)) {
			VPROC_PUT_SIGVEC(vp);
			return;
		}
	} else
		if (sqp)
			sigqueue_free(sqp);

	/*
	 * Signal gets dropped into sigvec->sv_sig; siginfo queued
	 * onto sigvec->sv_sigqueue.  The trysig-thread code will
	 * try to deliver this, or any other sigvec-queued signal.
	 */
	sigbitaddset(&sigvp->sv_sig, sigbit, sig);
	VPROC_PUT_SIGVEC(vp);

	/*
	 * First, we warn all threads that they might have a signal
	 * pending. Then we look for threads that are eligible to
	 * receive the signal. Since UT_UPDSIG is just a hint, there's
	 * no harm in making some threads check unnecessarily for a 
	 * signal.
	 */
	setpsync(&p->p_proxy, UT_UPDSIG);	/* PR_THREADS */
	VPROC_TRYSIG_THREAD(vp, rv);
}

void
pproc_sig_threads(
	bhv_desc_t *bhv,
	int sig,
	k_siginfo_t *info)
{
	proc_t		*p;
	vproc_t		*vp;
	proc_proxy_t	*prxy;
	uthread_t	*ut;
	int		mode;
	sigvec_t 	*sigvp;
	int		s;
	__uint64_t	sigbit;
	int 		nthreads=0, sqp_count=0;
	sigqueue_t 	*sqp=NULL, *sqp_start=NULL;

	const struct timeval notime = {0, 0};

	sigbit = sigmask(sig);

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);
	vp = BHV_VOBJ(bhv);

	prxy = &p->p_proxy;

	/*
	 * XXX	Need a mechanism to prevent uthreads from being created on
	 * XXX	different cells while we're trying to deliver a signal to
	 * XXX	all uthreads.
	 */
	if (sig == SIGCONT) {
		VPROC_GET_SIGVEC(vp, mode = VSIGVEC_UPDATE, sigvp);
		p_flagclr(p, SJSTOP);
		if (!(p->p_flag & SIGNORED)) {
			vproc_t *vpr;
			/* REFERENCED */
			int junk;

			vpr = VPROC_LOOKUP(p->p_ppid);
			if (vpr != NULL) {
				if (!(p->p_flag & SIGNORED))
					VPROC_PARENT_NOTIFY(vpr, p->p_pid,
						CLD_CONTINUED, sig,
						notime, notime, p->p_pgid,
						VPGRP_SIGSEQ(p->p_vpgrp),
						0, junk);
				VPROC_RELE(vpr);
			}
		}
	} else if (sigbitismember(&stopdefault, sigbit, sig)) {
		VPROC_GET_SIGVEC(vp, mode = VSIGVEC_ACCESS, sigvp);
		s = p_lock(p);

		/*
		 * Ignored signals never get sent -- if /proc wants to
		 * capture signals, let it.  Until we can really cancel
		 * signals via /proc let's not do this.
		 */
		if (!(p->p_flag & SJSTOP)
		    && !sigbitismember(&sigvp->sv_sigign, sigbit, sig)
		    && !sigbitismember(&sigvp->sv_sighold, sigbit, sig)
		    && !sigbitismember(&sigvp->sv_sigcatch, sigbit, sig)
		    && !tracing(p, sig)) {

			/*
			 * We don't really want to wake the process just
			 * so it can stop.
			 * So we mark the process as SJSTOP, which it
			 * will notice the next time it calls psig().
			 * Note that we do post the signal -- the SJSTOP
			 * bit simply tells the psig/stop code that the
			 * parent has already been notified.
			 */
			p->p_flag |= SJSTOP;
			p_unlock(p, s);

			/*
			 * Send the parent a SIGCLD to let it know the status
			 * of this process has changed.
			 * To provide compatibility with earlier IRIXs that
			 * allowed processes to mix job control with sysV
			 * signals/wait the decision whether to send is as
			 * follows:
			 * By default signal/sigset turn on the POSIX SNOCLDSTOP
			 * bit, thus preventing normal sysV parents from getting
			 * unwanted SIGCLDs.
			 * To provide for parents that do NOT catch SIGCLD
			 * but are expecting to be woken out of a wait - we
			 * also kick a waiting parent.
			 * If this process has been inherited by init, don't
			 * send SIGCLD - inherited processes never signal init.
			 */
			if (!(p->p_flag & SIGNORED)) {
				/* REFERENCED */
				int junk;
				vproc_t *vpr = VPROC_LOOKUP(p->p_ppid);

				/*
				 * The outer check of SIGNORED is an
				 * optimization: if it is set, we can avoid the
				 * VPROC_LOOKUP.  It is the reference on the
				 * parent that protects SIGNORED, so we have
				 * to check again after the lookup.
				 */
				if (vpr != NULL) {
				    if (!(p->p_flag & SIGNORED))
					VPROC_PARENT_NOTIFY(vpr,
						p->p_pid,
						CLD_STOPPED, sig,
						notime, notime, p->p_pgid,
						VPGRP_SIGSEQ(p->p_vpgrp),
						0, junk);
				    VPROC_RELE(vpr);
				}

			}
		} else {
			p_unlock(p, s);
		}
	} else {
		VPROC_GET_SIGVEC(vp, mode = VSIGVEC_ACCESS, sigvp);
	}
	

	/*
	 * If a siginfo structure is passed, create a sigqueue structure
	 * and hand it to sigtouthread_common, if the process is interested
	 * in siginfos -- unless the signal is ignoring this signal and
	 * the process (uthread?) isn't being traced.
	 * The sigqueue struct is allocated here because sigqueue_alloc
	 * could sleep, and we need to have the target uthread ut_lock
	 * spinlock held when we call sigtouthread_common.
	 */
	if (info) {
		if (!sigismember(&sigvp->sv_sainfo, sig) ||
		    (sigismember(&sigvp->sv_sigign, sig) && !tracing(p, sig)))
			info = NULL;
	}

	/*
	 * Must lock the signal vector before the proxy uthread list in
	 * the following code to prevent possible deadlocks (PV 632732).
	 */

	/* Guess how many uthreads are attached to (unlocked) proxy. */ 
	nthreads = prxy->prxy_nthreads;

dosig:
	if (!info) {
		uscan_access(prxy);
		for (ut = prxy_to_thread(prxy); ut; ut = ut->ut_next)
			sigtouthread_common(ut, ut_lock(ut), sig, sigvp, NULL);
	} else {

		/* Create dynamic sigqueue_t list based on nthreads guess. */
		while (sqp_count < nthreads) {
			/*
			 * Copy the information over to a sigqueue_t structure.
			 * Don't sleep with sigvec lock held.
			 */
			sqp = sigqueue_alloc(KM_NOSLEEP);
			if (!sqp) {
				VPROC_PUT_SIGVEC(vp);
				sqp = sigqueue_alloc(KM_SLEEP);
				VPROC_GET_SIGVEC(vp, mode, sigvp);

				/* Has process changed mind about siginfos? */
				if (!sigismember(&sigvp->sv_sainfo, sig) ||
				    (sigismember(&sigvp->sv_sigign, sig) &&
				    !tracing(p, sig))) {
					sigqueue_free(sqp);
					while (sqp_start != NULL) {
						sqp = sqp_start;
						sqp_start = sqp_start->sq_next;
						sigqueue_free(sqp);
					}
					info = NULL;
					goto dosig;
				}
			}
			bcopy((caddr_t)info, (caddr_t)&sqp->sq_info, 
				sizeof(k_siginfo_t));
			sqp->sq_next = sqp_start;
			sqp_start = sqp;
			sqp_count++;
		}

		/* 
		 * Determine how many uthreads we really have based on the
		 * locked proxy.  If we did not allocate enough sigqueue_t
		 * structures, unlock the proxy and create some more. 
		 */
		uscan_access(prxy);
		if (sqp_count < prxy->prxy_nthreads) {
			nthreads = prxy->prxy_nthreads;
			uscan_unlock(prxy);
			goto dosig;
		}

		/* Call sigtouthread_common() using dynamic sigqueue_t list. */
		for (ut = prxy_to_thread(prxy); ut; ut = ut->ut_next) {
			sqp = sqp_start;
			ASSERT(sqp);
			sqp_start = sqp_start->sq_next;
			sigtouthread_common(ut, ut_lock(ut), sig, sigvp, sqp);
		}

		/*
		 * The number of uthreads may have decreased while or after
		 * we allocated our sigqueue_t structures.  If so, we have
		 * some extra structures to free up.
		 */
		while (sqp_start != NULL) {
			sqp = sqp_start;
			sqp_start = sqp_start->sq_next;
			sigqueue_free(sqp);
		}
	}

	if (IS_THREADED(prxy)) {
		if (sigbitismember(&stopdefault, sigbit, sig)) {
			/*
		 	 * Delay new thread creation so that they can not 
		 	 * start in user mode.
		 	 */
			create_hold(prxy);
		}
		else if (sig == SIGCONT) {
			/*
		 	 * Allow new threads to be created now.
		 	 */
			create_rele(prxy);
		}
	}
	uscan_unlock(prxy);
	VPROC_PUT_SIGVEC(vp);
}

int
pproc_trysig_thread(
	bhv_desc_t *bhv)
{
	proc_t	*p;
	vproc_t	*vp;
	proc_proxy_t *prxy;
	uthread_t *ut;
	sigvec_t *sigvp;
	__uint64_t sigbit;
	int sig;
	int delivered;
	sigqueue_t *sqp;

	p = BHV_TO_PROC(bhv);
	vp = BHV_VOBJ(bhv);
	ASSERT(p != NULL);

	prxy = &p->p_proxy;

	VPROC_GET_SIGVEC(vp, VSIGVEC_UPDATE, sigvp);
	uscan_access(prxy);

	if (sigisempty(&sigvp->sv_sig))
		goto out;

	for (sig = 1, sigbit = sigmask(1);
	     sig < NSIG;
	     sig++, sigbit <<= 1) {
again:
		if (!sigbitismember(&sigvp->sv_sig, sigbit, sig))
			continue;

		ASSERT(!sigbitismember(&jobcontrol, sigbit, sig));

		/*
		 * Must take signal out of sv_sig before dequeueing
		 * siginfo -- sigdeq will set sv_sig on way out if
		 * there's another siginfo pending.
		 */
		sigbitdelset(&sigvp->sv_sig, sigbit, sig);
		sqp = sigdeq(&sigvp->sv_sigpend, sig, sigvp);

		delivered = 0;

		for (ut = prxy_to_thread(prxy); ut; ut = ut->ut_next) {

			int s = ut_lock(ut);

			/*
			 * If this uthread is waiting for this signal,
			 * or isn't holding the signal, or if the signal
			 * will be fatal to the process, deliver it.
			 */
			if ((ut->ut_flags & UT_INKERNEL) &&
			    (sigbitismember(&ut->ut_sigwait, sigbit, sig) ||
			     !sigbitismember(ut->ut_sighold, sigbit, sig))) {
				sigtouthread_common(ut, s, sig, sigvp, sqp);
				sqp = NULL;
				delivered = 1;
				break;
			} else
				ut_unlock(ut, s);
		}

		/*
		 * If the signal didn't get delivered, put it back in
		 * the signal bucket.  Otherwise, see if there was
		 * _another_ siginfo queued, before continuing.
		 */
		if (!delivered) {
			sigbitaddset(&sigvp->sv_sig, sigbit, sig);
			if (sqp)
				sigaddq(&sigvp->sv_sigqueue, sqp, sigvp);
		} else
			goto again;
	}

	if (!sigisempty(&sigvp->sv_sig)) {
		uscan_unlock(prxy);
		VPROC_PUT_SIGVEC(vp);
		return ESRCH;
	}
out:
	uscan_unlock(prxy);
	VPROC_PUT_SIGVEC(vp);
	return 0;
}

sigvec_t *
pproc_get_sigvec(bhv_desc_t *bhv, int flag)
{
	proc_t	*p = BHV_TO_PROC(bhv);
	sigvec_t *sigvp = &p->p_sigvec;

	ASSERT(p != NULL);

	if (flag == VSIGVEC_UPDATE)
		sigvec_lock(sigvp);
	else if (flag == VSIGVEC_ACCESS)
		sigvec_acclock(sigvp);
	else {
		ASSERT(flag == VSIGVEC_PEEK);
	}

	return sigvp;
}

void
pproc_put_sigvec(bhv_desc_t *bhv)
{
	proc_t	*p = BHV_TO_PROC(bhv);

	ASSERT(p != NULL);
	sigvec_unlock(&p->p_sigvec);
}

/*
 * Return old sigaction to caller.
 */
void
pproc_get_sigact(
	bhv_desc_t *bhv,
	int sig,
	k_sigaction_t *oact)
{
	proc_t	*p;
	vproc_t	*vp;
	__uint64_t sigbit = sigmask(sig);
	sigvec_t *sigvp;
	int flags = 0;

	p = BHV_TO_PROC(bhv);
	vp = BHV_VOBJ(bhv);
	ASSERT(p != NULL);

	VPROC_GET_SIGVEC(vp, VSIGVEC_ACCESS, sigvp);
	
	if (sigbitismember(&p->p_proxy.prxy_sigonstack, sigbit, sig))
		flags |= SA_ONSTACK;
	if (sigbitismember(&sigvp->sv_sainfo, sigbit, sig))
		flags |= SA_SIGINFO;
	if (sigbitismember(&sigvp->sv_sigrestart, sigbit, sig))
		flags |= SA_RESTART;
	if (sigbitismember(&sigvp->sv_sigresethand, sigbit, sig))
		flags |= SA_RESETHAND;
	if (sigbitismember(&sigvp->sv_signodefer, sigbit, sig))
		flags |= SA_NODEFER;

	/*
	 * SNOWAIT,SNOCLDSTOP is inherited across fork
	 * and are off by default on exec
	 */
	if (sig == SIGCLD) {
		if (sigvp->sv_flags & SNOCLDSTOP)
			flags |= SA_NOCLDSTOP;
		if (sigvp->sv_flags & SNOWAIT)
			flags |= SA_NOCLDWAIT;
	}

	oact->sa_flags = flags;
	oact->k_sa_handler = sigvp->sv_hndlr[sig-1];
	oact->sa_mask = sigvp->sv_sigmasks[sig-1];

	VPROC_PUT_SIGVEC(vp);
}

/*
 * A few things to keep in mind
 *
 * Under SysV, if parent ignores SIGCLD, then the children don't become
 * zombies when they exit.  We make a note of this by turning on SA_NOCLDWAIT
 * in the action flag when we implement ssig based on setsigact().
 *
 * Similarly, SA_RESETHAND and SA_NODEFER will be turned on
 * for the traditional SysV signal call.  This will reset the handler and
 * keep the dispatched signal from being blocked.
 *
 * By default SysV doesn't want SIGCLD when a child stop/contd.
 * so we turn on SA_NOCLDSTOP for SysV signal calls also.
 * SysV is also expected to send parent a SIGCLD for each zombie child,
 * to do that parent is expected to reinstall handler inside the sig handler,
 * the kernel will check for zombie children then and send the parent a SIGCLD.
 * IRIX does this for Posix also (though not required), but not for BSD.
 *
 * By default, under Posix and BSD, a SIGCLD will be generated for the parent
 * whenever a child stops. Only Posix allows the option of avoiding that by
 * setting SA_NOCLDSTOP.
 */
void
pproc_set_sigact(
	bhv_desc_t *bhv,
	int sig,
	void (*disp)(),
	k_sigset_t *mask,
	int flags,
	int (*sigtramp)())
{
	proc_t	*pp;
	vproc_t	*vp;
	__uint64_t sigbit = sigmask(sig);
	sigvec_t *sigvp;

	pp = BHV_TO_PROC(bhv);
	vp = BHV_VOBJ(bhv);
	ASSERT(pp != NULL);

	VPROC_GET_SIGVEC(vp, VSIGVEC_UPDATE, sigvp);
	if (sigtramp != NULL)
		pp->p_proxy.prxy_sigtramp = sigtramp;

	if (disp != SIG_DFL && disp != SIG_IGN) {
		if (flags & SA_SIGINFO) {
			sigbitaddset(&sigvp->sv_sainfo, sigbit, sig);
		} else
		if (sigbitismember(&sigvp->sv_sainfo, sigbit, sig)) {
			sigbitdelset(&sigvp->sv_sainfo, sigbit, sig);
			sigdelq(&sigvp->sv_sigqueue, sig, sigvp);
		}
		
		sigbitdelset(&sigvp->sv_sigign, sigbit, sig);
		sigbitaddset(&sigvp->sv_sigcatch, sigbit, sig);
		
		sigdiffset(mask, &cantmask);	/* clear KILL, STOP */
		
		sigvp->sv_sigmasks[sig-1] = *mask;
		
		if (!sigbitismember(&cantreset, sigbit, sig)) {
			if (flags & SA_RESETHAND) {
				sigbitaddset(&sigvp->sv_sigresethand,
						sigbit, sig);
			} else {
				sigbitdelset(&sigvp->sv_sigresethand,
						sigbit, sig);
			}
		}
		
		if (flags & SA_NODEFER) {
			sigbitaddset(&sigvp->sv_signodefer, sigbit, sig);
		} else {
			/* psig will block */
			sigbitdelset(&sigvp->sv_signodefer, sigbit, sig);
		}
		
		if (flags & SA_ONSTACK) {
			sigaddset(&pp->p_proxy.prxy_sigonstack, sig);
		} else {
			sigdelset(&pp->p_proxy.prxy_sigonstack, sig);
		}
		
		if (flags & SA_RESTART) {
                        sigbitaddset(&sigvp->sv_sigrestart, sigbit, sig);
                } else {
                        sigbitdelset(&sigvp->sv_sigrestart, sigbit, sig);
		}
		
	} else {
		/* SIG_DFL or SIG_IGN */

		if (disp == SIG_IGN ||
		    sigbitismember(&ignoredefault, sigbit, sig)) {
			int sig_l = sig;

			sigbitaddset(&sigvp->sv_sigign, sigbit, sig);
			sigbitdelset(&sigvp->sv_sig, sigbit, sig);
			sigdelq(&sigvp->sv_sigqueue, sig, sigvp);

			(void) uthread_apply(&pp->p_proxy, UT_ID_NULL,
					     sigdelete, &sig_l);

		} else {	/* SIG_DFL but not ignoredefault */

			sigbitdelset(&sigvp->sv_sigign, sigbit, sig);
		}

		sigemptyset(&sigvp->sv_sigmasks[sig-1]);
		sigbitdelset(&sigvp->sv_sigcatch, sigbit, sig);
	}

	sigvp->sv_hndlr[sig-1] = disp;	/* always do this */

	if (sig == SIGCLD) {
		k_siginfo_t si, *sip;

		if (sigismember(&sigvp->sv_sainfo, SIGCLD))
			sip = &si;
		else
			sip = NULL;

		if (flags & SA_NOCLDSTOP) {
			sigvp->sv_flags |= SNOCLDSTOP;
		} else {
			sigvp->sv_flags &= ~SNOCLDSTOP;
		}
		
		if (flags & SA_NOCLDWAIT ||
		    disp == SIG_IGN && ((flags & _SA_BSDCALL) == 0)) {
			sigvp->sv_flags |= SNOWAIT;
			VPROC_PUT_SIGVEC(vp);
			freechildren(pp);
		} else {
			sigvp->sv_flags &= ~SNOWAIT;
			VPROC_PUT_SIGVEC(vp);
		}
		
		/*
	 	 * for SysV and Posix(IRIX special)
		 * BSD signals are implemented based on sigaction with
		 * _SA_BSDCALL on
		 */
		if ((flags & _SA_BSDCALL) == 0) {
			if (anydead(pp, sip))
				sigtoproc(pp, SIGCLD, sip);
		}
	} else
		VPROC_PUT_SIGVEC(vp);

	/*
	 * Let all uthreads know that they need to update their
	 * cached bits and (possibly) toss current signals and siginfos.
	 * We can't do this with sigvec lock held, because pproc_sig_thread
	 * needs to acquire uthread scan lock and then sigvec lock.
	 */
	if (pp->p_proxy.prxy_shmask & PR_THREADS) {
		setpsync(&pp->p_proxy, UT_UPDSIGVEC);
	}
}

void
sigsetcur(uthread_t *ut, int signo, k_siginfo_t *infop, int forceinfo)
{
	proc_t *p = UT_TO_PROC(ut);
	sigvec_t *sigvp = &p->p_sigvec;
	sigqueue_t *sqp;
	int s;

	sigvec_acclock(sigvp);
	if (signo && (infop || forceinfo) &&
	    sigismember(&sigvp->sv_sainfo, signo)) {
		k_siginfo_t *sip;

		sqp = sigqueue_alloc(KM_SLEEP);
		sip = &sqp->sq_info;

		/*
		 * If the caller provided a k_siginfo_t, use data from
		 * that, otherwise create the info.
		 */
		if (infop)
			*sip = *infop;
		else {
			bzero((caddr_t)sip, sizeof(k_siginfo_t));
			sip->si_signo = signo;
			sip->si_code = SI_USER;
			sip->si_pid = current_pid();
			sip->si_uid = ut->ut_cred->cr_ruid;
		}
	} else
		sqp = NULL;

	s = ut_lock(ut);
	ut->ut_cursig = signo;

	if (ut->ut_curinfo)
		sigqueue_free(ut->ut_curinfo);

	ut->ut_curinfo = sqp;
	ut_unlock(ut, s);

	sigvec_unlock(sigvp);
}


#define cantsend(p, ut, sig) \
	(sigismember(&(p)->p_sigvec.sv_sigign, (sig)) || \
	 sigismember((ut)->ut_sighold, (sig)))

int
pproc_ctty_access(
	bhv_desc_t	*bhv,
	enum jobcontrol	access)
{
	proc_t	*p = BHV_TO_PROC(bhv);
	uthread_t *ut = curuthread;
	int sig;
	dev_t devnum;
	vsession_t *vsp;

	/*
	 * Check if the session has a controlling terminal
	 */
	mraccess(&p->p_who);
	vsp = VSESSION_LOOKUP(p->p_sid);
	mraccunlock(&p->p_who);
	VSESSION_CTTY_DEVNUM(vsp, &devnum);
	VSESSION_RELE(vsp);
	if (devnum == NODEV) {
		/* if no controlling terminal */
		return EIO;
	}

	switch (access) {
	case JCTLGST:
		return 0;

	case JCTLREAD:
		if (cantsend(p, ut, SIGTTIN))
			return EIO;
		sig = SIGTTIN;
		break;

	case JCTLWRITE: 
		if (cantsend(p, ut, SIGTTOU))
			return -1;
		sig = SIGTTOU;
		break;
	}

	/*
	 * If the process group of the calling
	 * process is orphaned, return error.
	 * Other processes can change p_pgroup, so grab p_lock.
	 */
	{
		pid_t	sid;
		int	is_orphaned;
		VPGRP_GETATTR(p->p_vpgrp, &sid, &is_orphaned, NULL);
		if (is_orphaned)
			return EIO;
	}
	signal(p->p_pgid, sig);

	/*
	 * Note that only this process can change its own
	 * signal handling status.  Thus the above check
	 * that he doesn't ignore or hold signal
	 * means that we stop the process or catch the signal.
	 */
	if (checkstop())
		return EINTR;
	return 0;
}


int
pproc_ctty_clear(
	bhv_desc_t	*bhv)
{
	int error = 0;
	proc_t	*p = BHV_TO_PROC(bhv);

	mrlock(&p->p_who, MR_ACCESS, PZERO);
	if (p->p_pid == p->p_sid)	/* cannot be session leader */
		error = EINVAL;
	else
		p_flagset(p, SNOCTTY);
	mrunlock(&p->p_who);
	return error;
}


void
pproc_ctty_hangup(
	bhv_desc_t	*bhv)
{
	proc_t	*p = BHV_TO_PROC(bhv);
	vsession_t *vsp;

	/*
	 * Check that the process has a controlling terminal.
	 */
	mrlock(&p->p_who, MR_ACCESS, PZERO);
	vsp = VSESSION_LOOKUP(p->p_sid);
	mrunlock(&p->p_who);
	VSESSION_CTTY_DEALLOC(vsp, SESSTTY_HANGUP);
	VSESSION_RELE(vsp);
}
