/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1994 Silicon Graphics, Inc.		  *
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

#ident	"$Revision: 3.401 $"

#include <stdarg.h>
#include <sys/types.h>
#include <ksys/as.h>
#include <os/as/as_private.h> /* XXX */
#include <sys/callo.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/getpages.h>
#include <sys/immu.h>
#include <sys/pfdat.h>
#include <sys/page.h>
#include <sys/kabi.h>
#include <sys/kmem.h>
#include <sys/ksignal.h>
#include <sys/par.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/prctl.h>
#include <sys/proc.h>
#include <sys/psw.h>
#include <sys/reg.h>
#include <sys/resource.h>
#include <sys/runq.h>
#include <sys/schedctl.h>
#include <sys/signal.h>
#include <sys/strmp.h>
#include <sys/swap.h>
#include <sys/sysinfo.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/ktime.h>
#include <sys/time.h>
#include <sys/tuneable.h>
#include <ksys/exception.h>
#include <sys/xlate.h>
#include <sys/capability.h>
#include <sys/sat.h>
#include <sys/space.h>
#include <ksys/sthread.h>
#include <ksys/xthread.h>
#include <sys/hwperfmacros.h>
#include <sys/atomic_ops.h>
#include <sys/calloinfo.h>
#include <sys/ddi.h>
#include <sys/klog.h>
#include <sys/rt.h>
#include "os/proc/pproc_private.h"	/* XXX bogus */
#include <ksys/hwperf.h>
#ifdef	CELL
#include <ksys/cell/cell_hb.h>
#endif
#include <sys/lpage.h>
#include <ksys/vhost.h>
#ifdef NUMA_BASE
#include <sys/nodepda.h>
#endif

/*
 * Clock
 *
 * Functions:
 *	implement callouts
 *	maintain user/system times
 *	maintain date
 *	profile
 *	alarm clock signals
 *	jab the scheduler
 */
extern sema_t	vfswakeup;
extern void	onesec_maint(void);
extern void	tick_maint(void);
extern void	tick_actions(void);
static int	updateload(proc_t *, void *, int);
static void	calcrss(uthread_t *);
static void callout_itentry_init(callout_info_t *, int, char *);
static pgcnt_t getrss(uthread_t *);

sema_t		second_sema;		/* v'd once a second */

time_t		time;			/* time in seconds since 1970 */
time_t		lbolt;			/* time in HZ since last boot */

#if (MAXCPUS > 128)
int		sched_tick_mask=0;	/* (see comment where used below) */
#endif

int	one_sec = 1;
extern int vhandkicklim;	/* # pages before kick vhand	*/
int	vhandcnt;		/* counter for vhand kick	*/
int	vfssynccnt;		/* counter for vfs_syncr	*/
uint	sxbrkcnt;		/* count of uthreads which are SXBRK */
ulong	freeswap;		/* amount of free swap */


time_t lastadjtime;		/* HZ since last adjtime(2) */
time_t lastadjtod = DIDADJTIME;	/* for 1 hour after last adjtime(2) */

#if !defined(CLOCK_CTIME_IS_ABSOLUTE)
callout_info_t *calltodo;
#endif /* !CLOCK_CTIME_IS_ABSOLUTE */
callout_info_t *fastcatodo;
extern struct callout *timeout_get_queuehead(long list, struct callout *pnew);

static toid_t find_migrated_timeout(toid_t id);
static lock_t migrated_timeout_lock;

static int rqlen;
#ifdef notdef
static int sqlen;
#endif

extern int		pdcount;	/* count of pdinserted pages */
extern int		pdflag;		/* flag is nonzero if pdcount pages */

/*
 * Compute a Tenex-style load average of a quantity on 1, 5 and 15 minute 
 * intervals, using 'fixed-point' arithmetic with 3 decimal digits to right.
 *
 *  avg[T+t] = avg[T] * exp(-t/c) + nrun * (1 - exp(-t/c))
 *  where c = 1, 5, and 15 minutes, t = calculation interval
 *
 * Exponential constants for the specified interval:
 */
#define AVENRUN_INTVL	4	/* interval between calculations */
#define	CEXP_0	958		/* (int) (exp(-4/60) * 1024) */
#define	CEXP_1	1010		/* (int) (exp(-4/300) * 1024) */
#define	CEXP_2	1019		/* (int) (exp(-4/900) * 1024) */

__int32_t avenrun[3];		/* smoothed load averages */
static int nrun;		/* # of runnable processes (see updateload) */

static void
calcavenrun(void) 
{
	register __int32_t *avg = avenrun;

#define AVGCALC(n, exp) \
	avg[n] = (exp * (__int64_t)avg[n] + \
		 (((1024 - exp) * (__int64_t)nrun) << 10)) >> 10

	AVGCALC(0, CEXP_0);
	AVGCALC(1, CEXP_1);
	AVGCALC(2, CEXP_2);

#undef AVGCALC
}

#ifdef NUMA_BASE
#define SYSWAIT_NODE(_node, _field) NODEPDA(_node)->syswait._field
#define SYSWAIT_BACKOFF_THRESHOLD	(HZ * 5)	/* 5 secs */
#define SYSWAIT_BACKOFF_RATE		100		/* one out of 100 */
#define SYSWAIT_BIGSYS			10		/* one out of 10 */

/* Only the clock master re-calculates the global syswait count */
#define RECALC_SYSWAIT()					\
{								\
	if (private.p_flags & PDAF_CLOCK) {			\
		recalc_syswait();				\
		if (syswait.iowait || syswait.swap || syswait.physio) { \
			SYSINFO.wioocc++;			\
			SYSINFO.wioque += (syswait.iowait + 	\
				syswait.swap + syswait.physio);	\
		}						\
	}							\
}

/* Reset the node's syswait field by taking the current value and removing it.
 * This should prevent overflows from occurring.  We just can't set it to zero
 * because it could change while we are resetting it.  After doing this for
 * all nodes the accum value should represent the global state of syswait.
 * We can then apply that to our node to save it.
 */
#define RESET_SYSWAIT_VAL(_accum, _tmp, _node, _field)		\
{								\
	_tmp = SYSWAIT_NODE(_node, _field);			\
	_accum._field += _tmp;					\
	if (_tmp) {						\
		atomicAddInt(&NODEPDA(_node)->syswait._field, ~(_tmp - 1)); \
	}							\
}

static void
recalc_syswait(void)
{
	int	i;
	struct syswait	tmp_syswait;
	static time_t	last_wait = 0;

	/* To eliminate contention on the global syswait counter
	 * each node has its own. We add up the syswait counts of all
	 * nodes and store the result in the global count that will be
	 * seen by the other cpus.  Maybe the clock cpu is not the best
	 * choice for this since it gets stuck doing everything, but then
	 * again maybe that is a good reason to pick it ???
	 */

#if defined(SN0XXL) || defined(SN1)
	/*
	 * Large systems cant afford to make this calculation every tick.
	 */
	if (lbolt % SYSWAIT_BIGSYS != 0)
		return;
#endif
	if (lbolt - last_wait >= SYSWAIT_BACKOFF_THRESHOLD) {

		/* We are backing off to a slower rate since nothing 
		 * showed up within the threshold limit.  If something
		 * shows up again we'll go back to every tick.
		 */

		if (lbolt % SYSWAIT_BACKOFF_RATE != 0) {
			return;
		}
	}
		
	bzero(&tmp_syswait, sizeof(tmp_syswait));
	for (i = 0; i < numnodes; i++) {

		/* If the total count ends up being negative we must
		 * have passed the node that incremented it and we only
		 * caught the decrement.  Will set it to zero later.
		 */

		tmp_syswait.iowait += SYSWAIT_NODE(i, iowait);
		tmp_syswait.swap += SYSWAIT_NODE(i, swap);
		tmp_syswait.physio += SYSWAIT_NODE(i, physio);
	}

	/* Fix negative counts where the increment ocurred on a cpu
	 * after we went past it in the loop above. 
	 */

	if (tmp_syswait.iowait < 0) {
		tmp_syswait.iowait = 0;
	}
	if (tmp_syswait.swap < 0) {
		tmp_syswait.swap = 0;
	}
	if (tmp_syswait.physio < 0) {
		tmp_syswait.physio = 0;
	}

	/* Update the global syswait counts with the accumulated result */

	if (tmp_syswait.iowait != syswait.iowait) {
		syswait.iowait = tmp_syswait.iowait;
	}
	if (tmp_syswait.swap != syswait.swap) {
		syswait.swap = tmp_syswait.swap;
	}
	if (tmp_syswait.physio != syswait.physio) {
		syswait.physio = tmp_syswait.physio;
	}

	if (tmp_syswait.iowait || tmp_syswait.swap || tmp_syswait.physio) {

		/* Somebody is waiting for I/O, reset backoff */

		last_wait = lbolt;
	}
}

#define RESET_SYSWAIT_PERIOD	30	/* seconds */
#define RESET_SYSWAIT()		reset_syswait()

static void
reset_syswait(void)
{
	int	i, j;
	struct syswait	tmp_syswait;
	static	int reset_syswait_timeout = RESET_SYSWAIT_PERIOD;

	/* Periodically reset everyone's counter to prevent overflows and save
	 * the current syswait state in our's. 
	 */

	if (--reset_syswait_timeout > 0) {
		return;
	}
	reset_syswait_timeout = RESET_SYSWAIT_PERIOD;

	bzero(&tmp_syswait, sizeof(tmp_syswait));
	for (i = 0; i < numnodes; i++) {
		RESET_SYSWAIT_VAL(tmp_syswait, j, i, iowait);
		RESET_SYSWAIT_VAL(tmp_syswait, j, i, physio);
		RESET_SYSWAIT_VAL(tmp_syswait, j, i, swap);
	}
	atomicAddInt(&nodepda->syswait.iowait, tmp_syswait.iowait);
	atomicAddInt(&nodepda->syswait.physio, tmp_syswait.physio);
	atomicAddInt(&nodepda->syswait.swap, tmp_syswait.swap);
}
#else
#define RECALC_SYSWAIT()					\
{								\
	if (private.p_flags & PDAF_CLOCK) {			\
		if (syswait.iowait || syswait.swap || syswait.physio) { \
			SYSINFO.wioocc++;			\
			SYSINFO.wioque += (syswait.iowait + 	\
				syswait.swap + syswait.physio);	\
		}						\
	}							\
}
#define RESET_SYSWAIT()
#endif	/* NUMA_BASE */

