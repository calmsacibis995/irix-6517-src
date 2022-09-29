/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 3.275 $"

#include "sys/types.h"
#include "sys/atomic_ops.h"
#include "ksys/as.h"
#include "sys/buf.h"
#include "sys/capability.h"
#include "sys/cmn_err.h"
#include "sys/cred.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "ksys/fdt.h"
#include "ksys/vfile.h"
#include "sys/kmem.h"
#include "sys/kusharena.h"
#include "limits.h"
#include "sys/mman.h"
#include "sys/prctl.h"
#include "sys/resource.h"
#include "sys/runq.h"
#include "sys/splock.h"
#include "sys/sat.h"
#include "sys/sched.h"
#include "sys/sema.h"
#include "ksys/vsession.h"
#include "sys/schedctl.h"
#include "sys/sysinfo.h"
#include "sys/sysmacros.h"
#include "sys/systm.h"
#include "sys/var.h"
#include "sys/vnode.h"
#include "ksys/vproc.h"
#include "sys/space.h"
#include "fs/procfs/prsystm.h"	
#include "os/proc/pproc_private.h"

extern int maxup;
static void blockself(void);

/*
 * System call interface
 *	prctl(option, [value, [value2]])
 *	procblk:
 *		blockproc(pid)
 *		unblockproc(pid)
 *		setblockproc(pid, count)
 */

/*
 * prctl - process ioctl
 */
struct prctla {
	sysarg_t	option;
	sysarg_t	value;
	sysarg_t	value2;
	sysarg_t	value3;	/* perm arg to PR_ATTACHADDRPERM */
	sysarg_t	value4;	/* "hint" attachaddr to PR_ATTACHADDRPERM */
};

