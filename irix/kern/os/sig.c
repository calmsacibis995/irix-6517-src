/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1997 Silicon Graphics, Inc.		  *
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

#ident	"$Revision: 3.472 $"

#include <sys/types.h>
#include <sys/atomic_ops.h>
#include <ksys/as.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <ksys/vfile.h>
#include <sys/fpu.h>
#include <sys/kabi.h>
#include <sys/kmem.h>
#include <sys/ksignal.h>
#include <sys/kthread.h>
#include <sys/kucontext.h>
#include <sys/mman.h>
#include <sys/par.h>
#include <sys/param.h>
#include <sys/pcb.h>
#include <sys/pda.h>
#include <sys/poll.h>
#include <sys/prctl.h>
#include <ksys/vproc.h>
#include <ksys/vpgrp.h>
#include <sys/proc.h>
#include <sys/procset.h>
#include <sys/ptrace.h>
#include <sys/reg.h>
#include <ksys/vsession.h>
#include <sys/splock.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/timers.h>
#include <sys/uio.h>
#include <sys/vnode.h>
#include <sys/watch.h>
#include <procfs/prsystm.h>
#include <sys/mac_label.h>
#include <sys/schedctl.h>
#include <sys/runq.h>
#include <sys/xlate.h>
#include <sys/rtmon.h>
#include <sys/sat.h>
#include <ksys/exception.h>
#include <sys/lock.h>
#include "os/proc/pproc_private.h"
#include "fs/procfs/prdata.h"
#include <sys/buf.h>

#include <core.out.h>

#if !defined(CORE_OUT_H_REV) || CORE_OUT_H_REV < 1

This file requires a tot eoe/include/core.out.h in order to build
correctly.  Since there doesn't appear to be any easy way to
communicate this to everyone who might build this file (yes I've tried
email) I have placed this note here instead. You need to p_tupdate
eoe/include/core.out.h and then you may additionally need to do a
"make headers" from the eoe/include directory.

  -tcl

#endif

#include <string.h>
#ifdef ULI
#include <ksys/uli.h>
#endif

#define tracing(p, sig) \
		(((p)->p_flag & STRC) || (sigismember(&(p)->p_sigtrace, sig)))

/*
 * XXX not quite correct:
 *	- SIGCONT after sending SIGCLD but before parent waits
 *	- completely handle cancelled signal recovery
 *		would make some of the code simpler
 */
k_sigset_t	stopdefault = 	{ sigmask(SIGSTOP)|sigmask(SIGTSTP)|
				  sigmask(SIGTTIN)|sigmask(SIGTTOU) };
k_sigset_t	jobcontrol =	{ sigmask(SIGSTOP)|sigmask(SIGTSTP)|
				  sigmask(SIGTTIN)|sigmask(SIGTTOU) |
				  sigmask(SIGCONT)};
#if	(_MIPS_SIM != _ABIO32)
k_sigset_t 	fillset =	{ 0xffffffffffffffffLL };
k_sigset_t	ignoredefault =	{ sigmask(SIGCLD)|sigmask(SIGCONT)|
				  sigmask(SIGWINCH)|
				  sigmask(SIGURG)|sigmask(SIGPWR)|
				  sigmask(SIGPTRESCHED)|sigmask(SIGPTINTR)|
				  sigmask(SIGCKPT)|sigmask(SIGRESTART) };
#else
k_sigset_t      fillset =       { 0xffffffff, 0xffffffff };
k_sigset_t	ignoredefault =	{ sigmask(SIGCLD)|sigmask(SIGCONT)|
				  sigmask(SIGWINCH)|
				  sigmask(SIGURG)|sigmask(SIGPWR), /* comma */
				  sigmask(SIGPTRESCHED)|sigmask(SIGPTINTR)|
				  sigmask(SIGCKPT)|sigmask(SIGRESTART) };
#endif
k_sigset_t	coredefault =	{ sigmask(SIGQUIT)|sigmask(SIGILL)|
				  sigmask(SIGTRAP)|sigmask(SIGABRT)|
				  sigmask(SIGEMT)|sigmask(SIGFPE)|
				  sigmask(SIGBUS)|sigmask(SIGSEGV)|
				  sigmask(SIGSYS)|sigmask(SIGXCPU)|
				  sigmask(SIGXFSZ) };
k_sigset_t	cantmask =	{ sigmask(SIGKILL)|sigmask(SIGSTOP) };
k_sigset_t 	cantreset =	{ sigmask(SIGILL)|sigmask(SIGTRAP)|
				  sigmask(SIGPWR) };
k_sigset_t 	emptyset;

static int	core(int);
static int	isjobstop(sigvec_t *, int);
static int	procxmt(void);
static void	dostop(uthread_t *, ushort, ushort, int, int);
static int 	stopsub(uthread_t *, ushort, ushort);
static int	sigpoll_check(k_sigset_t *, k_siginfo_t *, sigvec_t *);
static int	sigpoll(k_sigset_t *, struct timespec *, k_siginfo_t *);
static int	killsingle(pid_t, int);
static int	killpg(pid_t, int);

static int	sigtoprocset(procset_t *, int sig);
static int	sigtoprocset_scanfunc(proc_t *, void *, int);
static void	irix5_savesigcontext(irix5_sigcontext_t *, k_sigset_t *);
static int	sendsig(void (*hdlr)(), int, struct k_siginfo *, int);

static void	irix5_extra_savecontext(irix5_ucontext_t *);
static void 	asv_stopall(async_vec_t *asv);

/*
 * In general, the signal routines assume that the proc entry is not
 * locked (by the current process) on entry, and that the entry is unlocked
 * (by the current process) before return.  Exceptions to this policy
 * are noted with the offending routine.  Assume that the proc entry lock
 * must be held for diddling with everything but the parent-child-sibling
 * chains and signal fields.
 */

/*
 * Priority for tracing
 */

#define	IPCPRI	PZERO

/*
 * Tracing variables.
 * Used to pass trace command from
 * parent to child being traced.
 * This data base cannot be
 * shared and is locked
 * per user.
 */
/*
 * Locking strategy: The tracing process locks this structure whenever it
 * wishes to send a command.  The structure stays locked until the command
 * is completed by the traced process.  The tracing process will sleep
 * waiting for the command to complete on the >ip_wait< semaphore.
 * When the child has finished the command, it zaps the wait semaphore,
 * which wakes up the parent, which unlocks the structure, and all is well.
 */
struct {
	mutex_t	ip_lock;
	sema_t	ip_wait;
	int	ip_req;
	int	*ip_addr;
	__psint_t	ip_data;	/* Used as a target for ptrs */
					/* and machreg_t types */
	uthread_t *ip_tptr;
	int	ip_pid;
	int	ip_uid;
} ipc;

/*
 * regmap defines the write protect status of each of the 'special' regs
 * these are defined in sys/ptrace.h
 */
#define	ADDR(x)	((char *)&(x))

struct regmap {
	char *rm_addr;
	unsigned rm_width;
	unsigned rm_wrprot;
} regmap[NSPEC_REGS] = {
	{ 0,	sizeof(k_machreg_t),	0 },
	{ 0,	sizeof(k_machreg_t),	1 },
	{ 0,	sizeof(k_machreg_t),	1 },
	{ 0,	sizeof(k_machreg_t),	0 },
	{ 0,	sizeof(k_machreg_t),	0 },
	{ 0,	sizeof(__uint32_t),	0 },
	{ 0,	sizeof(__uint32_t),	1 },
};

struct zone *sigqueue_zone;

/*
 * Initialize the ptrace communication buffer lock, and anything else
 * that needs to be set up for signals and ptrace to work right.
 */
void
siginit(void)
{
	mutex_init(&ipc.ip_lock, MUTEX_DEFAULT, "ptrlock");
	initnsema(&ipc.ip_wait, 0, "ptrwait");

	sigqueue_zone = kmem_zone_init(sizeof(sigqueue_t), "sigqueues");
}


/*
 * Send the specified signal to all processes with 'pgrp' as process group.
 * Called by streamio/accessctty for quits and interrupts.
 */
void
signal(register pid_t pgid, int sig)
{
	struct vpgrp *vpg;

	if (pgid == 0)
		return;

	/*
	 * Find the vpgpr, if it exists, and perform a SENDSIG operation
	 * on it.
	 */
	vpg = VPGRP_LOOKUP(pgid);
	if (vpg == NULL)
		return;
	VPGRP_SENDSIG(vpg, sig, SIG_ISKERN, 0, NULL, NULL);
	VPGRP_RELE(vpg);

}

#ifdef DEBUG
extern int signaltrace;
#endif

/*
 * sigqfree(p)
 *
 * Frees the sigqueue structures on both the pending and free queues.
 */
void
sigqfree(sigqueue_t **sqpp)
{
	sigqueue_t *sqp, *nsqp;

	sqp = *sqpp;
	*sqpp = NULL;

	while (sqp) {
		nsqp = sqp->sq_next;
		sigqueue_free(sqp);
		sqp = nsqp;
	}
}

/*
 * sigaddq(sigqueue_t **, sigqueue_t *)
 *
 * Place infop onto specified sigqueue.
 *
 * The siginfo structures of different signals are sorted in increasing
 * signal value to allow higher priority signals to be sent first.
 *
 * If this siginfo is posted as part of sigqueue or async I/O, then
 * multiple siginfo structures of the same signal are placed next to each
 * other in FIFO order.
 *
 * Returns 0 on success, EALREADY if this wasn't posted as part of a
 * sigqueue call and there was already a matching signal.
 *
 * This is called with sigvec-lock held (if queueing on sigvec) or
 * the uthread spinlock held (if queueing on uthread).
 */
int
sigaddq(sigqueue_t **sqpp, sigqueue_t *sqp, sigvec_t *sigvp)
{
	sigqueue_t *sqp1, *sqp2;
	int sig = sqp->sq_info.si_signo;

	ASSERT(sig >= 1 && sig < NSIG);

	/*
	 * Traverse siqqueue.  Use two pointers to keep track of first
	 * siginfo with signo greater than sig.  If siginfo is to be queued,
	 * it will go between these two pointers.
	 */
	sqp1 = *sqpp;
	sqp2 = NULL;
	while (sqp1 != NULL) {
		ASSERT(!sqp2 ||
			sqp2->sq_info.si_signo <= sqp1->sq_info.si_signo);

		if (sqp1->sq_info.si_signo > sig)
			break;

		sqp2 = sqp1;
		sqp1 = sqp1->sq_next;
	}

	/*
	 * Determine if siginfo is to be added to queue, and its final
	 * placement.
	 */
	if (sqp2 == NULL) {
		/*
		 * Special case: list is empty or this is the lowest signal
		 * with siginfo.  Automatically place this element at head.
		 */
		*sqpp = sqp;
		sqp->sq_next = sqp1;
	} else {
		/*
		 * There are several cases in which we can insert this
		 * siginfo:
		 *
		 * 	1) This is the first siginfo for this signal.
		 *	b) We were posted by sigqueue.
		 *	3) We were posted by async I/O.
		 *	d) etc...
		 *
		 * If any of these are true, add the new element after
		 * sqp2.
		 */
		if (sqp2->sq_info.si_signo != sig
		    || sqp->sq_info.si_code == SI_QUEUE
		    || sqp->sq_info.si_code == SI_ASYNCIO
		    || sqp->sq_info.si_code == SI_TIMER
		    || sqp->sq_info.si_code == SI_MESGQ) {
			sqp->sq_next = sqp2->sq_next;
			sqp2->sq_next = sqp;
		} else {
			/*
			 * An instance of this signal is already on the
			 * queue and we don't need to keep multiple copies.
			 * 
			 * Clean up after ourselves by placing sqp back on
			 * the free list and return EALREADY for those who
			 * care.
			 */
			sigqueue_free(sqp);

			return (EALREADY);
		}
	}

	atomicAddInt(&sigvp->sv_pending, 1);

	return(0);
}

/*
 * sigqueue_t *sigdeq(sigqueue_t **sqpp, sig)
 *
 * Remove the first element from sqpp that matches sig and return it.
 *
 * If there is another signal of the same type on the queue, re-set the
 * appropriate bit in s_sig.  NOTE: this implies that the bit is
 * cleared before calling sigdeq(), or that setting s_sig won't matter.
 */
sigqueue_t *
sigdeq(sigpend_t *sp, int sig, sigvec_t *sigvp)
{
	sigqueue_t **psqp, *sqp, *nextqp;

	/*
	 * Search for an element on the queue whose signo matches sig.
	 */
	for (psqp = &sp->s_sigqueue; sqp = *psqp; psqp = &sqp->sq_next) {
		if (sqp->sq_info.si_signo > sig)
			return NULL;
		if (sqp->sq_info.si_signo == sig)
			break;
	}

	if (!sqp)
		return NULL;

	nextqp = *psqp = sqp->sq_next;

	/*
	 * Check to see if there is another siginfo for this signal.  If
	 * so, turn on the corresponding bit in sp->s_sig
	 */
	if (nextqp && nextqp->sq_info.si_signo == sig) {
		sigaddset(&sp->s_sig, sig);
	}

	atomicAddInt(&sigvp->sv_pending, -1);
 
	return sqp;
}

/*
 * sigdelq(sigqueue_t **psqp, int sig)
 *
 * Remove and discard all items from passed queue that match sig.
 *
 * Assumes the queue is locked when called.
 */
void
sigdelq(sigqueue_t **psqp, int sig, sigvec_t *sigvp)
{
	sigqueue_t *sqp;
	int dequeued = 0;

	ASSERT(sig != 0);
	
	while ((sqp = *psqp) != NULL) {
		if (sqp->sq_info.si_signo < sig) {
			psqp = &sqp->sq_next;
		} else if (sqp->sq_info.si_signo == sig) {
			*psqp = sqp->sq_next;
			sigqueue_free(sqp);
			dequeued--;
		} else {
			/* signo is greater than sig */
			break;
		}
	}

	if (dequeued)
		atomicAddInt(&sigvp->sv_pending, dequeued);	
}

/*
 * cursiginfofree(ut)
 *
 * Toss ut_curinfo.
 *
 * Assumes ut_lock has been called before entry.
 */
void
cursiginfofree(uthread_t *ut)
{
	ASSERT(ut_islocked(ut));

	if (ut->ut_curinfo) {
		ASSERT(ut->ut_cursig);
		ASSERT(ut->ut_cursig == ut->ut_curinfo->sq_info.si_signo);

		sigqueue_free(ut->ut_curinfo);
		ut->ut_curinfo = (sigqueue_t *)NULL;
	}
}

#ifdef CELL
static void
asv_sigtopid(async_vec_t *asv)
{
	_async_sig_t	*sinfo = &asv->async_arg.async_signal;
	vproc_t *vpr;
	vp_get_attr_t	attr;

	vpr = VPROC_LOOKUP_STATE(sinfo->s_pid, ZYES);
	if (vpr == NULL)
		return;

	VPROC_GET_ATTR(vpr, VGATTR_SIGNATURE, &attr);
	if (sinfo->s_time < attr.va_signature) {
		VPROC_RELE(vpr);
		return;
	}
	(void) sigtopid(sinfo->s_pid, sinfo->s_sig, SIG_ISKERN|SIG_TIME, 0,
			 (cred_t *)NULL, (k_siginfo_t *)NULL);
	VPROC_RELE(vpr);
}
#else
static void
asv_sigtopid(async_vec_t *asv)
{
	_async_sig_t	*sinfo = &asv->async_arg.async_signal;

	(void) sigtopid(sinfo->s_pid, sinfo->s_sig, SIG_ISKERN|SIG_TIME, 0,
			 (cred_t *)&sinfo->s_time, (k_siginfo_t *)NULL);
}
#endif

static int
send_sigtopid(pid_t pid, int sig)
{
	async_vec_t asv;

	if (sig < 1 || sig >= NSIG)
		return EINVAL;

	if (!pid)
		return ESRCH;

	/*
	 * It would be nice to VPROC_HOLD vpr before putting it on the
	 * list, but that could sleep, and the whole purpose of this
	 * mechanism is to be able to deliver a signal from a caller
	 * that can't sleep.
	 */

	asv.async_arg.async_signal.s_pid = pid;
	asv.async_arg.async_signal.s_sig = sig;
	asv.async_arg.async_signal.s_time = time;
	asv.async_func = asv_sigtopid;

	return async_call(&asv);
}

#ifdef CELL
/*
 * Post a signal to a process, using the process pid as the handle.
 */
int
sigtopid(
	pid_t pid,
	int sig, int flags,
	pid_t sid,
	cred_t *cred,
	k_siginfo_t *info)
{
	vproc_t *vpr;
	/* REFERENCED */
	int error;

	/*
	 * If caller can't afford to sleep, skip the virtualized route
	 * for now.  Queue the signal request with the async-call
	 * daemon
	 * XXX	Fix to pass siginfo?
	 */
	if (flags & SIG_NOSLEEP)
		return send_sigtopid(pid, sig);

#ifdef DEBUG
	if (signaltrace == -1 || signaltrace == pid) {
		cmn_err(CE_CONT,
			"signal %d (0x%x) sent by [%s,0x%x] to pid %d\n",
			sig, sigmask(sig),
			get_current_name(),
			curuthread,
			pid);
	}
#endif
	if (pid == current_pid()) {
		VPROC_SENDSIG(curvprocp, sig, flags, sid, cred, info, error);
		ASSERT(error == 0);
		return 0;
	}
	vpr = VPROC_LOOKUP_STATE(pid, ZYES);
	if (vpr == NULL)
		return ESRCH;

	VPROC_SENDSIG(vpr, sig, flags, sid, cred, info, error);
	ASSERT(error == 0);

	VPROC_RELE(vpr);
	return 0;
}
#else
/*
 * Post a signal to a process, using the process pid as the handle.
 */
int
sigtopid(
	pid_t pid,
	int sig, int flags,
	pid_t sid,
	cred_t *cred,
	k_siginfo_t *info)
{
	vproc_t *vpr;
	/* REFERENCED */
	int error;

	/*
	 * If caller can't afford to sleep, skip the virtualized route
	 * for now.  Queue the signal request with the async-call
	 * daemon -- it'll get passed back here with SIG_TIME set.
	 * XXX	Fix to pass siginfo?
	 */
	if (flags & SIG_NOSLEEP)
		return send_sigtopid(pid, sig);

#ifdef DEBUG
	if (signaltrace == -1 || signaltrace == pid) {
		cmn_err(CE_CONT,
			"signal %d (0x%x) sent by [%s,0x%x] to pid %d\n",
			sig, sigmask(sig),
			get_current_name(),
			curuthread,
			pid);
	}
#endif
	if (pid == current_pid() && !(flags & SIG_TIME)) {
		VPROC_SENDSIG(curvprocp, sig, flags, sid, cred, info, error);
		ASSERT(error == 0);
		return 0;
	}

	vpr = VPROC_LOOKUP_STATE(pid, ZYES);
	if (vpr == NULL)
		return ESRCH;

	if (flags & SIG_TIME) {
		vp_get_attr_t	attr;
		time_t *tp;

		VPROC_GET_ATTR(vpr, VGATTR_SIGNATURE, &attr);
		tp = (time_t *)cred;
		if (*tp < attr.va_signature) {
			VPROC_RELE(vpr);
			return ESRCH;
		}
		ASSERT(flags == (SIG_ISKERN|SIG_TIME));
	}

	VPROC_SENDSIG(vpr, sig, flags, sid, cred, info, error);
	ASSERT(error == 0);

	VPROC_RELE(vpr);
	return 0;
}
#endif

/* uthread_apply function */
int
sigdelete(uthread_t *ut, void *signo)
{
	int sig = *(int *)signo;
	sigvec_t *sigvp = &UT_TO_PROC(ut)->p_sigvec;
	int s = ut_lock(ut);

	sigdelset(&ut->ut_sig, sig);
	sigdelq(&ut->ut_sigqueue, sig, sigvp);

	ut_unlock(ut, s);
	return 0;
}

/*
 * External interface for delivering a signal to a specified uthread.
 *
 * Calls sigtouthread_common, which is also called via sigtopid/VPROC_SENDSIG.
 *
 * If a siginfo struct is passed, it will be queued.
 */
