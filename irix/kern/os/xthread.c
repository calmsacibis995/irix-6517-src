/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994-1996 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.26 $"

#include <sys/types.h>
#include <sys/atomic_ops.h>
#include <sys/cred.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/kthread.h>
#include <sys/ksignal.h>
#include <sys/pda.h>
#include <sys/rtmon.h>
#include <sys/rt.h>
#include <sys/runq.h>
#include <sys/schedctl.h>
#include <sys/systm.h>
#if CELL
#include <ksys/cell/remote.h>
#endif
#include <ksys/xthread.h>
#include <ksys/isthread.h>
#include <sys/proc.h>

/*
 * Interrupt Threads.
 * These can run most low level code - filesystems, networking, device drivers.
 * They are optimized for use by device driver interrupt handlers.
 *
 * - They don't handle signals.
 * - They can be traced with par (currently show as proc 0 ..)
 * - fuser can't find them.
 *
 * TODO (Potentially)
 * - really fix par to handle kthread ids
 * - /proc/xthread interface
 */

xthread_t xthreadlist;		/* the thread list */
static zone_t *xt_zone;		/* xthreads allocation pool */
lock_t xt_list_lock;		/* controls list */
static kt_ops_t xthreadops;

#ifdef ITHREAD_LATENCY
zone_t *xt_latzone;	/* latency stats allocation pool */
#endif

void
xthread_init(void)
{
	xt_zone = kmem_zone_init(sizeof(xthread_t), "xthread");
#ifdef ITHREAD_LATENCY
	xt_latzone = kmem_zone_init(sizeof(xt_latency_t), "intrlatency");
#endif
	spinlock_init(&xt_list_lock, "xt_list_lock");
	xthreadlist.xt_next = xthreadlist.xt_prev = &xthreadlist;
}

/*
 * Create and initialize a new xthread and return a pointer to it.  Normally
 * we set it on the run queue just before returning but passing in KT_NPRMPT
 * in the "flags" argument will prevent this.  If KT_NPRMPT is passed, then it
 * is the callers responsibility to put the newly created thread on the run
 * queue.  Note that use of KT_NPRMPT is just a flag to xthread_create() and
 * that flag is not set in the new thread's k_flags field.
 *
 * Each time the thread starts running, it will start executing the function
 * "func" and "arg0" will be passed to it.  When the thread calls ipsema()
 * it will suspend if this results in the semaphore count going less than
 * zero.  Regardless of whether the thread suspends or not, execution will
 * resume at "func" via a long jump.
 */
/* ARGSUSED */
void *
xthread_create(char *name,
		caddr_t stack_addr,
		uint_t stack_size,
		uint_t flags,
		uint_t pri,
		uint_t schedflags,
		xt_func_t func,
		void *arg0)
{
	xthread_t *xt;
	kthread_t *kt;
	int s;

	/* XXX will need to have perhaps a pool or no-sleep options .. */
	xt = kmem_zone_zalloc(xt_zone, KM_SLEEP|VM_DIRECT);
	ASSERT(((__psint_t)xt & 0x7) == 0); /* must be double aligned */
	xt->xt_cred = sys_cred;
	if (name)
		strncpy(xt->xt_name, name, sizeof(xt->xt_name));

	kt = XT_TO_KT(xt);
	kthread_init(kt, stack_addr,
		stack_size, KT_XTHREAD, pri, schedflags, &xthreadops);

	s = mutex_spinlock(&xt_list_lock);
	/* add to list */
	xthreadlist.xt_next->xt_prev = xt;
	xt->xt_next = xthreadlist.xt_next;
	xthreadlist.xt_next = xt;
	xt->xt_prev = &xthreadlist;
	mutex_spinunlock(&xt_list_lock, s);

	/* xthread specific stuff */
	if (flags & KT_STACK_MALLOC)
		kt->k_flags |= KT_STACK_MALLOC;
	kt->k_flags |= KT_ISEMA;
	xt->xt_sp = (caddr_t)(kt->k_regs[PCB_SP]);

#ifdef ITHREAD_LATENCY
	xt->xt_latstats = kmem_zone_zalloc(xt_latzone, KM_SLEEP);
	xt->xt_latstats->ls_min = ~0;
#endif

	/*
	 * Initialize xthread state and place on runq (if KT_NPRMPT wasn't
	 * specified.
	 */
	s = kt_lock(kt);
	xt->xt_func = func;
	xt->xt_arg = arg0;
	xt->xt_arg2 = 0;
	if (kt->k_flags & KT_BIND)
		start_rt(kt, kt->k_pri);
	ktimer_init(kt, AS_RUNQ_WAIT);
	if (!(flags & KT_NPRMPT))
		putrunq(kt, CPU_NONE);
	kt_unlock(kt, s);
	return xt;
}

