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

#ident "$Revision: 1.61 $"

#include <sys/types.h>
#include <ksys/as.h>
#include <sys/capability.h>
#include <sys/proc.h>
#include <ksys/vproc.h>
#include <ksys/cred.h>
#include <sys/errno.h>
#include <sys/prctl.h>
#include <sys/runq.h>
#include <sys/sched.h>
#include <sys/schedctl.h>
#include <sys/space.h>
#include <ksys/vsession.h>
#include <sys/times.h>
#include <sys/timers.h>
#include <sys/vnode.h>
#include <sys/sat.h>
#include <sys/mac_label.h>
#include "pproc.h"
#include "pproc_private.h"

#define TSPRItoNICE(p)	(2*NZERO - (p))
#define NICEtoTSPRI(p)	(2*NZERO - (p))
#define NICEMAX		(2*NZERO - 1)

/* Set nice value of a process to *nice. Return old nice value in *nice */
int
pproc_set_nice(
	bhv_desc_t	*bhv,
	int		*nice,
	cred_t		*scred,
	int		flags)
{
	proc_t	*p;

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);

	return proc_set_nice(p, nice, scred, flags);
}

int
proc_set_nice(
	proc_t	*p,
	int	*nice,
	cred_t	*scred,
	int	flags)
{
	int	s;
	kthread_t *kt;
	int	prs_nice;

	/* Note:
	 * prs_nice adheres to the SCHED_TS range
	 * nice values run from 0..2*NZERO-1
	 */
	if (flags & VNICE_INCR) {

		/* Semantics to accommodate the SVR4 nice syscall
		 *
		 * nice is an increment
		 */

		if ((*nice < 0 || *nice > 2*NZERO) && !_CAP_ABLE(CAP_SCHED_MGT))
			return EPERM;

		uscan_update(&p->p_proxy);

		/* Compute new priority within TS bounds */
		prs_nice = p->p_proxy.prxy_sched.prs_nice - *nice;
		if (prs_nice > TS_PRIO_MAX)
			prs_nice = TS_PRIO_MAX;
		else if (prs_nice < TS_PRIO_MIN)
			prs_nice = TS_PRIO_MIN;

		/* return new nice value */
		*nice = TSPRItoNICE(prs_nice);

	} else {
		/* Semantics to accommodate the BSD setpriority syscall.
		 * This also implements the sgi-specific schedctl(RENICE)
		 * call, which is similar to BSD setpriority.
		 *
		 * nice is an absolute value
		 */

		/* Compute new priority within TS bounds */
		if (*nice < 0)
			prs_nice = TS_PRIO_MAX;
		else if (*nice > NICEMAX)
			prs_nice = NICEtoTSPRI(NICEMAX);
		else
			prs_nice = NICEtoTSPRI(*nice);

		/*
		 * Perform permissions check (requiring credentials lock)
		 * before taking uscan lock for update to avoid deadlocking
		 * with setuid(), pv:554331.
		 */
		if (!hasprocperm(p, scred))
			return EPERM;

		uscan_update(&p->p_proxy);
		if (prs_nice > p->p_proxy.prxy_sched.prs_nice
		        && !_CAP_CRABLE(scred, CAP_SCHED_MGT)) {
			uscan_unlock(&p->p_proxy);
			return EPERM;
		}

		/* return old nice value */
		*nice = TSPRItoNICE(p->p_proxy.prxy_sched.prs_nice);
	}

	/* Fill in new priority.
	 * Nice values do not apply to multi-threaded procs.
	 * TS processes running at minimum priority are made weightless.
	 */
	p->p_proxy.prxy_sched.prs_nice = prs_nice;
	if (IS_THREADED(&p->p_proxy)) {	/* protect MT procs */
		uscan_unlock(&p->p_proxy);
		return 0;
	}

	kt = UT_TO_KT(prxy_to_thread(&p->p_proxy));
	if (kt) {	/* guard against defunct processes */
		s = kt_lock(kt);
		if (prxy_to_thread(&p->p_proxy)->ut_policy == SCHED_TS) {
			kt_initialize_pri(kt, PTIME_SHARE);
			wt_set_weight(kt, prs_nice);
		}
		kt_unlock(kt, s);
	}

	uscan_unlock(&p->p_proxy);
	return 0;
}