#ifdef NUMA_BASE
#define PDCOUNT_NODE(_node)		NODEPDA(_node)->pdcount
/* Only the clock master re-calculates the global pdcount */
#define RECALC_PDCOUNT()					\
{								\
	if (private.p_flags & PDAF_CLOCK) {			\
		recalc_pdcount();				\
	}							\
}

/* Reset the node's pdcount field by taking the current value and removing it.
 * see above comments for RESET_SYSWAIT_VAL
 */
#define RESET_PDCOUNT_VAL(_accum, _tmp, _node)			\
{								\
	_tmp = swap_int(&NODEPDA(_node)->pdcount, 0);		\
	_accum += _tmp;						\
}


#define PDCOUNT_RECALC_RATE		HZ		/* once a second */

static void
recalc_pdcount(void)
{
	int	i;
	int	tmp_pdcount;
	static int pdcount_recalc_rate=0;

	/*
	 * Note that only routines such as sar or osview actually
	 * use pdcount. Therefore we only update it once a second.
	 */
	if (--pdcount_recalc_rate > 0)
		return;
	pdcount_recalc_rate = PDCOUNT_RECALC_RATE;

	tmp_pdcount=0;
	for (i = 0; i < numnodes; i++) {
		tmp_pdcount += PDCOUNT_NODE(i);
	}

	if (tmp_pdcount < 0) {
		tmp_pdcount = 0;
	}

	if(tmp_pdcount != pdcount) {
		pdcount = tmp_pdcount;
	}

	if (tmp_pdcount && !pdflag) {
		pdflag = 1;
	}
}

#define RESET_PDCOUNT_PERIOD	30	/* seconds */
#define RESET_PDCOUNT()		reset_pdcount()

static void
reset_pdcount(void)
{
	int	i, j;
	int	tmp_pdcount;
	static	int reset_pdcount_timeout = RESET_PDCOUNT_PERIOD;

	/* Periodically reset everyone's counter to prevent overflows and save
	 * the current pdcount state in our's. 
	 */
	if (--reset_pdcount_timeout > 0) {
		return;
	}
	reset_pdcount_timeout = RESET_PDCOUNT_PERIOD;

	tmp_pdcount = 0;
	for (i = 0; i < numnodes; i++) {
		RESET_PDCOUNT_VAL(tmp_pdcount, j, i);
	}
	atomicAddInt(&nodepda->pdcount, tmp_pdcount);
}

#else
#define	RECALC_PDCOUNT()
#define RESET_PDCOUNT()
#endif	/* NUMA_BASE */


/* ARGSUSED */
void
second_thread(void *arg)
{
	extern sv_t runout;
	int	coalesced_kick_timeout = COALESCED_KICK_PERIOD;

	for (;;) {
		psema(&second_sema, PZERO);

		/*
		 * we really should run on the clock_processor
		 * XXX race with sysmp()
		 */
		if (cpuid() != clock_processor)
			(void)setmustrun(clock_processor);

		(void) nfreeswap(&freeswap);

		rqlen = 0;
		nrun  = 0;
		/* update load average */
		procscan(updateload, 0);
		if (rqlen) {
			SYSINFO.runque += rqlen;
			SYSINFO.runocc++;
		}

		if ((time % AVENRUN_INTVL) == 0)
			calcavenrun();

		/*
		 * Periodically we're updating the amount of global system
		 * free memory.  Do this before waking coalesced -- it wants
		 * a reasonably accurate picture. 
		 */
		GLOBAL_FREEMEM_UPDATE();

		if (--vfssynccnt <= 0) {
			extern int vfs_syncr;
			vfssynccnt = vfs_syncr;
			cvsema(&vfswakeup);
		}

		if (--coalesced_kick_timeout <= 0) {
			coalesced_kick_timeout = COALESCED_KICK_PERIOD;
			COALESCED_KICK();
		}

		RESET_SYSWAIT();
		RESET_PDCOUNT();

		/* wake up sched every second */
		sv_signal(&runout);

		onesec_maint();
	}
}

void
init_second()
{
	extern int onesec_pri;

	initnsema(&second_sema, 1, "second_sema");
	sthread_create("onesec", NULL, 4096, 0, onesec_pri, KT_PS, 
			second_thread, 0, 0, 0, 0);
}

