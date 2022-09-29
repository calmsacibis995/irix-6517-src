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

#ident	"$Revision: 3.411 $"

#include <sys/types.h>
#include <sys/atomic_ops.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/immu.h>
#include <sys/map.h>
#include <sys/par.h>
#include <sys/prf.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/pfdat.h>
#include <sys/prctl.h>
#include <sys/proc.h>
#include <sys/reg.h>
#include <sys/runq.h>
#include <sys/time.h>
#include <sys/sysinfo.h>
#include <sys/systm.h>
#include <ksys/vproc.h>
#include <sys/hwperftypes.h>
#include <sys/hwperfmacros.h>
#include <sys/kusharena.h> 
#include <ksys/sthread.h>
#include <ksys/xthread.h>
#include <ksys/isthread.h>
#include "os/proc/pproc_private.h"

/*
 * REACT/Pro timestamp
 */
#include <sys/rtmon.h>

#include <sys/timers.h>
#include <sys/var.h>
#include <ksys/exception.h>
#include <sys/space.h>

#ifdef _SHAREII
#include <sys/shareIIstubs.h>
#endif /*_SHAREII*/

#if TFP_MOVC_CPFAULT_FAST_WAR || _SHAREII
extern void restorefp(int);
#endif

#if EXTKSTKSIZE == 1
/* 
 * Need to include getsp() and schedctl.h for ishiband() macro which is 
 * used in kernel stack extension code.
 */
#include <sys/schedctl.h>

/* This macro defines the criteria for a kthread to own a stackext page */
#define	kthread_owns_stackext(kt)	(KT_ISBASERT(kt) &&		\
					 !KT_ISNBASEPRMPT(kt))
int setup_stackext(register uthread_t *);
uint stackext_alloc(int);
void stackext_free(uint);
#endif /* EXTKSTKSIZE == 1 */

static void presume(uthread_t *, kthread_t *);

static void utexitswtchfinish(void *, void *);
static void istexitswtchfinish(void *, void *);
extern void toidlestk(kthread_t *, void (*)(void *, void *), void *, void *);

extern void	rrmResume(uthread_t *, int);
extern void	rrmSuspend(uthread_t *, int);
#if USE_PTHREAD_RSA
extern int	find_runable_nid(kusharena_t *);
extern int	find_free_rsaid(kusharena_t *, int);
extern void rsa_upcall(uthread_t *ut, kusharena_t *kusp, k_machreg_t arg0,
		       k_machreg_t arg1, k_machreg_t arg2, k_machreg_t arg3);
#endif

extern int	icache_size;
#ifdef RT_SCALE
extern void	rt_setpri(short pri);
#endif

/*
 * given a new (possibly NULL) kthread, start it off
 */
void
resumenewthread(kthread_t *nkt, kthread_t *okt)
{
	ASSERT(!(private.p_flags & PDAF_BROADCAST_OFF));
	ASSERT(private.p_switching);
	ASSERT(issplhi(getsr()));
	ASSERT(nkt ? nkt->k_wchan == 0 : 1);
	ASSERT(nkt ? nkt->k_sonproc == private.p_cpuid : 1);

        KBLA_CRESUME(nkt);

	/*
	 * If no kthread is runnable, idle.
	 */
	if (nkt == NULL) {
#if EXTKSTKSIZE == 1
idle:
#endif
		resumeidle(okt);
	} else if (KT_ISSTHREAD(nkt)) {
		stresume(KT_TO_ST(nkt), okt);
	} else if (KT_ISXTHREAD(nkt)) {
		xtresume(KT_TO_XT(nkt), okt);
	} else {
#if EXTKSTKSIZE == 1
		/*
		 * make certain that kernel stack extension page is
		 * setup  for the incoming process. If not, then set it up.
		 */
		if (!setup_stackext(KT_TO_UT(nkt))) {
#ifdef DEBUG
			/* undo previous/aborted kt_startrun */
			nkt->k_sonproc = CPU_NONE;
#endif
			goto idle;
		}
#endif
		presume(KT_TO_UT(nkt), okt);
	}
	/* NOTREACHED */
}

/*
 * Put the current thread on the run queue and call the scheduler.
 */
void
qswtch(int flags)
{
	int s;
	kthread_t *kt = curthreadp;

	ASSERT(private.p_kstackflag == PDA_CURKERSTK);
	ASSERT( (caddr_t)getsp() > kt->k_stack &&
		(caddr_t)getsp() < (kt->k_stack + kt->k_stacksize));
	ASSERT(flags != RESCHED_P);
	/* put thread on runq at whatever priority it currently is at */
	s = kt_lock(kt);
	if (flags & RESCHED_Y)
		private.p_lticks = 0;

	if (KT_ISUTHREAD(kt)) 
		ut_endrun(KT_TO_UT(kt));
	else if (flags & RESCHED_KP)
		kt->k_preempt = 1;
	putrunq(kt, CPU_NONE);
	kt_nested_unlock(kt);
	swtch(flags);
	enableintr(s);
}

void
user_resched(int flags)
{
	int s;
	kthread_t *kt = curthreadp;

	ASSERT(private.p_kstackflag == PDA_CURKERSTK);
	ASSERT( (caddr_t)getsp() > kt->k_stack &&
		(caddr_t)getsp() < (kt->k_stack + kt->k_stacksize));
	ASSERT(KT_ISUTHREAD(kt));

	s = kt_lock(kt);
	if (flags & RESCHED_Y) 
		private.p_lticks = 0;

	if (private.p_lticks <= 0 || (flags & RESCHED_Y)) {
		kt_endtslice(kt);
		/*
		 * Gang scheduler entry point: might release and
		 * reaquire the kthread lock.
		 */
		if (IS_SPROC(KT_TO_UT(kt)->ut_pproxy) && gang_startrun(kt, s)) 
			return;
	} else {
		if ((flags & RESCHED_P) && private.p_lticks > 0)
			kt->k_preempt = 1;
	}
	KT_TO_UT(kt)->ut_rticks = private.p_lticks;

	if (is_batch(kt)) 
		batch_meter(kt, s);
	else if (!is_bcritical(kt) || !batch_critical_meter(kt, s)) {
		putrunq(kt, CPU_NONE);
		kt_nested_unlock(kt);
		swtch(flags);
	}	
	enableintr(s);
}

/*
 * Isolated processors must always execute in the system with BROADCAST
 * interrupts enabled or we'll miss tlb flushes that clean kernel virtual
 * addresses, etc.
 * We check explicitly in the beginning of syscall, trap, tlbmiss, tlbmod, intr
 * but with kernel preemption there are windows  before our calls that
 * we could get preempted. We have that preemption in VEC_int call kpswtch
 * so we can do the right thing.
 * It is CRITICAL that all interrupts be disabled when calling this
 * routine.
 * Note that is relies on the fact that preempted threads return to
 * here.
 */
void
kpswtch()
{
	SYSINFO.kpreempt++;
#ifdef	MP
	if (private.p_flags & PDAF_BROADCAST_OFF)
		check_delay_tlbflush(ENTRANCE);
#endif
	qswtch(RESCHED_KP);
}


/* ARGSUSED */
void
ut0exitswtchfinish(
	void	*arg1,
	void	*arg2)
{
	uthread_t	*ut;

	ut = (uthread_t *)arg1;

	uthread_destroy(ut);

	resumenewthread(getrunq(), NULL);
	/* NOTREACHED */
}

void
ut0exitswtch(void)
{
	kthread_t *kt = curthreadp;
	uthread_t *ut = private.p_curuthread;

	ASSERT(issplhi(getsr()));
	ASSERT(curuthread);
#if DEBUG
	private.p_switching = 1;
#endif
	ASSERT(private.p_nextthread == kt || private.p_nextthread == NULL);
	private.p_nextthread = 0;
	kt->k_sonproc = CPU_NONE;

	/*
	 * If the scheduler knew something about this process, let it know
	 * of this process' demise.
	 */
	exitrunq(kt);

	toidlestk(kt, ut0exitswtchfinish, ut, NULL);
}