/*
 * return the nice value of a process
 */
int
pproc_get_nice(
	bhv_desc_t	*bhv,
	int		*nice,
	cred_t		*scred)
{
	proc_t	*p;

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);

	return proc_get_nice(p, nice, scred);
}

int
proc_get_nice(  
	proc_t	*p,
	int	*nice, 
	cred_t	*scred)
{
	if (!hasprocperm(p, scred))
		return EPERM;

	/* Semantics to accommodate the BSD getpriority syscall.
	 */
	*nice = TSPRItoNICE(p->p_proxy.prxy_sched.prs_nice);

	return 0;
}

/*
 * Set directory of a process.
 */
void
pproc_set_dir(
	bhv_desc_t	*bhv,
	vnode_t		*vp,	
	vnode_t		**vpp,	/* IN/OUT */
	int		flag)
{
	proc_t		*p = BHV_TO_PROC(bhv);

	if (p->p_proxy.prxy_shmask & PR_SDIR) {
		/*
		 * Hold vp for the shared block so that if this process/thread
		 * goes away before others have synced, there is still a
		 * reference to it 
		 */
		VN_HOLD(vp);

		/*
		 * Add to all shared processes (if we're sharing dirs)
		 */
		if (IS_SPROC(&p->p_proxy)) {
			shaddr_t *sa = p->p_shaddr;
			mutex_lock(&sa->s_fupdsema, 0);
			if (*vpp) {
				/*
				 * If *vpp was shared, also give back
				 * shared block's ref
				 */
				ASSERT((*vpp)->v_count >= 2);
				VN_RELE(*vpp);
			}
			if (flag & VDIR_ROOT) {
				sa->s_rdir = vp;
			} else {
				sa->s_cdir = vp;
			}
			mutex_unlock(&sa->s_fupdsema);
			setshdsync(sa, p, PR_SDIR, UT_UPDDIR);
		} else {
			pshare_t *ps = p->p_proxy.prxy_utshare;
			mutex_lock(&p->p_proxy.prxy_utshare->ps_fupdsema, 0);
			if (*vpp) {
				/*
				 * If *vpp was shared, also give back
				 * shared block's ref
				 */
				ASSERT((*vpp)->v_count >= 2);
				VN_RELE(*vpp);
			}
			if (flag & VDIR_ROOT) {
				ps->ps_rdir = vp;
			} else {
				ps->ps_cdir = vp;
			}
			mutex_unlock(&p->p_proxy.prxy_utshare->ps_fupdsema);
			setpsync(&p->p_proxy, UT_UPDDIR);
		}
	}

	if (*vpp)
		VN_RELE(*vpp);
	*vpp = vp;
}

/*
 * Set umask.
 */
int
pproc_umask(
	bhv_desc_t	*bhv,
	short		cmask,
	short		*omask)	/* IN/OUT */
{
	proc_t	*p;
	int s;

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);

	if (p->p_proxy.prxy_shmask & PR_SUMASK) {
		/*
		 * Push new cmask to all shared processes
		 */
		if (IS_SPROC(&p->p_proxy)) {
			shaddr_t *sa = p->p_shaddr;
			s = mutex_spinlock(&sa->s_rupdlock);
			*omask = sa->s_cmask;
			sa->s_cmask = cmask;
			mutex_spinunlock(&sa->s_rupdlock, s);

			setshdsync(sa, p, PR_SUMASK, UT_UPDUMASK);
		} else {
			/*
			 * Push new cmask to all p's uthreads
			 */
			pshare_t *ps = p->p_proxy.prxy_utshare;
			s = mutex_spinlock(&ps->ps_rupdlock);
			*omask = ps->ps_cmask;
			ps->ps_cmask = cmask;
			mutex_spinunlock(&ps->ps_rupdlock, s);

			/*
			 * Have to release ps_rupdlock before calling
			 * setpsync -- it could sleep.
			 */
			setpsync(&p->p_proxy, UT_UPDUMASK);
		}
	}

	return(0);
}

