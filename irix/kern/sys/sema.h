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
#ifndef __SYS_SEMA_H__
#define __SYS_SEMA_H__

#ident	"$Revision: 3.119 $"

#include <sys/types.h>
#include <sys/timespec.h>
#include <sys/param.h>

struct kthread;
struct proc;
struct uthread_s;

/*
 * Mutual exclusion locks and synchronization routines.
 *
 * The Irix kernel supports both sleeping and spinning mutual exclusion locks,
 * synchronization variables, multi-access/single-update mutual exclusion locks,
 * plus counting semaphores which can be used for both mutual exclusion and
 * synchronization.
 */

/*
 * Sleeping mutex lock routines.
 *
 * Sleeping mutexes implement priority-inheritance: a thread of execution
 * (thread) which owns a mutex can inherit a better priority from another
 * thread of execution waiting for the mutex; the inherited priority
 * is lost when the mutex is released.
 *
 * Mutexes can not be acquired by interrupt handlers, and a mutex can only
 * be released by the thread of execution that acquired it -- semaphore
 * routines can be used, instead, by clients than cannot follow these rules. 
 */

typedef struct mutex {
#ifdef SEMAINFOP
	void		*m_info;	/* name/meter info */
#endif
	__psunsigned_t	m_bits;
	struct kthread	*m_queue;	/* sleep queue head */
} mutex_t;

/*
 * Types for mutex_init(), mutex_alloc()
 */
#define MUTEX_DEFAULT	0x0

#ifdef _KERNEL
/*
 * void	mutex_lock(mutex_t *mp, int flags);
 * int	mutex_trylock(mutex_t *mp); 		1 => success; 0 => failure
 * void	mutex_unlock(mutex_t *mp);
 *
 * flags:
 *	PLTWAIT (sys/param.h): this is a long term wait - not to
 *	be used in load average calculations
 */
extern void	mutex_lock(mutex_t *, int);
extern int	mutex_trylock(mutex_t *);
extern void	mutex_unlock(mutex_t *);

#define METER_NAMSZ	16
#define METER_NO_SEQ	-1

/*
 * void mutex_init(mutex_t *mp, int type, char *name);
 * void	init_mutex(mutex_t *mp, int type, char *name, long sequence);
 *
 * Name may be null -- it is only used when metering mutexes are installed,
 * tag the metering structure with an ascii name.
 * Only METER_NAMSZ-1 characters are recorded.
 *
 * If init_mutex interface is used to initialize, metering name is
 * constructed from 'name' prefix and and ascii suffix generated from
 * the 'sequence' argument:  [ "MyLock", 12 ] becomes "MyLock00012"
 */
#define mutex_init(mp, type, name)	init_mutex(mp, type, name, -1)
extern void	init_mutex(mutex_t *, int, char *, long);
extern void	mutex_destroy(mutex_t *);
extern int	mutex_rmproc(mutex_t *, struct kthread *);

/*
 * mutex_t *mutex_alloc(int type, int flags, char *name, long sequence);
 *
 * Name can be null; sequence == METER_NO_SEQ -> no debug sequencer number.
 * Flags: only KM_SLEEP/KM_NOSLEEP are recognized (sys/kmem.h).
 */
extern mutex_t	*mutex_alloc(int, int, char *, long);
extern void	mutex_dealloc(mutex_t *);

/*
 * Debugging routines.
 *
 * mutex_owner -- returns pointer to owning thread (sleeping mutexes only)
 * mutex_owned -- returns non-zero if mutex is currently owned.
 * mutex_mine -- returns non-zero if current thread of execution owns the mutex.
 */
extern void	*mutex_owner(mutex_t *);
extern int	mutex_owned(mutex_t *);
extern int	mutex_mine(mutex_t *);
#endif /* _KERNEL */


/*
 * Spinning mutex lock routines.
 */

/*
 * lock_t *spinlock_alloc(int, char *);
 * void spinlock_init(lock_t *lp, char *name);
 * void init_spinlock(lock_t *lp, char *name, int name-sequence);
 * void meter_spinlock(lock_t *lp, char *name);
 *
 * Allocate+initialize/initialize spinning lock.
 * Name may be null -- it is only used when debugging spinlocks are installed.
 * Only METER_NAMSZ-1 characters are recorded.
 *
 * If init_spinlock interface is used to initialize, metering name is
 * constructed from 'name' prefix and and ascii suffix generated from
 * the 'sequence' argument:  [ "SpinLck", 12 ] becomes "SpinLck0012"
 *
 * meter_spinlock is used to force spinlock metering for the named spinlock
 * when the metering spinlock package is installed, but global spinlock
 * metering is disabled.
 *
 * void spinlock_dealloc(lock_t *lp);
 * void spinlock_destroy(lock_t *lp);
 *
 * Decommission+deallocate/decommission spinning lock.
 */