/*
 * utexitswtch - uthread has exited, toss upages and look for another thread
 *	to schedule.
 */
void
utexitswtch(void)
{
	kthread_t *kt = curthreadp;
	uthread_t *ut = private.p_curuthread;

	/* don't preempt while we're in the middle of switching */
	ASSERT(issplhi(getsr()));

	SYSINFO.pswitch++;

	ASSERT(ut);
	ASSERT(kt->k_sqself);
	ASSERT(kt->k_sonproc == cpuid());
	ASSERT(kt->k_lastrun == cpuid());

	ASSERT(curuthread->ut_as.utas_segtbl == NULL);
#if DEBUG
	private.p_switching = 1;
#endif
	ASSERT(private.p_nextthread == kt || private.p_nextthread == NULL);
	private.p_nextthread = 0;

	/*
	 * If the scheduler knew something about this process, let it know
	 * of this thread's demise.
	 */
	exitrunq(kt);
	kt->k_sonproc = CPU_NONE;

#ifdef _SHAREII
	/* 
	** Tell Share the thread is exiting.
	*/
	SHR_PCNTDN(ut);
	SHR_THREADEXIT(ut);
#endif /*_SHAREII*/

	/*
	 * Transfer to idle stack.
	 * This NULLs p_curkthread, p_curuthread
	 */
	ASSERT(issplhi(getsr()));

	toidlestk(kt, utexitswtchfinish, ut, NULL);
	/* NOTREACHED */
}

sv_t	reap_wait;
lock_t	reapqlock;
kthread_t *reapq;

/* ARGSUSED */
static void
utexitswtchfinish(void *a, void *b)
{
	uthread_t *ut = (uthread_t *)a;
	utas_t *utas = &ut->ut_as;
	kthread_t *nkt, *kt = UT_TO_KT(ut);

	ASSERT(issplhi(getsr()));

	/*
	 * WARNING - now on idle stack
	 */
	private.p_curpri = 0;
#ifdef RT_SCALE
	/* Inform the rt scheduler that p_curpri has changed */
	rt_setpri(0);
#endif
	/*
	 * switching away from non-standard utlbmiss handler?
	 */
	if (utas->utas_flag & UTAS_TLBMISS) {
		/* thread will never run again so this is just for debug */
		utas->utas_flag &= ~UTAS_TLBMISS;

		if (private.p_utlbmissswtch & UTLBMISS_COUNT) {
			private.p_utlbmisses = 0;
			private.p_ktlbmisses = 0;
		}
		utlbmiss_reset();
	}

	ASSERT(private.p_utlbmissswtch == 0);

	nested_spinlock(&reapqlock);
	ASSERT(kt->k_flink == kt);
	if (!(nkt = reapq)) {
		reapq = kt;
		sv_signal(&reap_wait);
	} else {
		kt->k_flink = nkt;
		kt->k_blink = nkt->k_blink;
		nkt->k_blink->k_flink = kt;
		nkt->k_blink = kt;
		ASSERT(sv_signal(&reap_wait) == 0);
	}
	nested_spinunlock(&reapqlock);

	nkt = getrunq();
	if (!nkt)
		nkt = idle();	/* already on idle stack */
	resumenewthread(nkt, NULL);
	/* NOTREACHED */
}

void
kthread_reap()
{
	kthread_t *kt;
	vproc_t *vp;
	int s;

	for (;;) {
		s = mutex_spinlock(&reapqlock);
		if (kt = reapq) {
			if (kt->k_flink == kt)
				reapq = 0;
			else {
				reapq = kt->k_flink;
				reapq->k_blink = kt->k_blink;
				kt->k_blink->k_flink = reapq;
				kt->k_flink = kt->k_blink = kt;
			}
			mutex_spinunlock(&reapqlock, s);

			switch (kt->k_type) {
			case KT_UTHREAD:
				vp = UT_TO_VPROC(KT_TO_UT(kt));
				uthread_destroy(KT_TO_UT(kt));
				if (vp)
					vproc_destroy(vp);
				break;
			case KT_STHREAD:
				stdestroy(KT_TO_ST(kt));
				break;
			case KT_XTHREAD:
				xtdestroy(KT_TO_XT(kt));
				break;
#ifdef DEBUG
			default:
				panic("unknown kthread type");
				break;
#endif
			}
		} else {
			sv_wait(&reap_wait, 0, &reapqlock, s);
		}
	}
}

void
kthread_reap_init()
{
	extern int reaper_pri;

	init_sv(&reap_wait, SV_DEFAULT, "reap", 0);
	spinlock_init(&reapqlock, "reapqlock");
	sthread_create("reaper", 0, (1024 * sizeof(void *)), 0, reaper_pri,
			KT_PS, (st_func_t *)kthread_reap, 0, 0, 0, 0);
}

/*
 * The "movc" instruction does not generate a coprocessor
 * unusable interrupt, so we need to preload the FP for
 * mips4 programs which have referenced the FP anytime
 * in the past and which do not currently own the FP.
 */
#if TFP_MOVC_CPFAULT_FAST_WAR
void
tfp_loadfp(void)
{
	uthread_t *ut = curuthread;
	proc_t *p = UT_TO_PROC(ut);

	int fr = ABI_IS_IRIX5_64(ut->ut_pproxy->prxy_abi) ||
		ABI_IS_IRIX5_N32(ut->ut_pproxy->prxy_abi);

	if (p->p_stat != SZOMB
	    && (private.p_fpowner != curuthread)
	    && fr	/* really: can it use mips4 instructions? */
	    && (PCB(pcb_ownedfp))) {
		checkfp(private.p_fpowner, 0);	/* first unload */
		restorefp(fr ? SR_FR : 0);
		USER_REG(EF_SR) |= SR_CU1;
	}
}
#endif	/* TFP_MOVC_CPFAULT_FAST_WAR */

#if USE_PTHREAD_RSA

extern void syscall_save_rsa( uthread_t *, rsa_t *);
extern void save_rsa_fp( uthread_t *, rsa_t *);
extern void restore_rsa_fp( uthread_t *, rsa_t * );
extern void save_rsa( uthread_t *, rsa_t *);
extern void restore_rsa( uthread_t *, rsa_t * );
/*
 * This resume is only a part of kernel initiated context switches.
 */
