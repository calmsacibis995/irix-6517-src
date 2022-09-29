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
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#include <bstring.h>
#include <string.h>
#include <sys/types.h>
#include <sys/acct.h>
#include <ksys/vpag.h>
#include <sys/atomic_ops.h>
#include <ksys/as.h>
#include <sys/cmn_err.h>
#include <ksys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <ksys/vfile.h>
#include <sys/immu.h>
#include <sys/kthread.h>
#include <sys/kmem.h>
#include <sys/numa.h>
#include <sys/par.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/pfdat.h>
#include <os/numa/pm.h>
#include <sys/prctl.h>
#include <ksys/vpgrp.h>
#include <sys/proc.h>
#include "os/proc/pproc_private.h"
#include "os/proc/pproc.h"
#include "os/as/as_private.h"
#include <sys/profil.h>
#include <sys/resource.h>
#include <sys/rtmon.h>
#include <sys/runq.h>
#include <sys/sbd.h>
#include <sys/sched.h>
#include <sys/schedctl.h>
#include <sys/sema.h>
#include <ksys/vsession.h>
#include <sys/sysinfo.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/space.h>
#include <sys/time.h>
#include <sys/var.h>
#include <sys/vnode.h>
#include <ksys/pid.h>
#include <ksys/fdt.h>
#include <ksys/exception.h>
#include <ksys/childlist.h>
#include <sys/sat.h>
#include <sys/capability.h>
#include <procfs/prsystm.h>	/* for pr_tracemasks */
#include <sys/ksignal.h>
#ifdef _SHAREII
#include	<sys/shareIIstubs.h>
#endif /* _SHAREII */
#include <sys/hwperftypes.h>
#include <sys/hwperfmacros.h>
#include <sys/unc.h>
#ifdef CKPT
#include <sys/ckpt.h>
#endif

timespec_t	forktime;

int tracefork;

extern void	rrmResume(uthread_t *, int);
extern void	gfx_shdup(uthread_t *, uthread_t *);

extern int maxup;

static int dofork(uint, rval_t *, caddr_t *, size_t, pid_t);
static int procfork(uint, pid_t *, caddr_t *, size_t);

/*
 * Create a new process -- the internal version of sys fork.
 *
 * It returns 1 in the new process, 0 in the old.
 */
int
newproc(void)
{
	pid_t pid = 0;

	if (procfork(0, &pid, NULL, 0))
		cmn_err_tag(142,CE_PANIC, "newproc - fork failed");

	/* return 0 for parent, 1 for child */
	return pid == 0;
}

#ifdef CKPT
/*
 * pidfork system call.
 */
struct forka {
	sysarg_t pid;
};

/*
 * fork with a predefined pid.
 * If pid == 0, pick a pid at random.
 */
int
pidfork(struct forka *uap, rval_t *rvp)
{
	/* Don't allow requests of pid 0 */
	if (uap->pid <= 0 || uap->pid >= MAXPID)
		return ESRCH;
	return dofork(0, rvp, NULL, 0, uap->pid);
}
#endif /* CKPT */

/*
 * fork system call.
 */
/* ARGSUSED */
int
fork(void *uap, rval_t *rvp)
{
	return dofork(0, rvp, NULL, 0, (pid_t)0);
}

/*
 * sproc system call
 */
struct sproca {
	void (*entry)();
	sysarg_t shmask;
	sysarg_t arg;
};

struct sprocb {
	void (*entry)();
	sysarg_t shmask;
	sysarg_t arg;
	caddr_t stkptr;
	usysarg_t stklen;
};

struct nsproc_args {
	void (*entry)();
	sysarg_t shmask;
	sysarg_t tinfo;
	sysarg_t sinfo;
};

#ifdef CKPT
struct sprocpid
{
	void (*entry)();
	sysarg_t shmask;
	sysarg_t arg;
	caddr_t stkptr;
	usysarg_t stklen;
	sysarg_t pid;
};

struct nsproctid_args {
	void (*entry)();
	sysarg_t shmask;
	sysarg_t tinfo;
	sysarg_t sinfo;
	sysarg_t tid;
};

#define SPROC_COMMON	struct sprocpid
#else
#define SPROC_COMMON	struct sprocb
#endif

extern int	sprocsp(struct sprocb *, rval_t *);
extern int	nsproc(struct nsproc_args *, rval_t *);
static int	sproc_common(SPROC_COMMON *, rval_t *);
static void	newthread_eframe(SPROC_COMMON *);
static int	thread_save(uthread_t *, uthread_t *);

#if _MIPS_SIM == _ABI64
static int irix5_to_prt(enum xlate_mode, void *, int , xlate_info_t *);
#endif

#ifdef CKPT
static int nsproc_common(struct nsproc_args *uap, rval_t *rvp, int tid);

int
nsproctid(struct nsproctid_args *uap, rval_t *rvp)
{
	struct nsproc_args arg;

	arg.entry = uap->entry;
	arg.shmask = uap->shmask;
	arg.tinfo = uap->tinfo;
	arg.sinfo = uap->sinfo;

	return (nsproc_common(&arg, rvp, uap->tid));
}

int
nsproc(struct nsproc_args *uap, rval_t *rvp)
{
	return (nsproc_common(uap, rvp, UT_ID_NULL));
}

static int
nsproc_common(struct nsproc_args *uap, rval_t *rvp, int tid)
#else /* CKPT */
int
nsproc(struct nsproc_args *uap, rval_t *rvp)
#endif
{
#if _MIPS_SIM == _ABI64
	int		abi = get_current_abi();
#endif
	prthread_t	prt;
	prsched_t	*prs;
	prsched_t	prs_args;
	uthread_t	*put = curuthread;
	uthread_t	*ut;
	proc_proxy_t	*prxy = put->ut_pproxy;
	int		error, s;
	job_t		*job;

	/*
	 * Try to keep the uninformed out.  Caller must have already
	 * requested multi-threading.  The must-have PR_THREADS is just
	 * to make it harder to stumble through here by accident.
	 */
	if (!(uap->shmask & PR_THREADS))
		return EINVAL;

	if (COPYIN_XLATE((void *)uap->tinfo, &prt, sizeof(prthread_t),
			 irix5_to_prt, abi, 1)) {
		return (EFAULT);
	}

	if (uap->sinfo) {

		/* Reject batch users who want specific scheduling.
		 */
		if (!_CAP_CRABLE(get_current_cred(), CAP_SCHED_MGT))
			return EPERM;

		if (copyin((void *)uap->sinfo, &prs_args, sizeof(prsched_t)))
			return EFAULT;

		/* Reject TS request - there should only be one job per process.
		 */
		if (prs_args.prs_policy == SCHED_TS)
			return EINVAL;
			
		prs = &prs_args;
	} else {
		prs = 0;
	}
	prt.prt_entry = uap->entry;
	prt.prt_flags = uap->shmask;

	if (!(put->ut_flags & UT_PTHREAD))
		return EPERM;

	if (error = uthread_dup(put, &ut, UT_TO_PROC(curuthread),
			PR_THREADS | (PR_SALL & ~PR_SPROC),
#ifdef CKPT
			tid))
#else
			UT_ID_NULL))