void
sigtouthread(
	uthread_t *ut,
	int sig,
	k_siginfo_t *info)
{
	sigvec_t *sigvp;
	sigqueue_t *sqp;

	ASSERT(!sigismember(&jobcontrol, sig));

	if (sig < 1 || sig >= NSIG)
		return;

	sigvp = &UT_TO_PROC(ut)->p_sigvec;

	if (info &&
	    sigismember(&sigvp->sv_sainfo, sig) &&
	    (!sigismember(&sigvp->sv_sigign, sig) ||
	     tracing(UT_TO_PROC(ut), sig))) {
		sqp = sigqueue_alloc(KM_SLEEP);
		bcopy((caddr_t)info, (caddr_t)&sqp->sq_info,
				sizeof(k_siginfo_t));
	} else
		sqp = NULL;

	/*
	 * We call sigtouthread_common with the uthread locked
	 * for the convenience of VPROC_TRYSIG_THREAD -- it needs
	 * to check uthread state before trying to deliver a
	 * signal to a pthreaded process uthread.
	 */
	sigtouthread_common(ut, ut_lock(ut), sig, sigvp, sqp);
}

/*
 * Deliver signal 'sig' to the designated uthread.
 * Upon entry, signal vector structure is locked, as is uthread spinlock.
 */
void
sigtouthread_common(
	uthread_t *ut,
	int s,
	int sig,
	sigvec_t *sigvp,
	sigqueue_t *sqp)
{
	kthread_t *kt;
	__uint64_t sigbit;
	int already;

	ASSERT(ut_islocked(ut));
	sigbit = sigmask(sig);

	/*
	 * sigaddq discards sqp if there's already a matching siginfo queued.
	 */
	if (sqp && (already = sigaddq(&ut->ut_sigqueue, sqp, sigvp))) {
		ASSERT(already == EALREADY);
		already = already;	/* quiet compiler message */
		ASSERT(sigismember(&ut->ut_sig, sig));
		goto out;
	}

	kt = UT_TO_KT(ut);

	/*
	 * SIGCONT is special -- regardless of its state we must
	 * always restart the uthread if it is stopped at a jobcontrol
	 * stop.  If a parent attempts to do a wait after receiving
	 * a SIGCLD, and the child is SIGCONT'd before the parent gets
	 * the wait return, the parent will not wake up. POSIX doesn't
	 * seem to specify what should happen here. SVR4 solves this with
	 * a masterfully complex signal info queueing mechanism.
	 */
	if (sig == SIGCONT) {

		if (!sigisempty(&ut->ut_sig)) {
			/* delete siginfos for the stop signals */
			if (sigismember(&ut->ut_sig, SIGSTOP))
				sigdelq(&ut->ut_sigqueue, SIGSTOP, sigvp);
			if (sigismember(&ut->ut_sig, SIGTSTP))
				sigdelq(&ut->ut_sigqueue, SIGTSTP, sigvp);
			if (sigismember(&ut->ut_sig, SIGTTOU))
				sigdelq(&ut->ut_sigqueue, SIGTTOU, sigvp);
			if (sigismember(&ut->ut_sig, SIGTTIN))
				sigdelq(&ut->ut_sigqueue, SIGTTIN, sigvp);
			
			/* cancel pending stop signals */
			sigdiffset(&ut->ut_sig, &stopdefault);
		}

		if (ut->ut_flags & UT_STOP && ut->ut_whystop == JOBCONTROL) {
			/* a UT_STOPped thread shouldn't be sleeping anymore */
			ASSERT(kt->k_wchan == 0);

			unstop(ut);
		}
	} else if (sigbitismember(&stopdefault, sigbit, sig)) {
		ASSERT(ut_islocked(ut));
		/* always clear SIGCONT */
		sigdelq(&ut->ut_sigqueue, SIGCONT, sigvp);
		sigdelset(&ut->ut_sig, SIGCONT);
	}

	ASSERT(ut_islocked(ut));
	/*
	 * Ignored signals never get sent, and
	 * SIGCONT, SIGPWR, SIGWINCH, SIGCLD, SIGURG, and SIGIO
	 * behave such that SIG_DFL is the same as SIG_IGN.
	 */
	if (sigbitismember(&sigvp->sv_sigign, sigbit, sig)
			||
	    (sigbitismember(&ignoredefault, sigbit, sig)
	     && !sigbitismember(&sigvp->sv_sigcatch, sigbit, sig)
	     && !tracing(UT_TO_PROC(ut), sig)))
		goto out;

	sigbitaddset(&ut->ut_sig, sigbit, sig);
	ASSERT(ut_islocked(ut));

	/*
	 * Deal with job control stop signals - default case only.
	 * If we're tracing these signals we really have no choice but to
	 * wake the process threads since otherwise the parent will think
	 * the process has stopped on a job control signal but
	 * in fact it's stopped for the debugger.
	 */
	if (sigbitismember(&stopdefault, sigbit, sig)
	    && !sigbitismember(ut->ut_sighold, sigbit, sig)
	    && !sigbitismember(&sigvp->sv_sigcatch, sigbit, sig)
	    && !tracing(UT_TO_PROC(ut), sig)) {

		/*
                 * if handle SIGCONT then posix requires that the process
                 * will be woken up from the sleep with EINTR when SIGCONT
                 * is posted which is the only way this process can be 
		 * resumed now.
                 */
                if (sigismember(&sigvp->sv_sigcatch, SIGCONT))
			thread_interrupt(kt, &s);
		goto out;
	}

	ASSERT(ut_islocked(ut));
	/*
	 * If the signal is held there is nothing more
	 * to do at this point unless the thread is waiting in sigpoll
	 * for the signal, then  we need to wake up that process so that
	 * it can check if this is the signal that it is waiting for.
	 * Note that we must check cantmask because multithreaded processes
	 * can change their sighold masks from user space.
	 */
	if (sigbitismember(ut->ut_sighold, sigbit, sig) &&
	    !sigismember(&cantmask, sig) &&
	    !sigbitismember(&ut->ut_sigwait, sigbit, sig))
		goto out;

	/* Special case for SIGPTRESCHED - don't terminate syscalls
	 * if this signal is generated.
	 */
	if (sig == SIGPTRESCHED)
		goto out;

	ASSERT(ut_islocked(ut));
	ASSERT(!(kt->k_flags & KT_SLEEP && ut->ut_flags & UT_STOP));

	ASSERT(ut_islocked(ut));
	/*
	 * If the process is sleeping at an interruptible priority, then set
	 * it running.
	 */
	if (thread_interrupt(kt, &s))
		goto out;

	/*
	 * Or if it's stopped and this is a fatal signal, walk it to
	 * the gallows.
	 */
	if (ut->ut_flags & UT_STOP) {
		if (sig == SIGKILL) {
			unstop(ut);
			goto out;
		}
		ASSERT(kt->k_wchan == 0 || (kt->k_flags & KT_NWAKE));
	} else

	if (!(kt->k_flags & KT_SLEEP) &&
	    (KT_ISBASERT(kt) && !KT_ISNBASEPRMPT(kt)) ||
	    sig == SIGALRM || sig == SIGVTALRM || sig == SIGPROF) {
		evpostRunq(kt);
	} else {
		/*
		 * process is either sleeping at non-breakable priority or
		 * running - if it's going to die make sure it's not
		 * sitting on a block request
		 */
		if (isfatalsig(ut, sigvp)) {
			if (kt->k_wchan == (caddr_t)&ut->ut_pblock.s_wait)
				setrun(kt);
		}
	}
	ASSERT(ut_islocked(ut));
out:
	ut_unlock(ut, s);
	return;
}

/*
 * issig - returns current signal or 0 if no signal pending
 *
 * A found signal is removed from the ut_sig list and made current
 * (set into ut_cursig).
 *
 * If the current signal is one we'd prefer to ignore, move it to ut_sig
 * and look for a different one.
 *
 * If the caller passed the SIG_NOSTOPDEFAULT bit, then we do not return
 * any stop signals that are still set to have the default action.
 *
 * If caller passed the SIG_NOPTRESCHED bit,
 * then we do not return SIGPTRESCHED (regardless of action).
 *
 * This routine only looks at the current uthread's already-delivered
 * signals.  If there were any signals sent to the process that
 * weren't delivered to a specific uthread, they'd be in the sigvec's
 * bucket -- but the uthread would have looked there at syscall/trap
 * entry already to see if there were any of interest to it.
 */
int
issig(int isl, int deliver)
{
	uthread_t	*ut = curuthread;
	sigvec_t	*sigvp = &UT_TO_PROC(ut)->p_sigvec;
	int		n;
	int		s;

	ASSERT(sigvp != NULL);

	if (!isl)
		s = ut_lock(ut);

	/*
	 * if already have a current signal pending, deliver it
	 */
	if (n = ut->ut_cursig) {
		if ((deliver & SIG_NOSTOPDEFAULT)
		    && sigismember(&stopdefault, n)
		    && !sigismember(&sigvp->sv_sigcatch, n)
		    && !sigismember(&sigvp->sv_sigign, n)) {

			sigaddset(&ut->ut_sig, n);
			ut->ut_cursig = 0;
			
			/* requeue siginfo of this sig */
			if (ut->ut_curinfo != (sigqueue_t *)NULL) {
				sigaddq(&ut->ut_sigqueue,
					ut->ut_curinfo,
					sigvp);
				ut->ut_curinfo = NULL;
			}
		} else {
			/*
			 * debugger did this -- let's promote siginfo for
			 * this signal (if any)
			 */
			if (ut->ut_curinfo == NULL)
				ut->ut_curinfo = sigdeq(&ut->ut_sigpend,
							n, sigvp);
			goto out;
		}
	}

	/*
	 * For each signal, check how we wish to handle it.
	 * fsig() ignores held signals
	 */
	while (n = fsig(ut, sigvp, deliver)) {

		__uint64_t sigbit = sigmask(n);

		/*
		 * Ignored signals can get here if we're
		 * debugging via /proc or to squash races where the signal
		 * was being caught when a process posted the signal, but
		 * just at that time, the receiving process was in signal(2)
		 * or sigaction(2) setting the signal to SIG_IGN!
		 *
		 * Also check the 'special signals': those that have
		 * SIG_DFL == SIG_IGN
		 * If we want the signal, set it to be the current signal
		 */

		sigbitdelset(&ut->ut_sig, sigbit, n);

		/* XXX	Fix proc reference
		*/
		if (sigbitismember(&sigvp->sv_sigcatch, sigbit, n) ||
		    tracing(UT_TO_PROC(ut), n))
			break;
		
		if (!sigbitismember(&sigvp->sv_sigign, sigbit, n)
		    && !sigbitismember(&ignoredefault, sigbit, n)) {
			break;
		}
		
		/* if ignored then delete it from the queue now */
		sigdelq(&ut->ut_sigqueue, n, sigvp);
	}
	
	/* either found a valid signal or all are to be ignored */
	
	ut->ut_cursig = n;

	/*
	 * If we have a valid signal, attempt to copy siginfo from the queue
	 * to ut_curinfo.
	 */
	ASSERT(ut->ut_curinfo == NULL);
	if (n)
		ut->ut_curinfo = sigdeq(&ut->ut_sigpend, n, sigvp);
out:	
	if (n) {
		/* REFERENCED */
		int	rv;

		rv = thread_interrupt(UT_TO_KT(ut), &s);
		ASSERT(rv == 0);
	}
	
	if (!isl)
		ut_unlock(ut, s);

	return n;
}

/*
 * isfatalsig - return true if a posted signal would kill the process
 *
 * a signal is considered fatal if ALL the following conditions hold:
 *
 * 1) process not being traced
 * 2) the signal is NOT ignored, caught or held
 * 3) the signal isn't one of the signals that are ignored by
 *    default (SIGCLD, SIGPWR, etc) or are never fatal (job control).
 *
 * One could argue that there are no fatal signals if one
 * is PTRACED since the tracer could cancel the signal.
 * There are some anomalies however - in particular blockproc - if
 * we ignore fatal signals, then blocproc won't wake up - this isn't
 * what would have happened if the process wasn't being debugged.
 * However, the most common fatal signal is probably SIGINT - generated
 * because the user hit ^C trying to get back into dbx. We probably
 * shouldn't kick the process out of blockproc in that case.
 * So, we ignore fatal signals for PTRACE'd processes - the only remaining
 * bug being that on last /proc close the process will still potentially
 * have a fatal signal pending, and not go away.
 */
int
isfatalsig(uthread_t *ut, sigvec_t *sigvp)
{
	k_sigset_t s1, s2;
	proc_proxy_t *prxy = ut->ut_pproxy;

	/* XXX fix proc reference */

	if (!(prxy->prxy_flags & PRXY_EXIT) &&
	    (PTRACED(UT_TO_PROC(ut)) || sigisempty(&ut->ut_sig)))
		return(0);

	if (!sigvp) {
		ASSERT(ut == curuthread);
		VPROC_GET_SIGVEC(curvprocp, VSIGVEC_PEEK, sigvp);
	}

	s2 = sigvp->sv_sigcatch;
	sigorset(&s2, ut->ut_sighold);
	sigorset(&s2, &sigvp->sv_sigign);
	sigorset(&s2, &ignoredefault);
	sigorset(&s2, &stopdefault);
	s1 = ut->ut_sig;
	sigdiffset(&s1, &s2);
	
	if (sigisempty(&s1))
		return(0);
	
	return(1);
}

/*
 * Put the argument process into the stopped state and notify the
 * parent and other interested parties via wakeup and/or signal.
 * Note: for why == JOBCONTROL, sigvec is locked on entry.
 */
void
stop(
        uthread_t *ut, 
        ushort why, 
        ushort what,
        int stopall)
{
	struct proc *cp = UT_TO_PROC(ut);
	vproc_t *vpr;
	int s, s2;
	int tracewait;
	const struct timeval notime = {0, 0};

	ASSERT(ut == curuthread);

	/* Release the floating point unit if held. */
	checkfp(ut, 0);

	/*
         * Give up step right if we have it so that other uthreads can do
         * their own single-stepping.
	 */
	if (ut->ut_flags & UT_SRIGHT) {
		ut_flagclr(ut, UT_SRIGHT);
                prsright_release(ut->ut_pproxy);
	}

	/* First notify our parent that we are stopping - we
	 * do this before grabbing p_lock.
	 */
	if (!(cp->p_flag & SIGNORED) &&
	    ( (why == JOBCONTROL && !(cp->p_flag & SJSTOP)) ||
	      (cp->p_flag & STRC))) {

		/* If we get a reference on our parent, then our
		 * parent cannot exit until we VPROC_RELE.
		 * Thus, our ppid cannot change, and we don't
		 * need to hold any locks to check the SIGNORED flag.
		 *
		 * If we don't get a reference on our parent, then
		 * our parent has begun to exit. Thus, we will
		 * be inherited by init. So treat vpr == NULL
		 * equivelent to SIGNORED being set.
		 *
		 * If we find that we are inherited by init, then we
		 * don't bother to notify our parent. Inherited children
		 * do not signal init.
		 */
		vpr = VPROC_LOOKUP(cp->p_ppid);
		if (vpr != NULL) {
			if (!(cp->p_flag & SIGNORED)) {
				/* REFERENCED */
				int junk;

				VPROC_PARENT_NOTIFY(vpr, cp->p_pid,
						    why == JOBCONTROL ?
						    CLD_STOPPED : CLD_TRAPPED,
						    what, notime, notime,
						    cp->p_pgid,
						    VPGRP_SIGSEQ(cp->p_vpgrp),
						    0, junk);
			}
			/* Release our parent now - even though in the
			 * STRC case we will have to re-lookup after we
			 * are continued.
			 */
			VPROC_RELE(vpr);
		}
	}

	/* if going to die, don't stop */
	ASSERT(!ut_islocked(ut));
	s = ut_lock(ut);
	ASSERT(ut_islocked(ut));
	if (ut->ut_cursig == SIGKILL || sigismember(&ut->ut_sig, SIGKILL)) {
		ut_unlock(ut, s);
		return;
	}

	if (why == JOBCONTROL) {
		/*
		 * a job control stop
		 */
		ASSERT(!(UT_TO_KT(ut)->k_flags & KT_SLEEP));

		if (tracewait = ut->ut_flags & UT_TRWAIT)
			ut->ut_flags &= ~UT_TRWAIT;
		ut->ut_flags |= UT_STOP;
		ut->ut_whystop = why;
		ut->ut_whatstop = what;

		if (!(cp->p_flag & SJSTOP)) {
			p_flagset(cp, SJSTOP);
		}

		/*
		 * In this case, we entered with sigvec locked as a
		 * a protection against the pending stopped state, and
		 * SJSTOP, being cleared by delivery of a SIGCONT.
		 * Although this is well-defined, it's ugly.
		 */

		ut_endrun(ut);
		s2 = splprof();
		ktimer_switch(UT_TO_KT(ut), AS_JCL_WAIT);
		splx(s2);
		ut_nested_unlock(UT_TO_KT(ut));
		VPROC_PUT_SIGVEC(curvprocp);
		
		if (tracewait)
			prasyncwake();


		/* give up processor */
		swtch(RESCHED_S);
		enableintr(s);

	} else if (cp->p_flag & SPROCTR || ut->ut_flags & UT_PRSTOPX) {
		/*
		 * we may not even be SPROPEN! but have inherited
		 * tracing and stop anyway
		 */
		/*
		 * if we're skipping over any watchpoints, cancel them now
		 * in case the world changes while we're stopped
		 */
		if (ut->ut_watch) {
			ut_unlock(ut, s);
			cancelskipwatch();
			s = ut_lock(ut);
			ASSERT((ut->ut_watch->pw_flag & WINTSYS) == 0);
			ASSERT((ut->ut_flags & UT_WSYS) == 0);
		}

		/*
		 * If there is a pending requested stop, do that now
		 */
		if (why != REQUESTED) {
			while (ut->ut_flags & UT_PRSTOPBITS) {
				dostop(ut, REQUESTED, 	/* note that dostop */
				       0, 0, s);        /* drops ut_lock */
				s = ut_lock(ut);
			}
		}

		/* Do real stop.
		 * Recheck condition, we may have stopped above.
		 */
		/*
		 * Recheck the PRSTOPBITS for REQUESTED stops, since
		 * last time we checked without ut_lock. Bug 548177.
		 * Do the check after any cancelskipwatch which drops
		 * ut_lock, letting debugger clear prstop bits. Bug
		 * 545339.
		 */

		if ((cp->p_flag & SPROCTR) && ((why != REQUESTED) || 
					(ut->ut_flags & UT_PRSTOPBITS))) {
			dostop(ut, why, what,   /* note that dostop */
                               stopall, s);	/* drops ut_lock */
			s = ut_lock(ut);
		}

		/*
		 * after restarting, also allow requested stops
		 * proc(4) says that a requested stop after a SIGNALLED or
		 * FAULTED will immediately stop w/ REQUESTED
		 */
		while (ut->ut_flags & UT_PRSTOPX) {
			dostop(ut, REQUESTED, 0, 0, s);
			s = ut_lock(ut);
		}
		ut_unlock(ut, s);

	} else if (cp->p_flag & STRC) {
		/*
		 * Race with our parent exiting - if it exits
		 * then it (in pproc_reparent) will send us a SIGKILL -
		 * which we check for up above, and we still hold our
		 * p_lock, so we can go into the STOP state without
		 * a race.
		 */
		ASSERT(!(UT_TO_KT(ut)->k_flags & KT_SLEEP));

		ASSERT(ut_islocked(ut));
		ut->ut_flags |= UT_STOP;
		ut->ut_whystop = why;
		ut->ut_whatstop = what;

		/* XXX This process had better be single-uthreaded! */

		ut_endrun(ut);
		s2 = splprof();
		ktimer_switch(UT_TO_KT(ut), AS_JCL_WAIT);
		splx(s2);
		ut_nested_unlock(UT_TO_KT(ut));
	
		/* give up processor */
		swtch(RESCHED_S);
		enableintr(s);

		/*
		 * But could we have gotten a stop signal in the interim?
		 */
		/* Notify parent that we have been continued */
		if (!(cp->p_flag & SIGNORED)) {
			vpr = VPROC_LOOKUP(cp->p_ppid);
			if (vpr != NULL) {
				if (!(cp->p_flag & SIGNORED)) {
					/* REFERENCED */
					int junk;

					VPROC_PARENT_NOTIFY(vpr, cp->p_pid,
						CLD_CONTINUED, what, notime,
						notime, cp->p_pgid,
						VPGRP_SIGSEQ(cp->p_vpgrp),
						0, junk);
				}
				VPROC_RELE(vpr);
			}
		}

      	} else if (why == CHECKPOINT) {
		ASSERT(ut->ut_flags & UT_CKPT);

		dostop(ut, why, what, 0, s);
	} else {
		/*
		 * if why=FAULTED and SPROCTR/SPRSTOP are cleared,
		 * don't stop
		 */
		ut_unlock(ut, s);
	}
}

