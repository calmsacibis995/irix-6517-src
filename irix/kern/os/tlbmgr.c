/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1996, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.126 $"

#include <sys/cmn_err.h>
#include <sys/types.h>
#include <sys/atomic_ops.h>
#include "os/as/as_private.h"
#include <sys/debug.h>
#include <sys/immu.h>
#include <sys/idbgentry.h>
#include <sys/kmem.h>
#include <sys/ktrace.h>
#include <sys/map.h>
#include <sys/par.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/prctl.h>
#include <sys/proc.h>
#include <sys/reg.h>
#include <sys/runq.h>
#include <sys/sysinfo.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/timers.h>
#include <sys/var.h>
#include <sys/lpage.h>

static int clean_aged_sptmap(struct sysbitmap *);

/* If there are no isolated processors in the system we use tlbflushmask
 * to track tlbsync completion.  If there are isolated processors we use
 * tlbflushcount.  In each case this minimizes the number of cache misses
 * though the isolated processor algorithm takes longer.
 */
#ifdef EVEREST
volatile cpumask_t tlbflushmask;
#endif /* EVEREST */

/* cacheline aligned variables for performance, especially on SN0 */
hotIntCounter_t	tlbflushcount;
hotIntCounter_t	tlbcleancount;

/*
 * Put these counters in cachelines by themselves. 
*/
#if MP
#pragma fill_symbol (tlbflushcount, L2cacheline)
#pragma fill_symbol (tlbcleancount, L2cacheline)
#endif /* MP */


static int64_t	tlbids[1024];
#define	tlbid	tlbids[512]
#if 0
static int64_t	tlbid = 1;	/* each uthread gets it own unique id */
#endif
extern cpumask_t isolate_cpumask;
extern int clrsz;
extern int sptsz;

extern void tlbflush(uint64_t);
static void tlbunmap(pas_t *, __psunsigned_t vpn);
extern void dump_tlbinfo(int);

static struct zone *tlbpid_zone = NULL;

#if TLB_TRACE && DEBUG
static void idbg_tlbtrace(__psunsigned_t);
static void idbg_kvfault(__psunsigned_t);
static void testtlb(void);
struct ktrace *tlb_trace_buf;
static void chkmap(struct bitmap *bm);
static void chkkvfault(struct sysbitmap *bm, cpuid_t id);
static int dochkkvfault = 0;
static int dotesttlb = 0;
static int dochkmap = 0;
#endif

/*
 * Software structures for tlb management.
 */
typedef struct tlbpid_item_s tlbpid_item_t;

struct tlbpid_item_s {
	tlbpid_item_t	*ti_next;
};

typedef struct tlbinfo {
	/* For tlbpid management */
	int64_t		ti_lastuser[TLBHI_NPID];
	tlbpid_item_t	*ti_tail;
	tlbpid_item_t	ti_item[TLBHI_NPID];
	tlbpid_item_t	*ti_prev[TLBHI_NPID];
	int		ti_flush;
	int		ti_nextinval;

	/* For K2SEG management */
	int		ti_tlbflush_tickcnt;	/* ticks until next flush */
	int		ti_lastindex;	/* tlb index last flushed */
	int		ti_count;	/* count of tlb entries flushed */
#ifdef ULI
	/* For private tlbpid allocation */
	int		ti_total_pids;
	int		ti_flush_interval;
#endif
} tlbinfo_t;

#define NO_LASTUSER ((int64_t)-1)

/* Use highest tlbpid value to indicate "no tlbpid" */
#define TLBPID_NONE ((tlbpid_t)(-1))

#ifdef IP32
/* Set the rates based on the booting cpu */
static int tlb_flush_rate, clean_aged_rate;
#endif

/*
 * Recycle old dirty pages every CLEAN_AGED_RATE seconds.
 *
 * Flush one tlb entry every TLB_FLUSH_RATE ticks.
 * TLB_FLUSH_RATE should be set such that we make it comfortably through
 * the entire tlb once every CLEAN_AGED_RATE seconds.

 * Once every CLEAN_AGED_RATE seconds, if all of the processors have
 * cycled through all their random tlbs, we merge all of the aged_stale
 * pages onto the clean list, and allow the next batch of stale pages to
 * age for CLEAN_AGED_RATE seconds.  If all of the processors aren't
 * ready, we'll try again next second.
 */
#if R4000 && !R10000
#define TLB_FLUSH_RATE 9	/* 40-42 entries * 90ms = 3.60-3.78 seconds */
#define CLEAN_AGED_RATE 4
#elif R10000 && R4000
#define R4K_TLB_FLUSH_RATE 9	/* 40-42 entries * 90ms = 3.60-3.78 seconds */
#define R4K_CLEAN_AGED_RATE 4
#endif
#if R10000 && !R4000
#define TLB_FLUSH_RATE 8	/* 58 entries * 80ms = 4.64 seconds */
#define CLEAN_AGED_RATE 5
#elif R10000 && R4000
#define R10K_TLB_FLUSH_RATE 8	/* 58 entries * 80ms = 4.64 seconds */
#define R10K_CLEAN_AGED_RATE 5
#define TLB_FLUSH_RATE	tlb_flush_rate
#define CLEAN_AGED_RATE	clean_aged_rate
#endif
#if TFP
#define TLB_FLUSH_RATE 2	/* 192 entries * 20ms = 3.84 seconds */
#define CLEAN_AGED_RATE 4
#endif
#if BEAST
#define TLB_FLUSH_RATE 1	/* 1024 entries * 10ms = 10.24 seconds */
#define CLEAN_AGED_RATE 11
#endif

static	unsigned int clean_aged;
#ifndef DEBUG
static
#endif
	unsigned int reset_aged;


void
tlb_onesec_actions(void)
{
	int	i;
	int	flush = 0;

	ASSERT(private.p_flags & PDAF_CLOCK);
	/*
	 * It's been long enough since we recycled used kernel
	 * virtual pages.  Note that clean_aged gets reset to zero
	 * in clean_aged_sptmap (if it suceeds) and now in
	 * clean_stale_sptmap, since clean_stale_sptmap now flushes
	 * the aged maps, too.
	 */
	if (reset_aged) {
		reset_aged = 0;
		flush = 1;
	} else if (++clean_aged >= CLEAN_AGED_RATE)
		flush = clean_aged_sptmap(&sptbitmap);
	if (flush) {
		/*
		 * Restart flush counters from 0.
		 */
		for (i = 0; i < maxcpus; i++) {
			if ((pdaindr[i].CpuId == -1) ||
			    (pdaindr[i].pda->p_tlbinfo == NULL))
				continue;

			pdaindr[i].pda->p_tlbinfo->ti_count = 0;
		}

		clean_aged = 0;
	}
#if TLB_TRACE && DEBUG
	if (dochkmap)
		chkmap(&sptbitmap.sptmap);
#endif
}

/*
 * Per-tick tlb maintenance activities.
 */
void
tlb_tick_actions(void)
{
	register tlbinfo_t *tip = private.p_tlbinfo;
	/*
	 * This is part of the heuristic that supports recycling of
	 * kernel virtual addresses.  Here, we implement a gradual
	 * tlb flush.
	 * We used to use (lbolt % TLB_FLUSH_RATE), but since we're not synced
	 * to the master PDAF_CLOCK we might miss the lbolt value we're
	 * supposed to flush on.  We now use a per cpu tlb flush tick counter
	 * so we don't miss any flushes (and we also avoid a divide).
	 */
	tip->ti_tlbflush_tickcnt--;
	if (tip->ti_tlbflush_tickcnt == 0) {
		register int lastindex = tip->ti_lastindex;

		/*
		 * Gradually flush entire non-wired tlb.  This allows 
		 * us to occasionally recycle stale kernel virtual 
		 * addresses (by calling "clean_aged_sptmap")
		 */
		tip->ti_tlbflush_tickcnt = TLB_FLUSH_RATE;
		++lastindex;
#ifdef JUMP_WAR
		if (lastindex >= getwired())
#endif

		invaltlb(lastindex);
		if (lastindex == (NTLBENTRIES - 1)) {
#ifndef JUMP_WAR
			tip->ti_lastindex = TLBRANDOMBASE - 1;
#else
			tip->ti_lastindex = getwired() - 1;
#endif
		} else
			tip->ti_lastindex = lastindex;

		/* Need atomic update since other processors may
		 * clear the ti_count when stale sptmap is reset.
		 */
		atomicAddInt(&tip->ti_count, 1);
	}
	return;
}


/*
 * Initialize tlbpid management structures.
 */
void
tlbinfo_init(void)
{
	tlbinfo_t *tip;
	int i;
	static int tlbinitted = 0;

	if (!tlbinitted) {
		tlbinitted = 1;
		tlbid = 1;
#if TLB_TRACE && DEBUG
		tlb_trace_buf = ktrace_alloc(512, 1);
		idbg_addfunc("tracetlb", idbg_tlbtrace);
		idbg_addfunc("kvfault", idbg_kvfault);
#endif
	}


	tip = private.p_tlbinfo = 
		(tlbinfo_t *)kmem_alloc(sizeof(tlbinfo_t), KM_NOSLEEP);
	ASSERT(tip); 

	/* Set up LRU initial state */
	for (i = 0; i < TLBHI_NPID; i++) {
		tip->ti_lastuser[i] = NO_LASTUSER;
		tip->ti_item[i].ti_next = &(tip->ti_item[i+1]);
		tip->ti_prev[i] = &(tip->ti_item[i-1]);
	}

	/* 0 is unused, so pull it off the list */
	tip->ti_item[TLBHI_NPID-1].ti_next = &(tip->ti_item[1]);
	tip->ti_prev[1] = &(tip->ti_item[TLBHI_NPID-1]);

	/* Good hygiene: clear unused fields */
	tip->ti_prev[0] = NULL;	
	tip->ti_item[0].ti_next = NULL;
#ifdef IP32
	if (IS_R10000()) {
		TLB_FLUSH_RATE = R10K_TLB_FLUSH_RATE;
		CLEAN_AGED_RATE = R10K_CLEAN_AGED_RATE;
	} else {
		TLB_FLUSH_RATE = R4K_TLB_FLUSH_RATE;
		CLEAN_AGED_RATE = R4K_CLEAN_AGED_RATE;
	}
#endif
	/* Verify a basic assumption about this algorithm */
	ASSERT(TLBHI_NPID >= NTLBENTRIES-TLBRANDOMBASE);
	tip->ti_flush = 0;
	tip->ti_nextinval = TLBRANDOMBASE;
	tip->ti_tlbflush_tickcnt = TLB_FLUSH_RATE;

	tip->ti_tail = &(tip->ti_item[TLBHI_NPID-1]);

	/* Initialize data for K2SEG management */
	tip->ti_lastindex = TLBRANDOMBASE - 1;
	tip->ti_count = 0;
	reset_aged = 1;		/* restart all aged counters */

#ifdef ULI
	/* Initialize private pid allocation data */
	tip->ti_total_pids = TLBHI_NPID;
	tip->ti_flush_interval = TLBHI_NPID / (NTLBENTRIES - TLBRANDOMBASE);
#endif

	if (tlbpid_zone == NULL)
		tlbpid_zone = kmem_zone_init(sizeof(tlbpid_t) * maxcpus,
						"tlb pids");

#if defined(DEBUG) || defined(PIDDEBUG)
	if (cpuid() == 0)
		idbg_addfunc("tlbinfo", dump_tlbinfo);
#endif
}