#endif
		return error;

#ifdef _SHAREII
	/*
	 * ShareII must be notified of new thread.
	 * SHR_THREADNEW assumes ut->ut_proc is set correctly.
	 * swine -- uthread_sched_setscheduler calls shareII, but too late
	 * as already on runq!
	 */
	SHR_THREADNEW(put, ut);
#endif
	/*
	 * nsproc'd uthreads do not inherit caller's 'mustrun' state,
	 * but do inherit the cpuset
	 */
	job_allocate();
	uscan_update(prxy);
	job = prxy->prxy_sched.prs_job;
	ASSERT(!KT_ISPS(UT_TO_KT(ut)));
	uthreadrunq(UT_TO_KT(ut), UT_TO_KT(put)->k_cpuset, &prxy->prxy_sched);
	uthread_attachrunq(UT_TO_KT(ut), PDA_RUNANYWHERE, job);

	/* If we just added a default job then cache it for future use.
	 */
	if (!job) {
		ASSERT(ut->ut_job);
		job_cache(ut->ut_job);
		prxy->prxy_sched.prs_job = ut->ut_job;
	}

	/* If no scheduling attributes specified assume process scope and
	 * inherit from the process.
	 * Have the new uthread inherit the UT_FIXADE flag from the
	 * parent.  This is required for the Ada compiler.
	 */
	if (!prs) {
		int s = ut_lock(ut);
		ut->ut_flags |= (put->ut_flags & UT_FIXADE) | UT_PTPSCOPE;
		ut_unlock(ut, s);

		prs_args.prs_policy = prxy->prxy_sched.prs_policy;
		prs_args.prs_priority = (prs_args.prs_policy == SCHED_TS)
			? prxy->prxy_sched.prs_nice
			: prxy->prxy_sched.prs_priority;
	} else {
		if (put->ut_flags & UT_FIXADE)
			ut_flagset(ut, UT_FIXADE);
	}
	ASSERT(UT_TO_KT(ut)->k_onrq == CPU_NONE);
	ASSERT(UT_TO_KT(ut)->k_sonproc == CPU_NONE);

	s = ut_lock(put);
	if (is_batch(UT_TO_KT(put)) || is_bcritical(UT_TO_KT(put))){
		UT_TO_KT(ut)->k_flags &= ~(KT_PS|KT_BASEPS|KT_BIND);
		UT_TO_KT(ut)->k_pri = UT_TO_KT(ut)->k_basepri = 
						UT_TO_KT(put)->k_basepri;
	}
	ut_unlock(put, s);	
		
	if (!is_batch(UT_TO_KT(put)) && !is_bcritical(UT_TO_KT(put)) && 
		(error = uthread_sched_setscheduler(ut, &prs_args))) {
		uscan_unlock(prxy);
		ASSERT(prxy->prxy_nlive > 1);
		atomicAddInt(&prxy->prxy_nlive, -1);
		uthread_exit(ut, 0);
		return error;
	}
	uscan_unlock(prxy);

	/*
	 * thread_save returns 1 to child, 0 to parent.
	 */
	if (thread_save(put, ut)) {
		SPROC_COMMON	sb;

		ASSERT(curuthread == ut);
		ASSERT(put->ut_prda);

		if (error = lockprda(&ut->ut_prda)) {
			vproc_t *vpr;

			/*
			 * Let initiating thread know of failure.
			 */
			s = prxy_lock(prxy);
			put->ut_prda->t_sys.t_syserror = error;
			sv_signal(&UT_TO_KT(put)->k_timewait);
			prxy_unlock(prxy, s);

			ASSERT(prxy->prxy_nlive > 1);
			atomicAddInt(&prxy->prxy_nlive, -1);
			vpr = UT_TO_VPROC(ut);
			VPROC_HOLD(vpr);
			uthread_exit(ut, 0);	/* bye */
			ASSERT(0);
		}

		/*
		 * Set up uthread-private signal mask before parent returns.
		 * Up to this point, ut_sighold has pointed
		 * to a k_sigset_t in the pshare structure
		 * which has _all_ signals blocked.
		 */
		uthread_setsigmask(ut, put->ut_sighold);

		/* If this the special event thread mark the proxy.
		 */
		if (prt.prt_flags & PR_EVENT) {
			prxy->prxy_sigthread = ut;
		}

		/*
		 * Let initiating thread know that final hurdle has
		 * been passed.
		 */
		s = prxy_lock(prxy);
		put->ut_prda->t_sys.t_syserror = ENOINTRGROUP;
		sv_signal(&UT_TO_KT(put)->k_timewait);
		prxy_unlock(prxy, s);

		/* Set up for newthread_eframe.
		 * No refs to parent data so this can be done after wake up.
		 */
		sb.entry = prt.prt_entry;
		sb.arg = (sysarg_t)prt.prt_arg;
		sb.stkptr = prt.prt_stkptr;
		newthread_eframe(&sb);
		rvp->r_val1 = 0;
		rvp->r_val2 = 1;
	} else {
		/* Init utas */
		ASSERT(ut->ut_as.utas_tlbid);
		ASSERT(ut->ut_as.utas_ut == ut);
		set_tlbpids(&ut->ut_as, (unsigned char)-1);
		setup_wired_tlb_notme(&ut->ut_as, 1);
		ut->ut_as.utas_utlbmissswtch = put->ut_as.utas_utlbmissswtch;
		ut->ut_as.utas_flag = put->ut_as.utas_flag & (UTAS_TLBMISS);
		ASSERT((ut->ut_as.utas_utlbmissswtch == 0) ||
					(put->ut_as.utas_flag & UTAS_TLBMISS));

		ASSERT(put->ut_prda);
		put->ut_prda->t_sys.t_syserror = 0;
		rvp->r_val1 = ut->ut_id;
		s = kt_lock(UT_TO_KT(ut));
		/*
		 * Need to initialize ktimer to some valid non-active state.
		 * If we use an active state, then we end up double charging
		 * CPU and, worse yet, if we end up going to sleep or getting
		 * preempted before putrunq() puts us into AS_RUNQ_WAIT or
		 * it tosses us onto a CPU's next thread, then we could see
		 * absurdly high (and incorrect) CPU usage.  We could use a
		 * special state, say AS_NONE, but then ktimer_switch() would
		 * have to have special code to check for this and we don't
		 * want to slow that down for this single case of starting up
		 * a new thread.  Finally, we could have a new non-active
		 * state for just this situation, say AS_NASCENT_WAIT, but
		 * that would add a timer accumulator to the timer array for
		 * very little purpose.  So, as a compromise, we use
		 * AS_RUNQ_WAIT here.
		 */
		ktimer_init(UT_TO_KT(ut), AS_RUNQ_WAIT);
		putrunq(UT_TO_KT(ut), CPU_NONE);
		kt_unlock(UT_TO_KT(ut), s);

		/*
		 * We can't lockprda for the child, 'cause deep in the
		 * bowels of the address space code there's a call to
		 * lockpages, which calls pfault(!), which assumes that
		 * the page to be lock belongs to curuthread.  So, child
		 * will have to dump an error message in parent's prda (sigh).
		 * N.B.  We use the proxy lock here instead of the
		 * more-intuitive ut_lock(put), because the child (above)
		 * can't call sv_signal while holding put's ut_lock.
		 */
		s = prxy_lock(prxy);
		while (!(error = put->ut_prda->t_sys.t_syserror)) {
			prxy_wait(prxy, &UT_TO_KT(put)->k_timewait, s);
			s = prxy_lock(prxy);
		}
		prxy_unlock(prxy, s);

		if (error != ENOINTRGROUP)
			return error;
		rvp->r_val2 = 0;
	}

	return 0;
}