typedef		int (*splfunc_t)(void);

#ifdef _KERNEL
extern lock_t	*spinlock_alloc(int, char *);
extern void	spinlock_dealloc(lock_t *);
extern void	spinlock_init(lock_t *, char *);
extern void	init_spinlock(lock_t *, char *, long);
extern void	spinlock_destroy(lock_t *);
extern void	meter_spinlock(lock_t *, char *);

/*
 * int	mutex_spinlock(lock_t *sp);
 * int	mutex_spintrylock(lock_t *sp);
 * int	mutex_spinlock_spl(lock_t *sp, splfunc_t);
 *
 *	return an opaque ``cookie'' that is passed to mutex_spinunlock();
 *	mutex_spinlock and mutex_spintrylock set interrupt level to splhi;
 *	mutex_spintrylock returns 0 on failure, and doesn't set ipl;
 *	mutex_spinlock_spl calls the specified ipl routine;
 *
 * void	mutex_spinunlock(lock_t *sp, int rv);
 * int	mutex_spintrylock(lock_t *sp, int rv);
 *	returns non-zero on success, 0 on failure
 */
#if MP
extern int	mutex_spintrylock_spl(lock_t *, splfunc_t);
extern int	mutex_spinlock(lock_t *);
extern int	mutex_spintrylock(lock_t *);
extern void	mutex_spinunlock(lock_t *, int);
extern int	mutex_spinlock_spl(lock_t *, splfunc_t);
extern int	spinlock_islocked(lock_t *);
extern int	spinlock_initialized(lock_t *);
#else
extern int			splhi(void);
extern void			splx(int);
#define mutex_spinlock(l)	splhi()
#define mutex_spintrylock(l)	splhi()
#define mutex_spinunlock(l,x)	splx(x)
#define mutex_spinlock_spl(x,y)	y()
#define mutex_spintrylock_spl(x,y)	y()

#define	spinlock_islocked(l)	(!isspl0(getsr()))
#define	spinlock_initialized(l)	1
#endif

#endif /* _KERNEL */

/*
 * Sync variables.
 *
 * Wait for a condition/state change.
 */
typedef struct sv {
#ifdef SEMAINFOP
	void		*sv_info;	/* name/meter info */
#endif
	__psunsigned_t	sv_queue;	/* sleep queue head | SV_BITS */
} sv_t;

/*
 * void	sv_wait(sv_t *svp, int flags, void *lp, int rv);
 * int	sv_wait_sig(sv_t *svp, int flags, void *lp, int rv);
 *
 * Place caller on svp's wait-queue,
 * releasing (spinning or sleeping) mutex lock.
 *
 * If passed lock is a sleeping mutex, rv must be 0;
 * otherwise it is the return value from mutex_spinlock call --
 *
 * flags:
 *	PRECALC (sys/param.h): ignore priority bits, and instead use
 *	current priority when determining calculating priority after
 *	sleeping on mutex.
 *
 *	flags & TIMER_SHIFT(index) (sys/timers.h): sleep state to which
 *	thread is charged while waiting on this sync variable.
 */
extern void	sv_wait(sv_t *, int, void *, int);
extern int	sv_wait_sig(sv_t *, int, void *, int);

/*
 * sv_signal -- wake up, at most, one thread waiting on sync-variable
 * sv_broadcast -- wake up all threads waiting on sync-variable
 * sv_broadcast_bounded -- wake up all threads waiting on a sync-variable
 *                         but do not wake up any more than were waiting
 *                         at initial entry (no infinite loop, thanks).
 *
 * return: number of threads wakened
 */
extern int	sv_signal(sv_t *);
extern int	sv_foreach_wake(sv_t *,
				void (*)(struct kthread *, void *), void *);
extern int	sv_broadcast(sv_t *);
#ifdef _KERNEL
extern int	sv_broadcast_bounded(sv_t *);
#endif /* _KERNEL */

/*
 * Initialize sync variables.
 *
 * void	sv_init(sv_t *svp, int type, char *name);
 * void	init_sv(sv_t *svp, int type, char *name, long sequencer);
 *
 * Name may be null; used only when metering routines are installed
 * (see mutex_init, init_mutex above).
 */
