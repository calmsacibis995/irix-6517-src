/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/* NOTES
 *
 * This module implements signal management.
 *
 * Key points:
	* The signal mask is in memory and as a k_sigset_t requires special
	  handling to manage it directly (format varies according to ABI)
	* Signals are sent as interrupts, that is the pthread is woken up
	  and then sends the signal to itself.
	* Signal handlers change the pthread mask so we have to intercept
	  them and fix the mask on entry and exit.
 */

#include "common.h"
#include "intr.h"
#include "pt.h"
#include "pthreadrt.h"
#include "sys.h"
#include "vp.h"
#include "sig.h"

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/timers.h>
#include <ucontext.h>

extern int	_ksigprocmask(int, const sigset_t *, sigset_t *);

#define PR_THREAD_KILL_BEARER	(pid_t)0xffff	/* XXX magic */


#define	SIG_IGNOREDEFAULT	{ sigmask(SIGCLD)	\
				  | sigmask(SIGCONT)	\
				  | sigmask(SIGWINCH)	\
				  | sigmask(SIGURG)	\
				  | sigmask(SIGPWR) }
static sigset_t		sig_restart;
static const sigset_t	sig_ignoredefault = SIG_IGNOREDEFAULT;
static sigset_t		sig_ignore = SIG_IGNOREDEFAULT;
static sigset_t		sig_noblock = { sigmask(SIGKILL) | sigmask(SIGSTOP) };

static void	(*sig_handlers[NSIG])(int, siginfo_t *, void *);
static void	sig_fixup_mask(int, siginfo_t *, void *);


#define KERNEL_ABI_IS_NEW	((PRDA->t_sys.t_flags & T_HOLD_KSIG_O32) == 0)

void		(*sig_set_to_kset)(sigset_t *, k_sigset_t *);
void		(*sig_kset_to_set)(k_sigset_t *, sigset_t *);
static void	sig_set_to_kset_newabi(sigset_t *, k_sigset_t *);
static void	sig_set_to_kset_oldabi(sigset_t *, k_sigset_t *);
static void	sig_kset_to_set_newabi(k_sigset_t *, sigset_t *);
static void	sig_kset_to_set_oldabi(k_sigset_t *, sigset_t *);


/*
 * sig_bootstrap()
 */
void
sig_bootstrap(void)
{
	sigaddset(&sig_noblock, SIGPTRESCHED);
	sigaddset(&sig_noblock, SIGPTINTR);

	/* Set up function pointers according to kernel signal
	 * mask format.  The PRDA mask is copied directly from
	 * the pthread signal mask.
	 */
	if (KERNEL_ABI_IS_NEW) {
		sig_set_to_kset = sig_set_to_kset_newabi;
		sig_kset_to_set = sig_kset_to_set_newabi;
	} else {
		sig_set_to_kset = sig_set_to_kset_oldabi;
		sig_kset_to_set = sig_kset_to_set_oldabi;
	}
}


/*
 * pthread_kill(pt_id, sig)
 *
 * Arrange for sig to be sent to pt_id.  If sig is 0, state whether pt_id
 * specifies to a valid thread or not.
 */