void
xthread_setup(char *name, int pri, thd_int_t *tip, xt_func_t *func, void *arg)
{
	void *itp;
	kthread_t *kt;
	int s;

	ASSERT(tip->thd_flags & THD_REG);  /* We've been requested */

	/*
	 * If we already have a thread going, don't bother to create a
	 * new one.
	 */
	if (tip->thd_flags & THD_OK)
		return;

	/*
	 * If we trip the following ASSERT() an old thread is still
	 * around, but isn't useable.
	 */
	ASSERT(!(tip->thd_flags & THD_EXIST));

	/*
	 * Create and initialize the xthread.  We defer putting the new
	 * xthread onto the run queue till we've got everything initialized.
	 * If a caller to xthread_setup() ever needs the set-on-runq operation
	 * defered even further we'll have to come up with some way to flag
	 * that ...
	 */
	initnsema(&tip->thd_isync, 0, "thd_isync");
	itp = xthread_create(name, 0, KTHREAD_DEF_STACKSZ, KT_NPRMPT,
			     pri, KT_PS, func, arg);
	tip->thd_xthread = itp;
#ifdef ITHREAD_LATENCY
	tip->thd_latstats=xthread_get_latstats(itp);
#endif
	atomicSetInt(&tip->thd_flags, THD_EXIST|THD_OK);

	kt = itp;
	s = kt_lock(kt);
	putrunq(kt, CPU_NONE);
	kt_unlock(kt, s);
}

void
xthread_exit(void)
{
	int s;
	kthread_t *kt = curthreadp;
	xthread_t *xt = KT_TO_XT(kt);

	s = kt_lock(kt);
	end_rt(kt);
	kt_unlock(kt, s);

#ifdef ITHREAD_LATENCY
	kmem_zone_free(xt_latzone, xt->xt_latstats);
#endif
	/* remove from list */
	s = mutex_spinlock(&xt_list_lock);
	xt->xt_next->xt_prev = xt->xt_prev;
	xt->xt_prev->xt_next = xt->xt_next;
	mutex_spinunlock(&xt_list_lock, s);

	/*
	 * this gets us off our stack and onto the IDLE stack
	 * which queues us for the reaper thread to call
	 * xtdestroy to finally remove all the memory
	 */
	istexitswtch();
	/* NOTREACHED */
}

void
xthread_set_func(xthread_t *xt, xt_func_t *func, void *arg)
{
	kthread_t *kt = XT_TO_KT(xt);
	int s;

	s = kt_lock(kt);
	xt->xt_func = func;
	xt->xt_arg = arg;
	xt->xt_arg2 = 0;
	kt_unlock(kt, s);
}

/*
 * final throes of xthread - this is called from the reaper thread
 */
void
xtdestroy(xthread_t *xt)
{
	/* release any kthread stuff */
	kthread_destroy(XT_TO_KT(xt));
	kmem_zone_free(xt_zone, xt);
}

/*
 * KTOPS for xthreads
 */
static cred_t *
xthread_get_cred(kthread_t *kt)
{
#if defined(CELL)
	if (KT_TO_XT(kt)->xt_info) {
		transinfo_t     *ti;

		ti = (transinfo_t *)KT_TO_XT(kt)->xt_info;
		if (ti->ti_trans) {
			if (ti->ti_cred == NULL)
				ti->ti_cred = credid_getcred(ti->ti_trans->tr_credid);
			return (ti->ti_cred);
		}
	}
#endif
	return KT_TO_XT(kt)->xt_cred;
}

static void
xthread_set_cred(kthread_t *kt, cred_t *cr)
{
	ASSERT(cr->cr_ref > 0);
#if defined(CELL)
	if (KT_TO_XT(kt)->xt_info) {
		transinfo_t     *ti;

		ti = (transinfo_t *)KT_TO_XT(kt)->xt_info;
		ti->ti_cred = cr;
	} else
#endif
	KT_TO_XT(kt)->xt_cred = cr;
}

static char *
xthread_get_name(kthread_t *kt)
{
	return KT_TO_XT(kt)->xt_name;
}

/* ARGSUSED */
static void
xthread_update_null(kthread_t *kt, long val)
{
	ASSERT(KT_ISXTHREAD(kt));
}

static kt_ops_t xthreadops = {
	xthread_get_cred,
	xthread_set_cred,
	xthread_get_name,
	xthread_update_null,
	xthread_update_null,
	xthread_update_null,
	xthread_update_null
};


/* Interrupt thread infrastructure
 */
#ifdef ITHREAD_LATENCY
void
xthread_zero_latstats(xt_latency_t *l)
{
	bzero(l, sizeof(xt_latency_t));
	l->ls_min = ~0;
}

xt_latency_t *
xthread_get_latstats(xthread_t *s)
{
	/* XXX May need some locking later.
	 */
	return s->xt_latstats;
}

void
xthread_set_latstats(xthread_t *s, xt_latency_t *l)
{
	/* XXX May need some locking later.
	 */
	s->xt_latstats = l;
}

/* May want to make this a macro to avoid function call overhead */
void
xthread_set_istamp(xt_latency_t *l)
{
	l->ls_istamp = get_timestamp();
} /* xthread_set_istamp */

