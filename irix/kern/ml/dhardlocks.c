/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1995, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 3.99 $"

#if MP

/*
 * Debug hardware locks.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sbd.h>
#include <sys/immu.h>
#include <sys/sysmacros.h>
#include <sys/pda.h>
#include <sys/sema_private.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/sema.h>
#include <sys/splock.h>
#include <sys/debug.h>
#include <sys/kopt.h>
#include <string.h>
#include <sys/atomic_ops.h>

#ifdef EVEREST
#include <sys/EVEREST/everest.h>
#endif

#ifdef SABLE_RTL
/*
 * we won't use this file anyway.  Just let it compile.
 */
#undef		mutex_spinlock
#undef		mutex_spinunlock

extern int	mutex_spinlock(lock_t *);
extern void	mutex_spinunlock(lock_t *, int);
#endif	/* !SABLE_RTL */

static void spinlockdebug(lock_t *, struct k_splockmeter *, inst_t *, int,
			  int (*)(void), int (*)(void));
static void spinunlockdebug(lock_t *, struct k_splockmeter *, inst_t *);
static int tryspinlockdebug(lock_t *, struct k_splockmeter *, inst_t *);
static int trybitlockdebug(uint *, uint, struct k_splockmeter *, inst_t *);

/*
 * Definition for casting a generic lock into private.p_curlock
 */
#define lock_type	lock_t *

/*
 * Spinlock metering:
 *
 * There are various types and sizes of spinlocks, most of which make
 * special use of the non-lock bits.  Because of this, the debugging
 * are careful to not touch any bits except the designated lock bits.
 *
 * A certain number of metering structures are preallocated at boot
 * time, enough to suffice at least until the kernel heap is initialized.
 *
 * The metering structure is found by traversing a sorted (by lock address)
 * binary tree.
 */

extern int try_spinlock(lock_t *);

#ifdef DEBUG
int spinlocks_metered;
#endif

/*
 * lockmeter_cache manages splockmeter_t structures preallocated by
 * initsplocks.  When they are depleted, splock_meter_zone is used
 * as a backup.  The initial lockmeter_cache memory is always kept
 * separate from the zone memory so the zone won't try to do
 * garbage-collection on it.
 */
static char *do_splockmeteron;			/* only meter this lock */
static lock_t	lockmeter_cache_lock;	/* to protect lockmeter_cache */
static struct k_splockmeter *lockmeter_cache; /* cache of unused splockmeters */
static struct k_splockmeter *lockmeter_cache_start;
static struct k_splockmeter *lockmeter_cache_end;
static zone_t *splock_meter_zone;

struct k_splockmeter *lockmeter_chain;	/* head of in-use splockmeters chain */
avltree_desc_t lockmeter_tree;		/* head of in-use splockmeters tree */

static mrlock_t	lockmeter_lock;		/* to protect lockmeter_tree */
static int lockmeter_update = OWNER_NONE;/* to avoid update starvation... */
static int nsplock = 2048;		/* # preallocated meter structs */

#ifdef SPLDEBUG
/* we don't want to log internal uses of spl routines ! */

#define spl7	__spl7
#define splx	__splx
#define splhi	__spl6

extern int __spl6(void);
extern int __spl7(void);
extern void __splx(int);

extern void do_splx_log(int, void *);

void __mutex_spinlock()
{
  cmn_err(CE_PANIC,"shouldn't get here!\n");
}

void __mutex_spinlock_spl()
{
  cmn_err(CE_PANIC,"shouldn't get here!\n");
}

void __mutex_spinunlock()
{
  cmn_err(CE_PANIC, "should never get here!");
}

void __mutex_bitlock()
{
  cmn_err(CE_PANIC, "should never get here!");
}

void __mutex_bitunlock()
{
  cmn_err(CE_PANIC, "should never get here!");
}
#endif /* SPDEBUG */

#if DEBUG
#define CKTRIES 1000
#define CHKCONDECL	int cktries = CKTRIES;

/* must reset cktries before calling du_conpoll, since du_conpoll
 * may turn right around and call a spinlock routine and we could
 * recurse until the lock holder finally releases the lock, possibly
 * overflowing the stack.
 */
#define CHKCON()	kdebug && --cktries <= 0 ? cktries = CKTRIES, du_conpoll() : (void)0
#else
#define CHKCONDECL
#define CHKCON()
#endif

static void splockmeter_free(k_splockmeter_t *);
static int internal_mutex_spinlock(lock_t *);
static void internal_mutex_spinunlock(lock_t *, int);

/*
 * k_splockmeter avl-tree operations
 */
static __psunsigned_t
lockmeter_avl_start(avlnode_t *np)
{
	return ((__psunsigned_t ) (((k_splockmeter_t *)np)->m_addr));
}

static __psunsigned_t
lockmeter_avl_end(avlnode_t *np)
{
	return ((__psunsigned_t ) (((k_splockmeter_t *)np)->m_addr) + 1);
}

avlops_t lockmeter_avlops = {
	lockmeter_avl_start,
	lockmeter_avl_end,
};

/*
 * Find the k_splockmeter that manages ``addr''.
 * This routine is called only from idbg, so we won't force it to
 * use locks.
 */
k_splockmeter_t *
lockmeter_find(void *addr)
{
	return((k_splockmeter_t *)
			avl_find(&lockmeter_tree, (__psunsigned_t)addr));
}

/*
 * Find the k_splockmeter that manages ``addr'', with appropriate locking.
 */
static k_splockmeter_t *
lockmeter_get(void *addr)
{
	register k_splockmeter_t *spm;
	int s;
again:
	s = spl7();
	/*
	 * Don't starve out updater wanna-be
	 */
	while (lockmeter_update != OWNER_NONE) {
		ASSERT(lockmeter_update != cpuid());
		splx(s);
		goto again;
	}
	while (!mr_tryaccess_noinherit(&lockmeter_lock))
		;
	spm = (k_splockmeter_t *)
			avl_find(&lockmeter_tree, (__psunsigned_t)addr);

	mr_unlock_noinherit(&lockmeter_lock);
	splx(s);

	return (spm);
}

