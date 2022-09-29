/**************************************************************************
 *									  *
 * 		 Copyright (C) 1987-1993 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 3.280 $"

/*#include "limits.h" */
#include "sys/types.h"
#include "sys/atomic_ops.h"
#include "sys/cmn_err.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "sys/idbgentry.h"
#include "sys/kmem.h"
#include "sys/ksignal.h"
#include "sys/kthread.h"
#include "sys/param.h"
#include "sys/par.h"
#include "sys/pda.h"
#include "sys/runq.h"
#include "sys/sema.h"
#include "sys/systm.h"
#include "sys/timers.h"
#include "ksys/xthread.h"
#include "procfs/prsystm.h"
#include "sys/rt.h"
#include "sys/ddi.h"
#include "sys/uthread.h"
#include "ksys/behavior.h"
#include "sys/space.h"
#include "sys/sema_private.h"
#include "sys/proc.h"
#include "sys/sched.h"
#include "sys/getpages.h"
#include <sys/klstat.h>

#ifdef SEMAMETER
void _mrupdate(mrlock_t *, inst_t *);
void _mraccess(mrlock_t *, inst_t *, int);
void idbg_mrlocks(__psint_t);
#endif
extern int mrcheckupd(mrlock_t *);

static void mrdq(mrlock_t *, kthread_t *);

static kthread_t *semadq(sema_t *);
static int semawake(sema_t *, int);
static void semaaddproc(sema_t *, kthread_t *);
static int semarmproc(sema_t *, kthread_t *);
/* int mon_runq(mon_t *, int); */
/* static void mon_runq_timeout(mon_t *); */

extern semahist_t	*semahist;	/* array of history records */
extern uint		histmax;	/* maximum entries before wrap */
uint			histpos;	/* next entry to write */
lock_t			histlock;	/* lock on update of counter */
unsigned short		semadefhist = 0;	/* default history */
uint			semalhistmax = -1; /* original histmax value */
semahist_t		*semalhist = 0;

struct syncinfo		syncinfo;
int			semadefkhist = 0;
int			semadefkhistlen = 0;

extern int		mrlock_starve_threshold;

#ifdef DEBUG
extern int kdbx_on;
#endif

#ifdef SEMAMETER
/*
 * [Potentially] inline versions of smeter() and mrmeter() for internal use
 * to keep code readable but fast under varying #ifdef's.
 */
static __inline k_semameter_t *
_smeter(sema_t *sp)
{
#ifdef SEMAINFOP
	return (k_semameter_t *)sp->s_info;
#else
	return smeter(sp);
#endif
}

static __inline k_mrmeter_t *
_mrmeter(mrlock_t *mrp)
{
#ifdef SEMAINFOP
	return (k_mrmeter_t *)mrp->mr_info;
#else
	return mrmeter(mrp);
#endif
}
#endif /* SEMAMETER */
 
#ifndef SEMAINFOP
/*
 * Return the name of a wchan.  If our caller knows what kind of lock it is
 * we can use that to limit our search to the appropriate list; otherwise we
 * have to search through all of the lock lists till we find a match.  Since
 * this later case is mostly usd by idbg, the performance hit isn't generally
 * a big issue.
 */
/*ARGSUSED*/
char *
wchanname(void *wchan, int flags)
{
#ifdef SEMAINFO
	wchaninfo_t *winfo;
	int syncflags;

	/*
	 * It would be nice to do a better job for these non-sync variable
	 * cases, but this is good enough for now.
	 */
	if (flags & KT_INDIRECTQ)
		return "indirectq";

	syncflags = flags & (KT_WSEMA|KT_WMUTEX|KT_WSV|KT_WMRLOCK);
	if (syncflags) {
		mpkqueuehead_t *infolist;
		switch (syncflags) {
		    case KT_WMUTEX:  infolist = mutexinfolist; break;
		    case KT_WSEMA:   infolist = semainfolist;  break;
		    case KT_WMRLOCK: infolist = mrinfolist;    break;
		    case KT_WSV:     infolist = svinfolist;    break;

		    default:
			cmn_err(CE_PANIC, "wchanname: illegal sync flag"
				" combination 0x%x for wchan 0x%x\n",
				flags, wchan);
			/*NOTREACHED*/
		}
		if (winfo = wchaninfo(wchan, infolist))
			return (char *)winfo;
	} else {
		if (winfo = wchaninfo(wchan, mutexinfolist))
			return (char *)winfo;
		if (winfo = wchaninfo(wchan, semainfolist))
			return (char *)winfo;
		if (winfo = wchaninfo(wchan, mrinfolist))
			return (char *)winfo;
		if (winfo = wchaninfo(wchan, svinfolist))
			return (char *)winfo;
	}
#endif
	return "";
}
#endif /* !SEMAINFOP */

int semabuckets = SEMABUCKETS;
#ifdef SEMAMETER
int semametered = 1;
#else
int semametered = 0;
#endif

/*
 * Return a pointer to the base of the info structure for a wchan.
 * Caller must ensure that wchan/info doesn't go away.
 */
/*ARGSUSED*/
wchaninfo_t *
wchaninfo(void *wchan, mpkqueuehead_t *winfolist)
{
#if !defined(SEMAINFO)
	return NULL;
#elif defined(SEMAINFOP)
	return (wchaninfo_t *)*(void **)wchan;
#else
	mpkqueuehead_t *bp;
	int s;
	kqueue_t *lp;

	/*
	 * Return failure if hash table lists aren't initialized
	 * yet.  This only happens early on in system initialization.
	 */
	bp = &winfolist[SEMABUCKET(wchan)];
	if (kqueue_isnull(&bp->head))
		return NULL;
	s = mpkqueue_lock(bp);

	for (lp = kqueue_first(&bp->head);
	     lp != kqueue_end(&bp->head);
	     lp = kqueue_next(lp)) {
		wchaninfo_t *winfo = baseof(wchaninfo_t, list, lp);
		if (winfo->wchan == wchan) {
			mpkqueue_unlock(bp, s);
			return winfo;
		}
	}

	mpkqueue_unlock(bp, s);
	return NULL;
#endif
}

/*
 * Return a pointer to the base of the meter structure for a lock.
 * Caller must ensure that lock/meter doesn't go away.
 */
#ifdef SEMAMETER
#pragma weak wchanmeter = wchaninfo
#else
/*ARGSUSED*/
wchaninfo_t *
wchanmeter(void *wchan, mpkqueuehead_t *winfolist)
{
	return NULL;
}
#endif

/*
 * Lists of metering structures for semaphores and multi-reader locks.
 * These are always present so idbg can scan them.
 */
mpkqueuehead_t semainfolist[SEMABUCKETS];
mpkqueuehead_t mrinfolist[SEMABUCKETS];

#ifdef SEMAINFO
static zone_t *semainfo_zone;
static zone_t *mrinfo_zone;
#endif


#ifdef SEMAMETER
static int rslog(uint, uint, sema_t *, kthread_t *, k_semameter_t *, inst_t *);

#define	slog(t,o,s,w,m,c)	histmax && rslog(t,o,s,w,m,c)

static mr_entry_t *mrmeter_entry(k_mrmeter_t *km, short type, inst_t *pc,
				 int vmflags);
static mr_entry_t *mrmeter_getentry(k_mrmeter_t *km, kthread_t *kt,
				    short from, short to);

#define s_sema(km)	((sema_t *)((km)->s_base.wchan))
#define mr_lock(km)	((mrlock_t *)((km)->mr_base.wchan))

#define	upwait(km)	{if (++(km)->s_nwait > (km)->s_maxnwait) \
				 (km)->s_maxnwait = (km)->s_nwait;\
			 ASSERT(!(s_sema(km)->s_un.s_st.count <= 0) || \
				-(s_sema(km)->s_un.s_st.count) == (km)->s_nwait); \
			}

#define	downwait(km)	{ASSERT((km)->s_nwait > 0); \
			 (km)->s_nwait--; \
			 ASSERT(!(s_sema(km)->s_un.s_st.count <= 0) || \
				-(s_sema(km)->s_un.s_st.count) == (km)->s_nwait); \
			}

#ifdef SEMA_KHIST

#define STARTHOLDLOG(km, callpc) { \
		k_histo_t *kp = HMETER(km); \
		if (kp) { \
			kp->h_ts = get_timestamp(); \
			kp->h_pc = callpc; \
		} \
		}

#define DOHOLDLOG(km) { \
		k_histo_t *kp = HMETER(km); \
		if (kp && kp->h_ts) { \
			int elapsed = get_timestamp() - kp->h_ts; \
			if (elapsed < 0) \
				elapsed += TIMER_MAX; \
			kp->h_ts = 0; \
			HOLDLOG(kp, elapsed, kp->h_pc); \
			kp->h_pc = 0; \
		} \
		}
#endif

#endif	/* SEMAMETER */

#ifdef DEBUG
uint_t semaq_waited;
#endif

static zone_t	*sema_zone;
#ifdef SEMAMETER
static zone_t	*mr_log_zone;
#endif

struct zone	*mria_zone_public;
struct zone	*mria_zone_private;

void
sema_init(void)
{
	int i, nmria;
	mria_t *mriap;

	sema_zone = kmem_zone_init(sizeof(sema_t), "semas");
	mria_zone_private = kmem_zone_private(sizeof(mria_t),
				"Private mrlock inheritance structures");
	mria_zone_public = kmem_zone_init(sizeof(mria_t),
				"Public mrlock inheritance structures");

	/*
	 * Compute number of reserved mri_t's for use by vhand since
	 * this is the only thread that holds more than MRI_MAXLINKS (8)
	 * simultaneous mrlocks. Vhand holds one lock per region being
	 * swapped. Assuming worst case of 1 page per swappable region,
	 * compute the #of locks from maxpglst in each of NPGLSTS classes
	 * of pages. Also,
	 * 1. Give reserve atleast 64 * MRI_MAXLINKS(8) mri_t's as
	 *    this has worked during irix6.5 beta testing, and
	 * 2. Respect mrlock_num_mria tunable if set higher than
	 *    computed number, giving ultimate cotrol to end-user
	 *    configurations.
	 */
	nmria = (maxpglst * NPGLSTS /  MRI_MAXLINKS) + 1; /* +1 for ceil */
	nmria = nmria < 64 ? 64 : nmria;
	nmria = nmria < mrlock_num_mria ? mrlock_num_mria : nmria;

#ifdef DEBUG
	cmn_err(CE_NOTE, "Number of mria = %d\n", nmria);
#endif
	for (i = 0; i < nmria; i++) {
		mriap = kmem_zone_alloc(mria_zone_public, KM_NOSLEEP);
		if (mriap == 0)
			cmn_err(CE_PANIC,"Could not allocate initial mria");
		kmem_zone_free(mria_zone_private, mriap);
	}

#ifdef SEMAMETER
	mr_log_zone = kmem_zone_init(sizeof(mr_log_t), "mrlog");
	idbg_addfunc("mrlocks", idbg_mrlocks);
#endif
#ifndef SEMAINFO
	/*
	 * allocwchaninfo() will never be called so the 1 bucket hash lists
	 * will never get initialized.  Since idbg is the only entity outside
	 * the ksync code that accesses these lists, this simply keeps things
	 * prettier when various idbg commands are executed.
	 */
	mpkqueue_init(semainfolist);
	mpkqueue_init(mrinfolist);
#endif
}

sema_t *
sema_alloc(int km_flags, int count, char *name)
{
	register sema_t *sp = (sema_t *)kmem_zone_alloc(sema_zone,
							km_flags & KM_NOSLEEP);
	if (sp)
		initnsema(sp, count, name);

	return sp;
}

void
sema_dealloc(register sema_t *sp)
{
	freesema(sp);
	kmem_zone_free(sema_zone, sp);
}

/*
 * Initialize the given semaphore to the specified value.
 */
/*ARGSUSED3*/
void
init_sema(register sema_t *sp, int val, char *name, long sequence)
{
	_SEMAMETER(k_semameter_t *km;)

	sp->s_un.s_st.count = val;
	sp->s_queue = NULL;
	ASSERT(sp->s_un.s_st.count >= 0);

#ifdef SEMAMETER
	sp->s_un.s_st.flags = semadefhist & SEMA_NOHIST;

	/*
	 * Init history/metering if defaultsemameter is set AND
	 * dynamic allocator initialized.
	 */
	_SEMAINFOP(sp->s_info = NULL;)
	if (defaultsemameter)
		semameteron(sp, name, sequence);

	/*
	 * Semaphore logging uses spinlocks.  In all cases other than
	 * SEMAOP_INIT and SEMAOP_FREE, an splhi() has already been
	 * done before calling the logging code.  Rather than put a call
	 * to splhi inside the logging code, where it would be superfluous
	 * 99.9% of the time, put one here and in 'freesema'.
	 */
	km = _smeter(sp);
	if (km) {
		int ospl = splhi();
		slog(SEMAOP_INIT, 0, sp, 0, km, 0);
		splx(ospl);
	}
#else
	sp->s_un.s_st.flags = 0;
#ifdef SEMANAME
	_SEMAINFOP(sp->s_info = NULL;)
	allocwchaninfo(sp, name, sequence,
		       semainfolist, sizeof (wchaninfo_t),
		       &semainfo_zone, "semainfo");
#else
	_SEMAINFOP(sp->s_info = name;)
#endif
#endif

#if defined(DEBUG) || defined(SYNCINFO)
	atomicAddInt(&syncinfo.semaphore.current, 1);
	atomicAddInt(&syncinfo.semaphore.initialized, 1);
#endif
}


/*
 * Free resources associated with a semaphore.
 *
 *	Inverse of initsema.  Many semaphores are permanently
 *	allocated, but the following transient objects have
 *	have semaphores:  pipes (go away at close), mount table
 *	entries (go away at umount), ts driver opens (go away
 *	at close).  There are also several SysV file system
 *	functions (sync, statfs) that create semaphores that
 *	persist for the duration of a system call.
 *
 *	The semaphore data structure itself is defined by the
 *	caller and doesn't need to be freed.  Calling initsema
 *	allocates a spinlock, however, which is freed by calling
 *	freesema.
 */
/*ARGSUSED*/
void
freesema(register sema_t *sp)
{
	/* ASSERT(!wait_getq(sp));	nobody should be hanging around */

#if defined(SEMAMETER)
	semameteroff(sp);
#elif defined(SEMANAME)
	freewchaninfo(sp, semainfolist, semainfo_zone);
#endif

#if defined(DEBUG) || defined(SYNCINFO)
	atomicAddInt(&syncinfo.semaphore.current, -1);
#endif
}

void
semahiston(register sema_t *sp)
{
	sema_flagclr(sp, SEMA_NOHIST);
}

void
semahistoff(register sema_t *sp)
{
	sema_flagset(sp, SEMA_NOHIST);
}

#ifdef SEMAMETER
/*
 * semameteron - turn on history/metering for a semaphore
 * Returns 0 if history on
 *	  -1 if memory allocator not initialized OR not enough memory
 * This is safe to call WHILE semaphore is being used
 */
int
semameteron(sema_t *sp, char *name, long sequence)
{
	k_semameter_t *kp = (k_semameter_t *)
		allocwchaninfo(sp, name, sequence,
			       semainfolist, sizeof (k_semameter_t),
			       &semainfo_zone, "semameters");
	if (!kp)
		return -1;
#ifdef SEMA_KHIST
	if (semadefkhist) {
		k_histo_t *khp = (k_histo_t *)kern_calloc(1, sizeof(*khp));
		if (!khp) {
			kmem_zone_free(semainfo_zone, kp);
			return -1;
		}
		if (khp && semadefkhistlen) {
			ALLOC_HISTO_DATA(&khp->h_hold, semadefkhistlen);
			ALLOC_HISTO_DATA(&khp->h_waiting, semadefkhistlen);
			ALLOC_HISTO_DATA(&khp->h_count, semadefkhistlen);
			if (   !khp->h_hold.he_data
			    || !khp->h_waiting.he_data
			    || !khp->h_count.he_data) {
				FREE_HISTO_DATA(&khp->h_count);
				FREE_HISTO_DATA(&khp->h_waiting);
				FREE_HISTO_DATA(&khp->h_hold);
				kern_free(khp);
				kmem_zone_free(semainfo_zone, kp);
				return -1;
			}
		}
		kp->s_histo = khp;
	}
#endif
	return 0;
}

/*
 * semameteroff - turn off history/metering for a semaphore
 */
void
semameteroff(register sema_t *sp)
{
	k_semameter_t *kp = _smeter(sp);
	if (!kp)
		return;

	_SEMAINFOP(sp->s_info = NULL;)
	mpkqueue_remove(&semainfolist[SEMABUCKET(sp)], &kp->s_base.list);

#ifdef SEMA_KHIST
	{
		k_histo_t *khp = HMETER(kp);
		if (khp) {	
			FREE_HISTO_DATA(&khp->h_count);
			FREE_HISTO_DATA(&khp->h_waiting);
			FREE_HISTO_DATA(&khp->h_hold);
			kern_free(khp);
		}
	}
#endif

	kmem_zone_free(semainfo_zone, kp);
}