void
swtch_rsa_resume( uthread_t *ut)
{
	int newnid, newrsa;
	kusharena_t *kusp;
	uint64_t mask, oldrbits;

	/* Always update nallocated for a ut_sharena process */

	kusp = ut->ut_sharena;
	atomicAddInt((int *) &kusp->nallocated,1);

#ifdef PTHREAD_RSA_DEBUG
	if (kusp->fbits[0] & 0x01)
		cmn_err(CE_PANIC, "upcall RSA has been freed!\n");
#endif

	/* If uthread was not prempted in user space OR if NID is invalid
	 * then we simply return to restart a "normal" uthread.
	 */

	newnid = ut->ut_prda->t_sys.t_nid;
	if (!ut->ut_rsa_runable || (newnid <= NULL_NID) || (newnid > NT_MAXNIDS))
		return;

	/* Attempt to re-obtain our NID and it's associated RSA.  If we fail
	 * (some other uthread resumed our NID) then we set nid to NULL_NID
	 * and load the upcall RSA into the uthread context.
	 */

	mask = (1LL << (newnid % bitsizeof(uint64_t)));
	oldrbits = atomicClearUint64(
		&kusp->rbits[newnid/bitsizeof(uint64_t)], mask);

	if (!(oldrbits & mask)) {

		/* someone "stole" our NID
		 */
		/*
		 * restore context from upcall RSA and let library
		 * code determine next course of action.
		 *
		 * NOTE: We set ut_rsa_runable to 4 rather than 0.  This is
		 * because we MIGHT get kpreempted inside the dyield syscall
		 * and we need to set a non-zero value into ut_rsa_runable
		 * in order to perform a "full restore" from the syscall.
		 * This value of 4 is only used to indicate "stolen NID".
		 */
		ut->ut_rsa_runable = 4;
		rsa_upcall(ut, kusp, RSAupcall_no_context, NULL, NULL, NULL);
	} else {
		if ((newrsa = kusp->nid_to_rsaid[newnid]) >= ut->ut_maxrsaid)
		{
				
/*			cmn_err(CE_WARN,"bad rsa on restore (newnid %d newrsa 0x%x), load upcall RSA\n", newnid, newrsa); */

			/* if can't find someone to run, restore
			 * context from upcall RSA and let library
			 * code determine next course of action.
			 */
			ut->ut_rsa_runable = 0;
			rsa_upcall(ut, kusp, RSAupcall_inval_rsaid, newnid,
				   newrsa, NULL);

			restore_rsa( ut, &kusp->rsa[0].rsa );

		} else switch(ut->ut_rsa_runable) {
		case 0:
			/* has a nid, but not pre-empted */
			break;
		case 1:
			restore_rsa_fp( ut, &kusp->rsa[newrsa].rsa);
			break;

		case 2:
			/* Move registers from RSA to UT_EXCEPTION.
			 * RSA was pre-allocated by library.
			 */
			restore_rsa( ut, &kusp->rsa[newrsa].rsa);
			break;
		case 3:

			/* Move registers from RSA to UT_EXCEPTION.
			 * RSA was "allocate on demand" so free it up.
			 */
			restore_rsa( ut, &kusp->rsa[newrsa].rsa);
			kusp->nid_to_rsaid[newnid] = 0;

			atomicSetUint64(
				&kusp->fbits[newrsa/bitsizeof(uint64_t)],
				1LL<<(newrsa % bitsizeof(uint64_t)));
			/* We could get kpreempted before actually executing
			 * backtouser code, so reset ut_rsa_runable to the
			 * original state prior to allocating the RSA.
			 */
			ut->ut_rsa_runable = 2;
			break;

		default:
			cmn_err(CE_PANIC, "swtch_rsa_resume: ut_rsa_runable = %d\n",
				ut->ut_rsa_runable);
		}
	}

#ifdef PTHREAD_RSA_DEBUG
	newnid = ut->ut_prda->t_sys.t_nid;
	if (kusp->fbits[0] & 0x01)
		cmn_err(CE_PANIC, "upcall RSA has been freed!\n");

	if ((newnid != NULL_NID) &&
	    (kusp->rbits[newnid/64] & (1LL << (newnid%64))))
		cmn_err(CE_PANIC,"bad rbits for newnid %d\n", newnid);
#endif /* PTHREAD_RSA_DEBUG */
}

void
swtch_rsa_save( uthread_t *ut)
{
	int nid, rsaid;
	kusharena_t *kusp;

	/* always update nallocated for ut_sharena processes */

	kusp = ut->ut_sharena;
	atomicAddInt((int *) &kusp->nallocated,-1);

#ifdef PTHREAD_RSA_DEBUG
	if (kusp->fbits[0] & 0x01)
		cmn_err(CE_PANIC, "upcall RSA has been freed!\n");
#endif /* PTHREAD_RSA_DEBUG */
	/* If uthread was not prempted in user space OR if NID is invalid
	 * then we simply return to restart a "normal" uthread.
	 */

	nid = ut->ut_prda->t_sys.t_nid;
	if (!ut->ut_rsa_runable || (nid <= NULL_NID) || (nid > NT_MAXNIDS))
		return;

#ifdef PTHREAD_RSA_DEBUG
	{
	uint64_t temp;

	temp = kusp->rbits[nid/64];

	if (temp & (1LL << (nid%64)))
		cmn_err(CE_PANIC,"swtch: nid %d has bad rbits 0x%x 0x%x\n",
			nid, temp, kusp->rbits[nid/64]);
	}
#endif /* PTHREAD_RSA_DEBUG */
	/* if rsa_runable is set, then process entered kernel through
	 * an interrupt NOT and exception.  This means that the
	 * user registers are in the RSA and that this thread of
	 * execution could be resumed by another process context
	 * belonging to this sproc group.
	 */
	if (ut->ut_rsa_runable == 1) {
		/* RSA is preallocated by library  AND is saved directly in
		 * locore code using a "split eframe"
		 */

		extern int checkfp_rsa(struct uthread_s *, rsa_t *);


		/* unload FP state.  If can't be unloaded directly
		 * into the RSA, we must copy it from the PCB.
		 */

		nid = ut->ut_prda->t_sys.t_nid;
		rsaid = kusp->nid_to_rsaid[nid];

		if (rsaid == NULL_RSA) {
			cmn_err(CE_WARN, "swtch: preemption with ut_rsa_runable set but NO valid RSA\n");
		} else {
			/* Only mark NID as runable if it has a
			 * valid RSA allocated.
			 */
			if (!checkfp_rsa(ut, &kusp->rsa[rsaid].rsa))
				save_rsa_fp(ut, &kusp->rsa[rsaid].rsa);

			ASSERT(kusp->rsa[rsaid].rsa_gregs[CTX_R0] == 0);
			atomicSetUint64(
				&kusp->rbits[nid/bitsizeof(uint64_t)],
				1LL<<(nid % bitsizeof(uint64_t)));
		}

	} else if (ut->ut_rsa_runable == 2) {

		/* RSA is stored via copy from exception eframe */

		nid = ut->ut_prda->t_sys.t_nid;
		ASSERT(kusp->nid_to_rsaid[nid] == NULL_RSA);

		if ((rsaid = kusp->nid_to_rsaid[nid]) == NULL_RSA) {

			/* RSA management is "allocate on demand" */

			if (rsaid = find_free_rsaid(kusp, ut->ut_maxrsaid-1)) {

				kusp->nid_to_rsaid[ nid ] = rsaid;
				ut->ut_rsa_runable = 3;
			} else
				cmn_err_tag(1793, CE_WARN,"runable == 2, no RSA available\n");
		}

		if (rsaid != NULL_RSA) {

			checkfp(ut,0);
			save_rsa( ut, &kusp->rsa[rsaid].rsa);

			atomicSetUint64(
				&kusp->rbits[nid / bitsizeof(uint64_t)],
				1LL<<(nid % bitsizeof(uint64_t)));
		} else
			cmn_err_tag(1794, CE_WARN,"runable == 2, no RSA available\n");
	} else {
		cmn_err(CE_PANIC,"swtch_rsa_save: illegal ut_rsa_runable == %d\n",
			ut->ut_rsa_runable);
	}
}
#endif /* USE_PTHREAD_RSA */

/*
 * {passivate, quicken}_uthread() are broken out of swtch() so that the
 * passivation can be done lazily by an ithread when a uthread is interrupted,
 * and so that uthreads that "resume" elsewhere than after the save() in swtch()
 * (because they were interrupted rather than voluntarily calling save())
 * can be properly restored to runnability.
 *
 * IWBN if some kindly and sage uthread/proc gurus cast an eye on them to
 * be sure that nothing got dropped in the break-out.
 */