static void
lockmeter_insert(k_splockmeter_t *spm)
{
	avlnode_t *node;
	k_splockmeter_t *ospm;
	int s;

	s = spl7();
	ASSERT(lockmeter_update != cpuid());
	while (!(compare_and_swap_int(&lockmeter_update, OWNER_NONE, cpuid())))
	{
		splx(s);
		spl7();
	}
	while (!mr_tryupdate_noinherit((mrlock_t *)&lockmeter_lock))
		;

	node = avl_insert(&lockmeter_tree, (avlnode_t *)spm);
	lockmeter_chain = (k_splockmeter_t *)lockmeter_tree.avl_firstino;
	if (node == NULL)
		ospm = (k_splockmeter_t *)
			avl_find(&lockmeter_tree, (__psunsigned_t)spm->m_addr);

	lockmeter_update = OWNER_NONE;
	mr_unlock_noinherit(&lockmeter_lock);
	splx(s);

	if (node == NULL) {
		cmn_err(CE_WARN,
			"lockmeter_insert: duplicate for 0x%x (%s)",
			spm->m_addr, spm->m_name);

		splockmeter_free(spm);
		cmn_err(CE_WARN,
			"lockmeter_insert: current lock in tree:(%s)\n",
			ospm->m_name);
	}
}

/*
 * Remove addr's k_splockmeter from the in-use tree;
 * return a pointer to the k_splockmeter structure.
 */
static k_splockmeter_t *
lockmeter_delete(void *addr)
{
	register k_splockmeter_t *spm;
	int s;

	s = spl7();
	ASSERT(lockmeter_update != cpuid());
	while (!(compare_and_swap_int(&lockmeter_update, OWNER_NONE, cpuid())))
	{
		splx(s);
		spl7();
	}
	/*
	 * lockmeter_update is latched -- no need to drop spl here?
	 */
	while (!mr_tryupdate_noinherit(&lockmeter_lock))
		;

	spm = (k_splockmeter_t *)
			avl_find(&lockmeter_tree, (__psunsigned_t)addr);
	if (spm) {
		avl_delete(&lockmeter_tree, (avlnode_t *)spm);
		if (lockmeter_chain == spm)
			lockmeter_chain = (k_splockmeter_t *)
						lockmeter_tree.avl_firstino;
	}

	lockmeter_update = OWNER_NONE;
	mr_unlock_noinherit(&lockmeter_lock);
	splx(s);

	return spm;
}

/*
 * splock initialization -- called very early
 *
 * Allocate a cache of k_splockmeter structs and queue them up for
 * quick allocation.  Not having a metering structure isn't fatal --
 * the lock just won't get metered, but we'd like to have meter
 * structures available for locks that are initialized early.
 */
pfn_t
initsplocks(pfn_t fpage)
{
	int m, i;
	register struct k_splockmeter *spm;

	splockmeter = 1;

	/*
	 * If "splockmeteron" environment variable is defined,
	 * only the lock whose name that matches its string
	 * will be metered by default.  Any other lock that wishes
	 * to be metered can call meter_spinlock() or meter_bitlock().
	 * To disable all spinlock metering:
	 *	either INCLUDE hardlocks instead of dhardlocks (duh);
	 *	or "boot dksc(X,Y,Z)/unix splockmeteron=unmatchable".
	 */
	do_splockmeteron = kopt_find("splockmeteron");
	if (do_splockmeteron && !strcmp(do_splockmeteron, ""))
		do_splockmeteron = NULL;
	
	m = sizeof(struct k_splockmeter) * nsplock;
	bzero((caddr_t)(PHYS_TO_K0(ctob(fpage))), m);

	lockmeter_cache_start =
	lockmeter_cache = (struct k_splockmeter *) PHYS_TO_K0(ctob(fpage));

	/* suppress insert dup warnings */
	lockmeter_tree.avl_flags = AVLF_DUPLICITY;

	for (i = 0, spm = lockmeter_cache; ++i < nsplock; spm++) {
		spm->m_avlnode.avl_forw = (avlnode_t *)(spm+1);
	}
	lockmeter_cache_end = spm;

	/*
	 * Don't bother metering the splockmap spinlock --
	 * it isn't of any real interest.  And can't meter
	 * the lock which protects the lockmeter chain.
	 */
	lockmeter_cache_lock = SPIN_INIT;

	avl_init_tree(&lockmeter_tree, &lockmeter_avlops);

	mrlock_init(&lockmeter_lock, MRLOCK_BARRIER,
			"lockmeter",  -1);

	return fpage + (btoc(m));
}

static void
spinmeter_alloc(void *addr, char *name, int type)
{
	register k_splockmeter_t *spm;
	int s;

	if (lockmeter_cache) {
		s = internal_mutex_spinlock(&lockmeter_cache_lock);
		spm = lockmeter_cache;
		if (spm)
			lockmeter_cache =
				(k_splockmeter_t *)spm->m_avlnode.avl_forw;
		internal_mutex_spinunlock(&lockmeter_cache_lock, s);
	} else
		spm = NULL;

	if (!spm) {
		if (pdaindr[getcpuid()].CpuId < 0 ) 
			return;
		if (!splock_meter_zone)
			splock_meter_zone = kmem_zone_init(
				sizeof(struct k_splockmeter), "splockmeter");
		ASSERT(splock_meter_zone);
		if (!splock_meter_zone)
			return;
		spm = kmem_zone_alloc(splock_meter_zone, KM_NOSLEEP);
		if (!spm)
			return;
	}

	bzero(spm, sizeof(k_splockmeter_t));

	/*
	 * Got a metering structure -- put it in the tree.
	 */
	spm->m_addr = addr;
	spm->m_inuse = 1;
	spm->m_type = type;

	spm->m_owner = OWNER_NONE;
	if (name) {
		strncpy(spm->m_name, name, METER_NAMSZ);
		spm->m_name[METER_NAMSZ-1] = '\0';
	}

	lockmeter_insert(spm);

#ifdef DEBUG
	atomicAddInt(&spinlocks_metered, 1);
#endif
}

