/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1996, Silicon Graphics, Inc.	  *
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

#ident	"$Revision: 3.354 $"

#include <sys/types.h>
#include <sys/acct.h>
#include <ksys/as.h>
#include <sys/atomic_ops.h>
#include <sys/buf.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <ksys/childlist.h>
#include <ksys/exception.h>
#include <ksys/vfile.h>
#include <ksys/fdt.h>
#include <sys/kabi.h>
#include <sys/kmem.h>
#include <sys/kresource.h>
#include <sys/ksignal.h>
#include <sys/lock.h>
#include <sys/numa.h>
#include <sys/par.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <ksys/pid.h>
#include <sys/prctl.h>
#include <sys/proc.h>
#include <sys/procset.h>
#include <procfs/prsystm.h>
#include <sys/sema.h>
#include <sys/resource.h>
#include <sys/runq.h>
#include <ksys/vsession.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/ktime.h>
#include <sys/time.h>
#include <sys/var.h>
#include <sys/vnode.h>
#include <sys/wait.h>
#include <sys/sat.h>
#include <sys/imon.h>
#include <sys/rtmon.h>
#include <ksys/vpgrp.h>
#include <ksys/vproc.h>
#include <sys/xlate.h>
#include <sys/frs.h>
#include "os/proc/pproc_private.h"

#include <sys/hwperftypes.h>
#include <sys/hwperfmacros.h>

#include <sys/kusharena.h>
#include <sys/unc.h>

extern int gfx_exit(void);
extern void gfx_shcheck(uthread_t *);

/* register a callback function to be called when the specified
 * process exits. Returns 0 on success, ESRCH if the specified proc
 * is invalid.
 */
/* ARGSUSED */
int
add_exit_callback(pid_t pid, int flags, void (* func)(void *), void *arg)
{
	vproc_t *vpr;
	int unlock;
	int error;

	if (pid != current_pid()) {
		if ((vpr = VPROC_LOOKUP(pid)) == NULL)
			return ESRCH;
		unlock = 1;
	} else {
		vpr = curvprocp;
		unlock = 0;
	}

	VPROC_ADD_EXIT_CALLBACK(vpr, flags, func, arg, error);

	if (unlock)
		VPROC_RELE(vpr);

	return error;

}

/*
 * exit system call:
 * pass back caller's arg
 */
struct exita {
	sysarg_t rval;
};

/* ARGSUSED1 */
int
rexit(struct exita *uap, rval_t *rvp)
{
	exit(CLD_EXITED, uap->rval);

	/* NOTREACHED */
	return(0);
}

/*
 * Release resources.
 * Enter zombie state.
 * Wake up parent and init processes, and dispose of children.
 *
 * If a subsystem needs to guarantee that a process does not exit -
 * use VPROC_LOOKUP/VPROC_RELE.
 */