/*
 * Performs a stop operation on the current thread.  For certain types of
 * stops, such as important events for debuggers, stopping one thread, will
 * depending on debugging flags, cause all of the other threads to stop
 * as well.
 */
#define		FALSE		0
#define		TRUE		1
static void
dostop(
        uthread_t *ut, 
        ushort why, 
        ushort what, 
        int stopall,
        int s)
{
	proc_t *cp = UT_TO_PROC(ut);
	proc_proxy_t *px = ut->ut_pproxy;
	int tracewait;		/* is someone waiting for thread stop */
	int ss;
	int dopollwake = FALSE;	/* should we wake up poll waiters */
	int docount = 0;
	int pwait = 0;

	/*
	 * If this is the stopall case, no requested stops are pending.
	 * But, it is possible that prstopj is set on us.
	 */
	ASSERT(!stopall || ((ut->ut_flags & UT_PRSTOPBITS) == 0));

	/*
	 * In the non-multithread case, take the easy path. In the mt
	 * case, take the easy path if we do not have to start a jstop
	 * and we do not have to count ourself. The hard path consists
	 * of seeing whether we really should start a jstop or actually
	 * count ourself.
	 */
        if (px->prxy_utshare == NULL)
	        stopall = FALSE;
	else {
		int isarmed = (px->prxy_flags & PRXY_JSARMED);

		/*
		 * Process can turn jsarmed just after we check, that's
		 * fine, we will be counted as stopped in that case by
		 * prstop_proc or asv_stopall who set jsarmed.
		 */
		docount = (isarmed && (ut->ut_flags & UT_HOLDJS));
		if (((px->prxy_flags & PRXY_JSTOP) == 0) || isarmed)
			stopall = FALSE;
		/* All 4 combinations of <stopall, docount> possible */
	}

	/*
         * Deal with the case in which we have to deal with multiple
         * threads.
	 */
	if (stopall || docount) {
		int stopself = FALSE;

		ss = prxy_stoplock(px);

		/*
                 * If a joint stop is going on, count myself if needed.
                 */
		if (px->prxy_flags & PRXY_JSARMED) {
			if (ut->ut_flags & UT_HOLDJS) {
				stopself = TRUE;
				ASSERT(docount);
			}
			stopall = FALSE;
		}
	
		/*
		 * In stopall case, prxy_jscount must be zero because
		 * a joint stop cannot be in progress.
		 */
		else if (stopall) {
			ASSERT(px->prxy_jscount == 0);
			ASSERT(docount == 0);
			/*
			 * Optimization for single thread case.
			 */
			if (px->prxy_nthreads == 1) {
				stopself = TRUE;
				stopall = FALSE;
				px->prxy_jscount = 1;
			} 
			/*
			 * Poke all threads to stop later on.
			 */
			else {
				/*
			 	 * The uscan_hold makes sure the proc/proxy 
			 	 * is around for asyncd.
			 	 */
				nuscan_hold(px);
			}
			prxy_nflagset(px, PRXY_JSARMED);
		} else {
			ASSERT(0);
		}

	        tracewait = stopsub(ut, why, what);
		ut_endrun(ut);
		ut_nested_unlock(ut);

		if (stopself)
			dopollwake = jsacct(px, 1, &pwait);
		/*
		 * Release ut lock after any possible changes to prxy
		 * fields so that prwstop_proc can not return before
		 * the updates to the prxy.
		 */
		prxy_stopunlock(px, ss);
		if (stopall) {
			/*
                 	 * If no other thread has started a collective stop and
                 	 * this is the type of operation that should begin one,
                 	 * then we arrange for that to happen asynchronously.
		 	 */
			async_vec_t asv;

			asv.async_arg.async_null = (long)cp;
			asv.async_func = asv_stopall;
			(void)async_call(&asv);
		}
	}

	/*
         * Here is the simple case in which no other threads have to be
         * considered.
	 */
	else {
	        tracewait = stopsub(ut, why, what);
		ut_endrun(ut);
		ut_nested_unlock(ut);
		dopollwake = jsacct(px, 0, &pwait);
	}

	/*
	 * Now wakeup anyone who might be polling.  If the the joint poll
         * option is specified, we should try not to wake up pollers until
	 * all threads have stopped.
	 */
	if (cp->p_trace && ((dopollwake == TRUE) || 
				((px->prxy_flags & PRXY_JPOLL) == 0)))
		prpollwakeup(cp->p_trace,
				POLLIN|POLLPRI|POLLOUT|POLLRDNORM|POLLRDBAND);

	/*
	 * Wake up those in procfs waiting for the thread to the process
         * to stop. We try not to do too many wakeups by seeing if some
	 * one is really waiting (PWAIT), and whether asyncd will be 
	 * doing the wakeup later.
	 */
	if (tracewait || pwait)
		prasyncwake();	/* XXX should depend on value of why */

	/* give up processor */
	swtch(RESCHED_S);
	enableintr(s);
}

int
stopsub(
        uthread_t *ut, 
        ushort why, 
        ushort what)
{
        int tracewait, s;

	ut->ut_whystop = why;
	ut->ut_whatstop = what;
	ASSERT(!(UT_TO_KT(ut)->k_flags & KT_SLEEP));

	ASSERT(issplhi(getsr()));
	tracewait = ut->ut_flags & UT_TRWAIT;
	ASSERT(!(ut->ut_flags & UT_STOP));
	ut->ut_flags &= ~(UT_PRSTOPBITS|UT_TRWAIT|UT_HOLDJS);
	ut->ut_flags |= (UT_STOP | UT_EVENTPR);
	s = splprof();
	ktimer_switch(UT_TO_KT(ut), AS_JCL_WAIT);
	splx(s);
	return (tracewait);
}


static void
asv_stopall(async_vec_t *asv)
{
	proc_t *cp = (proc_t *)(asv->async_arg.async_null);
	proc_proxy_t *px = &(cp->p_proxy);

	prstop_proc(cp, 0);
	uscan_rele(px);
}

/*
 * Perform the action specified by the current signal.
 *
 * issig simply returns whether a signal is pending, and psig
 * processes the signal.
 *
 * Returns - 0 if no signals processed/all cancelled or default == ignore
 *	     1 if signal really there/was processed
 */
int
psig(
	int		*backup_pc,
	struct sysent	*callp)
{
	uthread_t *ut = curuthread;
	proc_t *p = UT_TO_PROC(ut);	/* XXX remove */
	sigvec_t *sigvp;
	int n, s;
	__uint64_t sigbit;

#ifdef DEBUG
	if (*backup_pc)
		ASSERT(callp != NULL);
#endif
	/*
	 * This code doesn't modify the sigvec data, so access is sufficient.
	 */
	VPROC_GET_SIGVEC(curvprocp, VSIGVEC_ACCESS, sigvp);

#ifdef CELL
	if (UT_TO_KT(ut)->k_flags&KT_BHVINTR) {
		bla_prevent_starvation(*backup_pc); 	/* ZZZ - backup_pc for debug only */
	}
#endif

	s = ut_lock(ut);
	UT_TO_KT(ut)->k_flags &= ~KT_BHVINTR;

	/*
	 * XXX races with /proc setting cursig (since we may be UT_PRSTOP
	 * but still running)
	 * At the least it means that we shouldn't reference ut_cursig
	 * without ut_lock...
	 */
again:
	if (ut->ut_cursig == 0)
		/* sets ut_cursig */
		issig(1, SIG_ALLSIGNALS);

	if (ut->ut_cursig == 0) {
		thread_interrupt_clear(UT_TO_KT(ut), 1);
		ut_unlock(ut, s);
		VPROC_PUT_SIGVEC(curvprocp);

		/* This code arranges for system calls to be restarted
		 * if interrupted by either a job control stop signal
		 * of by a signal that procfs was tracing, and has been
		 * canceled.
		 */
		if (*backup_pc) {
			ut->ut_exception->u_eframe.ef_epc -= 8;

			/* Only do this once, since psig is called
			 * in a loop.
			 */
			*backup_pc = 0;
		}
		return 0;
	}

	n = ut->ut_cursig;

	if ((p->p_flag & SPROCTR) && (sigismember(&p->p_sigtrace, n))) {
		ut_unlock(ut, s);

		VPROC_PUT_SIGVEC(curvprocp);
		stop(ut, SIGNALLED, n, 1);

		VPROC_GET_SIGVEC(curvprocp, VSIGVEC_ACCESS, sigvp);
		s = ut_lock(ut);

		/*
		 * If /proc wants to cancel the signal
		 * it will have cleared ut_cursig.
		 */
		if ((n = ut->ut_cursig) == 0)
			goto again;

	} else if (p->p_flag & STRC) {
		int done;

		ut_unlock(ut, s);
		VPROC_PUT_SIGVEC(curvprocp);

		/*
		 * If traced, always stop, and stay
		 * stopped until released by parent or tracer.
		 */
		do {
			stop(ut, SIGNALLED, n, 0);
			done = procxmt();
			if (n == SIGKILL || sigismember(&ut->ut_sig, SIGKILL))
				break;
		} while (!done && (p->p_flag & STRC));

		VPROC_GET_SIGVEC(curvprocp, VSIGVEC_ACCESS, sigvp);
		s = ut_lock(ut);

		/* was signal cancelled? */
		if ((n = ut->ut_cursig) == 0)
			goto again;
	}

	/*
	 * sigtoproc() lets this through if this proc/sig was being traced
	 * at the time.  Things may have changed since then.
	 * Clear signal if there is no handler and it is being ignored.
	 */
	
	sigbit = sigmask(n);

	if (!sigbitismember(&sigvp->sv_sigcatch, sigbit, n)
	    && (sigbitismember(&ignoredefault, sigbit, n)
		|| sigbitismember(&sigvp->sv_sigign, sigbit, n))) {
		cursiginfofree(ut);
		ut->ut_cursig = 0;		/* check again with issig */
		goto again;
	}

	/* see if ut_cursig should stop process */
	if (isjobstop(sigvp, s)) {
		/*
		 * isjobstop side-effects: clears ut_cursig and unlocks
		 * sigvec lock and ut_lock.
		 */
		VPROC_GET_SIGVEC(curvprocp, VSIGVEC_ACCESS, sigvp);
		s = ut_lock(ut);
		goto again;
	}

	/*
	 * OK.  We found a signal of interest, so decide what the user
	 * intends us to do with it.
	 */
	ASSERT(!sigbitismember(&sigvp->sv_sigign, sigbit, n));
	ASSERT(sigbitismember(&sigvp->sv_sigcatch, sigbit, n)
	       || !sigbitismember(&ignoredefault, sigbit, n));
	ASSERT(sigbitismember(&sigvp->sv_sigcatch, sigbit, n)
	       || !sigbitismember(&stopdefault, sigbit, n));

	thread_interrupt_clear(UT_TO_KT(ut), 1);

	if (sigbitismember(&sigvp->sv_sigcatch, sigbit, n)) {
		void (*handler)();
		k_siginfo_t *sip = NULL;
		int dosiginfo = 0;

		handler = sigvp->sv_hndlr[n-1];
		
		/*
		 * save siginfo pointer here, in case the
		 * the signal's reset bit is on
		 */
		if (sigbitismember(&sigvp->sv_sainfo, sigbit, n)) {
			dosiginfo = 1;
			if (ut->ut_curinfo)
				sip = &ut->ut_curinfo->sq_info;
		}
		
		/*
		 * A process thread that called sigsuspend() would have had
		 * the old mask in ut_suspmask, otherwise just use current
		 * ut_sighold.  Note that sendsig uses ut_suspmask when
		 * delivering the sig handler, and will restore ut_sighold
		 * from ut_suspmask after signal delivery.
		 */
		if (ut->ut_flags & UT_SIGSUSPEND)
			ut->ut_flags &= ~UT_SIGSUSPEND;
		else
			ut->ut_suspmask = *ut->ut_sighold;
		
		sigorset(ut->ut_sighold, &sigvp->sv_sigmasks[n-1]);
		
		if (!sigbitismember(&sigvp->sv_signodefer, sigbit, n))
			sigbitaddset(ut->ut_sighold, sigbit, n);

		ut_unlock(ut, s);

		/* Handle system call restarts */
		if (*backup_pc) {
			if (sigismember(&sigvp->sv_sigrestart, n) &&
			    !(callp->sy_flags & SY_NORESTART))
				ut->ut_exception->u_eframe.ef_epc -= 8;
			else
				ut->ut_exception->u_eframe.ef_a3 = 1;

			/* Only do this once - since psig is called
			 * in a loop.
			 */
			*backup_pc = 0;
		}

		if (sigbitismember(&sigvp->sv_sigresethand, sigbit, n)) {
			int flags;
			
			flags = sigvp->sv_flags & SNOCLDSTOP ? SA_NOCLDSTOP : 0;
			
			if (sigvp->sv_flags & SNOWAIT)
				flags |= SA_NOCLDWAIT;
			
			VPROC_PUT_SIGVEC(curvprocp);
			CURVPROC_SET_SIGACT(n, SIG_DFL, &emptyset, flags, 0);
		} else
			VPROC_PUT_SIGVEC(curvprocp);
		
		/*
		 * sendsig requires no locks.
		 */
		atomicAddLong(&ut->ut_pproxy->prxy_ru.ru_nsignals, 1);
		ASSERT((__psint_t)handler > 1);

		LOG_TSTAMP_EVENT(RTMON_SIGNAL,
			TSTAMP_EV_SIGRECV, UT_TO_KT(ut)->k_id,
			 (__int64_t)(__psint_t) handler, n, p->p_pid);

		if (sendsig(handler, n, sip, dosiginfo) == 0) {
			/* free the last sent siginfo */
			s = ut_lock(ut);
			cursiginfofree(ut);
			ut->ut_cursig = 0;
			ut_unlock(ut, s);
			return(1);
		}

		/* sendsig failed! - book 'em, Dano */
		ut->ut_cursig = 0;	/* fast, loose */
		n = SIGSEGV;
	} else {
		cursiginfofree(ut);
		ut->ut_cursig = 0;
		ut_unlock(ut, s);

		VPROC_PUT_SIGVEC(curvprocp);

		LOG_TSTAMP_EVENT(RTMON_SIGNAL,
			TSTAMP_EV_SIGRECV, UT_TO_KT(ut)->k_id,
			 (__int64_t)(__psint_t) SIG_DFL, n, NULL);
	}

	/*
	 * The default action for uncaught signals is to terminate the
	 * process, possibly with a core dump;
	 *
	 * Also, if this is a multi-threaded process and the primary
	 * thread has already dumped core, we must dump core as well.
	 */
	if (sigismember(&coredefault, n)
	    || ut->ut_pproxy->prxy_coredump_vp) {
		if (core(n))
			exit(CLD_DUMPED, n);
	}
	exit(CLD_KILLED, n);

	/* NOTREACHED */
}

/*
 * We increment the stack by an additional 64 bytes before returning to
 * handle signal so as to avoid the temporaries nanothreads uses below
 * the stack pointer.
 */
#define NANOSTK_NOCLOBBER 64

/*
 * sendsig - send a signal to a process thread
 * we must ALWAYS save ALL registers since a sigreturn is considered
 * a system call, and after a sigreturn a signal may be pending, which
 * would then cause only SOME registers to be re-saved...
 */
static int
sendsig(register void (*hdlr)(), int sig, k_siginfo_t *sip, int dosiginfo)
{
	int	oss_flags;
	int	stacksz = 0;
	int	abi;
	/* REFERENCED */
	int	error;
	uthread_t *ut = curuthread;
	proc_proxy_t	*prxy = ut->ut_pproxy;
	eframe_t *ep = &curexceptionp->u_eframe;
	__psunsigned_t sp;
	__psunsigned_t align_mask = 0;

	abi = get_current_abi();

	/* assert basic ABI size of ucontext_t - it is very
	 * important that this be <= 128 words (either 32 or 64 bit).
	 * setjmp, etc all rely on this.
	 * There are also alignment constraints - check all these here
	 */
	ASSERT(sizeof(ucontext_t) <= 128 * sizeof(machreg_t));
	ASSERT(sizeof(irix5_ucontext_t) == 128 * sizeof(__uint32_t));
	ASSERT(sizeof(irix5_n32_ucontext_t) <= 128 * sizeof(__uint64_t));
	ASSERT(sizeof(irix5_64_ucontext_t) <= 128 * sizeof(__uint64_t));

	if (dosiginfo) {
		switch (abi) {
		case ABI_IRIX5:
			stacksz = sizeof(irix5_siginfo_t)
				+ sizeof(irix5_ucontext_t)
				+ sizeof(irix5_64_gregset_t);
			align_mask = ~0x7;
			break;
		case ABI_IRIX5_N32:
			stacksz = sizeof(irix5_siginfo_t)
				+ sizeof(irix5_n32_ucontext_t);
			align_mask = ~0xf;
			break;
#if (_MIPS_SIM == _ABI64)
		case ABI_IRIX5_64:
			stacksz = sizeof(irix5_64_siginfo_t)
				+ sizeof(irix5_64_ucontext_t);
			/* 64 bit abi requires quad-word aligned stack ptr */
			align_mask = ~0xf;
			break;
#endif
		}
	} else {
		/* BSD sigcontext */
		switch (abi) {
		case ABI_IRIX5:
			stacksz = sizeof(struct irix5_sigcontext);
			align_mask = ~0x7;
			break;
		case ABI_IRIX5_N32:
#if (_MIPS_SIM == _ABI64)
		case ABI_IRIX5_64:
#endif
			/* n32 and 64 abi's require quad-aligned stack ptr */
			stacksz = sizeof(struct irix5_sigcontext);
			align_mask = ~0xf;
			break;
		}
	}

	/* when deciding if should deliver sig on alternate sig stack,
	 * make sure that the user has executed the sigaltstack or sigstack
	 * system calls - it is not sufficient to simply check:
	 *	sigismember(prxy->prxy_sigonstack, sig)
	 * since it is legal to call sigaction for a signal specifying the
	 * the SA_ONSTACK flag in the sigaction call, but not to have
	 * set up the alternate stack using the sigaltstack call. In that
	 * case, we have to deliver the signal on the current stack.
	 * We check for this case by checking that prxy_sigsp is non-zero.
	 */
	oss_flags = prxy->prxy_ssflags;
	if (!(oss_flags & (SS_ONSTACK|SS_DISABLE)) &&
	     prxy->prxy_sigsp != 0 &&
	     sigismember(&prxy->prxy_sigonstack, sig)) {
		/* prxy_sigsp points to top of alternate stack */
		sp = (__psunsigned_t)prxy->prxy_sigsp - stacksz;
		/* don't put in prxy_ssflags yet, wait till after savecontext */
		oss_flags |= SS_ONSTACK;
	} else {
		sp = ep->ef_sp - stacksz - NANOSTK_NOCLOBBER;
	}

	sp &= align_mask;

	/*
	 * verify that signal context space is accessable by the user.
	 * prxy_siglb is set to be 0 for sigstack case, and correctly
	 * set for sigaltstack case
	 */
	if ((prxy->prxy_ssflags & SS_ONSTACK) &&
	    sp < (__psunsigned_t)prxy->prxy_siglb) {
#ifdef DEBUG
		cmn_err(CE_CONT, "bad altstack for sendsig [%s:%d] "
				 "sp 0x%x pc 0x%x siglb 0x%x sigsp 0x%x\n",
				 get_current_name(), current_pid(), 
				 sp, ep->ef_epc,
				 prxy->prxy_siglb, prxy->prxy_sigsp);
#endif
		return(-1);
	}
	if (error = useracc((void *)sp, stacksz, B_READ, NULL)) {
#ifdef DEBUG
		cmn_err(CE_CONT, "bad stack for sendsig [%s:%d] "
				 "sp 0x%x pc 0x%x ra 0x%x error %d stksz 0x%x\n",
				 get_current_name(), current_pid(),
				 sp, ep->ef_epc,
				 ep->ef_ra, error, stacksz);
#endif
		return -1;
	}
	/* NOTE: aspacelock locked from useracc */

	if (dosiginfo) {
		/*
		 * Note that we can't really assert that the sainfo bit
		 * is on since by now, another thread could have changed
		 * the disposition
		ASSERT(sigismember(&curp->p_sigvec->sv_sainfo, sig));
		 */
		/*
	 	 * Svr4/XPG siginfo expects handler(sig, usip, ucp)
	 	 */

		/*
		 * Must translate between internal kernel siginfo
		 * and external one. This involves decoding the union
		 */
		switch (abi) {
		case ABI_IRIX5: {
			irix5_siginfo_t *usip;
			irix5_ucontext_t *ucp;

			ucp = (irix5_ucontext_t *)sp;
			irix5_savecontext(ucp, &ut->ut_suspmask);
			ep->ef_a2 = (k_machreg_t)ucp;

			if (sip) {
				usip = (irix5_siginfo_t *) (sp + sizeof(*ucp));
				irix5_siginfoktou(sip, usip);
				ep->ef_a1 = (k_machreg_t)usip;
			} else
				ep->ef_a1 = 0;

			ucp->irix5_extra_ucontext = sip ?
			  (app32_long_t) (sp + sizeof(*ucp) + sizeof(*usip)) :
			  (app32_long_t) (sp + sizeof(*ucp));

			irix5_extra_savecontext(ucp);

			/* push ucontext */
			prxy->prxy_oldcontext = (void *)ucp;
			break;
		}
		case ABI_IRIX5_N32: {
			irix5_siginfo_t *usip;
			irix5_n32_ucontext_t *ucp;

			ucp = (irix5_n32_ucontext_t *)sp;
			irix5_n32_savecontext(ucp, &ut->ut_suspmask);
			ep->ef_a2 = (k_machreg_t)ucp;
			if (sip) {
				usip = (irix5_siginfo_t *) (sp + sizeof(*ucp));
				irix5_siginfoktou(sip, usip);
				ep->ef_a1 = (k_machreg_t)usip;
			}
			else
				ep->ef_a1 = 0;

			/* push ucontext */
			prxy->prxy_oldcontext = (void *)ucp;
			break;
		}
#if (_MIPS_SIM == _ABI64)
		case ABI_IRIX5_64: {
			irix5_64_siginfo_t *usip;
			irix5_64_ucontext_t *ucp;

			ucp = (irix5_64_ucontext_t *)sp;
			irix5_64_savecontext(ucp, &ut->ut_suspmask);
			ep->ef_a2 = (k_machreg_t)ucp;
			if (sip) {
				usip = (irix5_64_siginfo_t *)
							(sp + sizeof(*ucp));
				irix5_64_siginfoktou(sip, usip);
				ep->ef_a1 = (k_machreg_t)usip;
			} else
				ep->ef_a1 = 0;

			/* push ucontext */
			prxy->prxy_oldcontext = (void *)ucp;
		}
#endif
		}

		/* indicates to sigtramp that we're passing back a ucontext */
		sig |= 0x80000000;

	} else {
		irix5_savesigcontext((struct irix5_sigcontext *)sp,
				     &ut->ut_suspmask);
		ep->ef_a2 = sp;
		/*
	 	 * Pass the 'code' value from the kernel thread into the
	 	 * signal handler if the signal carries such a
	 	 * value.
	 	 */
		if (sig == SIGFPE || sig == SIGTRAP || sig == SIGILL ||
					sig == SIGSEGV || sig == SIGBUS) {
			ep->ef_a1 = ut->ut_code;
			ut->ut_code = 0;
		} else
			ep->ef_a1 = 0;

		/* push sigcontext */
		prxy->prxy_oldcontext = (void *)sp;
	}

	/* record new sigstack state */
	prxy->prxy_ssflags = oss_flags;

	/*
	 * setup register to enter signal handler
	 * when resuming user process
	 */
	ep->ef_a0 = sig;
	ep->ef_sp = sp;
	ep->ef_a3 = (k_machreg_t)hdlr;
	ep->ef_epc = (k_smachreg_t)((__psint_t)prxy->prxy_sigtramp);
	ep->ef_t9 = (k_machreg_t)prxy->prxy_sigtramp;
	unuseracc((void *)sp, stacksz, B_READ);
	return(0);
}