void
passivate_uthread(kthread_t *kt)
{
	struct proc *p;
	uthread_t *ut;
	utas_t *utas;

	ASSERT(KT_ISUTHREAD(kt));
	kt->k_sonproc = CPU_NONE;
	ut = KT_TO_UT(kt);
	p = UT_TO_PROC(ut);

#ifdef _SHAREII
	/* 
	 * This process is no longer running.
	 * If it's also not on a run queue, tell share that there is one fewer 
	 * runnable thread.
	 */
	if (UT_TO_KT(ut)->k_onrq == -1)
		SHR_PCNTDN(ut);
	SHR_SWTCH(ut);
#endif /*_SHAREII*/

	/*
	 * If ut is being counted using performance counters then before 
	 * doing the context switch the values should be read and the
	 * virtual counters should be incremented.
         */
	SUSPEND_HWPERF_COUNTERS(ut);

#ifdef USE_PTHREAD_RSA
	/*
	 * Too bad. getting pre-empted. Mark in kusharena if special process.
	 */
	if (ut->ut_sharena && ut->ut_prda)
	{
		swtch_rsa_save( ut );
	}
#endif
	/*
	 * Switching away from fast profile process,
	 * stop fast clock.
	 * XXX There may be no proc if sleeping during proc exit 
	 * XXX but in this case fast profiling will be off, anyhow.
	 * XXX SPROFFAST ought to be moved to avoid referencing the proc.
	 */
	if (p != NULL && p->p_flag & SPROFFAST)
		stopprfclk();

	/*
	 * If thread is using a non-standard utlbmiss handler,
	 * turn it off when switching out, so threads switching
	 * in don't have to go through a lot of trouble to
	 * determine that they have the correct handler.
	 */
	utas = &ut->ut_as;
	if (utas->utas_flag & UTAS_TLBMISS) {
		if (private.p_utlbmissswtch & UTLBMISS_COUNT) {
			if (private.p_utlbmisses) {
				ut->ut_acct.ua_ufaults += private.p_utlbmisses;
				private.p_utlbmisses = 0;
			}
			if (private.p_ktlbmisses) {
				ut->ut_acct.ua_kfaults += private.p_ktlbmisses;
				private.p_ktlbmisses = 0;
			}
		}
		/*
		 * If this is a delayed tlbmiss handler swtch for large
		 * pages the process may not have switched yet. So
		 * skip utlbmiss_reset.
		 */
		if (!((utas->utas_flag & UTAS_LPG_TLBMISS_SWTCH) && 
		    (private.p_utlbmissswtch == 0)))
			utlbmiss_reset();
	}
	ASSERT(private.p_utlbmissswtch == 0);

	/*
	 * If there is more than one CPU, pull the floating point
	 * registers if the process touched them.  On MP systems, it
	 * is a bad idea to create reasons for an idle processor to be
	 * unable to run this process.
	 */
#if TFP
	/* On TFP we always unload the FP, even for Uniprocessor.  The
	 * problem is that if we keep the FP loaded, then run a user
	 * with a different value for the DM (Debug Mode) FP bit, then
	 * we can cause an (unintended) FP interrupt since toggling
	 * the state of the DM bit changes which bits in the fpc_csr
	 * acutally generate an interrupt (in one case it's the CAUSE
	 * bits, in the other the flags).
	 */
	checkfp(ut,0);
#else
	ON_MP(checkfp(ut, 0));
#endif

#if JUMP_WAR
	clr_jump_war_wired();
#endif

#if EXTKSTKSIZE == 1
#define	kthread_owns_stackext(kt)	(KT_ISBASERT(kt) &&		\
					 !KT_ISNBASEPRMPT(kt))

	/*
	 * Check to see if the out going process should own the current
	 * kernel stack extension page. i.e. is it currently using the
	 * extension page or is it a real-time process.
	 */
	if ((((__psunsigned_t)getsp() <= (__psunsigned_t)KSTACKPAGE) || 
						(kthread_owns_stackext(kt)))) {
		if (!ut->ut_kstkpgs[KSTEIDX].pgi)
			ut->ut_kstkpgs[KSTEIDX].pgi = private.p_stackext.pgi;
		else {
			if (ut->ut_kstkpgs[KSTEIDX].pgi !=
			    private.p_stackext.pgi)
				cmn_err(CE_PANIC,
				"Stack Extension Page is inconsistent");
		}
		private.p_stackext.pgi = 0;
	} else {
		/*
		 * He must have changed his priority since he was
		 * last run.  Now he can no longer hold onto the
		 * page, so take it from him.
		 */
		ut->ut_kstkpgs[KSTEIDX].pgi = 0;
	}
#endif
}

void
quicken_uthread(kthread_t *kt, int flags)
{
	struct proc *p;
	uthread_t *ut;
	utas_t *utas;

	ut = KT_TO_UT(kt);
	p = UT_TO_PROC(ut);

	/*
	 * If the context switch is to a process where
	 * the performance counters have been enabled
	 * before, then restart them.
	 */
	START_HWPERF_COUNTERS(ut);

	/* set up coproc usable flags */
#define	USERREG(ut, x)	(((k_machreg_t *)&ut->ut_exception->u_eframe)[(x)])
	if (private.p_fpowner == ut)
		USERREG(ut, EF_SR) |= SR_CU1;
	else
		USERREG(ut, EF_SR) &= ~SR_CU1;
	utas = &ut->ut_as;
	if (utas->utas_flag & UTAS_TLBMISS) {
		/*
		 * Delayed large page tlbmiss switch.
		 * Make sure the tlbs are flushed for this process
		 * alone.
		 */
		if (utas->utas_flag & UTAS_LPG_TLBMISS_SWTCH) {
			utas_flagclr(utas, UTAS_LPG_TLBMISS_SWTCH);
			flushtlb_current();
		}

		ASSERT(utas->utas_utlbmissswtch);
		utlbmiss_resume_nopin(utas);
	}

	/*
	 * Bundle a number of unlikely events into p_flag2 bits,
	 * so we can optimize special-action checking.
	 * The only unlikely event left is SPROFFAST, but will
	 * leave the comment above in case someone adds more...
	 * XXX There may be no proc during proc exit 
	 * XXX but in this case fast profiling will be off.
	 * XXX SPROFFAST ought to be moved.
	 */
	if (p != NULL && p->p_flag & SPROFFAST) {
		/*
		 * If resuming proc that requested
		 * fast profiling, start fast clk.
		 */
		startprfclk();
	}

#ifdef _SHAREII
	/*
	 * If RELOADFP is set, then load the FP before the
	 * process starts. This allows the SHAREII system
	 * uthread to use FP in-kernel (no coproc unusable
	 * exception).
	 * XXX: Should check p_abi to see if SR_FR is really
	 * needed, but not necessary here since SHARE is the
	 * only user of this facility.
	 */
	if (ut->ut_flags & UT_RELOADFP) {
		if (private.p_fpowner)
			checkfp(private.p_fpowner, 0);
		restorefp(SR_FR);
	}
#endif

	if (kt->k_runcond & RQF_GFX)
		rrmResume(ut, flags);	/* graphics resume */

	if (is_bcritical(kt) && kt->k_binding == CPU_NONE) 
		private.p_runrun = 1;

#ifdef USE_PTHREAD_RSA
	if (ut->ut_sharena && (ut->ut_prda)) {
		swtch_rsa_resume( ut );
	}
#endif
	return;
}

/*
 * This routine is called from a process to reschedule the CPU.
 * If the calling process is not in RUN state, arrangements for it to restart
 * must have been made elsewhere, usually by calling via sleep.
 *
 * On return, spl() is preserved, unless we "return" to run some other
 * process.
 */