#define sv_init(svp, type, name)	init_sv(svp, type, name, -1)
extern void	init_sv(sv_t *, int, char *, long);

#define SV_FIFO		0x0		/* sv_t is FIFO type */
#define SV_LIFO		0x2		/* sv_t is LIFO type */
#define SV_PRIO		0x4		/* sv_t is PRIO type */
#define SV_KEYED	0x6		/* sv_t is KEYED type */
#define SV_DEFAULT	SV_FIFO

/*
 * sv_t *sv_alloc(int type, int flags, char *name)
 *
 * Name may be null.
 * Type: SV_DEFAULT or SV_LIFO.
 * Flags: only KM_SLEEP/KM_NOSLEEP are recognized (sys/kmem.h).
 */
extern sv_t	*sv_alloc(int, int, char *);

/*
 * sv_destroy -- uninitialize sync variable
 * sv_dealloc -- uninitialize and deallocate sync variable
 */
extern void	sv_destroy(sv_t *);
extern void	sv_dealloc(sv_t *);

/*
 * wsycnv  -- remove p from svp's wait-queue, and place on scheduler run queue
 * unsycnv -- remove p from svp's wait-queue
 *
 * return: 1 if thread was found on wait-queue, 0 otherwise.
 */
extern int	wsyncv_proc(sv_t *, struct proc *);
extern int	wsyncv_uthread(sv_t *, struct uthread_s *);
extern int	wsyncv(sv_t *, struct kthread *);
extern int	unsyncv(sv_t *, struct kthread *);

/*
 * Debugging routines:
 *
 * int	sv_waitq(sv_t *svp) -- returns the # of threads sleeping on svp
 */
extern int	sv_waitq(sv_t *);


/*
 * Counting semaphores.
 *
 * Multi-use synchronizing objects.
 *
 * Can be used for mutual exclusion by clients which break basic
 * mutex_lock rules, or which don't want priority inheritance.
 * Semaphores _are_ breakable by signals, depending on the flags argument
 * to psema() -- if the PMASK portion of the flags argument is > PZERO,
 * the semaphore can be broken by a signal.
 *
 * A mutual exclusion semaphore must be initialized to 1.
 *
 * Semaphores can also be used as synchronizing objects.  The semantics
 * differ from sync variables, in that a mutual exclusion lock isn't
 * implicitly released when the sync semaphore is ``acquired''.
 */

/*
 * Semaphore structure.
 */
typedef struct sema_s {
#ifdef SEMAINFOP
	void		*s_info;	/* name/meter info */
#endif
	union {
		struct {
			short	count;
			short	flags;
		} s_st;
		uint_t	s_lock;
	} s_un;
	struct kthread	*s_queue;	/* sleep queue head */
} sema_t;

/*
 * sema flags -- depend on big-endian ordering to work
 */
#define SEMA_NOHIST	0x0001	/* don't collect semaphore history */
#define SEMA_LOCK	0x0004	/* lock bit */

#ifdef _KERNEL
/*
 * void	initsema(sema_t *sp, int count);
 * void	initnsema(sema_t *sp, int count, char *name);
 * void	init_sema(sema_t *sp, int count, char *name, long name-sequence);
 * void	freesema(sema_t *sp);
 * sema_t sema_alloc(int vmflags, int count, char *name);
 * void	sema_dealloc(sema_t *sp);
 *
 * count: 1 for mutual-exclusion semaphores; 0 for synchronizing semaphores
 */
#define initsema(sp, val)		init_sema(sp, val, NULL, -1)
#define initnsema(sp, val, name)	init_sema(sp, val, name, -1)
extern void	init_sema(sema_t *, int, char *, long);
extern void	freesema(sema_t *);
extern sema_t	*sema_alloc(int, int, char *);
extern void	sema_dealloc(sema_t *);
extern short	valusema(sema_t *);

#define initnsema_mutex(S, N)	initnsema(S, 1, N)
#define initnsema_synch(S, N)	initnsema(S, 0, N)

/*
 * semaphore-equivalents of sv_wait and sv_waitsig -- these take pointers
 * to locked semaphores as the mutual exclusion lock to release after the
 * caller is placed on svp's sleep queue.
 *
 * void	sv_sema_wait(sv_t *svp, int flags, sema_t *sp);
 * int	sv_sema_wait_sig(sv_t *svp, int flags, sema_t *sp);
 *
 * The arguments are:
 *	sync-variable on which to sleep;
 *	flags for sync-variable;
 *	address of semaphore to release.
 */