int
clock(eframe_t *ep)
{
	kthread_t *kt = curthreadp;
	k_machreg_t	ps = ep->ef_sr;
	uthread_t	*ut = kt && KT_ISUTHREAD(kt) ? KT_TO_UT(kt) : NULL;
	struct proc	*pp = ut ? UT_TO_PROC(ut) : NULL;
	register int	a;
	pgcnt_t		rss;

	ackrtclock();	/* acknowledge the clock interrupt	*/

	ASSERT(issplhi(getsr()));
	ASSERT(private.p_switching == 0);

	/*
	 * Blip the LED's if necessary.
	 */
	bump_leds();

	tick_actions();	/* machine-dependent per-tick activities */

#ifdef	CELL
	hb_update_local_heart_beat(); /* Update local heart beat */
#endif

	RECALC_SYSWAIT();
	RECALC_PDCOUNT();

#ifdef ULI
	/* we may have interrupted out of a ULI proc. This is only
	 * really a clock tick during a user process if curuli is
	 * also clear.
	 */
	if (USERMODE(ps) && ut && pp && !private.p_curuli)
#else
	if (USERMODE(ps) && ut && pp)
#endif
	{
		ASSERT(pp);		/* won't always be true! */
		a = CPU_USER;
		if (pp->p_profn) {
			/*
			 * Set a flag so that user will later accumulate
			 * a pc tick for this clock tick.  We can't just
			 * call addupc here because it may take a page
			 * fault and need to sleep.  It's a sin to sleep
			 * in the clock interrupt handler.
			 */
			if (pp->p_flag & SPROF) {
				/*
				 * Have to check the flag now since it could
				 * be profiling with the R10000 counters,
				 * in which case SPROF will not be set.
				 */
				ut_flagset(ut, UT_OWEUPC);
				PCB(pcb_resched) = 1;
			}
		}
#if !NO_WIRED_SEGMENTS
#if FAST_LOCORE_TFAULT
		/* In "fast locore tfault" mode, the utas_segflags are almost
		 * always set and does not indicate segement table mode.
		 * Instead, locore uses u_nexttlb to point into the wired
		 * range, and if out-of-bounds it indicates that we're in
		 * segment table mode and should do random 2nd level dropins.
		 * To start filling the wired entries with new entries, we
		 * simply need to reset u_nexttlb and we will start re-using
		 * the wired entries for new 2nd level dropins ... without
		 * needing to clear all of the maps. We MUST change the tlbpid
		 * to eliminate the non-wired 2nd level entries (unfortunately
		 * this will also effectively flush all 1st level entries).
		 */
		if ((ut->ut_exception->u_nexttlb >= NWIREDENTRIES-TLBWIREDBASE)
		    && (++ut->ut_as.utas_tlbcnt >= tune.t_tlbdrop)) {
			ut->ut_as.utas_tlbcnt = 0;
			ut->ut_exception->u_nexttlb = 0;
			new_tlbpid(&ut->ut_as, VM_TLBINVAL);
		}
#else /* !FAST_LOCORE_TFAULT */
		/*
		 * If process is running in segment-table mode,
		 * see if it will be well-behaved for awhile
		 * and use only NWIRED tlb entries.
		 * NOTE: No need for this code if we don't have wired
		 * second level tlbs (like TFP).
		 */
		if (++ut->ut_as.utas_tlbcnt >= tune.t_tlbdrop) {
			ut->ut_as.utas_tlbcnt = 0;
			if (ut->ut_as.utas_segflags) {
				setup_wired_tlb(1);
#ifdef R4000
				new_tlbpid(&ut->ut_as, VM_TLBINVAL);
#endif
			}
		}
#endif /* !FAST_LOCORE_TFAULT */
#endif /* !NO_WIRED_SEGMENTS */

		/*
		 * if we should, update the process
		 * virtual itimer and if it expired post the correct signal
		 * NOTE: this occurs for the normal clock tick (10ms)
		 */
		if (timerisset(&ut->ut_timer[UT_ITIMER_VIRTUAL].it_value) &&
		    itimerdecr(&ut->ut_timer[UT_ITIMER_VIRTUAL],
				USEC_PER_TICK) == 0)
			sigtouthread(ut, SIGVTALRM, (k_siginfo_t *)NULL);

	} else if (kt == NULL) {

		/* idling - see how */
		a = CPU_IDLE;

		if (sxbrkcnt)
			a = CPU_SXBRK;
		/*
		 * any time we're idle - account for wait I/O
		 * this overrides interest in SXBRK
		 */

		if (syswait.iowait) {
			a = CPU_WAIT;
			SYSINFO.wait[W_IO]++;
		}
		if (syswait.swap) {
			a = CPU_WAIT;
			SYSINFO.wait[W_SWAP]++;
		}
		if (syswait.physio) {
			a = CPU_WAIT;
			SYSINFO.wait[W_PIO]++;
		}

	} else {
		if (private.p_gfx_waitc) {
			a = CPU_WAIT;
			SYSINFO.wait[W_GFXC]++;
		} else if (private.p_gfx_waitf) {
			a = CPU_WAIT;
			if (ut)
				ut->ut_acct.ua_graphfifo++;
			SYSINFO.wait[W_GFXF]++;
		} else {
			if (KT_ISXTHREAD(kt))
				a = CPU_INTR;
			else
				a = CPU_KERNEL;
			if (ut)
				ut->ut_prftime++;
		}
	}

	SYSINFO.cpu[a]++;

	/*
	 * This could be a user thread that is exiting, so it might
	 * not have a process attached.
	 */
	if (pp && pp->p_stat == SRUN) {
		struct rlimit *rlp;
		timespec_t utime, stime;

		ASSERT(KT_ISUTHREAD(kt) && ut);

		rlp = &pp->p_rlimit[RLIMIT_CPU];

		/* XXX if this is a multi-threaded app, this
		 * is incorrect - we are looking only at this
		 * thread's u+s time, not the whole procs.
		 * But... to look at the whole proc would be
		 * expensive, and since resource limits are
		 * not part of any posix spec, and this
		 * will still perform the useful attribute
		 * of RLIMIT_CPU - to catch runaway processes.
		 */
		ktimer_read(UT_TO_KT(ut), AS_USR_RUN, &utime);
		ktimer_read(UT_TO_KT(ut), AS_SYS_RUN, &stime);
		if (rlp->rlim_cur != RLIM_INFINITY &&
		    utime.tv_sec + stime.tv_sec +1 > rlp->rlim_cur)
		{
			extern int cpulimit_gracetime;
			extern void qprintf(char *f, ...);

			if (cpulimit_gracetime == 0) {
				/*
				 * old behaviour
				 */

				sigtouthread(ut, SIGXCPU, (k_siginfo_t *)NULL);

				/*
			 	 * Don't give the signal every clock tick.
			 	 */
				if (rlp->rlim_cur < rlp->rlim_max)
					rlp->rlim_cur += 5;
			} else {

				/*
				 * new behaviour, we will send once SIGXCPU, so the
				 * process can checkpoint or whatever necessary and
				 * after a grace time defined by systuneable
				 * cpulimit_gracetime we send SIGKILL.
				 * The reason for this new new behaviour is that it
				 * was possible for a process to completely ignore 
				 * SIGXCPU.
				 */

				if (pp->p_flag & SGRACE) {
					sigtouthread(ut,SIGKILL,(k_siginfo_t *)NULL);
				} else {

					sigtouthread(ut, SIGXCPU, (k_siginfo_t *)NULL);
					rlp->rlim_cur += cpulimit_gracetime;
					pp->p_flag |= SGRACE;
					
				}
			}

		}

		/*
		 * Update the process profile itimer if it is
		 * set and if it expired post the SIGPROF signal.
		 * NOTE: this occurs for the normal clock tick (10ms).
		 */
		if (timerisset(&ut->ut_timer[UT_ITIMER_PROF].it_value) &&
		    itimerdecr(&ut->ut_timer[UT_ITIMER_PROF], USEC_PER_TICK)
									== 0)
			sigtouthread(ut, SIGPROF, (k_siginfo_t *)NULL);

		/*
		 * since # utlbmisses only updated on context switch
		 * update here also
		 */
		ut->ut_acct.ua_ufaults += private.p_utlbmisses;
		private.p_utlbmisses = 0;

		/*
		 * If this is a pthreads process, and it has a prda,
		 * and the prda's resched counter is non-null,
		 * decrement the counter --  and if it goes to zero,
		 * send pthread reschedule signal.
		 * XXX	Is this still needed?
		 */
		if (ut->ut_flags & UT_PTHREAD && ut->ut_prda) {
			/*
			 * Don't need atomic operator -- the uthread
			 * might be in the middle of updating this,
			 * but it only does processor-atomic stores,
			 * never increments/decrements.
			 * The worst that can happen is that we miss
			 * an update, and catch it next clock tick.
			 */
			if ((a = ut->ut_prda->t_sys.t_resched) > 0) {
				ut->ut_prda->t_sys.t_resched = --a;
				if (a == 0) {
					int s = ut_lock(ut);
					sigaddset(&ut->ut_sig, SIGPTRESCHED);
					ut_unlock(ut, s);
				}
			}
		}

		/*
		 * Once a second update in calcrss() is not good enough?
		 * Count 1K blocks, not pages.
		 * XXX If rss is negative, vhand is busy; ignore it.
		 */
		rss = getrss(ut);
		if (rss > 0) {
			if ((rss * (NBPC / 1024)) >
					ut->ut_pproxy->prxy_ru.ru_maxrss)
				ut->ut_pproxy->prxy_ru.ru_maxrss =
							rss * (NBPC / 1024);
			ut->ut_acct.ua_mem += rss;
		}
#ifdef DEBUG
		else if (rss < 0) {
			cmn_err(CE_DEBUG,
			"Strange ut_mem update: pid=%d  ut_mem=%lx  rss=%lx\n",
				pp->p_pid, ut->ut_acct.ua_mem, rss);
		}
#endif			
	}
	if (ut) {
		/*
		 * Time slice and preemption checks.
		 */
		tschkRunq(ut);
	}
#if (MAXCPUS > 128)
	/*
	 * On large systems, calling the wtree routines every clock tick
	 * on every cpu causes EXTREMELY hot cache lines in the wtree & job_t structures.
	 * In fact, the system hangs under some circumstances when access to
	 * the structures starts to take > 10ms. In this case, the next clock tick has
	 * already occurred when we exit clock() & we stop making forward progress.
	 *
	 * The fix (aka hack) is to call the cpu_sched_tick on every nth clock
	 * tick. We pick lbolt as the randomizer. This variable is monitonically
	 * increasing & incremented by 1 cpu once every clock tick (10ms).
	 *
	 * The following code does the following:
	 *	- at clock tick 0, cpu 0-63 will call cpu_sched_tick
	 *	- at clock tick 1, cpu 64-127 will call cpu_sched_tick
	 *
	 * The sched_tick_mask is set during boot and is a function of the number 
	 * of cpus in the system. The mask is 0 for 0-63 cpus, 1 for 64-127 cpus, ...
	 */
	if ((lbolt & sched_tick_mask) == ((cpuid()>>6)&sched_tick_mask))
#endif
		cpu_sched_tick(kt);

        /*
         * Numa memory management periodic ops
         */
        MEM_TICK();
        
	if (private.p_flags & PDAF_CLOCK) {
		unsigned long ofreemem;

		++lbolt;
		tick_maint();	/* muck with one_sec */
                
		/* "double" long arithmetic for minfo.freemem */
		ofreemem = MINFO.freemem[0];
		MINFO.freemem[0] += GLOBAL_FREEMEM();
		if (MINFO.freemem[0] < ofreemem)
			MINFO.freemem[1]++;

		if (--vhandcnt <= 0) {
			if (GLOBAL_FREEMEM() < vhandkicklim) {
				cvsema(&vhandsema);
			}
			vhandcnt = 2*HZ;
		} else if (rsswaitcnt) {
			cvsema(&vhandsema);
			vhandcnt = 2*HZ;
		} else if (GLOBAL_FREEMEM() < tune.t_gpgslo) {
			/*
			 * Push up vhand if memory is really low.
			 * We don't just wake up vhand here because
			 * we want to give runnable processes (who
			 * possibly are about to release their regions)
			 * a chance to run.
			 */
			if (vhandcnt > 5)
				vhandcnt = 5;
		}

		ASSERT(loclkok(ep));
		if (one_sec) {
			one_sec = 0;
			vsema(&second_sema);
#if DEBUG
			if (valusema(&second_sema))
				cmn_err(CE_WARN,"one second clock processing still pending after %d seconds\n", valusema(&second_sema)-1);
#endif

			/*
			 * Update memory usage for the current running process
			 */
			if (ut)
				calcrss(ut);

		}

		/*
		 * klog_need_action is set in icmn_err, indicating klogwakeup needs
		 * to be called. Note that this is done only on the PDAF_CLOCK cpu.
		 */
		if (klog_need_action) {
			klog_need_action = 0;
			klog_unlocked_wakeup();
		}
	}

	/*
	 * If we are using the event counters and there are
	 * more events being tracked than there are counters,
	 * multiplex the events every tick. (R10000)
	 */
	MULTIPLEX_HWPERF_COUNTERS();


	return(0);
}

/*
 * Update system load average.
 */
/*ARGSUSED*/
static int
updateload(proc_t *pp, void *arg, int mode)
{
	if (mode == 1 && pp->p_stat == SRUN && uscan_tryaccess(&pp->p_proxy)) {
		register uthread_t *ut;

		for (ut = prxy_to_thread(&pp->p_proxy); ut; ut = ut->ut_next) {
			kthread_t	*kt;
			/*
			 * If the thread is asleep but not sxbrk, don't
			 * mark it as on-the-runq, but do mark it as
			 * runnable if it isn't doing a long-term wait
			 * and it isn't breakable (this implies just waiting
			 * for some kinda mutex?).
			 * We don't bother locking ut_lock, 'cause we're
			 * just generating statistics.
			 */
			kt = UT_TO_KT(ut);
			if ((kt->k_flags & KT_SLEEP) &&
			    !(ut->ut_flags & UT_SXBRK)) {
				if ((kt->k_flags & (KT_LTWAIT|KT_NWAKE))
						== KT_NWAKE)
					nrun++;
				continue;
			}
			if (ut->ut_flags & UT_STOP)
				continue;

			if (! is_weightless(UT_TO_KT(ut)))
				nrun++;

			rqlen++;
		}
		uscan_unlock(&pp->p_proxy);
	}
	return(0);
}

static pgcnt_t
getrss(uthread_t *ut)
{
	vasid_t vasid;
	ppas_t *ppas;
	pas_t *pas;
	int as_lookup_pinned(uthread_t *, vasid_t *);

	/*
	 * Hack - this is the only code that tries all this grot from interrupt
	 * level, and we hope to remove this RSN.
	 * So, we need to practice careful reference here to avoid problems
	 * when a process happens to being execing
	 * We have a chance since we are in fact the running process
	 * so we can take snapshots
	 */
	if (AS_ISNULL(&ut->ut_asid))
		return 0;
	if (as_lookup_pinned(ut, &vasid))
		return 0;
	ppas = (ppas_t *)vasid.vas_pasid;
	pas = VASID_TO_PAS(vasid);
	return pas->pas_rss + ppas->ppas_rss;
}