#else

/*ARGSUSED*/
int
semameteron(register sema_t *sp, char *name, long sequence)
{
	return 1;
}

/*ARGSUSED*/
void
semameteroff(register sema_t *sp)
{
}
#endif

/*
 * semawait(sema_t *sp, int pri, int ospl, inst_t *callpc)
 *
 * Called when thread has to sleep on queue.
 * Semaphore lock must be held.
 * The lock is released on return
 * The spl level is restored to 'ospl' on return
 *
 * Returns:
 *	 -1 - semaphore NOT acquired AND signal occurred
 *	  0 - semaphore acquired or received setrun()
 * Signal handling is subtle:
 * 	we need to make the assumption that if we are
 *	yanked off the semaphore queue WITHOUT acquiring the semaphore
 *	(k_prtn == 0) then we must in fact have a signal that we are either
 *	catching or defaulting. Ignored signals MUST not ever be received
 *	this solves the following problem:
 *	1) one no longer must loop psema'ing if a breakable priority
 *		waiting for the condition to be satisfied (ala the old
 *		sleep)
 *	In addition this scheme (of not passing on ignored signals)
 *	is more efficient since 2 ctxt switches per ignored signal are
 *	not necessary.
 */
/*ARGSUSED*/
int
semawait(
	register sema_t	*sp,		/* sema we're waiting for */
	register int	pri,		/* pri thread should sleep at */
	int 		ospl,
	inst_t		*callpc)
{
	kthread_t *kt = private.p_curkthread;
	int interruptible;
	int sleeptimer = TIMER_SHIFTBACK(pri);
	int trwake;

	pri &= TIMER_MASK;
	ASSERT(private.p_kstackflag == PDA_CURKERSTK);
	ASSERT(bitlock_islocked(&sp->s_un.s_lock, SEMA_LOCK));
	ASSERT(!(kt->k_flags & KT_SYNCFLAGS));

	/* lock kt before adding to semaphore to avoid ABBA */
	kt_nested_lock(kt);

	interruptible = (KT_ISUTHREAD(kt) && ((pri & PMASK) > PZERO));
	if (interruptible)
		bla_isleep(&ospl);

	if (!interruptible) {
		trwake = thread_blocking(kt,
			pri & PLTWAIT ? KT_LTWAIT|KT_NWAKE : KT_NWAKE);
	} else {
		if (thread_interrupted(kt)) {
			++sp->s_un.s_st.count;
			_sema_unlock(sp);
			kt_unlock(kt, ospl);
			bla_iunsleep();
			return -1;
		}
		trwake = thread_blocking(kt, KT_LTWAIT);
	}
	semaaddproc(sp, kt);
	_SEMAMETER(slog(SEMAOP_PSEMA, SSOP_PSLP, sp, 0, 0, callpc);)
	_sema_unlock(sp);

	thread_block(kt, SEMAWAIT, KT_WSEMA, trwake, ospl, sleeptimer);

	if (interruptible)
		bla_iunsleep();

	if (interruptible && !kt->k_prtn && thread_interrupted(kt))
		return -1;
	return(0);
}

/*
 * Called when a thread needs to be awoken
 * from sleeping on the semaphore --
 * this thread will get the semaphore.
 * Called with the semaphore lock held (at splhi),
 * and is released upon exit.
 * Returns:
 *	1 if someone woken (always!)
 */
static int
semawake(register sema_t *sp, register int s)
{
	kthread_t *kt;

	ASSERT(sp->s_queue);
	kt = semadq(sp);
	_sema_unlock(sp);

	kt_nested_lock(kt);
	thread_unblock(kt, KT_WSEMA);
	kt_unlock(kt, s);
	return(1);
}

static xthread_t *
isemawake(register sema_t *sp, register int s,
	int pri, xt_func_t *func, void *arg, void *arg2)
{
	kthread_t *kt;
	xthread_t *xt;

	ASSERT(sp->s_queue);
	kt = semadq(sp);
	_sema_unlock(sp);

	kt_nested_lock(kt);
	xt = KT_TO_XT(kt);
	if (func)
		xt->xt_func = func;
	if (arg)
		xt->xt_arg = arg;
	xt->xt_arg2 = arg2;
	kt->k_pri = kt->k_basepri = pri;
	thread_unblock(kt, KT_WSEMA);
	kt_unlock(kt, s);
	return(xt);
}

/*
 * Called by someone wishing to wakeup any process (if there is one)
 * sleeping on a synchronizing semaphore.
 * Returns:
 *	0 if no-one waiting for semaphore (semaphore unaltered)
 *	1 if someone woken up
 */
int
cvsema(register sema_t *sp)
{
	register int s;
	_SEMAMETER(k_semameter_t *km = _smeter(sp);)

	if (sp->s_un.s_st.count >= 0) {
#ifdef SEMAMETER
		if (km) {
			km->s_cvsema++;
			km->s_cvnoslp++;
			slog(SEMAOP_CVSEMA, SSOP_CVNOWAKE, sp, 0, km,
			     (inst_t *)__return_address);
			_SEMA_KHIST(DOHOLDLOG(km);)
		}
#endif
		return 0;
	}

	s = sema_lock(sp);
	if (sp->s_un.s_st.count < 0) {
		sp->s_un.s_st.count++;
#ifdef SEMAMETER
		if (km) {
			slog(SEMAOP_CVSEMA, SSOP_CVWAKE, sp, sp->s_queue,
			     km, (inst_t *)__return_address);
			km->s_cvsema++;
			km->s_thread = sp->s_queue;
			km->s_id = sp->s_queue ? sp->s_queue->k_id : 0;
			_SEMA_KHIST(DOHOLDLOG(km);)
		}
#endif
		return semawake(sp, s);
	}
	sema_unlock(sp, s);
	/* The sync object has been unlocked at this point, making it possible
	 * for another thread to free the sync object's memory.  Referencing
	 * the sync object after this point is not guaranteed safe.
	 */

#ifdef SEMAMETER
	if (km) {
		km->s_cvsema++;
		km->s_cvnoslp++;
		slog(SEMAOP_CVSEMA, SSOP_CVNOWAKE, sp, 0, km,
		     (inst_t *)__return_address);
		_SEMA_KHIST(DOHOLDLOG(km);)
	}
#endif
	return(0);
}

/*
 * Called by someone wishing to wakeup an interrupt thread (if there is one)
 * sleeping on a synchronizing semaphore by calling ipsema.
 * Returns:
 *	NULL if no-one waiting for semaphore (semaphore unaltered)
 *	Otherwise, a pointer to the thread woken up
 * If an interrupt thread is woken up, it will execute 
 *	func(arg, arg2) at priority pri
 * If either func or arg is null, then the current value (set by
 * xthread_create or xthread_set_func) is used.
 * When an interrupt thread is restarted for any reason *except*
 * by icvsema, it will be called with an arg2 == 0.
 */
xthread_t *
icvsema(register sema_t *sp,
	int pri, xt_func_t *func, void *arg, void *arg2)
{
	register int s;
	_SEMAMETER(k_semameter_t *km = _smeter(sp);)

	s = sema_lock(sp);
	if (sp->s_un.s_st.count < 0) {
		sp->s_un.s_st.count++;
#ifdef SEMAMETER
		if (km) {
			slog(SEMAOP_CVSEMA, SSOP_CVWAKE, sp, sp->s_queue,
			     km, (inst_t *)__return_address);
			km->s_cvsema++;
			km->s_thread = sp->s_queue;
			km->s_id = sp->s_queue ? sp->s_queue->k_id : 0;
			_SEMA_KHIST(DOHOLDLOG(km);)
		}
#endif
		return isemawake(sp, s, pri, func, arg, arg2);
	}
	sema_unlock(sp, s);
	/* The sync object has been unlocked at this point, making it possible
	 * for another thread to free the sync object's memory.  Referencing
	 * the sync object after this point is not guaranteed safe.
	 */

#ifdef SEMAMETER
	if (km) {
		km->s_cvsema++;
		km->s_cvnoslp++;
		slog(SEMAOP_CVSEMA, SSOP_CVNOWAKE, sp, 0, km,
		     (inst_t *)__return_address);
		_SEMA_KHIST(DOHOLDLOG(km);)
	}
#endif
	return(NULL);
}

/*
 * Called to wakeup a particular thread sleeping on a semaphore.
 * thread will wake with the semaphore
 * Note lock ordering:
 *	kt_lock
 *	sema_lock
 * Returns:
 *	0 - kt not queued on semaphore
 *	1 - kt de-queued and now owns semaphore
 */

int
wsema(register sema_t *sp, register kthread_t *kt)
{
	register int s;

	s = kt_lock(kt);
	_sema_lock(sp);
	return _wsema(sp, s, kt, (inst_t *)__return_address);
}

/*
 * _wsema - called with kt_lock & sema_lock
 * Returns w/ everything unlocked
 */
/*ARGSUSED*/
int
_wsema(register sema_t *sp, register int s, kthread_t *kt, inst_t *callpc)
{
	_SEMAMETER(k_semameter_t *km = _smeter(sp);)

	if (semarmproc(sp, kt) == 0) {
#ifdef SEMAMETER
		if (km) {
			km->s_wsema++;
			slog(SEMAOP_WSEMA, SSOP_WMISS, sp, 0, km, callpc);
			km->s_wnoslp++;
		}
#endif
		_sema_unlock(sp);
		kt_unlock(kt, s);
		return(0);
	}
#ifdef SEMAMETER
	if (km) {
		km->s_wsema++;
		slog(SEMAOP_WSEMA, SSOP_WWAKE, sp, kt, km, callpc);
		km->s_thread = kt;
		km->s_id = kt->k_id;
	}
#endif
	_sema_unlock(sp);

	ASSERT((kt->k_flags & KT_SYNCFLAGS) == KT_WSEMA);
	thread_unblock(kt, KT_WSEMA);
	kt_unlock(kt, s);
	return(1);
}

/*
 * Called to acquire a semaphore unconditionally.
 * Returns:
 *	0 - semaphore acquired
 *     -1 - signal occurred
 *
 * NOTE: differs from sleep semantically in that if a signal is being
 * ignored it will never be seen and thus psema will not return as the old
 * sleep did
 */
int
psema(register sema_t *sp, int pri)
{
	register int s;
	register short count;
	_SEMAMETER(k_semameter_t *km = _smeter(sp);)

	s = sema_lock(sp);

	ASSERT(private.p_curkthread);

	/*
	 * Loading count into a register saves an extra memory load,
	 * even in the non-SEMAMETER case!  Current compiler doesn't
	 * seem to be able to optimize pre-decrement and -increment.
	 */
	count = --sp->s_un.s_st.count;
	if (count < 0) {
#ifdef SEMAMETER
		if (km) {
			km->s_psema++;
#ifdef SEMA_KHIST
			{ k_histo_t *kp;
			if (kp = HMETER(km))
				COUNTLOG(kp, -count,
						(__psint_t)__return_address); }
#endif
		}
#endif

		LSTAT_SEMA_UPDATE(sp, LSTAT_ACT_SLEPT);
		return(semawait(sp, pri, s, (inst_t *)__return_address));
	}

#ifdef SEMAMETER
	if (km) {
		km->s_psema++;
		km->s_phits++;
		km->s_thread = private.p_curkthread;
		km->s_id = private.p_curkthread ? private.p_curkthread->k_id:0;
		km->s_lastpc = (inst_t *)__return_address;
		slog(SEMAOP_PSEMA, SSOP_PHIT, sp, 0, km,
		     (inst_t *)__return_address);
		_SEMA_KHIST(STARTHOLDLOG(km, (inst_t *)__return_address);)
	}
#endif

	sema_unlock(sp, s);

	LSTAT_SEMA_UPDATE(sp, LSTAT_ACT_NO_WAIT);
	return(0);
}

void
ipsema(register sema_t *sp)
{
	register int s;
	register short count;
	register kthread_t *kt;
	_SEMAMETER(k_semameter_t *km = _smeter(sp);)

	s = sema_lock(sp);

	kt = private.p_curkthread;
	ASSERT(KT_ISXTHREAD(kt));
	ASSERT(private.p_kstackflag == PDA_CURKERSTK);

	/*
	 * Loading count into a register saves an extra memory load,
	 * even in the non-SEMAMETER case!  Current compiler doesn't
	 * seem to be able to optimize pre-decrement and -increment.
	 */
	count = --sp->s_un.s_st.count;
	if (count < 0) {
#ifdef SEMAMETER
		if (km) {
			km->s_psema++;
#ifdef SEMA_KHIST
			{ k_histo_t *kp;
			if (kp = HMETER(km))
				COUNTLOG(kp, -count,
						(__psint_t)__return_address); }
#endif
		}
#endif
		kt_nested_lock(kt);

		semaaddproc(sp, kt);
		ASSERT(bitlock_islocked(&sp->s_un.s_lock, SEMA_LOCK));
		_SEMAMETER(slog(SEMAOP_PSEMA, SSOP_PSLP, sp, 0, km,
				(inst_t *)__return_address);)
		_sema_unlock(sp);

		kt->k_flags |= KT_WSEMA;

		ASSERT(!kt->k_monitor);
		s =  splprof();
		ktimer_switch(kt, AS_SLEEP_WAIT);
		splx(s);
		LSTAT_SEMA_UPDATE(sp, LSTAT_ACT_SLEPT);
		xthread_epilogue(kt);
		/* NOTREACHED */
	}

#ifdef SEMAMETER
	if (km) {
		km->s_psema++;
		km->s_phits++;
		km->s_thread = private.p_curkthread;
		km->s_id = private.p_curkthread ? private.p_curkthread->k_id:0;
		km->s_lastpc = (inst_t *)__return_address;
		slog(SEMAOP_PSEMA, SSOP_PHIT, sp, 0, km,
		     (inst_t *)__return_address);
		_SEMA_KHIST(STARTHOLDLOG(km, (inst_t *)__return_address);)
	}
#endif

	sema_unlock(sp, s);

	LSTAT_SEMA_UPDATE(sp, LSTAT_ACT_NO_WAIT);
	loopxthread(kt, KT_TO_XT(kt)->xt_sp);
	/* NOTREACHED */
}

#ifdef DEBUG
/*
 * Return boolean value "the executing thread owns the given sema"
 * This is only known when metering is on.
 */
/*ARGSUSED*/
int
ownsema(register sema_t *sp)
{
#ifdef SEMAMETER
	k_semameter_t *km = _smeter(sp);
	if (km)
		return(km->s_thread == private.p_curkthread);
#endif
	return(sp->s_un.s_st.count <= 0);
}
#endif
 
/*
 * Called to release a semaphore unconditionally
 * Returns:
 *	0 - if noone waiting on semaphore
 *	1 - if someone was woken
 */
int
vsema(register sema_t *sp)
{
	register int s;
	register short count;
	_SEMAMETER(k_semameter_t *km = _smeter(sp);)

	s = sema_lock(sp);

	count = ++sp->s_un.s_st.count;
	if (count <= 0) {
#ifdef SEMAMETER
		if (km) {
			km->s_vsema++;
			km->s_lastpc = (inst_t *)__return_address;
			slog(SEMAOP_VSEMA, SSOP_VWAKE, sp, sp->s_queue,
			     km, (inst_t *)__return_address);
			km->s_thread = sp->s_queue;
			km->s_id = sp->s_queue ? sp->s_queue->k_id : 0;
			_SEMA_KHIST(DOHOLDLOG(km);)
		}
#endif
		return semawake(sp, s);
	}

#ifdef SEMAMETER
	if (km) {
		km->s_vsema++;
		km->s_vnoslp++;
		km->s_lastpc = (inst_t *)__return_address;
		km->s_thread = private.p_curkthread;
		km->s_id = private.p_curkthread ? private.p_curkthread->k_id:0;
		slog(SEMAOP_VSEMA, SSOP_VNOWAKE, sp, 0, km,
		     (inst_t *)__return_address);
		_SEMA_KHIST(DOHOLDLOG(km);)
	}
#endif

	sema_unlock(sp, s);
	/* The sync object has been unlocked at this point, making it possible
	 * for another thread to free the sync object's memory.  Referencing
	 * the sync object after this point is not guaranteed safe.
	 */
	return(0);
}

/*
 * Called to conditionally acquire a semaphore.
 * Returns:
 *	0 - couldn't acquire semaphore
 *	1 - semaphore acquired
 */