extern void	sv_sema_wait(sv_t *, int, sema_t *);
extern int	sv_sema_wait_sig(sv_t *, int, sema_t *);

/*
 * semameteron -- enable metering for the specified semaphore
 * semameteroff -- disable metering for the specified semaphore
 *
 * These routines do nothing if metering packing isn't installed.
 * Metering package recognizes only METER_NAMSZ-1 characters.
 */
extern int	semameteron(sema_t *, char *, long);
extern void	semameteroff(sema_t *);

#ifdef DEBUG
extern int	ownsema(sema_t *);
#endif

/*
 * int	psema(sema_t *sp, int flags);
 *
 * Called to acquire a semaphore unconditionally.
 * Returns:
 *	0 - semaphore ``acquired'' (may sleep)
 *	If (flags & PMASK) > PZERO:
 *         -1 - PCATCH set and signal occurred
 */
extern int	psema(sema_t *, int);

/*
 * void ipsema(sema_t *sp);
 *
 * Interrupt rendezvous semaphore, can only be called by an ithread.
 *
 * Acquiring semaphore decrements its count;
 * semaphore acquired only if semaphore count was > 0 on entry.
 *
 * If semaphore is NOT acquired, ipsema does NOT return.  Instead,
 * the ithread will restart its currently bound function when
 * it is next made runnable.
 */
extern void	ipsema(sema_t *);

/*
 * int cpsema(sema_t *sp);
 *
 * Returns:	1 if semaphore acquired;
 *		0 if semaphore not acquired.
 *
 * Acquiring semaphore decrements its count;
 * semaphore acquired only if semaphore count was > 0 on entry.
 */
extern int	cpsema(sema_t *);

/*
 * int vsema -- unconditionally increment counter, wakes up first queued thread
 * int cvsema -- conditionally increments counter and wake up first thread
 *
 * return 1 if a queued thread was woken; 0 otherwise.
 */
extern int	vsema(sema_t *);
extern int	cvsema(sema_t *);

/*
 * int	wsema -- remove thread from sema's wait-queue, and schedule to run
 *
 * return 1 if thread was found on wait-queue, 0 otherwise.
 */
extern int	wsema(sema_t *, struct kthread *);

/*
 * Turn semaphore history mechanism off/on.
 * Ignored when metering system isn't installed.
 */
extern void semahistoff(sema_t *);
extern void semahiston(sema_t *);

/*
 * If metering is installed, and defaultsemameter is set, _all_ semaphores,
 * sleeping mutexes, sync-variables and mrlocks are metered.
 * This variable is set iff DEFAULTSEMAMETER is defined in the system's
 * irix.sm config file.
 */
extern unsigned int	defaultsemameter;

#endif /* _KERNEL */

/* +1 for realtime class with all positive pri's */
#define NSCHEDCLASS (-(PWEIGHTLESS)+1)

typedef struct priq_s {
	struct mri_s 	*pq_pq[NSCHEDCLASS];
#ifdef DEBUG
	short    	pq_pri; /* lowest in queue */
#endif
} priq_t;

/*
 * kpriq_t is for the list of waiters, the list
 * being sorted in decending order. Other than
 * the sort order, kpriq_t differs from priq_t
 * in its use of links: because a thread can be
 * waiting on atmost one mrlock, well known links
 * (k_flink & k_blink) are used to queue on kpriq_t
 */
typedef struct kpriq_s {
	struct kthread 	*pq_pq[NSCHEDCLASS];
#ifdef DEBUG
	short    	pq_pri; /* highest in queue */
#endif
} kpriq_t;

/*
 * Multi-reader locks
 */
typedef struct mrlock_s {
#ifdef SEMAINFOP
	void		*mr_info;	/* name/meter info */
#endif
	uint_t	mr_lbits;
	union {
		struct {
			short	qcount;
			short	qflags;
		} mr_st;
		uint_t	qbits;
	} mr_un;
	/* all queues below are priority ordered */
	kpriq_t		mr_waiters;	/* sleep queue */
	priq_t 		mr_holders;	/* list of holders */
#ifdef DEBUG
	int		mr_nholders;
#endif
} mrlock_t;

#define mr_pcnt		mr_un.mr_st.qcount
#define mr_qbits	mr_un.qbits
#define mr_qlock	mr_un.qbits

/*
 * Arguments to mrlock()
 * NOTE: mrlock/cmrlock are deprecated interfaces. More compact interfaces
 * should now be used - mraccess, mrupdate.  These two (along with mrunlock)
 * are very fast, involving one function call with ~10 instructions w/ 1 memory
 * reference for the fast path.
 */