/*
 * Physical implementation of VPROC_GET_PROC.
 */
/* ARGSUSED */
void
pproc_get_proc(
	bhv_desc_t	*bhv,
	proc_t		**pp,
	int		local_only)
{
	*pp = BHV_TO_PROC(bhv);
}

/*
 * Physical implementation of VPROC_GET_ATTR.
 */
void
pproc_get_attr(
	bhv_desc_t	*bhv,
	int		flags,
	vp_get_attr_t	*attr)
{
	proc_t	*p = BHV_TO_PROC(bhv);

	if (flags & VGATTR_PPID)
		attr->va_ppid = p->p_ppid;
	if (flags & VGATTR_PGID)
		attr->va_pgid = p->p_pgid;
	if (flags & VGATTR_SID)
		attr->va_sid  = p->p_sid;
	if (flags & VGATTR_HAS_CTTY)
		attr->va_has_ctty = !(p->p_flag & SNOCTTY);
	if (flags & VGATTR_SIGNATURE)
		attr->va_signature = p->p_start;

	if ((flags & VGATTR_UID) || (flags & VGATTR_GID)) {
		cred_t *cr = pcred_access(p);
		if (flags & VGATTR_UID)
			attr->va_uid = cr->cr_uid;
		if (flags & VGATTR_GID)
			attr->va_gid = cr->cr_gid;
		pcred_unaccess(p);
	}
	
	if (flags & VGATTR_CELLID)
	        attr->va_cellid = cellid();
}

/*
 * Physical implementation of VPROC_GET_XATTR. Most of the information
 * is what prgetpsinfo() would return.
 */
void
pproc_get_xattr(
	bhv_desc_t	*bhv,
	int		flags,
	vp_get_xattr_t	*attr)
{
	timespec_t	utime, stime;
	as_getasattr_t 	asattr;
	pgcnt_t		size, rss;
	vasid_t		vasid;
	proc_t		*p = BHV_TO_PROC(bhv);
	uthread_t 	*ut;

	pproc_get_attr(bhv, flags, &attr->xa_basic);
	if (flags & VGXATTR_NAME) {
		bcopy(p->p_comm, attr->xa_name, 
					min(sizeof(p->p_comm), PSCOMSIZ));
		bcopy(p->p_psargs, attr->xa_args, 
					min(sizeof(p->p_psargs), PSARGSZ));
	}
	if (flags & VGXATTR_STAT) {
		attr->xa_pflag = p->p_flag;
		attr->xa_stat = p->p_stat;
	}
	if (flags & VGXATTR_NICE)
		attr->xa_nice = 2*NZERO - p->p_proxy.prxy_sched.prs_nice;
	if (flags & VGXATTR_UID) {
		/*
		 * Better call this ZNO, else we can be reading random
		 * memory off p_cred, though we can not be dereferencing
		 * a NULL pointer via p_cred.
		 */
		cred_t *cr = pcred_access(p);
		attr->xa_realuid = cr->cr_ruid;
		attr->xa_saveuid = cr->cr_suid;
		attr->xa_basic.va_uid = cr->cr_uid;
		pcred_unaccess(p);
	}

	/*
	 * Prevent uthread(s) from detaching. Check if we got one.
	 */
	uscan_hold(&p->p_proxy);
	if ((ut = prxy_to_thread(&p->p_proxy)) == NULL) {
		uscan_rele(&p->p_proxy);
		attr->xa_size = attr->xa_stksz = attr->xa_rss = 
			attr->xa_pri = attr->xa_uflag = 0;
		attr->xa_lcpu = -1;
		attr->xa_slpchan = 0;
		bzero(&attr->xa_utime, sizeof(attr->xa_utime));
		bzero(&attr->xa_stime, sizeof(attr->xa_stime));
		return;
	}

	if (flags & (VGXATTR_SIZE|VGXATTR_RSS)) {
		as_getassize(ut->ut_asid, &size, &rss);
		if (flags & VGXATTR_RSS)
			attr->xa_rss = rss;
		if (flags & VGXATTR_SIZE) {
			attr->xa_size = size;
			if (as_lookup(ut->ut_asid, &vasid)) {
				attr->xa_stksz = 0;
			} else {
				VAS_GETASATTR(vasid, AS_STKSIZE, &asattr);
				as_rele(vasid);
				attr->xa_stksz = asattr.as_stksize;
			}
		}
	}
	if (flags & VGXATTR_WCHAN)
		attr->xa_slpchan = UT_TO_KT(ut)->k_wchan;
	if (flags & VGXATTR_PRI) {
		attr->xa_pri = kt_pri(UT_TO_KT(ut));
		if (attr->xa_pri < 0) attr->xa_pri = 
						p->p_proxy.prxy_sched.prs_nice;
	}
	if (flags & VGXATTR_LCPU)
		attr->xa_lcpu = UT_TO_KT(ut)->k_lastrun;
	if (flags & VGXATTR_STAT)
		attr->xa_uflag = ut->ut_flags;
	if (flags & VGXATTR_UST) {
		VPROC_READ_US_TIMERS(PROC_TO_VPROC(p), &utime, &stime);
		TIMESPEC_TO_TIMEVAL(&utime, &attr->xa_utime);
		TIMESPEC_TO_TIMEVAL(&stime, &attr->xa_stime);
	}
	uscan_rele(&p->p_proxy);
}