/*
 * Find the unheld signal in bit-position representation in ut_sig.
 * No need to hold any process-general signal locks -- everything's
 * that needed is local to the uthread.
 */
int
fsig(uthread_t *ut, sigvec_t *sigvp, int deliver)
{
	k_sigset_t pend;

	ASSERT(ut_islocked(ut));

	/*
	 * Multithreaded processes can set ut_sighold from user space, so
	 * they can be trusted w.r.t. cantmask signals -- SIGKILL and SIGSTOP.
	 */
	if (sigismember(&ut->ut_sig, SIGKILL))
		return(SIGKILL); 
	if (sigismember(&ut->ut_sig, SIGSTOP))
		return(SIGSTOP); 

	pend = ut->ut_sig;

	/* held signals are not delivered */
	sigdiffset(&pend, ut->ut_sighold);

	if (!sigisempty(&pend)) {
		int sig;
		__uint64_t sigbit;

		/* don't deliver default jc stop sig */
		if (deliver & SIG_NOSTOPDEFAULT) {
			k_sigset_t stop = stopdefault;

			ASSERT(sigvp);

			sigdiffset(&stop, &sigvp->sv_sigign);
			sigdiffset(&stop, &sigvp->sv_sigcatch);
			sigdiffset(&pend, &stop);
		}

		for (sig = 1, sigbit = sigmask(1);
		     sig < NSIG;
		     sig++, sigbit <<= 1) {
			if (sigbitismember(&pend, sigbit, sig))
				return sig;
		}
	}

	return 0;
}

/*
 * Handle default actions for job control signals.
 * We get here if the signal was held at the time that
 * sigtoproc() was originally called or if it's time to stop.
 *
 * Note that the process has woken up (we are only called from psig)
 * and that if we ignore this signal then most likely the system call
 * that was interrupted (if any) will get EINTR - we need to get
 * ignoring working correctly....
 *
 * Returns 1 if signal was dealt with (ut_lock unlocked)
 *	   0 otherwise (ut_lock still locked)
 */
static int
isjobstop(sigvec_t *sigvp, int s)
{
	proc_t *p;	/* XXX fix */
	uthread_t *ut = curuthread;
	int n = ut->ut_cursig;
	__uint64_t sigbit = sigmask(n);
	
	if (sigbitismember(&stopdefault, sigbit, n)
	    && !sigbitismember(&sigvp->sv_sigcatch, sigbit, n)
	    && !sigbitismember(&sigvp->sv_sigign, sigbit, n)) {

		cursiginfofree(ut);
		
		ut->ut_cursig = 0;
		
		/* if a SIGCONT has been posted - delete it and don't stop */
		if (sigismember(&ut->ut_sig, SIGCONT)) {
			sigdelset(&ut->ut_sig, SIGCONT);
			ut_unlock(ut, s);
			VPROC_PUT_SIGVEC(curvprocp); /* release sigvec lock */
			return(1);
		}

		ut_unlock(ut, s);

		/*
		 * If process has become orphaned since the job control signal 
		 * was posted, discard the signal -- unless signal is SIGSTOP.
		 */
		p = UT_TO_PROC(ut);	/* XXX fix */

		if (n == SIGSTOP) {
			stop(ut, JOBCONTROL, n, 0); /*  releases sigvec lock */
		} else if (p->p_vpgrp != NULL) {
			pid_t	sid;
			int	is_orphaned;

			VPGRP_GETATTR(p->p_vpgrp, &sid, &is_orphaned, NULL);
			if (!is_orphaned)
				stop(ut, JOBCONTROL, n, 0);
			else
				VPROC_PUT_SIGVEC(curvprocp);
		}
		return(1);
	}

	return(0);
}

/*
 * used by waitsys()
 * Return 1 if stop signal pending, 0
 * otherwise (even if a SIGCONT canceled out SIGTSTP we still return 1
 * to restart the search in wait).
 */
int
wait_checkstop(void)
{
	return sigisready() && issig(0, SIG_ALLSIGNALS);
}

/*
 * checkstop - check if a stop is requested for the caller
 * This is used by routines that cannot afford to use the
 * the automatic stopping built into sleep/semawait due to
 * resources that would be held that would deadlock the system.
 * Once those resources are freed, the caller can then call checkstop()
 * to perform any required stop (job or debug). The return value
 * tells the caller whether the signal is real or has been cancelled
 * by a debugger or is done with (job control)
 * Return 1 if still pending signals present, otherwise 0
 */
int
checkstop(void)
{
	return sigisready() && issig(0, SIG_ALLSIGNALS);
}

/*
 * Create a core image on the file "core"
 */
struct coreheader32 {
	struct coreout	coreout;
	unsigned int	gpregs[NGP_REGS];
	unsigned int	fpregs[NFP_REGS];
	unsigned int	specregs[NSPEC_REGS];
	unsigned int	handlers[NSIG_HNDLRS];
	unsigned int	exdata[3];
	struct core_thread_data thrd_data;
};

struct coreheader64 {
	struct coreout	coreout;
	k_machreg_t	gpregs[NGP_REGS];
	k_machreg_t	fpregs[NFP_REGS];
	k_machreg_t	specregs[NSPEC_REGS];
	app64_ptr_t	handlers[NSIG_HNDLRS];
	unsigned int	exdata[3];
	struct core_thread_data thrd_data;
};

static int dumpvmap(vnode_t *, kvmap_t *, int, caddr_t, off_t, size_t *);
static int dumpvaddr(vnode_t *, enum uio_seg, caddr_t, size_t,
	 off_t, ssize_t *);
static void	putidesc32(void *, struct coreout *);
static void	putidesc64(void *, struct coreout *);

extern int corepluspid;	/* make core file with pid appended */

static int
core(int sigcause)		/* sigcause -- signal that caused dump */
{
	auto struct vnode *vp;
	uthread_t *ut = curuthread;
	proc_t *pp = UT_TO_PROC(curuthread);
	void *chp;
	struct coreout *cop;
	caddr_t vmapp, curvmap;
	int coreheadersize, vmapsize, vmapstructsize;
	off_t curoffset;
	int error, closerr;
	ssize_t resid;
	size_t delta=0;
	char corename[4+1+10+1];	/* "core" + "." + long pid + null */
	int cflags = 0;
	vasid_t vasid;
	as_getattr_t asattr;
	as_setrangeattr_t as_lock;
	kvmap_t *kvp;
	int i, nvmaps, primary_thread, res;
	proc_proxy_t *prxy = ut->ut_pproxy;
	int abi = ut->ut_pproxy->prxy_abi;

	/* the last thing we want to do is drop core on a guy
	 * who was blasted for using too much memory
	 */
	if (pp->p_flag & SBBLST)
		return(0);

	if (ut->ut_cred->cr_uid != ut->ut_cred->cr_ruid ||
	    ut->ut_cred->cr_gid != ut->ut_cred->cr_rgid)
		return(0);
	if (pp->p_rlimit[RLIMIT_CORE].rlim_cur == 0)
		return(0);

	/* lock the proxy coredump info */
	mutex_lock(&prxy->prxy_coredump_mutex, PZERO);

	strcpy(corename, "core");
	if (corepluspid || (pp->p_flag & SCOREPID)) {
		strcat(corename, ".");
		numtos(pp->p_pid, &corename[5]);
	}

	/* The first thread to come through this path is the primary
	 * coredump thread.  It is responsible for dumping the vmaps
	 * and dumps its register info in the normal spot in the
	 * corefile.  Thereafter all threads which pass through here
	 * are deemed secondary threads and only dump their register
	 * state and PRDA in a special extension area. We know if we
	 * are primary or secondary on passing through here based on
	 * whether the vp stored in the proxy is set or not.
	 */

	/* if the corefile is not already open, open it */
	if ((vp = prxy->prxy_coredump_vp) == 0) {
		if (pp->p_rlimit[RLIMIT_FSIZE].rlim_cur == 0)
			cflags |= VZFS;
		error = vn_open(corename, UIO_SYSSPACE, FWRITE | FTRUNC | FCREAT,
				(0666 & ~ut->ut_cmask) & PERMMASK,
				&vp, CRCORE, cflags, NULL);
		if (error) {
			mutex_unlock(&prxy->prxy_coredump_mutex);
			return 0;
		}
		VOP_ACCESS(vp, VWRITE, ut->ut_cred, error);
		if (error || vp->v_type != VREG) {
			printf("%s: core file access error\n", get_current_name());
			error = EACCES;
			VOP_CLOSE(vp, FWRITE, L_TRUE, ut->ut_cred, closerr);
			VN_RELE(vp);
			mutex_unlock(&prxy->prxy_coredump_mutex);
			return((error|closerr) == 0);
		}
		prxy->prxy_coredump_vp = vp;
		primary_thread = 1;
	} else {
		if (ut->ut_cursig == SIGKILL || sigismember(&ut->ut_sig,
				 SIGKILL)) {

			/* The process has been killed while the primary
			 * thread was dumping core.  Skip the dump.
			 */

			mutex_unlock(&prxy->prxy_coredump_mutex);
			return(0);
		}
		primary_thread = 0;
	}

	/* to give a clean core dump - get FP state */
	checkfp(ut, 0);
	
	/* common data for primary and secondary threads */
	if (ABI_HAS_64BIT_REGS(abi)) {
		coreheadersize = sizeof(struct coreheader64);
		chp = kmem_alloc(coreheadersize, KM_SLEEP);
		cop = &((struct coreheader64 *)chp)->coreout;
		if (ABI_IS_IRIX5_N32(abi))
			cop->c_magic = CORE_MAGICN32;
		else
			cop->c_magic = CORE_MAGIC64;
	} else {
		coreheadersize = sizeof(struct coreheader32);
		chp = kmem_alloc(coreheadersize, KM_SLEEP);
		cop = &((struct coreheader32 *)chp)->coreout;
		cop->c_magic = CORE_MAGIC;
	}
	
	cop->c_version = CORE_VERSION1;
	cop->c_sigcause = sigcause;
	bcopy(pp->p_comm, cop->c_name, CORE_NAMESIZE);
	bcopy(pp->p_psargs, cop->c_args, CORE_ARGSIZE);
	
	bzero(cop->c_idesc, sizeof(cop->c_idesc));
#ifdef ULI
	/* if a fault occurred in the ULI handler, we need to copy
	 * over the saved register values so they get dumped properly.
	 * Only do this if this is the primary faulting thread.
	 * XXX now that we can dump state for both threads, this
	 * should probably be done.
	 */
	if (primary_thread)
		uli_core();
#endif
	if (ABI_HAS_64BIT_REGS(abi))
		putidesc64(chp, cop);
	else
		putidesc32(chp, cop);
	
	if (primary_thread) {
		/* dump primary thread data */
		
		as_lookup_current(&vasid);

		/*
		 * Disable future locking to prevent vfault
		 * from locking down any pages we may fault-in
		 * during the core dump.
		 */
		as_lock.as_op = AS_UNLOCK_BY_ATTR;
		as_lock.as_lock_attr = AS_FUTURELOCK;
		error = VAS_SETRANGEATTR(vasid, NULL, NULL, &as_lock, NULL);
		if (error) {
			kmem_free(chp, coreheadersize);
			mutex_unlock(&prxy->prxy_coredump_mutex);
			return 0;
		}

		VAS_LOCK(vasid, AS_SHARED);
		if (error = VAS_GETATTR(vasid, AS_GET_KVMAP, NULL, &asattr)) {
			kmem_free(chp, coreheadersize);
			VAS_UNLOCK(vasid);
			mutex_unlock(&prxy->prxy_coredump_mutex);
			return 0;
		}

		cop->c_vmapoffset = coreheadersize;
		nvmaps = cop->c_nvmap = asattr.as_kvmap.as_nentries;
		kvp = asattr.as_kvmap.as_kvmap;

		/* alloc vmap struct */
#if (_MIPS_SIM == _ABI64)
		if (ABI_IS_IRIX5_64(abi)) {
			vmapsize = nvmaps * sizeof(struct vmap64);
			vmapstructsize = sizeof(struct vmap64);
		} else
#endif
		{
			vmapsize = nvmaps * sizeof(struct vmap);
			vmapstructsize = sizeof(struct vmap);
		}
		vmapp = kmem_zalloc(vmapsize, KM_SLEEP);
		curvmap = vmapp;

		curoffset = coreheadersize + vmapsize;

		/* first dump stack area(s) */
		for (i = 0; i < nvmaps; i++) {
			if (kvp[i].v_type == VSTACK) {
				res = dumpvmap(vp, &kvp[i], abi, curvmap,
					curoffset, &delta);
				if (res < 0) {
					kmem_free(chp, coreheadersize);
					kmem_free(vmapp, vmapsize);
					kern_free(kvp);
					VAS_UNLOCK(vasid);
					if (res == -SIGKILL)
						(void)vn_remove(corename, 
							UIO_SYSSPACE, RMFILE);
					mutex_unlock(
						&prxy->prxy_coredump_mutex);
					if (res == -SIGKILL)
						return(0);
					return(1);
				}
				curoffset += delta;
				curvmap += vmapstructsize;
			}
		}

		/*
		 * now do all the others - any previously dumped kvmaps will
		 * have been marked VDUMPED .. so we skip those
		 */
		for (i = 0; i < nvmaps; i++) {
			if (kvp[i].v_flags & VDUMPED)
				continue;
			ASSERT((caddr_t)curvmap < (vmapp + vmapsize));
			res = dumpvmap(vp, &kvp[i], abi, curvmap,
					      curoffset, &delta);
			if (res < 0) {
				kmem_free(chp, coreheadersize);
				kmem_free(vmapp, vmapsize);
				kern_free(kvp);
				VAS_UNLOCK(vasid);
				if (res == -SIGKILL)
					(void)vn_remove(corename, UIO_SYSSPACE,
						RMFILE);
				mutex_unlock(&prxy->prxy_coredump_mutex);
				if (res == -SIGKILL)
					return(0);
				return(1);
			}
			curoffset += delta;
			curvmap += vmapstructsize;
		}
		kern_free(kvp);
		kvp = NULL;

		/* write out vmaps */
		if (vn_rdwr(UIO_WRITE, vp, (caddr_t)vmapp, vmapsize,
			    cop->c_vmapoffset, UIO_SYSSPACE, 0,
			    pp->p_rlimit[RLIMIT_CORE].rlim_cur,
			    ut->ut_cred, &resid, &ut->ut_flid) != 0
		    || resid != 0) {
			cop->c_nvmap = 0;
		}

		vn_rdwr(UIO_WRITE, vp, (caddr_t)chp, coreheadersize,
			0, UIO_SYSSPACE, 0,
			pp->p_rlimit[RLIMIT_CORE].rlim_cur, ut->ut_cred,
			&resid, &ut->ut_flid);

		kmem_free(chp, coreheadersize);
		kmem_free(vmapp, vmapsize);

		/* store EOF offset so the secondary threads know
		 * where to start dumping their data
		 */
		prxy->prxy_coredump_fileoff = curoffset;
		VAS_UNLOCK(vasid);
	} else {
		/* if this is a threaded process and this is a
		 * secondary thread, we are only interested in the
		 * thread-specific data, i.e. the registers and
		 * PRDA. The primary thread will already have dumped
		 * the vmaps.
		 */
		caddr_t addr;
		off_t fileoff;
		size_t len;
		struct core_thread_data thrd_data;

		/* Update nthreads. If this is the first secondary
		 * thread to dump, update thrd_offset as well
		 */
		thrd_data.nthreads = ++ prxy->prxy_coredump_nthreads;
		if (thrd_data.nthreads == 1) {
			thrd_data.thrd_offset = prxy->prxy_coredump_fileoff;
			addr = (caddr_t)&thrd_data.thrd_offset;
			len = (size_t)&thrd_data.desc_offset
			      - (size_t)&thrd_data.thrd_offset;

			if (ABI_HAS_64BIT_REGS(abi)) {
				fileoff = (off_t)(&((struct coreheader64*)0)
						  -> thrd_data.thrd_offset);
			} else {
				fileoff = (off_t)(&((struct coreheader32*)0)
						  -> thrd_data.thrd_offset);
			}
		} else {
			addr = (caddr_t)&thrd_data.nthreads;
			len = (size_t)sizeof(thrd_data.nthreads);

			if (ABI_HAS_64BIT_REGS(abi)) {
				fileoff = (off_t)(&((struct coreheader64*)0)
						  -> thrd_data.nthreads);
			} else {
				fileoff = (off_t)(&((struct coreheader32*)0)
						  -> thrd_data.nthreads);
			}
		}
		if ((res = dumpvaddr(vp, UIO_SYSSPACE, addr, len, fileoff,
				 &resid)) < 0) {
			if (res == -SIGKILL)
				(void)vn_remove(corename, UIO_SYSSPACE, RMFILE);
			mutex_unlock(&prxy->prxy_coredump_mutex);
			if (res == -SIGKILL)
				return(0);
			return(1);
		}

		/* dump the registers */
		if (ABI_HAS_64BIT_REGS(abi)) {
			addr = (caddr_t)&((struct coreheader64*)chp)->gpregs;
			len = (size_t)&((struct coreheader64*)chp)->thrd_data -
				(size_t)&((struct coreheader64*)chp)->gpregs;
		} else {
			addr = (caddr_t)&((struct coreheader32*)chp)->gpregs;
			len = (size_t)&((struct coreheader32*)chp)->thrd_data -
				(size_t)&((struct coreheader32*)chp)->gpregs;
		}
		if ((res = dumpvaddr(vp, UIO_SYSSPACE, addr, len, 
				prxy->prxy_coredump_fileoff, &resid)) < 0) {
			if (res == -SIGKILL)
				(void)vn_remove(corename, UIO_SYSSPACE, RMFILE);
			mutex_unlock(&prxy->prxy_coredump_mutex);
			if (res == -SIGKILL)
				return(0);
			return(1);
		}
		prxy->prxy_coredump_fileoff += len;

		/* dump the PRDA */
		if ((res = dumpvaddr(vp, UIO_SYSSPACE, (caddr_t)ut->ut_prda, 
				sizeof(struct prda),
				prxy->prxy_coredump_fileoff, &resid)) < 0) {
			if (res == -SIGKILL)
				(void)vn_remove(corename, UIO_SYSSPACE, RMFILE);
			mutex_unlock(&prxy->prxy_coredump_mutex);
			if (res == -SIGKILL)
				return(0);
			return(1);
		}
		prxy->prxy_coredump_fileoff += sizeof(struct prda);
	}
	/* vp is released in uthread_exit() */
	mutex_unlock(&prxy->prxy_coredump_mutex);
	return 1;
}