#if _MIPS_SIM == _ABI64
struct irix5_prt {
	app32_ptr_t	prt_entry;
	app32_ptr_t	prt_arg;
	__uint32_t	prt_flags;
	app32_ptr_t	prt_stkptr;
	__uint32_t	prt_stklen;
};

/* ARGSUSED */
static int
irix5_to_prt(
	enum xlate_mode mode,
	void *to,
	int count,
	xlate_info_t *info)
{
	COPYIN_XLATE_PROLOGUE(irix5_prt, prthread);

	target->prt_entry =
		(void (*)(void *, size_t))(__psint_t)source->prt_entry;
	target->prt_arg = (caddr_t)(__psint_t)source->prt_arg;
	target->prt_flags = source->prt_flags;
	target->prt_stkptr = (caddr_t)(__psint_t)source->prt_stkptr;
	target->prt_stklen = source->prt_stklen;

	return (0);
}
#endif


#ifdef CKPT
int
pidsprocsp(struct sprocpid *uap, rval_t *rvp)
{
	/*
	 * Keep old attribute bits and flags.
	 * Don't allow requests of pid 0.
	 */
	if (uap->pid <= 0 || uap->pid >= MAXPID)
		return ESRCH;

	return sproc_common(uap, rvp);
}
#endif

int
sproc(struct sproca *uap, rval_t *rvp)
{
	SPROC_COMMON	sb;

	sb.entry = uap->entry;
	sb.shmask = uap->shmask & (PR_BLOCK | PR_NOLIBC | PR_SALL);
	sb.arg = uap->arg;
	sb.stkptr = NULL;
	sb.stklen = (usysarg_t)AS_USE_RLIMIT;

#ifdef CKPT
	sb.pid = 0;
#endif
	return sproc_common(&sb, rvp);
}

int
sprocsp(struct sprocb *uap, rval_t *rvp)
{
	SPROC_COMMON	sb;

	/*
	 * Keep only 'old' attribute bits and flags.
	 */
	sb.shmask = uap->shmask & (PR_BLOCK | PR_NOLIBC | PR_SALL);
	sb.entry = uap->entry;
	sb.arg = uap->arg;
	sb.stkptr = uap->stkptr;
	sb.stklen = uap->stklen;

#ifdef CKPT
	sb.pid = 0;
#endif
	return sproc_common(&sb, rvp);
}

/*
 * Common code for the pidsprocsp, sproc and sprocsp system calls.
 */
static int
sproc_common(SPROC_COMMON *uap, rval_t *rvp)
{
	uthread_t *ut = curuthread;
	int error;
	uint shmask;
	SPROC_COMMON sb;

	shmask = uap->shmask;

	/*
	 * make sure stklen gives us enough room to manuver
	 * If user provides own stack then we don't care about length
	 */
	if (uap->stkptr == NULL &&
	    uap->stklen != (usysarg_t)AS_USE_RLIMIT &&
	    uap->stklen < 8192)
		return EINVAL;

	if (IS_THREADED(ut->ut_pproxy))
		return EPERM;

	if (!IS_SPROC(ut->ut_pproxy) && !allocshaddr(UT_TO_PROC(ut)))
		return EAGAIN;

	/*
	 * Copy args now because uap points into parent uthread
	 */
	sb.entry = uap->entry;
	sb.arg = uap->arg;
	sb.stkptr = uap->stkptr;
	/*
	 * share masks are strictly inherited - a child cannot
	 * share more than its parent shares
	 */
#ifdef CKPT
	error = dofork((shmask & ut->ut_pproxy->prxy_shmask) |
			PR_SPROC | (shmask & PR_FLAGMASK), rvp,
			&sb.stkptr, uap->stklen, uap->pid);
#else
	error = dofork((shmask & ut->ut_pproxy->prxy_shmask) |
			PR_SPROC | (shmask & PR_FLAGMASK), rvp,
			&sb.stkptr, uap->stklen, (pid_t)0);
#endif
	if (error) {
		/*
		 * allocshaddr may have done a setup_wired_tlb and is
		 * counting on new_tlbpid() being called "later".  If
		 * we get an error from dofork, this might not have happened.
		 * Do it now, to be safe.
		 * XXX	What setup_wired_tlb call?  Is this obsolete?
		 */
		new_tlbpid(&ut->ut_as, VM_TLBINVAL);

		return error;
	}

	if (rvp->r_val1 == 0) {
		/*
		 * New process thread -- set up eframe.
		 */
		
		newthread_eframe(&sb);
	} else {
		/*
		 * We are the old process.
		 * See if we wanted to block on sproc.
		 */
		if (shmask & PR_BLOCK) {
			int s;

			s = ut_lock(ut);
			blockset(ut, -1, 1, s);	/* unlocks ut */
		}
	}

	return 0;
}


/*
 * Set up exception frame for new thread.
 */