#define MR_ACCESS	1
#define MR_UPDATE	2

/*
 * mrlock types
 *
 * A barrier-type mrlock will hold off future accessers if an update request
 * is made while the lock is in access mode -- which avoids the possibility
 * of update-starvation in a high-access mrlock.  But it won't allow
 * double-tripping in access mode, and is a bit more expensive to acquire
 * (in access mode) than a dbltrippible mrlock (about seven MIPS instructions,
 * including one branch).
 */
#define MRLOCK_BARRIER		0x1
#define MRLOCK_BEHAVIOR		0x2
#define MRLOCK_DBLTRIPPABLE	0x4
#define MRLOCK_ALLOW_EQUAL_PRI	0x8

#define MRLOCK_DEFAULT	MRLOCK_BARRIER

#ifdef _KERNEL
#define mrinit(mrp, name)	mrlock_init(mrp, MRLOCK_BARRIER, name, -1)
#define mrbhinit(mrp, name)	mrlock_init(mrp, MRLOCK_BARRIER|MRLOCK_BEHAVIOR, name, -1)
extern void	mrlock_init(mrlock_t *, int type, char *name, long sequence);
extern void	mrfree(mrlock_t *);
extern void	mrmeteron(mrlock_t *, char *, long);
extern void	mrmeteroff(mrlock_t *);

/*
 * mraccessf/mrupdatef take flags to be passed in while sleeping;
 * only PLTWAIT is currently supported.
 */
extern void	mraccessf(mrlock_t *, int);	/* grab for READ/ACCESS */
extern void	mrupdatef(mrlock_t *, int);	/* grab for WRITE/UPDATE */

extern int	mrspinaccess(mrlock_t *);
extern int	mrspinupdate(mrlock_t *);
extern int	mrtryspinaccess(mrlock_t *, int *);
extern int	mrtryspinupdate(mrlock_t *, int *);
extern void	mrspinunlock(mrlock_t *, int);
extern void	mrspinaccess_upgrade(mrlock_t *, int);
extern void	mrspinupdate_upgrade(mrlock_t *, int);

#define mraccess(mrp)	mraccessf(mrp, 0)	/* grab for READ/ACCESS */
#define mrupdate(mrp)	mrupdatef(mrp, 0)	/* grab for WRITE/UPDATE */

extern void	mrlock(mrlock_t *, int, int);
extern int	cmrlock(mrlock_t *, int);
extern int	mrtryaccess(mrlock_t *);	/* conditional READ */
extern int	mrtryupdate(mrlock_t *);	/* conditional WRITE */
extern void	mraccunlock(mrlock_t *);	/* release READ/ACCESS */
extern void	mrunlock(mrlock_t *);		/* release any mode */
extern void	mrdemote(mrlock_t *);		/* drop WRITE to READ */
extern int	mrtrypromote(mrlock_t *);	/* try READ -> WRITE */
extern int	ismrlocked(mrlock_t *, int);
extern int	mrislocked_access(mrlock_t *);
extern int	mrislocked_update(mrlock_t *);
extern int	mrislocked_any(mrlock_t *);
extern int	mrgetnwaiters(mrlock_t *);

#ifdef CELL
extern void	mrbhaccess(mrlock_t *);		/* grab for READ/ACCESS */
extern void	mrbhaccunlock(mrlock_t *);	/* release READ/ACCESS */
extern void	mrbhdemote(mrlock_t *);		/* drop WRITE to READ */
#endif /* CELL */
/*
 * the following always bypass any metering but are otherwise equivalent
 * to the above
 */
extern void mr_access(mrlock_t *);
extern void mr_update(mrlock_t *);
extern int mr_tryaccess(mrlock_t *);
extern int mr_tryupdate(mrlock_t *);
extern void mr_accunlock(mrlock_t *);
extern void mr_unlock(mrlock_t *);
extern void mr_demote(mrlock_t *);
extern int mr_trypromote(mrlock_t *);

/*
 * Locks with no priority inheritance: NO waiting allowed on these locks
 * Cannot be mixed with normal versions which support inheritance.
 */
extern int mr_tryaccess_noinherit(mrlock_t *);
extern int mr_tryupdate_noinherit(mrlock_t *);
extern void mr_unlock_noinherit(mrlock_t *);

/*
 * Mrlock tunables.
 */
extern int mrlock_num_mria; 	  /* No. of overflow mria's 		  */