static void
splockmeter_free(register k_splockmeter_t *spm)
{
	register int s;

#ifdef DEBUG
	atomicAddInt(&spinlocks_metered, -1);
#endif
	if (spm >= lockmeter_cache_start && spm <= lockmeter_cache_end) {
		s = mutex_spinlock(&lockmeter_cache_lock);
		spm->m_avlnode.avl_forw = (avlnode_t *)lockmeter_cache;
		lockmeter_cache = spm;
		mutex_spinunlock(&lockmeter_cache_lock, s);
		return;
	}

	ASSERT(splock_meter_zone);
	kmem_zone_free(splock_meter_zone, spm);
}

static void
spinmeter_free(register void *addr)
{
	register k_splockmeter_t *spm;

	spm = lockmeter_delete(addr);

	if (spm)
		splockmeter_free(spm);
}

void
meter_spinlock(register lock_t *lp, char *name)
{
	ASSERT(*lp == SPIN_INIT);
	spinmeter_alloc(lp, name, SPINTYPE_SPIN);
}

/*
 * Allocate and initialize a spinlock
 */
void
init_spinlock(register lock_t *lp, char *name, long sequence)
{
	char namebuf[METER_NAMSZ];

	*lp = SPIN_INIT;
#if defined(DEBUG) || defined(SYNCINFO)
	atomicAddInt(&syncinfo.spinlock.current, 1);
	atomicAddInt(&syncinfo.spinlock.initialized, 1);
#endif

	if (do_splockmeteron && (!name || strcmp(do_splockmeteron, name)))
		return;

	spinmeter_alloc(lp, makesname(namebuf, name, sequence), SPINTYPE_SPIN);
}

/*
 * Allocate and initialize a spinlock
 */
void
spinlock_init(register lock_t *lp, char *name)
{
	*lp = SPIN_INIT;
#if defined(DEBUG) || defined(SYNCINFO)
	atomicAddInt(&syncinfo.spinlock.current, 1);
	atomicAddInt(&syncinfo.spinlock.initialized, 1);
#endif

	if (do_splockmeteron && (!name || strcmp(do_splockmeteron, name)))
		return;

	spinmeter_alloc(lp, name, SPINTYPE_SPIN);
}

/*
 * Free a spinlock
 */
void
spinlock_destroy(register lock_t *lp)
{
#if defined(DEBUG) || defined(SYNCINFO)
	atomicAddInt(&syncinfo.spinlock.current, -1);
#endif
	spinmeter_free((void *)lp);
}

/* ARGSUSED */
lock_t *
spinlock_alloc(register int vmflags, char *name)
{
	register lock_t *lp = (lock_t *)kmem_alloc(sizeof(lock_t), vmflags);
	if (lp)
		spinlock_init(lp, name);

	return lp;
}

void
spinlock_dealloc(register lock_t *lp)
{
	spinlock_destroy(lp);
	kmem_free(lp, sizeof(lock_t));
}

int
spinlock_initialized(register lock_t *sp)
{
	return *sp & SPIN_INIT;
}

int
spinlock_islocked(register lock_t *sp)
{
	return *sp & SPIN_LOCK;
}

static int
internal_mutex_spinlock(register lock_t *lck)
{
	register int ospl = splhi();
#ifndef SPLMETER
	while (!try_spinlock(lck)) {
		splx(ospl);
		ospl = splhi();
	}
#else
	while (!try_spinlock(lck))
		;
#endif /* SPLMETER */
	return ospl;
}

static void
internal_mutex_spinunlock(register lock_t *lck, int ospl)
{
	ASSERT(*lck & 1);
	*lck &= ~1;
	splx(ospl);
}

/*
 * Finally -- the external lock interfaces...
 */
int
mutex_spinlock(register lock_t *lck)
{
	register struct k_splockmeter *spm;
	register inst_t *save_ra = (inst_t *)__return_address;
	register int ospl = splhi();
	CHKCONDECL;

#ifdef SPLDEBUG
	do_splx_log(6, __return_address );
#endif
	private.p_curlock = (lock_type)lck;
	private.p_curlockcpc = save_ra;

#ifndef SPLMETER
	if (splockmeter && (spm = lockmeter_get((void *)lck))) {
		spinlockdebug(lck, spm, save_ra, ospl, splhi, splhi);
	} else {
		while (!try_spinlock(lck)) {
			splx(ospl);
			ospl = splhi();
			private.p_curlock = (lock_type)lck;
			private.p_curlockcpc = save_ra;
			CHKCON();
		}
	}
#else
	/* can't afford to call splhi,splx repeatedly */
	if (splockmeter && (spm = lockmeter_get((void *)lck))) {
		spinlockdebug(lck, spm, save_ra, 0, 0, splhi);
	} else {
		while (!try_spinlock(lck))
			;
	}
#endif /* ! SPLMETER */

	private.p_curlock = 0;
	private.p_lastlock = (lock_type)lck;

	return(ospl);
}

int
mutex_spinlock_spl(register lock_t *lck, register splfunc_t splr)
{
	register struct k_splockmeter *spm;
	register inst_t *save_ra = (inst_t *)__return_address;
	register int ospl = (*splr)();
	CHKCONDECL;

	private.p_curlock = (lock_type)lck;
	private.p_curlockcpc = save_ra;
#ifdef SPLDEBUG
	do_splx_log((int)(long long)splr, __return_address);
#endif

#ifndef SPLMETER
	if (splockmeter && (spm = lockmeter_get((void *)lck))) {
		spinlockdebug(lck, spm, save_ra, ospl, splr, splr);
	} else {
		while (!try_spinlock(lck)) {
			splx(ospl);
			ospl = (*splr)();
			private.p_curlock = (lock_type)lck;
			private.p_curlockcpc = save_ra;
			CHKCON();
		}
	}
#else
	/* can't afford to call splhi,splx repeatedly */
	if (splockmeter && (spm = lockmeter_get((void *)lck))) {
		spinlockdebug(lck, spm, save_ra, 0, 0, splr);
	} else {
		while (!try_spinlock(lck))
			;
	}
#endif /* ! SPLMETER */

	private.p_curlock = 0;
	private.p_lastlock = (lock_type)lck;

	return(ospl);
}

