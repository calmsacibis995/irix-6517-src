/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1993 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef __SYS_SEMA_PRIVATE_H__
#define __SYS_SEMA_PRIVATE_H__

#ident	"$Revision: 1.62 $"

#include <sys/sema.h>
#include <sys/kthread.h>
#include <ksys/kqueue.h>
#include <sys/cmn_err.h>
#include <sys/kmem.h>
#include <sys/debug.h>

/*
 * The kernel sleeping synchronization object package, ksync, can be
 * compiled with a number of cpp defines to provide information about the
 * ksync objects (often referred to as wait channels or "wchan") for
 * performance analysis.  This can include extensive metering information or
 * simply the names of the ksync objects which can then be reported by ps,
 * par, and various idbg commands.
 *
 * Three different versions of the ksync code are provided: ksync.a,
 * ksync_metered.a, and ksync_named.a.  ksync.a provides only
 * synchronization services with no ancilary information.  This is the
 * default production version of the code; the other versions should only be
 * used when doing performance analysis.  ksync_metered instruments all
 * ksync objects with metering information.  This information can be very
 * valuable but introduces a fairly heavy performance load itself which may
 * obscur the performance problem being analyzed ...  ksync_named.a
 * instruments all ksync objects with a name which can be reported by ps,
 * par, and vaious idbg commands.  This introduces an extra load when
 * initializing and destroying ksync objects but otherwise doesn't affect
 * the performance of using them.
 *
 * Finally, if the cpp define SEMAINFOP is defined when the kernel is
 * compiled, a (void *) ``information'' pointer field is added as the first
 * field in every ksync object.  This field is used to point at the
 * ancillary data for each ksync object and can significantly decrease the
 * performance impact of accessing that data (without SEMAINFOP, the data
 * is stored and looked up via a hash table).  With this define, even
 * ksync.a can provide the base name for a ksync object in an incredibly
 * lightweight manner (the info pointer is used to point at the literal
 * string that was passed to the ksync object initialization routine).  The
 * only down sides to using this are the need to recompile the kernel and
 * the fairly minor storage overhead of adding a pointer to every ksync
 * object.  Thus, if you want to introduce as little performance
 * perturbation as possible when trying to trace down a performance problem
 * using ksync.a with SEMAINFOP defined is the way to go.  */

/*
 * Simple defines to make to eliminate need for #ifdef's with simple
 * statements, cover some common combinations, and disallow others.  */
#ifdef SEMAMETER
#define _SEMAMETER(x)	x
#else
#define _SEMAMETER(x)
#endif

#ifdef SEMAINFOP
#define _SEMAINFOP(x)	x
#else
#define _SEMAINFOP(x)
#endif

#ifdef SEMA_KHIST
#define _SEMA_KHIST(x)	x
#else
#define _SEMA_KHIST(x)
#endif

#if defined(SEMAMETER) || defined(SEMANAME)
#define SEMAINFO
#define _SEMAINFO(x)	x
#else
#define _SEMAINFO(x)
#endif

#if defined(SEMAMETER) && defined(SEMANAME)
#error can't have both SEMAMETER and SEMANAME defined at the same time
#endif


/*
 * Private definitions for mutexes.
 */
#define MUTEX_BITS	0x3
#define MUTEX_LOCKBIT	0x1		/* mutex bitlock */
#define MUTEX_QUEUEBIT	0x2		/* waiter(s) queued */
#define MUTEX_STRIP(M)	((M) & ~MUTEX_BITS)
#define MUTEX_OWNER(M)  (kthread_t *)MUTEX_STRIP((__psunsigned_t)(M))
#define MUTEX_WAITERS(M) 	((M)->m_bits & MUTEX_QUEUEBIT)
#define MUTEX_QUEUE_HEAD(M) 	((M)->m_queue)

#if MP
#define MUTEX_ISLOCKED(M)	(((M)->m_bits) & MUTEX_LOCKBIT)
#else
#define MUTEX_ISLOCKED(M)	issplhi(getsr())
#endif

/*
 * Private definitions for sync variables.
 */
#define SV_LOCK		0x1	/* sv_t bitlock */
#define	SV_TYPE		0x6	/* type field: SV_FIFO, SV_LIFO, SV_PRIO */
#define SV_BITS		0x7	/* all control bits */
#define SV_WAITER(svp)	((kthread_t *)((svp)->sv_queue & ~SV_BITS))