void
swtch(int flags)
{
	kthread_t *nkt, *kt = curthreadp;
	uthread_t *ut;

	/*
	 * we should always have broadcast interrupts on in the kernel if we're
	 * an isolated processor
	 */
	ASSERT(!(private.p_flags & PDAF_BROADCAST_OFF));
	ASSERT(issplhi(getsr()));
	ASSERT(getsr() & SR_IE);
	/*
	 * we should never, ever deschedule the panicing/dumping thread
	 */
	ASSERT(!in_panic_mode());

	KBLA_SAVE(kt);

	private.p_nextthread = 0;

	if (private.p_prfswtch) {
		#pragma mips_frequency_hint NEVER
		if (--private.p_prfswtchcnt == 0) {
			/*
			 * Do a profiling ``interrupt'' with our caller's
			 * return address and stack pointer as the PC and SP.
			 * We pass in 0 for the RA since only the stacktrace
			 * profiling code uses it and then only for functions
			 * where it can't find the stack frame information.
			 * Given that our caller has called us, the stacktrace
			 * code should be able to find the stack frame
			 * information.  Finally, we pass in 0 for the SR
			 * since we want to indicate kernel mode for the
			 * swtch() profile and all current platforms use 0 for
			 * the SR_KSU_MASK bits.
			 *
			 * XXX We're using a bogus hack to get a (PC, SP)
			 * XXX pair for prfintr().  We use an ancillary
			 * XXX assembly routine to get swtch()'s (PC, SP)
			 * XXX at the point of the call and then use that.
			 * XXX Unfortunately this makes ``function''
			 * XXX profiling worthless.  As soon as bug #507810,
			 * XXX ``want __return_sp intrinsic,'' is available
			 * XXX we need to switch to using that.
			 *
			 * XXX Right now, with this code at this point in
			 * XXX swtch(), we're counting all calls to swtch() --
			 * XXX even those that don't result in a context
			 * XXX switch (the majority of these come from
			 * XXX time-slice pre-emption calls which end up
			 * XXX switching back to the same process.  This ends
			 * XXX up distorting our profiling data.
			 * XXX Unfortunately, if we want to wait till we know
			 * XXX that we're switching, we have to duplicate this
			 * XXX code in istswtch() since we dive into that
			 * XXX routine immediately following this point.
			 * XXX Moreover, if we want PC-based profiling to have
			 * XXX any value at all, we'd need to pass in our
			 * XXX caller's PC and SP values which would add to
			 * XXX the overhead of that call even when not doing
			 * XXX profiling; albeit minorly.  This is an issue
			 * XXX still to be thought out more ...
			 */
			__psunsigned_t pc, sp;
			getpcsp(&pc, &sp);
			prfintr(pc, sp, 0, 0);
		}
	}

	if (!KT_ISUTHREAD(kt)) {
		istswtch(flags);
		return;
	}

	ut = curuthread;
	ASSERT(ut == KT_TO_UT(kt));

#ifdef CELL
	ASSERT(!(kt->k_flags & KT_SLEEP)
		|| (kt->k_flags & KT_NWAKE)
		|| (bla_curlocksheld() > 0) == (kt->k_bla.kb_in_gbla));
#endif

	ASSERT(private.p_kstackflag == PDA_CURKERSTK);

	ASSERT(kt->k_sqself);
	ASSERT(kt->k_sonproc == cpuid());

	/*
	 * We want to force a gfx process to suspend here, even if this
	 * process might actually be able to run again.  This will force it
	 * to give up the graphics semaphore temporarily, which other gfx
	 * processes may be sleeping on.
	 */
	if (kt->k_runcond & RQF_GFX)
		rrmSuspend(ut, flags);

	ASSERT(private.p_switching == 0);
	private.p_switchuthread = NULL;
#if DEBUG
	private.p_switching = 1;
#endif
	/*
	 * We are no longer running on this processor for all intents and
	 * purposes, k_sqself protects our kernel stack.
	 */
	kt->k_sonproc = CPU_NONE;

	/*
	 * If tracing resched's, dump old process's
	 * priorities and reasons for resched.
	 */
	if (IS_TSTAMP_EVENT_ACTIVE(RTMON_TASK|RTMON_TASKPROC)) {
		#pragma mips_frequency_hint NEVER
		if (IS_TSTAMP_EVENT_ACTIVE(RTMON_TASK) ||
		    (UT_TO_PROC(ut)->p_flag & SPARSWTCH))
			fawlty_resched(kt, flags);
	}

	nkt = getrunq();
	ASSERT(private.p_switching == 1);

	/*
	 * If the same process is to run again, don't want to go thru save
	 * and resume.  Just return.
	 */
	if (nkt == kt) {
		utas_t *utas = &ut->ut_as;
		/*
		 * If rerunning the same process, we don't need to kickidle.
		 */
		if (tlbpid_is_usable(utas)) {
			tlbpid_use(utas, tlbpid(utas));
		} else {
			new_tlbpid(utas, VM_TLBVALID | VM_TLBNOSET);
		}
#if TFP
		/*
		 * Assign a new ICACHE PID to this process if necessary.
		 */
		if (icachepid_is_usable(utas))
			icachepid_use(utas, icachepid(utas));
		else
			new_icachepid(utas, VM_TLBVALID | VM_TLBNOSET);
#endif
		ASSERT(kt->k_sqself);
#if DEBUG
		private.p_switching = 0;
#endif
		if (IS_TSTAMP_EVENT_ACTIVE(RTMON_TASK|RTMON_TASKPROC)) {
			#pragma mips_frequency_hint NEVER
			if (IS_TSTAMP_EVENT_ACTIVE(RTMON_TASK) ||
			    (UT_TO_PROC(ut)->p_flag & SPARSWTCH))
				log_tstamp_event(TSTAMP_EV_EOSWITCH, kt->k_id,
						 RTMON_PACKPRI(kt, private.p_idletkn),
						 current_pid(), NULL);
		}

		if (is_bcritical(kt) && kt->k_binding == CPU_NONE) 
			private.p_runrun = 1;
		/*
		 * We had to suspend to release the graphics semaphore,
		 * and we want to run again, so we'll need to resume.
		 */
		if (kt->k_runcond & RQF_GFX)
			rrmResume(ut, flags);
		return;
	}

	/* 
	 * there are cases where the process voluntarily switches out
	 * without putting itself on the runq(sginap, delay)
	 */
	if (flags & (SEMAWAIT|MUTEXWAIT|SVWAIT|RESCHED_Y)) {
		atomicAddLong(&ut->ut_pproxy->prxy_ru.ru_nvcsw, 1);
	} else {
		atomicAddLong(&ut->ut_pproxy->prxy_ru.ru_nivcsw, 1);
	}
	SYSINFO.pswitch++;

	/*
	 * All processes go through this save which stashes their
	 * context and prepares for them to be restarted or trashed.
	 */

	/* 0 on save, 1 on resume */
	if (save(kt->k_regs)) {
		ASSERT(issplhi(getsr()));
		ASSERT(private.p_kstackflag == PDA_CURKERSTK);
		ASSERT(curthreadp == kt && curuthread == ut);
#if DEBUG
		private.p_switching = 0;
#endif
		ASSERT(kt->k_wchan == 0);
		ASSERT(kt->k_timers.kp_curtimer == AS_SYS_RUN);
		ASSERT(!(kt->k_flags & KT_SLEEP));

		if (IS_TSTAMP_EVENT_ACTIVE(RTMON_TASK|RTMON_TASKPROC)) {
			#pragma mips_frequency_hint NEVER
			if (IS_TSTAMP_EVENT_ACTIVE(RTMON_TASK) ||
			    (UT_TO_PROC(ut)->p_flag & SPARSWTCH))
				log_tstamp_event(TSTAMP_EV_EOSWITCH, kt->k_id,
						 RTMON_PACKPRI(kt, private.p_idletkn),
						 current_pid(), NULL);
		}

		quicken_uthread(kt, flags);
		return;
	}

	passivate_uthread(kt);
	resumenewthread(nkt, kt);
	/* NOTREACHED */
}

#if USE_PTHREAD_RSA

void
restore_rsa( uthread_t *ut, rsa_t *rsa )
{
	/* copy user registers */

	bcopy(&rsa->rsa_context.__gregs[CTX_AT], &ut->ut_exception->u_eframe,
		(EF_RA-EF_AT+1)*sizeof(k_machreg_t));

	ut->ut_exception->u_eframe.ef_mdlo = rsa->rsa_context.__gregs[CTX_MDLO];
	ut->ut_exception->u_eframe.ef_mdhi = rsa->rsa_context.__gregs[CTX_MDHI];
	ut->ut_exception->u_eframe.ef_epc = rsa->rsa_context.__gregs[CTX_EPC];

	/* reload FP state from RSA into PCB */

	restore_rsa_fp(	ut, rsa );
}