int
_nested_spintrylock(register lock_t *lck, inst_t *save_ra)
{
	register struct k_splockmeter *spm;

	private.p_curlock = (lock_type)lck;
	private.p_curlockcpc = save_ra;

	if (splockmeter && (spm = lockmeter_get((void *)lck))) {
		if (tryspinlockdebug(lck, spm, save_ra) == 0) {
			private.p_curlock = 0;
			return(0);
		}
	} else {
		if (!try_spinlock(lck)) {
			private.p_curlock = 0;
			return(0);
		}
	}

	private.p_curlock = 0;
	private.p_lastlock = (lock_type)lck;

	return(1);
}

int
nested_spintrylock(register lock_t *lck)
{
	ASSERT(!isspl0(getsr()));
	return(_nested_spintrylock(lck, (inst_t *)__return_address));
}

int
mutex_spintrylock(register lock_t *lck)
{
	register int ospl = splhi();

	if (!_nested_spintrylock(lck, (inst_t *)__return_address)) {
		splx(ospl);
		return(0);
	}
#ifdef EVEREST
	/* ospl can be zero on EVEREST: ensure non-zero on success */
	return ospl|OSPL_TRY1;
#else
	ASSERT(ospl);
	return ospl;
#endif
}

int
mutex_spintrylock_spl(register lock_t *lck, register splfunc_t splr)
{
	register int ospl = (*splr)();

	if (!_nested_spintrylock(lck, (inst_t *)__return_address)) {
		splx(ospl);
		return(0);
	}
#ifdef EVEREST
	/* ospl can be zero on EVEREST: ensure non-zero on success */
	return ospl|OSPL_TRY1;
#else
	ASSERT(ospl);
	return ospl;
#endif
}


void
nested_spinlock(register lock_t *lck)
{
	register struct k_splockmeter *spm;
	register inst_t *save_ra = (inst_t *)__return_address;
	CHKCONDECL;

	ASSERT(!isspl0(getsr()));

	private.p_curlock = (lock_type)lck;
	private.p_curlockcpc = save_ra;

	if (splockmeter && (spm = lockmeter_get((void *)lck))) {
		spinlockdebug(lck, spm, save_ra, 0, 0, 0);
	} else {
		while (!try_spinlock(lck))
			CHKCON();
	}

	private.p_curlock = 0;
	private.p_lastlock = (lock_type)lck;
}

/*
 * Release the given spinlock without involving priority levels.
 */
void
_mp_spinunlock(register lock_t *lck, inst_t *save_ra)
{
	register struct k_splockmeter *spm;

	if (splockmeter && (spm = lockmeter_get((void *)lck))) {
		spinunlockdebug(lck, spm, save_ra);
	} else {
		ASSERT(*lck & 1);
		*lck &= ~1;
	}
}

void
nested_spinunlock(register lock_t *lck)
{
	ASSERT(!isspl0(getsr()));
	_mp_spinunlock(lck, (inst_t *)__return_address);
}

/*
 * Release the given spinlock
 */
void
mutex_spinunlock(register lock_t *lck, int spl)
{
	_mp_spinunlock(lck, (inst_t *)__return_address);
	splx(spl);
#ifdef SPLDEBUG
	do_splx_log( spl, __return_address );
#endif	
}

void
mutex_io_spinunlock(register lock_t *lck, int spl)
{
	wbflush();
	_mp_spinunlock(lck, (inst_t *)__return_address);
	splx(spl);
}

static void
spinlockdebug(
	register lock_t *lck,
	register struct k_splockmeter *spm,
	register inst_t *ra,
	register int ospl,
	int (*splr)(void),
	int (*splrlog)(void))
{
	register int tries = 0;
	register unsigned ts;
	uint elapsed;
	int racers, once = 0;
	CHKCONDECL;

#ifdef DEBUG
	if (spm->m_owner == cpuid())
		debug("ring");
#endif
	ts = get_timestamp();

	atomicAddInt(&spm->m_gotfirst_num, 1);

	while (!try_spinlock(lck)) {
		tries++;
		if (!once) {
			once = 1;
			atomicAddInt(&spm->m_racers_num, 1);
			racers = atomicAddInt(&spm->m_racers, 1);
#ifdef SPLMETER
			store_if_greater(&spm->m_racers_max, racers);
#endif
			atomicAddInt(&spm->m_racers_total, racers);
		}		
		if ((__psint_t)splr != 0) {
			splx(ospl);
			ospl = (*splr)();
		}
		CHKCON();
	}

	if (once)
		atomicAddInt(&spm->m_racers, -1);
	else
		atomicAddInt(&spm->m_gotfirst, 1);
	spm->m_ts = get_timestamp();	/* start timer after got the lock */
	ra -= 2; /* want the caller not the return addr after the call */

	elapsed = spm->m_ts - ts;

	spm->m_totalwait += elapsed;
	if (spm->m_totalwait < elapsed) /* timer overflow */
	  cmn_err(CE_PANIC, "!spinlock @ 0x%x wait timer overflow!", lck);

	if (spm->m_waittime < (unsigned)elapsed) {
		spm->m_waittime = (unsigned)elapsed;
		spm->m_waitpc = ra;
		spm->m_waitcpu = cpuid();
		spm->m_splevel = (void*)splrlog;
	}

	ASSERT(spm->m_owner == OWNER_NONE);
	spm->m_owner = cpuid();
	ASSERT(spm->m_lcnt == spm->m_ucnt);
	spm->m_lcnt++;
	if (tries > 0)
		spm->m_wait++;
	spm->m_lastlock = ra;
}

static int
tryspinlockdebug(
	register lock_t *lck,
	register struct k_splockmeter *spm,
	register inst_t *ra)
{
	ASSERT(spm->m_owner != cpuid());

	if (!try_spinlock(lck)) {
		spm->m_wait++;
		return(0);
	}
	spm->m_ts = get_timestamp();
	ASSERT(spm->m_owner == OWNER_NONE);
	spm->m_owner = cpuid();
	ASSERT(spm->m_lcnt == spm->m_ucnt);
	spm->m_lcnt++;
	spm->m_lastlock = ra;
	return(1);
}