static void
newthread_eframe(SPROC_COMMON *uap)
{
	register k_machreg_t *p_int;
	exception_t *up = curexceptionp;

	/*
	 * We want to return to 'entry' using our stack.
	 * We will clean out tmp registers, but leave the s
	 * registers intact, especially s0 since it is used
	 * for the real entry point.
	 */

#if (_MIPS_SIM == _ABIO32)
	for (p_int = &USER_REG(EF_AT); p_int < &USER_REG(EF_T7);)
		*p_int++ = 0;
#endif
#if (_MIPS_SIM == _ABI64 || _MIPS_SIM == _ABIN32)
	for (p_int = &USER_REG(EF_AT); p_int < &USER_REG(EF_T3);)
		*p_int++ = 0;
#endif
	for (p_int = &USER_REG(EF_T8); p_int < &USER_REG(EF_GP);)
		*p_int++ = 0;

	/* Don't zero ef_fp here - treat it as just another
	 * s register, and preserve its value across the sproc call.
	 *
	   up->u_eframe.ef_fp = 0;
	 */
	up->u_eframe.ef_ra = 0;
	up->u_eframe.ef_mdlo = 0;
	up->u_eframe.ef_mdhi = 0;
	up->u_eframe.ef_epc = (k_machreg_t) uap->entry;
	up->u_eframe.ef_a0 =  (k_machreg_t) uap->arg;

	/*
	 * Since stack has been grown, we know that
	 * regva is BELOW where our stack should be.
	 * New processes stack looks like:
	 *
	 *     top -----------------
	 *         |               |
	 *         |    ARG_SAVE   | (sizeof(eframe_t) bytes)
	 *         |               |
	 *         |    stack      |
	 *         |       |       |
	 *         |       V       |
	 *         |---------------|
	 */

	/*
	 * Set up stack, making sure it's quad-word aligned.
	 * Add a little register spill area -- are eight enough?
	 */
	up->u_eframe.ef_sp =
		((k_machreg_t) uap->stkptr - 8 * sizeof(k_machreg_t)) & ~0xF;

	/*
	 * Keep the old csr, which contains rounding modes,
	 * enables, etc.
	 */
	bzero(PCB(pcb_fpregs), sizeof(PCB(pcb_fpregs)));
	PCB(pcb_fpc_eir) = 0;
	PCB(pcb_ownedfp) = 0;
	PCB(pcb_ssi.ssi_cnt) = 0;
}

static int
dofork(uint shmask, rval_t *rvp, uvaddr_t *stkptr, size_t stklen, pid_t pid)
{
	int error;

	SYSINFO.sysfork++;
	error = procfork(shmask, &pid, stkptr, stklen);
	rvp->r_val1 = pid;
	if (error) {
		_SAT_FORK(pid, error);
		return error;
	}

	if (rvp->r_val1 == 0) {		/* child -- successful procfork */
		rvp->r_val2 = 1;	/* tell libc fork we're the kid */
		return 0;
	}

	rvp->r_val2 = 0;
	_SAT_FORK(rvp->r_val1, 0);
	return 0;
}

static int procdup(proc_t *, uthread_t *, uint, uvaddr_t *, size_t);
void setustk(uthread_t *);

static int
procfork(uint shmask, pid_t *pidp, uvaddr_t *stkptr, size_t stklen)
{
	proc_t *cp, *pp;
	uthread_t *cuth, *puth;
	int error;

	/*
	 * Get a new entry to use.
	 */
	if (error = procnew(&cp, pidp, shmask))
		return error;

	/*
	 * Take a snapshot of the process creation time for /proc --
	 * used to set va_mtime for /proc directory.
	 */
	nanotime(&forktime);

	puth = curuthread;
        pp = UT_TO_PROC(puth);

	/* XXX change cp to cuth's proxy? */
	if (error = uthread_dup(puth, &cuth, cp, shmask, UT_ID_NULL))
		goto err1;

	/*
	 * Set up scheduler info to match caller's.
	 * Must be paired with uthread_dup because there's implied
	 * state that must be torn down if there's a subsequent
	 * fork error / uthread_exit.
	 */
	cp->p_proxy.prxy_shmask = shmask;
	forkrunq(puth, cuth);

	cuth->ut_pproxy->prxy_abi = puth->ut_pproxy->prxy_abi;
	cuth->ut_pproxy->prxy_syscall = puth->ut_pproxy->prxy_syscall;

	ASSERT(cp->p_ptimers == NULL);
	/* cp->p_ptimers = NULL; */
	cp->p_ticks = lbolt;

	{
		/* XXX push sigvec into proxy */
		sigvec_t *psv = &cp->p_sigvec;
		sigvec_t *ppsv = &pp->p_sigvec;
		mrlock_t csvmr;

		bcopy(&psv->sv_lock, &csvmr, sizeof(mrlock_t)); /* ugly:step 1*/
		sigvec_lock(ppsv);
#ifdef boringly_slow
		/*
		 * Copy parent's signal vector information to the child.
		 */
		for (s = 0; s < NUMSIGS; s++) {
			sigorset(&psv->sv_hndlr[s], &ppsv->sv_hndlr[s]);
			sigorset(&psv->sv_sigmasks[s], &ppsv->sv_sigmasks[s]);
		}

		sigorset(&psv->sv_sigign, &ppsv->sv_sigign);
		sigorset(&psv->sv_sigcatch, &ppsv->sv_sigcatch);
		sigorset(&psv->sv_sigrestart, &ppsv->sv_sigrestart);
		sigorset(&psv->sv_signodefer, &ppsv->sv_signodefer);
		sigorset(&psv->sv_sigresethand, &ppsv->sv_sigresethand);
		sigorset(&psv->sv_sainfo, &ppsv->sv_sainfo);
		sigorset(&psv->sv_sighold, &ppsv->sv_sighold);
#else
		bcopy(ppsv, psv, sizeof(sigvec_t));
		bcopy(&csvmr, &psv->sv_lock, sizeof(mrlock_t)); /* ugly:step 2*/
		sigemptyset(&psv->sv_sigpend.s_sig);
		psv->sv_sigpend.s_sigqueue = (sigqueue_t *)NULL;
#endif
		/*
		 * Inherit SNOCLDSTOP - that's POSIX
		 * Inherit SNOWAIT for the sake of old sysV jobs.
		 */
		psv->sv_flags = (ppsv->sv_flags & (SNOCLDSTOP|SNOWAIT));
		sigvec_unlock(ppsv);

		/* mrlock_init(&psv->sv_lock, MRLOCK_DEFAULT, "sigvec", cp->p_pid); */
	}
		 
#ifdef NEVER
	/*
	 *  Ditto for p_realtimer.it_interval, but then may get dirtied by
	 *  exit() again because that field is shared with ei.{xstat,ru_info}.
	 *  But proc structs are now zalloc'd, so this is no longer necessary.
	 */
	ASSERT(!timerisset(&cp->p_realtimer.it_value));
	timerclear(&cp->p_realtimer.it_interval);  /* shared fields */
#endif
	ASSERT(!timerisset(&cp->p_realtimer.it_interval));

	/*
	 * Don't need to hold locks here -- this is an anonymous process.
	 */
	cp->p_flag = pp->p_flag &
		(SPROFFAST|SPROF32|SPROF|SNOCTTY|SCOREPID);

	cp->p_proxy.prxy_flags = pp->p_proxy.prxy_flags & PRXY_USERVME;

#ifdef DEBUG
	if (tracefork < 0 || (tracefork > 0 && tracefork == pp->p_pid))
		printf("\\pid %d (proc 0x%x) forked pid %d (proc 0x%x)\n",
		       pp->p_pid, pp, cp->p_pid, cp);
#endif

	prfork(puth, cuth);

#ifdef CKPT
	ckpt_fork(pp, cp);
#endif
	cp->p_proxy.prxy_fp.pfp_fp = pp->p_proxy.prxy_fp.pfp_fp;
	cp->p_proxy.prxy_fp.pfp_fpflags = pp->p_proxy.prxy_fp.pfp_fpflags;

	/*
	 * Inherit parent's exitsig if it is an sproc.
	 */
	if (shmask) {
		cp->p_exitsig = pp->p_exitsig;
		if (pp->p_flag & SABORTSIG)
			cp->p_flag |= SABORTSIG;
	}

	cp->p_ppid = pp->p_pid;
	bcopy(&pp->p_ui, &cp->p_ui, sizeof(struct user_info));
	cp->p_acflag = pp->p_acflag | AFORK;

	child_pidlist_add(pp, cp->p_pid);
	/* process can now be found by VPROC_LLOKUP */

	if (thread_save(puth, cuth)) {
		int s;
		/* Child resumes here */

		*pidp = 0;

		/* XXX */
		if (curprocp->p_flag & SFDT)
			p_flagclr(curprocp, SFDT);

		/* return stkptr to caller */
		if (stkptr)
			*stkptr = (caddr_t)cuth->ut_flt_badvaddr;

		/*
		 * Inherit par tracing in child if desired.
		 */
		s = p_lock(cp);
		if (pp->p_flag & SPARINH) {
			#pragma mips_frequency_hint NEVER
			cp->p_flag |= (pp->p_flag & (SPARSYS|SPARSWTCH|SPARINH|SPARPRIV));
			cp->p_parcookie = pp->p_parcookie;
		} else
			cp->p_parcookie = 0;
		p_unlock(cp, s);
	} else {
		int s;
		/*
		 * Copy process.
		 */
		if (error = procdup(cp, cuth, shmask, stkptr, stklen))
			goto err4;

		/*
		 * Notify rtmond of successful forks so it can track process
		 * relationships.  This actually generates more events than
		 * we typically want since this will cause events to be
		 * generated for every process in the system when we often
		 * are just tracing a small handful of processes.  We may
		 * need to think about ways of trying to avoid unneeded
		 * events ...
		 */
		if (IS_TSTAMP_EVENT_ACTIVE(RTMON_PIDAWARE)) {
			#pragma mips_frequency_hint NEVER
			fawlty_fork(UT_TO_KT(puth)->k_id,
				    UT_TO_KT(cuth)->k_id,
				    pp->p_comm);
		}

		/*
		 * Have parent give up processor after its priority is
		 * recalculated so that the child runs first (it's already
		 * on the run queue at sufficiently good priority to accomplish
		 * this).  This allows the dominant path of the child
		 * immediately execing to break the multiple use of copy-on-
		 * write pages with no disk home.  The parent will get to
		 * steal them back rather than gratuitously copying them.
		 */
		*pidp = cp->p_pid;

		/*
		 * Notify UniCenter about new fork/sproc, not new thread.
		 */
		UC_FORK_NOTIFY(cp->p_pid);

		s = kt_lock(UT_TO_KT(cuth));	
		/*
		 * Need to initialize ktimer to some valid non-active state.
		 * If we use an active state, then we end up double charging
		 * CPU and, worse yet, if we end up going to sleep or getting
		 * preempted before putrunq() puts us into AS_RUNQ_WAIT or
		 * it tosses us onto a CPU's next thread, then we could see
		 * absurdly high (and incorrect) CPU usage.  We could use a
		 * special state, say AS_NONE, but then ktimer_switch() would
		 * have to have special code to check for this and we don't
		 * want to slow that down for this single case of starting up
		 * a new thread.  Finally, we could have a new non-active
		 * state for just this situation, say AS_NASCENT_WAIT, but
		 * that would add a timer accumulator to the timer array for
		 * very little purpose.  So, as a compromise, we use
		 * AS_RUNQ_WAIT here.
		 */
		ktimer_init(UT_TO_KT(cuth), AS_RUNQ_WAIT);
		putrunq(UT_TO_KT(cuth), CPU_NONE);
		kt_unlock(UT_TO_KT(cuth), s);
		private.p_runrun = 1;
		if (!KT_ISRT(UT_TO_KT(puth)))
			private.p_lticks = 0;
	}

	return 0;

err4:
        prexit(cp);

	uthread_exit(cuth, 0);

	child_pidlist_delete(pp, cp->p_pid);

err1:
	/* Process will no longer be found by VPROC_LOOKUP */
	PID_PROC_FREE(cp->p_pid);
	VPROC_HOLD(PROC_TO_VPROC(cp));
	vproc_destroy(PROC_TO_VPROC(cp));
	/*
	 * Fork must see only EAGAIN or EMEMRETRY on error.
	 */
	if (shmask == 0 && error != EMEMRETRY)
		error = EAGAIN;
		
	return error;
}