void
exit(int why, int what)
{
	uthread_t *ut = curuthread;
	vproc_t *vpr;
	proc_proxy_t *prxy = ut->ut_pproxy;
	int ut_exitflags, s;

	if (current_pid() == INIT_PID)
		cmn_err(CE_PANIC,
			"init died (why = %d, what = 0x%x)", why, what);

	/*
	 * Notify UniCenter about exitting process, not thread.
	 */
	UC_EXIT_NOTIFY(why, what, current_pid());

	if (prxy->prxy_fp.pfp_dismissed_exc_cnt)
		cmn_err_tag(281,CE_NOTE, "Dismissed %d exceptions for %s (pid %d)",
			    prxy->prxy_fp.pfp_dismissed_exc_cnt,
			    get_current_name(), current_pid());

	/*
	 * Accumulate this threads timer and acct information into the proxy.
	 * Must be done before accounting.
	 */
	dump_timers_to_proxy(ut);
	s = prxy_lock(prxy);
	uthread_acct(ut, &prxy->prxy_exit_acct);
	prxy_unlock(prxy, s);

	ASSERT(!issplhi(getsr()));
	if (UT_TO_KT(ut)->k_runcond & RQF_GFX)
		gfx_exit();
	ASSERT(!issplhi(getsr()));

	/*
	 * Frame Scheduler callback
	 */
	frs_thread_exit(ut);

	vpr = UT_TO_VPROC(ut);
	ASSERT(vpr);

	VPROC_HOLD(vpr);

	ASSERT(prxy->prxy_nlive >= 1);
	atomicAddInt(&prxy->prxy_nlive, -1);

	if (ut->ut_flags & UT_PTHREAD) {
		/*
		 * If this isn't the first thread to exit/exec,
		 * just leave gracefully.
		 */
		VPROC_THREAD_STATE(vpr, THRD_EXIT, s);
		if (s) {
			if (UT_TO_KT(ut)->k_runcond & RQF_GFX)
				gfx_shcheck(ut);
			LOG_TSTAMP_EVENT(RTMON_PIDAWARE, TSTAMP_EV_EXIT,
					 UT_TO_KT(ut)->k_id, current_pid(), NULL, NULL);

			uthread_exit(ut, 0);
			ASSERT(0);
			/* NOTREACHED */
		}
	}
	VPROC_SET_STATE(vpr, SZOMB);

	ASSERT(!issplhi(getsr()));

#if R10000
	/*
	 * This code is copied out of uthread_divest() because we must ensure
	 * that hardware profiling interrupts are torn down *before* we
	 * tear down the proc p_profp/p_profn.  If we don't ensure this
	 * ordering, we could take a hardware profiling interrupt and then
	 * crash when dereferencing a p_profp that is NULL (or even worse,
	 * random garbage if the memory is reallocated soon enough and
	 * reused).  We end up with this particular ordering problem because
	 * we tear down the proc/proxy here *before* we tear down the last
	 * uthread of a process a few lines further down.  (sigh)
	 */
	if (ut->ut_cpumon) {
		hwperf_disable_counters(ut->ut_cpumon);
		hwperf_cpu_monitor_info_free(ut->ut_cpumon);
		ut->ut_cpumon = NULL;
	}
#endif

	ASSERT(prxy->prxy_nthreads == 1);
	pproc_exit(ut, why, what, &ut_exitflags);
	ASSERT(UT_TO_VPROC(ut));

	/*
	 * NB: we distribute process exit events when tracing
	 * several different classes of events because the
	 * monitoring processes use these events to decide
	 * when to terminate tracing; when doing things on
	 * a per-process or multi-process basis.
	 */
	LOG_TSTAMP_EVENT(RTMON_PIDAWARE, TSTAMP_EV_EXIT,
		UT_TO_KT(ut)->k_id, current_pid(), NULL, NULL);

	uthread_exit(ut, ut_exitflags);

	/* no deposit, no return */
}

/*
 * Format siginfo structure for wait system calls.
 * assumed siglck locked
 */

static void
winfo(child_pidlist_t *cpid, k_siginfo_t *si, int waitflag)
{
        si->si_signo = SIGCLD;
        si->si_code = cpid->cp_wcode;
        si->si_pid = cpid->cp_pid;
        si->si_status = cpid->cp_wdata;
        si->si_stime = hzto(&cpid->cp_stime);
        si->si_utime = hzto(&cpid->cp_utime);

        if (waitflag) {
                cpid->cp_wcode = 0;
                cpid->cp_wdata = 0;
	}
}

/*
 * waitsys wrapper
 * guts of svr4 waitid
 */
struct waitsysa {
	sysarg_t	idtype;
	sysarg_t	id;
	siginfo_t	*info;
	sysarg_t	options;
	struct rusage	*rup;
};

#if _MIPS_SIM == _ABI64
int rusage_to_irix5(void *, int , xlate_info_t *);
#endif

int
waitsys_wrapper(struct waitsysa *uap, rval_t *rvp)
{
	k_siginfo_t kinfo;
	struct rusage rup;
	int error;
	irix5_siginfo_t i5_info;
	uint infosize;
	uint rupsize;
	int abi = get_current_abi();

#if _MIPS_SIM == _ABI64
	irix5_64_siginfo_t i5_64_info;

	infosize = ABI_IS_IRIX5_64(abi) ?
			sizeof(siginfo_t) : sizeof(irix5_siginfo_t);
	rupsize = ABI_IS_IRIX5_64(abi) ?
			sizeof(struct rusage) : sizeof(struct irix5_rusage);
#else
	infosize = sizeof(irix5_siginfo_t);
	rupsize = sizeof(struct irix5_rusage);
#endif

	if (uap->info == 0)
		return EINVAL;
	if (error = useracc(uap->info, infosize, B_WRITE, NULL))
		return error;
	unuseracc(uap->info, infosize, B_WRITE);

	if (uap->rup) {
		if (error = useracc(uap->rup, rupsize, B_WRITE, NULL))
			return error;
		unuseracc(uap->rup, rupsize, B_WRITE);
	}

	ASSERT((void *)uap == curuthread->ut_scallargs);

	error = waitsys((idtype_t)uap->idtype, (id_t)uap->id, &kinfo,
			(int)uap->options, uap->rup ? &rup : 0, rvp);
	if (error)
		return error;

	switch (abi) {
	case ABI_IRIX5:
	case ABI_IRIX5_N32:
		irix5_siginfoktou(&kinfo, &i5_info);
		if (copyout(&i5_info, uap->info, sizeof i5_info))
			return EFAULT;
		break;
#if _MIPS_SIM == _ABI64
	case ABI_IRIX5_64:
		irix5_64_siginfoktou(&kinfo, &i5_64_info);
		if (copyout(&i5_64_info, uap->info, sizeof i5_64_info))
			return EFAULT;
		break;
#endif
	default:
		cmn_err(CE_PANIC,
			"waitsys: unrecognized ABI in switch, abi 0x%x", abi);
		/* NOTREACHED */
	}

	if (uap->rup && XLATE_COPYOUT(&rup, uap->rup, sizeof rup,
					rusage_to_irix5, abi, 1)) 
		return EFAULT;
	return 0;
}