static void
spinunlockdebug(
	lock_t *lck,
	register struct k_splockmeter *spm,
	register inst_t *ra)
{
	register unsigned ts = get_timestamp();
	uint elapsed;

	ASSERT(*lck & 1);

	if (spm->m_owner != cpuid()) {
		ASSERT(spm->m_owner == OWNER_NONE &&
			spm->m_lcnt == spm->m_ucnt);
		spm->m_lcnt++;
		cmn_err(CE_WARN, "spinlock @ 0x%x owner inconsistent!", lck);
	}

	spm->m_ucnt++;
	ASSERT(spm->m_lcnt == spm->m_ucnt);
	if (spm->m_owner != cpuid()) {
		ASSERT(spm->m_owner == OWNER_NONE);
		cmn_err(CE_WARN, "spinlock @ 0x%x owner inconsistent!", lck);
	}

	spm->m_owner = OWNER_NONE;

	elapsed = ts - spm->m_ts;

	spm->m_totalheld += elapsed;
	if (spm->m_totalheld < elapsed) /* timer overflow */
		cmn_err(CE_PANIC, "!spinlock @ 0x%x hold timer overflow!", lck);

	if (spm->m_elapsed < (unsigned)elapsed) {
		spm->m_elapsed = (unsigned)elapsed;
		spm->m_elapsedpc = spm->m_lastlock;
		spm->m_elapsedpcfree = ra - 2;
		spm->m_elapsedcpu = cpuid();
	}
	spm->m_lastunlock = ra - 2;

	*lck &= ~1;
}

int try_bitlock(uint *, uint);
static void bitlockdebug(uint *, uint, struct k_splockmeter *, inst_t *,
			  int, int (*)(void), int (*)(void));
static void bitunlockdebug(uint *, uint, struct k_splockmeter *, inst_t *);

/*
 * Initialize an integer bitlock
 *
 * Current bitlock users: mrlocks, vnodes, proc structs, rnodes.
 */
void
meter_bitlock(register uint *lp, char *name)
{
	spinmeter_alloc(lp, name, SPINTYPE_BIT);
}

void
init_bitlock(register uint *lck, uint bit, char *name, long sequence)
{
	char namebuf[METER_NAMSZ];

	/*
	 * Initialize bitlock.  Preserve existing bits???
	 */
	*lck &= ~bit;
#if defined(DEBUG) || defined(SYNCINFO)
	atomicAddInt(&syncinfo.bitlock.current, 1);
	atomicAddInt(&syncinfo.bitlock.initialized, 1);
#endif

	if (do_splockmeteron && (!name || strcmp(do_splockmeteron, name)))
		return;

	spinmeter_alloc(lck, makesname(namebuf, name, sequence), SPINTYPE_BIT);
}

void
destroy_bitlock(register uint *lck)
{
#if defined(DEBUG) || defined(SYNCINFO)
	atomicAddInt(&syncinfo.bitlock.current, -1);
#endif
	spinmeter_free((void *)lck);
}

int
bitlock_islocked(register uint_t *lock, register uint_t bit)
{
	return (*lock & bit) && issplhi(getsr());
}


int
mutex_bitlock_spl(register uint *lck, uint bit, register splfunc_t splr)
{
	register int ospl;
	register inst_t *save_ra = (inst_t *)__return_address;
	register struct k_splockmeter *spm;
	CHKCONDECL;

#ifdef SPLDEBUG
	do_splx_log((int)(long long)splr, __return_address);
#endif 
#ifndef SPLMETER
	if (splockmeter && (spm = lockmeter_get((void *)lck))) {
		ospl = (*splr)();
		private.p_curlock = (lock_type)lck;
		private.p_curlockcpc = save_ra;
		bitlockdebug(lck, bit, spm, save_ra, ospl, splr, splr);
	} else {
		ospl = (*splr)();
		private.p_curlock = (lock_type)lck;
		private.p_curlockcpc = save_ra;
		while (!try_bitlock(lck, bit)) {
			splx(ospl);
			ospl = (*splr)();
			private.p_curlock = (lock_type)lck;
			private.p_curlockcpc = save_ra;
			CHKCON();
		}
	}
#else
	if (splockmeter && (spm = lockmeter_get((void *)lck))) {
		/*
		 * can't afford to call splhi,splx repeatedly
		 */
		ospl = (*splr)();
		private.p_curlock = (lock_type)lck;
		private.p_curlockcpc = save_ra;
		bitlockdebug(lck, bit, spm, save_ra, 0, 0, splr);
	} else {
		ospl = (*splr)();
		private.p_curlock = (lock_type)lck;
		private.p_curlockcpc = save_ra;
		while (!try_bitlock(lck, bit)) {
			;
		}
	}
#endif /* ! SPLMETER */

	private.p_curlock = 0;
	private.p_lastlock = (lock_type)lck;

	return(ospl);
}

int
mutex_bitlock(register uint *lck, uint bit)
{
	register int ospl;
	register inst_t *save_ra = (inst_t *)__return_address;
	register struct k_splockmeter *spm;
	CHKCONDECL;

#ifndef SPLMETER
	if (splockmeter && (spm = lockmeter_get((void *)lck))) {
		ospl = splhi();
		private.p_curlock = (lock_type)lck;
		private.p_curlockcpc = save_ra;
		bitlockdebug(lck, bit, spm, save_ra, ospl, splhi, splhi);
	} else {
		ospl = splhi();
		private.p_curlock = (lock_type)lck;
		private.p_curlockcpc = save_ra;
		while (!try_bitlock(lck, bit)) {
			splx(ospl);
			ospl = splhi();
			CHKCON();
		}
	}
#else
	/* can't afford to call splhi,splx repeatedly */
	if (splockmeter && (spm = lockmeter_get((void *)lck))) {
		ospl = splhi();
		private.p_curlock = (lock_type)lck;
		private.p_curlockcpc = save_ra;
		bitlockdebug(lck, bit, spm, save_ra, 0, 0, splhi);
	} else {
		ospl = splhi();
		private.p_curlock = (lock_type)lck;
		private.p_curlockcpc = save_ra;
		while (!try_bitlock(lck, bit)) {
			;
		}
	}
#endif /* ! SPLMETER */

	private.p_curlock = 0;
	private.p_lastlock = (lock_type)lck;

	return(ospl);
}