int
uthread_dup(
	uthread_t *puth,	/* calling uthread */
	uthread_t **utp,	/* return uthread via */
	proc_t *p,		/* target process */
	uint shmask,		/* share-mask */
	uthreadid_t ut_id)	/* uthread id */
{
	uthread_t *ut;
	cred_t *cr;
	int error, i;

	/*
	 * XXX	Change arg from proc * to proxy_t *.
	 */
	if (error = uthread_create(p, shmask, utp, ut_id))
		return error;

	ut = *utp;

	ASSERT(ut->ut_curinfo == (struct sigqueue *)NULL);

	/* this is where gfx pthread apps get there gfx */
	if ((UT_TO_KT(puth)->k_runcond & RQF_GFX) &&
	    /* (shmask & PR_THREADS) */
	    (ut->ut_flags & UT_PTHREAD)) {
		gfx_shdup(puth, ut);
	}

	ut->ut_flags |= puth->ut_flags & UT_ISOLATE;
	ut->ut_syscallno = puth->ut_syscallno;
	ut->ut_cmask = puth->ut_cmask;
	ut->ut_sharena = puth->ut_sharena;

	ut->ut_policy = puth->ut_policy;
	ut->ut_tslice = puth->ut_tslice;
	ut->ut_flid.fl_sysid = puth->ut_flid.fl_sysid;
	ut->ut_flid.fl_pid = p->p_pid;	/* XXX ugh */
	UT_TO_KT(ut)->k_nodemask = UT_TO_KT(puth)->k_nodemask;

	/*
	 * If the parent is using the event counters, then
	 * the child will use them as well, in the same
	 * way as the parent. Make sure that the child's
	 * virtual counters are zeroed out.
	 */
	HWPERF_FORK_COUNTERS(puth, ut);

	/*
	 * Timers are cancelled on fork.
	 */
#ifdef DEBUG
	for (i = 0; i < ITIMER_MAX-ITIMER_UTNORMALIZE; i++) {
		ASSERT(!timerisset(&ut->ut_timer[i].it_interval));
		ASSERT(!timerisset(&ut->ut_timer[i].it_value));
	}
#endif

	cr = pcred_access(p);
	ut->ut_cred = cr;
	crhold(cr);
	pcred_unaccess(p);

	/*
	 * Set up sat info for the process 
	 */
	_SAT_PROC_INIT(ut, puth);

	return 0;
}