static void
calcrss(uthread_t *ut)
{
	register preg_t	*prp;
	register reg_t	*rp;
	int doingshd = 0;
	vasid_t vasid;
	pas_t *pas;
	ppas_t *ppas;
	pgcnt_t rss = 0;
	int as_lookup_pinned(uthread_t *, vasid_t *);

	/*
	 * Hack - this is the only code that tries all this grot from interrupt
	 * level, and we hope to remove this RSN.
	 * So, we need to practice careful reference here to avoid problems
	 * when a process happens to being execing
	 * We have a chance since we are in fact the running process
	 * so we can take snapshots
	 */
	if (AS_ISNULL(&ut->ut_asid))
		return;
	if (as_lookup_pinned(ut, &vasid))
		return;
	if (VAS_TRYLOCK(vasid, AS_SHARED)) {
		pas = VASID_TO_PAS(vasid);
		ppas = (ppas_t *)vasid.vas_pasid;
		prp = PREG_FIRST(ppas->ppas_pregions);
doshd:
		while (prp) {
			rp = prp->p_reg;
			if (rp->r_flags & RG_PHYS) {
				prp = PREG_NEXT(prp);
				continue;
			}
			rss += prp->p_nvalid;
			prp = PREG_NEXT(prp);
		}
		if (!doingshd) {
			ppas->ppas_rss = rss;
			rss = 0;
			doingshd++;
			prp = PREG_FIRST(pas->pas_pregions);
			goto doshd;
		}
		pas->pas_rss = rss;
		VAS_UNLOCK(vasid);
	} else {
		/* no charges?? */
		;
	}
}


/*
 * Call the "vanilla" shake routine for each cpu's free zone.
 */
/* ARGSUSED */
static int
callout_shake(int level)
{
	cpuid_t cpu;
	int page_count = 0;

	ASSERT(level == SHAKEMGR_MEMORY);

	for (cpu=0; cpu < maxcpus; cpu++) {
		zone_t *callout_free_zone;

		if ((pdaindr[cpu].CpuId == -1) || !(pdaindr[cpu].pda->p_flags & PDAF_ENABLED))
			continue;

		callout_free_zone = CI_FREE_ZONE(&CALLTODO(cpu));

		page_count += zone_shake(callout_free_zone);
	}

	return(page_count);
}


/*
 * Per-CPU callout initialization.
 */
void
calloutinit_cpu(cpuid_t cpu)
{
	extern int ncallout;
	int nbytes, num_per_page, numpages;
	zone_t *callout_free_zone;
	cnodeid_t cnode;

	callout_free_zone = kmem_zone_private(sizeof(struct callout), "callout");

	CI_FREE_ZONE(&CALLTODO(cpu)) = callout_free_zone;
	(void)kmem_zone_private_mode_noalloc(callout_free_zone);
	(void)kmem_zone_enable_shake(callout_free_zone);

	nbytes = kmem_zone_unitsize(callout_free_zone);
	ASSERT(nbytes > 0);
	num_per_page = ctob(1) / nbytes;
	ASSERT(num_per_page > 0);

	/*
	 * Determine how many pages per CPU we'd need to allocate so that
	 * the total number of pages allocated stores ncallout structures.
	 */
	numpages = (((ncallout + num_per_page -1) / num_per_page) + maxcpus-1) / maxcpus;
	ASSERT(numpages > 0);

	cnode = cputocnode(cpu);
	kmem_zone_reserve_node(cnode, callout_free_zone, ctob(1)*numpages);

	/* Prevent shake routine from reclaiming entire callout list. */
	kmem_zone_minsize(callout_free_zone, numpages*num_per_page);

	shake_register(SHAKEMGR_MEMORY, callout_shake);
}


/*
 * Called once to perform global initialization required for timeout processing.
 */
void
calloutinit()
{

#if !CLOCK_CTIME_IS_ABSOLUTE
	calltodo = (callout_info_t *)kmem_zalloc(sizeof (*calltodo) * maxcpus,
					VM_DIRECT|KM_SLEEP);

	fastcatodo = (callout_info_t *)kmem_zalloc(sizeof (*fastcatodo),
					VM_DIRECT|KM_SLEEP);

	spinlock_init(&CI_LISTLOCK(fastcatodo), "fastcatodo");
#endif

	calloutinit_cpu(master_procid);

	spinlock_init(&migrated_timeout_lock, "migr_to");
}


/*
 * Allocate a callout structure.
 *
 * Try to get a structure from the target cpu's zone.  
 * If this fails, try to allocate more memory from the
 * target CPU's node and add it to the target's zone.
 * If this fails, try all the other cpus' zones.
 * If this fails, try to allocate any page to the target's zone.
 * If this fails, we're probably in trouble; return NULL
 * and let the caller deal with it.
 *
 * We raise to splprof before calling into any routine that needs
 * to grab a spinlock, because callout_alloc may be called from
 * an interrupt routine that interrupts at splprof, and we need
 * to avoid double-tripping on whatever locks are used.  We have
 * elected to repeatedly raise and lower IPL level as we try the
 * various altenatives rather than just holding it at splprof
 * for the duration of this function.  That way we won't end up
 * holding off interrupts for too long (we especially want to avoid
 * holdoffs that increase with the number of CPUs).  It's fairly
 * unusual to get all the way through the various cases, anyway;
 * most of the time, the initial attempt will succeed and we will
 * have done a single splprof/splx pair.
 */
static struct callout *
callout_alloc(cpuid_t targ)
{
        struct callout *co;
	zone_t *callout_free_zone;
	cpuid_t cpu, my_cpuid;
	int s;

	/* Try to get a free structure from the target cpu's zone. */
	callout_free_zone = CI_FREE_ZONE(&CALLTODO(targ));
again:
	s = splprof();
	co = kmem_zone_alloc(callout_free_zone, VM_NOSLEEP);
	splx(s);
	if (!co) {
		/* Try to add another page to the empty zone */
		void *ptr;

		s = splprof();
		ptr = kvpalloc_node(cputocnode(targ), 1, VM_NOSLEEP, 0);
		splx(s);
		if (ptr) {
			kmem_zone_fill(callout_free_zone, ptr, ctob(1));
			goto again;
		}
	} else {
		co->c_ownercpu = targ;
		return(co);
	}

	/*
	 * If we get here, it means that the target CPU's callout zone
	 * is empty and we were unable to allocate more memory to fill
	 * the zone.
	 *
	 * Try other cpus' lists.
	 */
	my_cpuid = cpuid();

	for (cpu=my_cpuid+1; cpu < maxcpus; cpu++) {
		if ((pdaindr[cpu].CpuId == -1) || !(pdaindr[cpu].pda->p_flags & PDAF_ENABLED))
			continue;
		callout_free_zone = CI_FREE_ZONE(&CALLTODO(cpu));
		s = splprof();
		co = kmem_zone_alloc(callout_free_zone, VM_NOSLEEP);
		splx(s);
		if (co) {
			co->c_ownercpu = cpu;
			return(co);
		}
	}

	for (cpu=0; cpu < my_cpuid; cpu++) {
		if ((pdaindr[cpu].CpuId == -1) || !(pdaindr[cpu].pda->p_flags & PDAF_ENABLED))
			continue;
		callout_free_zone = CI_FREE_ZONE(&CALLTODO(cpu));
		s = splprof();
		co = kmem_zone_alloc(callout_free_zone, VM_NOSLEEP);
		splx(s);
		if (co) {
			co->c_ownercpu = cpu;
			return(co);
		}
	}

	/*
	 * Try a kvpalloc without specifying node.
	 * We'd really prefer not to do this, since the callout structures
	 * from another node will be stuck on this node indefinitely.  Still,
	 * it's preferable to panicing.
	 */
	{
		void *ptr;

		s = splprof();
		ptr = kvpalloc(1, VM_NOSLEEP, 0);
		splx(s);
		if (ptr) {
			kmem_zone_fill(callout_free_zone, ptr, ctob(1));
			goto again;
		}
	}

	cmn_err_tag(317,CE_WARN, "Out of callouts cpu%d-->cpu%d\n", cpuid(), targ);

	return(NULL);
}


/*
 * Free the specified callout structure.
 */

void
callout_free(struct callout *co)
{
	cpuid_t owner_cpuid = co->c_ownercpu;
	zone_t *callout_free_zone = CI_FREE_ZONE(&CALLTODO(owner_cpuid));
	int s;

	ASSERT(owner_cpuid != CPU_NONE);

	/* Put it back in the zone for the CPU that owns it. */

	s = splprof();
	kmem_zone_free(callout_free_zone, co);
	splx(s);
}


/*
 * timeout is called to arrange that fun(arg) is called in tim/HZ seconds.
 * An entry is sorted into the callout structure.
 * The time in each structure entry is the number of HZ's more
 * than the previous entry. In this way, decrementing the
 * first entry has the effect of updating all entries.
 *
 * The panic is there because there is nothing
 * intelligent to be done if an entry won't fit.
 *
 * timeout now queues function to be invoked on the same cpu that
 * timeout was called
 */
toid_t
timeout(void (*fun)(), void *arg, long tim, ...)
{
	toid_t retval;

	va_list ap;
	va_start(ap, tim);
	retval = dotimeout(cpuid(), fun, arg, tim, callout_get_pri(), C_NORM, ap);
	va_end(ap);

	if (retval == NULL)
		/*
		 * The kernel was unable to allocate space for
                 * a timeout request.  Since timeout is a comptability 
		 * interface, the kernel cannot return
                 * an error and allow processing to be handler
                 * by the caller.  
		 */
		cmn_err(CE_PANIC,
			"Timeout table overflow.\n Tune ncallout to a higher value.");

	return(retval);
}