static int
_nested_bittrylock(register uint *lck, uint bit, inst_t *save_ra)
{
	register struct k_splockmeter *spm;

	private.p_curlock = (lock_type)lck;
	private.p_curlockcpc = save_ra;

	if (splockmeter && (spm = lockmeter_get((void *)lck))) {
		if (trybitlockdebug(lck, bit, spm, save_ra) == 0) {
			private.p_curlock = 0;
			return(0);
		}
	} else {
		if (!try_bitlock(lck, bit)) {
			private.p_curlock = 0;
			return(0);
		}
	}

	private.p_curlock = 0;
	private.p_lastlock = (lock_type)lck;

	return(1);
}

int
nested_bittrylock(register uint *lck, uint bit)
{
	ASSERT(!isspl0(getsr()));
	return _nested_bittrylock(lck, bit, (inst_t *)__return_address);
}

int
mutex_bittrylock(register uint *lck, uint bit)
{
	register int ospl;

	ospl = splhi();
	if (_nested_bittrylock(lck, bit, (inst_t *)__return_address) == 0) {
		splx(ospl);
		return 0;
	}
#ifdef EVEREST
	/* ospl can be zero on EVEREST: ensure non-zero on success */
	return ospl|OSPL_TRY1;
#else
	ASSERT(ospl);
	return ospl;
#endif
}

/*
 * Acquire the given spinlock, with no spl involved.
 */
void
nested_bitlock(register uint *lck, uint bit)
{
	register inst_t *save_ra = (inst_t *)__return_address;
	register struct k_splockmeter *spm;
	CHKCONDECL;

	ASSERT(!isspl0(getsr()));
	private.p_curlock = (lock_type)lck;
	private.p_curlockcpc = save_ra;

	if (splockmeter && (spm = lockmeter_get((void *)lck))) {
		bitlockdebug(lck, bit, spm, save_ra, 0, 0, 0);
	} else {
		while (!try_bitlock(lck, bit)) {
			CHKCON();
		}
	}

	private.p_curlock = 0;
	private.p_lastlock = (lock_type)lck;
}

/*
 * Release the given spinlock, with no spl involved.
 */
void
nested_bitunlock(register uint *lck, uint bit)
{
	register struct k_splockmeter *spm;
	ASSERT(issplhi(getsr()));

	if (splockmeter && (spm = lockmeter_get((void *)lck))) {
		bitunlockdebug(lck, bit, spm, (inst_t *)__return_address);
	} else {
		ASSERT(*lck & bit);
		*lck &= ~bit;
	}
}

/*
 * Release the given spinlock
 */
void
mutex_bitunlock(register uint *lck, uint bit, int spl)
{
	register struct k_splockmeter *spm;
	ASSERT(issplhi(getsr()));

	if (splockmeter && (spm = lockmeter_get((void *)lck))) {
		bitunlockdebug(lck, bit, spm, (inst_t *)__return_address);
	} else {
		ASSERT(*lck & bit);
		*lck &= ~bit;
	}

	splx(spl);
#ifdef SPLDEBUG
	do_splx_log(spl, __return_address);
#endif
}

static void
bitlockdebug(
	register uint *lck,
	register uint bit,
	register struct k_splockmeter *spm,
	register inst_t *ra,
	register int ospl,
	int (*splr)(void),
	int (*splrlog)(void))
{
	register int tries = 0;
	register unsigned ts;
	uint elapsed;
	int racers, once = 0;
	CHKCONDECL;

	ASSERT(spm->m_owner != cpuid());
	ts = get_timestamp();

	atomicAddInt(&spm->m_gotfirst_num, 1);

	while (!try_bitlock(lck, bit)) {
		tries++;
		if (!once) {
			once = 1;
			atomicAddInt(&spm->m_racers_num, 1);
			racers = atomicAddInt(&spm->m_racers, 1);
#ifdef SPLMETER
			store_if_greater(&spm->m_racers_max, racers);
#endif
			atomicAddInt(&spm->m_racers_total, racers);
		}		
		if ((__psint_t)splr != 0) {
			splx(ospl);
			ospl = (*splr)();
		}
		CHKCON();
	}
	if (once)
		atomicAddInt(&spm->m_racers, -1);
	else
		atomicAddInt(&spm->m_gotfirst, 1);

	spm->m_ts = get_timestamp();	/* start timer after got the lock */
	ra -= 2; /* want the caller not the return addr after the call */

	elapsed = spm->m_ts - ts;

	spm->m_totalwait += elapsed;
	if (spm->m_totalwait < elapsed) /* timer overflow */
		cmn_err(CE_PANIC, "!spinlock @ 0x%x wait timer overflow!", lck);

	if (spm->m_waittime < (unsigned)elapsed) {
		spm->m_waittime = (unsigned)elapsed;
		spm->m_waitpc = ra;
		spm->m_waitcpu = cpuid();
		spm->m_splevel = (void*)splrlog;
	}

	ASSERT(spm->m_owner == OWNER_NONE);
	spm->m_owner = cpuid();
	ASSERT(spm->m_lcnt == spm->m_ucnt);
	spm->m_lcnt++;
	if (tries > 0)
		spm->m_wait++;
	spm->m_lastlock = ra;
}

static int
trybitlockdebug(
	register uint *lck,
	uint bit,
	struct k_splockmeter *spm,
	register inst_t *ra)
{
	ASSERT(spm->m_owner != cpuid());

	if (!try_bitlock(lck, bit)) {
		spm->m_wait++;
		return(0);
	}
	spm->m_ts = get_timestamp();	/* start timer after got the lock */
	ASSERT(spm->m_owner == OWNER_NONE);
	spm->m_owner = cpuid();
	ASSERT(spm->m_lcnt == spm->m_ucnt);
	spm->m_lcnt++;
	spm->m_lastlock = ra;
	return(1);
}