int
cpsema(register sema_t *sp)
{
	register int s;
	_SEMAMETER(k_semameter_t *km = _smeter(sp);)

	s = sema_lock(sp);

	if (sp->s_un.s_st.count <= 0) {
#ifdef SEMAMETER
		if (km) {
			km->s_cpsema++;
#ifdef SEMA_KHIST
			{ k_histo_t *kp;
			if (kp = HMETER(km))
				COUNTLOG(kp, -sp->s_un.s_st.count,
						(__psint_t)__return_address); }
#endif
			slog(SEMAOP_CPSEMA, SSOP_CPMISS, sp, 0, km,
			     (inst_t *)__return_address);
		}
#endif
		sema_unlock(sp, s);
		LSTAT_SEMA_UPDATE(sp, LSTAT_ACT_NO_WAIT);
		return(0);
	}

	sp->s_un.s_st.count--;

#ifdef SEMAMETER
	if (km) {
		km->s_cpsema++;
		km->s_cphits++;
		km->s_thread = private.p_curkthread;
		km->s_id = private.p_curkthread ? private.p_curkthread->k_id:0;
		km->s_lastpc = (inst_t *)__return_address;
		slog(SEMAOP_CPSEMA, SSOP_CPHIT, sp, 0,
		     km, (inst_t *)__return_address);
		_SEMA_KHIST(STARTHOLDLOG(km, (inst_t *)__return_address);)
	}
#endif
	sema_unlock(sp, s);
	LSTAT_SEMA_UPDATE(sp, LSTAT_ACT_REJECT);
	return(1);
}

/*
 * unsema - remove a thread from the queue on a semaphore before the
 *	 semaphore is v'd.  psema will signal the thread that no v
 *	 occured.
 */
/*ARGSUSED*/
void
unsema(sema_t *sp, kthread_t *kt, inst_t *ra)
{
	ASSERT(kt_islocked(kt));

	/*
	 * This avoids a deadlock when we're trying to remove a thread
	 * from a semaphore, and it's already off but trying to get back
	 * on.
	 */
	while (!nested_bittrylock(&sp->s_un.s_lock, SEMA_LOCK)) {
		if (kt->k_wchan != (caddr_t)sp)
			return;
		DELAY(1);
	}

	if (semarmproc(sp, kt) != 0) {
#ifdef SEMAMETER
		k_semameter_t *km = _smeter(sp);
		if (km) {
			km->s_unsema++;
			slog(SEMAOP_UNSEMA, SSOP_RMV, sp, kt, km, ra);
		}
#endif
		ASSERT((kt->k_flags & KT_SYNCFLAGS) == KT_WSEMA);
		kt->k_prtn = 0;
		thread_unblock(kt, KT_WSEMA);
	}
	_sema_unlock(sp);
}

/*
 * semaaddproc - add a process to the queue on a particular
 * semaphore.  Assumes semaphore queue locked on entry, leaves it
 * locked on exit.
 *
 * It would have been nice for the sleep/count semaphores to use a
 * prio ordered queue but unfortunately there is some code in the
 * system that relies on FIFO ordering.
 */
static void
semaaddproc(sema_t *sp, kthread_t *tp)
{
	_SEMAMETER(k_semameter_t *km;)
	kthread_t *tt = sp->s_queue;

	ASSERT(tp->k_flink == tp && tp->k_blink == tp);
	ASSERT(tp->k_wchan == 0);
	tp->k_wchan = (caddr_t)sp;

	if (!tt) {
		sp->s_queue = tp;
	} else {
		/*
		 * It may appear to the casual observer that we're
		 * putting the new kthread on the head of the list.
		 * Note, however, that this is a floating doubly-linked
		 * list, in which the head is determined by the s_queue
		 * pointer, and that inserting in front of the 'head'
		 * element is the same as appending to the end of the list.
		 */
		tp->k_flink = tt;
		tp->k_blink = tt->k_blink;
		tt->k_blink->k_flink = tp;
		tt->k_blink = tp;
	}

#ifdef SEMAMETER
	km = _smeter(sp);
	if (km)
		upwait(km);
#endif
}

/*
 * semarmproc - delete a particular thread from the queue
 * of a particular semaphore.
 * Assumes semaphore locked on entry, leaves it locked on exit.
 * Returns:
 *	0 - thread not on queue
 *	1 - thread on queue and removed
 */
static int
semarmproc(sema_t *sp, kthread_t *kt)
{
	_SEMAMETER(k_semameter_t *km;)
	kthread_t *bkt, *fkt;

	/*
	 * The semaphore lock protects the kthread's k_wchan.
	 * By the time we've gotten here, kt might have gotten a vsema
	 * and have been taken off sv's queue, but the waker will have
	 * turned off k_wchan.
	 */
	if ((sema_t *)kt->k_wchan != sp)
		return 0;

	fkt = kt->k_flink;
	kt->k_wchan = 0;

	ASSERT(fkt && kt->k_blink);

	if (kt != fkt) {
		bkt = kt->k_blink;
		fkt->k_blink = bkt;
		bkt->k_flink = fkt;
		sp->s_queue = fkt;
	} else {
		sp->s_queue = NULL;
		ASSERT(sp->s_un.s_st.count >= -1);
	}

	kt->k_flink = kt->k_blink = kt;
	sp->s_un.s_st.count++;

#ifdef SEMAMETER
	km = _smeter(sp);
	if (km)
		downwait(km);
#endif
	return(1);
}

/*
 * semadq - dequeue the first thread on a semaphore.
 * Semaphore is locked on entry, left locked on exit;
 * semaphore's count has already been adjusted.
 *
 * Returns:
 *	NULL if queue empty
 *	proc ptr of first on queue
 */
static kthread_t *
semadq(sema_t *sp)
{
	_SEMAMETER(k_semameter_t *km;)
	kthread_t *kt = sp->s_queue;
	kthread_t *fkt;

	ASSERT(kt);

	fkt = kt->k_flink;
	kt->k_wchan = 0;

	if (kt != fkt) {
		fkt->k_blink = kt->k_blink;
		kt->k_blink->k_flink = fkt;
		sp->s_queue = fkt;
	} else {
		sp->s_queue = NULL;
		ASSERT(sp->s_un.s_st.count >= 0);
	}

	kt->k_flink = kt->k_blink = kt;

#ifdef SEMAMETER
	km = _smeter(sp);
	if (km)
		downwait(km);
#endif
	return(kt);
}

#undef valusema
short
valusema(sema_t *sp)
{
	return(sp->s_un.s_st.count);
}

#ifdef SEMAMETER
/*
 * Called to log an entry in the history buffer.
 * Always called with raised spl
 */
static int
rslog(
	uint	op,
	uint	subop,
	sema_t	*sp,
	kthread_t *wkt,
	k_semameter_t *km,
	inst_t	*callpc)
{
	int s;
	semahist_t *hp;
	kthread_t *kt = private.p_curkthread;
	extern int dispseq;

	/* if we don't have a meter/history struct OR asked not to record
	 * history, return
	 */
	if (sp->s_un.s_st.flags & SEMA_NOHIST)
		return 0;
	
	if (!km)
		km = _smeter(sp);
	if (!km)
		return 0;

	s = mutex_spinlock(&histlock);
	hp = semahist+histpos;
	histpos = (histpos + 1) % histmax;
	mutex_spinunlock(&histlock, s);

	hp->h_sid = sp;
	hp->h_thread = kt;
	if (kt)
		hp->h_id = kt->k_id;
	else
		hp->h_id = 0;
	hp->h_woken = wkt;
	if (wkt != 0)
		hp->h_wid = wkt->k_id;
	else
		hp->h_wid = 0;
	hp->h_cpuid = cpuid();
	hp->h_op = op;
	hp->h_subop = subop;
	hp->h_callpc = (caddr_t)callpc;
	hp->h_blink = km->s_blink;
	hp->h_seq = dispseq;
	hp->h_ts = get_timestamp();
	km->s_blink = hp;
	return 0;
}
#endif

/*
 * Size/resize the length of the semaphore history table.
 *
 * XXX There's some concern about how this is allocated...
 * XXX Should go into .h file...
 */
int
rslog_size(int sz)
{
	int s;
	semahist_t *hp;
	semahist_t *op;
	int osz;
	int bsz;
	
	/*
	 * Set up the original lhistmax value, if appropriate.
	 */
	s = mutex_spinlock(&histlock);
	if (semalhistmax == (uint)-1) {
		semalhistmax = histmax;
		semalhist = semahist;
	}
	mutex_spinunlock(&histlock, s);

	/*
	 * Easy.
	 */
	if (sz == histmax)
		return 0;
	if (sz < 0)
		return EINVAL;
	
	if (sz > semalhistmax) {
		hp = kmem_alloc(sz * sizeof(semahist_t), VM_DIRECT|VM_NOSLEEP);
		if (!hp) {
			return ENOMEM;
		}
		bsz = sz;
	} else if (!sz) {
		hp = 0;
		sz = 0;
	} else {
		hp = semalhist;
		bsz = semalhistmax;
	}
	
	/*
	 * Switcheroo.
	 */
	s = mutex_spinlock(&histlock);
	osz = histmax;
	if (osz > semalhistmax)
		op = semahist;
	else
		op = 0;
	if (hp)
		bzero(hp, bsz * sizeof(semahist_t));
	semahist = hp;
	histmax = sz;
	histpos = 0;
	mutex_spinunlock(&histlock, s);
	
	/*
	 * Clean up.
	 */
	if (osz && op)
		kmem_free(op, osz * sizeof(semahist_t));
	return 0;
}

/*
 * This function is called to add to the histogram for a semaphore
 */
/*ARGSUSED*/
int
semakhistlog(k_histelem_t *hp, int value, inst_t *pc)
{
#ifdef SEMA_KHIST
	int ix;
	int upper;
	int diff;

	/*
	 * Update the totals here.
	 */
	hp->he_total += value;
	hp->he_updates++;
	
	/*
	 * If we exceeded threshold, update also.
	 */
	if ((int)value > (int)hp->he_value) {
		hp->he_value = value;
		hp->he_pc = pc;
	}
	
	/*
	 * If histogram elements not enabled, return.
	 */
	if (!hp->he_length || !hp->he_data || !hp->he_offset)
		return 0;

	switch (hp->he_policy) {
	case HP_ADDOFF:
		diff = ((int)hp->he_length * (int)hp->he_offset);
		upper = (int)value - (int)hp->he_base;
		if (upper <= 0)
			ix = 0;
		else {
			ix = ((value - hp->he_base - 1) / hp->he_offset) + 1;
			if (ix >= hp->he_length)
				ix = hp->he_length - 1;
		}
		break;
	case HP_MULOFF:
		if ((int)value < (int)hp->he_base)
			ix = 0;
		else {
			upper = hp->he_base * hp->he_offset;
			for (ix = 1, upper = hp->he_base;
						ix < hp->he_length;
						upper *= hp->he_offset, ix++) {
				if ((int)value <= (int)upper) 
					continue;
				ix -= 1;
				break;
			}
			if (ix >= hp->he_length)
				ix = hp->he_length - 1;
		}
		break;
	default:
		return 0;
	}

	/*
	 * Update here.
	 */	
	hp->he_data[ix]++;
	
	return 1;
#else
	return 0;
#endif
}

/*
 * Clear the semaphore histo
 */
/*ARGSUSED*/
void
semakhistclr(sema_t *sp)
{
#ifdef SEMA_KHIST
	_SEMAMETER(k_semameter_t *km = _smeter(sp);)
	k_histo_t *khp;

	if (!km)
		return;

	sema_lock(sp);

	khp = HMETER(km);
	if (!khp) {
		sema_unlock(sp);
		return;
	}

	khp->h_ts = 0;
	khp->h_pc = 0;
	CLRHISTOLOG(&khp->h_hold);
	CLRHISTOLOG(&khp->h_waiting);
	CLRHISTOLOG(&khp->h_count);

	sema_unlock(sp);
#endif
}

/*ARGSUSED*/
int
semakhistinit(sema_t *sp, unsigned short length)
{
#ifdef SEMA_KHIST
	k_histo_t *khp;
	int error = 0;
	_SEMAMETER(k_semameter_t *km = _smeter(sp);)

	/*
	 * Ping to see if we support sema khist
	 */
	if (!sp || !length)
		return 0;

	if (!km) {
		if (semameteron(sp, NULL, -1) < 0)
			return ENOMEM;
		km = _smeter(sp);
	}
	sema_lock(sp);
	khp = HMETER(km);
	if (!khp) {
		km->s_histo = (k_histo_t *)kmem_alloc(sizeof(*khp),
						VM_DIRECT|VM_NOSLEEP);
		if (!km->s_histo) {
			sema_unlock(sp);
			semameteroff(sp);
			return ENOMEM;
		}
		bzero(km->s_histo, sizeof(*khp));
		khp = km->s_histo;
	}

	FREE_HISTO_DATA(&khp->h_count);
	FREE_HISTO_DATA(&khp->h_waiting);
	FREE_HISTO_DATA(&khp->h_hold);

	if (length) {
		ALLOC_HISTO_DATA(&khp->h_hold, length);
		ALLOC_HISTO_DATA(&khp->h_waiting, length);
		ALLOC_HISTO_DATA(&khp->h_count, length);
		if (!khp->h_hold.he_data || !khp->h_waiting.he_data ||
						!khp->h_count.he_data) {
			FREE_HISTO_DATA(&khp->h_count);
			FREE_HISTO_DATA(&khp->h_waiting);
			FREE_HISTO_DATA(&khp->h_hold);
			error = ENOMEM;
		}
	}

	sema_unlock(sp);
	return error;
#else
	return ENXIO;
#endif
}

/*
 * make a null terminated name using the prefix and a sequencer
 * returns ptr to name
 */
char *
makesname(
	char *name,		/* should be a char name[METER_NAMSZ] */
	const char *pref,
	register long ival)
{
	register char *p;
	register int i;
	register long n;

	/* put in prefix */
	for  (i = 0; i < METER_NAMSZ-1 && *pref; i++, pref++)
		name[i] = *pref;

	if (ival < 0 || !pref) {
		while (i < METER_NAMSZ-1) {
			name[i] = '\0';
			i++;
		}
	} else {
		/*
		 * all blanks between prefix & number is zeroes
		 */
		while (i < METER_NAMSZ-1) {
			name[i] = '0';
			i++;
		}

		while (ival >= 1000000)
			ival %= 1000000;	/* let's be safe */
		p = &name[METER_NAMSZ-2];

		while (ival > 9) {
			n = ival;
			ival /= 10;
			*p-- = n - ival * 10 + '0';
		}
		*p-- = ival + '0';

	}

	name[METER_NAMSZ-1] = '\0';
	return(name);
}



/*
 * mrlock - implement multi-access/single-update lock.
 *
 * Multiple accessers or one updater are allowed in.
 *
 * This version does not allow recursive update, nor spin waiting.
 * mrlocks locks may be demoted or conditionally promoted.
 *
 * lbits:
 *
 *     16	16
 *  +------+---------+
 *  | WCNT |B|U|ACCNT|	lbits
 *  +------+---------+
 *  | PCNT |V|   |Q|M|	psema count, queue mgmt bits
 *  +----------------+-------------+
 *  |   mrlock wait-queue pointer  |
 *  +------------------------------+
 *
 *	V	MR_V		outstanding V operation on wait-queue
 *	PCNT	MR_PCNT		count of P operations on wait-queue
 *	Q	MR_QLOCK	lock bit on wait-queue
 *	WCNT	MR_WAIT		# waiting for lock
 *	B	MR_BARRIER	barrier-type mrlock
 *	U	MR_UPD		lock held for update
 *	ACCNT	MR_ACC		# of accessers currently holding lock
 *
 * When attemping to grant mrlock, assembler atomic-op routines check
 * whether the proposed lock would be valid.  If so, the lock is
 * acquired, and caller returns.
 *
 * If not, the MR_WAIT bits are incremented, and the caller is queued
 * on the mrlock's wait-queue.
 *
 * When a lock is released (last accesser or only updater releases),
 * if the MR_WAIT counter is non-zero, it is decremented by one, the
 * lock is set to update-mode, and the first waiter is dequeued from
 * the wait queue (via mrpsema()).  The thread is awakened, owning the
 * lock -- if it has wanted only access mode, mr_demote is called, which
 * demotes the state of the lock, and pulls off any more accessers-to-be
 * from the wait queue (see mr_dqacc()).
 *
 * If access lock is requested, but the lock is initialized as barrier-type
 * and _any_ callers are waiting (MR_WAIT), caller queues on the wait list,
 * and waits until woken (with the lock).
 *
 * MR_ACC bits can never overflow into (underflow out of) MR_UPD or MR_WAIT --
 * accessers are wait-listed when access bits have already flowed into the
 * high MR_ACC bit.  When _any_ MR_ACC bits are set, MR_UPD must be zero,
 * so it acts as a redline, avoiding underflow when decrementing MR_ACC
 * on unlock.
 */

#ifdef MP
#define mr_islocked(mrp) ((mrp)->mr_qlock & MR_QLOCK)
#else
#define mr_islocked(mrp) 1 /* FIX: isn't this splhi() or something?*/
#endif