/*
 * each uthread needs a unique tlbid ...
 */
int64_t
newtlbid(void)
{
	return atomicAddInt64(&tlbid, 1);
}

/*
 * See if thread has a valid tlbpid which hasn't been stolen by some
 * other thread running on this processor.
 */
int
tlbpid_is_usable(utas_t *utas) 
{
	ASSERT_NOMIGRATE;
	return((tlbpid(utas) != TLBPID_NONE) && 
		(private.p_tlbinfo->ti_lastuser[tlbpid(utas)] == utas->utas_tlbid) ||
		(tlbpid(utas) == 0));
}

/*
 * Specified thread will use the specified tlbpid, so move tlbpid to
 * end of LRU list.  Every couple of times through tlbpid_use, zap one
 * random tlb entry.  This insures that by the time we cycle through
 * all the tlbpids, the entire tlb will have been flushed.
 *
 * We don't do anything special to handle wired tlb entries, since the
 * resume code flushes these on every context switch.
 */
void
tlbpid_use(utas_t *utas, tlbpid_t tlbpid)
{
	tlbinfo_t *tip; 
	tlbpid_item_t *prevp, *myp, *tail;

	/*
	 * System processes use tlbpid 0, which is not managed by this code.
	 */
	if (tlbpid == 0)
		return;

	ASSERT(tlbpid != TLBPID_NONE);
	/* Better not be able to migrate to another cpu */
	ASSERT_NOMIGRATE;

	tip = private.p_tlbinfo;
	tip->ti_lastuser[tlbpid] = utas->utas_tlbid;

	prevp = tip->ti_prev[tlbpid];
	myp = prevp->ti_next;

#if PIDDEBUG
	if (myp != &(tip->ti_item[tlbpid])) {
		cmn_err(CE_WARN,"tlbpid %d myp 0x%x != &ti_item[tlbpid] 0x%x\n",
			tlbpid, myp, &(tip->ti_item[tlbpid]));
	}
#endif /* PIDDEBUG */
	/*
	 * If this tlbpid is already at the tail of the LRU list, we
	 * leave the list alone.  Otherwise, move this tlbpid to the end.
	 */
	if (myp != tip->ti_tail) {
		prevp->ti_next = myp->ti_next;
		tip->ti_prev[prevp->ti_next-tip->ti_item] = prevp;

		/* Now insert at the end of the LRU list */
		tail = tip->ti_tail;
		myp->ti_next = tail->ti_next;
		tip->ti_prev[myp->ti_next-tip->ti_item] = myp;

		tail->ti_next = myp;
		tip->ti_prev[myp-tip->ti_item] = tail;

		tip->ti_tail = myp;
	}

	/* Once in a while, flush a random entry */
#ifdef ULI
	if (++tip->ti_flush >= tip->ti_flush_interval) {
#else
	if (++tip->ti_flush >= TLBHI_NPID/(NTLBENTRIES-TLBRANDOMBASE)) {
#endif
		tip->ti_flush = 0;
#if JUMP_WAR
		if (tip->ti_nextinval >= getwired())
#endif

		invaltlb(tip->ti_nextinval);
		if (++tip->ti_nextinval >= NTLBENTRIES)
			tip->ti_nextinval = TLBRANDOMBASE;
	}
}

#ifdef ULI
/* allocate a private tlbpid for the exclusive use of one process
 * NOTE must be freed when process exits or we lose it forever!
 */

#ifdef DEBUG
int availpids()
{
    return(private.p_tlbinfo->ti_total_pids);
}
int pidflushinterval()
{
    return(private.p_tlbinfo->ti_flush_interval);
}
#define dprintf(x) printf x
#else
#define dprintf(x)
#endif

tlbpid_t
alloc_private_tlbpid(void)
{
    tlbinfo_t *tip;
    tlbpid_item_t *itemp, *prevp;
    int s, pid;

    ASSERT_NOMIGRATE;
    tip = private.p_tlbinfo;

    /* The total available pids must be at least as great as the
     * number of non-wired tlb slots in order for the tlb flushing
     * algorithm to work
     */
    ASSERT(tip->ti_total_pids >= NTLBENTRIES - TLBRANDOMBASE);
    if (tip->ti_total_pids <= NTLBENTRIES - TLBRANDOMBASE) {
	dprintf(("out of tlbpids %d\n", tip->ti_total_pids));
	return(0);
    }

    /* protect against clock() */
    s = splhi();

    /* grab the pid */
    itemp = tip->ti_tail->ti_next;
    pid = itemp - tip->ti_item;
    tip->ti_lastuser[pid] = curuthread->ut_as.utas_tlbid; /* XXX which uthread? */

    /* remove from LRU list */
    prevp = tip->ti_prev[pid];
    prevp->ti_next = itemp->ti_next;
    tip->ti_prev[itemp->ti_next-tip->ti_item] = prevp;

    tip->ti_total_pids--;
    tip->ti_flush_interval = tip->ti_total_pids /
	(NTLBENTRIES - TLBRANDOMBASE);

    splx(s);
    return(pid);
}

void
free_private_tlbpid(tlbpid_t pid)
{
    tlbpid_item_t *itemp, *nextp;
    tlbinfo_t *tip;
    int s;

    ASSERT_NOMIGRATE;

    tip = private.p_tlbinfo;

    itemp = &tip->ti_item[pid];
    
    s = splhi();

    /* insert at tail of LRU list so it won't be used again
     * until we've been through the entire list, so any remaining
     * entries in the TLB with this pid will have been flushed
     */
    nextp = tip->ti_tail->ti_next;
    itemp->ti_next = nextp;
    tip->ti_prev[pid] = tip->ti_prev[nextp - tip->ti_item];
    tip->ti_prev[pid]->ti_next = itemp;
    tip->ti_prev[nextp - tip->ti_item] = itemp;
    tip->ti_tail = itemp;

    tip->ti_total_pids++;
    tip->ti_flush_interval = tip->ti_total_pids /
	(NTLBENTRIES - TLBRANDOMBASE);

    splx(s);
}
#endif

/*
 * TLB PIDs are managed on a per-processor basis.
 *
 *	utas	-> uthread AS needing new TLBID
 *	flag	-> VM_TLBINVAL: invalidate ids on other processors (called
 *			when a thread wants to invalidate all of its
 *()]);			extant TLB entries).
 *		   VM_TLBVALID: leave ids for other processors untouched
 *			(called when a uthread is resuming and finds that
 *			it does not have a valid ID on this machine).
 *		   VM_TLBNOSET: do not set tlbpid in the tlb for
 *			the current uthread
 *	NOTE: although there is always a current uthread, if we are
 *		called from swtch, then the 'current' thread is the thread
 *		we are going to start, but may currently be running the old guy.
 *		In this case, we do NOT wish to tamper with the tlb, since
 *		it isn't really ours yet.
 *
 * tlbpid's are allocated using an LRU scheme.  The invaltlb in tlbpid_use
 * insures that by the time we reallocate the tlbpid at the front of the per-
 * processor LRU list, there will be no entries left in the tlb that use
 * that PID.
 */
void
new_tlbpid(struct utas_s *utas, int flag)
{
	register	int i;
	register	unsigned char *s;
	register 	tlbinfo_t *tip;
	int		s1;

	if (flag & VM_TLBINVAL) {
		MINFO.tdirt++;
		s = utas->utas_tlbpid;
		for (i = maxcpus; i > 0; i--)
			*s++ = TLBPID_NONE;
	} else {
		/*
		 * Just to make sure that whoever calls new_tlbpid()
		 * understands the flag argument, and isn't passing
		 * ``0'' because it's fashionable.
		 */
		ASSERT(flag & VM_TLBVALID);
	}

	/* no preemption, no migration permitted */
	s1 = splhi();
	tip = private.p_tlbinfo;
	tlbpid(utas) = tip->ti_tail->ti_next - tip->ti_item;
	ASSERT(tlbpid(utas) != TLBPID_NONE);
	ASSERT(tlbpid(utas) < TLBHI_NPID);
#ifdef PIDDEBUG
	{ int i;
	for (i = 0; i < NTLBENTRIES; i++) {
		extern tlbpidmatch(int, int);

		if (tlbpidmatch(tlbpid(utas), i)) {
			cmn_err(CE_CONT, "TLB pid %d has entry at index %d\n",
				tlbpid(utas), i);
			debug("busy");
		}
	      }
	}
#endif

	tlbpid_use(utas, tlbpid(utas));

	/*
	 * set_tlbpid sets current tlbpid and changes all the tlbpid fields
	 * in the wired tlb entries.  Note that new_tlbpid is sometimes
	 * called with on not the current running thread.
	 */
	if (curuthread && &curuthread->ut_as == utas && !(flag & VM_TLBNOSET)) {
		ASSERT(UT_TO_KT(curuthread)->k_sonproc == cpuid());
		set_tlbpid(tlbpid(utas));
	}
	splx(s1);

	MINFO.tlbpids++;
#if TFP
	/* If we're calling new_tlbpid to invalidate all TLB entries, then
	 * we really need to invalidate the ICACHE PID as well.  If we're
	 * only calling new_tlbpid because process has no ID on this cpu,
	 * then we let ICACHE ID be handled independently.
	 */
	if (flag & VM_TLBINVAL)
		new_icachepid(utas, flag);
#endif /* TFP */
}

/*
 * Called to set all tlbpids for uthread ``utas'' to ``value''.
 * NOTE: this can ONLY be called by the current uthread OR if the
 * caller can guarantee that the subject uthread cannot be running or
 * begin to run. Otherwise we could cause it to run with an invalid tlbpid
 */
void
set_tlbpids(utas_t *utas, unsigned char value)
{
	register int i, maxc = maxcpus;
	register unsigned char *s = utas->utas_tlbpid;
#if TFP
	register unsigned char *si = utas->utas_icachepid;
#endif

	ASSERT((value==0) || (value==(unsigned char)-1));
	for (i = maxc; i > 0; i--) {
		*s++ = value;
#if TFP
		/* No reason for separate set_icaachepids() routine.  Only
		 * called to initialize the pid array to "0" (system proc) or
		 * "-1" (need to allocate).
		 */
		*si++ = value;
#endif
	}
}


/*
 * Invalidate tlb entries for vpn for all
 * threads in address space.
 */
static void
_tlbunmap(pas_t *pas, __psunsigned_t vpn)
{
	ppas_t *ppas;

	/* Better not be able to migrate to another cpu */
	ASSERT_NOMIGRATE;

	ASSERT(mrislocked_access(&pas->pas_aspacelock) || \
		(mrislocked_update(&pas->pas_aspacelock)));
	ASSERT(mrislocked_access(&pas->pas_plistlock) || \
		(mrislocked_update(&pas->pas_plistlock)));
	/*
	 * The caller has the plistlock locked so we can scan
	 * the ppas list
	 */
	if (!pas) {
		unmaptlb(0, vpn);
	} else {
		for (ppas = pas->pas_plist; ppas; ppas = ppas->ppas_next) {
			if (ppas->ppas_utas == NULL)
				continue;
			if (tlbpid_is_usable(ppas->ppas_utas))
				unmaptlb(ppas->ppas_utas->utas_tlbpid, vpn);
		}
	}
}

static void
tlbunmap(pas_t *pas, __psunsigned_t vpn)
{
	_tlbunmap(pas, vpn);

	if (IS_MP)
		atomicAddIntHot(&tlbcleancount, -1);
}

/*
 * MP wrapper for tlbunmap.
 */
void
tlbclean(pas_t *pas, pgno_t vpn, cpumask_t forcemask)
{
	int i, s, tlbcleans_sent;
	cpuid_t id;

	MINFO.tphys++;

#if defined(R4000) || defined(R10000)
	/* This is an optimization for address space sharing among a
	 * large number of processes.  Once the number of processes
	 * gets large, it's much faster to use tlbsync() to blast
	 * all TLB entries and let then fault back in rather than
	 * searching the processlist and blasting the individual
	 * entry for each process.
	 * Of course, processors with large numbers of TLB entries,
	 * like the R8000, have a larger refill penalty so this
	 * optimization does not apply (at least not as the same
	 * threshold value).
	 */
	if (pas->pas_refcnt > 16) {
		tlbsync(/*asgen*/ 0, forcemask, FLUSH_MAPPINGS );
		return;
	}
#endif

	mraccess(&pas->pas_plistlock);
#ifndef MP
	_tlbunmap(pas, vpn);
#else /* MP */

	tlbcleans_sent = 0;
	/* no preemption, no migration */
	s = splhi();
	atomicAddIntHot(&tlbcleancount, maxcpus-1);
	if (CPUMASK_IS_ZERO(isolate_cpumask))
	  for (i = 0; i < maxcpus; i++) {
		if (((id = pdaindr[i].CpuId) != -1) &&
		    (id != private.p_cpuid)) {

			tlbcleans_sent++;
			nested_cpuaction(id, (cpuacfunc_t)tlbunmap,
				pas, (void *)vpn, 0, 0);
		}
	} else for (i = 0; i < maxcpus; i++) {
		if (((id = pdaindr[i].CpuId) != -1) &&
		    (id != private.p_cpuid)) {
			/*
			 * Pages don't get stolen from isolated processors
			 * Unless...if force bitmask has cpu set, hit that
			 * cpu too.
			 */
		    	if ((!(pdaindr[i].pda->p_flags & PDAF_ISOLATED)) ||
			    CPUMASK_TSTB(forcemask, id)) {
				tlbcleans_sent++;
				nested_cpuaction(id, (cpuacfunc_t)tlbunmap,
					pas, (void *)vpn, 0, 0);
			}
		}
	}

	if (tlbcleans_sent != (maxcpus-1))
		atomicAddIntHot(&tlbcleancount, 1+tlbcleans_sent-maxcpus);

	_tlbunmap(pas, vpn);
	splx(s);

	ASSERT(!issplhi(getsr()));
	while (fetchIntHot(&tlbcleancount)) {
#ifdef DEBUG
		if (kdebug)
			du_conpoll();
#endif
		continue;
	}
#endif /* MP */
	mrunlock(&pas->pas_plistlock);
}

/*
 * lightweight tlbflush(). Scans proc p, plus all others in shaddr,
 * if they are running flush the random tlb entries, otherwise zap
 * the tlbpid. Don't muck with wired entries. Only good if nothing
 * has been removed from address space.
 */
static void
_tlbunmapsa(pas_t *pas)
{
	ppas_t *ppas;
	register int id = cpuid();

	if (!pas)
		return;
	
	ASSERT_NOMIGRATE;

	ASSERT(mrislocked_access(&pas->pas_aspacelock) || \
		(mrislocked_update(&pas->pas_aspacelock)));
	ASSERT(mrislocked_access(&pas->pas_plistlock) || \
		(mrislocked_update(&pas->pas_plistlock)));
	/*
	 * The caller has the plistlock locked so we can scan
	 * the ppas list
	 */

	for (ppas = pas->pas_plist; ppas; ppas = ppas->ppas_next) {
		if (&curuthread->ut_as == ppas->ppas_utas) {
			flush_tlb(TLBFLUSH_RANDOM);
		} else {
			ppas->ppas_utas->utas_tlbpid[id] = TLBPID_NONE;
#if TFP
			ppas->ppas_utas->utas_icachepid[id] = (unsigned char)-1;
#endif
		}
	}
}

static void
tlbunmapsa(pas_t *pas)
{
	_tlbunmapsa(pas);

	if (IS_MP)
		atomicAddIntHot(&tlbcleancount, -1);
}

/*
 * MP wrapper for tlbunmapsa.
 */
void
tlbcleansa(pas_t *pas, ppas_t *ppas, cpumask_t forcemask)
{
	int i, s, tlbcleansas_sent;
	cpuid_t id;

#if defined(R4000) || defined(R10000)
	/* This is an optimization for address space sharing among a
	 * large number of processes.  Once the number of processes
	 * gets large, it's much faster to use tlbsync() to blast
	 * all TLB entries and let then fault back in rather than
	 * searching the processlist and blasting the individual
	 * entry for each process.
	 * Of course, processors with large numbers of TLB entries,
	 * like the R8000, have a larger refill penalty so this
	 * optimization does not apply (at least not as the same
	 * threshold value).
	 */
	if (pas->pas_refcnt > 16) {
		tlbsync(/*asgen*/ 0, forcemask, FLUSH_MAPPINGS );
		return;
	}
#endif

	mraccess(&pas->pas_plistlock);
	tlbcleansas_sent = 0;
	s = splhi();		/* no preemption, no migration */

	if (!IS_MP ||
	    ((pas->pas_flags & PAS_SHARED) == 0) || pas->pas_refcnt == 1)
	{
		if (IS_MP)
			tlbdirty(ppas->ppas_utas);
		flush_tlb(TLBFLUSH_RANDOM);
		splx(s);
		mrunlock(&pas->pas_plistlock);
		return;
	}
	else { /* avoid cc-1110 error "not reachable" for SP systems */
		atomicAddIntHot(&tlbcleancount, maxcpus-1);
		if (CPUMASK_IS_ZERO(isolate_cpumask))
		  for (i = 0; i < maxcpus; i++) {
			if (((id = pdaindr[i].CpuId) != -1) &&
				(id != private.p_cpuid)) {

				tlbcleansas_sent++;
				nested_cpuaction(id, (cpuacfunc_t)tlbunmapsa,
					pas, 0, 0, 0);
			}
		} else for (i = 0; i < maxcpus; i++) {
			if (((id = pdaindr[i].CpuId) != -1) &&
				(id != private.p_cpuid)) {
				/*
				 * Pages don't get stolen from isolated processors
				 * Unless...if force bitmask has cpu set, hit that
				 * cpu too.
				 */
					if ((!(pdaindr[i].pda->p_flags & PDAF_ISOLATED)) ||
					CPUMASK_TSTB(forcemask, id)) {
					tlbcleansas_sent++;
					nested_cpuaction(id, (cpuacfunc_t)tlbunmapsa,
						pas, 0, 0, 0);
				}
			}
		}

		if (tlbcleansas_sent != (maxcpus-1))
			atomicAddIntHot(&tlbcleancount, 1+tlbcleansas_sent-maxcpus);
		_tlbunmapsa(pas);
		splx(s);

		ASSERT(!issplhi(getsr()));
		while (fetchIntHot(&tlbcleancount)) {
#ifdef DEBUG
			if (kdebug)
				du_conpoll();
#endif
			continue;
		}
		mrunlock(&pas->pas_plistlock);
	}
}


/*
 * Mark uthread tlbs dirty on all processors
 * (except this one) on which we have previously run.
 * We do this by zapping the tlbpids for the uthread.
 */
void
tlbdirty(utas_t *utas)
{
	register int i, maxc = maxcpus;
	register unsigned char *s = utas->utas_tlbpid;
#if TFP
	register unsigned char *si = utas->utas_icachepid;
#endif
	register int id = cpuid();

	ASSERT_NOMIGRATE;

	MINFO.tdirt++;
#if TFP
	for (i = 0; i < maxc; i++, s++, si++) {
#else
	for (i = 0; i < maxc; i++, s++) {
#endif
		if (i != id) {
			*s = TLBPID_NONE;
#if TFP
			*si = (unsigned char)-1;
#endif
		}
	}
}

/*
 * Flush tlb
 * A asgen != 0 indicates that the wired entries,
 * as well as the random tlb entries, are to be flushed.
 *
 * NOTE: asgen == -1 now used as special flag to signify that we
 * need to update the tlbflushmask instead of the tlbflushcount. This is
 * an optimization for non-isolated processor case of "flush random".
 *
 * NOTE for MP:
 * Since we must avoid preemption during this routine, and since
 * the only way to avoid preemption is to run at splhi, we'll
 * push this requirement to the caller since the caller is usually
 * executing at splhi.  This then allows us to use the nested_mutex
 * calls.
 */
void
tlbflush(uint64_t asgen)
{

#if MP
	/* We must be at splhi() once we clear our tlbflushlock bit until
	 * we complete the flush. The actual requirement is no-preemption
	 * since we've indicated to the other cpus that we have (or are
	 * about to) scrub the tlb entries.
	 * In fact, we mustbe non-preemptable until after clearing the
	 * p_kvfault.
	 * Setting the thread "mustrun" is NOT sufficient.
	 */
	ASSERT(issplhi(getsr()));
	ASSERT((asgen == -1) || fetchIntHot(&tlbflushcount) > 0);
#endif
#ifdef EVEREST
	if (asgen == -1) {
		CPUMASK_ATOMCLR(tlbflushmask, private.p_cpumask);
		asgen = 0;
	} else
#endif /* EVEREST */

		{
		atomicAddIntHot(&tlbflushcount, -1);
		}

	KTRACE_TLBENTER(TLB_TRACE_TLBFLUSH, asgen, curuthread, 0, 0);
	MINFO.tlbflush++;

	/* Only blow out wired entries if the current running uthread
	 * is sharing address space with the thread that originated the flush.
	 * Otherwise we could blow away someone who is either running or
	 * in tfault, and overwrite their uptbl entries.
	 * Note that since we are in the kernel, some of the first wired
	 * entries may have been replaced with the thread's upage/kstk, so no
	 * need to flush those.
	 */
	if (asgen && curuthread && !AS_ISNULL(&curuthread->ut_asid)) {
		if (ASID_TO_GEN(curuthread->ut_asid) == asgen) {
			flush_tlb(TLBFLUSH_NONKERN);
			zapsegtbl(&curuthread->ut_as);
		} else
			flush_tlb(TLBFLUSH_RANDOM);
	} else
		flush_tlb(TLBFLUSH_RANDOM);

	/* Zero out history of kernel virtual translations
	 * on the current cpu.
	 */
	if (IS_MP)
		if (private.p_kvfault) {
#ifdef R4000
			int i, sz;

			sz = clrsz / NBBY;
			for (i=0; i < CACHECOLORSIZE; i++)
				bzero(private.p_clrkvflt[i], sz);
#endif
			bzero(private.p_kvfault, (sptsz + NBBY - 1) / NBBY);
		}

#if JUMP_WAR
	clr_jump_war_wired();
#endif
}

/* tlbflush_rand 
 * Entry point invoked by cpu to perform cpuvaction to flush all random
 * tlb entries.  Used instead of the more expensive "cpuaction" version
 * of tlbflush when no parameter is needed.
 */
void
tlbflush_rand(void)
{
	int flushs;

	/* The following code is used to ensure that the tlbflushcount is
	 * decremented the appropriate number of times in case multiple
	 * non-specific tlbflush occur at the same time (since cpuvaction
	 * only has a single bit).
	 */

#ifdef EVEREST
	if (CPUMASK_TSTM(tlbflushmask, private.p_cpumask)) {
		tlbflush(-1);
	}
#endif /* EVEREST */
	flushs = swap_int((int *)&private.p_tlbflush_cnt, 0);
	if (flushs == 0)
		return;
	if (flushs > 1)
		atomicAddIntHot(&tlbflushcount, -flushs+1);

	tlbflush(0);
}

#if JUMP_WAR
/*
 * Flush out the wired entries used for the branch-at-end-of-page
 * workaround.  Also clean out the corresponding info in the pda.
 */

void
clr_jump_war_wired(void)
{
	int i;
	int s;
	int idx;

	s = splhi();
	if (private.p_jump_war_pid == 0) {
		splx(s);
		return;
	}

	private.p_jump_war_pid = 0;
	private.p_jump_war_uthreadid = 0;

	if ((idx = getwired()) > NWIREDENTRIES) {
		for (i = NWIREDENTRIES; i < idx; i++)
			_invaltlb(i);
		setwired(NWIREDENTRIES);
	}
	splx(s);
}
#endif

static struct sysbitmap	*clean_stale_sptmap_pre_op(struct sysbitmap *bm);
static void	clean_stale_sptmap_post_op(struct sysbitmap *bm);

/*
 * Flush tlbs on all non-isolated processors.  Returns only when
 * all processors have flushed.
 *
 * Could be called because:
 *	Swapping pages out
 *	New (system) virtual/physical association
 *	No system virtual memory
 *	Msync with invalidation
 *	Shaddr process detaching segment
 *
 * asgen - Address Space being flushed
 * isomask - mask of isolated cpus that address space is running on.
 * flags - reasons to call tlbsync() | [ sleep flag ]
 */
void
tlbsync(uint64_t asgen, cpumask_t isomask, int flags)
{
	int s, i, id;
	pda_t *npda;
	int force = 0;
	cpumask_t local_mask;
	uint64_t asgenparm;
	int tlbsyncs_sent=0;

	/*
	 * this routine is called for lots of different reasons - lets
	 * make sure callers understand the different semantics
	 */
	ASSERT(CPUMASK_IS_ZERO(isomask) || (flags & (STEAL_PHYSMEM|FLUSH_MAPPINGS)));
	ASSERT(((flags & FLUSH_WIRED) == 0) || asgen != 0LL);
	ASSERT(((flags & FLUSH_WIRED) == 0) || (flags & FLUSH_MAPPINGS));

	MINFO.tlbsync++;
	asgenparm = (flags & FLUSH_WIRED) ? asgen : 0LL;

#ifndef MP
	/*
	 * fast path for simplex
	 */
	{
	struct sysbitmap *clean_sptmap;
	clean_sptmap = clean_stale_sptmap_pre_op(&sptbitmap);
	tlbflush(asgenparm);
	if (clean_sptmap != NULL)
		clean_stale_sptmap_post_op(clean_sptmap);
	return;
	}

#else /* MP */
	/* MP kernels no longer reclaim stale/aged sptmap entries on every
	 * tlbsync().  Several reasons for this:
	 * (1) Since they're 64-bits the sptmap itself can be large enough
	 *     to avoid exhausting the free entries faster than they can
	 *     aged.
	 * (2) 64-bit MP kernels have less need for K2 addresses since
	 *     all pages can be direct mapped.
	 * (3) NUMA systems used tlbsync() more often (page migration, for
	 *     example) so we can incur a lot of additional unecessary
	 *     overhead performing a bitmap merge on every tlbsync.
	 * So, we let the natural aging algorithm take care of all sptmap
	 * recovery.
	 */


	/*
	 * flush on all non-isolated processors.  Don't have to worry
	 * about isolated processors when STEALing:  A process executing 
	 * on an isolated processor has its pages locked down.  If a process
	 * migrates off and then back to an isolated processor, it'll be 
	 * assigned a new tlbpid.  No way to reference through a stale tlb 
	 * entry.
	 * Must avoid taking generic interrupts between the time we set
	 * tlbflushmask and the time we initiate cpuaction's; otherwise,
	 * we could block on a spin lock in an interrupt handler (and the
	 * spin lock could be held by a processor waiting in sptalloc for 
	 * a tlbflush to complete).
	 */

	/* this keeps us from preemption/migration */
	s = splhi();
#if SN0
	KTRACE_TLBENTER(TLB_TRACE_TLBSYNC, sa, 0, flags, 0);
#else
	KTRACE_TLBENTER(TLB_TRACE_TLBSYNC, sa, isolate_cpumask, flags, 0);
#endif

	if (flags & (STEAL_PHYSMEM | FLUSH_MAPPINGS)) {
		/*
		 * remove mask of isolated cpus and myself
		 */
		local_mask = maskcpus;
		CPUMASK_CLRM(local_mask, isolate_cpumask);

		if (CPUMASK_IS_NONZERO(isomask)) {
			/* add back isolated cpus that AS is running on */
			CPUMASK_SETM(local_mask, isomask);
			force = 1;
		} else
			/* this is an optimization to avoid reading the
			 * p_flags field of each cpu's PDA when we have
			 * NO isolated processors.  On EVEREST systems this
			 * ends up saving 2 microseconds per processor when
			 * there are no isolated processors.
			 */
			if (CPUMASK_IS_ZERO(isolate_cpumask))
				force = 2;
	} else if (flags & NO_VADDR) {
		if (CPUMASK_IS_NONZERO(isolate_cpumask)) {
			local_mask = kvfaultcheck(flags);
		} else {
			local_mask = maskcpus;
		}
	} else if (flags & ISOLATED_ONLY) {
		if (CPUMASK_IS_NONZERO(isolate_cpumask))
			local_mask = kvfaultcheck(flags);
		else
			CPUMASK_CLRALL(local_mask);
	} else {
		/* nothing to do, return spl to previous value */
		splx(s);
		ASSERT(1);
		return;
	}

	KTRACE_TLBENTER(TLB_TRACE_TLBSYNC1, 0, force, local_mask, 0);
	CPUMASK_CLRM(local_mask, private.p_cpumask);

	ASSERT(fetchIntHot(&tlbflushcount) >= 0);
	
#ifdef EVEREST
	/* The following statement is an optimization for the common
	 * case of no isolated processors and no wired tlb flush
	 * (i.e. this case would be page migration on SN0).
	 * No need to touch ANY data
	 * in pda of the cpus we're sending to since we send to all
	 * enabled processors in the system.
	 * NOTE: kernel will execute correctly without the following
	 * statement, but speedup is on the order of 25% in a 24P IP19.
	 * NOTE: This optimization degrades performance on SN0, at
	 * least when the number of processors is 58 or so (takes
	 * more than TWICE as long as the non-optimized path).
	 */
	if ((force == 2) && !(flags & FLUSH_WIRED)) {

		tlbflushmask = local_mask;

		for (i = 0; i < maxcpus; i++) {

			if ((id = pdaindr[i].CpuId) == -1 )
				continue;

			/* local_mask is set to the mask of cpus to flush. */

			if (CPUMASK_TSTB(local_mask, id))
				sendintr(id, DOTLBACTION);
		}

		/* tlbflush() must execute without preemption.  In addition it must
		 * execute on the same cpu which executed the "sendintr()" loop
		 * above.  So we just keep splhi() a little while longer.
		 */
		tlbflush(-1); 
		splx(s);

		/*
		 * Wait for all processors to finish flushing tlbs.
		 */
		ASSERT(!issplhi(getsr()));
		while (CPUMASK_IS_NONZERO(tlbflushmask)) {
#ifdef DEBUG
			if (kdebug)
				/* Can't invoke du_conpoll if it may be running as
				 * an interrupt thread.
				 */
			  if (private.p_kstackflag != PDA_CURINTSTK)
					du_conpoll();
#endif
		}
		return;
	} /* END of optimization */
#endif /* EVEREST */

	atomicAddIntHot(&tlbflushcount, maxcpus);

	for (i = 0; i < maxcpus; i++) {

		if ((id = pdaindr[i].CpuId) == -1 )
			continue;
		npda = pdaindr[i].pda;

		/* local_mask is set to the mask of cpus to flush. */

		if (CPUMASK_TSTB(local_mask, id)) {
			int s1;

			/* The following statement is an optimization for
			 * the common case of non-isolated cpus and avoids
			 * overhead of obtaining a lock.
			 */
			if (force || !(npda->p_flags & PDAF_BROADCAST_OFF)) {

				tlbsyncs_sent++;
				if (flags & FLUSH_WIRED) {
					/* we get one fast asgen request */
					if (compare_and_swap_ptr(
						(void **)&npda->p_tlbsync_asgen,
						0, (void *)asgenparm))

						sendintr(id, DOACTION);
					else
						/* asgen request "busy" use
						 * slower cpuaction.
						 */
						nested_cpuaction(id,
							(cpuacfunc_t)tlbflush,
							(void *)asgenparm, 0,
							 0, 0);
				} else {
					atomicAddUint(&npda->p_tlbflush_cnt, 1);
					/* We're short-circuiting the
					 * cpuvaction path (p_tlbflush_cnt =>
					 * cpuvaction).
					 */
					sendintr(id, DOTLBACTION);
				}
				continue;
			}
			s1 = mutex_spinlock_spl(&npda->p_special, spl7);

			if ((npda->p_flags & PDAF_BROADCAST_OFF) && (!force)) {
				ASSERT(npda->p_flags & PDAF_ISOLATED);
				KTRACE_TLBENTER(TLB_TRACE_TLBSYNC_DEL,
						id, npda->p_flags, 0, 0);
				npda->p_delayacvec |= DA_TLB_FLUSH;
				mutex_spinunlock(&npda->p_special, s1);
			} else {
				mutex_spinunlock(&npda->p_special, s1);
				tlbsyncs_sent++;
				nested_cpuaction(id, (cpuacfunc_t)tlbflush,
						(void *)asgenparm, 0, 0, 0);
/*
 * Actually, the sequence of mutex_spinunlock and cpuaction opens up a race --
 * if after the mutex_spinunlock this cpu takes an intr, the other cpu can
 * acquire its own p_special and be able to isolate itself. When this cpu
 * comes back from the intr, it will still intr the other cpu, increasing
 * latency. Since this is a low probability event, we live with this, since
 * holding the p_special lock across a cpuaction itself increases the latency. 
 */
			}
		}
	}

	if (tlbsyncs_sent != (maxcpus-1))
		atomicAddIntHot(&tlbflushcount, 1+tlbsyncs_sent-maxcpus);

	/* tlbflush() must execute without preemption.  In addition it must
	 * execute on the same cpu which executed the "sendintr()" loop
	 * above.  So we just keep splhi() a little while longer.
	 */
	ASSERT(fetchIntHot(&tlbflushcount) > 0);
	tlbflush(asgenparm); 
	splx(s);

	/*
	 * Wait for all processors to finish flushing tlbs.
	 * No need to lock if just looking.
	 */
	ASSERT(!issplhi(getsr()));
	while (fetchIntHot(&tlbflushcount)) {
#ifdef DEBUG
		ASSERT(fetchIntHot(&tlbflushcount) >= 0);
		if (kdebug)
			/* Can't invoke du_conpoll if it may be running as
			 * an interrupt thread.
			 */
			if (private.p_kstackflag != PDA_CURINTSTK)
				du_conpoll();
#endif
	}

	ASSERT(fetchIntHot(&tlbflushcount) >= 0);
	return;
#endif /* MP */
}

static int skip_clean_aged_sptmap = 0;
/*
 * Every entry in the aged_sptmap has been aging for
 * quite a while now.  In fact, in the time it's been
 * aging, we've done a flush_tlb_index on every entry
 * in the tlb on every processor; thus, everything in 
 * the map is now guaranteed to be tlb-clean.  We can
 * safely recycle it!
 *
 * possible modifications: 
 * -we could keep track of how many items are in the 
 *  various maps, and only do this work if it's worth it.
 * -recycle only a portion of the aged map each time
 *  (this should especially be considered if we start
 *  using larger bitmaps)
 */
static int
clean_aged_sptmap(struct sysbitmap *bm)
{
	int s, i;
#ifdef R4000
	int nmaps;
	register struct bitmap *abitmap, *sbitmap;
#endif
	/*
	 * If any of the processors hasn't made it through the entire tlb
	 * since last time we merged, skip the merge for now.  The most
	 * likely cause for this is when we drop into the DEBUGger on a
	 * processor.
	 */
	for (i = 0; i < maxcpus; i++) {
		register pda_t 	*npda;

		if (pdaindr[i].CpuId == -1)
			continue;

		npda = pdaindr[i].pda;

		if (npda->p_flags & PDAF_ISOLATED)
			continue;

		if (npda->p_tlbinfo->ti_count < NRANDOMENTRIES) {
#ifdef DEBUG
			/* Although we recover from this, it really
			 * shouldn't occur if the various tlb flushing
			 * constants are correct and clock ticks are
			 * occurring on a timely basis (unless cpu
			 * is non-preemptive).
			 */
			if ( !(npda->p_flags & PDAF_NONPREEMPTIVE))
				cmn_err(CE_WARN,"clean_aged_sptmap: cpu %d ti_count %d\n",
					i,  npda->p_tlbinfo->ti_count);
#endif /* DEBUG */		 
			skip_clean_aged_sptmap++; /* informational only */
			return(0);
		}
	}

#if SN0
	KTRACE_TLBENTER(TLB_TRACE_CLEAN_AGE, 0, 0, 0, 0);
#else
	KTRACE_TLBENTER(TLB_TRACE_CLEAN_AGE, isolate_cpumask, 0, 0, 0);
#endif 

	/*
	 * Isolated processors may not be taking clock interrupts, so
	 * they may not be participating in the gradual tlb flush.
	 */
	if (CPUMASK_IS_NONZERO(isolate_cpumask))
		tlbsync(0LL, allclr_cpumask, ISOLATED_ONLY);

	/*
	 * Return aged system virtual memory to bitmap.
	 *
	 * Must lock the aged map -- clean_stale_bitmap() pulls in
	 * these bits, too.  No need to protect the colormaps, though --
	 * clean_stale_bitmap() doesn't bother with them.
	 */
	s = mutex_bitlock(&bm->aged_sptmap.m_lock, MAP_LOCK);
	if (bm->aged_sptmap.m_lowbit <= bm->aged_sptmap.m_highbit)
		mergebitmaps(bm, &bm->sptmap, &bm->aged_sptmap);

	/* Now let the stale stuff age for a while */

	if (bm->stale_sptmap.m_lowbit <= bm->stale_sptmap.m_highbit)
		bmapswtch(&bm->aged_sptmap, &bm->stale_sptmap);
	nested_bitunlock(&bm->aged_sptmap.m_lock, MAP_LOCK);

#ifdef R4000
	for (nmaps = 0;
	     nmaps <= CACHECOLORMASK;
	     nmaps++) {

		abitmap = bm->aged_sptmap.m_color[nmaps];
		sbitmap = bm->stale_sptmap.m_color[nmaps];

		nested_bitlock(&abitmap->m_lock, MAP_LOCK);
		if (abitmap->m_lowbit <= abitmap->m_highbit)
			mergebitmaps(bm, bm->sptmap.m_color[nmaps], abitmap);
		if (sbitmap->m_lowbit <= sbitmap->m_highbit)
			bmapswtch(abitmap, sbitmap);
		nested_bitunlock(&abitmap->m_lock, MAP_LOCK);
	}
#endif
	splx(s);

	return(1);
}

static struct sysbitmap *
clean_stale_sptmap_pre_op(struct sysbitmap *bm)
{
	int s;
#ifdef R4000
	int nmaps;
	register struct bitmap *tbitmap, *sbitmap, *abitmap;
#endif

	if (cpsema(&bm->sptmaplock) == 0)
		return NULL;

	/*
	 * If we get here, then we have acquired the sptmaplock mutex.
	 * There's no reason to lock temp_sptmap -- this is the only routine
	 * that ever manipulates it.
	 */
	if (bm->stale_sptmap.m_lowbit <= bm->stale_sptmap.m_highbit)
		bmapswtch(&bm->temp_sptmap, &bm->stale_sptmap);

	/*
	 * Pull in the aged bits.
	 */
	if (bm->aged_sptmap.m_lowbit < bm->aged_sptmap.m_highbit) {
		s = mutex_bitlock(&bm->aged_sptmap.m_lock, MAP_LOCK);
		mergebitmaps(bm, &bm->temp_sptmap, &bm->aged_sptmap);
		mutex_bitunlock(&bm->aged_sptmap.m_lock, MAP_LOCK, s);
	}

#ifdef R4000
	for (nmaps = 0;
	     nmaps <= CACHECOLORMASK;
	     nmaps++) {

		sbitmap = bm->stale_sptmap.m_color[nmaps];
		abitmap = bm->aged_sptmap.m_color[nmaps];
		tbitmap = bm->temp_sptmap.m_color[nmaps];

		if (sbitmap->m_lowbit <= sbitmap->m_highbit)
			bmapswtch(tbitmap, sbitmap);

		if (abitmap->m_lowbit <= abitmap->m_highbit) {
			s = mutex_bitlock(&abitmap->m_lock, MAP_LOCK);
			mergebitmaps(bm, tbitmap, abitmap);
			mutex_bitunlock(&abitmap->m_lock, MAP_LOCK, s);
		}
	}
#endif
	/*
	 * Restart the aged-map flushing cycle -- no need to flush
	 * again for a while.
	 */

	reset_aged = 1;		/* delay aged flushing */
	return bm;
}

static void
clean_stale_sptmap_post_op(struct sysbitmap *bm)
{
	int s = splhi();
#ifdef R4000
	int nmaps;
	register struct bitmap *tbitmap;
#endif

	if (bm->temp_sptmap.m_lowbit <= bm->temp_sptmap.m_highbit)
		mergebitmaps(bm, &bm->sptmap, &bm->temp_sptmap);

#ifdef R4000
	for (nmaps = 0; nmaps <= CACHECOLORMASK; nmaps++) {
		tbitmap = bm->temp_sptmap.m_color[nmaps];
		if (tbitmap->m_lowbit <= tbitmap->m_highbit)
			mergebitmaps(bm, bm->sptmap.m_color[nmaps], tbitmap);
	}
#endif
	splx(s);

	vsema(&bm->sptmaplock);
}

/*
 * In some (rare?) cases, KV memory may get used so quickly that
 * the normal time-based heuristics can't keep up.  In this
 * case, we may need to specially recycle all the pages on
 * stale_map (rather than aged_map).
 */
int
clean_stale_sptmap(struct sysbitmap *bm, int flags)
{
	if (clean_stale_sptmap_pre_op(bm) == NULL) {
		/*
		 *  The semaphore was being held, which means that some other
		 *  processor is now doing the switch/tlbsync/merge.  Wait
		 *  for completion (unless the caller specified TLB_NOSLEEP), 
		 *  and just return.  There is no need to do redundant work.
		 */
		if (flags & TLB_NOSLEEP)
			return 0;

		psema(&bm->sptmaplock, PSWP|TIMER_SHIFT(AS_MEM_WAIT));
		vsema(&bm->sptmaplock);
		return 1;
	}

	tlbsync(0LL, allclr_cpumask, NO_VADDR);

	clean_stale_sptmap_post_op(bm);
	MINFO.tvirt++;
	return 1;
}


/*
 * newptes - called when a virtual translation is taken away - like shrink
 * and detach
 * This should be called rather than new_tlbpid since this can take care of
 * multi-threaded
 * For multi-threaded - the thread MUST have locked aspacelock(UPD)
 * shared - for shared processes, indicates whether local or global address
 *	    operation.  != 0 => global, == 0 => local
 *
 * WARNING - this routine may NOT be called unless:
 *	1) the current thread iscalling on its own behalf
 *	2) the caller can guarantee that the subject process
 *		isn't executing and cannot execute.
 *	This is becuase new_tlbpid could be zapping the tlbpid array at
 *	the same time as the process is starting to run, thus giving
 *	the target process a bogus tlbpid (or one that matches someone else)
 *
 * NOTE: yes, there are folks in /proc who violate this...
 */
void
newptes(pas_t *pas, ppas_t *ppas, uint shared)
{
	ppas_t *tppas;

	if (((pas->pas_flags & PAS_SHARED) == 0) || !shared) {
		/*
		 * either unmapping a MAP_LOCAL/private region or
		 * plain old boring process - just give it a new tlbpid and
		 * flush its wired entries
		 */
		setup_wired_tlb(0);
		new_tlbpid(ppas->ppas_utas, VM_TLBINVAL);
		return;
	}

	/*
	 * A multi-threaded process
	 *	since we have the UPDate lock - noone can be faulting
	 *	or otherwise mucking with their wired entries
	 *	So, we flush the entire tlb, and zap their wired entries
	 * tlbflush will also flush ALL wired entries so we first zap the
	 * the copies in the upage, then flush the entire tlb.
	 * the segment table is also zapped in setup_wired_tlb
	 */
	ASSERT(mrislocked_update(&pas->pas_aspacelock));
	
	mraccess(&pas->pas_plistlock);
	for (tppas = pas->pas_plist; tppas; tppas = tppas->ppas_next) {
		if (tppas->ppas_utas == NULL)
			continue;
		if (&curuthread->ut_as == tppas->ppas_utas) {
			setup_wired_tlb(1);
		} else {
			/* zap wired entries */
			setup_wired_tlb_notme(tppas->ppas_utas, 1);
		}
	}
	mrunlock(&pas->pas_plistlock);
	/* flush tlb -- at least we don't have to give them a new tlbpid :-) */
	tlbsync(BHV_TO_VAS(&pas->pas_bhv)->vas_gen,
		pas->pas_isomask, FLUSH_MAPPINGS|FLUSH_WIRED);
}
/*
 * newpmap - called when the pmap format is changed on the target.  Always
 * called 3rd party.  Target(s) must not run.
 */
void
newpmap(pas_t *pas, ppas_t *ppas)
{
	ppas_t *tppas;

	if ((pas->pas_flags & PAS_SHARED) == 0) {
		/*
		 * either unmapping a MAP_LOCAL/private region or
		 * plain old boring process - just give it a new tlbpid and
		 * flush its wired entries
		 */
		setup_wired_tlb_notme(ppas->ppas_utas, 1);
		new_tlbpid(ppas->ppas_utas, VM_TLBINVAL);
		return;
	}

	/*
	 * A multi-threaded process
	 *	since we have the UPDate lock - noone can be faulting
	 *	or otherwise mucking with their wired entries
	 *	So, we flush the entire tlb, and zap their wired entries
	 * tlbflush will also flush ALL wired entries so we first zap the
	 * the copies in the upage, then flush the entire tlb.
	 * the segment table is also zapped in setup_wired_tlb
	 */
	ASSERT(mrislocked_update(&pas->pas_aspacelock));
	
	mraccess(&pas->pas_plistlock);
	for (tppas = pas->pas_plist; tppas; tppas = tppas->ppas_next) {
		if (tppas->ppas_utas == NULL)
			continue;
		/* zap wired entries */
		setup_wired_tlb_notme(tppas->ppas_utas, 1);
	}
	mrunlock(&pas->pas_plistlock);
	/* flush tlb -- at least we don't have to give them a new tlbpid :-) */
	tlbsync(BHV_TO_VAS(&pas->pas_bhv)->vas_gen,
		pas->pas_isomask, FLUSH_MAPPINGS|FLUSH_WIRED);
}

/*
 * flush the tlb of the current uthread's mappings.
 * This of course only affects the processor the uthread is currently
 * running on, it is assumed that the caller is handling everything else
 */
void
flushtlb_current(void)
{
	int s;

	/* no preemption, no migration */
	s = splhi();
	setup_wired_tlb(1);
	new_tlbpid(&curuthread->ut_as, VM_TLBINVAL);
	splx(s);
	return;
}

/*
 * It invalidates the tlbpid for the process on all other cpus except the 
 * current cpu. This does not take care of sprocs and is called by large page 
 * code in vfault, to blow away invalid 16k tlb entries caused by tfault when
 * they are replaced by large page tlb entries.
 */
void
tlbinval_skip_my_cpu(struct utas_s *utas)
{
	register unsigned char *s;
	register int	i;

	/* This routine makes no sense if we can migrate to another cpu  */
	ASSERT_NOMIGRATE;

	MINFO.tdirt++;
	s = utas->utas_tlbpid;
	for (i = 0; i < maxcpus; i++, s++)
		if (i != cpuid())
			*s = TLBPID_NONE;
}

/*
 * For idbg, dump tlbinfo information.
 * If flag is set, dump full information; otherwise, just dump the
 * executive summary.
 */
void
dump_tlbinfo(int cpu)
{
	tlbinfo_t *tip;
	tlbpid_item_t *ti;
	int i;

	if ((cpu < 0) || (cpu > maxcpus))
		cpu = cpuid();

 	tip = pdaindr[cpu].pda->p_tlbinfo;
	qprintf("  tlb management info for cpu %d (0x%x)\n", cpu, tip);
	qprintf ("lastindex=%d count=%d ", tip->ti_lastindex, tip->ti_count);
	qprintf("nextinval=%d flush=%d\n", tip->ti_nextinval, tip->ti_flush);
#ifdef ULI
	qprintf("total pids=%d flush interval=%d\n", tip->ti_total_pids,
		tip->ti_flush_interval);
#endif


	qprintf("tlbpid (+lastuser) LRU list:\n");
	for (i=1, ti=tip->ti_tail->ti_next; i<TLBHI_NPID; i++, ti=ti->ti_next) {
		qprintf("%d (%d) ", ti-tip->ti_item, 
			tip->ti_lastuser[ti-tip->ti_item]);
		if ((i % 8) == 0)
			qprintf("\n");
	}
	qprintf("\n");
}


tlbpid_t *
tlbpid_memalloc(void)
{
	int i;
	tlbpid_t *tp = kmem_zone_alloc(tlbpid_zone, KM_SLEEP|VM_NO_DMA);
	tlbpid_t *tpt;

	for(i = maxcpus, tpt = tp; i > 0; i--, tpt++)
		*tpt = TLBPID_NONE;

	return tp;
}

void
tlbpid_memfree(tlbpid_t *tp)
{
	kmem_zone_free(tlbpid_zone, tp);
}


#if MP
/*
 * delayed action functions for isolated processor.
 * they are called on the isolated processor, upon entering the kernel via
 * syscall or interrupt.
 */
void
da_flush_tlb(void)
{
#ifdef R4000
	int i;
#endif

	/* could be called on entering the kernel at syscall or intr */
	flush_tlb(TLBFLUSH_RANDOM);
#if R4000 || R10000
	if (private.p_kvfault) {
		bzero(private.p_kvfault, sptsz / NBBY);
#if R4000
		for (i=0; i < CACHECOLORSIZE; i++)
			bzero(private.p_clrkvflt[i], clrsz / NBBY);
#endif	/* R4000 */
	}
#endif	/* R4000 || R10000 */
}

/*
 * called by syscall(), intr(), tlbmiss(), tlbmod(), trap()
 * Upon entry from user space to the kernel, we check to see if
 * we need to do any flushing, and enable broadcast interrupts for as
 * long as we are executing in kernel mode.
 *
 * Note: due to kernel preemption, we must re-check that we are on
 * an isolated processor after preventing premption/migration
 */
void
check_delay_tlbflush(int flag)
{
	register int s;

	/* no preemption, no migration */
	s = spl7();
	if (!(private.p_flags & PDAF_ISOLATED)
	    && !(private.p_flags & PDAF_BROADCAST_OFF))
	{
		/* preempted or migrated between macro check and spl */
		ASSERT(private.p_delayacvec == 0);
		splx(s);
		return;
	}

	KTRACE_TLBENTER(TLB_TRACE_CHECK_DEL, flag, 0, 0, 0);
	nested_spinlock(&private.p_special);
	if (flag == ENTRANCE) {
		private.p_flags &= ~PDAF_BROADCAST_OFF;
		if (private.p_delayacvec & DA_TLB_FLUSH) {
			private.p_delayacvec &= ~DA_TLB_FLUSH;
			KTRACE_TLBENTER(TLB_TRACE_FLUSH_DEL, 0, 0, 0, 0);
			nested_spinunlock(&private.p_special);
			da_flush_tlb();
			splx(s);
			return;
		}
#if TLB_TRACE && DEBUG
		if (dotesttlb)
			testtlb();
		if (dochkkvfault)
			chkkvfault(&sptbitmap, cpuid());
#endif
	} else if (private.p_flags & PDAF_ISOLATED) {	/* EXIT case */
		ASSERT(!(private.p_flags & PDAF_BROADCAST_OFF));
		ASSERT(!(private.p_delayacvec & DA_TLB_FLUSH));
		private.p_flags |= PDAF_BROADCAST_OFF;
	}
	mutex_spinunlock(&private.p_special, s);
}
#endif

#if defined (R10000)
/*
 * tlb_vaddr_to_pde: takes the given virtual address and the tlbpid,
 *			and fills in the pde information for this
 *			address using the tlb entry that maps this
 *			virtual address, if there is one.
 */

void
tlb_vaddr_to_pde(unsigned char tlbpid,
		 caddr_t	vaddr,
		 pde_t *pd_info)
{
	uint 		page_mask;
	__psint_t 	tlblo;
	__psunsigned_t  page_size;
	pfn_t pfn;

	pg_clrpgi(pd_info);

	tlblo = tlb_probe(tlbpid, vaddr, &page_mask);

	if (tlblo != -1) {
		pfn = (tlblo & TLBLO_PFNMASK) >> TLBLO_PFNSHIFT;
		/* convert tlb pfn to base pfn */
		pfn >>= PGSHFTFCTR;
		pg_setpfn(pd_info, pfn);


		pg_setccuc(pd_info,
			   ((tlblo & TLBLO_CACHMASK) >> TLBLO_CACHSHIFT),
			   ((tlblo & TLBLO_UATTRMASK) >> TLBLO_UATTRSHIFT));

		page_size = ((__psunsigned_t)(page_mask >> PGMASK_SHFT) + 1);
		page_size <<= MIN_PNUMSHFT;
		
		pg_set_page_mask_index(pd_info,
				       PAGE_SIZE_TO_MASK_INDEX(page_size));
		pg_setvalid(pd_info);
	}

	return;
}

#endif /* R10000 */

#if TLB_TRACE && DEBUG
static char *ev[] = {
	"tlbflush", "tlbsync", "tlbsync_del", "clean_age",
	"check_del", "flush_del", "tlbsync1",
};
extern char *tbl_pdaflg[];

static char *tab_syncflags[] = {
	"nosleep",		/* 0x01 */
	"steal_physmem",	/* 0x02 */
	"no_vaddr",		/* 0x04 */
	"flush_mappings",	/* 0x08 */
	"isolated_only",	/* 0x10 */
	"flush_wired",		/* 0x20 */
};

void
tlb_trace_entry(ktrace_entry_t *ktep)
{		  
	qprintf("%s:%d cpu %d ",
		ev[(long)ktep->val[4]],
		ktep->val[1],
		ktep->val[2]);
	prsymoff(ktep->val[0], "ra ", NULL);
	if (ktep->val[3] != 0) {
		printflags((ulong)(ktep->val[3]), tbl_pdaflg, " flags");
		qprintf("\n");
	} else {
		qprintf(" NO FLAGS SET\n");
	}
	switch((long)ktep->val[4]) {
	case TLB_TRACE_TLBFLUSH:
		qprintf(" asgen 0x%x curuthread 0x%x\n",
			ktep->val[5],
			ktep->val[6]);
			
		break;
	case TLB_TRACE_TLBSYNC:
		qprintf(" asgen 0x%x isomask 0x%x clean_sptmap 0x%x",
			ktep->val[5],
			ktep->val[6],
			ktep->val[8]);
		printflags((ulong) ktep->val[7], tab_syncflags, " flags");
		qprintf("\n");
		break;
	case TLB_TRACE_TLBSYNC1:
		qprintf(" kvmask 0x%x force %d localmask 0x%x\n",
			ktep->val[5],
			ktep->val[6],
			ktep->val[7]);
		break;
	case TLB_TRACE_TLBSYNC_DEL:
		qprintf(" target-cpu %d ", ktep->val[5]);
		printflags((ulong) ktep->val[6], tbl_pdaflg, " target-flags");
		qprintf("\n");
		break;
	case TLB_TRACE_CLEAN_AGE:
		qprintf(" isomask 0x%x\n", ktep->val[5]);
		break;
	case TLB_TRACE_CHECK_DEL:
		qprintf(" %s\n", ktep->val[5] == ENTRANCE ? "ENTRANCE" : "EXIT");
		break;
	case TLB_TRACE_FLUSH_DEL:
		break;
	}
}

/*
 * Print out the last "count" entries in the tlb trace buffer.
 */
static void
idbg_tlbtrace(__psunsigned_t count)
{
	ktrace_entry_t	*ktep;
	ktrace_snap_t	kts;
	int		nentries;
	int		skip_entries;

	if (tlb_trace_buf == NULL) {
		qprintf("The tlb trace buffer is not initialized\n");
		return;
	}
	nentries = ktrace_nentries(tlb_trace_buf);
	if (count == -1) {
		count = nentries;
	}
	if ((count <= 0) || (count > nentries)) {
		qprintf("Invalid count.  There are %d entries.\n", nentries);
		return;
	}

	ktep = ktrace_first(tlb_trace_buf, &kts);
	if (count != nentries) {
		/*
		 * Skip the total minus the number to look at minus one
		 * for the entry returned by ktrace_first().
		 */
		skip_entries = nentries - count - 1;
		ktep = ktrace_skip(tlb_trace_buf, skip_entries, &kts);
		if (ktep == NULL) {
			qprintf("Skipped them all\n");
			return;
		}
	}
	while (ktep != NULL) {
		tlb_trace_entry(ktep);
		ktep = ktrace_next(tlb_trace_buf, &kts);
	}
}

#define PPERUINT	(NBBY * sizeof(uint))

/*
 * for each element in the free sptmap, check whether there is a a valid
 * tlb translation
 */
static void
chkmap(struct bitmap *bm)
{
	uint		*p, mask;
	int		s, i, j, size;
	caddr_t		vaddr;
	uint		pg;
	__psint_t	rv;

	s = spl7();
	p = (uint *)bm->m_map;
	vaddr = (caddr_t)ctob(bm->m_unit0);
	ASSERT((sptsz % PPERUINT) == 0);
	size = sptsz / PPERUINT;
	for (i = 0; i < size; i++, p++, vaddr += ctob(PPERUINT)) {
		if ((mask = *p) == 0)
			continue;
		for (j = 0; j < PPERUINT; j++) {
			if ((mask & (1 << j)) == 0)
				continue;
			rv = tlb_probe(0, vaddr + ctob(j), &pg);
			if (rv != -1 && ((rv & (TLBLO_V|TLBLO_D|TLBLO_G)) != TLBLO_G)) {
				cmn_err(CE_CONT, "chkmap: vaddr 0x%x in tlb! tlblo 0x%x *p 0x%x j %d i %d\n",
				vaddr + ctob(j), rv, *p, j, i);
				debug(0);
			}
		}
	}
	splx(s);
}

/*
 * Simple test to check kvfault bits ..
 * Only should have 1 cpu isolated...
 */
void
testkvfault(void)
{
	cpumask_t iso;
	struct sysbitmap *clean_sptmap;
	caddr_t v;
	int s;

	/* first clear out any kvfault bits we might have on as well
	 * as any stale K2SEG addresses
	 */
	clean_stale_sptmap(&sptbitmap, 0);

	/* test color maps */
	v = kvpalloc(1, VM_VACOLOR|VM_VM, 1);
	ASSERT(IS_KSEG2(v));
	*v = 0x1;
	kvpfree(v, 1); /* should move it to stale map */

	/* this moves stale to temp */
	s = splhi();
	if (clean_sptmap = clean_stale_sptmap_pre_op(&sptbitmap)) {
		iso = kvfaultcheck(NO_VADDR);
		ASSERT(CPUMASK_TSTM(iso, private.p_cpumask));

		splx(s);
		tlbsync(0, NO_VADDR);
		clean_stale_sptmap_post_op(clean_sptmap);
	} else
		splx(s);


	/* now test regular */
	v = kvpalloc(1, VM_VM, 0);
	ASSERT(IS_KSEG2(v));
	*v = 0x1;
	kvpfree(v, 1); /* should move it to stale map */

	/* this moves stale to temp */
	s = splhi();
	if (clean_sptmap = clean_stale_sptmap_pre_op(&sptbitmap)) {
		iso = kvfaultcheck(NO_VADDR);
		ASSERT(CPUMASK_TSTM(iso, private.p_cpumask));

		splx(s);
		tlbsync(0, NO_VADDR);
		clean_stale_sptmap_post_op(clean_sptmap);
	} else
		splx(s);
}

#if     TFP
extern  void tlbgetent(int, int, void *, void *);
#endif
#if     R4000 || R10000
extern  void tlbgetent(int, k_machreg_t *, uint *, uint *);
#endif
int vt = 0;

/*
 * go through each tlb entry and make sure that no K2SEG address is mapped
 * that is in the free spt bit map
 */
static void
testtlb(void)
{
	int s, i, firstbit;
	k_machreg_t hi;
	uint lo0, lo1;
	caddr_t vaddr;
	struct bitmap *bm;
	pgno_t vpn;

	s = splhi();
	for (i = 0; i < NTLBENTRIES; i++) {
		tlbgetent(i, &hi, &lo0, &lo1);
		vaddr = (caddr_t)(hi & TLBHI_VPNMASK);
		if (!IS_KSEG2(vaddr))
			continue;
		if (lo0 & TLBLO_V) {
			/* even entry valid */
			vpn = btoc(vaddr);
#if R4000
			if (vpn >= sptbitmap.sptmap.m_color[0]->m_unit0) {
				bm = sptbitmap.sptmap.m_color[vpn & CACHECOLORMASK];
				firstbit = vpn - bm->m_unit0;
				firstbit /= CACHECOLORSIZE;
				if (vt)
					qprintf("even color %d  vpn 0x%x fb 0x%x\n",
						vpn & CACHECOLORMASK, vpn, firstbit);
			} else
#endif /* R4000 */
			{
				bm = &sptbitmap.sptmap;
				firstbit = vpn - bm->m_unit0;
				if (vt)
					qprintf("even reg vpn 0x%x fb 0x%x\n", vpn, firstbit);
			}
			if (bftstclr(bm->m_map, firstbit, 1) != 1) {
				qprintf("tlbentry %d even hi 0x%x in spt free map! bitmap 0x%x firstbit %d\n",
					i, hi, bm, firstbit);
				debug(0);
			}
		}
		if (lo1 & TLBLO_V) {
			/* odd entry valid */
			vaddr += ctob(1);
			vpn = btoc(vaddr);
#if R4000
			if (vpn >= sptbitmap.sptmap.m_color[0]->m_unit0) {
				bm = sptbitmap.sptmap.m_color[vpn & CACHECOLORMASK];
				firstbit = vpn - bm->m_unit0;
				firstbit /= CACHECOLORSIZE;
				if (vt)
					qprintf("odd color %d  vpn 0x%x fb 0x%x\n",
						vpn & CACHECOLORMASK, vpn, firstbit);
			} else
#endif /* R4000 */
			{
				bm = &sptbitmap.sptmap;
				firstbit = vpn - bm->m_unit0;
				if (vt)
					qprintf("odd reg vpn 0x%x fb 0x%x\n", vpn, firstbit);
			}
			if (bftstclr(bm->m_map, firstbit, 1) != 1) {
				qprintf("tlbentry %d odd hi 0x%x in spt free map! bitmap 0x%x firstbit %d\n",
					i, hi, bm, firstbit);
				debug(0);
			}
		}
	}
	splx(s);
}

/*
 * check that kvfault doesn't contain any entries that correspond to
 * 'free' sptmap entries that are still mapped ....
 */
static void
chkkvfault(struct sysbitmap *bm, cpuid_t id)
{
	uint		*p, *pfree, mask;
	pda_t		*npda;
	int		s, i, j, k, size;
	caddr_t		vaddr;
	struct bitmap	*bitmap;
	uint		pg;
	__psint_t	rv;

	npda = pdaindr[id].pda;
	if ((p = (uint *)npda->p_kvfault) == 0)
		return;

	s = spl7();
	bitmap = &bm->sptmap;
	pfree = (uint *)bitmap->m_map;
	vaddr = (caddr_t)ctob(bitmap->m_unit0);
	ASSERT((sptsz % PPERUINT) == 0);
	size = sptsz / PPERUINT;
	for (i = 0; i < size; i++, p++, pfree++, vaddr += ctob(PPERUINT)) {
		if ((mask = (*p & *pfree)) == 0)
			continue;
		for (j = 0; j < PPERUINT; j++) {
			if ((mask & (1 << j)) == 0)
				continue;
			rv = tlb_probe(0, vaddr + ctob(j), &pg);
			if (rv != -1 &&
			    ((rv & (TLBLO_V|TLBLO_D|TLBLO_G)) != TLBLO_G)) {
				cmn_err(CE_CONT, "chkkv: vaddr 0x%x in tlb! tlblo 0x%x *p 0x%x *pfree 0x%x j %d i %d\n",
				vaddr + ctob(j), rv, *p, *pfree, j, i);
				debug(0);
			}
		}
	}

#if R4000
	/*
	 * now do color maps
	 * These are interwoven
	 */
	ASSERT((clrsz % PPERUINT) == 0);
	size = clrsz / PPERUINT;
	for (k = 0; k < CACHECOLORSIZE; k++) {
		if ((p = (uint *)npda->p_clrkvflt[k]) == NULL)
			continue;
		bitmap = bm->sptmap.m_color[k];
		pfree = (uint *)bitmap->m_map;
		vaddr = (caddr_t)ctob(bitmap->m_unit0);
		for (i = 0; i < size; i++, p++, pfree++, vaddr += CACHECOLORSIZE * ctob(PPERUINT)) {
			if ((mask = (*p & *pfree)) == 0)
				continue;
			for (j = 0; j < PPERUINT; j++) {
				if ((mask & (1 << j)) == 0)
					continue;
				rv = tlb_probe(0, vaddr + CACHECOLORSIZE * ctob(j), &pg);
				if (rv != -1 && ((rv & (TLBLO_V|TLBLO_D|TLBLO_G)) != TLBLO_G)) {
					cmn_err(CE_CONT, "chkkv: color %d vaddr 0x%x in tlb! tlblo 0x%x i %d *p 0x%x *pfree 0x%x j %d\n",
						k,
						vaddr + CACHECOLORSIZE * ctob(j),
						rv, i, *p, *pfree, j);
					debug(0);
				}
			}
		}
	}
#endif
	splx(s);
}

static void
prkvfault(struct sysbitmap *bm, cpuid_t id)
{
	uint *p;
	pda_t *npda;
	int i, j, k, size;
	caddr_t vaddr;
	struct bitmap *bitmap;

	npda = pdaindr[id].pda;
	if ((p = (uint *)npda->p_kvfault) == 0) {
		qprintf("no kvfault for cpu %d\n", id);
		return;
	}

	qprintf("kvfault for cpu %d\n", id);
	bitmap = &bm->sptmap;
	vaddr = (caddr_t)ctob(bitmap->m_unit0);
	qprintf("sptmap starts at 0x%x for %d pages\n", vaddr, sptsz);
	size = sptsz / PPERUINT;
	for (i = 0; i < size; i++, p++, vaddr += ctob(PPERUINT)) {
		if (*p == 0)
			continue;
		for (j = 0; j < PPERUINT; j++)
			if (*p & (1 << j))
				qprintf("0x%x ", vaddr + ctob(j));
	}
	qprintf("\n");

#if R4000
	/*
	 * now do color maps
	 * These are interwoven
	 */
	size = clrsz / PPERUINT;
	for (k = 0; k < CACHECOLORSIZE; k++) {
		bitmap = bm->sptmap.m_color[k];
		vaddr = (caddr_t)ctob(bitmap->m_unit0);
		qprintf("colormap %d starts at 0x%x for %d pages\n",
			k, vaddr, clrsz);
		if ((p = (uint *)npda->p_clrkvflt[k]) == NULL)
			continue;
		for (i = 0; i < size; i++, p++, vaddr += CACHECOLORSIZE * ctob(PPERUINT)) {
			if (*p == 0)
				continue;
			for (j = 0; j < PPERUINT; j++)
				if (*p & (1 << j))
					qprintf("0x%x ", vaddr + CACHECOLORSIZE * ctob(j));
		}
		qprintf("\n");
	}
#endif
}

/* ARGSUSED */
static void
idbg_kvfault(__psunsigned_t x)
{
	int i;

	for (i = 0; i < maxcpus; i++) {
		if (pdaindr[i].CpuId == -1)
			continue;
		prkvfault(&sptbitmap, i);
	}
}
#endif