int
prctl(struct prctla *uap, rval_t *rvp)
{
	vproc_t *vpr;
	uthread_t *ut;
	int isself = 0;
	int error = 0;
	int s;
	rlim_t lim;

	switch (uap->option) {
	case PR_ISBLOCKED:
		if (uap->value == 0 || uap->value == current_pid()) {
			vpr = curvprocp;
			isself = 1;
		} else if ((vpr = VPROC_LOOKUP((pid_t)uap->value)) == NULL)
			return ESRCH;

		VPROC_PRCTL(vpr, uap->option, 0, isself, get_current_cred(),
				current_pid(), rvp, error);
		if (!isself)
			VPROC_RELE(vpr);

		break;

	case PR_MAXPROCS:
		rvp->r_val1 = maxup;
		break;

	case PR_MAXPPROCS:
		/* return how many processors caller can run on */
		rvp->r_val1 = runprunq(curthreadp);
		break;

	case PR_GETSTACKSIZE:
		lim = getaslimit(curuthread, RLIMIT_STACK);

		if (ABI_IS_IRIX5(get_current_abi())) {
		       if (lim == RLIM_INFINITY) {
			       rvp->r_val1 = RLIM32_INFINITY;
		       } else if (lim > UINT_MAX) {
			       rvp->r_val1 = UINT_MAX;
		       } else {
			       rvp->r_val1 = lim;
		       }
		} else {
			rvp->r_val1 = lim;
		}
		break;

	case PR_SETSTACKSIZE:
		{
		struct rlimit value;

		if (ABI_IS_IRIX5(get_current_abi())) {
			if (uap->value > RLIM32_INFINITY)
				return EINVAL;
			value.rlim_cur = RLIM32_CONV(uap->value);
		} else {
			value.rlim_cur = uap->value;
		}

		value.rlim_max = value.rlim_cur;
		error = setrlimitcommon(RLIMIT_STACK, &value);
		if (error)
			return error;

		rvp->r_val1 = getaslimit(curuthread, RLIMIT_STACK);
		break;
		}

	case PR_UNBLKONEXEC:
		if (uap->value == 0 || uap->value == current_pid()) {
			return EINVAL;
		} else if ((vpr = VPROC_LOOKUP((pid_t)uap->value)) == NULL)
			return ESRCH;

		VPROC_PRCTL(vpr, uap->option, 0, 0, get_current_cred(),
				current_pid(), rvp, error);
		VPROC_RELE(vpr);

		if (!error)
			CURVPROC_SET_UNBLKONEXECPID(uap->value, error);

		break;

	case PR_RESIDENT:
		/*
		 * All processes are always ``resident'' now that
		 * 1) they have no uareas that can be swapped out; and
		 * 2) their kernel stacks are no longer swapped while
		 *    a uthread is in the kernel
		 */
		break;

	case PR_SETEXITSIG:
	case PR_SETABORTSIG:
		/* 0 ==> no signal to be sent */
		if (uap->value < 0 || uap->value >= NSIG)
			return EINVAL;

		VPROC_PRCTL(curvprocp, uap->option, uap->value, 1, NULL,
				0, rvp, error);

		break;

	case PR_ATTACHADDRPERM:
	case PR_ATTACHADDR:
		{
		proc_t *sourcep;
		as_addspace_t asd;
		as_addspaceres_t asres;
		struct prattach_args_t prargs;
		struct prattach_results_t prres;
		vasid_t vasid;

		if (uap->option == PR_ATTACHADDRPERM) {
			if (copyin((caddr_t)uap->value, &prargs,
					sizeof(prargs)))
				return(EFAULT);
			if (prargs.prots & (PROT_WRITE|PROT_EXEC))
				prargs.prots |= PROT_READ;
			if (prargs.flags & ~PRATTACH_MASK)
				return EINVAL;
		} else {
			prargs.local_vaddr = -1;
			prargs.vaddr = uap->value2;
			prargs.pid = uap->value;
			prargs.prots = 0; /* default to RW */
			prargs.flags = 0;
		}

		asd.as_op = AS_ADD_ATTACH;
		asd.as_addr = (uvaddr_t)prargs.local_vaddr;
		asd.as_prot = prargs.prots;
		asd.as_attach_srcaddr = (uvaddr_t)prargs.vaddr;
		asd.as_attach_flags = 0;

		if (prargs.pid != current_pid()) {
			/* get address space id of 'source' */
			if ((vpr = VPROC_LOOKUP(prargs.pid)) == NULL)
				return ESRCH;
			VPROC_GET_PROC(vpr, &sourcep);
			uscan_hold(&sourcep->p_proxy);
			asd.as_attach_asid = prxy_to_thread(&sourcep->p_proxy)->ut_asid;
			uscan_rele(&sourcep->p_proxy);
			VPROC_RELE(vpr);
		} else {
			asd.as_attach_flags |= AS_ATTACH_MYSELF;
		}
		as_lookup_current(&vasid);
		if (error = VAS_ADDSPACE(vasid, &asd, &asres)) {
			return error;
		}

		/* return same logical offset into region as user passed in */
		rvp->r_val1 = (__psint_t)asres.as_attachres_vaddr;

		/* Copy attach info to caller if extended attach call */
		if (uap->option == PR_ATTACHADDRPERM) {
			prres.local_vaddr = rvp->r_val1;
			prres.region_base = (__int64_t)asres.as_addr;
			prres.region_size = asres.as_attachres_size;
			prres.prots = asres.as_prot;
			/* XXX cheat */
			(void)copyout(&prres, (caddr_t)uap->value2,
					sizeof(prres));
		}
		break;
		}
	case PR_TERMCHILD:
		VPROC_PRCTL(curvprocp, PR_TERMCHILD, 0, 1, NULL,0, rvp, error);

		break;

	case PR_GETSHMASK:
		/*
		 * get shmask info
		 * We also try to pack alot of info about us and the
		 * specified process
		 */
		if (!IS_SPROC(curuthread->ut_pproxy))
			return EINVAL;

		if (uap->value == 0 || uap->value == current_pid()) {
			rvp->r_val1 = curuthread->ut_pproxy->prxy_shmask;
			return 0;
		} else if ((vpr = VPROC_LOOKUP((pid_t)uap->value)) == NULL)
			return ESRCH;

		VPROC_PRCTL(vpr, PR_GETSHMASK, 0, 0, get_current_cred(),
				current_pid(), rvp, error);
		VPROC_RELE(vpr);

		if (!error) {
			/* VPROC_PRCTL sets r_val1 to the target processes
			 * shmask. We and that with the current process
			 * shmask.
			 */
			rvp->r_val1 &= curuthread->ut_pproxy->prxy_shmask;
		}

		break;

	case PR_GETNSHARE:

		if (!IS_SPROC(curuthread->ut_pproxy)) {
			rvp->r_val1 = 0;
			return 0;
		}
		VPROC_PRCTL(curvprocp, PR_GETNSHARE, 0, 1, NULL,0, rvp, error);

		break;

	case PR_LASTSHEXIT:

		if (!IS_SPROC(curuthread->ut_pproxy)) {
			rvp->r_val1 = 1;
			return 0;
		}
		VPROC_PRCTL(curvprocp, PR_LASTSHEXIT, 0, 1, NULL,0, rvp, error);

		break;

	case PR_COREPID:

		if (uap->value == 0 || uap->value == current_pid()) {
			vpr = curvprocp;
			isself = 1;
		} else if ((vpr = VPROC_LOOKUP((pid_t)uap->value)) == NULL)
			return ESRCH;

		VPROC_PRCTL(vpr, PR_COREPID, uap->value2, isself,
			get_current_cred(), current_pid(), rvp, error);

		if (!isself)
			VPROC_RELE(vpr);
		break;

	case PR_INIT_THREADS:
	{
		/*
		 * This call sets the necessary process and
		 * thread bits so that all the process threads will
		 * 1) get the 'pthreads' bit set;
		 * 2) receive pthreads resched signals from clock.
		 *
		 * Also sets up user (library)-level control of
		 * signal mask.
		 */
		ut = curuthread;
		if (ut->ut_pproxy->prxy_shmask & PR_SPROC)
			return EINVAL;

		if (ut->ut_pproxy->prxy_utshare)
			break;

		if (ut->ut_prda == NULL) {
			if (error = lockprda(&ut->ut_prda)) {
				return error;
			}
			ASSERT(ut->ut_prda->t_sys.t_pid == current_pid());
		}

		/*
		 * No failures from here onward.
		 */

		s = ut_lock(ut);
		ut->ut_flags |= UT_PTHREAD|UT_PTPSCOPE;
		ut_unlock(ut, s);

		/*
		 * Caller could have made syssgi(SGI_PROCMASK_LOCATION)
		 * request earlier.
		 */
		if (ut->ut_sighold == &UT_TO_PROC(ut)->p_sigvec.sv_sighold)
			uthread_setsigmask(ut, ut->ut_sighold);

		/*
		 * Switch from single to multi-threaded environment should
		 * not be seen half-done.  There a number of possible 
		 * ugly interactions with procfs code.  Holding uscan_update
		 * allows us to make sure sure that all scanners see one
		 * environment or the other.
		 */
		uscan_update(ut->ut_pproxy);
		allocpshare();
		prmulti(curprocp);
		uscan_unlock(ut->ut_pproxy);

		if (ut->ut_pproxy->prxy_sched.prs_job = ut->ut_job) {
			job_cache(ut->ut_job);
		}
		ut->ut_pproxy->prxy_sigthread = 0;	/* no one yet */

		break;
	}

	case PR_THREAD_CTL:
	{
		ut = curuthread;

		if (!(ut->ut_flags & UT_PTHREAD))
			return EPERM;

		switch (uap->value) {

		case PR_THREAD_BLOCK:
			blockself();
			break;

		case PR_THREAD_UNBLOCK:
			CURVPROC_UNBLOCK_THREAD((uthreadid_t)uap->value2, s);
			return s ? 0 : ESRCH;

		case PR_THREAD_EXIT:
			/*
			 * Let a uthread exit without removing the process.
			 * If it's the last uthread, say bye-bye.
			 * XXX	This may change if/when nanothreads are
			 * XXX	implemented.
			 */
			ASSERT(ut->ut_pproxy->prxy_nlive >= 1);
			if (atomicAddInt(&ut->ut_pproxy->prxy_nlive, -1) == 0) {
				atomicAddInt(&ut->ut_pproxy->prxy_nlive, 1);
				exit(CLD_EXITED, 0);
			}
			if (UT_TO_KT(ut)->k_runcond & RQF_GFX) {
				extern int gfx_exit(void);
				extern void gfx_shcheck(uthread_t *);
				gfx_exit();
				gfx_shcheck(ut);
			}

			/* Accumulate accounting information. */
			dump_timers_to_proxy(ut);
			s = prxy_lock(ut->ut_pproxy);
			uthread_acct(ut, &ut->ut_pproxy->prxy_exit_acct);
			prxy_unlock(ut->ut_pproxy, s);

			vpr = UT_TO_VPROC(ut);
			VPROC_HOLD(vpr);
			uthread_exit(ut, 0);
			break;

		case PR_THREAD_KILL:
		{
			uthreadid_t tid = uap->value2;

			ASSERT(ut->ut_prda);
			if (tid == UT_ID_NULL)
				tid = ut->ut_prda->t_sys.t_rpid;

			/*
			 * This returns 1 on success, 0 otherwise.
			 */
			CURVPROC_SIG_THREAD(tid, (int)uap->value3, s);
			return s ? 0 : ESRCH;
		}

		case PR_THREAD_SCHED:
		{
			uthreadid_t	tid = uap->value2;
			prsched_t	prs;
			prsched_t	oprs;


			/* Read in new scheduling attrs.
			 */
			if (uap->value3) {

			    if (!_CAP_CRABLE(get_current_cred(), CAP_SCHED_MGT))
				return EPERM;

			    if (copyin((void*)uap->value3, &prs, sizeof(prs)))
				return EFAULT;

			    if (ut->ut_pproxy->prxy_sched.prs_flags & PRSBATCH
				|| prs.prs_policy == SCHED_TS)
				return EINVAL;
			}

			/* Look up uthread by id unless it's us.
			 */
			if (tid != UT_ID_NULL) {

				uscan_access(ut->ut_pproxy);
				uscan_forloop(ut->ut_pproxy, ut) {
					if (ut->ut_id != tid) {
						continue;
					}
					break;
				}

				if (!ut) {
					uscan_unlock(ut->ut_pproxy);
					return ESRCH;
				}
			}

			/* Copy current scheduling attrs.
			 */
			if (uap->value4) {
				oprs.prs_policy = ut->ut_policy;
				oprs.prs_priority =
					(oprs.prs_policy == SCHED_TS)
					? ut->ut_pproxy->prxy_sched.prs_nice
					: UT_TO_KT(ut)->k_basepri;
			}

			/* Change scheduling attrs.
			 */
			if (uap->value3
			    && !(error = uthread_sched_setscheduler(ut, &prs))){
				int s = ut_lock(ut);
				ut->ut_flags &= ~UT_PTPSCOPE;
				ut_unlock(ut, s);
			}
			if (tid != UT_ID_NULL)
				uscan_unlock(ut->ut_pproxy);

			/* Write out current scheduling attrs.
			 */
			if (uap->value4 && !error
			    && copyout(&oprs,
			   		 (caddr_t)uap->value4, sizeof(oprs))){
				error = EFAULT;
			}
		}
		break;

		default:
			return EINVAL;
		}

		break;
	}

	default:
		return EINVAL;
	}
	return error;
}