void
pproc_getpagg(
	bhv_desc_t	*bhv,
	vpagg_t	**vpagpp)
{
	proc_t	*p = BHV_TO_PROC(bhv);

	*vpagpp = p->p_vpagg;
}

void
pproc_setpagg(
	bhv_desc_t	*bhv,
	vpagg_t	*vpag)
{
	proc_t	*p = BHV_TO_PROC(bhv);

	p->p_vpagg = vpag;
}

int
pproc_setrlimit(
	bhv_desc_t	*bhv,
	int		which,
	struct rlimit	*newlim)
{
	proc_t	*p = BHV_TO_PROC(bhv);
	struct rlimit *rlim;
	vasid_t vasid;
	as_setattr_t setattr;
	int s;

	ASSERT(which >= 0 && which < RLIM_NLIMITS);

	rlim = &p->p_rlimit[which];
	if (newlim->rlim_cur > rlim->rlim_max ||
	    newlim->rlim_max > rlim->rlim_max)
		if (!_CAP_CRABLE(p->p_cred, CAP_PROC_MGT))
			return EPERM;

	/*
	 * For process segment sizes, we could check against MAXUMEM...
	 */
	switch (which) {
	case RLIMIT_FSIZE:
		if (IS_SPROC(&p->p_proxy) &&
		    (p->p_proxy.prxy_shmask & PR_SULIMIT)) {
			shaddr_t	*sa;

			sa = p->p_shaddr;
			s = mutex_spinlock(&sa->s_rupdlock);
			sa->s_limit = newlim->rlim_cur;
			setshdsync(sa, p, PR_SULIMIT, UT_UPDULIMIT);
			mutex_spinunlock(&sa->s_rupdlock, s);
		}
		break;
	case RLIMIT_RSS:
	case RLIMIT_VMEM:
	case RLIMIT_DATA:
	case RLIMIT_STACK:
		as_lookup_current(&vasid);
		if (which == RLIMIT_STACK)
			setattr.as_op = AS_SET_STKMAX;
		else if (which == RLIMIT_RSS)
			setattr.as_op = AS_SET_RSSMAX;
		else if (which == RLIMIT_DATA)
			setattr.as_op = AS_SET_DATAMAX;
		else
			setattr.as_op = AS_SET_VMEMMAX;
		setattr.as_set_max = newlim->rlim_cur;
		VAS_SETATTR(vasid, 0, &setattr);
		break;
	}

	*rlim = *newlim;

	return 0;
}

void
pproc_getrlimit(
	bhv_desc_t	*bhv,
	int		which,
	struct rlimit	*lim)
{
	proc_t	*p = BHV_TO_PROC(bhv);
	uthread_t *ut;
	struct rlimit *rlimp;

	ASSERT(which >= 0 && which < RLIM_NLIMITS);