#if MP
#define sv_islocked(svp) (((svp)->sv_queue & SV_LOCK) && issplhi(getsr()))
#else
#define sv_islocked(svp) issplhi(getsr())
#endif

/*
 * Private definitions for semas.
 */
#define	sema_lock(sp)		mutex_bitlock(&(sp)->s_un.s_lock, SEMA_LOCK)
#define	_sema_lock(sp)		nested_bitlock(&(sp)->s_un.s_lock, SEMA_LOCK)
#define	_sema_trylock(sp)	nested_bittrylock(&(sp)->s_un.s_lock, SEMA_LOCK)
#define	sema_unlock(sp,s)	mutex_bitunlock(&(sp)->s_un.s_lock,SEMA_LOCK, s)
#define	_sema_unlock(sp)	nested_bitunlock(&(sp)->s_un.s_lock, SEMA_LOCK)
#define	sema_flagset(sp,b)	bitlock_set(&(sp)->s_un.s_lock, SEMA_LOCK, b)
#define	sema_flagclr(sp,b)	bitlock_clr(&(sp)->s_un.s_lock, SEMA_LOCK, b)
#if MP
#define sema_islocked(S) (((S)->s_un.s_st.flags&SEMA_LOCK) && issplhi(getsr()))
#else
#define sema_islocked(S) issplhi(getsr())
#endif

/*
 * int	_wsema -- remove thread from semaphore queue
 *	returns 1 if thread was found on sema's waitq, 0 otherwise.
 *
 * int	semawait -- put semaphore on waitq
 *	returns non-zero if thread was woken via signal
 */
extern int	semawait(sema_t *, int, int, inst_t *);
extern int	_wsema(sema_t *, int, struct kthread *, inst_t *);
extern void	unsema(sema_t *, struct kthread *, inst_t *);
extern void	sema_indirect_wait(struct kthread *, struct kthread *);

extern void	indirect_wait_enqueue(struct kthread *, struct kthread *);
extern void	indirect_wait_switch(struct kthread *);
extern void	indirect_wake(struct kthread *);

extern int	rslog_size(int);

extern void	thread_block(struct kthread *, int, int, int, int, long);
extern void	thread_unblock(struct kthread *, int);

#ifdef _SPACE_H

#include "fs/procfs/prsystm.h"

static __inline int
thread_blocking(kthread_t *kt, int flags)
{
	extern void     ut_endrun(struct uthread_s *);
	int		retcode = 0;

	kt->k_prtn = 1;
	kt->k_flags |= KT_SLEEP | flags;

	if (KT_ISUTHREAD(kt)) {
		uthread_t *ut;

		ut = KT_TO_UT(kt);
		/*
		 * If being traced, kick waiting parent.
		 */
		if (ut->ut_flags & (UT_PRSTOPBITS|UT_TRWAIT|UT_HOLDJS))
			retcode = prblocktrace(ut);

		ut_endrun(ut);
	}
	return(retcode);
}
#endif

/*
 * Other constants.
 */
#define SLP_WIDTH	128
#define TZERO		10


/*
 * mrlock
 *
 * DO NOT CHANGE ANY OF THESE WITHOUT CHECKING THE ASSEMBLER ROUTINES
 * in os/ksync/mrlock.s
 */

/*
 * mr_lbits
 */
#define MR_ACCINC	0x00000001
#define MR_ACC		0x00003fff	/* # of current access locks */
#define MR_ACCMAX 	0x00002000	/* no more accesses beyond here */
#define MR_UPD 		0x00004000	/* have update lock */
#define MR_BARRIER 	0x00008000	/* there are access barriers */

#define MR_WAITINC 	0x00010000	/* increment waiters by this */
#define MR_WAIT 	0x3fff0000	/* waiters of any kind */
#define MR_WAITSHFT	16

/*
 * mr_qbits
 */
#define MR_QLOCK		0x0001	
#define MR_V			0x0002
#define MR_BEHAVIOR		0x0004
#define MR_DBLTRIPPABLE		0x0008
#define MR_ALLOW_EQUAL_PRI	0x0010 	/* Allow equal priority accessors to
					 * jump over waiting updators.
					 */