/*
 * procblk - blocking calls
 */
struct procblka {
	usysarg_t	id;
	sysarg_t	pid;
	sysarg_t	count;
};

int
procblk(struct procblka *uap)
{
	vproc_t *vpr;
	int isself = 0;
	int error = 0;

	if (uap->id > 5)
		return EINVAL;

	/* If a set command, then check the count for valid range */
	if ((uap->id == 2 || uap->id == 5) &&
	    (uap->count > PR_MAXBLOCKCNT || uap->count < PR_MINBLOCKCNT))
		return EINVAL;

	if (current_pid() == uap->pid) {
		vpr = curvprocp;
		isself = 1;
	} else if ((vpr = VPROC_LOOKUP(uap->pid)) == NULL)
		return ESRCH;

	VPROC_PROCBLK(vpr, uap->id, uap->count, get_current_cred(),
			isself, error);

	if (!isself)
		VPROC_RELE(vpr);

	return error;
}

void
blockme(void)
{
	uthread_t *ut = curuthread;
	ssleep_t *st = &ut->ut_pblock;
	int s = ut_lock(ut);

	/*
	 * check for fatalsigs after locking ut_lock since otherwise
	 * there is a window where we check for fatal signal, get
	 * interrupted and someone sends us a signal, posts it, checks
	 * if we are sleeping on this ssleep_t s_wait, which fails, then we
	 * execute this code
	 */
	if (isfatalsig(ut, (sigvec_t *)NULL)) {
		ut_unlock(ut, s);
		return;
	}

	while (st->s_waitcnt > 0) {
		st->s_waitcnt--;
		/* I should be asleep */
		if (st->s_slpcnt > 0)
			st->s_slpcnt--;
		else {
			ut_sleep(ut, &st->s_wait, PZERO|PLTWAIT, s);
			s = ut_lock(ut);
		}
	}
	ut_unlock(ut, s);
}