static int
dumpvmap(vnode_t *vp, kvmap_t *kvp, int abi, caddr_t vmap, off_t offset,
	 size_t *delta)
{
	ssize_t resid;
	size_t dumpsize;
	int flag, res;
#if (_MIPS_SIM == _ABI64)
	struct vmap64 *v64 = NULL;
#endif
	struct vmap *v = NULL;

	/* copy kvmap to appropriate vmap */
#if (_MIPS_SIM == _ABI64)
	if (ABI_IS_IRIX5_64(abi)) {
		v64 = (struct vmap64 *)vmap;
		v64->v_type = kvp->v_type;
		v64->v_vaddr = (uint64_t)kvp->v_vaddr;
		v64->v_len = kvp->v_len;
	} else
#endif
	{
		v = (struct vmap *)vmap;
		v->v_type = kvp->v_type;
		v->v_vaddr = (unsigned)(__psunsigned_t)kvp->v_vaddr;
		v->v_len = kvp->v_len;
	}

	/* update kvp flags so we don't try to dump again */
	kvp->v_flags = VDUMPED;

	/*
	 * VPARTIAL dumps are dumps of the 1st 4096 bytes
	 * of a text region, to get the program headers (Elf_Phdr)
	 * for that region. This is to help debuggers verify
	 * that a .so that one is debugging against is actually
	 * the .so that was running at the time of the core dump.
	 */
	if (kvp->v_type != VPHYS && kvp->v_type != VGRAPHICS) {
		if (kvp->v_type == VTEXT) {
			dumpsize = 4096;
			flag = VPARTIAL;
		} else {
			dumpsize = kvp->v_len;
			flag = VDUMPED;
		}

		if ((res = dumpvaddr(vp, UIO_USERSPACE, (caddr_t)kvp->v_vaddr, 
				dumpsize, offset, &resid)) < 0) {
			return(res);
		}
		
		/*
		 * reduce length by what wasn't written.
		 * this makes v_len slightly misleading in that for vmaps
		 * that aren't dumped, it shows real region length, but
		 * for vmaps that are dumped it shows dumped length.
		 *
		 * For VPARTIAL region dumps, set vmapp->v_len
		 * to 4096 (dumpsize), then subtract resid.
		 */
#if (_MIPS_SIM == _ABI64)
		if (ABI_IS_IRIX5_64(abi)) {
			v64->v_len = dumpsize - resid;
			v64->v_offset = offset;
			v64->v_flags = flag;
		} else
#endif
		{
			v->v_len = dumpsize - resid;
			v->v_offset = offset;
			v->v_flags = flag;
		}
		*delta = (dumpsize - resid);
		return(0);
	}
	return(0);
}

int	dump_chunksize = 1024 * 1024;
#define DUMP_CHUNK	dump_chunksize

static int
dumpvaddr(vnode_t *vp, enum uio_seg seg, caddr_t addr, size_t len, 
	off_t fileoff, ssize_t *resid)
{
	uthread_t *ut = curuthread;
	size_t dumpsize_left, chunksize;
	caddr_t curaddr;
	off_t curoffset;

	dumpsize_left = len;
	curoffset = fileoff;
	curaddr = addr;

	while (dumpsize_left) {

		/* We loop here writing out DUMP_CHUNKs to allow
		 * a SIGKILL signal to come in if someone wants to
		 * to interrupt core file creation.  The core
		 * file gets thrown away in this case.
		 */

		if (dumpsize_left > DUMP_CHUNK) {
			chunksize = DUMP_CHUNK;
		}
		else {
			chunksize = dumpsize_left;
		}
		if (vn_rdwr(UIO_WRITE, vp, curaddr,
			    chunksize, curoffset, seg, 0,
			    curprocp->p_rlimit[RLIMIT_CORE].rlim_cur,
			    ut->ut_cred, resid, &ut->ut_flid) != 0)
			return(-1);
		if (ut->ut_cursig == SIGKILL || sigismember(&ut->ut_sig,
				 SIGKILL)) {
			return(-SIGKILL);
		}
		if (*resid) {

			/* write didn't fully complete - stop now */

			break;
		}
		dumpsize_left -= chunksize;
		curoffset += chunksize;
		curaddr += chunksize;
	}
	return(0);
}

static void
putidesc64(void *arg_chp, register struct coreout *cop)
{
	register struct idesc *idescp;
	register struct coreheader64 *chp = arg_chp;
	register k_machreg_t *regptr;
	exception_t	*up = curexceptionp;

	idescp = &cop->c_idesc[I_GPREGS];
	idescp->i_flags = IVALID;
	idescp->i_offset = (__psunsigned_t)chp->gpregs - (__psunsigned_t)chp;
	idescp->i_len = sizeof(chp->gpregs);
	chp->gpregs[0] = 0;		/* zero register */
	bcopy(&up->u_eframe.ef_at, &chp->gpregs[1],
		sizeof(chp->gpregs) - sizeof(chp->gpregs[0]));

	idescp = &cop->c_idesc[I_FPREGS];
	idescp->i_flags = IVALID;
	idescp->i_offset = (__psunsigned_t)chp->fpregs - (__psunsigned_t)chp;
	idescp->i_len = sizeof(chp->fpregs);
	bcopy(up->u_pcb.pcb_fpregs, chp->fpregs, sizeof(chp->fpregs));

	idescp = &cop->c_idesc[I_SPECREGS];
	idescp->i_flags = IVALID;
	idescp->i_offset = (__psunsigned_t)chp->specregs - (__psunsigned_t)chp;
	idescp->i_len = sizeof(chp->specregs);

	regptr = &chp->specregs[0];
	*regptr++ = up->u_eframe.ef_epc;
	*regptr++ = up->u_eframe.ef_cause;
	*regptr++ = up->u_eframe.ef_badvaddr;
	*regptr++ = up->u_eframe.ef_mdhi;
	*regptr++ = up->u_eframe.ef_mdlo;
	*regptr++ = up->u_pcb.pcb_fpc_csr;
	*regptr   = up->u_pcb.pcb_fpc_eir;

	idescp = &cop->c_idesc[I_SIGHANDLER];
	idescp->i_flags = IVALID;
	idescp->i_offset = (__psunsigned_t)chp->handlers - (__psunsigned_t)chp;
	idescp->i_len = sizeof(chp->handlers);

	/* For N32, this deliberately places the handlers right-justified
	 * and sign-filled in the 64bit field
	 */
	bcopy(curprocp->p_sigvec.sv_hndlr,
	      chp->handlers, sizeof(chp->handlers));

	idescp = &cop->c_idesc[I_EXDATA];
	idescp->i_flags = IVALID;
	idescp->i_offset = (__psunsigned_t)chp->exdata - (__psunsigned_t)chp;
	idescp->i_len = sizeof(chp->exdata);

	chp->exdata[0] = 0;
	chp->exdata[1] = 0;
	chp->exdata[2] = 0;

	idescp = &cop->c_idesc[I_THREADDATA];
	idescp->i_flags = IVALID;
	idescp->i_offset = (__psunsigned_t)&chp->thrd_data - (__psunsigned_t)chp;
	idescp->i_len = sizeof(chp->thrd_data);

	chp->thrd_data.thrd_offset = 0;
	chp->thrd_data.nthreads = 0;
	chp->thrd_data.desc_offset[I_GPREGS] = 0;
	chp->thrd_data.desc_offset[I_FPREGS]
		= (int)((size_t)&chp->fpregs - (size_t)&chp->gpregs);
	chp->thrd_data.desc_offset[I_SPECREGS]
		= (int)((size_t)&chp->specregs - (size_t)&chp->gpregs);
	chp->thrd_data.desc_offset[I_SIGHANDLER]
		= (int)((size_t)&chp->handlers - (size_t)&chp->gpregs);
	chp->thrd_data.desc_offset[I_EXDATA]
		= (int)((size_t)&chp->exdata - (size_t)&chp->gpregs);
	chp->thrd_data.prda_offset
		= (int)((size_t)&chp->thrd_data - (size_t)&chp->gpregs);
	chp->thrd_data.prda_len = sizeof(struct prda);
}

static void
putidesc32(void *arg_chp, register struct coreout *cop)
{
	register struct idesc *idescp;
	register struct coreheader32 *chp = arg_chp;
	register uint *regptr;
	register k_machreg_t *k_regptr;
	register int i;
	exception_t	*up = curexceptionp;

	idescp = &cop->c_idesc[I_GPREGS];
	idescp->i_flags = IVALID;
	idescp->i_offset = (__psunsigned_t)chp->gpregs - (__psunsigned_t)chp;
	idescp->i_len = sizeof(chp->gpregs);
	chp->gpregs[0] = 0;		/* zero register */

	i = (sizeof(chp->gpregs) - sizeof(chp->gpregs[0]))
				 / sizeof(chp->gpregs[0]);
	k_regptr = &up->u_eframe.ef_at;
	regptr = &chp->gpregs[1];
	for ( ; i ; i--)
		*regptr++ = *k_regptr++;

	idescp = &cop->c_idesc[I_FPREGS];
	idescp->i_flags = IVALID;
	idescp->i_offset = (__psunsigned_t)chp->fpregs - (__psunsigned_t)chp;
	idescp->i_len = sizeof(chp->fpregs);

	regptr = chp->fpregs;
	k_regptr = up->u_pcb.pcb_fpregs;
	for (i = 0; i < 32; i++)
		*regptr++ = *k_regptr++;

	idescp = &cop->c_idesc[I_SPECREGS];
	idescp->i_flags = IVALID;
	idescp->i_offset = (__psunsigned_t)chp->specregs - (__psunsigned_t)chp;
	idescp->i_len = sizeof(chp->specregs);

	regptr = &chp->specregs[0];
	*regptr++ = up->u_eframe.ef_epc;
	*regptr++ = up->u_eframe.ef_cause;
	*regptr++ = up->u_eframe.ef_badvaddr;
	*regptr++ = up->u_eframe.ef_mdhi;
	*regptr++ = up->u_eframe.ef_mdlo;
	*regptr++ = up->u_pcb.pcb_fpc_csr;
	*regptr   = up->u_pcb.pcb_fpc_eir;

	idescp = &cop->c_idesc[I_SIGHANDLER];
	idescp->i_flags = IVALID;
	idescp->i_offset = (__psunsigned_t)chp->handlers - (__psunsigned_t)chp;
	idescp->i_len = sizeof(chp->handlers);
#if (_MIPS_SIM == _ABI64)
	for (i = 0; i < NSIG_HNDLRS; i++)
		chp->handlers[i] =
		  (unsigned int)(__psunsigned_t)curprocp->p_sigvec.sv_hndlr[i];
#else
	bcopy(curprocp->p_sigvec.sv_hndlr,
	      chp->handlers, sizeof(chp->handlers));
#endif
	idescp = &cop->c_idesc[I_EXDATA];
	idescp->i_flags = IVALID;
	idescp->i_offset = (__psunsigned_t)chp->exdata - (__psunsigned_t)chp;
	idescp->i_len = sizeof(chp->exdata);

	chp->exdata[0] = 0;
	chp->exdata[1] = 0;
	chp->exdata[2] = 0;

	idescp = &cop->c_idesc[I_THREADDATA];
	idescp->i_flags = IVALID;
	idescp->i_offset = (__psunsigned_t)&chp->thrd_data - (__psunsigned_t)chp;
	idescp->i_len = sizeof(chp->thrd_data);

	chp->thrd_data.thrd_offset = 0;
	chp->thrd_data.nthreads = 0;
	chp->thrd_data.desc_offset[I_GPREGS] = 0;
	chp->thrd_data.desc_offset[I_FPREGS]
		= (int)((size_t)&chp->fpregs - (size_t)&chp->gpregs);
	chp->thrd_data.desc_offset[I_SPECREGS]
		= (int)((size_t)&chp->specregs - (size_t)&chp->gpregs);
	chp->thrd_data.desc_offset[I_SIGHANDLER]
		= (int)((size_t)&chp->handlers - (size_t)&chp->gpregs);
	chp->thrd_data.desc_offset[I_EXDATA]
		= (int)((size_t)&chp->exdata - (size_t)&chp->gpregs);
	chp->thrd_data.prda_offset
		= (int)((size_t)&chp->thrd_data - (size_t)&chp->gpregs);
	chp->thrd_data.prda_len = sizeof(struct prda);
}

/*
 * sys-trace system call. (my fave!)
 */
struct ptracea {
	sysarg_t	req;
	sysarg_t	pid;
	int		*addr;
	sysarg_t	data;
};

int
ptrace(struct ptracea *uap, rval_t *rvp)
{
	vproc_t *vpr;
	proc_t *p;
	uthread_t *ut;
	int s;
	int error = 0;

	if (uap->req <= 0) {
		p_flagset(curprocp, STRC);
		return 0;
	}

	/*
	 * see if any of our children match the process he asked to trace.
	 */
	vpr = ptrsrch(curprocp, (pid_t)uap->pid);
	if (vpr == NULL)
		error = ESRCH;
	else {
		VPROC_GET_PROC(vpr, &p);
		if (p->p_proxy.prxy_shmask & PR_THREADS)
			error = EPERM;	/* sorry -- don't know how */
		else
		if (_MAC_ACCESS(get_current_cred()->cr_mac,
							p->p_cred, MACWRITE)) {
			error = EACCES;
			VPROC_RELE(vpr);
		}
	}

	if (error) {
		_SAT_PTRACE(uap->req,uap->pid,NULL,error);
		return(error);
	}

	ut = prxy_to_thread(&p->p_proxy);

	s = ut_lock(ut);
	/* Found somebody.  Make sure they're stopped */
	ASSERT(p->p_pid == uap->pid);
	if (!(ut->ut_flags & UT_STOP)) {
		ut_unlock(ut, s);
		_SAT_PTRACE(uap->req,uap->pid,p->p_cred,ESRCH);

		VPROC_RELE(vpr);
		return(ESRCH);
	}

	/*
	 * We've found the child entry we wish to deal with at this point.
	 * At this point the uthread lock is still locked
	 * since the child is STOPped theres nothing it can do.
	 */
	ut_unlock(ut, s);

	/*
	 * Free our local process lock and acquire the communication lock.
	 * Set up the communication block.
	 */
	mutex_lock(&ipc.ip_lock, IPCPRI);
	ipc.ip_tptr = ut;
	ipc.ip_uid = p->p_cred->cr_uid;
	ipc.ip_data = uap->data;
	ipc.ip_addr = uap->addr;
	ipc.ip_req = uap->req;
	ipc.ip_pid = uap->pid;

	/*
	 * Wait for the child to respond.  We sleep on the wait semaphore
	 * in the communication block.  This means that we are always safe
	 * since only the subject will notice our command and kick us
	 * when done.
	 */
	while (ipc.ip_req > 0) {
		s = ut_lock(ut);
		if (ut->ut_flags & UT_STOP)
			unstop(ut);
		ut_unlock(ut, s);
		psema(&ipc.ip_wait, IPCPRI);
	}
	/*
	 * The child performed our request.  Clean up and release the
	 * lock on the communication area.
	 */
	if (ipc.ip_req < 0) {
		error = EIO;
	} else {
		rvp->r_val1 = ipc.ip_data;
		error = 0;
	}
	mutex_unlock(&ipc.ip_lock);
	_SAT_PTRACE(uap->req, uap->pid, p->p_cred, error);

	VPROC_RELE(vpr);

	return error;
}

/*
 * Code that the child process executes to implement the command
 * of the parent process in tracing.
 *
 * Returning 0 implies stay in UT_STOPed, returning non-zero
 * means resume user mode execution.
 */