#define	mr_lockq(mrp)		mutex_bitlock(&(mrp)->mr_qlock, MR_QLOCK)
#define	mr_trylockq(mrp)	mutex_bittrylock(&(mrp)->mr_qlock, MR_QLOCK)
#define	mr_unlockq(mrp,s)	mutex_bitunlock(&(mrp)->mr_qlock, MR_QLOCK, s)
#define	mr_nested_lockq(mrp)	nested_bitlock(&(mrp)->mr_qlock, MR_QLOCK)
#define	mr_nested_trylock(mrp)	(nested_bittrylock(&(mrp)->mr_qlock, MR_QLOCK))
#if defined(_COMPILER_VERSION) && (_COMPILER_VERSION>=700)
#define mr_nested_unlockq(mrp)	__synchronize(), (mrp)->mr_qlock &= ~MR_QLOCK
#else
#define mr_nested_unlockq(mrp)	(mrp)->mr_qlock &= ~MR_QLOCK
#endif

/*
 * Internal routines used by mrlock code.
 * None of these can be called by strangers.
 */
extern void	mr_dqacc(mrlock_t *, int);
extern void	mr_access(mrlock_t *);
extern void	mr_update(mrlock_t *);
extern int	mr_tryaccess(mrlock_t *);
extern int	mr_tryupdate(mrlock_t *);
extern void	mr_accunlock(mrlock_t *);
extern void	mr_unlock(mrlock_t *);
extern void	mr_demote(mrlock_t *);
extern int	mr_trypromote(mrlock_t *);
extern void	mr_bhaccess(mrlock_t *);
extern void	mr_bhaccunlock(mrlock_t *);
extern void	mr_bhdemote(mrlock_t *);

extern void	mr_update_barrier(mrlock_t *);

extern void	init_mria(mria_t *, kthread_t *);
extern void	destroy_mria(mria_t *);

/*
 * Priority Inheritance
 */
#define	BREAK	0
#define	SPIN	1
#define YIELD	2
#define RETRY	3
#define SIGNAL  4

#define RES_MUTEX	1
#define RES_MRLOCK	2

extern int 		resource_test_owner_pri(void *, char);
extern kthread_t * 	mrlock_lpri_owner(mrlock_t *);
extern kthread_t * 	mrlock_hpri_waiter(mrlock_t *);
extern kthread_t * 	mrlock_peek_hpri_waiter(mrlock_t *);
extern kthread_t * 	mrlock_resort_queue(mrlock_t *, kthread_t *);
extern void		drop_inheritance(void *, char, kthread_t *);


extern char *makesname(char *, const char *, long);

/*
 * Sleeping lock info structures.  These are dynamically allocated per ksync
 * object (wchan) if the metering or name package is installed.  Each type
 * of info structure is maintained on a separate list to facilitate searches
 * for hot locks, etc.  Each of the ksync object specific metering
 * structures in the metering package (ksync_metered.a) start with a common
 * structure which contains the object name and the forward, backward links
 * for its list, and a back pointer to the object.  The name package
 * (ksync_named.a) just uses this base structure.
 */
typedef struct wchaninfo { /* must be first element of metering structure */
	char	name[METER_NAMSZ];	/* name of wchan (must be first) */
	kqueue_t list;			/* our link in the info list */
	void	*wchan;			/* ksync object being metered/named */
} wchaninfo_t;

#if defined(SEMAINFO) && !defined(SEMAINFOP)
#define	SEMABUCKETS	1024
#define	SEMABUCKET(p)	(((__psunsigned_t)(p)>>4) & (SEMABUCKETS-1))
#else
#define	SEMABUCKETS	1
#define	SEMABUCKET(p)	0
#endif

extern int semabuckets;			/* = SEMABUCKETS */
extern int semametered;			/* = SEMAMETER ? 1 : 0 */
extern wchaninfo_t *wchaninfo(void *, mpkqueuehead_t *);
extern wchaninfo_t *wchanmeter(void *, mpkqueuehead_t *);

/* XXX remove following ifndef when we no longer compile the kernel -o32 */
#ifndef _GENASSYM
/*
 * Add an info structure for a wchan.  This should only be used by the
 * ksync code and is intended as a template for the various *meteron()
 * routines.
 */