static void
blockself(void)
{
	uthread_t *ut = curuthread;
	ssleep_t *st = &ut->ut_pblock;
	int s = ut_lock(ut);

	/*
	 * check for fatalsigs after locking ut_lock since otherwise
	 * there is a window where we check for fatal signal, get
	 * interrupted and someone sends us a signal, posts it, checks
	 * if we are sleeping on this ssleep_t s_wait, which fails, then we
	 * execute this code
	 */
	if (isfatalsig(ut, (sigvec_t *)NULL)) {
		ut_unlock(ut, s);
		return;
	}

	if (st->s_slpcnt > 0)
		st->s_slpcnt--;
	else {
		ut_sleep(ut, &st->s_wait, PZERO|PLTWAIT, s);
		s = ut_lock(ut);
	}

	ut_unlock(ut, s);
}

/*
 * blockset - do actual blocking
 * called with ut_lock locked 
 * returns unlocked.
 */
void
blockset(
	uthread_t	*ut,
	int		cnt,
	int		bump,
	int		s)
{
	ssleep_t *st = &ut->ut_pblock;

	ASSERT(ut_islocked(ut));

	/* if not bumping then set everything according to 'cnt' first,
	 * then wake up potential sleeper.
	 */
	if (!bump) {
		/* zero out any existing waits/wakes */
		if (cnt < 0) {
			st->s_slpcnt = 0;
			st->s_waitcnt = -cnt;
			ut->ut_flags |= UT_BLOCK;
		} else if (cnt > 0) {
			st->s_waitcnt = 0;
			st->s_slpcnt = cnt;
		} else
			st->s_waitcnt = st->s_slpcnt = 0;

		/* wake up thread if blocked on s_wait */
		if (UT_TO_KT(ut)->k_wchan == (caddr_t)&ut->ut_pblock.s_wait)
			setrun(UT_TO_KT(ut));

		ut_unlock(ut, s);

		return;
	}

	if (cnt < 0) {
		/* block the process */
		st->s_waitcnt += -cnt;
		if (st->s_waitcnt == st->s_slpcnt)
			st->s_waitcnt = st->s_slpcnt = 0;
		else
			ut->ut_flags |= UT_BLOCK;
	} else if (cnt > 0) {
		/*
		 * Unblock process if it is blocked;
		 * either way, account for the number
		 * of logical unblock futures ut gets --
		 * "cnt-1" if a real unblock occurred.
		 */
		if (UT_TO_KT(ut)->k_wchan == (caddr_t)&ut->ut_pblock.s_wait) {
			st->s_slpcnt += cnt - 1;
			setrun(UT_TO_KT(ut));
		} else
			st->s_slpcnt += cnt;
	}
	ut_unlock(ut, s);
}
/*
 * blockcnt - get the effective block count for a thread
 */
void
blockcnt(
	uthread_t *ut,
	short *blkcnt,
	uchar_t *isblocked)
{
	ssleep_t *st;
	int s;

	*blkcnt = 0;
	*isblocked = 0;

	st = &ut->ut_pblock;

	s = ut_lock(ut);

	*blkcnt = st->s_waitcnt - st->s_slpcnt;
	if (*isblocked = sv_waitq(&st->s_wait))
		(*blkcnt)++;

	ut_unlock(ut, s);
}