toid_t
timeout_pri(void (*fun)(), void *arg, long tim, int pri, ...)
{
	toid_t retval;
	va_list ap;

	/* timeouts should never equal 0 */
	ASSERT((pri > 0) && (pri <= 255));

	va_start(ap, pri);
	retval = dotimeout(cpuid(), fun, arg, tim, pri, C_NORM, ap);
	va_end(ap);

	return(retval);
}

/* This routine is identical to timeout() but causes timein processing
 * to occur on the timein interrupt stack.
 *
 * NOTE: Should ONLY be used on short duration routines which are simply
 * awakening another thread or incrementing a counter.
 */

toid_t
timeout_nothrd(void (*fun)(), void *arg, long tim, ...)
{
	toid_t retval;

	va_list ap;
	va_start(ap, tim);
	retval = dotimeout(cpuid(), fun, arg, tim, 0, C_NORM_ISTK, ap);
	va_end(ap);

	if (retval == NULL)
		/*
		 * The kernel was unable to allocate space for
                 * a timeout request.  Since timeout is a comptability 
		 * interface, the kernel cannot return
                 * an error and allow processing to be handled
                 * by the caller.  
		 */
		cmn_err(CE_PANIC,
			"Timeout table overflow.\n Tune ncallout to a higher value.");

	return(retval);
}

/*
 * prtimeout - queue a timeout on specified processor
 * If a processor with timeouts is isolated or restricted then the 
 * timeouts will migrate to the clock processor.
 */
toid_t
prtimeout(processorid_t prid, void (*fun)(), void *arg, long tim, ...)
{
	toid_t retval;

	va_list ap;
	va_start(ap, tim);
	retval = dotimeout(prid, fun, arg, tim, callout_get_pri(), C_NORM, ap);
	va_end(ap);

	if (retval == NULL)
		/*
		 * The kernel was unable to allocate space for
                 * a timeout request.  Since timeout is a comptability 
		 * interface, the kernel cannot return
                 * an error and allow processing to be handled
                 * by the caller.  
		 */
		cmn_err(CE_PANIC,
			"Timeout table overflow.\n Tune ncallout to a higher value.");

	return(retval);
}

#if RTINT_WAR
/* This routine is identical to prtimeout() but causes timein processing
 * to occur on the timein interrupt stack.
 *
 * NOTE: Should ONLY be used on short duration routines which are simply
 * awakening another thread or incrementing a counter.
 */

toid_t
prtimeout_nothrd(processorid_t prid, void (*fun)(), void *arg, long tim, ...)
{
	toid_t retval;

	va_list ap;
	va_start(ap, tim);
	retval = dotimeout(prid, fun, arg, tim, 0, C_NORM_ISTK, ap);
	va_end(ap);

	if (retval == NULL)
		/*
		 * The kernel was unable to allocate space for
                 * a timeout request.  Since timeout is a comptability 
		 * interface, the kernel cannot return
                 * an error and allow processing to be handled
                 * by the caller.  
		 */
		cmn_err(CE_PANIC,
			"Timeout table overflow.\n Tune ncallout to a higher value.");

	return(retval);
}
#endif

/*
 * fast_prtimeout - like prtimeout, but use the fast timeouts
 */

extern int fastclock;
toid_t
fast_prtimeout(processorid_t targcpu, void (*fun)(), void *arg, long tim, ...)
{
	toid_t retval;
	va_list ap;

	if (!fastclock)
		enable_fastclock();
	va_start(ap, tim);

	retval = dotimeout(targcpu, fun, arg, tim, callout_get_pri(), C_FAST, ap);
	va_end(ap);
	
	if (retval == NULL)
		cmn_err(CE_PANIC,"Timeout table overflow.\n Tune ncallout to a higher value.");
	return(retval);
}

/*
 * timeout routines for DDI/DKI compliant drivers.
 */

extern pl_t plbase;
extern pl_t pltimeout;
extern pl_t pldisk;
extern pl_t plstr;
extern pl_t plhi;

/* ARGSUSED */
toid_t
dtimeout(void (*fun)(), void *arg, long tim, pl_t pl, processorid_t prid)
{
	toid_t  retval;
	va_list ap;

	va_start(ap, prid);

	retval = dotimeout(prid, fun, arg, tim, callout_get_pri(), C_NORM, ap);
	va_end(ap);
	return(retval);
}

toid_t
itimeout(void (*fun)(), void *arg, long tim, pl_t pl, ...)
{
	toid_t  retval;
	va_list ap;

	va_start(ap, pl);

	retval = dotimeout(cpuid(), fun, arg, tim, callout_get_pri(), C_NORM, ap);
	va_end(ap);
	return(retval);
}

toid_t
itimeout_nothrd(void (*fun)(), void *arg, long tim, pl_t pl, ...)
{
	toid_t  retval;
	va_list ap;

	va_start(ap, pl);
	retval = dotimeout(cpuid(), fun, arg, tim, 0, C_NORM_ISTK,
									ap);
	va_end(ap);
	return(retval);
}

toid_t
fast_itimeout(void (*fun)(), void *arg, long tim, pl_t pl, ...)
{
	toid_t  retval;
	va_list ap;

	if (!fastclock)
		enable_fastclock();

	va_start(ap, pl);
	retval = dotimeout(cpuid(), fun, arg, tim, callout_get_pri(), C_FAST, ap);
	va_end(ap);
	return(retval);
}

toid_t
fast_itimeout_nothrd(void (*fun)(), void *arg, long tim, pl_t pl, ...)
{
	toid_t  retval;
	va_list ap;

	if (!fastclock)
		enable_fastclock();
	va_start(ap, pl);
	retval = dotimeout(cpuid(), fun, arg, tim, 0, C_FAST_ISTK,
									ap);
	va_end(ap);
	return(retval);
}

#ifdef CLOCK_CTIME_IS_ABSOLUTE
/*
 * clock_prtimeout - like prtimeout, but use the absolute timeouts
 * This function is only useful or used on systems that call
 * set_timer_intr with an absolute time rather then a number
 * of ticks.
 */
toid_t
clock_prtimeout(processorid_t targcpu, void (*fun)(), void *arg, __int64_t tim, int pri, ...)
{
	toid_t retval;
	va_list ap;
	
	va_start(ap, pri);
	retval = dotimeout(targcpu, fun, arg, tim, pri, C_CLOCK, ap);
	va_end(ap);
	if (retval == NULL)
		cmn_err(CE_PANIC,"Timeout table overflow.\n Tune ncallout to a higher value.");
	return(retval);
}

toid_t
clock_prtimeout_nothrd(processorid_t targcpu, void (*fun)(), void *arg,
		__int64_t tim, ...)
{
	toid_t retval;
	va_list ap;
	
	va_start(ap, tim);
	retval = dotimeout(targcpu, fun, arg, tim, 0, C_CLOCK_ISTK, ap);
	va_end(ap);
	if (retval == NULL)
		cmn_err_tag(138,CE_PANIC,"Timeout table overflow.\n Tune ncallout to a higher value.");
	return(retval);
}
#endif /* CLOCK_CTIME_IS_ABSOLUTE */
/*
** list is used to select which callout list to queue on
** list == C_FAST then queue em on the fast callout list
** list == C_NORM then queue em on processor 'targcpu' callout list
** list == C_CLOCK then queue em on the fast callout list using an
**	   absolute clock cycle count to cmp against. [EVEREST]
** Per-CPU lists each have an associated "list lock".
**
** if tim == TIMEPOKE_NOW then timepoke() is called immediately
*/

toid_t
dotimeout(
	register processorid_t targcpu,
	void (*fun)(),
	void *arg,
	__int64_t tim,
	int pri,
	long list,
	va_list ap)
{
	register struct callout *p1, *p2, *pnew, *phead;
	register toid_t id;
	void *arg1, *arg2, *arg3;
	int s;
	__int64_t tmp_tim;

	ASSERT(targcpu >= 0  &&  targcpu < maxcpus);
	arg1 = va_arg(ap, void *);
	arg2 = va_arg(ap, void *);
	arg3 = va_arg(ap, void *);

	/*
	 * Frame Scheduler
	 */
	if (pdaindr[targcpu].pda->p_frs_flags) {
		targcpu = clock_processor;
	}

#ifdef ISOLATE_DEBUG
	{
	extern int isolate_drop;

	if (pdaindr[targcpu].pda->p_flags & PDAF_ISOLATED) {
		cmn_err(CE_WARN,
			"Isolated processor %d executes dotimeout\n",
			pdaindr[targcpu].pda->p_cpuid);
		if (isolate_drop)
			debug((char *)0);
	}
	}
#endif
	
	pnew = callout_alloc(targcpu);
	if (!pnew)
		return(0);

	pnew->c_id = 0;
	pnew->c_flags = 0;
	pnew->c_func = fun;
	pnew->c_arg = arg;
	pnew->c_arg1 = arg1;
	pnew->c_arg2 = arg2;
	pnew->c_arg3 = arg3;
	pnew->c_pl = pri;
	pnew->c_cpuid = targcpu;
	pnew->c_time = tim;

	/* Grab the per-list lock in order to enqueue the new callout request */
	s = mutex_spinlock_spl(&CI_LISTLOCK(&CALLTODO(targcpu)), splprof);

	/*
	 * Find the head of the queue to insert the
	 * callout, if needed adjust the c_time field
	 * to have the correct units for the hardware
	 */
	phead = timeout_get_queuehead(list, pnew);

	/*
	 * Set any needed flags info.
	 */
	if (list == C_NORM_ISTK || list == C_FAST_ISTK || list == C_CLOCK_ISTK)
		pnew->c_flags |= C_FLAG_ISTK;
	else
		pnew->c_flags |= C_FLAG_ITHRD;

	ASSERT(phead);
	
	tmp_tim = pnew->c_time;
	id = pnew->c_id;	/* return all bits */

	/*
	 * Insert pnew into correct position in callout list.
	 */
	for (p1 = phead ; p2 = p1->c_next ; p1 = p2) {
	    if (p2->c_time > tmp_tim)
		break;

#if !defined(CLOCK_CTIME_IS_ABSOLUTE)
		/* Make times relative to prev callback */
		tmp_tim -= p2->c_time;
#endif /* !CLOCK_CTIME_IS_ABSOLUTE */
	}
	p1->c_next = pnew;
	pnew->c_next = p2;

#if !defined(CLOCK_CTIME_IS_ABSOLUTE)
	pnew->c_time = tmp_tim;
	/* Nothing to do for CLOCK_CTIME_IS_ABSOLUTE, times are absolute*/
	if (p2)
		p2->c_time -= tmp_tim;
#endif /* !CLOCK_CTIME_IS_ABSOLUTE */
	/*
	 * If we've just put something at the head of the queue,
	 * insure that a timer will go off at the right moment.
	 */
	if (phead == p1)
		set_timer_intr(targcpu, tmp_tim, list);

	mutex_spinunlock(&CI_LISTLOCK(&CALLTODO(targcpu)), s);
	return(id);
}