/* ARGSUSED*/
static __inline wchaninfo_t *
allocwchaninfo(void *wchan, char *name, long sequence,
	       mpkqueuehead_t *winfolist, size_t winfosize,
	       zone_t **zone, char *zonename)
{
	extern int kheap_initialized;
#ifdef SEMAINFOP
	wchaninfo_t *winfo = *(void **)wchan;
#else
	wchaninfo_t *winfo = wchaninfo(wchan, winfolist);
#endif

	if (winfo) {
		cmn_err(CE_WARN, "%s dup @ 0x%x for wchan 0x%x [%s]",
			zonename, winfo, wchan, winfo->name);
		return NULL;
	}
	if (!kheap_initialized) {
		_SEMAINFOP(*(void **)wchan = NULL;)
		return NULL;
	}
	if (!*zone) {
		mpkqueuehead_t *bp;
		int bi;

		*zone = kmem_zone_init((int)winfosize, zonename);
		ASSERT(*zone);

		for (bi = 0, bp = winfolist; bi < SEMABUCKETS; bi++, bp++)
			mpkqueue_init(bp);
	}

	/* allocate and initialize new info structure */
	winfo = kmem_zone_zalloc(*zone, KM_SLEEP);
	if (!name)
		winfo->name[0] = '\0';
	else
		if (sequence == -1) {
			strncpy(winfo->name, name, METER_NAMSZ-1);
			winfo->name[METER_NAMSZ-1] = '\0';
		} else
			makesname(winfo->name, name, sequence);

	/* add new wchan info structure to winfolist */
	winfo->wchan = wchan;
	_SEMAINFOP(*(void **)wchan = winfo;)
	mpkqueue_enter_last(&winfolist[SEMABUCKET(wchan)], &winfo->list);

	return winfo;
}

/*
 * Remove the info structure for a wchan.  This should only be used by the
 * ksync code and is intended as a template for the various *meteroff()
 * routines.  Note that this can't be used by *meteroff() routines like
 * semameteroff() and mrmeteroff() which have ancillary data structures to
 * free.
 */
static __inline void
freewchaninfo(void *wchan, mpkqueuehead_t *winfolist, zone_t *zone)
{
#ifdef SEMAINFOP
	wchaninfo_t *winfo = *(void **)wchan;
#else
	wchaninfo_t *winfo = wchaninfo(wchan, winfolist);
#endif
	if (winfo) {
		_SEMAINFOP(*(void **)wchan = NULL;)
		mpkqueue_remove(&winfolist[SEMABUCKET(wchan)], &winfo->list);
		kmem_zone_free(zone, winfo);
	}
}

#ifdef SEMAINFOP
/*
 * In turns out to be immensely easy to get the name of a wchan under this
 * scheme: If neither SEMAMETER nor SEMANAME is defined, then the info
 * pointer points at the literal string passing into the ksync object
 * initialization routine.  Otherwise it points at the info structure for
 * the object but since the first field in all info structures is the name,
 * we can return a pointer to that field without having to know what type of
 * ksync object we're looking at.  Thus, in both the metered and unmetered
 * cases we simply return the ``info'' pointer.
 */
static __inline char *
wchanname(void *wchan, int flags)
{
	static char *null = "(null)";
	static char *noname = "";
	static char *indirectq = "indirectq";
	static char *unknown = "unknown";

	if (flags & (KT_WSEMA|KT_WMUTEX|KT_WSV|KT_WMRLOCK)) {
		char *info;
		if (!wchan)
			return null;
		info = *(char **)wchan;
		return info ? info : noname;
	}
	/*
	 * It would be nice to do a better job for these non-sync variable
	 * cases, but this is good enough for now.
	 */
	if (flags & KT_INDIRECTQ)
		return indirectq;

	return unknown;
}
#else
char *wchanname(void *wchan, int flags);
#endif
#endif /* !_GENASSYM */


/*
 * Mutex metering.
 */
typedef struct mmeter {
	wchaninfo_t	mm_base;	/* meter head (must be first) */
	struct k_mmeter_data {		/* dynamically maintained data */
		unsigned int	d_lock;		/* locks attempted */
		unsigned int	d_unlock;	/* unlocks */
		unsigned int	d_lockhit;	/* locks, no wait */
		unsigned int	d_unoslp;	/* unlock, nobody waiting */

		unsigned int	d_trylock;	/* trylocks attempted */
		unsigned int	d_tryhit;	/* successful trylocks */

		uint64_t	d_id;		/* last id to acquire */
		struct kthread	*d_thread;	/* last thread to acquire */
		inst_t		*d_lastpc;	/* last routine that acquired */
		unsigned short	d_maxnwait;	/* maximum waiters ever */
		/* Wait timers */
		uint_t          d_waittime;     /* Max wait time */
		uint64_t        d_totalwaittime;/* Total wait time */
		inst_t          *d_waiter;      /* Who waited the longest */

		uint_t          d_ts;           /* Timestamp to compute hold time */
		uint_t          d_holdtime;     /* Max hold time */
		uint64_t        d_totalholdtime;/* Max hold time */
		inst_t          *d_holder;      /* Who held the longest */
	}		mm_data;
	unsigned short	mm_nwait;	/* number of waiters */
} mmeter_t;