static int
thread_save(
	uthread_t *puth,
	uthread_t *cuth)
{
	vasid_t		vasid;
	int		s;

	/* Parent's registers are copied to child. */
	bcopy((caddr_t)&UT_TO_KT(puth)->k_regs,
	      (caddr_t)&UT_TO_KT(cuth)->k_regs,
	      sizeof(UT_TO_KT(puth)->k_regs));

	/*
	 * Set up return for child's save() area --
	 * returns 0 on save, 1 on resume.
	 */
	s = disableintr();
	if (save(UT_TO_KT(cuth)->k_regs)) {
		utas_t *cutas = &cuth->ut_as;
		as_getasattr_t asattr;

		ASSERT(curuthread == cuth);
		ASSERT(issplhi(getsr()));

		/* in order to sleep - need utlbmiss stuff setup */
		ASSERT(private.p_utlbmissswtch == 0);

		cutas = &cuth->ut_as;

		if (cutas->utas_flag & UTAS_LPG_TLBMISS_SWTCH)  {
			cutas->utas_flag &= ~UTAS_LPG_TLBMISS_SWTCH;
			ASSERT(cutas->utas_utlbmissswtch & UTLBMISS_LPAGE);
		}

#if DEBUG
		private.p_switching = 0;
#endif

		if (cutas->utas_utlbmissswtch)
			utlbmiss_resume(cutas);

		/* xxx swtch callback scheme */
		if (UT_TO_KT(cuth)->k_runcond & RQF_GFX) {
			/* 0 means no kpreempting */
			rrmResume(cuth, 0);
		}

		/*
		 * If the context switch is to a process where
		 * the performance counters have been enabled
		 * before, then restart them.
		 */
		START_HWPERF_COUNTERS(cuth);
		spl0();

		LOG_TSTAMP_EVENT(RTMON_TASK|RTMON_TASKPROC,
			TSTAMP_EV_EOSWITCH, curthreadp->k_id,
			RTMON_PACKPRI(curthreadp, 0), current_pid(), NULL);

		/*
		 * If the parent has referenced its PRDA page, then we
		 * need to do copy-on-write processing now to change the
		 * pid.  Otherwise we'll defer this until the process vfaults
		 * on the page.
		 */
		as_lookup_current(&vasid);
		(void)VAS_GETASATTR(vasid, AS_PRDA, &asattr);

		if (asattr.as_prda != AS_ADD_UNKVADDR) {
			suword(&((struct prda *)asattr.as_prda)->t_sys.t_pid, 
					current_pid());
			suword(&((struct prda *)asattr.as_prda)->t_sys.t_rpid, 
					(pid_t)cuth->ut_id);
			/*
			 * If user-level sigprocmask flags are set in
			 * t_sys.t_flags but kernel hasn't set up sighold
			 * to point to the prda, turn off sigprocmask flags.
			 */
			if (((struct prda *)asattr.as_prda)->t_sys.t_flags &&
			    (cuth->ut_sighold ==
			     &cuth->ut_proc->p_sigvec.sv_sighold))
				suword(&((struct prda *)
					asattr.as_prda)->t_sys.t_flags, 0);

		}

		return 1;
	}
	enableintr(s);

	/*
	 * Save parents floating point state into the exception structure;
	 * parent gives up fp ownership when it does this. 
	 * This insures that the
	 * child has a copy of the parents floating point state and that
	 * either process will load the fp with their state before running.
	 */
	checkfp(puth, 0);

	/*	Setup child kernel stack -- copy parent's stack.
	 */
	setustk(cuth);

	return 0;
}

/*
 * Create a duplicate copy of a process, everything but stack.
 */