/*
 * mrunlock-equivalents of sv_wait and sv_waitsig.
 *
 * void	sv_mrlock_wait(sv_t *svp, int flags, mrlock_t *mrp);
 * int	sv_mrlock_wait_sig(sv_t *svp, int flags, mrlock_t *mrp);
 *
 * The arguments are:
 *	sync-variable on which to sleep;
 *	flags for sync-variable;
 *	address of mrlock to release.
 */
extern void	sv_mrlock_wait(sv_t *, int, mrlock_t *);
extern int	sv_mrlock_wait_sig(sv_t *, int, mrlock_t *);
#endif /* _KERNEL */


/*
 * The mp_* locking routines aren't intended for DDI/DKI-conforming drivers.
 * Nor do they have a future -- they will be removed completely in a future
 * release.
 */
#ifdef _KERNEL
/*
 * sleeping and spinning mutexes, and sync-variables
 */
#define	mp_mutex_lock(m,f)		mutex_lock(m,f)
#define	mp_mutex_unlock(m)		mutex_unlock(m)
#define	mp_mutex_trylock(m)		mutex_trylock(m)
#define	mp_sv_wait(sv,f,m,s)		sv_wait(sv,f,m,s)
#define	mp_sv_wait_sig(sv,f,m,s)	sv_wait_sig(sv,f,m,s)
#define	mp_mutex_spinlock(m)		mutex_spinlock(m)
#define	mp_mutex_spinunlock(m,s)	mutex_spinunlock(m,s)

/*
 * The nested_spin routines do not provide protection against
 * premption or interrupts, and can be only used when another
 * mutex_spinlock is already held -- the debugging spinlock code
 * enforces this.
 */
extern void	nested_spinlock(lock_t *);
extern int	nested_spintrylock(lock_t *);
extern void	nested_spinunlock(lock_t *);

/*
 * Bit locks -- these are similar to the mutex_spinlock suite,
 * except the lock is a single bit in a 32-bit or 64-bit word.
 *
 * int	mutex_bitlock(uint_t *lock, uint_t lock-bit);
 * int	mutex_bittrylock(uint_t *lock, uint_t lock-bit);
 * void	mutex_bitunlock(uint_t *lock, uint_t lock-bit, int rv);
 *
 * nested_bitlock can only be called when ``protected'' by another
 * mutex_bitlock or mutex_spinlock.
 *
 * mutex_bittrylock returns 0 on failure, cookie for unlock on success.
 *
 * int	bitlock_islocked(uint *lock, uint lock-bit) -- debug
 */

extern void		init_bitlock(uint *, uint, char *, long);
extern void		destroy_bitlock(uint *);
extern void		init_64bitlock(__uint64_t *, __uint64_t, char *, long);
extern void		destroy_64bitlock(__uint64_t *);
extern void		meter_bitlock(uint *, char *);
extern void		meter_64bitlock(__uint64_t *, char *);

#if MP
extern int		mutex_bitlock(uint *, uint);
extern int		mutex_bittrylock(uint *, uint);
extern void		mutex_bitunlock(uint *, uint, int);

extern int		mutex_64bitlock(__uint64_t *, __uint64_t);
extern int		mutex_64bittrylock(__uint64_t *, __uint64_t);
extern void		mutex_64bitunlock(__uint64_t *, __uint64_t, int);

extern void		nested_bitlock(uint *, uint);
extern int		nested_bittrylock(uint *, uint);
extern void		nested_bitunlock(uint *, uint);

extern void		nested_64bitlock(__uint64_t *, __uint64_t);
extern int		nested_64bittrylock(__uint64_t *, __uint64_t);
extern void		nested_64bitunlock(__uint64_t *, __uint64_t);

extern int		bitlock_islocked(uint_t *, uint_t);
extern int		bitlock64bit_islocked(__uint64_t *, uint_t);
extern int		mutex_bitlock_spl(uint_t *, uint_t, splfunc_t);

/*
 * Bitlock routines don't have as much to do on single-processor systems.
 */

#else /* !MP */

#define mutex_bitlock(l,b)	splhi()
#define mutex_bittrylock(l,b)	splhi()
#define mutex_bitunlock(l,b,x)	splx(x)

#define mutex_64bitlock(l,b)	splhi()
#define mutex_64bittrylock(l,b)	splhi()
#define mutex_64bitunlock(l,b,x)	splx(x)

#define nested_bitlock(l,b)	((void)0)
#define nested_bittrylock(l,b)	1
#define nested_bitunlock(l,b)	((void)0)