#define mm_lock		mm_data.d_lock
#define mm_unlock	mm_data.d_unlock
#define mm_lockhit	mm_data.d_lockhit
#define mm_unoslp	mm_data.d_unoslp
#define mm_trylock	mm_data.d_trylock
#define mm_tryhit	mm_data.d_tryhit
#define mm_id		mm_data.d_id
#define mm_thread	mm_data.d_thread
#define mm_lastpc	mm_data.d_lastpc
#define mm_maxnwait	mm_data.d_maxnwait
#define mm_waittime     mm_data.d_waittime
#define mm_totalwaittime mm_data.d_totalwaittime
#define mm_waiter       mm_data.d_waiter

#define mm_ts     mm_data.d_ts
#define mm_holdtime     mm_data.d_holdtime
#define mm_totalholdtime mm_data.d_totalholdtime
#define mm_holder       mm_data.d_holder

extern mpkqueuehead_t mutexinfolist[];
/* XXX remove following ifndef when we no longer compile the kernel -o32 */
#ifndef _GENASSYM
static __inline mmeter_t *
mutexmeter(mutex_t *mp)
{
	return (mmeter_t *)wchanmeter(mp, mutexinfolist);
}
#endif


/*
 * Synchronization variable metering.
 */
typedef struct svmeter {
	wchaninfo_t	sm_base;	/* meter head (must be first) */
	struct k_svmeter_data {		/* dynamically maintained data */
		unsigned int	d_wait;	/* sv_wait */
		unsigned int	d_waitsig;	/* sv_waitsig */
		unsigned int	d_sigbrk;	/* sv_waitsig broken */

		unsigned int	d_signal;	/* sv_signal called */
		unsigned int	d_signalled;	/* # woken by sv_signal */
		unsigned int	d_broadcast;	/* sv_broadcast called */
		unsigned int	d_broadcasted;	/* # woken by sv_broadcast */
		unsigned int	d_wsyncv;	/* calls to vsyncv */
		unsigned int	d_wsyncvd;	/* # woken by vsyncv */
		unsigned int	d_unsyncv;	/* calls to unsyncv */
		unsigned int	d_unsyncvd;	/* # woken by unsyncv */

		uint64_t	d_id;		/* last thread to acquire */
		struct kthread	*d_thread;	/* last thread to acquire */
		inst_t		*d_lastpc;	/* last routine that acquired */
		unsigned short	d_maxnwait;	/* maximum waiters ever */
	}		sm_data;
	unsigned short	sm_nwait;	/* number of waiters */
} svmeter_t;

#define sm_wait		sm_data.d_wait
#define sm_waitsig	sm_data.d_waitsig
#define sm_sigbrk	sm_data.d_sigbrk
#define sm_signal	sm_data.d_signal
#define sm_signalled	sm_data.d_signalled
#define sm_broadcast	sm_data.d_broadcast
#define sm_broadcasted	sm_data.d_broadcasted
#define sm_id		sm_data.d_id
#define sm_thread	sm_data.d_thread
#define sm_lastpc	sm_data.d_lastpc
#define sm_maxnwait	sm_data.d_maxnwait
#define sm_wsyncv	sm_data.d_wsyncv
#define sm_wsyncvd	sm_data.d_wsyncvd
#define sm_unsyncv	sm_data.d_unsyncv
#define sm_unsyncvd	sm_data.d_unsyncvd

extern mpkqueuehead_t svinfolist[];
/* XXX remove following ifndef when we no longer compile the kernel -o32 */
#ifndef _GENASSYM
static __inline svmeter_t *
svmeter(sv_t *svp)
{
	return (svmeter_t *)wchanmeter(svp, svinfolist);
}
#endif


/*
 * Semaphore metering.
 */

/*
 * History mechanism.  History records are linked backwards through the
 * history list.  Thus, a chain of operations can be followed to find
 * out what happened to the semaphore a little easier.  The end of the
 * chain occurs either when the linkage field is zero or the ID of
 * the semaphore recorded does not match that of the chain being followed.
 */