#ifdef DEBUG
#define INC_MRIA_COUNT(mriap)	((mriap)->mria_count++)
#define DEC_MRIA_COUNT(mriap)	((mriap)->mria_count--)
#define INC_NHOLDERS(mrp)	((mrp)->mr_nholders++)
#define DEC_NHOLDERS(mrp)	(ASSERT((mrp)->mr_nholders > 0), (mrp)->mr_nholders--)
#define SET_PQPRI(pq, pri)	((pq)->pq_pri = (pri))
#define CERT_DEBUG(x)		ASSERT(x)
#else
#define INC_MRIA_COUNT(mriap)
#define DEC_MRIA_COUNT(mriap)
#define INC_NHOLDERS(mrp)
#define DEC_NHOLDERS(mrp)
#define SET_PQPRI(pq, pri)
#define CERT_DEBUG(x)
#endif

#if defined(SEMASTAT)
/* note that SEMASTAT defeats DEBUG for lock consistency checks */
#ifdef MP
extern int mutex_bitlock_ra(uint *, uint, void *);
extern int mutex_bittrylock_ra(uint *, uint, void *);
#else
#define mutex_bitlock_ra(l,s,r) mutex_bitlock(l,s)
#define mutex_bittrylock_ra(l,s,r) mutex_bittrylock(l,s)
#endif
#undef  mr_lockq
#undef  mr_trylockq
#define mr_lockq(mrp)           mutex_bitlock_ra(&(mrp)->mr_qlock, MR_QLOCK, __return_address)
#define mr_trylockq(mrp)        mutex_bittrylock_ra(&(mrp)->mr_qlock, MR_QLOCK, __return_address)
#define mr_lockq_ra(mrp,ra)     mutex_bitlock_ra(&(mrp)->mr_qlock, MR_QLOCK, ra)

#elif defined(MRLOCK_CONSISTENCY_CHECK) || defined(DEBUG)
void
mr_check_consistency(mrlock_t *mrp)
{
	mri_t *mrip, *fp;
	int i;

	ASSERT_ALWAYS(mr_islocked(mrp));
	for (i = 0; i < NSCHEDCLASS; i++) {
		fp = mrp->mr_holders.pq_pq[i];
		if (!fp)
			continue;
		if (fp->mri_mrlock != mrp)
			cmn_err(CE_PANIC,
			   "mrlock 0x%x has MRI 0x%x with unknown lock 0x%x",
				   mrp, fp, fp->mri_mrlock);
		for (mrip = fp->mri_flink; mrip != fp; mrip = mrip->mri_flink)
		    if (mrip->mri_mrlock != mrp)
			cmn_err(CE_PANIC,
			   "mrlock 0x%x has MRI 0x%x with unknown lock 0x%x",
				   mrp, mrip, mrip->mri_mrlock);
	}
}

#undef mr_lockq
#undef mr_unlockq
#define mr_lockq_ra(mrp,ra)	mr_lockq(mrp)

int
mr_lockq(mrlock_t *mrp)
{
	int s;

	s = mutex_bitlock(&(mrp)->mr_qlock, MR_QLOCK);
	mr_check_consistency(mrp);
	return s;
}

void
mr_unlockq(mrlock_t *mrp, int s)
{
	mr_check_consistency(mrp);
	mutex_bitunlock(&(mrp)->mr_qlock, MR_QLOCK, s);
}

#else
#define mr_lockq_ra(mrp,ra)     mr_lockq(mrp)
#endif

/*
 * Multi-access (multi-reader), single-update (single-writer)
 * locking routines.
 */
k_mrmeter_t *mrmeter(mrlock_t *);

/*
 * mrinit - init a multi-reader single writer lock
 */
/*ARGSUSED3*/
void
mrlock_init(mrlock_t *mrp, int lock_type, char *name, long sequence)
{
	bzero(mrp, sizeof(*mrp));
	mrp->mr_lbits |= MR_BARRIER; /* Only thing supported */
	if (lock_type & MRLOCK_ALLOW_EQUAL_PRI)
		mrp->mr_qbits |= MR_ALLOW_EQUAL_PRI;
	if (lock_type & MRLOCK_DBLTRIPPABLE)
		mrp->mr_qbits |= MR_DBLTRIPPABLE;
	SET_PQPRI(&mrp->mr_holders, PX_PRIO_MAX+1); /* one more than maximum */
	SET_PQPRI(&mrp->mr_waiters, (-PX_PRIO_MAX)-1); /* one less than minimum */


#if defined(SEMAMETER)
	_SEMAINFOP(mrp->mr_info = NULL;)
	if (defaultsemameter)
		mrmeteron(mrp, name, sequence);
#elif defined(SEMANAME)
	_SEMAINFOP(mrp->mr_info = NULL;)
	allocwchaninfo(mrp, name, sequence,
		       mrinfolist, sizeof (wchaninfo_t),
		       &mrinfo_zone, "mrinfo");
#else
	_SEMAINFOP(mrp->mr_info = name;)
#endif

#if defined(DEBUG) || defined(SYNCINFO)
	atomicAddInt(&syncinfo.mrlock.current, 1);
	atomicAddInt(&syncinfo.mrlock.initialized, 1);
#endif
}


/*
 * cmrlock - conditionally lock - no sleeping
 * Returns: 1 if got lock
 *	    0 otherwise
 */
int
cmrlock(mrlock_t *mrp, int type)
{
	if (type == MR_ACCESS)
		return mrtryaccess(mrp);
	else
		return mrtryupdate(mrp);
}

void
init_mria(mria_t *mria, kthread_t *kt)
{
	mri_t *mrip;
	uchar_t i;

	/* Assumes that the structure is all zero otherwise */
	for (mrip = mria->mria_mril, i = 0; 
	    mrip < &mria->mria_mril[MRI_MAXLINKS]; mrip++, i++)
	{
		mrip->mri_flink = mrip->mri_blink = mrip;
		mrip->mri_index = i;
	}
	mria->mria_kt = kt;
}


/*
 * Insert into mrlock holder list in priority order (no ordering among
 * equal priority threads)
 */
INLINE static void
mr_insert_holder(priq_t *pq, mri_t *mrip, register int kpri)
{
	register mri_t *fp;

	ASSERT(mrip->mri_flink == mrip && mrip->mri_blink == mrip);

	mrip->mri_pri = kpri;
	/* maintain minimum */
	SET_PQPRI(pq, (pq->pq_pri > kpri) ? kpri : pq->pq_pri);
	if (kpri <= PBATCH_CRITICAL && kpri >= PBATCH) {
		#pragma mips_frequency_hint FREQUENT
		/* Insert in appropriate class, no ordering 	*/
		fp = pq->pq_pq[-kpri];
		if (!fp)
			pq->pq_pq[-kpri] = mrip;
		else  {
			/*
			 * arbitrarily insert at end; this doesn't require
			 * changing pq->pq_pq[-kpri] (the queue head).
			 */
			mrip->mri_flink = fp;
			mrip->mri_blink = fp->mri_blink;
			fp->mri_blink->mri_flink = mrip;
			fp->mri_blink = mrip;
		}
	}  else {
		/*
		 * These are priority ordered queues: either
		 * realtime (0 to 255) or weightless (-5 to -255).
		 */
		mri_t **qp;

		ASSERT(kpri >= 0 || kpri <= PWEIGHTLESS);
		qp = (kpri >= 0) ? &pq->pq_pq[0] : &pq->pq_pq[-PWEIGHTLESS];
		fp = *qp;
		if (!fp)
			*qp = mrip;
		else {
			if (kpri <= fp->mri_pri)
				*qp = mrip;
			else {
				fp = fp->mri_flink;
				while (fp != *qp) {
					if (kpri <= fp->mri_pri)
						break;
					fp = fp->mri_flink;
				}
			}
			mrip->mri_flink = fp;
			mrip->mri_blink = fp->mri_blink;
			fp->mri_blink->mri_flink = mrip;
			fp->mri_blink = mrip;
		}
	}
}

INLINE static void
mr_insert_waiter(kpriq_t *pq, kthread_t *kt, int kpri)
{
	register kthread_t *fkt;

	ASSERT(kt->k_flink == kt && kt->k_blink == kt);

	if (kpri == PTIME_SHARE_OVER) {
		#pragma mips_frequency_hint FREQUENT
		kpri = PTIME_SHARE;
	}
	kt->k_mrqpri = kpri;
	/* maintain maximum */
	SET_PQPRI(pq, (pq->pq_pri < kpri) ? kpri : pq->pq_pri);
	if (kpri <= PBATCH_CRITICAL && kpri >= PBATCH) {
		#pragma mips_frequency_hint FREQUENT
		/* Insert in appropriate class, among equals at end */
		fkt = pq->pq_pq[-kpri];
		if (!fkt) {
			#pragma mips_frequency_hint FREQUENT
			pq->pq_pq[-kpri] = kt;
		} else  {
			/*
			 * Insert at end.
			 */
			kt->k_flink = fkt;
			kt->k_blink = fkt->k_blink;
			fkt->k_blink->k_flink = kt;
			fkt->k_blink = kt;
		}
	}  else {
		/*
		 * These are priority ordered queues: either
		 * realtime (0 to 255) or weightless (-5 to -255).
		 */
		kthread_t **qp;

		ASSERT(kpri >= 0 || kpri <= PWEIGHTLESS);
		qp = (kpri >= 0) ? &pq->pq_pq[0] : &pq->pq_pq[-PWEIGHTLESS];
		fkt = *qp;
		if (!fkt)
			*qp = kt;
		else {
			if (kpri > fkt->k_mrqpri)
				*qp = kt;
			else {
				fkt = fkt->k_flink;
				while (fkt != *qp) {
					if (kpri > fkt->k_mrqpri)
						break;
					fkt = fkt->k_flink;
				}
			}
			kt->k_flink = fkt;
			kt->k_blink = fkt->k_blink;
			fkt->k_blink->k_flink = kt;
			fkt->k_blink = kt;
		}
	}
}


static mri_t *
findheldmri_gen(mrlock_t *mrp, kthread_t *kt)
{
	mri_t *mrip;

	for (mrip = &kt->k_mria.mria_mril[2];
		mrip < &kt->k_mria.mria_mril[MRI_MAXLINKS]; mrip++)
	{
		if (mrip->mri_mrlock == mrp)
			return mrip;
	}
	{
		mria_t *mriap;

		/* Unusual overflow case */
		for (mriap = kt->k_mria.mria_next; mriap; mriap = mriap->mria_next) {
			for (mrip = mriap->mria_mril;
				mrip < &mriap->mria_mril[MRI_MAXLINKS]; mrip++)
			{
				if (mrip->mri_mrlock == mrp)
					return mrip;
			}
		}
	}
	return 0;
}

INLINE static mri_t *
findheldmri(mrlock_t *mrp, kthread_t *kt)
{
	if (kt->k_mria.mria_mril[0].mri_mrlock == mrp)
		return(&kt->k_mria.mria_mril[0]);
	else if (kt->k_mria.mria_mril[1].mri_mrlock == mrp)
		return(&kt->k_mria.mria_mril[1]);
	return(findheldmri_gen(mrp, kt));
}

int
mrlock_mine(mrlock_t *mrp, kthread_t *kt)
{
	return(findheldmri(mrp, kt) != 0);
}

static mri_t *
findfreemri_gen(kthread_t *kt)
{
	mri_t *mrip;
	mria_t *mriap, *mriapp;

	for (mriap = &kt->k_mria; mriap; mriap = mriap->mria_next) {
		for (mrip = mriap->mria_mril;
		    mrip < &mriap->mria_mril[MRI_MAXLINKS]; mrip++)
		{
			if (mrip->mri_mrlock == 0) {
				INC_MRIA_COUNT(&kt->k_mria); /*count all mria */
				INC_MRIA_COUNT(mriap); /* count in this chunk */
				return mrip;
			}
		}
		mriapp = mriap;
	}
#ifdef DEBUG_MRLOCK
	cmn_err(CE_WARN, "Allocating Extra Chunk of mria, kt = 0x%x\n", kt);
#endif
	mriap = kmem_zone_zalloc(mria_zone_private, VM_NOSLEEP);
	if (!mriap) {
		cmn_err(CE_PANIC,
		    "Out of memory allocating mrlock inheritance structure ... \nIncrease the number of mria through the tunable variable mrlock_num_mria.\n");
	}
	init_mria(mriap, kt);
	mriapp->mria_next = mriap;
	INC_MRIA_COUNT(&kt->k_mria); /*count all mria */
	INC_MRIA_COUNT(mriap); /* count in this chunk */
	return(&mriap->mria_mril[0]);
}

INLINE static mri_t *
findfreemri(kthread_t *kt)
{
	if (!kt->k_mria.mria_mril[0].mri_mrlock)
		return(&kt->k_mria.mria_mril[0]);
	else if (!kt->k_mria.mria_mril[1].mri_mrlock)
		return(&kt->k_mria.mria_mril[1]);
	return(findfreemri_gen(kt));
}

#ifdef DEBUG
static short
find_holder_lpri(priq_t *pq)
{
	mri_t **qp;

	for (qp = &(pq->pq_pq[NSCHEDCLASS-1]); qp >= pq->pq_pq; qp--)
		if (*qp)
			return ((*qp)->mri_pri);
	/* priq is empty */ 
	return (PX_PRIO_MAX+1);
}

static short
find_waiter_hpri(kpriq_t *pq)
{
	kthread_t **qp;

	for (qp = pq->pq_pq; qp < &pq->pq_pq[NSCHEDCLASS]; qp++)
		if (*qp)
			return ((*qp)->k_mrqpri);
	/* priq is empty */ 
	return (-(PX_PRIO_MAX)-1);
}
#endif

void INLINE
mrrecordholder(mrlock_t *mrp, kthread_t *kt)
{
	mri_t *mrip;

	if ((mrp->mr_qbits & MR_DBLTRIPPABLE) &&
	     (mrip = findheldmri(mrp, kt)))
	{
		#pragma mips_frequency_hint NEVER
		ASSERT((int)mrip->mri_count >= 0);
		mrip->mri_count++;
	} else  {
		ASSERT(findheldmri(mrp, kt) == 0); /* avoid dbltrips on !DBLTRIPPABLE */ 
		mrip = findfreemri(kt);
		ASSERT(mrip);
		ASSERT(mrip->mri_count == 0);
		mrip->mri_mrlock = mrp;
		mr_insert_holder(&mrp->mr_holders, mrip, kt->k_basepri);
		INC_NHOLDERS(mrp);
	}
}


INLINE static void
mr_remove_holder_link(priq_t *pq, mri_t *mrip, register int kpri)
{
	mri_t *fp;

	if (kpri <= PBATCH_CRITICAL && kpri >= PBATCH) {
		#pragma mips_frequency_hint FREQUENT
		kpri = -kpri;
	} else if (kpri >= 0)
		kpri = 0;
	else {
		ASSERT(kpri <= PWEIGHTLESS);
		kpri = -PWEIGHTLESS;
	}

	fp = mrip->mri_flink;
	if (mrip != fp) {
		fp->mri_blink = mrip->mri_blink;
		mrip->mri_blink->mri_flink = fp;
		mrip->mri_flink = mrip->mri_blink = mrip;
		if (pq->pq_pq[kpri] == mrip)
			pq->pq_pq[kpri] = fp;
	} else {
		#pragma mips_frequency_hint FREQUENT
		ASSERT(mrip->mri_flink == mrip->mri_blink);
		ASSERT(pq->pq_pq[kpri] == mrip);
		pq->pq_pq[kpri] = NULL;
	}
	mrip->mri_mrlock = 0;
}

void INLINE
mrremoveholder(mrlock_t *mrp, kthread_t *kt)
{
	mri_t *mrip;

	ASSERT(kt);
	mrip = findheldmri(mrp, kt);
	if (mrp->mr_qbits & MR_DBLTRIPPABLE) {
		#pragma mips_frequency_hint NEVER
		ASSERT((int)mrip->mri_count >= 0);
		if (mrip->mri_count != 0) {
			mrip->mri_count--;
			return;
		}
	}
	DEC_NHOLDERS(mrp);
	mr_remove_holder_link(&mrp->mr_holders, mrip, mrip->mri_pri);
	DEC_MRIA_COUNT(&kt->k_mria);
	SET_PQPRI(&mrp->mr_holders, find_holder_lpri(&mrp->mr_holders));
	/* Drop inheritance */
	if (kt->k_inherit == mrp) {
		#pragma mips_frequency_hint NEVER
		kt_nested_lock(kt); /* Doesn't this double trip? */
		if (kt->k_inherit != mrp) { /* test, lock, re-test */
			kt_nested_unlock(kt);
			return;
		}
		ASSERT(!(kt->k_flags & KT_MUTEX_INHERIT));
		drop_inheritance(mrp, RES_MRLOCK, kt);
		kt_nested_unlock(kt);
		/*
		 * we need to wakeup the waiter,
		 * so he goes & kicks the next holder
		 */
		kt = mrlock_hpri_waiter(mrp); /* variable kt is reused */
		if (kt) {
			if (!kt_nested_trylock(kt))
				return; /* Don't try too hard! */
			if (kt->k_flags & KT_WMRLOCK) {
				kt->k_wchan = 0;
				thread_unblock(kt, KT_WMRLOCK);
			}
			kt_nested_unlock(kt);
		}
	}
}

/*
 * mrfree - free a multi-reader single writer lock
 */