/* May want to make this a macro to avoid function call overhead */
void
xthread_update_latstats(xt_latency_t *l)
{
	uint kstamp, ticklen, latency;

	kstamp = get_timestamp();
	/* Even if kstamp wrapped, the subtraction is correct! */
	ticklen = kstamp - l->ls_istamp;
	latency = (ticklen * timer_unit)/1000;

	l->ls_sum += latency;
	l->ls_cnt++;

	/* The first time through, latency will be both <min and >max. */
	if (latency < l->ls_min)
		l->ls_min = latency;
	if (latency > l->ls_max)
		l->ls_max = latency;

	/* Update the apropriate counter */
	if (latency < 100)
		l->ls_10us[latency/10]++;
	else if (latency < 1000)
		/* ls_100us[0] should never be incremented! */
		l->ls_100us[latency/100]++;
	else
		l->ls_long++;
} /* xthread_update_latstats */

void
xthread_print_latstats(xt_latency_t *l)
{
	void qprintf(char *, ...);
	long long cumulative = 0;
	int i;

	qprintf("==Interrupt Latency Stats==\n");
	qprintf("Count: %lld\n", l->ls_cnt);
	if (l->ls_cnt == 0)
		return;
	qprintf("Min: %dus  ", l->ls_min);
	qprintf("Avg: %lldus  ", l->ls_sum/l->ls_cnt);
	qprintf("Max: %dus\n", l->ls_max);
	qprintf("==\t==\t==\t==\n");
	qprintf("Time\t#\t%%\tCumulative %%\n");

	for(i=0; i<10; i++) {
		cumulative += l->ls_10us[i];
		qprintf("<%d0us\t%d\t%lld\t%lld\n",
			i+1,
			l->ls_10us[i],
			l->ls_10us[i] * 100 / l->ls_cnt,
			cumulative * 100 / l->ls_cnt);
	}

	/* <100 is already accounted for. */
	for(i=1; i<10; i++) {
		cumulative += l->ls_100us[i];
		qprintf("<%d00us\t%d\t%lld\t%lld\n",
			i+1,
			l->ls_100us[i],
			l->ls_100us[i] * 100 / l->ls_cnt,
			cumulative * 100 / l->ls_cnt);
	}

	cumulative += l->ls_long;
	qprintf(">=1ms\t%d\t%lld\t%lld\n", l->ls_long,
		l->ls_long * 100 / l->ls_cnt,
		cumulative * 100 / l->ls_cnt);
}
#endif /* ITHREAD_LATENCY */

#if defined(CLOCK_CTIME_IS_ABSOLUTE)
void
xpt_func(xpt_t *info)
{
	(info->xpt_func)(info->xpt_arg);
	ipsema(&info->xpt_isync);
}

void
xpt_init(xpt_t *info)
{
	if (info->xpt_thread_cpu != CPU_NONE)
		(void)setmustrun(info->xpt_thread_cpu);
	xthread_set_func(info->xpt_xthread, (xt_func_t *)xpt_func, info);
	ipsema(&info->xpt_isync);
}

void
xpt_timeout(xpt_t *info)
{
	if (!(info->xpt_flags & THD_EXIST))
		return;
	if (valusema(&info->xpt_isync) < 1)
		vsema(&info->xpt_isync);
	info->xpt_time += info->xpt_period;
	clock_prtimeout_nothrd(info->xpt_timeout_cpu, xpt_timeout, info,
							info->xpt_time);
}

void
xthread_periodic_timeout(char *name,
			int pri,
			__int64_t period,
			cpuid_t cpu,
			xpt_t *info,
			xpt_func_t *func,
			void *arg)
{
	if (info->xpt_flags & THD_EXIST)
		return;

	/*
	 * Note that *all* of this state must be initialized *before* we
	 * call xthread_setup() since the xthread will start running
	 * immediately in xpt_init().
	 */
	info->xpt_flags = THD_REG;
	info->xpt_func = func;
	info->xpt_arg = arg;
	info->xpt_period = period;
	info->xpt_thread_cpu = cpu;
	if (cpu == CPU_NONE)
		info->xpt_timeout_cpu = cpuid();
	else
		info->xpt_timeout_cpu = cpu;

	xthread_setup(name, pri, &info->tip, (xt_func_t *)xpt_init, info);

	/*
	 * Setup first periodic timeout.
	 */
	info->xpt_time = GET_LOCAL_RTC;
	xpt_timeout(info);
}

void
xpt_stop(xpt_t *info)
{
	info->xpt_flags = 0;
	xthread_exit();
	/* NOTREACHED */
}

void
xthread_periodic_timeout_stop(xpt_t *info)
{
	if (!(info->xpt_flags & THD_EXIST))
		return;
	xthread_set_func(info->xpt_xthread, (xt_func_t *)xpt_stop, info);
	vsema(&info->xpt_isync);
}
#endif /* CLOCK_CTIME_IS_ABSOLUTE */