typedef struct semahist_s {
	sema_t		*h_sid;		/* semaphore acquired */
	struct kthread	*h_thread;	/* acquiring thread */
	struct kthread	*h_woken;	/* thread awoken */
	uint64_t	h_id;		/* acquiring ID */
	uint64_t	h_wid;		/* awoken ID */
	short		h_cpuid;	/* processor doing the operation */
	char		h_op;		/* operation type */
	char		h_subop;	/* action performed */
	caddr_t		h_callpc;	/* calling pc */
	struct semahist_s *h_blink;	/* backwards history linkage */
	uint		h_seq;		/* sequence number */
	unsigned	h_ts;		/* timestamp */
} semahist_t;

extern semahist_t	*semahist;	/* array of history records */
extern unsigned int	histpos;	/* next entry to write */
extern lock_t		histlock;	/* lock on update of counter */

/*
 * This is an example of an historical element.
 */
typedef struct k_histelem {
	unsigned short	he_policy;	/* Policy (defined below) */
	unsigned short	he_length;	/* Length of array */
	int		he_base;	/* Base to use */
	int		he_offset;	/* Offset to use */
	__int64_t	he_total;	/* Total value for elems */
	int		he_updates;	/* Total number of updates */
	inst_t		*he_pc;		/* PC (threshold) */
	int		he_value;	/* Value (threshold) */
	unsigned int	*he_data;	/* Data array */
} k_histelem_t;

/*
 * These are the policies that are used to fill in historical data.
 */
#define HP_NOPOLICY	0		/* Do not keep histo data */
#define HP_ADDOFF	1		/* Add offset to base */
#define HP_MULOFF	2		/* Multiply each element by offset */


/*
 * This is the histogrammatical structure that we will use.
 */
typedef struct k_histo {
	time_t		h_ts;		/* Last hold time */
	inst_t		*h_pc;		/* Last PC */
	k_histelem_t	h_hold;		/* Information on held semas */
	k_histelem_t	h_waiting;	/* Information on waiting semas */
	k_histelem_t	h_count;	/* Information on value of count */
} k_histo_t;

/*
 * By default, when we allocate historical data, we do not set an initial
 * policy.
 */
#define ALLOC_HISTO_DATA(pp, len)	{ \
	(pp)->he_data = (unsigned int *) \
		kmem_alloc((len) * sizeof(unsigned int), \
						VM_DIRECT|VM_NOSLEEP); \
	if ((pp)->he_data) { \
		bzero((pp)->he_data, (len) * sizeof(unsigned int)); \
		(pp)->he_length = (len); \
	} \
	}
	
#define FREE_HISTO_DATA(pp) {  \
		if ((pp)->he_data && (pp)->he_length) { \
			kmem_free((pp)->he_data, \
				(pp)->he_length * sizeof(unsigned int)); \
			(pp)->he_length = 0; \
		} \
	}

#define HMETER(km)	(km->s_histo)
#define HOLDLOG(kp, value, pc)	\
			semakhistlog(&(kp)->h_hold, (value), (inst_t *)(pc))
#define WAITINGLOG(kp, value, pc)	\
			semakhistlog(&(kp)->h_waiting, (value), \
							(inst_t *)(pc))
#define COUNTLOG(kp, value, pc)	\
			semakhistlog(&(kp)->h_count, (value), (inst_t *)(pc))
#define CLRHISTOLOG(hp) { \
			bzero(&(hp)->he_total, \
				 ((char *)&(hp)->he_data - \
				 (char *)&(hp)->he_total)); \
			bzero((hp)->he_data, \
				(hp)->he_length * sizeof(unsigned int)); \
			}


extern int semadefkhist;
extern int semadefkhistlen;

/*
 * the semaphore metering struct - dynamically allocated per semaphore if
 * metering/history is requested
 */
typedef struct k_semameter {
	wchaninfo_t	s_base;		/* meter head (must be first) */
	struct k_semameter_data {	/* dynamically maintained data */
		unsigned int	d_psema;	/* psema's attempted */
		unsigned int	d_vsema;	/* vsema's attempted */
		unsigned int	d_phits;	/* successful psema's */
		unsigned int	d_vnoslp;	/* vsema, nobody waiting */
	
		unsigned int	d_cpsema;	/* cpsema's attempted */
		unsigned int	d_cphits;	/* successful cpsema's */
		unsigned int	d_cvsema;	/* cvsema attempts */
		unsigned int	d_cvnoslp;	/* cvsema, nobody waiting */
	
		unsigned int	d_wsema;	/* wsema's attempted */
		unsigned int	d_wnoslp;	/* wsema with nobody there */

		unsigned int	d_unsema;	/* unsema's */
	
		unsigned int	d_maxnwait;	/* maximum waiters ever */

		uint64_t	d_id;		/* last thread to acquire */
		struct kthread	*d_thread;	/* last thread to acquire */
		inst_t		*d_lastpc;	/* last pc that acquird sema */
	}		s_data;
	unsigned int	s_nwait;	/* number of waiters */
	semahist_t	*s_blink;	/* for history linkage */
	void		*s_histo;	/* histogram/hold ptr */
} k_semameter_t;