/*ARGSUSED*/
void
mrfree(mrlock_t *mrp)
{
	int s;
	mri_t *mrip;
	kthread_t *kt = private.p_curkthread;

	if (!(mrp->mr_lbits & MR_BARRIER)) {
		return;
		/* panic("MRLOCK NOT INITIALIZED!"); FIX */
	}

	if (kt) {
		s = mr_lockq(mrp);
		kt_nested_lock(kt);
		mrip = findheldmri(mrp, kt);
		if (mrip) { /* Heck, the guy could have unlocked before freeing! */
			mr_remove_holder_link(&mrp->mr_holders, mrip, mrip->mri_pri);
			SET_PQPRI(&mrp->mr_holders, find_holder_lpri(&mrp->mr_holders));
			DEC_MRIA_COUNT(&kt->k_mria);
			DEC_NHOLDERS(mrp);
		}
		kt_nested_unlock(kt);
		/* Avoid racing with remaining mrremoveholder's */
		while (mrlock_lpri_owner(mrp) != NULL) {
			mr_unlockq(mrp,s);
			DELAY(3);
			s = mr_lockq(mrp);
		}
		CERT_DEBUG(find_holder_lpri(&mrp->mr_holders) == PX_PRIO_MAX+1);
#ifdef DEBUG
		ASSERT(mrp->mr_nholders == 0);
#endif
		mr_unlockq(mrp, s);
	}

#if defined(SEMAMETER)
	mrmeteroff(mrp);
#elif defined(SEMANAME)
	freewchaninfo(mrp, mrinfolist, mrinfo_zone);
#endif

#if defined(DEBUG) || defined(SYNCINFO)
	atomicAddInt(&syncinfo.mrlock.current, -1);
#endif
}

/*
 * Destroy and free up a chain of mria structures.  Called by kthread_destroy()
 * in order to free up any extra mria structures that may have been allocated
 * by the thread in findfreemri_gen().  We assume that the first mria pointer
 * is a pointer into the middle of the kthread structure itself, so we don't
 * want to free that one!
 */
void
destroy_mria(mria_t *mriap)
{
	mria_t *next_mriap;

	for (mriap = mriap->mria_next; mriap; mriap = next_mriap) {
		next_mriap = mriap->mria_next;
		kmem_zone_free(mria_zone_private, mriap);
	}
}

kthread_t *
mrlock_lpri_owner(mrlock_t *mrp)
{
	mri_t **qp;
	priq_t *pq = &mrp->mr_holders;

	ASSERT(mr_islocked(mrp));
	for (qp = &(pq->pq_pq[NSCHEDCLASS-1]); qp >= pq->pq_pq; qp--)
		if (*qp)
			return *(MRI_TO_MRIA_KT_P(*qp));
	CERT_DEBUG(pq->pq_pri == PX_PRIO_MAX+1);
	/* priq is empty */ 
	return NULL;
}

/*
 * The peek version is used w/ no mrp lock, so no asserts.
 */
kthread_t *
mrlock_peek_hpri_waiter(mrlock_t *mrp)
{
	kthread_t **qp;
	kpriq_t *pq = &mrp->mr_waiters;

	for (qp = pq->pq_pq; qp < &pq->pq_pq[NSCHEDCLASS]; qp++)
		if (*qp)
			return(*qp);
	/* priq is empty */ 
	return NULL;
}

kthread_t *
mrlock_hpri_waiter(mrlock_t *mrp)
{
	kthread_t **qp;
	kpriq_t *pq = &mrp->mr_waiters;

	ASSERT(mr_islocked(mrp));
	for (qp = pq->pq_pq; qp < &pq->pq_pq[NSCHEDCLASS]; qp++)
		if (*qp)
			return(*qp);
	CERT_DEBUG(pq->pq_pri == (-PX_PRIO_MAX)-1);
	/* priq is empty */ 
	return NULL;
}


kthread_t *
mrlock_resort_queue(mrlock_t *mrp, kthread_t *kt)
{
	ASSERT(mr_islocked(mrp));
	/*
	 * We got here thinking that kt is waiting for mrp by looking at
	 * kt's wchan being equal to mrp. However ...
	 * Kt may have been given the mrlock, but it's w_chan may not have
	 * been nulled out yet. So, kt may not be on the wait queue at this
	 * point. We should use k_blink (being self-looped to kt)
	 * to test if it is not on the waitqueue, since mr_dqacc
	 * uses k_flink to (temporarily) link accessors on a local queue
	 * after it has dequeued an accessor, but before it wakes up
	 * the accessor (and subsequently null its wchan).
	 */
	if (kt->k_blink == kt) {
		/* kt is not on queue */
		return NULL;
	}
	/*
	 * Delete kt & reinsert at new pri.
	 */
	mrdq(mrp, kt);
	mr_insert_waiter(&mrp->mr_waiters, kt, kt->k_pri);
	return kt;
}

static int
#ifdef SEMAMETER
mrpsema(mrlock_t *mrp, int s, int flags, k_mrmeter_t *km)
#elif SEMASTAT
mrpsema(mrlock_t *mrp, int s, int flags, void *ra)
#else
mrpsema(mrlock_t *mrp, int s, int flags)
#endif
{
	int trwake, retval;
	int dqacc;
	kthread_t *kt = private.p_curkthread;
#ifdef CELL
	uint_t blarv;
#endif

	ASSERT(mr_islocked(mrp));
	ASSERT(private.p_curkthread);
	ASSERT(!(flags & ~(KT_WACC|KT_WUPD|KT_LTWAIT|KT_STARVE)));

	if (!(mrp->mr_qbits & MR_V)) {

		/*
		 * On access locks, support limited double tripping.
		 * Also, grant lock to acessor even if barrier is set
		 * (& updator waiting) if acessor is of higher priority.
		 */
		if (!(flags & KT_WUPD)) {
			kthread_t *qkt;

			/*
			 * Support double tripping for barrier locks.
			 * Allow if: this lock is for ACC, lock not held for
			 * update & the lock is already held by thread (in ACC). 
			 */
			if (!(mrp->mr_lbits & MR_UPD) &&
				(mrp->mr_qbits & MR_DBLTRIPPABLE))
			{ 
				mri_t *mrip;
				if (mrip = findheldmri(mrp, kt)) {
#ifdef DEBUG
					int rv = mrcheckupd(mrp);
					CERT_DEBUG(rv == 0);
#else
					(void)mrcheckupd(mrp);
#endif
					mrip->mri_count++;
					mr_unlockq(mrp, s);
					return 1;
				}
			}
			qkt = mrlock_hpri_waiter(mrp);
			/*
			 * Grant the lock if accessor of greater priority (or equal priority
			 * in case of non-barriers) than waiter, and lock is grantable
			 * (mrcheckupd()).
			 */
			if (qkt &&
			    ((kt->k_pri > qkt->k_pri) ||
			      ((mrp->mr_qbits & MR_ALLOW_EQUAL_PRI) && kt->k_pri == qkt->k_pri)) &&
			    mrcheckupd(mrp) == 0)
			{
				ASSERT(kt != NULL);
				mrrecordholder(mrp, kt);
				mr_unlockq(mrp, s);
				return 1;
			}
		}

restart:
		/*
		 * Only kind of double trip which can block was above
		 */
		ASSERT(!findheldmri(mrp, kt));
		ASSERT(mrp->mr_pcnt >= 0);
		mrp->mr_pcnt++;
		ASSERT_ALWAYS(mrp->mr_pcnt > 0); /* catch rollover */
#ifdef SEMAMETER
		if (km) {
			if (!(flags & KT_WUPD)) {
				km->mr_accpslp++;
			} else {
				km->mr_updpslp++;
			}
			if (++km->mr_nwait > km->mr_maxnwait)
				km->mr_maxnwait = km->mr_nwait;
		}
#endif
		ASSERT(private.p_kstackflag == PDA_CURKERSTK);
		ASSERT(!(kt->k_flags & KT_SYNCFLAGS));
		ASSERT(kt->k_flink == kt);

#ifdef CELL
		if ((flags & KT_WUPD) && (mrp->mr_qbits&MR_BEHAVIOR))
			blarv = bla_wait_for_mrlock (mrp);
#endif
		kt_fset(kt, flags&(KT_WUPD|KT_WACC|KT_STARVE));
		mr_insert_waiter(&mrp->mr_waiters, kt, kt->k_pri);
retry:
		while ((retval = resource_test_owner_pri(mrp, RES_MRLOCK)) != BREAK) {
			if (retval == RETRY) {
				continue;
			}
			ASSERT(mrp->mr_pcnt >= 0);
			/*
			 * Before dropping lock, remove kt from waiters
			 * list; otherwise the spl(0) below can result
			 * in additional mrlocks to be taken as part
			 * of interrupt handling (for eg. clock()->caclrss()->
			 * aspacelock()). These additional mrlocks
			 * can result in a concurrent mrrecordholder() with
			 * the third party mrrecordholder() from the
			 * of grant of this mrlock.
			 */
			mrp->mr_pcnt--;
			ASSERT(mrp->mr_pcnt >= 0);
			mrdq(mrp, kt);
#ifdef SEMAMETER
			if (km) {
				if (!(flags & KT_WUPD)) {
					km->mr_accpslp--;
				} else {
					km->mr_updpslp--;
				}
				km->mr_nwait--;
			}
#endif
			kt_fclr(kt, (KT_WUPD|KT_WACC|KT_STARVE));
			mr_nested_unlockq(mrp);
			/*
			 * drop spl to 0 to allow interrupts/context
			 * switches during spins.  This shouldn't be
			 * necessary, but there are cases where we call
			 * mrlock with interrupts disabled.
			 */
			spl0();
			if (retval == SPIN) {
				/* this delay is arbitrary */
				DELAY(3);
			} else {
				ASSERT(retval == YIELD);
				qswtch(RESCHED_Y);
			}
			(void)mr_lockq_ra(mrp,ra);
			/*
			 * While we had dropped the mrp lock & reacquired it,
			 * someone may have set the V bit (we are not on the wait queue).
			 * If V bit set, then grant the lock to ourself, else re-queue
			 * on the wait queue & retry as though first time through.
			 */
			if (!(mrp->mr_qbits & MR_V))
				goto restart;
			else
				goto out;
		}
		ASSERT(mr_islocked(mrp));
		kt_nested_lock(kt);
		if (kt->k_flags & (KT_WUPD|KT_WACC)) {
			/* No one "woke" this thread up in between */
			kt->k_wchan = (caddr_t)mrp;
			/* trade locks */
			mr_nested_unlockq(mrp);
			trwake = thread_blocking(kt,
					KT_NWAKE| (flags & KT_LTWAIT));
			thread_block(kt, MRWAIT, KT_WMRLOCK, trwake, s, 0);
			s = kt_lock(kt);
			if (kt->k_flags & (KT_WUPD|KT_WACC)) {
				/* pri inheritance code kicked us */
				kt_nested_unlock(kt);
				mr_nested_lockq(mrp);
				goto retry;
			}
		} else
			mr_nested_unlockq(mrp);
		kt_unlock(kt, s);

		ASSERT(!(kt->k_flags & KT_WUPD));
		ASSERT(kt->k_blink == kt && kt->k_flink == kt);
#ifdef CELL
		if ((flags & KT_WUPD) && (mrp->mr_qbits&MR_BEHAVIOR))
			bla_got_mrlock (blarv);
#endif
		return (flags&KT_STARVE);
	}

out:
#ifdef SEMAMETER
	if (km) {
		if (!(flags & KT_WUPD))
			km->mr_accphits++;
		else
			km->mr_updphits++;
	}
#endif

	mrp->mr_qbits &= ~MR_V;

	/*
	 * If we didn't have to wait on the sema, it means
	 * that mrvsema was called between the time we registered
	 * ourselves on the mrlock (MR_WAIT counter) and the time
	 * we got here.  In that case the lock must be in update mode,
	 * any any potential accessers are waiting.
	 */
	ASSERT(mrp->mr_lbits & (MR_UPD));
	mrrecordholder(mrp, kt);


	/*
	 * Didn't want update mode?  Downgrade and wake access-waiters.
	 */
	if (flags & KT_WUPD) {
		dqacc = 0;
	} else {
		mrp->mr_lbits &= (~MR_UPD);
		mrp->mr_lbits += MR_ACCINC;
		dqacc = mrp->mr_lbits & MR_WAIT;
	}

	if (dqacc)
		mr_dqacc(mrp, s);
	else
		mr_unlockq(mrp, s);
	return 1;
}

#define MRLOCK_INHERITANCE_TEST
#ifdef MRLOCK_INHERITANCE_TEST

mrlock_t test_mr;
int test_mr_initialized;

void
mri_test_init(void)
{
	if (!compare_and_swap_int(&test_mr_initialized, 0, 1))
		return;
	mrlock_init(&test_mr, 0, 0, 0);
}

void
mri_test_dolock(int cstime, int type)
{
	if (type == MR_ACCESS)
		mraccess(&test_mr);
	else
		mrupdate(&test_mr);
	while (cstime-- > 0)
		DELAY(1);
	mrunlock(&test_mr);
}

#endif /* MRLOCK_INHERITANCE_TEST */


#ifdef SEMAMETER

#if 0 /* This code was leaking */

/* mr_addreader -- add the current thread to the list of reader
   structures for the lock.  Note that insertion is O(1), so we can
   have recursive grabbing of locks.  Chaining should allow that to
   happen for free. */

zone_t *reader_zone;

int mr_reader_bytes_allocated = 0;
int mr_reader_bytes_deallocated = 0;

/*REFERENCED*/
static void 
mr_addreader(k_mrmeter_t *km)
{
	k_mrreader_t *kmrr;
	int index;
	int i,s;
	static int need_reader_zone = 1;

	if (need_reader_zone)
	{
		need_reader_zone = 0;
		reader_zone = kmem_zone_init(sizeof(k_mrreader_t),"mr reader structs");
	}

	if ((kmrr = (k_mrreader_t *)kmem_zone_zalloc(reader_zone,KM_NOSLEEP)) == NULL)
	{
		cmn_err_tag(284,CE_WARN,"!mr_addreader seriously low on memory.!");
		return;
	}
	mr_reader_bytes_allocated += sizeof(k_mrreader_t);
	kmrr->mr_tid = kthread_getid();
	kmrr->mr_ts = get_timestamp();
	kmrr->the_meter = km;
	index = kmrr->mr_tid % MR_READER_BUCKETS;
	/*  cmn_err(CE_WARN,"mr_addreader: adding thread %d in index %d!",kmrr->mr_tid, index); */

	i = MRBUCKET_NUM(km->mr_base.wchan);
	s = MRMETER_LOCK(i);

	kmrr->next = km->mr_readers[index];
	km->mr_readers[index] = kmrr;

	MRMETER_UNLOCK(i,s);

}

/* mr_remreader -- remove the current thread from the reader list.
   This function returns the elapsed number of ticks that this thread
   held this lock. */

/*REFERENCED*/
static int
mr_remreader(k_mrmeter_t *km)
{
	k_mrreader_t *kmrr, *bkmrr;
	uint64_t tid = kthread_getid();
	int index = tid % MR_READER_BUCKETS;
	unsigned int elapsed;
	int i,s;

	i = MRBUCKET_NUM(km->mr_base.wchan);
	s = MRMETER_LOCK(i); /* We're probably holding this lock for too
				long, but the addreader function can't get
				stomped on, which it looks like it is.
				Anyhow, this is for debugging purposes, So
				tough noogies. */

	for (bkmrr = NULL,kmrr = km->mr_readers[index] ; 
		kmrr ; 
		bkmrr = kmrr, kmrr = kmrr->next)
	{
		if (kmrr->mr_tid == tid) break;
	}

	if (!kmrr) /* Uh oh... */
	{
		MRMETER_UNLOCK(i,s);
		cmn_err_tag(285,CE_WARN,
			    "!mr_remreader: thread %d not found (index %d)!",
			    tid,index);
		return 0;
	}

	if (!bkmrr) /* It was the first one on the list */
	{
		bkmrr = km->mr_readers[index]->next;
		elapsed = get_timestamp() - km->mr_readers[index]->mr_ts;
		(void) kmem_zone_free(reader_zone,km->mr_readers[index]);
		km->mr_readers[index] = bkmrr;
	}
	else
	{
		bkmrr->next = kmrr->next;
		elapsed = get_timestamp() - kmrr->mr_ts;
		(void) kmem_zone_free(reader_zone,kmrr);
	}
	mr_reader_bytes_deallocated += sizeof(k_mrreader_t);
	MRMETER_UNLOCK(i,s);
	return elapsed;
}

/*REFERENCED*/
static void 
mr_freereaders(k_mrmeter_t *km)
{
	k_mrreader_t *kmrr, *kmrr2;
	int index;
	int i,s;

	i = MRBUCKET_NUM(km->mr_base.wchan);

	s = MRMETER_LOCK(i);

	for (index=0;index<MR_READER_BUCKETS;index++)
	{
		for (kmrr2 = NULL,kmrr=km->mr_readers[index];
			kmrr;
			kmrr = kmrr2)
		{
			kmrr2 = kmrr->next;
			kmem_zone_free(reader_zone,kmrr);
			mr_reader_bytes_deallocated += sizeof(k_mrreader_t);
		}
		km->mr_readers[index] = NULL;
	}
	MRMETER_UNLOCK(i,s);
}
#endif /* 0 */		