static int
procdup(proc_t *cp,
	uthread_t *cuth,
	uint shmask,
	caddr_t *stkptr,
	size_t stklen)
{
	int		error;
	proc_t		*pp = curprocp;
	vproc_t		*cvp;
	uthread_t	*puth = curuthread;
	vasid_t		vasid;
	vpgrp_t		*pvpg;
	int		s;
	/* REFERENCED */
	pm_t		*pm;
	as_new_t	asn;
	as_newres_t	asnres;
#if EXTKSTKSIZE == 1
	uint		pfn;
#endif
	proc_proxy_t	*pprxy = puth->ut_pproxy;
	proc_proxy_t	*cprxy = cuth->ut_pproxy;

	ASSERT(UT_TO_PROC(puth) == pp);

	/*
	 * Fork the file descriptor table for
	 * - regular processes 
	 * - non-fd sharing sprocs
	 * If fd sharing procs, just set cp's p_fdt to point to the sharena's.
	 *
	 * If the child is getting a new file descriptor table
	 * fdt_fork() will process the "no dup on fork flag".
	 * This flag is used by graphics (/dev/graphics and 
	 * /dev/opengl) as well as Indy video (/dev/vid).
	 */
	if (!IS_SPROC(&pp->p_proxy) ||
	     (!ISSHDFD(&pp->p_proxy)) ||
	     (!ISSHDFD(&cp->p_proxy))) {
		cp->p_fdt = fdt_fork();
		p_flagset(cp, SFDT);
	} else {
		cp->p_fdt = pp->p_shaddr->s_fdt;
		ASSERT(cp->p_fdt);
	}

	ASSERT(puth->ut_openfp == NULL);

	cprxy->prxy_sigtramp = pprxy->prxy_sigtramp;
	cprxy->prxy_siglb = pprxy->prxy_siglb;
	cprxy->prxy_sigstack = pprxy->prxy_sigstack;
	cprxy->prxy_oldcontext = pprxy->prxy_oldcontext;
	cprxy->prxy_sigonstack = pprxy->prxy_sigonstack;

	/* Init utas */
	ASSERT(cuth->ut_as.utas_tlbid);
	set_tlbpids(&cuth->ut_as, (unsigned char)-1);
	setup_wired_tlb_notme(&cuth->ut_as, 1);
	cuth->ut_as.utas_utlbmissswtch = puth->ut_as.utas_utlbmissswtch;
	cuth->ut_as.utas_flag = puth->ut_as.utas_flag & (UTAS_TLBMISS);
	ASSERT((cuth->ut_as.utas_utlbmissswtch == 0) ||
				(puth->ut_as.utas_flag & UTAS_TLBMISS));

	/*
	 * Inherit the vpag in the child.
	 * Need to get vproc for parent and do an op.
	 */
	if (pp->p_vpagg) {
		VPAG_JOIN(pp->p_vpagg, cp->p_pid);
		cp->p_vpagg = pp->p_vpagg;
	}

	/*
	 * initialize AS module
	 * For fork() we get a new AS, for sproc() we join our parent's
	 */
	as_lookup_current(&vasid);

	asn.as_utas = &cuth->ut_as;
	asn.as_stkptr = stkptr ? *stkptr : NULL;
	asn.as_stklen = stklen;
	if ((shmask & (PR_SADDR|PR_SPROC)) == (PR_SADDR|PR_SPROC)) {
		/* sproc && sharing addr */
		asn.as_op = AS_SPROC;
	} else {
		/* fork or sproc (!sharing) */
		if (shmask & PR_SPROC)
			asn.as_op = AS_NOSHARE_SPROC;
		else
			asn.as_op = AS_FORK;
	}

#ifdef _SHAREII
	/*
	 * ShareII must be notified of new thread.
	 * SHR_THREADNEW assumes ut->ut_proc is set correctly.
	 */
	SHR_THREADNEW(puth, cuth);
#endif /*_SHAREII*/

	if (error = VAS_NEW(vasid, &asn, &asnres)) {
		goto bad3;
	}

	cuth->ut_flt_badvaddr = (k_machreg_t)asnres.as_stkptr;
	cuth->ut_asid = asnres.as_casid;

	/*
	 * A few uthread initializations that we don't really
	 * want in uthread_dup -- the caller might have other
	 * ideas for these.
	 */
	cuth->ut_cursig = puth->ut_cursig;
	cuth->ut_rsa_npgs = puth->ut_rsa_npgs;
	cuth->ut_rsa_locore = puth->ut_rsa_locore;
	cuth->ut_maxrsaid = puth->ut_maxrsaid;

	/*
	 * No more possibility of fork failure -- add various
	 * 'errorless' allocations and initializations here.
	 */
	cuth->ut_cdir = puth->ut_cdir;
	cuth->ut_rdir = puth->ut_rdir;
	VN_HOLD(cuth->ut_cdir);
	if (cuth->ut_rdir)
		VN_HOLD(cuth->ut_rdir);

	if (shmask & PR_SPROC) 
		attachshaddr(pp->p_shaddr, cp);

	/*
	 * Sproc'd Children may inherit graphics if they share address space
	 * and file descriptors.
	 */
	/* this is where gfx sproc apps get there gfx */
	/* (after p_shaddr is setup) */
	if ((UT_TO_KT(puth)->k_runcond & RQF_GFX) &&
	    (shmask & (PR_SFDS|PR_SADDR)) == (PR_SFDS|PR_SADDR)) {
		gfx_shdup(puth, cuth);
	}

	/*	Flush the parents regions so that any pages which
	**	were made copy-on-write get flushed from cache.
	**	Unmodify all tlb entries for the parent's regions.
	**	Do this by giving the parent a new tlbpid. (We're flushing
	**	a lot of data pages that we need to and text pages that
	**	we don't need to.)
	**	Setting tlbpid(p) to (unsigned char)-1 forces swtch to
	**	do the new one when the parent is switch to again.
	**	Don't screw kernel processes.
	**	Since we're running process, this will actually change this
	**	processors wired entries
	**
	**	We don't worry about child - since its new, it will ALWAYS
	**	do a context switch first so the tlbpid of
	**	(unsigned char)-1 will be replaced.
	*/
	if (tlbpid(&puth->ut_as) != 0) {
		new_tlbpid(&puth->ut_as, VM_TLBINVAL);
	}

	/* set up affinity link */
	pm = asnres.as_pm;
        kthread_afflink_new(UT_TO_KT(cuth), UT_TO_KT(puth), pm);
	aspm_releasepm(pm);

#if EXTKSTKSIZE == 1
	/*
	 * If the parent has a kernel stack extension page then child
	 * gets one too.
	 */
	if (puth->ut_kstkpgs[KSTEIDX].pgi) {
		if (pfn = stackext_alloc(STACKEXT_NON_CRITICAL)) 
			cuth->ut_kstkpgs[KSTEIDX].pgi =
			    mkpde(PG_VR|PG_G|PG_SV|PG_M|pte_cachebits(), pfn);
	}
#endif

	/*
	 * Hold original exec vnode - we are past errors so child will
	 * definetly go through exit(), until we set SRUN, noone (/proc)
	 * can find this guy so we don't need it held till now
	 */
	if (cp->p_exec = pp->p_exec)
		VN_HOLD(cp->p_exec);

	/*
	 * Maintain refcount of interpreted script.
         */
	if (cp->p_script = pp->p_script) {
		struct vnode *sc = cp->p_script;

		VN_HOLD(sc);
		s = VN_LOCK(sc);
		sc->v_intpcount++;
		VN_UNLOCK(sc, s);
	}

	/*
	 * Profiling information is copied over to the new child.
	 */
	cp->p_profn = pp->p_profn;
	if (pp->p_profn) {
                cp->p_profp = kmem_alloc(pp->p_profn * sizeof(struct prof),
				 KM_SLEEP);
                bcopy(pp->p_profp, cp->p_profp,
				pp->p_profn * sizeof(struct prof));
        }

	/*
	 * Join parent's pgrp. 
	 */
	pvpg = pp->p_vpgrp;
	VPGRP_JOIN(pvpg, cp, 0);
	cp->p_pgid = pp->p_pgid;
	cp->p_vpgrp = pvpg;
	cp->p_sid = pp->p_sid;	/* child has the same sid if in the same pgrp */

	/*
	 * Make child generally visible and put it on the run queue.
	 */
	cvp = PROC_TO_VPROC(cp);
	VPROC_HOLD(cvp);
	VPROC_SET_STATE(cvp, SRUN);
	VPROC_RELE(cvp);

	pid_associate(cvp->vp_pid, cvp);

	return 0;

bad3:
	if (cp->p_flag & SFDT) {
		p_flagclr(cp, SFDT);
		fdt_forkfree(cp->p_fdt);
	}
	cp->p_fdt = NULL;

	if (error == EAGAIN)
		nomemmsg("fork/sproc");
	return error;
}

/*
 * procnew - get a new proc entry, and initialize it with the status SIDL,
 *	a unique PID, slock on, and return a pointer to the entry.  The
 *	new entry will be placed on the active process hash table.
 */