	uscan_hold(&p->p_proxy);
	ut = prxy_to_thread(&p->p_proxy);
	rlimp = &p->p_rlimit[which];

	switch (which) {
	case RLIMIT_VMEM:
	case RLIMIT_DATA:
	case RLIMIT_STACK:
	case RLIMIT_RSS:
		rlimp->rlim_cur = getaslimit(ut, which);
		break;
	default:
		break;
	}
	uscan_rele(&p->p_proxy);

	*lim = *rlimp;
}

/* The 'times' system call */

struct ustimes {
	timespec_t	us_utime;
	timespec_t	us_stime;
};

/* Get the user and sys time of this process.
 * return these times as summed timespec_t's
 */
static int
uthread_us_time(uthread_t *ut, void *arg)
{
	struct ustimes *accum = arg;
	kthread_t *kt = UT_TO_KT(ut);
	timespec_t timespec;

	ktimer_read(kt, AS_USR_RUN, &timespec);
	timespec_add(&accum->us_utime, &timespec);

	ktimer_read(kt, AS_SYS_RUN, &timespec);
	timespec_add(&accum->us_stime, &timespec);

	return 0;
}

/* Get all the times of the process 
 * return these times as summed timespec_t's
 */
static int
uthread_all_times(uthread_t *ut, void *arg)
{
	timespec_t	*ptime = arg;
	kthread_t *kt = UT_TO_KT(ut);
	timespec_t timespec; 
	register int i;        

	for (i = 0; i < MAX_PROCTIMER; i++) { 
		ktimer_read(kt, i, &timespec);
		timespec_add(ptime+i, &timespec);        
	}

	return 0;
}

void
pproc_times(
	bhv_desc_t	*bhv,
	struct tms	*rtime)
{
	proc_t		*p;
	struct ustimes	accum;

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);

	bzero(&accum, sizeof(accum));
	(void)uthread_apply(&p->p_proxy, UT_ID_NULL, uthread_us_time, &accum);

	/*
	 * Don't be tempted to use timespec_to_tick -- it rounds up
	 * even when nanoseconds are an even number of ticks.
	 */
	rtime->tms_utime = (NSEC_PER_SEC/NSEC_PER_TICK * accum.us_utime.tv_sec)
		+ (accum.us_utime.tv_nsec + NSEC_PER_TICK-1) / NSEC_PER_TICK;

	rtime->tms_stime = (NSEC_PER_SEC/NSEC_PER_TICK * accum.us_stime.tv_sec)
		+ (accum.us_stime.tv_nsec + NSEC_PER_TICK-1) / NSEC_PER_TICK;

	/* u_cru.ru_utime, u_cru.ru_stime are maintained in timeval units */
	rtime->tms_cutime = (clock_t) hzto(&p->p_cru.ru_utime);
	rtime->tms_cstime = (clock_t) hzto(&p->p_cru.ru_stime);
}

void
pproc_read_us_timers(
	bhv_desc_t	*bhv,
	timespec_t	*utimep,
	timespec_t	*stimep)
{
	proc_t		*p;
	struct ustimes	accum;

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);

	bzero(&accum, sizeof(accum));
	(void)uthread_apply(&p->p_proxy, UT_ID_NULL, uthread_us_time, &accum);

	/* Structure assignment */
	*utimep = accum.us_utime;
	*stimep = accum.us_stime;
}


void
pproc_read_all_timers(
	bhv_desc_t	*bhv,
	timespec_t	*timep)
{
	proc_t		*p;

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);

	(void)uthread_apply(&p->p_proxy, UT_ID_NULL, uthread_all_times,timep);

}