/* 
 * dump all multi-reader locks
++
++      kp mrlocks [n] - dump multi-reader locks
++        n: criterion for printing locks
++ 
++        0: Print all locks
++        1: Print locks with m_elapsed > 10
++        2: Print locks with m_waittime > 10
++        4: Print locks with m_totalwait > 1000000 (for now -- Humper)
++        5: Print locks with m_totalheld > 1000000 (for now -- Humper)
++        6: Print locks currently being held and their times
++        99:Clear mrlock data
++        The operations [1-5] don't make a distinction between whether it 
++        was readers or writers who triggered the prints -- they probably
++        should, but then the interface between this and idbg_hotlk would
++        be as ridiculous as the rest of this code.
 */

#define LONG_LONG long long
/*ARGSUSED*/
static int 
idbg_mrlock_criterion(__psint_t n, k_mrmeter_t *kmp)
{
	switch(n)
	{
	case 0:
		return 1; /* Print all of them */
	case 1:
		return (
			((LONG_LONG)kmp->mr_wwaittime*timer_unit)/1000 > 1000 ||
			((LONG_LONG)kmp->mr_rwaittime*timer_unit)/1000 > 1000
		);

	case 2:
		return (
			((LONG_LONG)kmp->mr_wholdtime*timer_unit)/1000 > 1000 ||
			((LONG_LONG)kmp->mr_rholdtime*timer_unit)/1000 > 1000
		);
	case 4:
		return (
			((LONG_LONG)kmp->mr_totalwwaittime*timer_unit)/1000 > 1000000 ||
			((LONG_LONG)kmp->mr_totalrwaittime*timer_unit)/1000 > 1000000
		);
	case 5:
		return (
			((LONG_LONG)kmp->mr_totalwholdtime*timer_unit)/1000 > 1000000 ||
			((LONG_LONG)kmp->mr_totalrholdtime*timer_unit)/1000 > 1000000
		);
	case 6:
		return (mr_lock(kmp)->mr_lbits & MR_ACC);
	default:
		qprintf("Bogus value passed to idbg_mrlock_criterion: %d", n);
		return 0;
	}
}

/*
extern zone_t *reader_zone;

static void
idbg_free_mr_readers(k_mrmeter_t *kmp)
{
	k_mrreader_t *kmrr, *temp;
	int i;

	for (i = 0; i < MR_READER_BUCKETS; i++)
	{
		for (kmrr = kmp->mr_readers[i] ; kmrr ; kmrr = temp)
		{
			temp = kmrr->next;
			kmem_zone_free(reader_zone,kmrr);
		}
	}
}
*/
 
void 
idbg_mrlocks(__psint_t n)
{
	mpkqueuehead_t *bp;
	int bi;

	if (n < 0 || n == 3 || (n > 6 && n != 99)) {
		qprintf("Usage: mrlocks <n>, where <n>\n");
		qprintf(" 0 - print all mrlocks (warning: this list takes several minutes to print)\n");
		qprintf(" 1 - print all mrlocks with waittime > 1000\n");
		qprintf(" 2 - print all mrlocks with holdtime > 1000\n");
		qprintf(" 4 - print all mrlocks with holdtime > 1000000\n");
		qprintf(" 5 - print all mrlocks with holdtime > 1000000\n");
		qprintf(" 6 - print all mrlocks with accessors\n");
		return;
	}
	for (bi = 0, bp = mrinfolist; bi < SEMABUCKETS; bi++, bp++) {
		kqueue_t *lp;
		for (lp = kqueue_first(&bp->head);
		     lp != kqueue_end(&bp->head);
		     lp = kqueue_next(lp)) {
			k_mrmeter_t *kp = (k_mrmeter_t *)baseof(wchaninfo_t, list, lp);
			/*
			 * Okay, so it's kind of silly to get the meter, get
			 * the lock, and then get the meter again in
			 * idbg_mrlock, but since we would like the semaphore
			 * dump as well, the overhead is worth it.
			 */
			if (n == 99) {
				/*
				 * I apologize for this ugliness, but it just
				 * had to be done.
				 */
				/* idbg_free_mr_readers(kp); */
				bzero(&kp->mr_firststat,
				      sizeof *kp - offsetof(k_mrmeter_t, mr_firststat));
			} else if (idbg_mrlock_criterion(n, kp))
				idbg_mrlock(mr_lock(kp));
		}
	}
}
#endif /* SEMAMETER */

/*
 * Dequeue thread kt from mrlock mrp
 */
static void
mrdq(mrlock_t *mrp, kthread_t *kt)
{
	kthread_t **qp, *fkt;

	ASSERT(kt);
	ASSERT(kt->k_mrqpri != PTIME_SHARE_OVER);
	if (kt->k_mrqpri <= PBATCH_CRITICAL && kt->k_mrqpri >= PBATCH)
		qp = &(mrp->mr_waiters.pq_pq[-kt->k_mrqpri]);
	else if (kt->k_mrqpri >= 0)
		qp = &(mrp->mr_waiters.pq_pq[0]);
	else {
		ASSERT(kt->k_mrqpri <= PWEIGHTLESS);
		qp = &(mrp->mr_waiters.pq_pq[-PWEIGHTLESS]);
	}

	fkt = kt->k_flink;
	if (kt != fkt) {
		fkt->k_blink = kt->k_blink;
		kt->k_blink->k_flink = fkt;
		kt->k_flink = kt->k_blink = kt;
		if (*qp == kt)
			*qp = fkt;
	} else {
		ASSERT(kt->k_flink == kt->k_blink);
		ASSERT(*qp == kt);
		*qp = NULL;
	}
	SET_PQPRI(&mrp->mr_waiters, find_waiter_hpri(&mrp->mr_waiters));
}

/*
 * Dequeue thread kt from mrlock mrp, where kt is in sub-queue qp.
 */
INLINE static void
mrdq_qp(kthread_t *kt, kthread_t **qp)
{
	kthread_t *fkt;

	ASSERT(kt);
	ASSERT(kt->k_mrqpri != PTIME_SHARE_OVER);
	fkt = kt->k_flink;
	if (kt != fkt) {
		fkt->k_blink = kt->k_blink;
		kt->k_blink->k_flink = fkt;
		kt->k_flink = kt->k_blink = kt;
		if (*qp == kt)
			*qp = fkt;
	} else {
		ASSERT(kt->k_flink == kt->k_blink);
		ASSERT(*qp == kt);
		*qp = NULL;
	}
}

/*
 * V operation on an mrlock's semaphore
 */
static void
#ifdef SEMAMETER
mrvsema(mrlock_t *mrp, int s, k_mrmeter_t *km)
#else
mrvsema(mrlock_t *mrp, int s)
#endif
{
	kthread_t *kt;
	int	  dqacc;

	ASSERT(mr_islocked(mrp));
	ASSERT(mrp->mr_lbits & MR_UPD);
	ASSERT(!(mrp->mr_qbits & MR_V));

	/*
	 * This code is a little magic in that there can only be one
	 * outstanding V operation that doesn't get consumed by a previous P.
	 *
	 * This is because the _only_ time mrvsema is called is when
	 * the mrlock is released and there is a waiter-to-be --
	 * and mrvsema is always called with the update bit set.
	 * mrvsema then either finds the waiter already posted on the
	 * wait queue (at which time it gets de-queued), or MR_V gets
	 * set.  If this happens, the next P/V operation _must_ be
	 * the next lock-wanter calling mrpsema expecting to wait for
	 * the lock (the first one gets it in this case).
	 */
	ASSERT(mrp->mr_pcnt >= 0);
	if (mrp->mr_pcnt) {
		mrp->mr_pcnt--;
		kt = mrlock_hpri_waiter(mrp);
		mrdq(mrp, kt);
		if (kt->k_flags&KT_STARVE) {
			mrrecordholder(mrp, kt);
			if (!(kt->k_flags & KT_WUPD)) {
				mrp->mr_lbits += MR_ACCINC;
				mrp->mr_lbits &= ~MR_UPD;
			}
		} else {
			mrp->mr_lbits &= ~MR_UPD;
		}
#ifdef SEMAMETER
		if (km) {
			km->mr_nwait--;
			if (kt->k_flags & KT_WUPD) {
				km->mr_updv++;
				km->mr_updlocks++;
				(void)mrmeter_getentry(km, kt,
					MRE_PENDING|MRE_UPDATE, MRE_UPDATE);
			} else {
				km->mr_accv++;
				atomicAddUint(&km->mr_acclocks, 1);
				(void)mrmeter_getentry(km, kt,
					MRE_PENDING|MRE_ACCESS, MRE_ACCESS);
			}
		}
#endif

		/*
		 * This thread isn't waiting for update -- change mrlock to
		 * access-mode, and wake up any more access waiters.
		 * The k_flags bits were set before queueing on the mrlock,
		 * so it should be safe to peek (and act) here.
		 */
		dqacc = ( !(kt->k_flags&KT_WUPD) && mrp->mr_lbits & MR_WAIT);
		if (dqacc)
			mr_dqacc(mrp, s);
		else
			mr_unlockq(mrp, s);

		s = kt_lock(kt);
		kt->k_flags &= (~(KT_WACC|KT_WUPD|KT_STARVE));
		/*
		 * This thread could, for example, be waiting on indirectwait
		 * so w_chan does not necessarily have to point at mrp.
		 */
		if ((mrlock_t *)kt->k_wchan == mrp) 
			kt->k_wchan = 0;
		if (kt->k_flags & KT_WMRLOCK)
			thread_unblock(kt, KT_WMRLOCK);
		kt_unlock(kt, s);

		return;
	}
	
	ASSERT(!(mrp->mr_qbits & MR_V));
	mrp->mr_qbits |= MR_V;

#ifdef SEMAMETER
	if (km)
		km->mr_vnoslp++;
#endif

	mr_unlockq(mrp, s);
}

/*
 * mr_dqacc -- dequeue any accessors on the mrlock queue.
 *
 *	Called indirectly via mr_demote, which is either called by current
 *	owner of the lock while in access mode, or by unlocker, before
 *	new owner of the lock is put on the run queue.
 *
 *	Called with mr_lockq locked. Unlocks it before returning.
 */
void
mr_dqacc(mrlock_t *mrp, int s)
{
	_SEMAMETER(k_mrmeter_t *km = _mrmeter(mrp);)
	kthread_t *nkt, *qkt, **qp = &mrp->mr_waiters.pq_pq[0];
	kthread_t *accessors, **ap = &accessors;
	int nacc = 0, naccg=0;
	short updatorhpri = (-PX_PRIO_MAX)-1; /* minimum */

	ASSERT(!(mrp->mr_lbits & MR_UPD));
	ASSERT(!(mrp->mr_qbits & MR_V));

	/*
	 * Walk the waiter queues, from highest to lowest priority and in
	 * arrival order for equal priority waiters.  We dequeue all of the
	 * initial accessors and stop as soon as we find an updator, or, if
	 * the mrlock is marked to allow dequeuing accessors of priority equal
	 * to the highest priority waiting updator, till we find an accessor
	 * or updator of lower priority.  We thread all of these accessors
	 * onto a linked list in the same order as our scan and then wake them
	 * up (in that order) after we drop the mrlock data structure lock.
	 */
	for (qkt = *qp;;) {
		/*
		 * qkt points at the next thread to look at in the current
		 * subqueue or NULL if we're at the end of that subqueue.  If
		 * we're at the end, iterate through the remaining queues
		 * till we find a non-empty queue.  Once we have a legitimate
		 * ``next thread,'' copy the pointer and move qkt.
		 */
		while (qkt == NULL) {
			qp++;
			if (qp == &(mrp->mr_waiters.pq_pq[NSCHEDCLASS]))
				goto wakeacc;
			qkt = *qp;
		}
		nkt = qkt;
		qkt = qkt->k_flink;
		if (qkt == *qp)
			qkt = NULL;	/* last element of subqueue */

		if (!(nkt->k_flags & KT_WACC)) {
			/*
			 * ntk is an updator: Generally if we run into an
			 * updator we want to stop scanning immediately.  If
			 * the mrlock is marked to allow dequeuing equal
			 * priority accessors behind the highest priority
			 * updator, we allow ourselves to continue scanning in
			 * order to find these accessors (if present).  Even if
			 * we're allowing this, we bail if we've already seen
			 * an updator of higher priority since we know that any
			 * accessors further down in the list are also lower
			 * priority.
			 */
			if (!(mrp->mr_qbits & MR_ALLOW_EQUAL_PRI) ||
			    updatorhpri > nkt->k_pri)
				break;
			updatorhpri = nkt->k_pri;
			continue;
		}
		/*
		 * nkt is an accessor: We stop scanning if the highest
		 * priority updator that we've seen is higher than our own
		 * priority.
		 */
		if (updatorhpri > nkt->k_pri)
			break;

		/*
		 * We have an accessor that we're willing to wake up.  Dequeue
		 * it from the waiter queue, mark it as a holder, and add it
		 * to the end of the accessor list we're building locally.
		 */
		ASSERT(mrp->mr_lbits & MR_WAIT);
		ASSERT(mrp->mr_pcnt > 0);

		mrdq_qp(nkt, qp);
		SET_PQPRI(&mrp->mr_waiters, find_waiter_hpri(&mrp->mr_waiters));
		if (nkt->k_flags&KT_STARVE) {
			mrrecordholder(mrp, nkt);
			naccg++;
		}
		nacc++;
		*ap = nkt;
		ap = &nkt->k_flink;
#ifdef SEMAMETER
		if (km) {
			(void)mrmeter_getentry(km, nkt,
					MRE_PENDING|MRE_ACCESS, MRE_ACCESS);
			atomicAddUint(&km->mr_acclocks, 1);
			km->mr_dqacc++;
			km->mr_nwait--;
		}
#endif
	}

wakeacc:
	/*
	 * We've now got nacc accessors that we want to wake up (possibly
	 * zero).  Decrement them from the waiter count and add them to the
	 * accessor holder count.  Note that we must do this before we drop
	 * the mrlock data structure lock in order to prevent an updator from
	 * slipping in before we wake up our accessors.
	 */
	ASSERT(mrp->mr_pcnt >= nacc && (mrp->mr_lbits & MR_WAIT) >= (nacc * MR_WAITINC));
	mrp->mr_pcnt -= nacc;
	mrp->mr_lbits += (naccg * MR_ACCINC - nacc * MR_WAITINC);
	mr_unlockq(mrp, s);

	/*
	 * NULL terminate the accessor list and walk it; waking up all the
	 * accessors.
	 */
	*ap = NULL;
	while (nkt = accessors) {
		accessors = nkt->k_flink;
		nkt->k_flink = nkt;
		s = kt_lock(nkt);

		ASSERT(!(nkt->k_flags & KT_WUPD));
		nkt->k_flags &= ~(KT_WACC|KT_STARVE);
		/*
		 * This thread could, for example, be waiting on indirectwait
		 * so w_chan does not necessarily have to point at mrp.
		 */
		if ((mrlock_t *)nkt->k_wchan == mrp)
			nkt->k_wchan = 0;
		if (nkt->k_flags & KT_WMRLOCK)
			thread_unblock(nkt, KT_WMRLOCK);
		kt_unlock(nkt, s);
	}
}

#ifdef SEMAMETER

/*ARGSUSED*/
void
_mraccess(mrlock_t *mrp, inst_t *retaddr, int bhlock)
{
	k_mrmeter_t *km = _mrmeter(mrp);
	unsigned int ts;
	int elapsed;
	mr_entry_t *mre;

	if (km) {
		ts = get_timestamp();
		mre = mrmeter_entry(km, MRE_ACCESS|MRE_PENDING,
					retaddr, KM_SLEEP);
	}

#ifdef CELL
	if (bhlock) {
		ASSERT(mrp->mr_qbits&MR_BEHAVIOR);
		mr_bhaccess(mrp);
	} else { 
		ASSERT(!(mrp->mr_qbits&MR_BEHAVIOR));
		mr_access(mrp);
	}
#else
	mr_access(mrp);
#endif


	if (km) {
/*
	        mr_addreader(km);
*/
		elapsed = get_timestamp() - ts;
		if (elapsed < 0)
			elapsed += TIMER_MAX;
		
		km->mr_totalrwaittime += elapsed;
		if (km->mr_rwaittime < (unsigned) elapsed)
		{
			km->mr_rwaittime = elapsed;
			km->mr_rwaitpc = retaddr;
			km->mr_rwaitcpu = cpuid();
		}		
		/*
		 * Weird case possible here -- the clock interrupt
		 * calls calcrss/cmrlock on the address space mrlock.
		 * If it acquires and releases the lock between the time
		 * we mrmeter_entry above and the time we get here,
		 * the mrunlock call from calcrss could free up the wrong
		 * log entry -- this mre in particular (yes, this is a bug).
		 * We'll just have to search for a matching entry.
		 * If there's a pending access that matches this thread,
		 * claim it (this thread can only have one pending lock
		 * call); otherwise take a plain access call.
		 */
		mre = mrmeter_getentry(km, private.p_curkthread,
					MRE_PENDING|MRE_ACCESS, MRE_ACCESS);
		/*
		 * If mre_use was MRE_PENDING, got lock without sleeping,
		 * and access count wasn't incremented for the caller.
		 */
		if (mre) {
			atomicAddUint(&km->mr_acclocks, 1);
		}
	}

	ASSERT(mrp->mr_lbits & MR_ACC);
	ASSERT(!(mrp->mr_lbits & MR_UPD));
}