#ifdef DEBUG
extern int signaltrace;
#endif

int
waitsys(idtype_t idtype, 
	id_t id,
	k_siginfo_t *si,
	int options,
	struct rusage *rup,
	rval_t *rvp)
{
	int found, error = 0;
	struct child_pidlist **list;
	struct child_pidlist *cpid;
	vproc_t *vpr;
	vpgrp_t	*vpg;
	proc_t *pp;
	int s;

	if (options == 0 || (options & ~(WOPTMASK|WUNTRACED)))
		return EINVAL;
	
	switch (idtype) {
	case P_PID:
		if (id < 0 || id >= MAXPID)
			return EINVAL;
		break;
	case P_PGID:
		if (id < 0 || id >= MAXPID)
			return EINVAL;
		vpg = VPGRP_LOOKUP(id);
		if (vpg == NULL)
			return ECHILD;
		VPGRP_RELE(vpg);
		break;
	case P_ALL:
		break;
	default:
		return EINVAL;
	}
        bzero(si, sizeof *si);

	/*
         * p_childlock protects our child chain
         * To avoid races, we set the SWSRCH bit when we start
         * the child search, any operation that can change the
         * status of a child, will reset our SWSRCH bit - before
         * we go to sleep, we check it.
         */
	pp = curprocp;

again:
        mutex_lock(&pp->p_childlock, PZERO);

    while (*(list = &pp->p_childpids) != NULL) {
	p_flagset(pp, SWSRCH);
	found = 0;
	do {
		cpid = *list;
		ASSERT(cpid != NULL);

		if (idtype == P_PID && id != cpid->cp_pid)
			continue;
		found++;

		/*
		 * zombie children - exited, core dumped, killed.. 
		 */
		switch (cpid->cp_wcode) {

		case CLD_EXITED:
		case CLD_DUMPED:
		case CLD_KILLED: {
			struct rusage rusage;
			int rtflags;

			ASSERT((pp->p_sigvec.sv_flags & SNOWAIT) == 0);

			/*
			 * If the caller did not choose to wait
			 * for this state, too bad: this process (child)
			 * is not going to get into any other state.
			 * Decrement found variable.
			 */
			if (!(options & WEXITED)) {
				if (idtype == P_PID)
					goto echild;
				else {
					found--;
					continue;
				}
			}

			if (idtype == P_PGID && id != cpid->cp_pgid) {
				found--;
				continue;
			}

			winfo(cpid, si, !(options & WNOWAIT));

			if (options & WNOWAIT)
				goto out;

			/* Remove this now-exited child from list. */
			*list = cpid->cp_next;

			/* Unlock p_childlock before doing remote ops. */
			mutex_unlock(&pp->p_childlock);

			vpr = VPROC_LOOKUP_STATE(cpid->cp_pid, ZYES);
			ASSERT(vpr != NULL);

			rtflags = 0;
#ifdef R10000
			VPROC_REAP(vpr, 0, &rusage, pp->p_proxy.prxy_cpumon,
					&rtflags);
#else
			VPROC_REAP(vpr, 0, &rusage, NULL, &rtflags);
#endif
			/*
			 * the VPROC_REAP deleted this vproc -
			 * so no VPROC_RELE needed.
			 */

			/* Add the rusage we got from our child into
			 * our rusage struct
			 */
			if (rtflags & VWC_RUSAGE) {
				ruadd(&pp->p_cru, &rusage);
				if (rup != NULL)
					bcopy(&rusage, rup, sizeof *rup);
			}

			rvp->r_val1 = 0;
			rvp->r_val2 = cpid->cp_xstat;

			/* free child list entry for this child */
			kmem_zone_free(child_pid_zone, cpid);

			p_flagclr(pp, SWSRCH);

			goto out_nosema;
		    }

		case CLD_TRAPPED:
			if (!(options & WTRAPPED))
				break;

			winfo(cpid, si, !(options & WNOWAIT));

			goto out;

		case CLD_STOPPED:
			if (!(options & WSTOPPED))
				break;

			winfo(cpid, si, !(options & WNOWAIT));

			/*
			 * Final check in case the parent is in the same pgrp
			 * and should be stopping also.
			 */
			if (pp->p_pgid == cpid->cp_pgid) {
				/*
			 	 * We need to drop psema and thus we note
				 * the pgrp signal sequence number beforehand.
				 */
				sequence_num_t	ch_pgsigseq = cpid->cp_pgsigseq;
				p_flagclr(pp, SWSRCH);
				mutex_unlock(&pp->p_childlock);

				/*
				 * Check signal sequence count to solve a
				 * race condition where a jcl signal can be
				 * delivered to the child ahead of the parent.
				 * This is particularly likely if the child
				 * resides on a cell other than its parent.
				 *
				 */
				ASSERT(pp->p_vpgrp);
				if (VPGRP_SIG_WAIT(pp->p_vpgrp, ch_pgsigseq))
					goto again;
				/*
				 * Signal sequence numbers match, so check
				 * if the parent too must stop.
				 */
				if (wait_checkstop())
					error = EINTR;

				goto out_nosema;
			}
			goto out;

		case CLD_CONTINUED:
			if (!(options & WCONTINUED))
				break;

			winfo(cpid, si, !(options & WNOWAIT));

			if (pp->p_pgid == cpid->cp_pgid) {
				sequence_num_t	ch_pgsigseq = cpid->cp_pgsigseq;
				p_flagclr(pp, SWSRCH);
				mutex_unlock(&pp->p_childlock);
				/*
				 * Check signal sequence count to solve a
				 * race condition where a jcl signal can be
				 * delivered to the child ahead of the parent.
				 * This is particularly likely if the child
				 * resides on a cell other than its parent.
				 *
				 * XXX is this procedure really necessary here?
				 */
				ASSERT(pp->p_vpgrp);
				if (VPGRP_SIG_WAIT(pp->p_vpgrp, ch_pgsigseq))
					goto again;
			
				if (wait_checkstop())
					error =  EINTR;

				goto out_nosema;
			}
			goto out;

		}

		if (idtype == P_PID)
			break;

	} while ((*(list = &((*list)->cp_next))) != NULL);

	if (!found)
		break;
	if (options & WNOHANG) {
		si->si_pid = 0;
		goto out;
	}

	mutex_unlock(&pp->p_childlock);
	s = p_lock(pp);
	if ((pp->p_flag & SWSRCH) == 0) {
		/* somebody changed status */
		p_unlock(pp, s);
	} else {
		pp->p_flag &= ~SWSRCH;
		if (p_sleepsig(pp, &pp->p_wait, PWAIT, s) == -1) {
			/* real signal - let's get out of here */
			return EINTR;
		}
	}
	mutex_lock(&pp->p_childlock, PZERO);
    }

echild:
	error = ECHILD;
out:
	p_flagclr(pp, SWSRCH);
        mutex_unlock(&pp->p_childlock);

out_nosema:

#if DEBUG
	if (signaltrace == -1 || signaltrace == pp->p_pid) {
		cmn_err(CE_DEBUG,
			"waitsys: [%s,%d] receives error %d pid %d wcode %d\n",
			get_current_name(), current_pid(),
			error, si->si_pid, si->si_code);
	}
#endif

	return error;
}