void
pproc_getrusage(
	bhv_desc_t	*bhv,
	int		flags,
	struct	rusage	*rup)
{
	proc_t		*p;
	struct ustimes	accum;

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);

	/* can either get this process's rusage, or the accrued rusage
	 * of waited-on children (p->p_cru)
	 */
	if (flags & VRUSAGE_SELF) {
		/* Must accumulate thread's timespecs into rusage struct */
		bzero(&accum, sizeof(accum));
		(void)uthread_apply(&p->p_proxy, UT_ID_NULL,
				    uthread_us_time, &accum);

		/*
		 * First, we copy accumulated event counters into
		 * caller's rusage structure, then calculate user
		 * and system times.
		 * This code had previously added the user/system time into
		 * the proxy structure first, then copied the entire proxy
		 * rusage -- but this is a mistake since the proxy rusage
		 * struct is an accumulator, and the caller's u/stimes will
		 * _again_ get added to the proxy ru_{us}time when it exits.
		 */
		bcopy(&p->p_proxy.prxy_ru, rup, sizeof(struct rusage));

		/* Convert from timespec to timeval */
		TIMESPEC_TO_TIMEVAL(&accum.us_utime, &rup->ru_utime);
		TIMESPEC_TO_TIMEVAL(&accum.us_stime, &rup->ru_stime);
	} else {
		bcopy(&p->p_cru, rup, sizeof(struct rusage));
	}
}

void
pproc_get_extacct(
	bhv_desc_t	*bhv,
	proc_acct_t	*acct)
{
	proc_t		*p;

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);

	bzero(acct, sizeof(proc_acct_t));
	(void) uthread_apply(&p->p_proxy, UT_ID_NULL, uthread_acct, acct);
}

/* Get the timers that an 'acct_timers_t' structure is interested
 * in.  Return these timers as accumulators - which is the elapsed
 * time in nanosecond units.
 */
static int
uthread_accum_timers(uthread_t *ut, void *arg)
{
	acct_timers_t	*timers = arg;
	kthread_t *kt = UT_TO_KT(ut);
	timespec_t timespec;

	ktimer_read(kt, AS_USR_RUN, &timespec);
	timers->ac_utime += TIMESPEC_TO_ACCUM(&timespec);

	ktimer_read(kt, AS_SYS_RUN, &timespec);
	timers->ac_stime += TIMESPEC_TO_ACCUM(&timespec);

	ktimer_read(kt, AS_BIO_WAIT, &timespec);
	timers->ac_bwtime += TIMESPEC_TO_ACCUM(&timespec);

	ktimer_read(kt, AS_PHYSIO_WAIT, &timespec);
	timers->ac_rwtime += TIMESPEC_TO_ACCUM(&timespec);

	ktimer_read(kt, AS_RUNQ_WAIT, &timespec);
	timers->ac_qwtime += TIMESPEC_TO_ACCUM(&timespec);

	return 0;
}

/* Called by prioctl(PIOCACINFO) and shadowacct */
void
pproc_get_acct_timers(
	bhv_desc_t	*bhv,
	acct_timers_t	*timers)
{
	proc_t		*p;

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);

	bzero(timers, sizeof(*timers));
	(void)uthread_apply(&p->p_proxy, UT_ID_NULL,
			    uthread_accum_timers, timers);
}

void
pproc_setinfo_runq(
	bhv_desc_t	*bhv,
	int		rqtype,
	int		arg,
	__int64_t	*rval)
{
	proc_t		*p;
	uthread_t	*ut;

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);

	if (IS_THREADED(&p->p_proxy)) {	/* protect MT procs */
		return;
	}
	ut = prxy_to_thread(&p->p_proxy);
	*rval = setinfoRunq(UT_TO_KT(ut), &ut->ut_pproxy->prxy_sched,
				rqtype, arg);
}

void
pproc_getrtpri(
	bhv_desc_t	*bhv,
	__int64_t	*rval)
{
	proc_t	*p;

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);

	if (p->p_proxy.prxy_shmask & PR_THREADS) {	/* protect MT procs */
		return;
	}

	/* XXX - this only gets the 1st thread */
	*rval = kt_rtpri(UT_TO_KT(prxy_to_thread(&p->p_proxy)));
}