/* ARGSUSED */
void
mraccessf(mrlock_t *mrp, int x)
{
	_mraccess(mrp, (inst_t *)__return_address, 0);
}

/* ARGSUSED */
void
mrupdatef(mrlock_t *mrp, int x)
{
	_mrupdate(mrp, (inst_t *)__return_address);
}

#ifdef CELL
void
mrbhaccess(mrlock_t *mrp)
{
	_mraccess(mrp, (inst_t *)__return_address, 1);
}
#endif /* CELL */

void
_mrupdate(mrlock_t *mrp, inst_t *retaddr)
{
	k_mrmeter_t *km = _mrmeter(mrp);
	register unsigned int ts;
	int elapsed;
	mr_entry_t *mre;

	if (km) {
		mre = mrmeter_entry(km, MRE_UPDATE|MRE_PENDING,
				retaddr, KM_SLEEP);
		ts = get_timestamp();
	}

	mr_update(mrp);
	ASSERT((mrp->mr_lbits & (MR_UPD|MR_ACC)) == MR_UPD);

	if (km) {
		km->mr_wts = get_timestamp();
		elapsed = km->mr_wts - ts;
		if (elapsed < 0)
			elapsed += TIMER_MAX;

		km->mr_totalwwaittime += elapsed;
		if (km->mr_wwaittime < (unsigned) elapsed)
		{
			km->mr_wwaittime = elapsed;
			km->mr_wwaitpc = retaddr;
			km->mr_wwaitcpu = cpuid();
		}
		ASSERT(mre->mre_use & MRE_UPDATE);

		/*
		 * If mre_use is MRE_PENDING, got lock without sleeping,
		 * and update count wasn't incremented for the caller.
		 */
		if (mre->mre_use & MRE_PENDING) {
			mre->mre_use = MRE_UPDATE;
			km->mr_updlocks++;
		}
	}
}

int
mrtryaccess(mrlock_t *mrp)
{
	k_mrmeter_t *km = _mrmeter(mrp);

	if (mr_tryaccess(mrp)) {
		if (km) {
			/* mr_addreader(km); */
			if (!(mrmeter_entry(km, MRE_ACCESS,
					(inst_t *)__return_address,
					KM_NOSLEEP))) {
				mr_accunlock(mrp);
				return 0;
			}
			atomicAddUint(&km->mr_acclocks, 1);
		}
		return 1;
	}

	return 0;
}

int
mrtryupdate(mrlock_t *mrp)
{
	k_mrmeter_t *km = _mrmeter(mrp);

	if (mr_tryupdate(mrp)) {
		if (km) {
			if (!(mrmeter_entry(km, MRE_UPDATE,
					(inst_t *)__return_address,
					KM_NOSLEEP))) {
				mr_unlock(mrp);
				return 0;
			}
			km->mr_wts = get_timestamp();
			km->mr_updlocks++;
		}
		return 1;
	}

	return 0;
}

void
mrunlock(mrlock_t *mrp)
{
	k_mrmeter_t *km = _mrmeter(mrp);
	int elapsed;

	if (km) {
		if (mrp->mr_lbits & MR_UPD) {
			elapsed = get_timestamp() - km->mr_wts;
			km->mr_totalwholdtime += elapsed;
			if (km->mr_wholdtime < elapsed)
			{
				km->mr_wholdtime = elapsed;
				km->mr_wholdpc = (inst_t *)__return_address;
				km->mr_wholdcpu = cpuid();
			}
			ASSERT((mrp->mr_lbits & (MR_UPD|MR_ACC)) == MR_UPD);
			(void)mrmeter_getentry(km, private.p_curkthread,
					MRE_UPDATE, 0);

			km->mr_updunlocks++;
		} else {
			mr_entry_t *mre = mrmeter_getentry(km,private.p_curkthread,MRE_ACCESS,MRE_ACCESS);
/*
			elapsed = mr_remreader(km);
*/
			elapsed = get_timestamp() - mre->mr_ts;
			km->mr_totalrholdtime += elapsed;
			if (km->mr_rholdtime < elapsed)
			{
				km->mr_rholdtime = elapsed;
				km->mr_rholdpc = (inst_t *)__return_address;
				km->mr_rholdcpu = cpuid();
			}

			mraccunlock(mrp);
			return;
		}
	} else {
		ASSERT(mrp->mr_lbits & (MR_UPD|MR_ACC));
		ASSERT(!(mrp->mr_lbits & MR_UPD) || !(mrp->mr_lbits & MR_ACC));
	}

	ASSERT((!(mrp->mr_qbits&MR_BEHAVIOR)) || (mrp->mr_lbits & MR_UPD));
	mr_unlock(mrp);
}

void
mraccunlock(mrlock_t *mrp)
{
	k_mrmeter_t *km = _mrmeter(mrp);

	ASSERT(mrp->mr_lbits & MR_ACC);
	ASSERT(!(mrp->mr_lbits & MR_UPD));
#ifdef CELL
	ASSERT(!(mrp->mr_qbits&MR_BEHAVIOR));
#endif

	if (km) {
		(void)mrmeter_getentry(km, private.p_curkthread, MRE_ACCESS, 0);
		atomicAddUint(&km->mr_accunlocks, 1);
	}

	mr_accunlock(mrp);
}

#ifdef CELL
void
mrbhaccunlock(mrlock_t *mrp)
{
	k_mrmeter_t *km = _mrmeter(mrp);

	ASSERT(mrp->mr_lbits & MR_ACC);
	ASSERT(!(mrp->mr_lbits & MR_UPD));
	ASSERT(mrp->mr_qbits&MR_BEHAVIOR);

	if (km) {
		(void)mrmeter_getentry(km, private.p_curkthread, MRE_ACCESS, 0);
		atomicAddUint(&km->mr_accunlocks, 1);
	}

	mr_bhaccunlock(mrp);
}
#endif /* CELL */

/*
 * Try to promote an mrlock to update.
 */
int
mrtrypromote(mrlock_t *mrp)
{
	ASSERT(mrp->mr_lbits & MR_ACC);
	if (mr_trypromote(mrp)) {
		k_mrmeter_t *km = _mrmeter(mrp);
		if (km) {
			mr_entry_t *mre = mrmeter_getentry(km,
							private.p_curkthread,
							MRE_ACCESS, MRE_UPDATE);
			ASSERT(mre);
			mre->mre_pc = (inst_t *)__return_address;

			km->mr_updlocks++;
			km->mr_accunlocks++;
		}
		return 1;
	}
	return 0;
}

/* 
 * mrdemote - demote from UPDATE to ACCESS
 *	This simply allows other accessers in - no other updaters
 */
void
mrdemote(mrlock_t *mrp)
{
	k_mrmeter_t *km = _mrmeter(mrp);

	ASSERT(mrp->mr_lbits & MR_UPD);

	if (km)
		km->mr_updunlocks++;

	mr_demote(mrp);

	if (km) {
		mr_entry_t *mre = mrmeter_getentry(km,
						private.p_curkthread,
						MRE_UPDATE, MRE_ACCESS);
		mre->mre_pc = (inst_t *)__return_address;
		atomicAddUint(&km->mr_acclocks, 1);
	}
}

#ifdef CELL
/* 
 * mrbhdemote - demote from UPDATE to ACCESS
 *	This simply allows other accessers in - no other updaters
 */
void
mrbhdemote(mrlock_t *mrp)
{
	k_mrmeter_t *km = _mrmeter(mrp);

	ASSERT(mrp->mr_lbits & MR_UPD);

	if (km)
		km->mr_updunlocks++;

	mr_bhdemote(mrp);

	if (km) {
		mr_entry_t *mre = mrmeter_getentry(km,
						private.p_curkthread,
						MRE_UPDATE, MRE_ACCESS);
		mre->mre_pc = (inst_t *)__return_address;
		atomicAddUint(&km->mr_acclocks, 1);
	}
}
#endif /* CELL */

/*
 * Metering routines for sleepable, multi-access/single-update locks.
 */

/*
 * mrmeteron - turn on history/metering for an mr-lock
 */
void
mrmeteron(mrlock_t *mrp, char *name, long sequence)
{
	k_mrmeter_t *kp = (k_mrmeter_t *)
		allocwchaninfo(mrp, name, sequence,
			       mrinfolist, sizeof (k_mrmeter_t),
			       &mrinfo_zone, "mrmeters");
	if (kp) {
		mr_log_t *logp;
		int i;

		/* initialize current-user log array */
		logp = &kp->mr_log;
		for (i = 0; i < MRE_ENTRIES; i++)
			logp->mrl_entries[i].mre_index = MRE_ENTRIES; /*magic*/
	}
}

/*
 * mrmeteroff - turn off metering for an mr-lock
 */
/*ARGSUSED*/
void
mrmeteroff(mrlock_t *mrp)
{
	k_mrmeter_t *kp = _mrmeter(mrp);
	mr_log_t *logp, *logt;

	if (!kp)
		return;

	_SEMAINFOP(mrp->mr_info = NULL;)
	mpkqueue_remove(&mrinfolist[SEMABUCKET(mrp)], &kp->mr_base.list);

	logp = &kp->mr_log;
	while (logt = logp->mrl_next) {
		logp->mrl_next = logt->mrl_next;
		kmem_zone_free(mr_log_zone, logt);
	}
	/* mr_freereaders(kp); */
	kmem_zone_free(mrinfo_zone, kp);
}

static mr_entry_t *
mrmeter_entry(k_mrmeter_t *km,
		short type,
		inst_t *pc,
		int vmflags)
{
	mr_log_t *logp, *logt = NULL;
	mr_entry_t *mre;
	int i, s;
retry:
	s = mutex_spinlock(&km->mr_loglock);
	for (logp = &km->mr_log; ; ) {
		for (i = 0, mre = &logp->mrl_entries[0];
		     i < MRE_ENTRIES;
		     i++, mre++) {
			if (mre->mre_use == 0) {
				mre->mre_use = type;
				mre->mre_pc = pc;
				mre->mr_ts = get_timestamp();
				mre->mre_kid = private.p_curkthread ?
					private.p_curkthread->k_id : 0;
				mutex_spinunlock(&km->mr_loglock, s);
				if (logt)
					kmem_zone_free(mr_log_zone, logt);
				return mre;
			}
		}

		if (!(logp->mrl_next)) {
			if (logt) {
				logp->mrl_next = logt;
				logt = NULL;
			} else
				break;
		}
		logp = logp->mrl_next;
	}

	mutex_spinunlock(&km->mr_loglock, s);
	ASSERT(!logt);
	if ((logt = kmem_zone_zalloc(mr_log_zone, vmflags)) == NULL)
		return NULL;
	for (i = 0; i < MRE_ENTRIES; i++)
		logt->mrl_entries[i].mre_index = i;
	goto retry;
}

static void
mrmeter_purge(k_mrmeter_t *km, mr_log_t *logp, mr_entry_t *mre, int s)
{
	mr_log_t *logt;
	int i;

	/*
	 * Think about tossing the log structure if the freed log
	 * isn't embedded in the meter struct.
	 */
	if (mre->mre_index != MRE_ENTRIES) {
		mre -= mre->mre_index;
		ASSERT(mre->mre_index == 0);

		for (i = 0; i < MRE_ENTRIES; i++, mre++) {
			if (mre->mre_use) {
				mutex_spinunlock(&km->mr_loglock, s);
				return;
			}
		}

		for (logt = &km->mr_log; logt; logt = logt->mrl_next) {
			if (logt->mrl_next == logp) {
				logt->mrl_next = logp->mrl_next;
				mutex_spinunlock(&km->mr_loglock, s);
				kmem_zone_free(mr_log_zone, logp);
				return;
			}
		}
	}
	mutex_spinunlock(&km->mr_loglock, s);
}

static mr_entry_t *
mrmeter_getentry(k_mrmeter_t *km, kthread_t *kt, short from, short to)
{
	mr_log_t *logp;
	mr_entry_t *mre;
	uint64_t id = kt ? kt->k_id : 0;
	int i, s;

	/*
	 * Return a pointer to an entry that matches 'from' state --
	 * change state to 'to'.
	 */
	s = mutex_spinlock(&km->mr_loglock);
	for (logp = &km->mr_log; logp; logp = logp->mrl_next) {
		for (i = 0, mre = &logp->mrl_entries[0];
		     i < MRE_ENTRIES;
		     i++, mre++) {
			if (mre->mre_use == from && mre->mre_kid == id) {
				mre->mre_use = to;
				if (to == 0) {
					mrmeter_purge(km, logp, mre, s);
					return mre;
				}
				goto out;
			}
		}
	}
out:
	mutex_spinunlock(&km->mr_loglock, s);

	ASSERT(mre || (from == (MRE_PENDING|MRE_ACCESS)));
	return mre;
}

#else /* not SEMAMETER */

/*ARGSUSED*/
void
mrmeteroff(register mrlock_t *mrp)
{
}

/*ARGSUSED*/
void
mrmeteron(register mrlock_t *mrp, char *name, long sequence)
{
}

/*ARGSUSED*/
k_mrmeter_t *
idbg_mrmeter(mrlock_t *mrp)
{
	return NULL;
}

#endif /* SEMAMETER */


#ifndef SEMASTAT
/*
 * Define stubs for lstat functions here if SEMASTAT is not enabled.
 * The real versions of these functions are in klstat.c but the file
 * is not loaded if SEMASTAT not defined.
 */

/* ARGSUSED */
int
lstat_user_command(
	int	fc,
	void	*arg)
{
	return ENOTSUP;
}
#endif /* !SEMASTAT */

/*
 * ismrlocked - check if lock is locked - for debugging only
 * Returns: 1 if is locked as requested
 *	    0 otherwise
 */
int
ismrlocked(register mrlock_t *mrp, int type)
{
	if (type == MR_ACCESS)
		return(mrp->mr_lbits & MR_ACC);
	else if (type == MR_UPDATE)
		return(mrp->mr_lbits & MR_UPD);
	else if (type == (MR_UPDATE|MR_ACCESS))
		return(mrp->mr_lbits & (MR_UPD|MR_ACC));
	else
		return(mrp->mr_lbits & MR_WAIT);
}

int
mrislocked_access(mrlock_t *mrp)
{
	return(mrp->mr_lbits & MR_ACC);
}

int
mrislocked_update(mrlock_t *mrp)
{
	return(mrp->mr_lbits & MR_UPD);
}

int
mrislocked_any(mrlock_t *mrp)
{
	return(mrp->mr_lbits & (MR_UPD|MR_ACC));
}

int
mrgetnwaiters(mrlock_t *mrp)
{
	return((mrp->mr_lbits & MR_WAIT) >> MR_WAITSHFT);
}

void
msinit(
	mslock_t	*msp,
	char		*name)
{
	sv_init(&msp->ms_sv, SV_DEFAULT, name);
	msp->ms_mode = 0;
	msp->ms_cnt = 0;
}

void
msdestroy(
	mslock_t	*msp)
{
	sv_destroy(&msp->ms_sv);
}

void
msaccess(
	mslock_t	*msp,
	mutex_t		*mutex)
{
	ASSERT(mutex_mine(mutex));

	do {
		if (msp->ms_mode == MS_FREE) {
			msp->ms_mode |= MS_ACC;
			break;
		}

		if (msp->ms_mode & MS_ACC)
			break;

		msp->ms_mode |= MS_WAITERS;
		sv_wait(&msp->ms_sv, PZERO, mutex, 0);
		mutex_lock(mutex, PZERO);
	} while (1);

	msp->ms_cnt++;
}

void
msupdate(
	mslock_t	*msp,
	mutex_t		*mutex)
{
	ASSERT(mutex_mine(mutex));

	do {
		if (msp->ms_mode == MS_FREE) {
			msp->ms_mode |= MS_UPD;
			break;
		}

		if (msp->ms_mode & MS_UPD)
			break;

		msp->ms_mode |= MS_WAITERS;
		sv_wait(&msp->ms_sv, PZERO, mutex, 0);
		mutex_lock(mutex, PZERO);
	} while (1);

	msp->ms_cnt++;
}

void
msunlock(
	mslock_t	*msp)
{
	msp->ms_cnt--;
	if (msp->ms_cnt == 0) {
		if (msp->ms_mode & MS_WAITERS)
			sv_broadcast(&msp->ms_sv);
		msp->ms_mode = MS_FREE;
	}
}