static void
bitunlockdebug(
	register uint *lck,
	uint bit,
	struct k_splockmeter *spm,
	register inst_t *ra)
{
	register unsigned ts = get_timestamp();
	uint elapsed;

	ASSERT(*lck & bit);

	if (spm->m_owner != cpuid()) {
		ASSERT(spm->m_owner == OWNER_NONE &&
			spm->m_lcnt == spm->m_ucnt);
		spm->m_lcnt++;
		cmn_err(CE_WARN, "bitlock @ 0x%x owner inconsistent!", lck);
	}

	spm->m_ucnt++;
	ASSERT(spm->m_lcnt == spm->m_ucnt);

	spm->m_owner = OWNER_NONE;

	elapsed = ts - spm->m_ts;;

	spm->m_totalheld += elapsed;
	if (spm->m_totalheld < elapsed) /* timer overflow */
	  cmn_err(CE_PANIC, "!spinlock @ 0x%x hold timer overflow!", lck);

	if (spm->m_elapsed < (unsigned)elapsed) {
		spm->m_elapsed = (unsigned)elapsed;
		spm->m_elapsedpc = spm->m_lastlock;
		spm->m_elapsedpcfree = ra - 2;
		spm->m_elapsedcpu = cpuid();
	}
	spm->m_lastunlock = ra - 2;

	*lck &= ~bit;
}

int try_64bitlock(__uint64_t *, __uint64_t);
static void _64bitlockdebug(__uint64_t *, __uint64_t, struct k_splockmeter *,
			inst_t *, int, int (*)(void), int (*)(void));
static void _64bitunlockdebug(__uint64_t *, __uint64_t, struct k_splockmeter *,
			inst_t *);
static int try64bitlockdebug(__uint64_t *, __uint64_t, struct k_splockmeter *,
			inst_t *);

void
meter_64bitlock(register __uint64_t *lp, char *name)
{
	spinmeter_alloc(lp, name, SPINTYPE_64BIT);
}

/*
 * Initialize a 64-bit bitlock
 */
void
init_64bitlock(register __uint64_t *lck,
		__uint64_t bit,
		char *name,
		long sequence)
{
	char namebuf[METER_NAMSZ];

	/*
	 * Initialize bitlock.  Preserve existing bits???
	 */
	*lck &= ~bit;
#if defined(DEBUG) || defined(SYNCINFO)
	atomicAddInt(&syncinfo.bitlock64.current, 1);
	atomicAddInt(&syncinfo.bitlock64.initialized, 1);
#endif

	if (do_splockmeteron && (!name || strcmp(do_splockmeteron, name)))
		return;

	spinmeter_alloc(lck, makesname(namebuf, name, sequence),
			SPINTYPE_64BIT);
}

void
destroy_64bitlock(register __uint64_t *lck)
{
#if defined(DEBUG) || defined(SYNCINFO)
	atomicAddInt(&syncinfo.bitlock64.current, -1);
#endif
	spinmeter_free((void *)lck);
}

int
bitlock64bit_islocked(register __uint64_t *lock, register uint_t bit)
{
	return (*lock & bit) && issplhi(getsr());
}

int
mutex_64bitlock(register __uint64_t *lck, __uint64_t bit)
{
	register int ospl;
	register inst_t *save_ra = (inst_t *)__return_address;
	register struct k_splockmeter *spm;
	CHKCONDECL;

	ASSERT(lck);
#ifndef SPLMETER
	if (splockmeter && (spm = lockmeter_get((void *)lck))) {
		ospl = splhi();
		private.p_curlock = (lock_type)lck;
		private.p_curlockcpc = save_ra;
		_64bitlockdebug(lck, bit, spm, save_ra, ospl, splhi, splhi);
	} else {
		ospl = splhi();
		private.p_curlock = (lock_type)lck;
		private.p_curlockcpc = save_ra;
		while (!try_64bitlock(lck, bit)) {
			splx(ospl);
			ospl = splhi();
			private.p_curlock = (lock_type)lck;
			private.p_curlockcpc = save_ra;
			CHKCON();
		}
	}
#else
	/* can't afford to call splhi,splx repeatedly */
	if (splockmeter && (spm = lockmeter_get((void *)lck))) {
		ospl = splhi();
		private.p_curlock = (lock_type)lck;
		private.p_curlockcpc = save_ra;
		_64bitlockdebug(lck, bit, spm, save_ra, 0, 0, splhi);
	} else {
		ospl = splhi();
		private.p_curlock = (lock_type)lck;
		private.p_curlockcpc = save_ra;
		while (!try_64bitlock(lck, bit)) {
			;
		}
	}
#endif /* ! SPLMETER */

	private.p_curlock = 0;
	private.p_lastlock = (lock_type)lck;

	return(ospl);
}

static int
_nested_64bittrylock(register __uint64_t *lck, __uint64_t bit, inst_t *save_ra)
{
	register struct k_splockmeter *spm;

	private.p_curlock = (lock_type)lck;
	private.p_curlockcpc = save_ra;

	ASSERT(lck);
	if (splockmeter && (spm = lockmeter_get((void *)lck))) {
		if (try64bitlockdebug(lck, bit, spm, save_ra) == 0) {
			private.p_curlock = 0;
			return(0);
		}
	} else {
		if (!try_64bitlock(lck, bit)) {
			private.p_curlock = 0;
			return(0);
		}
	}

	private.p_curlock = 0;
	private.p_lastlock = (lock_type)lck;

	return(1);
}

int
nested_64bittrylock(register __uint64_t *lck, __uint64_t bit)
{
	ASSERT(!isspl0(getsr()));
	return _nested_64bittrylock(lck, bit, (inst_t *)__return_address);
}

int
mutex_64bittrylock(register __uint64_t *lck, __uint64_t bit)
{
	register int ospl;

	ospl = splhi();
	if (_nested_64bittrylock(lck, bit, (inst_t *)__return_address) == 0) {
		splx(ospl);
		return 0;
	}
#ifdef EVEREST
	/* ospl can be zero on EVEREST: ensure non-zero on success */
	return ospl|OSPL_TRY1;
#else
	ASSERT(ospl);
	return ospl;
#endif
}