void
pproc_setmaster(
	bhv_desc_t	*bhv,
	pid_t		callers_pid,
	int		*error)
{
	proc_t		*p;
	proc_t		*sp;
	shaddr_t	*sa;
	int		s;

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);

	/* A process can set mastership if it is the
	 * current master, or there is no current master.
	 * No current master is indicated by s_master == 0
	 */
	if ((sa = p->p_shaddr) == NULL ||
	     sa->s_master && sa->s_master != callers_pid) {
		*error = EINVAL;
		return;
	}


	/*
	 * caller of setmaster must actually be in the share
	 * group that it is trying to set the master of.
	 * scan the list looking for the callers pid.
	 */
	mutex_lock(&sa->s_listlock, 0);
	for (sp = sa->s_plink; sp; sp = sp->p_slink) {
		if (sp->p_pid == callers_pid)
			break;
	}

	mutex_unlock(&sa->s_listlock);

	if (!sp) {
		*error = EINVAL;
		return;
	}

	s = mutex_spinlock(&sa->s_rupdlock);
	/* New master */
	sa->s_master = p->p_pid;
	mutex_spinunlock(&sa->s_rupdlock, s);

	*error = 0;
}

void
pproc_schedmode(
	bhv_desc_t	*bhv,
	int		arg,
	__int64_t	*rval,
	int		*error)
{
	proc_t		*p;
	shaddr_t	*sa;
	int		s;
	int 		is_batch;

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);

	ASSERT(arg >= SGS_FREE && arg <= SGS_GANG);
	
	if (!IS_SPROC(&p->p_proxy)) {
		*error = EINVAL;
		return;
	}
	if (p->p_vpgrp) {
                VPGRP_HOLD(p->p_vpgrp);
                VPGRP_GETATTR(p->p_vpgrp, NULL, NULL, &is_batch);
                if (is_batch) {
                        VPGRP_RELE(p->p_vpgrp);
			*error = 0;
			return;
                }
                VPGRP_RELE(p->p_vpgrp);
        }

	sa = p->p_shaddr;

	s = mutex_spinlock(&sa->s_rupdlock);

	*rval = sa->s_sched;
	sa->s_sched = (unchar) arg;
	if (prxy_to_thread(&p->p_proxy)->ut_prda)
		prxy_to_thread(&p->p_proxy)->ut_prda->t_sys.t_hint = arg;
	mutex_spinunlock(&sa->s_rupdlock, s);

	*error = 0;
}

void
pproc_sched_aff(
	bhv_desc_t	*bhv,
	int		flag,
	__int64_t	*rval)
{
	proc_t		*p = BHV_TO_PROC(bhv);
	uthread_t 	*ut;
	proc_proxy_t	*prxy = &p->p_proxy;

	ASSERT(p != NULL);
	if (flag == VSCHED_AFF_ON) {

		uscan_update(prxy);
		uscan_forloop(prxy, ut) {
			int s = ut_lock(ut);
			UT_TO_KT(ut)->k_flags &= ~KT_NOAFF;
			ut_unlock(ut, s);
		}
		uscan_unlock(prxy);
		prs_flagclr(&p->p_proxy.prxy_sched, PRSNOAFF);	

	} else if (flag == VSCHED_AFF_OFF) {

		uscan_update(prxy);
		uscan_forloop(prxy, ut) {
			int s = ut_lock(ut);
			UT_TO_KT(ut)->k_flags |= KT_NOAFF;
			UT_TO_KT(ut)->k_flags &= ~KT_HOLD;
			ut_unlock(ut, s);
		}
		uscan_unlock(prxy);
		prs_flagset(&p->p_proxy.prxy_sched, PRSNOAFF);

	} else {
		ASSERT(flag == VSCHED_AFF_GET);
		*rval = p->p_proxy.prxy_sched.prs_flags & PRSNOAFF ? 1 : 0;
	}
}

int
pproc_getcomm(
	bhv_desc_t	*bhv,
	cred_t		*cr,
	char		*bufp,
	size_t		len)
{
	proc_t		*p;
	int		error = 0;

	p = BHV_TO_PROC(bhv);
	ASSERT(p != NULL);

	if (_MAC_ACCESS(p->p_cred->cr_mac, cr, MACREAD)) {
		error = EACCES;
	} else {
		ASSERT(len <= PSCOMSIZ);
		bcopy(p->p_comm, bufp, len);
	}
	_SAT_PROC_ACCESS(SAT_PROC_ATTR_READ, p->p_pid, p->p_cred, error, -1);
	return error;
}