int
pthread_kill(pthread_t pt_id, int sig)
{
	register pt_t 	*pt;
	sigset_t	pt_sigmask;
	k_sigset_t	sig_noresched;

	if (sig < 0 || sig > MAXSIG) {
		return (EINVAL);
	}

	if ((pt = pt_ref(pt_id)) == NULL) {
		return (ESRCH);
	}
	TRACE(T_PT, ("pthread_kill(0x%x, %d)", pt, sig));

	/*
	 * If the sig that was passed in was 0, we just return now.  All
	 * the error checking we need do has already been finished.
	 */
	if (sig == 0) {
		pt_unref(pt);
		return (0);
	}

	/*
	 * Don't bother sending sig if it's ignored.
	 */
	if (sigismember(&sig_ignore, sig)) {
		pt_unref(pt);
		return (0);
	}

	/*
	 * If we're the target and the signal isn't blocked, send
	 * the signal immediately and return.
	 */
	if (pt == PT) {
		sig_kset_to_set(&pt->pt_ksigmask, &pt_sigmask);
		if (!sigismember(&pt_sigmask, sig)) {

			/* Disable resched so the target pthread will not
			 * change.  This can only happen if this signal is
			 * greater than SIGPTRESCHED and there is already
			 * a resched signal pending on this vp, but we
			 * never want to deliver a signal to an incorrect
			 * pthread.
			 */
			sigaddset(&pt_sigmask, SIGPTRESCHED);
			sig_set_to_kset(&pt_sigmask, &sig_noresched);
			VP_SIGMASK = sig_noresched;
			prctl(PR_THREAD_CTL, PR_THREAD_KILL,
				PR_THREAD_KILL_BEARER, sig);

			/* Unblock and deliver SIGPTRESCHED (if pending). */
			sigdelset(&pt_sigmask, SIGPTRESCHED);
			_ksigprocmask(SIG_SETMASK, &pt_sigmask, 0);
			pt_unref(pt);
			return (0);
		}
	}

	/*
	 * Add the signal to the target's pending set.  If the signal is
	 * blocked, return.  Otherwise, mark the thread as signalled and
	 * check to see if SIGPTINTR should be sent.
	 */
	sched_enter();
	lock_enter(&pt->pt_lock);

	sigaddset(&pt->pt_sigpending, sig);

	if (pt != PT) {
		sig_kset_to_set(&pt->pt_ksigmask, &pt_sigmask);
	}
	if (sigismember(&pt_sigmask, sig)
	    && !(pt->pt_sigwait && sigismember(pt->pt_sigwait, sig))) {
		lock_leave(&pt->pt_lock);
		sched_leave();
		pt_unref(pt);
		return (0);
	}

	pt->pt_signalled = TRUE;

	intr_notify(pt, INTR_SIGNAL);		/* Consumes pt_lock. */

	sched_leave();

	pt_unref(pt);
	return (0);
}


/*
 * pthread_sigmask(how, newset, oldset)
 *
 * This function is used to change the users signal mask.  If newset is
 * null, then the user just wants to examine their current set.
 *
 * If the signal mask changes, we may need to send signals that were
 * previously blocked.
 */
int
pthread_sigmask(int how, const sigset_t *newset, sigset_t *oldset)
{
	register pt_t	*pt_self = PT;
	sigset_t	set;
	sigset_t	pt_sigmask;

	TRACE(T_PT, ("pthread_sigmask(%d, 0x%08x%08x, 0x%x)", how,
		     newset ? newset->__sigbits[1] : NULL,
		     newset ? newset->__sigbits[0] : NULL,
		     oldset));

	if (newset
	    && how != SIG_BLOCK
	    && how != SIG_UNBLOCK
	    && how != SIG_SETMASK) {
		return (EINVAL);
	}

	/* User wants the old set returned.
	 */
	if (oldset != NULL) {
		sig_kset_to_set(&pt_self->pt_ksigmask, oldset);
	}

	/* If only looking, return.
	 */
	if (newset == NULL) {
		return (0);
	}
	set = *newset;

	/* Make sure the user doesn't try to mask special signals.
	 */
	sigdiffset(&set, &sig_noblock);
	sig_kset_to_set(&pt_self->pt_ksigmask, &pt_sigmask);

	/* Operate on the user version of the pthread's mask.
	 */
	switch(how) {

	case SIG_BLOCK:
		sigorset(&pt_sigmask, &set);
		break;

	case SIG_UNBLOCK:
		sigdiffset(&pt_sigmask, &set);
		break;

	case SIG_SETMASK:
		pt_sigmask = set;
		break;
	}

	sig_set_to_kset(&pt_sigmask, &pt_self->pt_ksigmask);
	VP_SIGMASK = pt_self->pt_ksigmask;

	/* Deliver any newly pending signals.
	 */
	if (sigisempty(&pt_self->pt_sigpending)) {
		return (0);
	}
	do {
		(void)sig_deliver_pending();
	} while (pt_self->pt_signalled);

	return (0);
}


/*
 * libc_sigprocmask(how, newset, oldset)
 *
 * Change sigprocmask() calls to pthread_sigmask().
 */