/* ARGSUSED */
int
procnew(proc_t **pp, pid_t *pidp, uint shmask)
{
	proc_t	*p;
	vproc_t	*vp;
	int cnt;
	pid_t newpid;
	int error = 0;
	int loc_npalloc;
	cred_t *cr = get_current_cred();

#ifndef CKPT
	ASSERT(*pidp == 0);
#endif

	cnt = uidact_incr(cr->cr_ruid);

	/* not superuser */
	if (cnt > maxup && cr->cr_uid && cr->cr_ruid) {
		uidact_decr(cr->cr_ruid);
		return EAGAIN;
	}

	/* Track number of currently allocated proc structures.
	 * This enforces the notion of 'nproc'.
	 */
	loc_npalloc = atomicAddInt(&npalloc, 1);

	/* Enforce system nproc limits: If we are not privileged, we
	 * cannot take the last process (npalloc == nproc). If npalloc
	 * is > than nproc, then we have gone over the limit.
	 */
	if (loc_npalloc >= nproc) {
		if (loc_npalloc > nproc || (cr->cr_uid && cr->cr_ruid)) {

			(void) atomicAddInt(&npalloc, -1);
			uidact_decr(cr->cr_ruid);

			SYSERR.procovf++;
			return EAGAIN;
		}
	}

	/* Get vproc and proc structures. */
	vp = vproc_create();
	ASSERT(vp);
	p = pproc_create(vp);
	ASSERT(p);

	if ((error = pid_alloc(*pidp, &newpid)) != 0) {
		/*
		 * error - EAGAIN for tablefull,
		 * EBUSY for requested pid (*pidp) in use
		 * ESRCH for requested pid (*pidp) not in valid range
		 * (EBUSY and ESRCH for chkpt/restart only).
		 */
		(void) atomicAddInt(&npalloc, -1);
		uidact_decr(cr->cr_ruid);
		pproc_return(p);
		vproc_return(vp);
		return error;
	}

#ifdef _SHAREII
	/*
	 * SHR_PROCCREATE enforces the process limit,
	 * and fills in the shadow proc structure pointer.
	 * Any failures after this point need to call vproc_destroy, 
	 * not vproc_return.
	 */
	if (SHR_PROCCREATE(p, SH_FORK_ALL))
	{
		/*
		 * error - EAGAIN for process limit exceeded
		 */
		(void) atomicAddInt(&npalloc, -1);
		uidact_decr(cr->cr_ruid);
		pproc_return(p);
		vproc_return(vp);
		return EAGAIN;
	}
#endif /* _SHAREII */

	p->p_pid = newpid;
	vp->vp_pid = newpid;

	pproc_struct_init(p);

	p->p_start = time;

	/* Establish the identity of this process. */
	p->p_cred = pcred_access(curprocp);
	crhold(p->p_cred);
	pcred_unaccess(curprocp);

	*pp = p;
	return 0;
}


void
copyustk(
	uthread_t	*ut,
	uthread_t	*src_ut)
{
	int		stackoffset;
	exception_t	*up = ut->ut_exception; /* correctly coloured VA */
	caddr_t		sp;
	int		stackmark;
	pde_t		*pde;

	pde = &ut->ut_kstkpgs[KSTKIDX];

	/*
	 * First copy exception area proper ...
	 * page_mapin() should be called after the exception copy
	 * because there is chance that page_mapin() might switch out
	 * the process which will change the save area for the child.
	 * Don't touch wired tlb entry cache and other non-pcb data.
	 *
	 * XXX	Copying the entire pcb/eframe isn't necessary for
	 * XXX	sprocs/uthreads.
	 */
	bcopy(src_ut->ut_exception, up, sizeof(pcb_t) + sizeof(eframe_t));

#if R4000
	sp = page_mapin(pdetopfdat(pde), VM_VACOLOR, colorof(KSTACKPAGE));
#else
	sp = page_mapin(pdetopfdat(pde), 0, 0);
#endif
	/*
	 * then copy some stack for the child process
	 * don't copy more than a page of stack
	 */
	stackoffset = ((__psint_t)&stackmark & (NBPP - 1));

	bcopy(&stackmark, sp + stackoffset, NBPP - stackoffset);

	page_mapout(sp);
}

/*
 * Setup context of child process
 */
void
setustk(uthread_t *ut)
{
	copyustk(ut, curuthread);
}

/* 
 * Initialize the wired tlb entries used for mapping page tables.
 * Called on fork, exec, region shrink or detach.
 * (Generally, when page tables are removed.)
 * Page table for wired entries is cleared.
 * Virtual address table is set to an invalid virtual address.
 * Wired tlb is cleared if current process.
 * Index of next open entry is reset.
 * Note: Wired tlb entries are "faulted in" by tfault on double tlb miss.
 */
/*ARGSUSED*/
void
setup_wired_tlb(int zap)
{
	uthread_t *ut = curuthread;
#if !NO_WIRED_SEGMENTS
	register int i;
#if R4000 || R10000
	register tlbpde_t *pd;
#else
	register pde_t *pd;
#endif
	register caddr_t *q;

#ifdef FAST_LOCORE_TFAULT
	/* zap segment table */
	if (zap) 
#endif
	zapsegtbl(&ut->ut_as);

	q = ut->ut_exception->u_tlbhi_tbl;
	pd = ut->ut_exception->u_ubptbl;
	for (i = 0; i < NWIREDENTRIES-TLBWIREDBASE; i++, pd++, q++) {
#if R4000 || R10000
		pd->pde_low.pgi = 0;
		pd->pde_hi.pgi = 0;
#else
		pd->pgi = 0;
#endif
		*q = (caddr_t)PHYS_TO_K0(0);

		/* clear out real TLB (except if upage/kstk) */
#if TLBKSLOTS == 1
		/*
		 * if TLBKSLOTS == 0, one half of pda entry
		 * is used to map it.
		 */
		if (i != (UKSTKTLBINDEX-TLBWIREDBASE))
#endif
			invaltlb(i + TLBWIREDBASE);
	}
	ut->ut_exception->u_nexttlb = 0;

#else /* NO_WIRED_SEGEMENTS */
	/* zap segment table */
	zapsegtbl(&ut->ut_as);
#endif /* NO_WIRED_SEGMENTS */
}

/*
 * Same as setup_wired_tlb, but used on threads other than the
 * currently executing thread.  Can be called at any time -- even
 * when there's no current exception struct.
 * Note that there are no locks here - the thread really shouldn't be
 * able to exit since our caller must have some reference. They
 * shouldn't be able to be filling in wired entries themselves since
 * this is only called threads that are in the callers AS and have
 * the aspacelock for update.
 */
/*ARGSUSED*/
void
setup_wired_tlb_notme(utas_t *utas, int zap)
{
#ifndef NO_WIRED_SEGMENTS
	uthread_t *ut = utas->utas_ut;
#if R4000 || R10000
	register tlbpde_t *pd;
#else
	register pde_t *pd;
#endif
	register caddr_t *q;
	register exception_t *up;
	int i;

	/* we're clobbering wired entries, so clear the segtbl flag */
#ifdef FAST_LOCORE_TFAULT
	if (zap)
#endif
	zapsegtbl(utas);

	up = ut->ut_exception;
	ASSERT((__psunsigned_t)up > K0BASE);
	q = up->u_tlbhi_tbl;
	pd = up->u_ubptbl;
	for (i = 0; i < NWIREDENTRIES-TLBWIREDBASE; i++, pd++, q++) {
		/*
		 * RACE - the process whose wired entries
		 * we are zapping may be context switching in
		 * on another CPU - the resume() code
		 * doesn't hold any locks - it just
		 * sets all the wired entries - so
		 * we must be sure that we invalidate the
		 * virtual address first.
		 */
		*q = (caddr_t)PHYS_TO_K0(0);
#if R4000 || R10000
		pd->pde_low.pgi = 0;
		pd->pde_hi.pgi = 0;
#else
		pd->pgi = 0;
#endif
	}
	up->u_nexttlb = 0;

#else /* NO_WIRED_SEGMENTS */
	/*
	 * we're clobbering wired entries, so clear the segtbl flag
	 */
	zapsegtbl(utas);

#endif	/* !NO_WIRED_SEGMENTS */
}