#define nested_64bitlock(l,b)		((void)0)
#define nested_64bittrylock(l,b)	1
#define nested_64bitunlock(l,b)		((void)0)

#define bitlock_islocked(l,b)		issplhi(getsr())
#define mutex_bitlock_spl(x,b,y)	y()

#endif /* !MP */

#define mp_mutex_bitlock(l,b)		mutex_bitlock(l,b)
#define mp_mutex_bitunlock(l,b,x)	mutex_bitunlock(l,b,x)

#if (_MIPS_SZPTR == 32)
#define mutex_psbitlock(L,B)		mutex_bitlock(L,B)
#define mutex_psbittrylock(L,B)		mutex_bittrylock(L,B)
#define mutex_psbitunlock(L,B,X)	mutex_bitunlock(L,B,X)
#define nested_psbitlock(L,B)		nested_bitlock(L,B)
#define nested_psbittrylock(L,B)	nested_bittrylock(L,B)
#define nested_psbitunlock(L,B)		nested_bitunlock(L,B)
#endif

#if (_MIPS_SZPTR == 64)			/* _MIPS_SIZELONG had better be 64 */
#define mutex_psbitlock(L,B)		mutex_64bitlock(L,B)
#define mutex_psbittrylock(L,B)		mutex_64bittrylock(L,B)
#define mutex_psbitunlock(L,B,X)	mutex_64bitunlock(L,B,X)
#define nested_psbitlock(L,B)		nested_64bitlock(L,B)
#define nested_psbittrylock(L,B)	nested_64bittrylock(L,B)
#define nested_psbitunlock(L,B)		nested_64bitunlock(L,B)
#endif

/*
 * Fast bitlock/set routines.
 *
 * bitlock_set(uint_t *bits, uint_t lock_bit, uint_t set_bits);
 * bitlock_clr(uint_t *bits, uint_t lock_bit, uint_t clear_bits);
 *
 * These routines atomically set/clear bits in the memory word,
 * respecting the lock_bit -- these are the fast-path equivalents
 * of acquiring the corresponding mutex_bitlock, setting/clearing
 * bits, and releasing the mutex_bitlock.
 */
extern uint_t	bitlock_clr(uint_t *, uint_t, uint_t);
extern uint_t	bitlock_set(uint_t *, uint_t, uint_t);

/*
 * mutex_bitlock-equivalents of sv_wait and sv_waitsig.
 * Only implemented currently for integer-size bitlocks.
 *
 * (Only the uint_t-sized versions are currently implemented.)
 *
 * The arguments are:
 *	sync-variable on which to sleep;
 *	flags for sync-variable;
 *	address of bit-lock to unlock;
 *	the lock bit;
 *	return value from mutex_bitlock call.
 */
extern void	sv_bitlock_wait(sv_t *, int, uint_t *, uint_t, int);
extern int	sv_bitlock_wait_sig(sv_t *, int, uint_t *, uint_t, int);

#define	mp_sv_bitlock_wait(s,f,l,b,x)		sv_bitlock_wait(s,f,l,b,x)
#define	mp_sv_bitlock_wait_sig(s,f,l,b,x)	sv_bitlock_wait_sig(s,f,l,b,x)
#endif /* _KERNEL */

#include <sys/mon.h>


/* Reader hash buckets for multi-reader locks */

typedef struct k_mrreader {
  uint64_t mr_tid;  /* Thread ID of this guy when he acquired me.  
		       We use this for comparison reaons because threads 
		       should release their own locks.  If you do something
		       *really* funky and off the wall, YMMV.  -- Humper */
  uint_t mr_ts;     /* When was this lock acquired for reading? */
  struct k_mrmeter *the_meter;
  struct k_mrreader *next; /* And chain all those babies up... */
} k_mrreader_t;

#define MR_READER_BUCKETS 13 /* XXX How many readers will a lock 
				realistically ever have? */

extern void	mrlock_barrier(mrlock_t *, int);
extern void	mrupdate_barrier(mrlock_t *);

#if _KERNEL && !_STANDALONE
/*
 * splock spinlocks -- these forms are obsolete.
 */
#define spsema(x) nested_spinlock(&(x))
#define svsema(x) nested_spinunlock(&(x))

#define initlock(l)	spinlock_init(l,0)
#define initnlock(l,n)	spinlock_init(l,n)
#define	freesplock(l)	spinlock_destroy(&(l))