#define s_psema		s_data.d_psema
#define s_vsema		s_data.d_vsema
#define s_phits		s_data.d_phits
#define s_vnoslp	s_data.d_vnoslp
#define s_cpsema	s_data.d_cpsema
#define s_cphits	s_data.d_cphits
#define s_cvsema	s_data.d_cvsema
#define s_cvnoslp	s_data.d_cvnoslp
#define s_wsema		s_data.d_wsema
#define s_wnoslp	s_data.d_wnoslp
#define s_unsema	s_data.d_unsema
#define s_maxnwait	s_data.d_maxnwait
#define s_id		s_data.d_id
#define s_thread	s_data.d_thread
#define s_lastpc	s_data.d_lastpc

extern mpkqueuehead_t semainfolist[];
/* XXX remove following ifndef when we no longer compile the kernel -o32 */
#ifndef _GENASSYM
static __inline k_semameter_t *
smeter(sema_t *sp)
{
	return (k_semameter_t *)wchanmeter(sp, semainfolist);
}
#endif

extern int semakhistlog(k_histelem_t *, int, inst_t *);
extern void semakhistclr(sema_t *);
extern int semakhistinit(sema_t *, unsigned short);

extern unsigned int	defaultsemameter;

/*
 * Major op code for semaphore log.
 */
#define	SEMAOP_PSEMA	1
#define	SEMAOP_VSEMA	2
#define	SEMAOP_CPSEMA	3
#define	SEMAOP_CVSEMA	4
#define	SEMAOP_WSEMA	5
#define	SEMAOP_UNSEMA	6
#define	SEMAOP_INIT	7
#define	SEMAOP_FREE	8

/*
 * Minor opcodes for semaphore log.
 */
/* psema */
#define	SSOP_PHIT	1
#define	SSOP_PSLP	2
#define	SSOP_PWAKE	6
#define	SSOP_PRESIG	7
#define	SSOP_POSTSIG	8
/* vsema */
#define	SSOP_VNOWAKE	3
#define	SSOP_VWAKE	4
/* cpsema */
#define	SSOP_CPHIT	1
#define	SSOP_CPMISS	5
/* cvsema */
#define	SSOP_CVNOWAKE	3
#define	SSOP_CVWAKE	4
/* wsema */
#define	SSOP_WMISS	5
#define	SSOP_WWAKE	4
/* unsema */
#define	SSOP_RMV	9

/*
 * Linked list of pointers to all allocated mrsplocks -- for metering
 */

typedef struct k_mrsmeter {
  mrlock_t *mrp;
  struct k_mrsmeter *next;
} k_mrsmeter_t;


/*
 * Multi-reader/single-writer locks.
 */

/*
 * mre_uses
 */
#define	MRE_ACCESS	0x01
#define	MRE_UPDATE	0x02
#define	MRE_PENDING	0x04
#define	MRE_ENDING	0x08
#define	MRE_TRYACCESS	0x10
#define	MRE_TRYUPDATE	0x20

#define	MRE_ENTRIES	8

typedef struct mr_entry_s {
	unsigned short	mre_use;
	short		mre_index;
	inst_t		*mre_pc;	/* calling pc of operation */
	uint64_t        mr_ts;		/* Reader timestamp */
	uint64_t	mre_kid;	/* kthread id */
} mr_entry_t;

typedef struct mr_log_s {
	mr_entry_t	mrl_entries[MRE_ENTRIES];	/* _must_ be first */
	struct mr_log_s *mrl_next;
} mr_log_t;

/*
 * Metering structure for multi-reader locks
 */