int
libc_sigprocmask(int how, const sigset_t *newset, sigset_t *oldset)
{
	int	retval;
	int	_sigprocmask(int, const sigset_t* , sigset_t *);

	ASSERT(PT);

	if (retval = pthread_sigmask(how, newset, oldset)) {
		setoserror(retval);
		return (-1);
	}

	return (0);
}


/*
 * sig_deliver_pending(void)
 *
 * Deliver a pending signal to this pthread.
 *
 * Returns whether or not an interrupted system call should be restarted.
 */
int
sig_deliver_pending(void)
{
	register pt_t		*pt_self = PT;
	sigset_t		set;
	sigset_t		pt_sigmask;
	k_sigset_t	sig_noresched;
	int 			s;
	int			restart = TRUE;

	TRACE(T_PT, ("sig_deliver_pending()"));

	sched_enter();
	lock_enter(&pt_self->pt_lock);

	pt_self->pt_signalled = FALSE;

	/* Remove any signals that are ignored.
	 */
	sigdiffset(&pt_self->pt_sigpending, &sig_ignore);

	/* Copy the pending signals and remove any that are blocked.
	 */
	set = pt_self->pt_sigpending;
	sig_kset_to_set(&pt_self->pt_ksigmask, &pt_sigmask);
	sigdiffset(&set, &pt_sigmask);

	if (!sigisempty(&set)) {

		/* Identify signal to be delivered.
		 */
		for (s = 1; s < NSIG; s++) {
			if (sigismember(&set, s)) {
				break;
			}
		}
		ASSERT(s != NSIG);

		sigdelset(&pt_self->pt_sigpending, s);
		sigdelset(&set, s);
		if (!sigisempty(&set)) {	/* more to go */
			pt_self->pt_signalled = TRUE;
		}

		lock_leave(&pt_self->pt_lock);
		sched_leave();

		/* Note whether this signal interrupts a syscall.
		 */
		if (!sigismember(&sig_restart, s)) {
			restart = FALSE;
		}

		/* Disable resched so the target pthread will not
		 * change.  This can only happen if this signal is
		 * greater than SIGPTRESCHED and there is already
		 * a resched signal pending on this vp, but we
		 * never want to deliver a signal to an incorrect
		 * pthread.
		 */
		sigaddset(&pt_sigmask, SIGPTRESCHED);
		sig_set_to_kset(&pt_sigmask, &sig_noresched);
		VP_SIGMASK = sig_noresched;

		/* Raise signal now.
		 */
		prctl(PR_THREAD_CTL, PR_THREAD_KILL, PR_THREAD_KILL_BEARER, s);

		/* Unblock and deliver SIGPTRESCHED (if pending). */
		sigdelset(&pt_sigmask, SIGPTRESCHED);
		_ksigprocmask(SIG_SETMASK, &pt_sigmask, 0);

	} else {
		lock_leave(&pt_self->pt_lock);
		sched_leave();
	}

	return (restart);
}


/*
 * libc_sigaction(sig, act, oact)
 *
 * The pthread version needs to track signal disposition so that
 * it can automatically ignore signals and also so that it can
 * keep the pthread signal mask with the changes made by the kernel
 * when it creates and deletes a signal handler context (sa_mask).
 */