/*
 * Special 'partial' exit -- called by process that will never again
 * leave kernel space.
 * Detachshaddr also closes shared file descriptors and removes other
 * sharable resources -- should we bother to close non-shared open file
 * descriptors, too?
 */
int
vrelvm(void)
{
	uthread_t *ut = curuthread;
	proc_t	*p = UT_TO_PROC(ut);
	vasid_t vasid;
	as_deletespace_t asd;

	/* No need to support sprocs or pthread programs turning
	 * themselves into kernel daemons.
	 */
	if (IS_SPROC(ut->ut_pproxy) || IS_THREADED(ut->ut_pproxy))
		return EINVAL;

	ASSERT(ut->ut_sighold == &p->p_sigvec.sv_sighold);

	asd.as_vrelvm_prda = ut->ut_prda;
	ut->ut_prda = 0;
	asd.as_vrelvm_detachstk = 0;

	as_lookup_current(&vasid);
	asd.as_op = AS_DEL_VRELVM;
	VAS_DELETESPACE(vasid, &asd, NULL);

	/*
	 * don't actually get rid of AS - some NFS deamons call fork()
	 * which needs an AS
	 */
	ASSERT(!IS_SPROC(ut->ut_pproxy));

	/* Protect against possible accessors of p_exec (however unlikely) */
	if (p->p_exec) {
		mrupdate(&p->p_who);
		VN_RELE(p->p_exec);
		p->p_exec = NULL;
		mrunlock(&p->p_who);
	}
	return 0;
}