typedef struct k_mrmeter {
	wchaninfo_t	mr_base;	/* meter head (must be first) */

#define	mr_firststat	mr_log		/* start of zero'able statistics */

	mr_log_t	mr_log;		/* current lock requests/holders */
	lock_t		mr_loglock;
	uint64_t	mr_lastacc;	/* threadid of last access acquired */
	uint64_t	mr_lastupd;	/* threadid of last update acquired */
	uint64_t	mr_lastunl;	/* threadid of last thread to unlock */
	struct kthread	*mr_lastathp;	/* thread address of last access */
	struct kthread	*mr_lastuthp;	/* thread address of last update */
	struct kthread	*mr_lastunlthp;	/* thread address of last unlock */
	inst_t		*mr_lastapc;	/* last routine to access */
	inst_t		*mr_lastupc;	/* last routine to update */
	uint64_t	mr_lastaccunl;	/* threadid of last thread to unlock */
	uint64_t	mr_lastupdunl;	/* threadid of last thread to unlock */
	inst_t		*mr_lastaccunlpc;	/* last routine to unlock */
	inst_t		*mr_lastupdunlpc;	/* last routine to unlock */
	uint_t		mr_acclocks;	/* # times acc-lock called */
	uint_t		mr_accunlocks;	/* # times acc-unlock called */
	uint_t		mr_updlocks;	/* # times upd-lock called */
	uint_t		mr_updunlocks;	/* # times upd-unlock called */

	/* Wait timing stuff */
	inst_t		*mr_wwaitpc;    /* Which writer waited the longest? */
	uint_t		mr_wwaittime;   /* longest writer waited for lock */
	cpuid_t		mr_wwaitcpu;    /* Which cpu was that? */ 
	uint64_t	mr_totalwwaittime; /* total time writer waited lock */

	inst_t		*mr_rwaitpc;    /* Which writer waited the longest? */
	uint_t		mr_rwaittime;   /* longest reader waited for lock */
	cpuid_t		mr_rwaitcpu;    /* Which cpu was that? */ 
	uint64_t	mr_totalrwaittime; /* total time reader waited lock */

	/* Hold timing stuff */
	uint_t		mr_wts;         /* Timestamp -- writer got the lock */
	uint_t		mr_wholdtime;   /* longest writer held lock */
	inst_t		*mr_wholdpc;    /* Who did this naughty thing? */
	cpuid_t		mr_wholdcpu;    /* Where did he do it */
	uint64_t	mr_totalwholdtime; /* total time writers held lock */

	uint64_t	mr_totalrholdtime; /* total time reader held lock */
#if 0
	k_mrreader_t	*mr_readers[MR_READER_BUCKETS]; /* Bloat much? */
#endif
	inst_t		*mr_rholdpc;    /* Bad bad bad boy */
	uint_t		mr_rholdtime;   /* longest reader held lock */
	cpuid_t		mr_rholdcpu;    /* Where was the crime committed? */
	uint_t 		mr_accphits;	/* # times no-wait mrpsema for access */
	uint_t 		mr_updphits;	/* # times no-wait mrpsema for update */
	uint_t 		mr_accpslp;	/* # times mrpsema, wait for access */
	uint_t 		mr_updpslp;	/* # times mrpsema, wait for update */
	uint_t 		mr_accv;	/* # times mrvsema, wakes accesser */
	uint_t 		mr_updv;	/* # times mrvsema, wakes updater */
	uint_t 		mr_vnoslp;	/* # times mrvsema called, no waiter */
	uint_t		mr_dqacc;	/* # accs dequeued w/ demote */
	short		mr_nwait;	/* # queued on mrlock */
	short		mr_maxnwait;	/* max # queued on mrlock */
} k_mrmeter_t;

extern mpkqueuehead_t mrinfolist[];
/* XXX remove following ifndef when we no longer compile the kernel -o32 */
#ifndef _GENASSYM
static __inline k_mrmeter_t *
mrmeter(mrlock_t *mrp)
{
	return (k_mrmeter_t *)wchanmeter(mrp, mrinfolist);
}
#endif


/*
 * synchronization primitive count information.
 */
struct synccnt {
	int	current;		/* number of current sync objects */
	int	initialized;		/* number ever initialized */
};
struct syncinfo {
	struct synccnt	mutex;		/* number of mutexes */
	struct synccnt	semaphore;	/* number of semaphores */
	struct synccnt	mrlock;		/* number of MR sleeping locks */
	struct synccnt	sv;		/* number of SVs */
	struct synccnt	spinlock;	/* number of spinlocks */
	struct synccnt	bitlock;	/* number of bitlocks */
	struct synccnt	bitlock64;	/* number of 64-bit bitlocks */
	struct synccnt	mrsplock;	/* number of MR spinlocks */
};

extern struct syncinfo	syncinfo;	/* sync primitive information */

#endif /* __SYS_SEMA_PRIVATE_H__ */