int
libc_sigaction(int sig, const struct sigaction *act, struct sigaction *oact)
{
	struct sigaction	act_cpy;
	int			ret;
	void			(*ohandler)(int, siginfo_t *, void *);

	extern int _ksigaction(int, const struct sigaction *,
			       struct sigaction *, void (*)(int, ...));
	extern void _sigtramp(int, ...);

	/*
	 * User shouldn't be allowed to get or set information about
	 * pthread specific signals.
	 */
	if (sig == SIGPTINTR || sig == SIGPTRESCHED) {
		return (EINVAL);
	}

	if (oact) {
		ohandler = sig_handlers[sig];
	}

	if (act) {
		/*
		 * Make sure the user doesn't try to mask special signals.
		 */
		sigdiffset(&act->sa_mask, &sig_noblock);

		/*
		 * Update sig_restart and sig_ignore sets.
		 *
		 * If installing a real signal handler, set restart bit to
		 * be whatever user wants and remove this signal from the
		 * ignore set.
		 * Intercept the handler via our own so that we can sync up
		 * the pt signal mask.
		 *
		 * If handler is SIG_IGN or the default behavior for this
		 * signal is to ignore it, then add signal to the ignore
		 * set.
		 */
		if (act->sa_handler != SIG_DFL && act->sa_handler != SIG_IGN) {
			if (act->sa_flags & SA_RESTART) {
				sigaddset(&sig_restart, sig);
			} else {
				sigdelset(&sig_restart, sig);
			}

			act_cpy = *act;
			sig_handlers[sig] = act->sa_handler;
			act_cpy.sa_handler = sig_fixup_mask;
			act = &act_cpy;

			sigdelset(&sig_ignore, sig);
		} else if (act->sa_handler == SIG_IGN
			   || sigismember(&sig_ignoredefault, sig)) {
			sigaddset(&sig_ignore, sig);
		}
	}

	/* Make sure the user sees his own handlers
	 */
	if ((ret = _ksigaction(sig, act, oact, _sigtramp)) == 0 && oact
	    && oact->sa_handler != SIG_IGN && oact->sa_handler != SIG_DFL) {
		oact->sa_handler = ohandler;
	}
	return (ret);
}


/*
 * sig_fixup_mask(sig, info, arg)
 *
 * Invoke user handler after the pt mask is updated and reset before
 * return to the trampoline code.
 */
static void
sig_fixup_mask(int sig, siginfo_t *info, void *arg)
{
	k_sigset_t	old_kmask;
	pt_t		*pt_self = PT;

	ASSERT(!sched_entered());

	/* No signals are unblocked so we need not check pending.
	 */
	old_kmask = pt_self->pt_ksigmask;
	pt_self->pt_ksigmask = VP_SIGMASK;

	sig_handlers[sig](sig, info, arg);

	sched_enter();
	lock_enter(&pt_self->pt_lock);

	pt_self->pt_ksigmask = old_kmask;	/* restore pt mask */

	if (!sigisempty(&pt_self->pt_sigpending)) {
		lock_leave(&pt_self->pt_lock);

		/* Any signals raised will remain pending on this
		 * vp until it goes through sigreturn().
		 */
		(void)sig_deliver_pending();
	} else {
		lock_leave(&pt_self->pt_lock);
	}
	/* sched_leave() */
	VP->vp_occlude = 0;
}


/*
 * libc_sigtimedwait(set, info, ts)
 *
 * Returns 0 on success, -1 on error.
 */
int
libc_sigtimedwait(const sigset_t *sigset, siginfo_t *info, const timespec_t*ts)
{
	register pt_t	*pt_self = PT;
	sigset_t	set = *sigset;
	sigset_t	pend;
	siginfo_t	local_siginfo;
	int		i;
	int 		retval;
	int		old_blocked = pt_self->pt_blocked;
	extern int 	__sigpoll(const sigset_t*, struct siginfo*,
				 const struct timespec*);

	if (info == (siginfo_t *)NULL) {
		info = &local_siginfo;
	}

	sigdiffset(&set, &sig_noblock);

retry_sigwait:
	/* Warning: do not replace with PT_INTR_PENDING.
	 * It's important to bump blocking _after_ acquiring the pt lock
	 * otherwise we can deadlock with interrupt deliver.
	 */
	for (;;) {
		sched_enter();
		lock_enter(&pt_self->pt_lock);
		pt_self->pt_blocked += PT_CNCL_THRESHOLD;
		if (!(pt_self->pt_flags & PT_INTERRUPTS)) {
			break;
		}
		lock_leave(&pt_self->pt_lock);
		sched_leave();

		(void)intr_check(PT_INTERRUPTS);
	}

	pend = pt_self->pt_sigpending;
	sigdiffset(&pend, &sig_ignore);
	sigandset(&pend, &set);

	if (sigisempty(&pend)) {

		sigaddset(&set, SIGPTINTR);
		pt_self->pt_sigwait = (sigset_t *)&set;

		lock_leave(&pt_self->pt_lock);
		sched_leave();

		/*
		 * "PT_BLOCK" code.  See libc.c for details.
		 */
		if (pt_is_bound(pt_self)) {
			retval = __sigpoll(&set, info, ts);
		} else {
			VP_RESERVE();
			retval = __sigpoll(&set, info, ts);
			VP_RELEASE();
		}

		pt_self->pt_sigwait = NULL;
		pt_self->pt_blocked = old_blocked;

		if (retval == -1) {
			return (-1);
		}

		if (info->si_signo == SIGPTINTR) {
			goto retry_sigwait;
		}

		return (info->si_signo);
	}

	pt_self->pt_blocked = old_blocked;

	for (i = 1; i < NSIG; i++) {
		if (sigismember(&pend, i)) {
			break;
		}
	}

	sigdelset(&pt_self->pt_sigpending, i);

	lock_leave(&pt_self->pt_lock);
	sched_leave();

	info->si_code = SI_USER;
	info->si_errno = 0;

	return (info->si_signo = i);
}