/*
 * Return a thread priority for a callout thread.  Used whenever
 * we are not given an explicit priority.
 */

int
callout_get_pri(void)
{
	extern int default_timeout_pri;

	if (private.p_kstackflag <= PDA_CURKERSTK) {
		kthread_t *kt = curthreadp;
		int pri = kt->k_basepri;

		if (kt->k_copri) {
			pri = kt->k_copri;
			ASSERT(pri != 255);
		} else if (pri < 0)
			pri = default_timeout_pri;
		else if (KT_ISUTHREAD(kt) && (pri < 255))
			pri++;

		ASSERT((pri >= 0) && (pri <= 255));
		return pri;
	}

	return default_timeout_pri;
}

/*
 * untimeout_body
 *
 * This routine attempts to find the specified timeout id and
 * disable the entry if possible by checking the todo list, the
 * pending list, timein thread list, and the migrated list.
 */
static int untimeout_migrated(toid_t, int);

static int
untimeout_body(toid_t id, int wait)
{
	register struct callout *p1, *p2, *p3;
	ci_itinfo_t *citp;
	callout_info_t *cip;
	register int s;
	union	c_tid c_tid;	/* pick this name to use macro in callo.h */
	__int64_t totaltime = 0;
	int rc = 0, found = 0;

	extern struct strintr *strintrrsrv;

	/* Races here are benign */
	if (strintrrsrv != NULL)
		streams_untimeout(id);

	/* determine which callout to search */
	c_id = id;
	cip = c_fast == 1 ? fastcatodo : &CALLTODO(c_cpuid);

	/* Search todo list */
	p1 = CI_TODO(cip);

	s = mutex_spinlock_spl(&CI_LISTLOCK(cip), splprof);
	for ( ; (p2 = p1->c_next) != 0; p1 = p2) {
#ifndef CLOCK_CTIME_IS_ABSOLUTE
	    totaltime += p2->c_time;
#endif /* CLOCK_CTIME_IS_ABSOLUTE */
		if (p2->c_id == c_id) {
			found = 1;
			p3 = p2->c_next;
#ifdef CLOCK_CTIME_IS_ABSOLUTE
			totaltime = p2->c_time;
#else
			if (p3)	/* carry overflow or delta */
				p3->c_time += p2->c_time;
#endif /* CLOCK_CTIME_IS_ABSOLUTE */
			p1->c_next = p3;
			rc = callout_time_to_hz(totaltime, c_cpuid, c_fast);
			break;
		}
	}

	if (!found) {
		/* Search pending callout list */
		p1 = CI_PENDING(cip);
		for ( ; (p2 = p1->c_next) != NULL; p1 = p2) {
			if (p2->c_id == c_id) {
				p1->c_next = p2->c_next;
				rc = found = 1;
				break;
			}
		}
	}
	
	if (found) {
		mutex_spinunlock(&CI_LISTLOCK(cip), s);
		callout_free(p2);
		return(rc);
	}

	/*
	 * If we're in wait mode, then we need to search the
	 * list of xthread infos to see if any timein threads are
	 * executing the one we're looking for.  If we find it,
	 * and we're not the thread executing the timeout (i.e.
	 * the timeout canceling itself -- see ec_recover()),
	 * then we go to sleep on the sync var.
	 */
	ASSERT(!found);
	if (wait) {
		int x;
		citp = cip->ci_ithrdinfo;
		for (x=0; x < cip->ci_ithrd_cnt; x++, citp++) {
			if ((citp->cit_toid == c_id) && (citp->cit_ithread !=
			    (struct xthread *)private.p_curkthread)) {
				citp->cit_flags |= CIT_WAITING;
				sv_wait(&citp->cit_sync, PZERO, &CI_LISTLOCK(cip), s);
				found = 1;
				break;
			}
		}
	}

	if (!found) {
		mutex_spinunlock(&CI_LISTLOCK(cip), s);
		rc = untimeout_migrated(id, wait);
	}

	return(rc);
}

int
untimeout_wait(toid_t id)
{
	ASSERT(id);
	return (untimeout_body(id, 1));
}

int
untimeout(toid_t id)
{
	/*
	 * If the id is 0 then there is no work to do.
	 */
	if (id == 0)
		return(0);
	return (untimeout_body(id, 0));
}

/*
 * given an ID, return true(1) if still in timeout queue
 * For the result to mean much, these functions should be called at spl > 1
 */
int
chktimeout(int id)
{
	register struct callout *p1, *p2;
	register int s, rv = 0;
	union c_tid c_tid; /* pick this name to use macros in callo.h */
	callout_info_t *callout_info;

	c_id = id;
	callout_info = &CALLTODO(c_cpuid);
	s = mutex_spinlock_spl(&CI_LISTLOCK(callout_info), splprof);
	for (p1 = CI_TODO(callout_info);
	     (p2 = p1->c_next) != 0; p1 = p2) {
		if (p2->c_cid == c_cid) {
			rv = 1;
			break;
		}
	}
	mutex_spinunlock(&CI_LISTLOCK(callout_info), s);
	return(rv);
}

__int64_t
do_chktimeout_tick(callout_info_t *callout_info, toid_t id, void (*fun)(), void *arg)
{	
	register struct callout *phead, *p1, *p2;
	__int64_t rv = 0;
	union	c_tid c_tid;	/* pick this name to use macro in callo.h */
	int s;

	/*
	 * When passed nothing return nothing
	 */
	if ((id == 0) && (fun == NULL))
		return (0);
	c_id = id;
	phead = CI_TODO(callout_info);

	s = mutex_spinlock_spl(&CI_LISTLOCK(callout_info), splprof);
	for ( p1 = phead; (p2 = p1->c_next) != 0; p1 = p2) {
#ifndef CLOCK_CTIME_IS_ABSOLUTE
		/* accumulating ticks before it */
		rv += p2->c_time;
#endif /* !CLOCK_CTIME_IS_ABSOLUTE */
		if ((p2->c_id == c_id) ||
		    (p2->c_func == fun && p2->c_arg == arg)){
		    break;
		}
	}
   	mutex_spinunlock(&CI_LISTLOCK(callout_info), s);

	if (p2 == 0) {		/* did not find it */
		if (id) {
			c_id = find_migrated_timeout(id);
			if(c_id)
				return(do_chktimeout_tick(callout_info,c_id, NULL, NULL));
				
		}
		return(0);
	}

#ifdef CLOCK_CTIME_IS_ABSOLUTE
	rv = p2->c_time;
#endif /* CLOCK_CTIME_IS_ABSOLUTE */

	if (rv <= 0)		/* found it, but was negative or zero */
		return(1);	/* return a small positive value */
	else
		return(rv);	/* found it, return accurate value */
}

/*
 * Process a timeout entry and release it back to the free list.
 */
void
timein_entry(struct callout *timeout_entry)
{
	register void *arg, *arg1, *arg2, *arg3;
	register void (*func)(void *, void *, void *, void *);
#ifdef ISOLATE_DEBUG
	pda_t   *npda;
	extern int isolate_debug, isolate_drop;
	extern int wsyncv();

	npda = pdaindr[cpuid()].pda;
	if (npda->p_flags & PDAF_ISOLATED &&
			isolate_debug &&
			timeout_entry->c_func != (void (*)())wsyncv) {
		cmn_err(CE_WARN,
			"Isolated proc %d about to run timeout 0x%x\n",
			npda->p_cpuid,timeout_entry->c_func );
		if (isolate_drop) debug((char *) 0);
	}
#endif /* ISOLATE_DEBUG */
	arg = timeout_entry->c_arg;
	arg1 = timeout_entry->c_arg1;
	arg2 = timeout_entry->c_arg2;
	arg3 = timeout_entry->c_arg3;
	func = (void (*)(void *, void *, void *, void *))
			timeout_entry->c_func;

	callout_free(timeout_entry);

	(*func)(arg, arg1, arg2, arg3); /* call the function */
}

/* ARGSUSED */
void
timein_entry_icvsema(ci_itinfo_t *citp, struct callout *to)
{
	void timein_body_ithrd(ci_itinfo_t *);
	void thread_timein(void *);
	kthread_t *kt = curthreadp;

	timein_body_ithrd(citp);
	
	/* Reset func, args and pri, in case thread is next started
	 * by vsema (rather than icvsema).  If it is again started
	 * by icvsema, these will be overridden.
	 */
	xthread_set_func(KT_TO_XT(kt), (xt_func_t *)thread_timein, citp);

	ipsema(CIT_GET_SEMA(citp));
}

/*
 * timein_entry_ithrd
 *
 * This routine links the specified callout entry to the tail
 * of the xthread callout list and then bumps the counting
 * semaphore.
 */