#ifdef MP
#define	ownlock(l)	spinlock_islocked(&(l))
extern void		mutex_io_spinunlock(lock_t *, int);
#define _trylock(l,f)	mutex_spintrylock_spl(&(l),f)
#else
#define ownlock(x)			1
#define mutex_io_spinunlock(l,y)	splx(y)
#define _trylock(l,y)			y()
#endif

#define	splockspl(l,f)	mutex_spinlock_spl(&(l),f)
#define	spunlockspl(l,s) mutex_spinunlock(&(l),s)
#define	splock(l)	mutex_spinlock(&(l))
#define	spunlock(l,s)	mutex_spinunlock(&(l),s)

extern void		mutex_io_spinunlock(lock_t *, int);
#define	io_splock(l)	mutex_spinlock(&(l))
#define	io_spunlock(l,s) mutex_io_spinunlock(&(l),s)
#define	io_splockspl(l,f) mutex_spinlock_spl(&(l),f)
#define	io_spunlockspl(l,s) mutex_io_spinunlock(&(l),s)

/*
 * timed wait on sync variable.
 * Puts thread on svp's wait-queue, releasing sleeping mutex mp.
 * These are either breakable or not. If breakable, returns the amount of
 * time remaining.
 */
extern void sv_bitlock_timedwait(sv_t *, int, uint *, uint, int, int,
					timespec_t *, timespec_t *);
extern int sv_bitlock_timedwait_sig(sv_t *, int, uint *, uint, int, int,
					timespec_t *, timespec_t *);
extern void sv_timedwait(sv_t *, int, void *, int, int,
					timespec_t *, timespec_t *);
extern int sv_timedwait_sig(sv_t *, int, void *, int, int,
				   struct timespec *, struct timespec *);
extern int sv_timedwait_sig_notify(sv_t *, int, void *, int, int,
				   struct timespec *, struct timespec *);

/*
 * int	sv_threshold(sv_t *svt, int threshold);
 *
 */
extern int	sv_threshold(sv_t *, int);

/*
 * Timeout flags for sv*timedwait*() routines.
 */
#define SVTIMER_FAST	0x001	    /* use fast clock timer */
#define SVTIMER_TRUNC	0x002	    /* truncate requested time to clock res */

/*
 * Aliases for mutexes and sync variables
 * for ease of porting Sun MP code
 */
#define kmutex_t	mutex_t
#define mutex_enter(m)	mutex_lock(m, PZERO)
#define mutex_tryenter(m) mutex_trylock(m)
#define mutex_exit(m)	mutex_unlock(m)

#define kcondvar_t	sv_t
#define cv_init(cv, nm, f, i)	sv_init(cv, SV_DEFAULT, nm)
#define cv_wait(cv, mp)	{ \
	sv_wait(cv, PZERO, mp, 0); \
	mutex_lock(mp, PZERO); \
}
#define cv_signal(cv)		sv_signal(cv)
#define cv_wait_sig(cv,mp)	sv_wait_sig(cv,PZERO,mp,0)
#define cv_broadcast(cv)	sv_broadcast(cv)
#define cv_destroy(cv)		sv_destroy(cv)

/*
 * aliases for read/write locks
 * for ease of porting for Sun MP code
 */
#define RW_READER	MR_ACCESS
#define RW_WRITER	MR_UPDATE

#define krwlock_t	mrlock_t

#define rw_init(r, nm, f, i)	mrinit(r, nm)
#define rw_enter(r, a)			mrlock(r, a, PZERO)
#define rw_exit(r)				mrunlock(r)
#define rw_tryupgrade(r)		mrtrypromote(r)
#define rw_downgrade(r)			mrdemote(r)
#define rw_destroy(r)			mrfree(r)
#define RW_WRITE_HELD(r)		ismrlocked(r, MR_UPDATE)
#define RW_READ_HELD(r)			ismrlocked(r, MR_ACCESS)
#endif /* _KERNEL */

#if _KERNEL

typedef struct {
	short	ms_mode;
	int	ms_cnt;
	sv_t	ms_sv;
} mslock_t;

#define MS_FREE		0
#define	MS_UPD		1
#define	MS_ACC		2
#define	MS_WAITERS	4

extern void msinit(mslock_t *, char *);
extern void msdestroy(mslock_t *);
extern void msaccess(mslock_t *, mutex_t *);
extern void msupdate(mslock_t *, mutex_t *);
extern void msunlock(mslock_t *);
extern int  is_msaccess(mslock_t *);
extern int  is_msupdate(mslock_t *);	

#endif /* _KERNEL */

#endif /* __SYS_SEMA_H__ */