static int
procxmt(void)
{
	register int i;
	register char *p;
	register __psint_t olddata;
	register unsigned regno;
	register int s;
	uthread_t *ut = curuthread;
	int width;
#if _MIPS_SIM == _ABI64
	int abi_has64bitregs = ABI_HAS_64BIT_REGS(get_current_abi());
#endif

	/*
	 * Make sure we were called validly.  If the communication lock
	 * isn't held, we know right away that it can't involve us.
	 */
	if (!mutex_owner(&ipc.ip_lock))
		return(0);

	/*
	 * Next, make sure that we are the intended target of this
	 * communication.  This check is safe, since we know the lock
	 * is held, and these fields should be valid.
	 */
	if (ipc.ip_tptr != ut || ipc.ip_pid != current_pid())
		return(0);

	i = ipc.ip_req;
	ipc.ip_req = 0;

	/*
	 * if we're skipping over any watchpoints, cancel them now
	 * in case the world changes while we're stopped
	 */
	if (ut->ut_watch) {
		cancelskipwatch();
		ASSERT((ut->ut_watch->pw_flag & WINTSYS) == 0);
		ASSERT((ut->ut_flags & UT_WSYS) == 0);
	}
	/*
	 * Perform the requested action.
	 */
	switch (i) {
	/*
	 * read user I/D
	 */
	case PTRC_RD_I:
	case PTRC_RD_D: {

		int data;	/* We are copying words, so give copyin
				 * a word address (ipc.ip_data is a __psint_t.)
				 */
		if ((__psint_t)ipc.ip_addr & (NBPW-1) ||
		    copyin(ipc.ip_addr, &data, NBPW))
			goto error;
		ipc.ip_data = data;
		break;
	}

	/*
	 * read user registers and u area values
	 */
	case PTRC_RD_REG:
		i = (__psint_t)ipc.ip_addr;
		if ((unsigned)(regno = i - GPR_BASE) < NGP_REGS)
			ipc.ip_data = (regno==0) ? 0 : USER_REG(EF_AT+regno-1);
		else if ((unsigned)(regno = i - FPR_BASE) < NFP_REGS) {
			checkfp(curuthread, 0);
			ipc.ip_data = PCB(pcb_fpregs)[regno];
		} else if ((unsigned)(regno = i - SIG_BASE) < NSIG_HNDLRS)
			ipc.ip_data = (__psint_t)UT_TO_PROC(ut)->
						p_sigvec.sv_hndlr[regno];
		else if ((unsigned)(regno = i - SPEC_BASE) < NSPEC_REGS) {
			switch(i) {
			case PC: ipc.ip_data = USER_REG(EF_EPC); break;
			case CAUSE: ipc.ip_data = USER_REG(EF_CAUSE); break;
			case BADVADDR: ipc.ip_data = USER_REG(EF_BADVADDR); break;
			case MMHI: ipc.ip_data = USER_REG(EF_MDHI); break;
			case MMLO: ipc.ip_data = USER_REG(EF_MDLO); break;
			case FPC_CSR: ipc.ip_data = PCB(pcb_fpc_csr); break;
			case FPC_EIR: ipc.ip_data = PCB(pcb_fpc_eir); break;
			default:
				goto error;
			}
		} else
			goto error;
		break;

	/*
	 * write user I
	 * Must set text up to allow writing
	 */
	case PTRC_WR_I:
		(void)fuiword(ipc.ip_addr);
		if (!write_utext(ipc.ip_addr, ipc.ip_data))
			goto error;
		break;

	/*
	 * write user D
	 */
	case PTRC_WR_D:
		(void)fuword(ipc.ip_addr);
		if (suword(ipc.ip_addr, 0) < 0)
			goto error;
		(void) suword(ipc.ip_addr, ipc.ip_data);
		dki_dcache_wbinval(ipc.ip_addr, sizeof(int));
		break;

	/*
	 * write u
	 */
	case PTRC_WR_REG:
		i = (__psint_t)ipc.ip_addr;
		if ((unsigned)(regno = i - GPR_BASE) < NGP_REGS) {
			if (regno == 0)
				goto error;
			p = (char *)&USER_REG(EF_AT+regno-1);
			width = sizeof(k_machreg_t);
		} else if ((unsigned)(regno = i - FPR_BASE) < NFP_REGS) {
			checkfp(curuthread, 0);
			p = (char *)&PCB(pcb_fpregs)[regno];
			width = sizeof(k_machreg_t);
		} else if ((unsigned)(regno = i - SIG_BASE) < NSIG_HNDLRS) {
			p = (char *)&UT_TO_PROC(ut)->p_sigvec.sv_hndlr[regno];
			width = sizeof(char *);
		} else if ((unsigned)(regno = i - SPEC_BASE) < NSPEC_REGS) {
			if (regmap[regno].rm_wrprot)
				goto error;
			/*
			 * complain if pc unaligned
			 */
			if (i == PC && (ipc.ip_data & sizeof(int)-1))
				goto error;
			switch(i) {
			case PC: p = ADDR(USER_REG(EF_EPC)); break;
			case CAUSE: p = ADDR(USER_REG(EF_CAUSE)); break;
			case BADVADDR: p = ADDR(USER_REG(EF_BADVADDR)); break;
			case MMHI: p = ADDR(USER_REG(EF_MDHI)); break;
			case MMLO: p = ADDR(USER_REG(EF_MDLO)); break;
			case FPC_CSR:
				p = ADDR(PCB(pcb_fpc_csr));
				ipc.ip_data &= ~FPCSR_EXCEPTIONS;
				break;
			case FPC_EIR: p = ADDR(PCB(pcb_fpc_eir)); break;
			default:
				goto error;
			}
			width = regmap[regno].rm_width;
		} else
			goto error;
		switch (width) {
		case sizeof(int):
			olddata = *(int *)p;
			*(int *)p = ipc.ip_data;
			ipc.ip_data = olddata;
			break;
		case sizeof(k_machreg_t):
			olddata = *(k_machreg_t *)p;
			*(k_machreg_t *)p = ipc.ip_data;
			ipc.ip_data = olddata;
			break;
		default:
			panic("ptrace regmap botch");
		}
		break;

	/*
	 * set signal and continue
	 * one version causes a trace-trap
	 */
	case PTRC_CONTINUE:
	case PTRC_STEP: {
		sigqueue_t *sqp;

		if ((__psint_t)ipc.ip_addr != 1) {
			/*
			 * complain if pc unaligned
			 */
			if ((__psint_t)ipc.ip_addr & (sizeof(int)-1))
				goto error;
			curexceptionp->u_eframe.ef_epc =
						(k_machreg_t)ipc.ip_addr;
		}

		if ((__psunsigned_t)ipc.ip_data >= NSIG)
			goto error;
		/*
		 * Getting out in the middle here.  Clear any pending
		 * signals, send ourselves a signal if desired, and
		 * kick the parent to let him know we are finished.
		 */
		s = ut_lock(ut);
		
		cursiginfofree(ut);

		ASSERT(UT_TO_PROC(ut)->p_sigvec.sv_sigqueue == NULL);
		while (sqp = ut->ut_sigqueue) {
			ut->ut_sigqueue = sqp->sq_next;
			sigqueue_free(sqp);
		}
		
		sigemptyset(&ut->ut_sig);
		
		if (i == PTRC_STEP)
			ut->ut_flags |= UT_STEP;
		
		if (ipc.ip_data)
			ut->ut_cursig = ipc.ip_data;
		else
			ut->ut_cursig = 0;
			/*sigtoproc(cp, ipc.ip_data, ASYNC_SIGNAL);*/

		ut_unlock(ut, s);

		vsema(&ipc.ip_wait);
		return (1);
	}

	/*
	 * read all regular registers
	 */
	case PTRC_RD_GPRS:
#if _MIPS_SIM == _ABI64
		if (abi_has64bitregs) {
			(void) sulong(ipc.ip_addr, 0);
			for (i=1; i < NGP_REGS; i++)
				(void) sulong( ((machreg_t *)ipc.ip_addr)+i,
						USER_REG(i+EF_AT-1));
			break;
		}
#endif
		(void) suword(ipc.ip_addr, 0);
		for (i=1; i < NGP_REGS; i++)
			(void) suword(ipc.ip_addr+i, USER_REG(i+EF_AT-1));
		break;

	/*
	 * read all floating point registers
	 */
	case PTRC_RD_FPRS:
		checkfp(ut, 0);
#if _MIPS_SIM == _ABI64
		if (abi_has64bitregs) {
			for (i=0; i < NFP_REGS; i++)
				(void) sulong( ((machreg_t *)ipc.ip_addr)+i,
							PCB(pcb_fpregs)[i]);
			break;
		}
#endif
		for (i=0; i < NFP_REGS; i++)
			(void) suword(ipc.ip_addr+i, PCB(pcb_fpregs)[i]);
		break;

	/*
	 * write all regular registers
	 */
	case PTRC_WR_GPRS:
#if _MIPS_SIM == _ABI64
		if (abi_has64bitregs) {
			for (i=1; i < NGP_REGS; i++)
				USER_REG(i+EF_AT-1) =
					fulong( ((machreg_t *)ipc.ip_addr)+i);
			break;
		}
#endif
		for (i=1; i < NGP_REGS; i++)
			USER_REG(i+EF_AT-1) = fuword(ipc.ip_addr+i);
		break;

	/*
	 * write all floating point registers
	 */
	case PTRC_WR_FPRS:
		checkfp(ut, 0);
#if _MIPS_SIM == _ABI64
		if (abi_has64bitregs) {
			for (i=0; i < NFP_REGS; i++)
				PCB(pcb_fpregs)[i] =
					fulong( ((machreg_t *)ipc.ip_addr)+i);
			break;
		}
#endif
		for (i=0; i < NFP_REGS; i++)
			PCB(pcb_fpregs)[i] = fuword(ipc.ip_addr+i);
		break;


	/* force exit */
	case PTRC_TERMINATE: {
		int tmp;

		/*
		 * Kick the parent so he knows something happened to us
		 * and go commit suicide.
		 */
		vsema(&ipc.ip_wait);
		s = ut_lock(ut);
		tmp = fsig(ut, (sigvec_t *)NULL, SIG_ALLSIGNALS);
		ut_unlock(ut, s);
		exit(CLD_EXITED, tmp);
	}

	default:
	error:
		ipc.ip_req = -1;
	}
	/*
	 * Did whatever was requested.  Kick the parent and go about our
	 * business.
	 */
	vsema(&ipc.ip_wait);
	return (0);
}

/*
 * Write to user instruction space.
 */
int
write_utext(void *addr, u_int data)
{
	vasid_t vasid;
	as_setrangeattr_t attrset;
	as_getvaddrattr_t attrres;
	int i, rdonly = 0;

	as_lookup_current(&vasid);

	if (VAS_GETVADDRATTR(vasid, AS_MPROT, addr, &attrres))
		return 0;

	if (!(attrres.as_mprot & PROT_WRITE)) {
	        /*
		 * We set as_prot_flags to one to inhibit tlb flushing 
		 * although this would normally not happen anyway.  The 
                 * only case in  which it would has to do with the 
                 * r[45]000_jump_war_correct code in pas_mrotect.  See
                 * the comment below for the other VAS_SETRANGEATTR
                 * call for an explanation of why we should not flush
                 * the tlb on this path.
		 */
		rdonly = 1;
		attrset.as_op = AS_PROT;
		attrset.as_prot_prot = attrres.as_mprot | PROT_WRITE;
		attrset.as_prot_flags = 1;

		if (VAS_SETRANGEATTR(vasid, addr, sizeof(int), &attrset, NULL))
			return 0;
	}

	i = suiword(addr, data);
	
	if (rdonly) {
		/*
		 * since we are often called from trapstart/end
		 * we must make sure that we don't cause a fault before
		 * the thread can execute the instruction, since if it did
		 * we would come into tlbmiss, which would de-install the
		 * breakpoint, cure the fault, call us to re-install the bkpt
		 * and we would get into an infinite loop.
		 * So, against our better judgement, we do NOT flush the
		 * tlb after resetting the WRITE protections....
                 * Setting as_prot_flags to one asks VAS_SETRANGEATTR
                 * to avoid flushing the tlb when it would otherwise do
                 * so.
		 */
		attrset.as_op = AS_PROT;
		attrset.as_prot_prot = attrres.as_mprot;
		attrset.as_prot_flags = 1;

		if (VAS_SETRANGEATTR(vasid, addr, sizeof(int), &attrset, NULL))
			return 0;
	} else {
		/* If writeable we need to keep icache coherent with dcache
		 * in the rdonly case this is done in the VAS layer.
		 */
		cache_operation(addr, sizeof(int), CACH_ICACHE_COHERENCY);
	}
	if (i < 0)
		return(0);

	return(1);
}

/*
 * This routine returns the current signal mask of the given
 * process, and sets the mask to hold off all signals except
 * SIGKILL and SIGSTOP.
 */
void
hold_nonfatalsig(k_sigset_t *set)
{
#if	(_MIPS_SIM != _ABIO32)
	k_sigset_t nset = { ~(sigmask(SIGKILL)|sigmask(SIGSTOP)) };
#else
	k_sigset_t nset = { ~(sigmask(SIGKILL)|sigmask(SIGSTOP)), 0xffffffff };
#endif
	assign_cursighold(set, &nset);
}

/*
 * Restore the signal handling of the given process.
 */
void
release_nonfatalsig(k_sigset_t *set)
{
	assign_cursighold(0, set);
}

void
assign_cursighold(k_sigset_t *oset, k_sigset_t *nset)
{
	uthread_t *ut = curuthread;
	k_sigset_t set;
	u_int cursig;
	int s;

	sigemptyset(&set);

	s = ut_lock(ut);
	if (oset)
		*oset = *ut->ut_sighold;
	*ut->ut_sighold = *nset;
	sigorset(&set, &ut->ut_sig);
	if (cursig = ut->ut_cursig)
		sigaddset(&set, cursig);
	sigdelset(&set, SIGPTRESCHED);
	sigdiffset(&set, ut->ut_sighold);
	if (oset)
		sigdiffset(&set, oset);

	if (!sigisempty(&set))
		thread_interrupt(UT_TO_KT(ut), &s);
	else
		thread_interrupt_clear(UT_TO_KT(ut), 1);
	ut_unlock(ut, s);
}

/* Return true if curuthread has an unheld signal pending */
int
sigisready(void)
{
	uthread_t *ut = curuthread;
	k_sigset_t set;
	u_int cursig = ut->ut_cursig;

	if (cursig == 0 && sigisempty(&ut->ut_sig))
		return 0;

	sigemptyset(&set);
	sigorset(&set, &ut->ut_sig);
	if (cursig) {
		sigaddset(&set, cursig);
	}

	/* Remove any held signals */
	sigdiffset(&set, ut->ut_sighold);

	return !sigisempty(&set);
}

/*
 * sigispend:
 *    this is used to see if a signal is already in the process of
 *    being delivered to a process
 */
static int
ut_sigispend(uthread_t *ut, void *arg)
{
	int	sig = (__psint_t) arg;
        return (ut->ut_cursig == sig || sigismember(&ut->ut_sig, sig));
}
int
sigispend(proc_t *p, int sig)
{
	sigvec_t *sigvp;

	/*
	 * Check if signal is pending for any individual thread.
	 */
	if (uthread_apply(&p->p_proxy, UT_ID_NULL, ut_sigispend,
				(void *)(__psint_t) sig))
		return 1;

	/*
	 * Check if signal is pending for the process at large.
	 */
	VPROC_GET_SIGVEC(PROC_TO_VPROC(p), VSIGVEC_PEEK, sigvp);

	if (sigismember(&sigvp->sv_sig, sig))
		return 1;

	return 0;
}

/*
 * Posix 1003.1b sigqueue()
 * differs from .1b by allowsing user to specify si_code
 * irix5_sigqueue
 */
struct sigqa {
	sysarg_t	pid;
	sysarg_t	signo;
	sysarg_t	si_code;
	sysarg_t	value;
};


int
sigqueue_sys(struct sigqa *uap)
{
	union sigval	value;
	vproc_t		*vpr;
	int		error;

	value.sival_ptr = (void *)uap->value;
	
	if ((vpr = VPROC_LOOKUP(uap->pid)) == NULL) {
		error = ESRCH;
	} else {
		error = ksigqueue(vpr, uap->signo, uap->si_code, value, 0);
		VPROC_RELE(vpr);
	}

	return error;
}

/*
 * ksigqueue:
 * 	This is the common function for sending queued signals to
 *	processes.  The kernel argument allows kernel callers to
 *	avoid permission checks.
 */
int
ksigqueue(
	vproc_t *vpr,
	int signo,
	int si_code,
	const union sigval value,
	int kernel)
{
	int error;
	int flags;
	k_siginfo_t info;
	pid_t sid;

	/*
	 * Short-circuit checks.
	 */
	if (signo < 0 || signo >= NSIG)
		return EINVAL;

	if (vpr->vp_pid == INIT_PID && sigismember(&cantmask, signo))
		return EPERM;

	/* set up the siginfo struct
	 * sigqueue only updates si_value
	 */
	bzero((caddr_t)&info, sizeof(k_siginfo_t));
	info.si_signo = signo;
	info.si_code = si_code;
	info.si_value = value;

	if (kernel) {
		flags = SIG_SIGQUEUE | SIG_ISKERN;
		sid = 0;
	} else {
		vp_get_attr_t attr;

		flags = SIG_SIGQUEUE;
		VPROC_GET_ATTR(curvprocp, VGATTR_SID, &attr);
		sid = attr.va_sid;
	}

	VPROC_SENDSIG(vpr, signo, flags, sid, get_current_cred(), &info, error);
        return error;
}


/*
 * The sigpoll*() routines implement sigwait(), sigwaitrt() and
 * sigtimedwait().
 */
struct sigpolla {
	sigset_t *set;
	siginfo_t *info;
	timespec_t *ts;
};

int	
sigpoll_sys(struct sigpolla *uap, rval_t *rvp)		
{
	timespec_t timespec;
	sigset_t uset;
	k_sigset_t kset;
	irix5_siginfo_t irix5info;
#if _MIPS_SIM == _ABI64
	irix5_64_siginfo_t irix5_64_info;
#endif
	k_siginfo_t info;
	int error;
	int abi = get_current_abi();

	if (copyin(uap->set, &uset, sizeof uset))
		return EFAULT;
	
	sigutok(&uset, &kset);
	
	if (uap->ts) {
		if (COPYIN_XLATE(uap->ts, &timespec, sizeof timespec,
				irix5_to_timespec, abi, 1))
			return EFAULT;
		if (timespec.tv_nsec < 0 || timespec.tv_nsec >= NSEC_PER_SEC)
			return EINVAL;
	}
	
	if (error = sigpoll(&kset, (uap->ts) ? &timespec : NULL, &info)) 
		return(error);
	
	if (uap->info) {
		switch(abi) {
		    case ABI_IRIX5:
		    case ABI_IRIX5_N32:
			    irix5_siginfoktou(&info, &irix5info);
			    if (copyout(&irix5info, uap->info,
					sizeof irix5info))
				    return EFAULT;
			    break;
#if _MIPS_SIM == _ABI64
		    case ABI_IRIX5_64:
			    irix5_64_siginfoktou(&info, &irix5_64_info);
			    if (copyout(&irix5_64_info, uap->info,
					sizeof irix5_64_info))
				    return EFAULT;
			    break;
#endif
		    default:
			    cmn_err(CE_PANIC,
				    "sigpoll: unrecognized ABI (0x%x)", abi);
			/* NOTREACHED */
		}
	}
	
	rvp->r_val1 = info.si_signo;

	return 0;
}

/*
 * sigpoll()
 *
 * Routine to actually implement sigwait() and its derivatives.
 *
 * Return 0 if a signal was received, error code otherwise.
 */
static int
sigpoll(k_sigset_t *kset, struct timespec *ts, k_siginfo_t *info)
{
	kthread_t *kt = curthreadp;
	uthread_t *ut = KT_TO_UT(kt);
	int error, s;
	timespec_t rts;
	sigvec_t *sigvp;
	int     zerotime = (ts && ts->tv_sec == 0 && ts->tv_nsec == 0);
	int     ret = 0;

	/*
	 * First, check to see if we are already handling a signal.
	 */
	VPROC_GET_SIGVEC(curvprocp, VSIGVEC_UPDATE, sigvp);
	s = ut_lock(ut);
	error = sigpoll_check(kset, info, sigvp);
	if (error == 0 || error == EINTR || zerotime)
		ret = 1;
	else {
		/*
	 	 * Set up ut_sigwait so we will know what signals 
		 * to wake up on. Do this early so if this is a
		 * pthread, signallers know to deliver it to this
		 * ut_sig.
	 	 */
#ifdef why_bother
		sigemptyset(&ut->ut_sigwait);
		sigorset(&ut->ut_sigwait, kset);
#else
		ut->ut_sigwait = *kset;
#endif
	}
	ut_unlock(ut, s);
	VPROC_PUT_SIGVEC(curvprocp);

	if (ret) {
		return error;
	}

	s = ut_lock(ut);

	/*
	 * If a signal came in while ut_lock was released, it should
	 * be in ut_sig by now, and a thread_interrupt should have 
	 * been done on us already.
	 */

	if (ts) {
		error = ut_timedsleepsig(ut, &kt->k_timewait, 0, s,
					 kt_has_fastpriv(kt) ? SVTIMER_FAST : 0,
					 ts, &rts);
	} else {
		error = ut_sleepsig(ut, &kt->k_timewait, 0, s);
	}

	/* Signal or timeout. */
	VPROC_GET_SIGVEC(curvprocp, VSIGVEC_UPDATE, sigvp);
	s = ut_lock(ut);
	error = sigpoll_check(kset, info, sigvp);
	ut_unlock(ut, s);
	VPROC_PUT_SIGVEC(curvprocp);

	sigemptyset(&ut->ut_sigwait); /* no locks?!! */

	return error;
}


/* 
 * sigpoll_check()
 *
 * Check to see if there are any signals we are waiting on.  If yes, return
 * 0 and fill in the info structure.  If we are in the middle of handling a
 * different signal, return EINTR.  If no signals were found, return EAGAIN.
 */