int
is_msupdate(
	mslock_t	*msp)
{
	return(msp->ms_mode & MS_UPD);
}

int
is_msaccess(
	mslock_t	*msp)
{
	return(msp->ms_mode & MS_ACC);
}


#undef mraccess
#undef mrupdate

#ifdef SEMAMETER
void mraccess(mrlock_t *mrp) 		{mraccessf(mrp, 0);}
void mrupdate(mrlock_t *mrp) 		{mrupdatef(mrp, 0);}
#define mraccess 	mr_access
#define mrupdate 	mr_update
#define mrunlock 	mr_unlock
#define mraccunlock 	mr_accunlock
#define mrtryaccess 	mr_tryaccess
#define mrtryupdate 	mr_tryupdate
#define mrdemote 	mr_demote
#define mrtrypromote 	mr_trypromote
#define mraccessf	mr_accessf
#define mrupdatef	mr_updatef
#else

/* Forward declarations */
void mraccess(mrlock_t *);
void mrupdate(mrlock_t *);

void mr_access(mrlock_t *mrp) 		{mraccess(mrp);}
void mr_update(mrlock_t *mrp) 		{mrupdate(mrp);}
void mr_unlock(mrlock_t *mrp) 		{mrunlock(mrp);}
void mr_accunlock(mrlock_t *mrp) 	{mraccunlock(mrp);}
int mr_tryaccess(mrlock_t *mrp) 	{return(mrtryaccess(mrp));}
int mr_tryupdate(mrlock_t *mrp) 	{return(mrtryupdate(mrp));}
int mr_trypromote(mrlock_t *mrp) 	{return(mrtrypromote(mrp));}
void mr_demote(mrlock_t *mrp) 		{mrdemote(mrp);}
#endif

void
mraccessf(mrlock_t *mrp, int flags)
{
	int s, starve=0;
	kthread_t *kt;
	_SEMAMETER(k_mrmeter_t *km = _mrmeter(mrp);)
#ifdef SEMASTAT
	uint64_t	rv, start, stime=0;
#endif

again:
	s = mr_lockq(mrp);
	if (mrp->mr_lbits & (MR_UPD|MR_ACCMAX)) {
		#pragma mips_frequency_hint NEVER
		mrp->mr_lbits += MR_WAITINC;
		flags = (flags & PLTWAIT) ? KT_LTWAIT|KT_WACC : KT_WACC;
		if (++starve > mrlock_starve_threshold)
			flags |= KT_STARVE;
#ifdef SEMAMETER
		if(!mrpsema(mrp, s, flags, km)) {
			#pragma mips_frequency_hint NEVER
			goto again;
		}
#elif SEMASTAT
		start = get_timestamp();
		rv = mrpsema(mrp, s, flags, __return_address);
		stime += get_timestamp()-start;
		if (!rv) {
			#pragma mips_frequency_hint NEVER
			goto again;
		}
		LSTAT_MR_UPDATE_TIME(mrp, LSTAT_ACT_SLEPT, stime);
#else
		if(!mrpsema(mrp, s, flags)) {
			#pragma mips_frequency_hint NEVER
			goto again;
		}
#endif
		/* mpsema unlocks mrp */
		ASSERT(mrislocked_access(mrp));
		return;
	}
	mrp->mr_lbits += MR_ACCINC;
	if (kt = private.p_curkthread) {
		#pragma mips_frequency_hint FREQUENT
		mrrecordholder(mrp, kt);
	}
	mr_unlockq(mrp,s);

	LSTAT_MR_COND_UPDATE_TIME(mrp, stime);
}

void
mrupdatef(mrlock_t *mrp, int flags)
{
	int s, starve=0;
	kthread_t *kt;
	_SEMAMETER(k_mrmeter_t *km = _mrmeter(mrp);)
#ifdef SEMASTAT
	uint64_t	rv, start, stime=0;
#endif

again:
	s = mr_lockq(mrp);
	if (mrp->mr_lbits & (MR_UPD|MR_ACC)) {
		#pragma mips_frequency_hint NEVER
		mrp->mr_lbits += MR_WAITINC;
		flags = (flags & PLTWAIT) ? KT_LTWAIT|KT_WUPD : KT_WUPD;
		if (++starve > mrlock_starve_threshold)
			flags |= KT_STARVE;
#ifdef SEMAMETER
		if(!mrpsema(mrp, s, flags, km)) {
			#pragma mips_frequency_hint NEVER
			goto again;
		}
#elif SEMASTAT
		start = get_timestamp();
		rv = mrpsema(mrp, s, flags, __return_address);
		stime += get_timestamp()-start;
		if (!rv) {
			#pragma mips_frequency_hint NEVER
			goto again;
		}
		LSTAT_MR_UPDATE_TIME(mrp, LSTAT_ACT_SLEPT, stime);
#else
		if(!mrpsema(mrp, s, flags)) {
			#pragma mips_frequency_hint NEVER
			goto again;
		}
#endif
		/* mpsema unlocks mrp */
		ASSERT(mrislocked_update(mrp));
		return;
	}
	mrp->mr_lbits |= MR_UPD;
	if (kt = private.p_curkthread) {
		#pragma mips_frequency_hint FREQUENT
		mrrecordholder(mrp, kt);
	}
	mr_unlockq(mrp,s);
	LSTAT_MR_COND_UPDATE_TIME(mrp, stime);
}

void
mraccess(mrlock_t *mrp)
{
	mraccessf(mrp, 0);
}

void
mrupdate(mrlock_t *mrp)
{
	mrupdatef(mrp, 0);
}

void
mrunlock(mrlock_t *mrp)
{
	int s;
	kthread_t *kt;
	_SEMAMETER(k_mrmeter_t *km = _mrmeter(mrp);)

	s = mr_lockq(mrp);
	if (kt = private.p_curkthread) {
		#pragma mips_frequency_hint FREQUENT
		mrremoveholder(mrp, kt);
	}
	if (mrp->mr_lbits & MR_ACC) {
		#pragma mips_frequency_hint FREQUENT
		mrp->mr_lbits -= MR_ACCINC;
	} 
	if ((mrp->mr_lbits & MR_WAIT) && !(mrp->mr_lbits & MR_ACC)) {
		#pragma mips_frequency_hint NEVER
		mrp->mr_lbits |= MR_UPD;
		mrp->mr_lbits -= MR_WAITINC;
#ifdef SEMAMETER
		mrvsema(mrp, s, km);
#else
		mrvsema(mrp, s);
#endif
		/* mrvsema unlocks mrp */
		return;
	}
	mrp->mr_lbits &= (~MR_UPD);
	mr_unlockq(mrp,s);

	/* The sync object has been unlocked at this point, making it possible
	 * for another thread to free the sync object's memory.  Referencing
	 * the sync object after this point is not guaranteed safe.
	 */
}

void
mraccunlock(mrlock_t *mrp)
{
	int s;
	kthread_t *kt;
	_SEMAMETER(k_mrmeter_t *km = _mrmeter(mrp);)

	s = mr_lockq(mrp);
	if (kt = private.p_curkthread) {
		#pragma mips_frequency_hint FREQUENT
		mrremoveholder(mrp, kt);
	}
	ASSERT(mrp->mr_lbits & MR_ACC);
	mrp->mr_lbits -= MR_ACCINC;
	if ((mrp->mr_lbits & MR_WAIT) && !(mrp->mr_lbits & MR_ACC)) {
		#pragma mips_frequency_hint NEVER
		mrp->mr_lbits |= MR_UPD;
		mrp->mr_lbits -= MR_WAITINC;
#ifdef SEMAMETER
		mrvsema(mrp, s, km);
#else
		mrvsema(mrp, s);
#endif
		/* mrvsema unlocks mrp */
		return;
	}
	mr_unlockq(mrp,s);

	/* The sync object has been unlocked at this point, making it possible
	 * for another thread to free the sync object's memory.  Referencing
	 * the sync object after this point is not guaranteed safe.
	 */
}

int
mrtryaccess(mrlock_t *mrp)
{
	int s;
	kthread_t *kt;

	s = mr_trylockq(mrp);
	if (s == 0)
		return 0;

	if (mrp->mr_lbits & (MR_UPD|MR_ACCMAX|MR_WAIT)) {
		mr_unlockq(mrp,s);
		LSTAT_MR_UPDATE(mrp, LSTAT_ACT_REJECT);
		return 0;
	}
	mrp->mr_lbits += MR_ACCINC;
	if (kt = private.p_curkthread) {
		#pragma mips_frequency_hint FREQUENT
		mrrecordholder(mrp, kt);
	}
	mr_unlockq(mrp,s);
	LSTAT_MR_UPDATE(mrp, LSTAT_ACT_NO_WAIT);
	return 1;
}

int
mrtryupdate(mrlock_t *mrp)
{
	int s;
	kthread_t *kt;

	s = mr_trylockq(mrp);
	if (s == 0)
		return 0;

	if (mrp->mr_lbits & (MR_UPD|MR_ACC|MR_WAIT)) {
		mr_unlockq(mrp,s);
		LSTAT_MR_UPDATE(mrp, LSTAT_ACT_REJECT);
		return 0;
	}
	mrp->mr_lbits |= MR_UPD;
	if (kt = private.p_curkthread) {
		#pragma mips_frequency_hint FREQUENT
		mrrecordholder(mrp, kt);
	}
	mr_unlockq(mrp,s);
	LSTAT_MR_UPDATE(mrp, LSTAT_ACT_NO_WAIT);
	return 1;
}

void
mrdemote(mrlock_t *mrp)
{
	int s;

	s = mr_lockq(mrp);
	ASSERT(mrp->mr_lbits & MR_UPD);
	mrp->mr_lbits &= (~MR_UPD);
	mrp->mr_lbits += MR_ACCINC;
	if (mrp->mr_lbits & MR_WAIT) {
		mr_dqacc(mrp, s);
		return;
	}
	mr_unlockq(mrp,s);
}

int
mrtrypromote(mrlock_t *mrp)
{
	int s;

	s = mr_trylockq(mrp);
	if (s == 0)
		return 0;

	ASSERT(!(mrp->mr_lbits & MR_UPD));
	if ((mrp->mr_lbits & MR_ACC) == MR_ACCINC) {
		mrp->mr_lbits -= MR_ACCINC;
		mrp->mr_lbits |= MR_UPD;
		mr_unlockq(mrp,s);
		LSTAT_MR_UPDATE(mrp, LSTAT_ACT_NO_WAIT);
		return 1;
	}
	/* Not the only accessor, can't upgrade */
	mr_unlockq(mrp,s);
	LSTAT_MR_UPDATE(mrp, LSTAT_ACT_REJECT);
	return 0;
}

#define SPLNULL	-1

int      
mrspinaccess(mrlock_t *mrp)
{
	int s, starve=0, flags;
	_SEMAMETER(k_mrmeter_t *km = _mrmeter(mrp);)
#ifdef SEMASTAT
	uint64_t	rv, start, stime=0;
#endif

again:
	s = mr_lockq(mrp);
	ASSERT(s != SPLNULL);
	if (mrp->mr_lbits & (MR_UPD|MR_ACCMAX)) {
		#pragma mips_frequency_hint NEVER
		mrp->mr_lbits += MR_WAITINC;
		flags = KT_WACC;
		if (++starve > mrlock_starve_threshold)
			flags |= KT_STARVE;
#ifdef SEMAMETER
		if(!mrpsema(mrp, s, flags, km)) {
			#pragma mips_frequency_hint NEVER
			goto again;
		}
#elif SEMASTAT
		start = get_timestamp();
		rv = mrpsema(mrp, s, flags, __return_address);
		stime += get_timestamp()-start;
		if (!rv) {
			#pragma mips_frequency_hint NEVER
			goto again;
		}
		LSTAT_MR_UPDATE_TIME(mrp, LSTAT_ACT_SLEPT, stime);
#else
		if(!mrpsema(mrp, s, flags)) {
			#pragma mips_frequency_hint NEVER
			goto again;
		}
#endif
		/* mpsema unlocks mrp */
		ASSERT(mrislocked_access(mrp));
		return SPLNULL;
	}

	mrp->mr_lbits += MR_ACCINC;
	LSTAT_MR_COND_UPDATE_TIME(mrp, stime);
	return s;
}

int
mrspinupdate(mrlock_t *mrp)
{
	int s, starve=0, flags;
	_SEMAMETER(k_mrmeter_t *km = _mrmeter(mrp);)
#ifdef SEMASTAT
	uint64_t	rv, start, stime=0;
#endif

again:
	s = mr_lockq(mrp);
	ASSERT(s != SPLNULL);
	if (mrp->mr_lbits & (MR_UPD|MR_ACC)) {
		#pragma mips_frequency_hint NEVER
		mrp->mr_lbits += MR_WAITINC;
		flags = KT_WUPD;
		if (++starve > mrlock_starve_threshold)
			flags |= KT_STARVE;
#ifdef SEMAMETER
		if(!mrpsema(mrp, s, flags, km)) {
			#pragma mips_frequency_hint NEVER
			goto again;
		}
#elif SEMASTAT
		start = get_timestamp();
		rv = mrpsema(mrp, s, flags, __return_address);
		stime += get_timestamp()-start;
		if (!rv) {
			#pragma mips_frequency_hint NEVER
			goto again;
		}
		LSTAT_MR_UPDATE_TIME(mrp, LSTAT_ACT_SLEPT, stime);
#else
		if(!mrpsema(mrp, s, flags)) {
			#pragma mips_frequency_hint NEVER
			goto again;
		}
#endif
		/* mpsema unlocks mrp */
		ASSERT(mrislocked_update(mrp));
		return SPLNULL;
	}
	mrp->mr_lbits |= MR_UPD;
	LSTAT_MR_COND_UPDATE_TIME(mrp, stime);
	return s;
}

void
mrspinunlock(mrlock_t *mrp, int s)
{
	_SEMAMETER(k_mrmeter_t *km = _mrmeter(mrp);)
	kthread_t *kt;

	if (s == SPLNULL) {
		s = mr_lockq(mrp);
		if (kt = private.p_curkthread) {
			#pragma mips_frequency_hint FREQUENT
			mrremoveholder(mrp, kt);
		}
	} 

	ASSERT(mr_islocked(mrp));
	if (mrp->mr_lbits & MR_ACC) {
		#pragma mips_frequency_hint FREQUENT
		mrp->mr_lbits -= MR_ACCINC;
	}
	if ((mrp->mr_lbits & MR_WAIT) && !(mrp->mr_lbits & MR_ACC)) {
		#pragma mips_frequency_hint NEVER
		mrp->mr_lbits |= MR_UPD;
		mrp->mr_lbits -= MR_WAITINC;
#ifdef SEMAMETER
		mrvsema(mrp, s, km);
#else
		mrvsema(mrp, s);
#endif
		/* mrvsema unlocks mrp */
		return;
	}
	mrp->mr_lbits &= (~MR_UPD);
	mr_unlockq(mrp,s);
}


void
mrspinaccess_upgrade(mrlock_t *mrp, int s)
{
	kthread_t *kt;

	if (s == SPLNULL)
		return;

	ASSERT(mrp->mr_lbits & MR_ACC);
	if (kt = private.p_curkthread) {
		#pragma mips_frequency_hint FREQUENT
		mrrecordholder(mrp, kt);
	}
	mr_unlockq(mrp,s);
}

void
mrspinupdate_upgrade(mrlock_t *mrp, int s)
{
	kthread_t *kt;

	if (s == SPLNULL)
		return;

	ASSERT(mrp->mr_lbits&MR_UPD);
	if (kt = private.p_curkthread) {
		#pragma mips_frequency_hint FREQUENT
		mrrecordholder(mrp, kt);
	}
	mr_unlockq(mrp,s);
}

int
mrtryspinaccess(mrlock_t *mrp, int *rv)
{
	int s;

	s = mr_trylockq(mrp);
	if (s == 0)
		return 0;

	if (mrp->mr_lbits & (MR_UPD|MR_ACCMAX)) {
		mr_unlockq(mrp,s);
		LSTAT_MR_UPDATE(mrp, LSTAT_ACT_REJECT);
		return 0;
	}
	*rv = s;
	mrp->mr_lbits += MR_ACCINC;
	LSTAT_MR_UPDATE(mrp, LSTAT_ACT_NO_WAIT);
	return 1;
}

int
mrtryspinupdate(mrlock_t *mrp, int *rv)
{
	int s;

	s = mr_trylockq(mrp);
	if (s == 0)
		return 0;

	if (mrp->mr_lbits & (MR_UPD|MR_ACC)) {
		mr_unlockq(mrp,s);
		LSTAT_MR_UPDATE(mrp, LSTAT_ACT_REJECT);
		return 0;
	}
	mrp->mr_lbits |= MR_UPD;
	*rv = s;
	LSTAT_MR_UPDATE(mrp, LSTAT_ACT_NO_WAIT);
	return 1;
}