/*
 * libc_sigwait(set, sig)
 *
 * Just use sigtimedwait().
 */
int
libc_sigwait(const sigset_t *set, int *sig)
{
	struct siginfo	info;

	if (libc_sigtimedwait(set, &info, NULL) == -1) {
		return (-1);
	}

	if (sig) {
		*sig = info.si_signo;
	}

	return (0);
}


/*
 * libc_sigpending(maskptr)
 *
 * Return the list of signals that are blocked and pending on this thread.
 */
int
libc_sigpending(sigset_t *maskptr)
{
	pt_t		*pt_self = PT;
	sigset_t	pt_sigmask;
	extern int 	__sigpending(sigset_t *);

	if (__sigpending(maskptr)) {
		return (-1);
	}

	/*
	 * clear any ignored sigs from the pending set first
	 */
	sigdiffset(&pt_self->pt_sigpending, &sig_ignore);

	sigorset(maskptr, &pt_self->pt_sigpending);
	sig_kset_to_set(&pt_self->pt_ksigmask, &pt_sigmask);
	sigandset(maskptr, &pt_sigmask);

	return (0);
}


/*
 * libc_raise(sig)
 *
 * Send sig to ourself.
 */
int
libc_raise(int sig)
{
	int error = pthread_kill(pthread_self(), sig);

	if (error) {
		setoserror(error);
		return (-1);
	}
	return (0);
}


/*
 * libc_sigsuspend(sig_mask)
 *
 * Replace the current signal mask with sig_mask and wait for any
 * non-masked signals to come in.  When a signal arrives deliver it
 * synchronously.
 *
 * Always returns -1 with errno set to EINTR.
 */
int
libc_sigsuspend(const sigset_t *sig_mask)
{
	sigset_t	mask;
	int		sig;

	/* Invert user mask so we can sigwait on it. */
	sigfillset(&mask);
	sigdiffset(&mask, sig_mask);

	ASSERT(!sched_entered());
	VP_SIGMASK = sched_mask;

	while (libc_sigwait(&mask, &sig) == -1) {
		;
	}
	ASSERT(!sigismember(sig_mask, sig));

	/* Make the signal pending on this vp then unblock just that
	 * signal via the kernel which will deliver it.
	 */
	prctl(PR_THREAD_CTL, PR_THREAD_KILL, PR_THREAD_KILL_BEARER, sig);
	mask = *sig_mask;
	sigdiffset(&mask, &sig_noblock);
	
	/* Don't unblock SIGPTRESCHED until we're sure the signal
	 * has been delivered. */
	sigaddset(&mask, SIGPTRESCHED);

	/* sched_leave() */
	_ksigprocmask(SIG_SETMASK, &mask, 0);	/* unblock it */

	/* Now unblock SIGPTRESCHED and deliver (if pending). */
	sigdelset(&mask, SIGPTRESCHED);
	_ksigprocmask(SIG_SETMASK, &mask, 0);

	/* handler runs with own mask */

	setoserror(EINTR);
	return (-1);
}