void
save_rsa( uthread_t *ut, rsa_t *rsa )
{
	/* copy user registers */

	bcopy(&ut->ut_exception->u_eframe, &rsa->rsa_context.__gregs[CTX_AT],
		(EF_RA-EF_AT+1)*sizeof(k_machreg_t));


	rsa->rsa_context.__gregs[CTX_MDLO] = ut->ut_exception->u_eframe.ef_mdlo;
	rsa->rsa_context.__gregs[CTX_MDHI] = ut->ut_exception->u_eframe.ef_mdhi;
	rsa->rsa_context.__gregs[CTX_EPC] = ut->ut_exception->u_eframe.ef_epc;

	/* reload FP state from RSA into PCB */

	save_rsa_fp( ut, rsa );
}


/* This routine searches the kusharena for a unallocated RSAID
 */
int
find_free_rsaid(kusharena_t *kusp, int maxrsaid)
{
	int i, istart, iend, rbit_index;
	__int64_t mask, oldfbits;

#ifdef PTHREAD_RSA_DEBUG
	if (kusp->fbits[0] & 0x01)
		cmn_err(CE_PANIC, "upcall RSA has been freed!\n");
#endif /* PTHREAD_RSA_DEBUG */
	istart = NULL_RSA+1;	/* NULL_RSA is reserved for upcalls */
	iend = maxrsaid;

again:
	for (i=istart; i <= iend; i++) {
		rbit_index = i / bitsizeof(uint64_t);
		mask = (1LL << (i % bitsizeof(uint64_t)));
		oldfbits = atomicClearUint64(&kusp->fbits[rbit_index], mask );
		if (oldfbits & mask)
			return( i );
	}
	if (istart != (NULL_RSA+1)) {
		/* search first portion of rsaid space */
		iend = istart;
		istart = NULL_RSA+1;
		goto again;
	}
	return 0;
}

/* This routine searches the kusharena for a "runable" nanothread (rsaid)
 * and if it finds one it clears the associated bit in the kusharena rbits
 * so that no other processor will find it "runable".
 */
#if 0

int last_nid = 0;

int
find_runable_nid(kusharena_t *kusp)
{
	int i, istart, iend, rbit_index;
	__int64_t mask, oldrbits;

	if ((istart = last_nid+1) > NT_MAXNIDS)
		iend = 0;
	iend = NT_MAXNIDS;

again:
	for (i=istart; i < iend; i++) {
		rbit_index = i / bitsizeof(uint64_t);
		mask = (1LL << (i % bitsizeof(uint64_t)));
		oldrbits = atomicClearUint64(&kusp->rbits[rbit_index],mask );
		if (oldrbits & mask) {
			last_nid = i;
			ASSERT_ALWAYS(kusp->nid_to_rsaid[i]);
			return( i );
		}
	}
	if (istart != 0) {
		/* search first portion of rsaid space */
		iend = istart;
		istart = 0;
		goto again;
	}
	return NULL_NID;
}
#endif

/* Directed Yield: 1 if yielded, 0 otherwise */
int
dyield(int new_nid)
{

	uthread_t *ut;
	kusharena_t *kusp;
	__int64_t oldrbits, mask;
	int	oldrsaid, oldnid, newrsa, preallocated;

	if (new_nid >= NT_MAXNIDS) {
		cmn_err_tag(1795, CE_WARN,"dyield: illegal request to yield to %d\n",
			new_nid);
		return 0;
	}
	ut = curuthread;

	if ((kusp = ut->ut_sharena) && (ut->ut_prda)) {
		mask = 1LL << (new_nid % bitsizeof(uint64_t));
		oldrbits = atomicClearUint64(
			&kusp->rbits[new_nid/bitsizeof(uint64_t)], mask);
		if (oldrbits & mask) {

			newrsa = kusp->nid_to_rsaid[new_nid];
			if (!newrsa) {
				/* Don't think this should occur */
				cmn_err_tag(1796, CE_WARN, "dyield: rbit set for NID %d but no RSA\n", new_nid);
				return(0);
			}

			oldnid = ut->ut_prda->t_sys.t_nid;
			preallocated = oldrsaid = kusp->nid_to_rsaid[oldnid];

			if (oldnid != NULL_NID) {
				/* handle allocate on demand case */
				if (oldrsaid == 0) {
					oldrsaid =
						find_free_rsaid(kusp,
							ut->ut_maxrsaid-1);
					kusp->nid_to_rsaid[oldnid] = oldrsaid;
				}
				/* need to range check value in table since
				 * user code has write-access to value
				 */
				if ((oldrsaid <= 0) || (oldrsaid >= ut->ut_maxrsaid)) {
					cmn_err_tag(1797,CE_WARN,"dyield NID %d bad RSA %d\n",
						oldnid, oldrsaid);
					return(1);
				}
				/* unload FP state and store into RSA */

				checkfp(ut,0);

				/* save user registers & FP state into RSA */

				save_rsa( ut, &kusp->rsa[oldrsaid].rsa);

				ut->ut_prda->sys_prda.prda_sys.t_nid = 0;

				atomicSetUint64(
					&kusp->rbits[oldnid/bitsizeof(uint64_t)],
					1LL << (oldnid % bitsizeof(uint64_t)));
			}

			/* load new RSA */

			if (preallocated) {

				if (ut->ut_rsa_locore) {

					restore_rsa_fp( ut,&kusp->rsa[newrsa].rsa);
					ut->ut_rsa_runable = 1;

				} else {

					restore_rsa( ut,&kusp->rsa[newrsa].rsa);
					ut->ut_rsa_runable = 2;
				}
			} else {
				restore_rsa( ut, &kusp->rsa[newrsa].rsa );
				kusp->nid_to_rsaid[new_nid] = 0;
				atomicSetUint64( kusp->fbits, 1LL<<newrsa);

				ut->ut_rsa_runable = 2;
			}

			ut->ut_prda->sys_prda.prda_sys.t_nid = new_nid;
			return(1);
		} 
	} else
		cmn_err_tag(1798, CE_WARN, "illegal dyield call\n");

	return 0;
}

void
rsa_upcall(uthread_t *ut, kusharena_t *kusp, k_machreg_t arg0,
	   k_machreg_t arg1, k_machreg_t arg2, k_machreg_t arg3)
{
	ut->ut_prda->t_sys.t_nid = NULL_NID;

	restore_rsa( ut, &kusp->rsa[NULL_NID].rsa );
	ASSERT(ut->ut_exception->u_eframe.ef_sp == (k_machreg_t)PRDA + _PAGESZ - 16);
	ut->ut_exception->u_eframe.ef_a0 = arg0;
	ut->ut_exception->u_eframe.ef_a1 = arg1;
	ut->ut_exception->u_eframe.ef_a2 = arg2;
	ut->ut_exception->u_eframe.ef_a3 = arg3;
}
#endif /* USE_PTHREAD_RSA */

/*
 * presume - have a new process - start it up
 */
static void
presume(register uthread_t *nth,	/* process thread to start up */
	kthread_t *okt)			/* pass this to resume() */
{
	/*REFERENCED*/
	kthread_t *nkt;

	ASSERT(nth);
	ASSERT(issplhi(getsr()));

	if (nth->ut_prda)
		nth->ut_prda->t_sys.t_cpu = cpuid();
	ASSERT(private.p_lticks <= nth->ut_tslice);

	/*
	 * Assign a new PID to this process if necessary.
	 */
	if (tlbpid_is_usable(&nth->ut_as)) {
		tlbpid_use(&nth->ut_as, tlbpid(&nth->ut_as));
		/*
		 * A mod to the scheduler causes ut_lastrun to get
		 * updated before we get here....
		 */
	} else {
		new_tlbpid(&nth->ut_as, VM_TLBVALID | VM_TLBNOSET);
	}

#if TFP
	/*
	 * Assign a new ICACHE PID to this process if necessary.
	 */
	if (icachepid_is_usable(&nth->ut_as))
		icachepid_use(&nth->ut_as, icachepid(&nth->ut_as));
	else
		new_icachepid(&nth->ut_as, VM_TLBVALID | VM_TLBNOSET);
#endif

#ifdef SPLMETER
	_cancelsplhi();
#endif

	nkt = UT_TO_KT(nth);

	/* actually start this thread */
#if TFP
	resume(nth, okt, tlbpid(&nth->ut_as), icachepid(&nth->ut_as));
#endif
#if R4000 || R10000
	resume(nth, okt, tlbpid(&nth->ut_as));
#endif
	/* NOTREACHED */
}