static int
sigpoll_check(k_sigset_t *kset, k_siginfo_t *info, sigvec_t *sigvp)
{
	uthread_t *ut = curuthread;
	int n;
	int error = 0;

	/*
	 * See if there is a signal currently being handled.
	 * If not, check the pending signals to see if there's a match.
	 */
	(void)issig(1, SIG_ALLSIGNALS);

	if (n = ut->ut_cursig) {
		/*
		 * If this is the signal we want, get the info and cancel
		 * the signal.  Otherwise, set error to EINTR.
		 */
		if (sigismember(kset, n)) {
			sigqueue_t *sqp;
			
			info->si_signo = n;
			
			if ((sqp = ut->ut_curinfo) != NULL) {
				info->si_code = sqp->sq_info.si_code;
				info->si_value = sqp->sq_info.si_value;
				
				cursiginfofree(ut);
			}
			ut->ut_cursig = 0;
		} else {
			error = EINTR;
		}
	} else {
		k_sigset_t pend;

		/*
		 * Grab pending signals and remove any signals that aren't
		 * in kset.
		 *
		 * NOTE: We only need to look in ut_sig because asynchronous
		 * signals will be delivered directly to a sigwaiter.
		 * NOT!  Waiter could have been blocking the signal.
		 */
		pend = ut->ut_sig;
		sigandset(&pend, kset);

		if (sigisempty(&pend)) {
			pend = sigvp->sv_sig;
			sigandset(&pend, kset);
		}

		/*
		 * If any signals remain in the pending list, then one of
		 * them is one we are interested in.  Otherwise, set error
		 * to EAGAIN.
		 */
		if (!sigisempty(&pend)) {
			int sig;
			sigqueue_t *sqp;
			__uint64_t sigbit;

			/*
			 * Find the signal that we are interested in.
			 */
			for (sig = 1, sigbit = sigmask(1);
			     sig < NSIG;
			     sig++, sigbit <<= 1) {
				if (sigbitismember(&pend, sigbit, sig))
					break;
			}

			ASSERT(sig >= 1 && sig < NSIG);

			/*
			 * Get this signal's info and clear it from the
			 * pending list.
			 */
			if (sigbitismember(&ut->ut_sig, sigbit, sig)) {
				sigbitdelset(&ut->ut_sig, sigbit, sig);
				sqp = sigdeq(&ut->ut_sigpend, sig, sigvp);
			} else {
				ASSERT(sigismember(&sigvp->sv_sig, sig));
				sigbitdelset(&sigvp->sv_sig, sigbit, sig);
				sqp = sigdeq(&sigvp->sv_sigpend, sig, sigvp);
			}

			/*
			 * Copy information about this signal into the info
			 * structure.
			 */
			info->si_signo = sig;
			
			if (sqp) {
				info->si_code = sqp->sq_info.si_code;
				info->si_value = sqp->sq_info.si_value;
				sigqueue_free(sqp);
			} else {
				info->si_code = 0;
				info->si_value.sival_int = 0;
			}
		} else {
			error = EAGAIN;
		}
	}

	return (error);
}