/*
 * libc_siglongjmp(env, val)
 *
 * Sync up the library's notion of the thread's sigmask with the
 * one in the ucontext.
 *
 * Potential bug: if thread is FIFO, may never see newly unmasked signals.
 * Need a way to do a sig_deliver_pending() on far side of siglongjmp().
 */
/* ARGSUSED */
void
libc_siglongjmp(sigjmp_buf env, int val)
{
	register ucontext_t	*ucp = (ucontext_t *)env;
	register pt_t		*pt_self = PT;

	if (ucp->uc_flags & UC_SIGMASK) {
		sig_set_to_kset(&ucp->uc_sigmask, &pt_self->pt_ksigmask);
		sched_enter();
		lock_enter(&pt_self->pt_lock);
		pt_self->pt_signalled = TRUE;
		lock_leave(&pt_self->pt_lock);
		sched_leave();
	}
}


/*
 * sig_abort()
 *
 * Determined suicide.
 */
void
sig_abort(void)
{
	sigset_t	pt_sigmask;

	sig_kset_to_set(&VP_SIGMASK, &pt_sigmask);
	sigdelset(&pt_sigmask, SIGABRT);
	sig_set_to_kset(&pt_sigmask, &VP_SIGMASK);
	prctl(PR_THREAD_CTL, PR_THREAD_KILL, PR_THREAD_KILL_BEARER, SIGABRT);
}


/*
 * sig_set_to_kset_newabi(u, k)
 *
 * Translate user mask to n32/64 kernel.
 */
void
sig_set_to_kset_newabi(sigset_t *u, k_sigset_t *k)
{
#if _MIPS_SIM != _ABIO32
	/*
	 * n32/64 lib, n32/64 kernel
	 */
	*k = ((k_sigset_t)u->__sigbits[1] << 32) | (k_sigset_t)u->__sigbits[0];
#else
	/*
	 * o32 lib, n32/64 kernel
	 */
	k->sigbits[0] = u->__sigbits[1];
	k->sigbits[1] = u->__sigbits[0];
#endif
}


/*
 * sig_set_to_kset_oldabi(u, k)
 *
 * Translate user mask to o32 kernel.
 */
void
sig_set_to_kset_oldabi(sigset_t *u, k_sigset_t *k)
{
#if _MIPS_SIM != _ABIO32
	/*
	 * n32/64 lib, o32 kernel
	 */
	*k = ((k_sigset_t)u->__sigbits[0] << 32) | (k_sigset_t)u->__sigbits[1];
#else
	/*
	 * o32 lib, o32 kernel
	 */
	k->sigbits[0] = u->__sigbits[0];
	k->sigbits[1] = u->__sigbits[1];
#endif
}


/*
 * sig_kset_to_set_newabi(k, u)
 *
 * Translate n32/64 kernel mask to user mask.
 */
void
sig_kset_to_set_newabi(k_sigset_t *k, sigset_t *u)
{
#if _MIPS_SIM != _ABIO32
	/*
	 * n32/64 lib, n32/64 kernel
	 */
	u->__sigbits[0] = (unsigned int)*k;
	u->__sigbits[1] = (unsigned int)(*k >> 32);
#else
	/*
	 * o32 lib, n32/64 kernel
	 */
	u->__sigbits[0] = k->sigbits[1];
	u->__sigbits[1] = k->sigbits[0];
#endif
	u->__sigbits[2] = 0;
	u->__sigbits[3] = 0;
}


/*
 * sig_kset_to_set_oldabi(k, u)
 *
 * Translate o32 kernel mask to user mask.
 */
void
sig_kset_to_set_oldabi(k_sigset_t *k, sigset_t *u)
{
#if _MIPS_SIM != _ABIO32
	/*
	 * n32/64 lib, o32 kernel
	 */
	u->__sigbits[0] = (unsigned int)(*k >> 32);
	u->__sigbits[1] = (unsigned int)*k;
#else
	/*
	 * o32 lib, o32 kernel
	 */
	u->__sigbits[0] = k->sigbits[0];
	u->__sigbits[1] = k->sigbits[1];
#endif
	u->__sigbits[2] = 0;
	u->__sigbits[3] = 0;
}