/*
 * Acquire the given spinlock, with no spl involved.
 */
void
nested_64bitlock(register __uint64_t *lck, __uint64_t bit)
{
	register struct k_splockmeter *spm;
	register inst_t *save_ra = (inst_t *)__return_address;
	CHKCONDECL;

	ASSERT(issplhi(getsr()));
	private.p_curlock = (lock_type)lck;
	private.p_curlockcpc = save_ra;

	ASSERT(lck);
	if (splockmeter && (spm = lockmeter_get((void *)lck))) {
		_64bitlockdebug(lck, bit, spm, save_ra, 0, 0, 0);
	} else {
		while (!try_64bitlock(lck, bit)) {
			CHKCON();
		}
	}

	private.p_curlock = 0;
	private.p_lastlock = (lock_type)lck;
}

void
nested_64bitunlock(register __uint64_t *lck, __uint64_t bit)
{
	register struct k_splockmeter *spm;
	ASSERT(issplhi(getsr()));

	if (splockmeter && (spm = lockmeter_get((void *)lck))) {
		_64bitunlockdebug(lck, bit, spm, (inst_t *)__return_address);
	} else {
		ASSERT(*lck & bit);
		*lck &= ~bit;
	}
}

/*
 * Release the given spinlock
 */
void
mutex_64bitunlock(register __uint64_t *lck, __uint64_t bit, int spl)
{
	register struct k_splockmeter *spm;
	ASSERT(issplhi(getsr()));

	if (splockmeter && (spm = lockmeter_get((void *)lck))) {
		_64bitunlockdebug(lck, bit, spm, (inst_t *)__return_address);
	} else {
		ASSERT(*lck & bit);
		*lck &= ~bit;
	}
	splx(spl);
}

static void
_64bitlockdebug(
	register __uint64_t *lck,
	register __uint64_t bit,
	register struct k_splockmeter *spm,
	register inst_t *ra,
	register int ospl,
	int (*splr)(void),
	int (*splrlog)(void))
{
	register int tries = 0;
	register unsigned ts;
	uint elapsed;
	int racers, once = 0;
	CHKCONDECL;

	ASSERT(spm->m_owner != cpuid());
	ts = get_timestamp();

	atomicAddInt(&spm->m_gotfirst_num, 1);

	ASSERT(lck);
	while (!try_64bitlock(lck, bit)) {
		tries++;
		if (!once) {
			once = 1;
			atomicAddInt(&spm->m_racers_num, 1);
			racers = atomicAddInt(&spm->m_racers, 1);
#ifdef SPLMETER
			store_if_greater(&spm->m_racers_max, racers);
#endif
			atomicAddInt(&spm->m_racers_total, racers);
		}		
		if ((__psint_t)splr != 0) {
			splx(ospl);
			ospl = (*splr)();
		}
		CHKCON();
	}
	if (once)
		atomicAddInt(&spm->m_racers, -1);
	else
		atomicAddInt(&spm->m_gotfirst, 1);

	spm->m_ts = get_timestamp();	/* start timer after got the lock */
	ra -= 2; /* want the caller not the return addr after the call */

	elapsed = spm->m_ts - ts;

	spm->m_totalwait += elapsed;
	if (spm->m_totalwait < elapsed) /* timer overflow */
	  cmn_err(CE_PANIC, "!spinlock @ 0x%x wait timer overflow!", lck);

	if (spm->m_waittime < (unsigned)elapsed) {
		spm->m_waittime = (unsigned)elapsed;
		spm->m_waitpc = ra;
		spm->m_waitcpu = cpuid();
		spm->m_splevel = (void*)splrlog;
	}

	ASSERT(spm->m_owner == OWNER_NONE);
	spm->m_owner = cpuid();
	ASSERT(spm->m_lcnt == spm->m_ucnt);
	spm->m_lcnt++;
	if (tries > 0)
		spm->m_wait++;
	spm->m_lastlock = ra;
}

static int
try64bitlockdebug(
	register __uint64_t *lck,
	register __uint64_t bit,
	struct k_splockmeter *spm,
	register inst_t *ra)
{
	ASSERT(spm->m_owner != cpuid());

	ASSERT(lck);
	if (!try_64bitlock(lck, bit)) {
		spm->m_wait++;
		return(0);
	}
	ASSERT(spm->m_owner == OWNER_NONE);
	spm->m_owner = cpuid();
	ASSERT(spm->m_lcnt == spm->m_ucnt);
	spm->m_lcnt++;
	spm->m_lastlock = ra;
	return(1);
}

static void
_64bitunlockdebug(
	register __uint64_t *lck,
	register __uint64_t bit,
	struct k_splockmeter *spm,
	register inst_t *ra)
{
	register unsigned ts = get_timestamp();
	uint elapsed;

	ASSERT(*lck & bit);

	if (spm->m_owner != cpuid()) {
		ASSERT(spm->m_owner == OWNER_NONE &&
			spm->m_lcnt == spm->m_ucnt);
		spm->m_lcnt++;
		cmn_err(CE_WARN, "64bitlock @ 0x%x owner inconsistent!", lck);
	}

	spm->m_ucnt++;
	ASSERT(spm->m_lcnt == spm->m_ucnt);

	spm->m_owner = OWNER_NONE;

	elapsed = ts - spm->m_ts;;

	spm->m_totalheld += elapsed;
	if (spm->m_totalheld < elapsed) /* timer overflow */
	  cmn_err(CE_PANIC, "!spinlock @ 0x%x hold timer overflow!", lck);

	if (spm->m_elapsed < (unsigned)elapsed) {
		spm->m_elapsed = (unsigned)elapsed;
		spm->m_elapsedpc = spm->m_lastlock;
		spm->m_elapsedpcfree = ra - 2;
		spm->m_elapsedcpu = cpuid();
	}
	spm->m_lastunlock = ra - 2;

	*lck &= ~bit;
}

#else
extern int tick;	/* shut up ANSI C */
#endif /* MP */