/*
 * switch out current sthread or xthread.
 *
 * Note that we want to use RTMON_TASK instead of RTMON_TASK|RTMON_TASKPROC
 * as we do in swtch() because we know that we're not contex switching a
 * uthread here ...
 */
void
istswtch(int flags)
{
	kthread_t *kt = curthreadp;
	kthread_t *nkt;

	ASSERT(issplhi(getsr()));
	ASSERT(getsr() & SR_IE);

	ASSERT(private.p_kstackflag == PDA_CURKERSTK);
	ASSERT(kt->k_sqself);
	ASSERT(kt->k_sonproc == cpuid());
	ASSERT(curvprocp == NULL);

	ASSERT(KT_ISXTHREAD(kt) || KT_ISSTHREAD(kt));

	ASSERT(private.p_switching == 0);
#if DEBUG
	private.p_switching = 1;
#endif
	/*
	 * We are no longer running on this processor for all intents and
	 * purposes, k_sqself protects our kernel stack.
	 */
	kt->k_sonproc = CPU_NONE;

	if (IS_TSTAMP_EVENT_ACTIVE(RTMON_TASK))
		fawlty_resched(kt, flags);

	nkt = getrunq();

	/*
	 * If the same process is to run again, don't want to go thru save
	 * and resume.  Just return.
	 */
	if (nkt == kt) {
		/*
		 * If rerunning the same sthread, we don't need to kickidle.
		 */
		ASSERT(nkt->k_sqself);
#if DEBUG
		private.p_switching = 0;
#endif
		LOG_TSTAMP_EVENT(RTMON_TASK,
			TSTAMP_EV_EOSWITCH, nkt->k_id,
			RTMON_PACKPRI(nkt, private.p_idletkn),
			NULL, NULL);
		return;
	}

	SYSINFO.kswitch++;

	/*
	 * All threads go through this save which stashes their
	 * context and prepares for them to be restarted or trashed.
	 */

	/* 0 on save, 1 on resume */
	if (save(kt->k_regs)) {
		ASSERT(private.p_kstackflag == PDA_CURKERSTK);
		ASSERT(curthreadp == kt);
		ASSERT(curvprocp == NULL);
		ASSERT(kt->k_wchan == 0);
		ASSERT(kt->k_timers.kp_curtimer == AS_SYS_RUN);
		ASSERT(private.p_switching);
		ASSERT(issplhi(getsr()));
		ASSERT(getsr() & SR_IE);
#if DEBUG
		private.p_switching = 0;
#endif
		LOG_TSTAMP_EVENT(RTMON_TASK,
			TSTAMP_EV_EOSWITCH, kt->k_id,
			RTMON_PACKPRI(kt, private.p_idletkn),
			NULL, NULL);

		ASSERT((caddr_t)getsp() > curthreadp->k_stack &&
			(caddr_t)getsp() <
			    (curthreadp->k_stack + curthreadp->k_stacksize));
		return;
	}

	ASSERT(private.p_utlbmissswtch == 0);

	resumenewthread(nkt, kt);
	/* NOTREACHED */
}

/*
 * switch out current xthread, to be resumed at the beginning again
 *
 * Note that we want to use RTMON_TASK instead of RTMON_TASK|RTMON_TASKPROC
 * as we do in swtch() because we know that we're not contex switching a
 * uthread here ...
 */
void
xthread_epilogue(kthread_t *kt)
{
	kthread_t *nkt;

	ASSERT(KT_ISXTHREAD(kt));

	ASSERT(kt_islocked(kt));

	ASSERT(private.p_kstackflag == PDA_CURKERSTK);
	ASSERT(kt->k_sqself);
	ASSERT(kt->k_sonproc == cpuid());
	ASSERT(curvprocp == NULL);
        KBLA_EMPTY(kt);
	KBLA_SAVE(kt);

	SYSINFO.kswitch++;

	if (IS_TSTAMP_EVENT_ACTIVE(RTMON_TASK))
		fawlty_resched(kt, SEMAWAIT);
#if DEBUG
	private.p_switching = 1;
#endif
	private.p_nextthread = 0;
	/*
	 * We are no longer running on this processor for all intents and
	 * purposes, k_sqself protects our kernel stack.
	 */
	kt->k_sonproc = CPU_NONE;
	kt->k_flags |= KT_ISEMA;
	KT_TO_XT(kt)->xt_arg2 = 0;
	kt_nested_unlock(kt);

	nkt = getrunq();

	ASSERT(private.p_utlbmissswtch == 0);

	resumenewthread(nkt, kt);
	/* NOTREACHED */
}

/*
 * Start of an xthread ...
 *
 * Note that we want to use RTMON_TASK instead of RTMON_TASK|RTMON_TASKPROC
 * as we do in swtch() because we know that we're not contex switching a
 * uthread here ...
 */
void
xthread_prologue(kthread_t *kt)
{
	register xthread_t *xt;
	register xt_func_t *func;
	register void *arg;
	register void *arg2;

	ASSERT(issplhi(getsr()));
	ASSERT(private.p_kstackflag == PDA_CURKERSTK);
	ASSERT(KT_ISXTHREAD(kt));
	xt = KT_TO_XT(kt);

	kt_nested_lock(kt);
	func = xt->xt_func;
	arg = xt->xt_arg;
	arg2 = xt->xt_arg2;
	kt_nested_unlock(kt);

	ASSERT(curvprocp == NULL);
	ASSERT(kt->k_wchan == 0);
#if DEBUG
	private.p_switching = 0;
#endif
	LOG_TSTAMP_EVENT(RTMON_TASK,
		TSTAMP_EV_EOSWITCH, kt->k_id,
		RTMON_PACKPRI(kt, private.p_idletkn),
		NULL, NULL);

	KBLA_RESUME(kt);
	KBLA_EMPTY(kt);
	
	spl0();
	func(arg, arg2);

	panic("xthread exited\n");
}

void
xthread_loop_prologue(kthread_t *kt)
{
	register xthread_t *xt;
	register xt_func_t *func;
	register void *arg;
	register void *arg2;
	int s;

	ASSERT(KT_ISXTHREAD(kt));
        KBLA_EMPTY(kt);
	
	xt = KT_TO_XT(kt);

	s = kt_lock(kt);
	func = xt->xt_func;
	arg = xt->xt_arg;
	arg2 = xt->xt_arg2 = 0;
	kt_unlock(kt, s);

	func(arg, arg2);

	panic("xthread exited\n");
}

/*
 * xtresume - have a new xthread - start it up
 * this is called from swtch/stswtch & resumeidle
 */
void
xtresume(xthread_t *nxt, kthread_t *okt)
{
	kthread_t *nkt;

	ASSERT(nxt);
#ifdef SPLMETER
	_cancelsplhi();
#endif
	ASSERT(issplhi(getsr()));

	nkt = XT_TO_KT(nxt);
	if (nkt->k_flags & KT_ISEMA) {
		ASSERT(KT_ISXTHREAD(nkt));
		kt_fclr(nkt, KT_ISEMA);
		restartxthread(nkt, nxt->xt_sp, okt);
	} else {
		resumethread(nkt, okt);
	}
	/* NOTREACHED */
}

/*
 * istexitswtch - i/sthread has exited.
 */