void
timein_entry_ithrd(callout_info_t *cip, struct callout *to, int s)
{
	struct callout *p1;
	ci_itinfo_t *citp;
	kthread_t *ktscan;
	kthread_t *kt;
	xthread_t *xt;
	int hipri;
	int x;

	xt = icvsema(CI_SEMA(cip), to->c_pl,
		     (xt_func_t *) timein_entry_icvsema,
		     NULL, to);

	if (xt) {
		/*
		 * Register untimeout information.  Note that the newly
		 * runable interrupt thread can't run yet because it's
		 * mustrun on the current CPU and we hold a spinlock.
		 * This prevents a race between the initialization here
		 * and the thread trying to read the information ...
		 */
		ASSERT_MP(XT_TO_KT(xt)->k_mustrun == cpuid());
		citp = (ci_itinfo_t*) xt->xt_arg;
		citp->cit_to = to;
		citp->cit_toid = to->c_id;
		mutex_spinunlock(&CI_LISTLOCK(cip), s);
		return;
	}

	/*
	 * Normally, the icvsema above will succeed, and the timein thread
	 * will be started directly.  However, when we get callouts faster
	 * than the timein threads can handle them, we fall back to having
	 * to queue the callout and wait for the next available timein thread.
	 */

	kt = NULL;
	hipri = 0;

	/*
	 * Before we queue this callout, we need to check if it is
	 * more important than the callouts currently being handled.
	 * If it is, then we need to boost the priority of one of the
	 * running timein threads to prevent priority inversion.
	 */
	citp = cip->ci_ithrdinfo;
	for (x=0; x < cip->ci_ithrd_cnt; x++, citp++) {
		ktscan = XT_TO_KT(citp->cit_ithread);
		if (ktscan->k_basepri >= to->c_pl) {
			kt = NULL;
			break;
		}
		if (ktscan->k_basepri > hipri) {
			hipri = ktscan->k_basepri;
			kt = ktscan;
		}
	}

	if (kt) {
		/*
		 * Boost priority
		 */
		int dequeued = 0;
retry:
		kt_nested_lock(kt);
		if (kt->k_onrq != CPU_NONE) {
			if (removerunq(kt))
				dequeued++;
			else {
				kt_nested_unlock(kt);
				goto retry;
			}
		}

		if (kt->k_copri == 0)
			kt->k_copri = kt->k_basepri;

		kt->k_basepri = to->c_pl;
		if (kt->k_pri < to->c_pl)
			kt->k_pri = to->c_pl;
		
		if (dequeued)
			putrunq(kt, CPU_NONE);

		kt_nested_unlock(kt);
	}
			
	/*
	 * Keep pending callout queue sorted by priority
	 */
	if (p1 = CI_PENDING_NEXT(cip)) {
		struct callout *prev = CI_PENDING(cip);
		while (p1->c_pl >= to->c_pl) {
			if (p1->c_next == NULL) {
				p1->c_next = to;
				to->c_next = NULL;
				goto done;
			}
			prev = p1;
			p1 = p1->c_next;
		}
		to->c_next = prev->c_next;
		prev->c_next = to;
	} else {
		CI_PENDING_NEXT(cip) = to;
		to->c_next = NULL;
	}

done:
	mutex_spinunlock(&CI_LISTLOCK(cip), s);

	vsema(CI_SEMA(cip));
}

/*
 * timein_body_ithrd()
 *
 * This routine scans the list of xthread callouts and executes them
 * at the proper spl/priority value (note that any entries on this
 * list are by definition expired.  We also handle untimeout() 
 * synchronization here.
 */
void
timein_body_ithrd(ci_itinfo_t *citp)
{
	callout_info_t *cip = citp->cit_calloinfo;
	register struct callout *list, *p1, *p2;
	register int s;
	kthread_t *kt = curthreadp;

	ASSERT(cip);
	ASSERT(cip->ci_flags & CA_ENABLED);

	/*
	 * If cit_to is set, which will be the usual case,
	 * our timein thread has been started by icvsema directly,
	 * and we're running at the correct priority.  Otherwise,
	 * we need to find the next timeout on the PENDING list.
	 */
	if ((p1 = citp->cit_to) == NULL) {
		list = CI_PENDING(cip);
		s = mutex_spinlock_spl(&CI_LISTLOCK(cip), splprof);
		kt->k_copri = 0;

		p1 = list->c_next;
		if (p1 == 0) {		/* we're done */
			mutex_spinunlock(&CI_LISTLOCK(cip), s);
			return;
		}

		p2 = p1->c_next;	/* advance to next item */
		list->c_next = p2;

		if (p1->c_next == NULL)
			CI_PENDING_NEXT(cip) = NULL;

		/* register untimeout information */
		citp->cit_toid = p1->c_id;
		citp->cit_to = p1;

		/*
		 * This is rare, but in the event our priority was boosted
		 * by a high priority callout, we must make sure we lower it
		 * after the urgent callout was handled.
		 */
		if (kt->k_basepri > p1->c_pl) {
			kt_nested_lock(kt);
			kt_initialize_pri(kt, p1->c_pl);
			reset_pri(kt);
			kt_nested_unlock(kt);
		}
		ASSERT(kt->k_basepri <= p1->c_pl);
		ASSERT(kt->k_pri <= p1->c_pl);

		mutex_spinunlock(&CI_LISTLOCK(cip), s);

		if (private.p_runrun == 1)
			qswtch(RESCHED_Y);

		/* Create another thread since we're overloaded */
		if (cip->ci_ithrd_cnt < CA_ITHRDS_PER_LIST)
			callout_itentry_init(cip, cpuid(), "timein");
	}

	ASSERT(citp->cit_toid == p1->c_id);
	ASSERT(citp->cit_to == p1);

	timein_entry(p1);

	/*
	 * Check to see if someone is waiting on the
	 * timeout we just fired.  If so, clear the wait
	 * state and wake em up.
	 */
	s = mutex_spinlock_spl(&CI_LISTLOCK(cip), splprof);
	if (citp->cit_flags & CIT_WAITING) {
		citp->cit_flags &= ~CIT_WAITING;
		sv_broadcast(&citp->cit_sync);
	}
	citp->cit_toid = 0;
	citp->cit_to = NULL;
	kt->k_copri = 0;
	mutex_spinunlock(&CI_LISTLOCK(cip), s);
}

/*
 * This function doesn't look like a loop, but it is.
 * The ipsema call always comes out calling the function set up in the xthread.
 */
void
thread_timein(void *arg)
{	
	timein_body_ithrd(arg);
	ipsema(CIT_GET_SEMA((ci_itinfo_t *)arg));
}

/*
 * Do one-time thread setup.  Won't get called more than once per thread
 * since the ipsema() will come out in the function setup in the
 * xthread_set_func() call.
 */
void
thread_timein_start(void *arg)
{
#if MP
	int mustruncpu = CIT_TO_CPU((ci_itinfo_t *)arg);
#endif

	/* NO-OP on UP */
	(void) setmustrun(mustruncpu);

	xthread_set_func(KT_TO_XT(curthreadp), (xt_func_t *)thread_timein, arg);
	ipsema(CIT_GET_SEMA((ci_itinfo_t *)arg));
	/* NOTREACHED */
}

/*
 * callout_info_init
 *
 * Initialize callout info structure.
 */
static void
callout_itentry_init(callout_info_t *cip, int targcpu, char *name)
{
	char threadname[20];
	ci_itinfo_t *citp;
	extern int default_timeout_pri;

	/* fill in ithrd info */
	if (atomicSetInt(&cip->ci_flags,CA_ITHRD_CREATING) & CA_ITHRD_CREATING)
		return;

	if (cip->ci_ithrd_cnt >= CA_ITHRDS_PER_LIST)
		return;

	citp = &cip->ci_ithrdinfo[cip->ci_ithrd_cnt];

	init_sv(&citp->cit_sync, SV_DEFAULT, name, targcpu);
	citp->cit_calloinfo = cip;

	sprintf(threadname, "%s%d", name, targcpu);
	citp->cit_ithread = xthread_create(threadname, 0,
				KTHREAD_DEF_STACKSZ, 0,
				default_timeout_pri, KT_PS,
				(xt_func_t *)thread_timein_start,
				(void *)(__psint_t)citp);

	/*
	 * Don't increment new thread cursor till we're done initializing
	 * all associated state -- like cit_ithread.  This avoids a race
	 * with timein_entry_ithrd() which scans the table up to the cursor.
	 */
	__synchronize();
	cip->ci_ithrd_cnt++;
	atomicClearInt(&cip->ci_flags, CA_ITHRD_CREATING);
}

static void
callout_info_init(callout_info_t *cip, int targcpu, char *name)
{
	ci_itinfo_t *citp;

	init_sema(&cip->ci_sema, 0, name, targcpu);
	cip->ci_flags |= ((targcpu << CA_CPU_SHIFT) & CA_CPU_MASK);

	/* allocate xthread info blocks */
	citp = (ci_itinfo_t *)kmem_zalloc(sizeof (ci_itinfo_t) *
					       CA_ITHRDS_PER_LIST, KM_SLEEP);

	/* now fill in ithrd info */
	cip->ci_ithrdinfo = citp;
	callout_itentry_init(cip, targcpu, name);
	cip->ci_flags |= CA_ENABLED;
	spinlock_init(&cip->ci_listlock, "cilock");
}

/*
 * thread_timein_init
 * This routine sets up each allocated callout list for
 * xthread handling.
 */
void
thread_timein_init(void)
{
	int i;

	for (i = 0; i < maxcpus; i++) {
		/* don't bother for CPUs that don't exist */
		if (!cpu_enabled(i))
			continue;
		callout_info_init(&CALLTODO(i), i, "timein");
	}

#if !defined(CLOCK_CTIME_IS_ABSOLUTE)
	/* setup fastclock handling */
	callout_info_init(fastcatodo, fastclock_processor, "ftimein");
#endif /* !CLOCK_CTIME_IS_ABSOLUTE */

#if (MAXCPUS > 128)
	if (numcpus <= 64)
		sched_tick_mask = 0;
	else if (numcpus <= 128)
		sched_tick_mask = 1;
	else if (numcpus <= 256)
		sched_tick_mask = 3;
	else
		sched_tick_mask = 7;
#endif
}