void
irix5_n32_savecontext(irix5_n32_ucontext_t *ucp, k_sigset_t *mask)
{
	proc_proxy_t *prxy = curuthread->ut_pproxy;
	register irix5_64_greg_t *surp;
	register __uint64_t *sfrp;
	register k_machreg_t *urp;
	register k_fpreg_t *frp;
	as_getasattr_t asattr;
	vasid_t vasid;

	as_lookup_current(&vasid);

	ucp->uc_flags = UC_ALL;
	ucp->uc_link = (app32_ptr_t)(__psunsigned_t)prxy->prxy_oldcontext;

	/* save signal mask */
	sigktou(mask, &ucp->uc_sigmask);

	if (!(prxy->prxy_ssflags & SS_ONSTACK)) {
		/* lowest stack address */
		VAS_LOCK(vasid, AS_SHARED);
		VAS_GETASATTR(vasid, AS_STKBASE|AS_STKSIZE, &asattr);
		VAS_UNLOCK(vasid);
		ucp->uc_stack.ss_sp =
				(app32_ptr_t)(__psunsigned_t)asattr.as_stkbase;
		ucp->uc_stack.ss_size = asattr.as_stksize;
		ucp->uc_stack.ss_flags = 0;
	} else {
		ucp->uc_stack.ss_sp =
				(app32_ptr_t)(__psunsigned_t)prxy->prxy_sigsp;
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
	surp = &ucp->uc_mcontext.gregs[0];
	*surp++ = 0;
	for (urp = &USER_REG(EF_AT); urp <= &USER_REG(EF_RA); urp++)
		*surp++ = *urp;
	*surp++ = USER_REG(EF_MDLO);
	*surp++ = USER_REG(EF_MDHI);
	*surp++ = USER_REG(EF_CAUSE);
	*surp++ = USER_REG(EF_EPC);

	/*
	 * give user the SR so can tell what mode - 32/64 address, endian,
	 * 16/32 fp regs
	 */
	*surp = USER_REG(EF_SR);
	ASSERT(surp == &ucp->uc_mcontext.gregs[IRIX5_64_NGREG-1]);

	if (!PCB(pcb_ownedfp)) {
		ucp->uc_flags &= ~UC_MAU;	/* don't save fpregs */
	} else {
		checkfp(curuthread, 0);	/* dump fp to pcb */

		sfrp = &ucp->uc_mcontext.fpregs.fp_r.fp_regs[0];
		for (frp = PCB(pcb_fpregs);
		     frp < &PCB(pcb_fpregs)[32];
		     sfrp++, frp++)
			*sfrp = (k_fpreg_t)*frp;
		ucp->uc_mcontext.fpregs.fp_csr = PCB(pcb_fpc_csr);
		/* don't save fp_eir since can't restore it anyway */
		PCB(pcb_fpc_csr) &= ~FPCSR_EXCEPTIONS;
	}
}

int
irix5_n32_restorecontext(irix5_n32_ucontext_t *u_ucp)
{
	k_fpreg_t *frp;
	k_machreg_t *urp;
	irix5_64_greg_t *surp;
	__uint64_t *sfrp;
	uthread_t *ut = curuthread;
	proc_proxy_t	*prxy = ut->ut_pproxy;
	as_setattr_t setattr;
	vasid_t vasid;
	irix5_n32_ucontext_t i5_n32_uc, *ucp = &i5_n32_uc;

	as_lookup_current(&vasid);

	if (copyin(u_ucp, ucp, sizeof(*ucp)))
		return EFAULT;

	if (ucp->uc_flags & UC_STACK) {
		if (ucp->uc_stack.ss_flags & SS_ONSTACK) {
			prxy->prxy_sigsp =
				(void *)(__psunsigned_t)ucp->uc_stack.ss_sp;
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
		for (urp = &USER_REG(EF_AT), surp = &ucp->uc_mcontext.gregs[1];
		     urp <= &USER_REG(EF_RA); urp++, surp++)
			*urp = *surp;
		USER_REG(EF_MDLO) = *surp++;
		USER_REG(EF_MDHI) = *surp++;
		surp++; 		/* skip EF_CAUSE */
		USER_REG(EF_EPC) = *surp;

		/* skip EF_SR */
		ASSERT(surp == &ucp->uc_mcontext.gregs[IRIX5_64_NGREG-2]);
	}

	if (ucp->uc_flags & UC_MAU) {
		checkfp(ut, 1);	/* toss current fp contents */
		for (sfrp = &ucp->uc_mcontext.fpregs.fp_r.fp_regs[0],
		    frp = PCB(pcb_fpregs);
		    frp < &PCB(pcb_fpregs)[32]; frp++, sfrp++)
			*frp = (k_fpreg_t)*sfrp;
		PCB(pcb_fpc_csr) =
		    ucp->uc_mcontext.fpregs.fp_csr & ~FPCSR_EXCEPTIONS;
	}

	if (ucp->uc_flags & UC_SIGMASK) {
		int s = ut_lock(ut);
		sigutok(&ucp->uc_sigmask, ut->ut_sighold);
		sigdiffset(ut->ut_sighold, &cantmask);
		ut_unlock(ut, s);
	}

	prxy->prxy_oldcontext = (void *)(__psunsigned_t)ucp->uc_link;
	return 0;
}

void
irix5_savecontext(irix5_ucontext_t *ucp, k_sigset_t *mask)
{
	proc_proxy_t *prxy = curuthread->ut_pproxy;
	register irix5_greg_t *surp;
	register __uint32_t *sfrp;
	register k_machreg_t *urp;
	register k_fpreg_t *frp;
	as_getasattr_t asattr;
	vasid_t vasid;

	as_lookup_current(&vasid);

	ucp->uc_flags = UC_ALL;
	ucp->uc_link = (app32_ptr_t)(__psunsigned_t)prxy->prxy_oldcontext;

	/* save signal mask */
	sigktou(mask, &ucp->uc_sigmask);

	if (!(prxy->prxy_ssflags & SS_ONSTACK)) {
		/* lowest stack address */
		VAS_LOCK(vasid, AS_SHARED);
		VAS_GETASATTR(vasid, AS_STKBASE|AS_STKSIZE, &asattr);
		VAS_UNLOCK(vasid);
		ucp->uc_stack.ss_sp = (ulong)asattr.as_stkbase;
		ucp->uc_stack.ss_size = asattr.as_stksize;
		ucp->uc_stack.ss_flags = 0;
	} else {
		ucp->uc_stack.ss_sp =
			(app32_ptr_t)(__psunsigned_t)prxy->prxy_sigsp;
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
	ucp->uc_triggersave = 0;	/* Obsolete field */
	surp = &ucp->uc_mcontext.gregs[0];
	*surp++ = 0;
	for (urp = &USER_REG(EF_AT); urp <= &USER_REG(EF_RA); urp++)
		*surp++ = *urp;
	*surp++ = USER_REG(EF_MDLO);
	*surp++ = USER_REG(EF_MDHI);
	*surp++ = USER_REG(EF_CAUSE);
	*surp = USER_REG(EF_EPC);

	ASSERT(surp == &ucp->uc_mcontext.gregs[IRIX5_NGREG-1]);

	if (!PCB(pcb_ownedfp)) {
		ucp->uc_flags &= ~UC_MAU;	/* don't save fpregs */
	} else {
		checkfp(curuthread, 0);	/* dump fp to pcb */

		sfrp = &ucp->uc_mcontext.fpregs.fp_r.fp_regs[0];
		for (frp = PCB(pcb_fpregs);
		     frp < &PCB(pcb_fpregs)[32];
		     sfrp++, frp++)
			*sfrp = (k_fpreg_t)*frp;
		ucp->uc_mcontext.fpregs.fp_csr = PCB(pcb_fpc_csr);
		/* don't save fp_eir since can't restore it anyway */
		PCB(pcb_fpc_csr) &= ~FPCSR_EXCEPTIONS;
	}
}

int
irix5_restorecontext(irix5_ucontext_t *u_ucp)
{
	register k_fpreg_t *frp;
	register k_machreg_t *urp;
	register irix5_greg_t *surp;
	register __uint32_t *sfrp;
	int sig_context = 0;
	proc_proxy_t	*prxy = curuthread->ut_pproxy;
	as_setattr_t setattr;
	vasid_t vasid;
	irix5_ucontext_t i5_uc, *ucp = &i5_uc;
	app32_long_t extra_context;

	as_lookup_current(&vasid);

	if (extra_context = fuword(&(u_ucp->irix5_extra_ucontext)))
		sig_context++;

	/* check alignment of pointer before referencing through it (488060) */

	if (copyin(u_ucp, ucp, sizeof(*ucp)))
		return EFAULT;

	if (ucp->uc_flags & UC_STACK) {
		if (ucp->uc_stack.ss_flags & SS_ONSTACK) {
			prxy->prxy_sigsp =
				(void *)(__psunsigned_t)ucp->uc_stack.ss_sp;
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

	if (sig_context) {
		/*
		 * The context is being restored by sigreturn, so we must
		 * retore the extra context block that was saved by sigsend.
		 */
		if (ucp->uc_flags & UC_CPU) {
			/*
			 * verify that extra context block is next to ucp
			 * (or siginfo). If it is not, we merely restore
			 * context from ucp and ignore the extra context block.
			 */
			if (((ulong) extra_context - (ulong) u_ucp) > 
			    (sizeof(irix5_ucontext_t) +
			     sizeof(irix5_siginfo_t))) {

				for (urp = &USER_REG(EF_AT),
				     surp = &ucp->uc_mcontext.gregs[1];
				     urp <= &USER_REG(EF_RA); urp++, surp++)
					*urp = *surp;

				USER_REG(EF_MDLO) = *surp++;
				USER_REG(EF_MDHI) = *surp++;
				surp++; 		/* skip EF_CAUSE */
				USER_REG(EF_EPC) = *surp;
			} else {
				irix5_64_gregset_t ext_reg;
				irix5_64_greg_t *secb = ext_reg;

				if (copyin((void *)(__psint_t)extra_context,
					   secb,
					   sizeof(irix5_64_gregset_t)))
					return EFAULT;

				secb++; /* skip r0 */
				for (urp = &USER_REG(EF_AT),
				     surp = &ucp->uc_mcontext.gregs[1];
				     urp <= &USER_REG(EF_RA);
				     urp++, surp++, secb++) {
					/* cmp 32-bit value in mcontext (surp)
					 * with lower half of 64-bit value saved
					 * in extra context block (secb)
				 	 */
					if (*surp == (irix5_greg_t) *secb) {
						/* unchanged by user
						 * copy full 64-bit value
						 */
						*urp = *secb;
					} else {
						/* save the sign-extended
						 * 32-bit value
						 */
						*urp = *surp;
					}
				}

				USER_REG(EF_MDLO) =
				  (*surp == (irix5_greg_t)*secb) ? *secb : *surp;
				surp++; secb++;
				USER_REG(EF_MDHI) =
				  (*surp == (irix5_greg_t)*secb) ? *secb : *surp;
				surp++; secb++;
				secb++; surp++; 	/* skip EF_CAUSE */
				USER_REG(EF_EPC) =
				  (*surp == (irix5_greg_t)*secb) ? *secb : *surp;
			}
			ASSERT(surp == &ucp->uc_mcontext.gregs[IRIX5_NGREG-1]);
		}
	} else if (ucp->uc_flags & UC_CPU) {
		/*
		 * We are restoring context saved by getcontext not sendsig.
		 * In this case, the extra context block was not saved so
		 * there is no need to restore it.
		 */  
		for (urp = &USER_REG(EF_AT), surp = &ucp->uc_mcontext.gregs[1];
		     urp <= &USER_REG(EF_RA); urp++, surp++)
			*urp = *surp;

		USER_REG(EF_MDLO) = *surp++;
		USER_REG(EF_MDHI) = *surp++;
		surp++;                 /* skip EF_CAUSE */
		USER_REG(EF_EPC) = *surp;
	}

	if (ucp->uc_flags & UC_MAU) {
		checkfp(curuthread, 1);	/* toss current fp contents */
		for (sfrp = &ucp->uc_mcontext.fpregs.fp_r.fp_regs[0],
			frp = PCB(pcb_fpregs);
		    frp < &PCB(pcb_fpregs)[32]; frp++, sfrp++)
			*frp = (k_fpreg_t)*sfrp;
		PCB(pcb_fpc_csr) =
		    ucp->uc_mcontext.fpregs.fp_csr & ~FPCSR_EXCEPTIONS;
	}

	if (ucp->uc_flags & UC_SIGMASK) {
		int s;
		k_sigset_t holdmask;

		sigutok(&ucp->uc_sigmask, &holdmask);
		sigdiffset(&holdmask, &cantmask);
		s = ut_lock(curuthread);		/* why bother? */
		*curuthread->ut_sighold = holdmask;
		ut_unlock(curuthread, s);
	}

	prxy->prxy_oldcontext = (void *)(__psunsigned_t)ucp->uc_link;

	return 0;
}


/* To support mips3/mips4 o32 binaries, we need to store 
 * full 64-bits values also in order for signals to work. Then we 
 * can detect changed lower halves of registers, and sign-extend 
 * the values into 64-bit values on signal exit. Called by sendsig()
 */
static void
irix5_extra_savecontext(irix5_ucontext_t *ucp)
{
	register irix5_64_greg_t *surp;
	register k_machreg_t *urp;

	/*
	 * Save extra machine context.
	 */
	surp = (irix5_64_greg_t *) ((__psunsigned_t) ucp->irix5_extra_ucontext);
	*surp++ = 0;
	for (urp = &USER_REG(EF_AT); urp <= &USER_REG(EF_RA); urp++)
		*surp++ = *urp;
	*surp++ = USER_REG(EF_MDLO);
	*surp++ = USER_REG(EF_MDHI);
	*surp++ = USER_REG(EF_CAUSE);
	*surp = USER_REG(EF_EPC);
}

/*
 * setcontext/getcontext system calls
 * NOTE: there are multi-abi versions of these routines
 */
struct contexta {
	void *ucp;
};

int
setcontext(struct contexta *uap)
{
	vasid_t vasid;

	/*
	 * In future releases, when the ucontext structure grows,
	 * getcontext should be modified to only return the fields
	 * specified in the uc_flags.
	 * That way, the structure can grow and still be binary
	 * compatible will all .o's which will only have old fields
	 * defined in uc_flags
	 */
	if (uap->ucp == NULL)
		exit(CLD_EXITED, 0);

	as_lookup_current(&vasid);

	switch(get_current_abi()) {
	case ABI_IRIX5:
		return irix5_restorecontext(uap->ucp);

	case ABI_IRIX5_N32:
		return irix5_n32_restorecontext(uap->ucp);

#if _MIPS_SIM == _ABI64
	case ABI_IRIX5_64:
		return irix5_64_restorecontext(uap->ucp);
#endif
	default:
		cmn_err(CE_PANIC, "Bad ABI value in switch, abi 0x%x\n",
			get_current_abi());
	}
	/* NOTREACHED */
}

int
getcontext(struct contexta *uap)
{
	/*
	 * In future releases, when the ucontext structure grows,
	 * getcontext should be modified to only return the fields
	 * specified in the uc_flags.
	 * That way, the structure can grow and still be binary
	 * compatible will all .o's which will only have old fields
	 * defined in uc_flags
	 */

	switch(get_current_abi()) {
	case ABI_IRIX5:
		 {
		 irix5_ucontext_t i5_uc;
		 i5_uc.irix5_extra_ucontext = 0; /* set only for signals */

		 irix5_savecontext(&i5_uc, curuthread->ut_sighold);
		 /* setup good return status for setcontext */
		 i5_uc.uc_mcontext.gregs[CTX_V0] =
			i5_uc.uc_mcontext.gregs[CTX_A3] = 0;
		 if (copyout(&i5_uc, uap->ucp, sizeof(i5_uc)))
			return EFAULT;
		 return 0;
		 }
		
	case ABI_IRIX5_N32:
		{
		irix5_n32_ucontext_t i5_n32_uc;

		irix5_n32_savecontext(&i5_n32_uc, curuthread->ut_sighold);
		 /* setup good return status for setcontext */
		i5_n32_uc.uc_mcontext.gregs[CTX_V0] =
			i5_n32_uc.uc_mcontext.gregs[CTX_A3] = 0;
		if (copyout(&i5_n32_uc, uap->ucp, sizeof(i5_n32_uc)))
			return EFAULT;
		return 0;
		}

#if _MIPS_SIM == _ABI64
	case ABI_IRIX5_64:
		{
		irix5_64_ucontext_t i5_64_uc;

		irix5_64_savecontext(&i5_64_uc, curuthread->ut_sighold);
		 /* setup good return status for setcontext */
		i5_64_uc.uc_mcontext.gregs[CTX_V0] =
			i5_64_uc.uc_mcontext.gregs[CTX_A3] = 0;
		if (copyout(&i5_64_uc, uap->ucp, sizeof(i5_64_uc)))
			return EFAULT;
		return 0;
		}
#endif
	default:
		cmn_err(CE_PANIC, "Bad ABI value in switch, abi 0x%x\n",
			get_current_abi());
	}
	/* NOTREACHED */
}

/*
 * IRIX5 sigcontext
 * User registers are always 64 bit
 */
static void
irix5_savesigcontext(irix5_sigcontext_t *scp, k_sigset_t *mask)
{
	register __uint64_t *srp;
	register k_fpreg_t *frp;
	register k_machreg_t *urp;
	register __uint32_t regbit, regmask;
	register uthread_t *ut = curuthread;
 
	/* all registers including coprocs */
	regmask = scp->sc_regmask = 0xffffffff;	

	/* save context */
	scp->sc_link = (__uint64_t)curuthread->ut_pproxy->prxy_oldcontext;
	/* save signal mask */
	sigktou(mask, &scp->sc_sigset);

	scp->sc_triggersave = 0;	/* Obsolete field */
	scp->sc_pc = USER_REG(EF_EPC);
	scp->sc_regs[0] = 0;
	for (urp = &USER_REG(EF_AT), srp = &scp->sc_regs[1], regbit = 1<<1;
	     urp <= &USER_REG(EF_RA);
	     urp++, srp++, regbit <<= 1) {
		if (regmask & regbit)
			*srp = *urp;
	}
	scp->sc_mdhi = USER_REG(EF_MDHI);
	scp->sc_mdlo = USER_REG(EF_MDLO);
	scp->sc_ownedfp = PCB(pcb_ownedfp);
	if (PCB(pcb_ownedfp)) {
		checkfp(ut, 0);	/* dump fp to pcb */
		for (srp = &scp->sc_fpregs[0], frp = PCB(pcb_fpregs);
		     frp < &PCB(pcb_fpregs)[32]; srp++, frp++)
			*srp = *frp;
		scp->sc_fpc_csr = PCB(pcb_fpc_csr);
	 	scp->sc_fpc_eir = PCB(pcb_fpc_eir);
		scp->sc_fp_rounded_result = PCB(pcb_fp_rounded_result);
		PCB(pcb_fpc_csr) &= ~FPCSR_EXCEPTIONS;
	}
	scp->sc_cause = USER_REG(EF_CAUSE);
	scp->sc_badvaddr = USER_REG(EF_BADVADDR);

	scp->sc_ssflags = ut->ut_pproxy->prxy_ssflags;
}

/*
 * Restore BSD/IRIX sigcontext - all registers 64 bits regardless of mode
 * user program is in.
 */
static int
irix5_restoresigcontext(irix5_sigcontext_t *u_scp)
{
	register __uint64_t *srp;
	register k_fpreg_t *frp;
	register k_machreg_t *urp;
	register __uint32_t regbit, regmask;
	k_sigset_t k_sigset;
	irix5_sigcontext_t i5_sc, *scp = &i5_sc;
	int s;

	if (copyin(u_scp, scp, sizeof(*scp)))
		return EFAULT;

	regmask = scp->sc_regmask;
	USER_REG(EF_EPC) = scp->sc_pc;
	for (urp = &USER_REG(EF_AT), srp = &scp->sc_regs[1], regbit = 1<<1;
	    urp <= &USER_REG(EF_RA);
	    urp++, srp++, regbit <<= 1)
		if (regmask & regbit)
			*urp = *srp;

	USER_REG(EF_MDHI) = scp->sc_mdhi;
	USER_REG(EF_MDLO) = scp->sc_mdlo;
	/*
	 * unless bit one of regmask is set we don't restore coprocessor
	 * state.
	 *
	 * Note here that we restore the fp state based on the
	 * flag 'scp->sc_ownedfp', rather than PCB(pcb_ownedfp).
	 * (changed from PCB(pcb_ownedfp) - bug # 238665)
	 * This is the same algorithm as is used in savecontext/restorecontext.
	 */
	if ((regmask & 1) && scp->sc_ownedfp) {
		checkfp(curuthread, 1);	/* toss current fp contents */
		for (frp = PCB(pcb_fpregs), srp = &scp->sc_fpregs[0];
		    frp < &PCB(pcb_fpregs)[32]; frp++, srp++)
			*frp = *srp;
		PCB(pcb_fpc_csr) = scp->sc_fpc_csr & ~FPCSR_EXCEPTIONS;
	}

	/*
	 * restore signal mask
	 * This version deals with a real sigset_t
	 */
	sigutok(&scp->sc_sigset, &k_sigset);
	s = ut_lock(curuthread);
	*curuthread->ut_sighold = k_sigset;
	sigdiffset(curuthread->ut_sighold, &cantmask);
	ut_unlock(curuthread, s);

	/* restore sigstack state */
	curuthread->ut_pproxy->prxy_ssflags =
				scp->sc_ssflags & (SS_ONSTACK|SS_DISABLE);

	/* restore context chain */
	curuthread->ut_pproxy->prxy_oldcontext =
					(void *)(__psunsigned_t)scp->sc_link;
	return 0;
}

/*
 * sigreturn(sigcontextptr) OR
 * sigreturn(0, ucontextptr)
 * called by libc _sigtramp
 */
struct sigreturna {
	irix5_sigcontext_t	*scp;
	void			*ucp;
	sysarg_t		signo;
};

static int irix5_restoresigcontext(irix5_sigcontext_t *);

int
sigreturn(struct sigreturna *uap)
{
#ifdef CKPT
	extern int ckpt_enabled;

	if (ckpt_enabled && ((uap->signo & 0xffff) == SIGCKPT) &&
	    ((curprocp->p_flag & (SPROPEN|SCKPT)) == (SPROPEN|SCKPT)))
		ut_flagset(curuthread, UT_CKPT);
#endif
	if (uap->scp == NULL) {
		switch (get_current_abi()) {
		case ABI_IRIX5:
			return irix5_restorecontext(uap->ucp);
		case ABI_IRIX5_N32:
			return irix5_n32_restorecontext(uap->ucp);
#if (_MIPS_SIM == _ABI64)
		case ABI_IRIX5_64:
			return irix5_64_restorecontext(uap->ucp);
#endif
		default:
			cmn_err(CE_PANIC, "Bad ABI value in switch, abi 0x%x",
				get_current_abi());
			/* NOTREACHED */
		}
	}

	/* irix5 and irix5_64 sigcontext are the same */
	return irix5_restoresigcontext(uap->scp);
}

/*
 * siginfoktou - convert a k_siginfo_t to a user one
 */
void
irix5_siginfoktou(k_siginfo_t *sip, irix5_siginfo_t *usip)
{
	usip->si_signo = sip->si_signo;
	usip->si_code = sip->si_code;
	usip->si_errno = sip->si_errno;
	if (SI_FROMUSER(sip)) {
		switch(sip->si_code) {
		case SI_USER:
			usip->si_pid = sip->si_pid;
			usip->si_uid = sip->si_uid;
			break;
		case SI_ASYNCIO:
		case SI_QUEUE:
		case SI_MESGQ:
		case SI_TIMER:	
			usip->si_value.sival_ptr =
					(__psint_t)sip->si_value.sival_ptr;
			break;
		}
	} else {
		switch(sip->si_signo) {
		case SIGILL:
		case SIGBUS:
		case SIGSEGV:
		case SIGFPE:
			usip->si_addr =
				(app32_ptr_t)(__psunsigned_t)sip->si_addr;
			break;
		case SIGCHLD:
			usip->si_pid = sip->si_pid;
			usip->si_status = sip->si_status;
			usip->si_utime = sip->si_utime;
			usip->si_stime = sip->si_stime;
			break;
		case SIGXFSZ:
		case SIGPOLL:
			usip->si_fd = sip->si_fd;
			usip->si_band = sip->si_band;
			break;
		}
	}
}

/*
 * svr4 sigsendset wrapper
 */
struct sigsendsysa {
       	procset_t	*psp;
       	sysarg_t	sig;
};

int
sigsendsys(struct sigsendsysa *uap)
{
	procset_t set;
	int error;

	if (uap->sig < 0 || uap->sig >= NSIG)
		return EINVAL;

	if (copyin(uap->psp, &set, sizeof set))
		return EFAULT;

	/*
	 * Check that the procset_t is valid.
	 */
	if (error = checkprocset(&set))
		return error;

	if (set.p_op == POP_AND &&
	    set.p_lidtype == P_PID &&
	    set.p_ridtype == P_ALL) {
		/* signaling a single process */
		return killsingle(set.p_lid == P_MYID ? current_pid()
				  : (pid_t)set.p_lid, uap->sig);
	}

	/* XXX possible optimizations re simple sets here */

	return sigtoprocset(&set, (int)uap->sig);
}

struct killa {
	sysarg_t pid;
	sysarg_t signo;
};

int
kill(struct killa *uap)
{
	pid_t pid = uap->pid;
	int signo = uap->signo;

	if (signo < 0 || signo >= NSIG)
		return EINVAL;

	if (pid > 0) {
		 /* signaling a single process, so do it the fast way */
		return killsingle(pid, signo);
	}

	/* Send sig to all processes, dependent on permissions */
	if (pid == -1) {
		procset_t set;

		setprocset(&set, POP_AND, P_ALL, P_MYID, P_ALL, P_MYID);

		return sigtoprocset(&set, signo);
	}

	/* send sig to process group indicated by uap->pid */
	return killpg(-pid, signo);
}
		

/*
 * killsingle(pid, sig)
 *
 * sends a signal to just the specified process
 */
static int
killsingle(pid_t pid, int sig)
{
	vproc_t *vpr;
	int error;
	k_siginfo_t info;
	vp_get_attr_t attr;

	if ((vpr = VPROC_LOOKUP_STATE(pid, ZYES)) == NULL) {
		_SAT_PROC_ACCESS(SAT_PROC_ATTR_WRITE, pid, NULL, ESRCH, sig);
		return ESRCH;
	}

	bzero(&info, sizeof(info));
	info.si_signo = sig;
	info.si_code = SI_USER;
	info.si_pid = current_pid();
	info.si_uid = get_current_cred()->cr_ruid;

	VPROC_GET_ATTR(curvprocp, VGATTR_SID, &attr);

	VPROC_SENDSIG(vpr, sig, 0, attr.va_sid, 
				get_current_cred(), &info, error);

	VPROC_RELE(vpr);

	_SAT_PROC_ACCESS(SAT_PROC_ATTR_WRITE, pid, NULL, error, sig);

	return error;
}

static int
killpg(pid_t pgid, int sig)
{
	vpgrp_t		*vpg;
	int		error;
	k_siginfo_t	info;
	vp_get_attr_t	attr;
	
	bzero(&info, sizeof(info));
	info.si_signo = sig;
	info.si_code = SI_USER;
	info.si_pid = current_pid();
	info.si_uid = get_current_cred()->cr_ruid;

	VPROC_GET_ATTR(curvprocp, VGATTR_SID|VGATTR_PGID, &attr);

	if (pgid == 0)
		pgid = attr.va_pgid;

	if (vpg = VPGRP_LOOKUP(pgid)) {
		error = VPGRP_SENDSIG(vpg, sig, 0, attr.va_sid,
					get_current_cred(), &info);
		VPGRP_RELE(vpg);
	} else
		error = ESRCH;

	return error;
}


/* support code for sending signals to process sets (sigsendset sys call) */

struct param {
	procset_t *psp;		/* process must belong to this procset */
	int sig;		/* signal to send */
	int nfound;		/* Nbr of processes found. */
	int nperm;		/* Nbr of process we had perms to send sig */
	pid_t sid;		/* Session id of originating process */
	cred_t *cred;		/* Credentials of originating process */
	k_siginfo_t *info;	/* Siginfo_t for interested signals */
};

/*
 * Attempt to deliver 'sig' to each process indicated by the procset_t.
 * The guts of the sigsendset system call.
 */

static int
sigtoprocset(
	procset_t *psp,
	int sig)
{
	int error;
	struct param param;
	k_siginfo_t info;
	vp_get_attr_t attr;

	/*
	 * Check for the special value P_MYID in either operand
	 * and replace it with the correct value.  We don't check
	 * for an error return from getmyid() because the idtypes
	 * have been validated by the checkprocset() call above.
	 */

	VPROC_GET_ATTR(curvprocp, VGATTR_PPID|VGATTR_PGID|VGATTR_SID, &attr);

	if (psp->p_lid == P_MYID)
		psp->p_lid = getmyid(psp->p_lidtype, &attr);

	if (psp->p_rid == P_MYID)
		psp->p_rid = getmyid(psp->p_ridtype, &attr);

	param.nfound = 0;
	param.nperm = 0;
	param.psp = psp;
	param.sig = sig;
	param.sid = attr.va_sid;
	param.cred = get_current_cred();

	bzero(&info, sizeof(info));
	info.si_signo = sig;
	info.si_code = SI_USER;
	info.si_pid = current_pid();
	info.si_uid = get_current_cred()->cr_ruid;
	param.info = &info;

	error = procscan(sigtoprocset_scanfunc, &param);
	
	if (param.nfound == 0)
		return ESRCH;

	if (param.nperm == 0)
		return EPERM;

	return error;
}

/*
 * sigtoproc_scanfunc
 * return 0 for additional scanning
 */

static int
sigtoprocset_scanfunc(proc_t *p, void *arg, int mode)
{
	int error;
	struct param *par;
	vproc_t *vpr;

	if (mode != 1)
		return 0;

	vpr = PROC_TO_VPROC(p);

	if (vpr->vp_pid == INIT_PID)
		return 0;

	par = (struct param *)arg;

	if (VPROC_HOLD_STATE(vpr, ZYES))
		return 0;

	if (!procinset(vpr, par->psp)) {
		VPROC_RELE(vpr);
		return 0;
	}

	par->nfound++;

	VPROC_SENDSIG(vpr, par->sig, 0, par->sid,
				par->cred, par->info, error);

	if (!error)
		par->nperm++;

	VPROC_RELE(vpr);
	return 0;
}

#if _MIPS_SIM == _ABI64
/*
 * irix5_64_restorecontext - called via sigreturn or setcontext(2)
 */
int
irix5_64_restorecontext(irix5_64_ucontext_t *u_ucp)
{
	register k_fpreg_t *frp;
	register k_machreg_t *urp;
	register irix5_64_greg_t *surp;
	register __uint64_t *sfrp;
	proc_proxy_t	*prxy = curuthread->ut_pproxy;
	as_setattr_t setattr;
	vasid_t vasid;
	irix5_64_ucontext_t i5_64_uc, *ucp = &i5_64_uc;

	as_lookup_current(&vasid);

	if (copyin(u_ucp, ucp, sizeof(*ucp)))
		return EFAULT;

	if (ucp->uc_flags & UC_STACK) {
		if (ucp->uc_stack.ss_flags & SS_ONSTACK) {
			prxy->prxy_sigsp = (void *)ucp->uc_stack.ss_sp;
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
		for (urp = &USER_REG(EF_AT), surp = &ucp->uc_mcontext.gregs[1];
		     urp <= &USER_REG(EF_RA); urp++, surp++)
			*urp = *surp;
		USER_REG(EF_MDLO) = *surp++;
		USER_REG(EF_MDHI) = *surp++;
		surp++; 		/* skip EF_CAUSE */
		USER_REG(EF_EPC) = *surp;

		/* skip EF_SR */
		ASSERT(surp == &ucp->uc_mcontext.gregs[IRIX5_64_NGREG-2]);
	}

	if (ucp->uc_flags & UC_MAU) {
		checkfp(curuthread, 1);	/* toss current fp contents */
		for (sfrp = &ucp->uc_mcontext.fpregs.fp_r.fp_regs[0],
		     frp = PCB(pcb_fpregs);
		     frp < &PCB(pcb_fpregs)[32]; frp++, sfrp++)
			*frp = (k_fpreg_t)*sfrp;
		PCB(pcb_fpc_csr) =
		    ucp->uc_mcontext.fpregs.fp_csr & ~FPCSR_EXCEPTIONS;
	}

	if (ucp->uc_flags & UC_SIGMASK) {
		register int s;

		s = ut_lock(curuthread);
		sigutok(&ucp->uc_sigmask, curuthread->ut_sighold);
		sigdiffset(curuthread->ut_sighold, &cantmask);
		ut_unlock(curuthread, s);
	}

	prxy->prxy_oldcontext = (void *)ucp->uc_link;
	return 0;
}

/*
 * irix5_64_savecontext - 64 bit version called when sending a
 * signal or getcontext(2)
 */
void
irix5_64_savecontext(irix5_64_ucontext_t *ucp, k_sigset_t *mask)
{
	proc_proxy_t *prxy = curuthread->ut_pproxy;
	register irix5_64_greg_t *surp;
	register __uint64_t *sfrp;
	register k_machreg_t *urp;
	register k_fpreg_t *frp;
	as_getasattr_t asattr;
	vasid_t vasid;

	as_lookup_current(&vasid);

	ucp->uc_flags = UC_ALL;
	ucp->uc_link = (app64_ptr_t)prxy->prxy_oldcontext;

	/* save signal mask */
	sigktou(mask, &ucp->uc_sigmask);

	if (!(prxy->prxy_ssflags & SS_ONSTACK)) {
		/* lowest stack address */
		VAS_LOCK(vasid, AS_SHARED);
		VAS_GETASATTR(vasid, AS_STKBASE|AS_STKSIZE, &asattr);
		VAS_UNLOCK(vasid);
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
	surp = &ucp->uc_mcontext.gregs[0];
	*surp++ = 0;
	for (urp = &USER_REG(EF_AT); urp <= &USER_REG(EF_RA); urp++)
		*surp++ = *urp;
	*surp++ = USER_REG(EF_MDLO);
	*surp++ = USER_REG(EF_MDHI);
	*surp++ = USER_REG(EF_CAUSE);
	*surp++ = USER_REG(EF_EPC);

	/*
	 * give user the SR so can tell what mode - 32/64 address, endian,
	 * 16/32 fp regs
	 */
	*surp = USER_REG(EF_SR);
	ASSERT(surp == &ucp->uc_mcontext.gregs[IRIX5_64_NGREG-1]);

	if (!PCB(pcb_ownedfp)) {
		ucp->uc_flags &= ~UC_MAU;	/* don't save fpregs */
	} else {
		checkfp(curuthread, 0);	/* dump fp to pcb */
		for (sfrp = &ucp->uc_mcontext.fpregs.fp_r.fp_regs[0],
			frp = PCB(pcb_fpregs);
		    frp < &PCB(pcb_fpregs)[32]; sfrp++, frp++)
			*sfrp = (k_fpreg_t)*frp;
		ucp->uc_mcontext.fpregs.fp_csr = PCB(pcb_fpc_csr);
		/* don't save fp_eir since can't restore it anyway */
		PCB(pcb_fpc_csr) &= ~FPCSR_EXCEPTIONS;
	}
}

/*
 * siginfoktou - convert a k_siginfo_t to a user one
 */
void
irix5_64_siginfoktou(k_siginfo_t *sip, irix5_64_siginfo_t *usip)
{
	usip->si_signo = sip->si_signo;
	usip->si_code = sip->si_code;
	usip->si_errno = sip->si_errno;
	if (SI_FROMUSER(sip)) {
		switch(sip->si_code) {
		case SI_USER:
			usip->si_pid = sip->si_pid;
			usip->si_uid = sip->si_uid;
			break;
		case SI_ASYNCIO:	
		case SI_QUEUE:
		case SI_MESGQ:
		case SI_TIMER:
			usip->si_value.sival_ptr =
					(__psint_t)sip->si_value.sival_ptr;
			break;
		}
	} else {
		switch(sip->si_signo) {
		case SIGILL:
		case SIGBUS:
		case SIGSEGV:
		case SIGFPE:
			usip->si_addr = (app64_ptr_t)sip->si_addr;
			break;
		case SIGCHLD:
			usip->si_pid = sip->si_pid;
			usip->si_status = sip->si_status;
			usip->si_utime = sip->si_utime;
			usip->si_stime = sip->si_stime;
			break;
		case SIGXFSZ:
		case SIGPOLL:
			usip->si_fd = sip->si_fd;
			usip->si_band = sip->si_band;
			break;
		}
	}
}
#endif	/* _ABI64 */
#ifdef CKPT
int
#if _MIPS_SIM == _ABI64
irix5_64_siginfoutok(irix5_64_siginfo_t *usip, k_siginfo_t *sip)
#else
irix5_siginfoutok(irix5_siginfo_t *usip, k_siginfo_t *sip)
#endif
{
	if ((usip->si_signo <= 0)||(usip->si_signo > NSIG))
		return EINVAL;

	if (usip->si_errno < 0)
		return EINVAL;

	sip->si_signo = usip->si_signo;
	sip->si_code = usip->si_code;
	sip->si_errno = usip->si_errno;

	if (SI_FROMUSER(sip)) {
		switch(sip->si_code) {
		case SI_USER:
			sip->si_pid = usip->si_pid;
			sip->si_uid = usip->si_uid;
			break;
		case SI_ASYNCIO:
		case SI_QUEUE:
		case SI_MESGQ:
		case SI_TIMER:
			sip->si_value.sival_ptr =
				(void *)usip->si_value.sival_ptr;
			break;
		default:
			return EINVAL;
		}
	} else {
		switch (sip->si_signo) {
		case SIGILL:
		case SIGBUS:
		case SIGSEGV:
		case SIGFPE:
			sip->si_addr = (void *)usip->si_addr;
			break;
		case SIGCHLD:
			sip->si_pid = usip->si_pid;
			sip->si_status = usip->si_status;
			sip->si_utime = usip->si_utime;
			sip->si_stime = usip->si_stime;
			break;
		case SIGXFSZ:
		case SIGPOLL:
			sip->si_fd = usip->si_fd;
			sip->si_band = usip->si_band;
			break;
		}
	}
	return 0;
}
#endif /* CKPT */