void
istexitswtch(void)
{
	kthread_t *kt = curthreadp;

	(void)splhi();

	SYSINFO.kswitch++;

	ASSERT(kt->k_sqself);
	ASSERT(kt->k_sonproc == cpuid());
	ASSERT(curvprocp == NULL);

	LOG_TSTAMP_EVENT(RTMON_PIDAWARE,
		TSTAMP_EV_EXIT, kt->k_id,
		current_pid(),
		NULL, NULL);
#if DEBUG
	private.p_switching = 1;
#endif
	private.p_nextthread = 0;
	kt->k_sonproc = CPU_NONE;

	/*
	 * Transfer to idle stack.
	 * This NULLs p_curkthread
	 */
	ASSERT(issplhi(getsr()));
	toidlestk(kt, istexitswtchfinish, kt, 0);
	/* NOTREACHED */
}

/* ARGSUSED */
static void
istexitswtchfinish(void *a, void *dummy)
{
	kthread_t *kt = (kthread_t *)a;
	kthread_t *nkt;

	ASSERT(issplhi(getsr()));
	/*
	 * WARNING - now on idle stack
	 */

	nested_spinlock(&reapqlock);
	ASSERT(kt->k_flink == kt);
	if (!(nkt = reapq)) {
		reapq = kt;
		sv_signal(&reap_wait);
	} else {
		kt->k_flink = nkt;
		kt->k_blink = nkt->k_blink;
		nkt->k_blink->k_flink = kt;
		nkt->k_blink = kt;
		ASSERT(sv_signal(&reap_wait) == 0);
	}
	nested_spinunlock(&reapqlock);
 
	nkt = getrunq();
	if (!nkt)
		nkt = idle(); /* already on idle stack */
	resumenewthread(nkt, NULL);
	/* NOTREACHED */
}

/*
 * stresume - have a new sthread - start it up
 */
void
stresume(sthread_t *nst, kthread_t *okt)
{
	kthread_t *nkt;

	ASSERT(nst);
#ifdef SPLMETER
	_cancelsplhi();
#endif
	ASSERT(issplhi(getsr()));

	nkt = ST_TO_KT(nst);
	resumethread(nkt, okt);
	/* NOTREACHED */
}

#if EXTKSTKSIZE == 1

pfd_t *Stackext_freepool;
int Stackext_freecnt;
int Stackext_resv;
lock_t Stackext_lock;

/*
 * setup_stackext() checks if the incoming uthread has its own kernel stack
 * extension page and if so sets that up as the current stack extension.
 * Otherwise, if the last uthread took the current stack ext. page, then
 * allocates another one.
 * If a different stack ext. page is setup then p_mapstackext flag is set
 * so that resume can map this along with the upage and kernel stack page.
 */
int
setup_stackext(register uthread_t *ut)
{
	uint stackext_pfn;

	if (ut->ut_kstkpgs[KSTEIDX].pgi) {
		kthread_t *kt = UT_TO_KT(ut);

		if (private.p_stackext.pgi) {
			if (private.p_bp_stackext.pgi) {
				stackext_free(private.p_stackext.pte.pg_pfn);
			} else {
				private.p_bp_stackext.pgi =
						private.p_stackext.pgi;
			}
		}
		private.p_stackext.pgi = ut->ut_kstkpgs[KSTEIDX].pgi;
		if (!kthread_owns_stackext(kt)) 
			ut->ut_kstkpgs[KSTEIDX].pgi = 0;

	} else {
		if (!private.p_stackext.pgi) {
			if (private.p_bp_stackext.pgi) {
				private.p_stackext.pgi =
						private.p_bp_stackext.pgi;
				private.p_bp_stackext.pgi = 0;
			} else {
				stackext_pfn = 
					stackext_alloc(STACKEXT_CRITICAL);
				if (stackext_pfn) {
					private.p_stackext.pgi = mkpde(PG_VR|PG_G|PG_SV|PG_M|pte_cachebits(), stackext_pfn);
				} else {
					stackext_setsxbrk();
					return(0);	
				}
			}
		} else {
			return(1);
		}
	}

	/*
	 * Set up a flag to map new stackext page later in resume() since
	 * we might by using the old one currently.
	 */

	private.p_mapstackext = 1;

	return(1);
}

#if KSTACKPOOL
extern int sync_dcaches_excl(void *, unsigned int, unsigned int, unsigned int);
#endif

/*
 * stackext_alloc() allocates a kernel stack extension page. If pagealloc()
 * has run out of pages, then there is a backup pool of pages  which can be
 * till the system frees up some pages.
 */

uint
stackext_alloc(int flags)
{
	pfd_t *pfd;
	int	s;

	if (pfd = pagealloc(0, VM_UNSEQ|VM_NO_DMA)) {
#if KSTACKPOOL
		/*
		 * Since pagealloc no longer does flushcaches,
		 * have to ensure that there will be no VCEs
		 * taken on this stack page.
		 * NOTE: Rather than invoke _bclean_caches, we call a lower
		 * level function which simply brings the page into our
		 * cache with the EXCLUSIVE attribute using the correct
		 * virtual alias ... this forces any potential VCEs to
		 * occur now and avoids problems in the future.
		 */
		sync_dcaches_excl(
			(void *)(KSTACKPAGE-NBPC),
			NBPP,
			pfdattopfn(pfd),
			CACH_NOADDR|CACH_AVOID_VCES);			       
#endif
		pfd_setflags(pfd, P_DUMP);
		return(pfdattopfn(pfd));
	}
	if (flags != STACKEXT_CRITICAL)
		return(0);

	s = mutex_spinlock(&Stackext_lock);
	pfd = Stackext_freepool;
	if (pfd) {
		Stackext_freepool = pfd->pf_next;
		Stackext_freecnt--;
		mutex_spinunlock(&Stackext_lock, s);
		return(pfdattopfn(pfd));
	}
	mutex_spinunlock(&Stackext_lock, s);
	return(0);
}

/*
 * stackext_free() frees up stack extension page.
 */

void
stackext_free(pfn)
uint	pfn;
{
	pfd_t *pfd;
	int	s;

	pfd = pfntopfdat(pfn);
	s = mutex_spinlock(&Stackext_lock);
	if (Stackext_freecnt < Stackext_resv) {
		pfd->pf_next = Stackext_freepool;
		Stackext_freepool = pfd;
		Stackext_freecnt++;
		mutex_spinunlock(&Stackext_lock, s);
		return;
	} else {
		mutex_spinunlock(&Stackext_lock, s);
		pfd_clrflags(pfd, P_DUMP); 
		pagefree(pfd);
		return;
	}
}

/*
 * stackext_init() initializes the freepool which is a backup supply of
 * pages in case of pagealloc() running out of memory.
 */
void
stackext_init(void)
{
	pfd_t *pfd = NULL;
	pfd_t *npfd;
	int i;

	/*
	 * 576040 - increase the number of extension pages
	 * to at least 10 to help prevent possible deadlock
	 * (described in bug report).
	 */
	Stackext_resv = MAX(numcpus, 10);

	for (i=0; i<Stackext_resv; i++) {
		npfd = pagealloc(0, VM_UNSEQ|VM_NO_DMA);
		if (npfd == NULL)
			break;
#if KSTACKPOOL
		/*
		 * Since pagealloc no longer does flushcaches,
		 * have to ensure that there will be no VCEs
		 * taken on these stack pages.
		 * NOTE: Rather than invoke _bclean_caches, we call a lower
		 * level function which simply brings the page into our
		 * cache with the EXCLUSIVE attribute using the correct
		 * virtual alias ... this forces any potential VCEs to
		 * occur now and avoids problems in the future.
		 */
		sync_dcaches_excl(
			(void *)(KSTACKPAGE-NBPC),
			NBPP,
			pfdattopfn(npfd),
			CACH_NOADDR|CACH_AVOID_VCES);			       
#endif
		pfd_setflags(npfd, P_DUMP);
		npfd->pf_next = pfd;
		pfd = npfd;
	}

	Stackext_freecnt = i;
	Stackext_freepool = pfd;
	spinlock_init(&Stackext_lock, "stackext");
}
#endif /* EXTKSTKSIZE == 1 */