void
delay(long ticks)
{
	timespec_t ts;
	
	if (ticks == 0)
		return;
	tick_to_timespec(ticks, &ts, NSEC_PER_TICK);
	nano_delay(&ts);
}

/*
 * delay current thread. non-breakable
 */
void
nano_delay(timespec_t *ts)
{
	if (ts->tv_sec != 0 || ts->tv_nsec != 0) {
		kthread_t *kt = curthreadp;
		int s = kt_lock(kt);
		kt_timedwait(kt, 0, s, 1, ts, NULL);
	}
}

/*
 * Adjtime system call.
 * If the delta is reasonable, do it.
 */
struct adjtimea {
	struct timeval *delta;
	struct timeval *olddelta;
};



int
adjtime(struct adjtimea *uap)
{
	struct timeval atv;		/* new adjustment */
	struct timeval oatv;		/* old adjustment */
	long odelta;
	/*REFERENCED(!MP)*/
#if _MIPS_SIM == _ABI64
	int abi = get_current_abi();
#endif

	if (!_CAP_ABLE(CAP_TIME_MGT))
		return EPERM;
	if (COPYIN_XLATE(uap->delta, &atv, sizeof atv,
			 irix5_to_timeval_xlate, abi, 1)) {
		_SAT_CLOCK(0,EFAULT);
		return EFAULT;
	}

	/* prevent overflow */
	if (atv.tv_sec <= -0x7fffffff/USEC_PER_SEC
	    || atv.tv_sec >= 0x7fffffff/USEC_PER_SEC) {
		_SAT_CLOCK(atv.tv_sec,EINVAL);
		return EINVAL;
	}

	VHOST_ADJ_TIME(atv.tv_sec*USEC_PER_SEC + atv.tv_usec, &odelta);
	_SAT_CLOCK(atv.tv_sec,0);	/* Log successful change */
	/*
	 * Mark last adjtime so that onesec_maint will
	 * know to reset tod chip as needed
	 */
	lastadjtime = lbolt+DIDADJTIME;

	/*
	 * return remaining old correction if asked
	 */
	if (uap->olddelta) {
		oatv.tv_sec = odelta / USEC_PER_SEC;
		oatv.tv_usec = odelta % USEC_PER_SEC;
		if (XLATE_COPYOUT(&oatv, uap->olddelta, sizeof oatv,
				   timeval_to_irix5_xlate, abi, 1))
			return EFAULT;
	}
	return 0;
}


/*
 * Get current time in the BSD style.
 */
struct gettimeofdaya {
	void	*tvp;
};

/* ARGSUSED1 */
int
gettimeofday(struct gettimeofdaya *uap)
{
	struct timeval tv;

	/*
	 * in 64 bit mode the tv struct has an initial 32 bit pad - since
	 * in 6.1 we had tv_Sec be a long, we need to guarantee that the
	 * top 32 bits is 0 - so to be safe, we bzero it here
	 * Note that 64 bit apps running on 64 bit kernels simply end up
	 * calling copyout - so this 'tv' is the actualy copy used.
	 */
	bzero(&tv, sizeof(tv));
	microtime(&tv);
	if (XLATE_COPYOUT(&tv, uap->tvp, sizeof tv,
			   timeval_to_irix5_xlate, get_current_abi(), 1))
		return EFAULT;
	return 0;
}

/*
 * Set the current time, BSD style.
 * Called from syssgi(2), not directly from sysent.
 */
int
settimeofday(void *uap)
{
	struct timeval atv;
	/*REFERENCED(!MP)*/
	cpu_cookie_t was_running;

	if (!_CAP_ABLE(CAP_TIME_MGT))
		return EPERM;

	if (COPYIN_XLATE(uap, &atv, sizeof atv,
			 irix5_to_timeval_xlate, get_current_abi(), 1)) {
		_SAT_CLOCK(0,EFAULT);
		return EFAULT;
	}

	/* assume the libc wrapper will have rounded the value.  There
	 * is no reason to put code in the kernel unless necessary,
	 * and the super user can do far greater damage with the wrong
	 * time, than simply badly formatted time.
	 */
	was_running = setmustrun(clock_processor);
	settime(atv.tv_sec, atv.tv_usec);
	wtodc();
	restoremustrun(was_running);
	_SAT_CLOCK(atv.tv_sec,0);
	return 0;
}

#if _MIPS_SIM == _ABI64
/*ARGSUSED*/
int
irix5_to_timeval_xlate(
	enum xlate_mode mode,
	void *to,
	int count,
	register xlate_info_t *info)
{
	ASSERT(count == 1);
	ASSERT(info->smallbuf != NULL);

	ASSERT(mode == SETUP_BUFFER || mode == DO_XLATE);

	if (mode == SETUP_BUFFER) {
		ASSERT(info->copybuf == NULL);
		ASSERT(info->copysize == 0);
		if (sizeof(struct irix5_timeval) <= info->inbufsize)
			info->copybuf = info->smallbuf;
		else
			info->copybuf = kern_malloc(
						sizeof(struct irix5_timeval));
		info->copysize = sizeof(struct irix5_timeval);
		return 0;
	}

	ASSERT(info->copysize == sizeof(struct irix5_timeval));
	ASSERT(info->copybuf != NULL);

	irix5_to_timeval((struct timeval *)to,
			 (struct irix5_timeval *)info->copybuf);
	return 0;
}

/*ARGSUSED*/
int
timeval_to_irix5_xlate(
	void *from,
	int count,
	register xlate_info_t *info)
{
	ASSERT(count == 1);
	ASSERT(info->smallbuf != NULL);

	if (sizeof(struct irix5_timeval) <= info->inbufsize)
		info->copybuf = info->smallbuf;
	else
		info->copybuf = kern_malloc(sizeof(struct irix5_timeval));
	info->copysize = sizeof(struct irix5_timeval);

	timeval_to_irix5((struct timeval *)from,
			 (struct irix5_timeval *)info->copybuf);

	return 0;
}
#endif	/* _ABI64 */

/* 
 *The following code is support for migrating timeouts
*/

struct migrated_timeout {
	struct migrated_timeout *next; /* link to next entry */
	toid_t oldid;		      /* The id the timeout was assigned */
	toid_t newid;		      /* The id of timeout after migration */
	time_t time;		      /* Time we put on list */
};

static struct migrated_timeout *migrated_timeouts;

/*
 * given an old timeout id plus the time left before the
 * timeout add an item to the migrated timeouts list.
 * this list is used to forward untimeout requests after a timeout has
 * been migrated. It returns a pointer to the field for the new timeout
 * id that is filled in after we get one.
 */
void *
allocate_migrate_timeout(void)
{
	struct migrated_timeout *to;
	to = (struct migrated_timeout *)kmem_alloc(sizeof(*to), KM_SLEEP);
	return (to);

}
void 
free_migrate_timeout(void *ptr)
{
	kmem_free(ptr,sizeof(struct migrated_timeout));
}

volatile toid_t *
add_migrated_timeout(toid_t oldid, long sec, void *ptr)
{
	struct migrated_timeout *to;
	int s;
	
	to = (struct migrated_timeout *)ptr;
	to->oldid = oldid;
	to->newid = 0;
	to->time = lbolt / HZ + sec + 5; /* What time should 
					  * this entry expire 
					  */
	s = mutex_spinlock_spl(&migrated_timeout_lock, splprof);
	to->next =  migrated_timeouts;
	migrated_timeouts = to;
	mutex_spinunlock(&migrated_timeout_lock, s);

	return (&to->newid);
}
/*
 * given an ID, kill the corresponding migrated time-out
 */

static int
untimeout_migrated(toid_t id, int wait)
{
	struct migrated_timeout *p1, *last;
	struct migrated_timeout *free;	
	int return_val = 0;
	register int s;

	free = NULL;
	/* If we do not have any migrated timeouts then return */
	if (!migrated_timeouts) 
		return 0;
  startover:
	s = mutex_spinlock_spl(&migrated_timeout_lock, splprof);
	p1 = migrated_timeouts;
	/* Check if the head of the list is what we are looking for */
	if (p1 && p1->oldid == id) {
		free = p1;
		migrated_timeouts = p1->next;
	} else if (p1) {
		for ( last = p1 ; (p1 = p1->next) != 0 ; last = p1) {
			if((p1->oldid == id)) {
				last->next = p1->next;
				free = p1;
				break;
			}
			if (lbolt / HZ > p1->time) {
				/* 
				 * if it is past the time that this timeout 
				 * was due it can be removed.
				 */
				last->next = p1->next;
				/* Need to drop lock before free */
				mutex_spinunlock(&migrated_timeout_lock, s);
				free_migrate_timeout((void *)p1);
				/* As we dropped the lock
				 * we do not know the state of
				 * the list so we start all over
				 */
				goto startover;
			}
		}
	}
	mutex_spinunlock(&migrated_timeout_lock, s);
	if (free) {
		/*
		 * Avoid the short race where we might just have put the
		 * timeout onto the migrated queue
		 */
		while(free->newid == 0);
		return_val = untimeout_body(free->newid, wait);
		kmem_free (free, sizeof(*free));
	}
	return return_val;
}

static toid_t
find_migrated_timeout(toid_t id)
{
	struct migrated_timeout *p1;
	int s;

	/* Peek to see if it's worth grabbing the migrated_timeout_lock. */
	if ((p1 = migrated_timeouts) == NULL) 
		return 0;

	s = mutex_spinlock_spl(&migrated_timeout_lock, splprof);
	p1 = migrated_timeouts;
	while (p1) {
		if (p1->oldid == id)
			break;
		p1 = p1->next;
	}
	mutex_spinunlock(&migrated_timeout_lock, s);
	
	return (p1 ? p1->newid : 0);
}
