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
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#include "sys/types.h"
#include "sys/anon.h"
#include "ksys/as.h"
#include "os/as/as_private.h"
#include "sys/atomic_ops.h"
#include "sys/buf.h"
#include "sys/cmn_err.h"
#include "sys/cred.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "sys/fault_info.h"
#include "sys/ksa.h"
#include "sys/fstyp.h"
#include "sys/getpages.h"
#include "sys/lpage.h"
#include "sys/immu.h"
#include "sys/idbgentry.h"
#include "sys/kabi.h"
#include "sys/kmem.h"
#include "sys/ktrace.h"
#include "sys/mman.h"
#include "sys/numa.h"
#include "sys/page.h"
#include "sys/param.h"
#include "sys/par.h"
#include "sys/pda.h"			/* for isolated proc */
#include "sys/pfdat.h"
#include "os/as/pmap.h"
#include "sys/sema.h"
#include "os/vm/scache.h"
#include "sys/schedctl.h"
#include "ksys/sthread.h"
#include "sys/swap.h"
#include "sys/sysinfo.h"
#include "sys/sysmacros.h"
#include "sys/systm.h"
#include "sys/tuneable.h"
#include "sys/uthread.h"
#include "sys/var.h"
#include "sys/vnode.h"
#include "ksys/vpag.h"
#include "ksys/vmmacros.h"
#include "ksys/vm_pool.h"
#include "sys/nodepda.h"

#ifdef	DEBUG
void	valid_reg_count(void);
#endif

/*
 * XXX anon-shake
 *	sysreg
 *	Get around locked/SXBRK regions:
 *		if SXBRKd proc owns a locked region - borrow lock ...
 *	unrefed shmem regions
 */
/*
 *	In order to accomodate for the absence of reference bits in the
 *	MIPS tlb, the valid bit and two software fields in the pde are
 *	used to simulate reference bit maintainance through valid faults.
 *	This table describes the bit settings and what they mean:
 *
 *			vr - valid and referenced bit
 *			sv - software valid
 *			nr - (three) counting reference bits
 *
 *	 vr sv nr		meaning
 *	 ------------------------------
 *	 1  1  XXX		A valid page in the process working set.
 *	 0  1  >tune.t_gpgsmsk	Page is in the working set, but has been
 *				set invalid in order to detect a reference.
 *				The page frame is still owned by the process.
 *	 0  1  <=tune.t_gpgsmsk	Page is not in the working set. The page
 *				frame is still owned by the process, but it
 *				is a candidate for reclamation by getpages.
 *	 0  0  XXX		The page is truly invalid. Full vfault()
 *				processing is required for this pte.
 *
 *	Note that this is a two-level scheme that may not be required if
 *	the order of ageregion() and getpages is reversed.
 *	It could be reduced to:
 *
 *	vr sv	meaning
 *	---------------
 *	1  1	Valid page in working set (recently referenced).
 *	0  1	Valid page set invalid to catch reference. Subject to reclaim.
 *	0  0	Invalid page. Full vfault processing required.
 */

int		rsskicklim;	/* how many remaining pages before kick	*/
				/* vhand  (due to RSS hogs)		*/
int		vhandkicklim;	/* how many remaining pages before kick	*/
				/* vhand  (due to lots of paging)	*/
sema_t		vhandsema;	/* vhand() synchronization		*/
static int	vhandnrmask;	/* vhand local tune.t_gtpgsmsk mask	*/

lock_t		vhandwaitlock;	/* lock to acces vhandwaitcnt/rsswaitcnt*/
int		rsswaitcnt;	/* # processes waiting for RSS prune	*/
sv_t		rsswaitsema;	/* wait sema for RSS prune		*/

uint_t		vkickedpages;	/* # pages kicked loose by vhand 	  */
uint_t		vscan;		/* vscan # */
sema_t		shaked_wait;	/* shaked() synchronization		*/

static void shaked(void);
static void vhand(void);
#if CELL
extern void unimport_page_demon(void);
#endif
static int	pchainpages;	/* # pages ignored due to double listing */
static int	outofswap;	/* # times out of swap */

static int freechunk(reg_t *);
static int writechunk(reg_t *);
static int swapchunk(reg_t *);
static pgcnt_t getpages(pas_t *, preg_t *, int, pgcnt_t, int[]);
static int tosspages(pas_t *, preg_t *, pde_t *, pgno_t, pgno_t, int, int[],  pgno_t *);
static int tossmpages(pas_t *, preg_t *, pde_t *, pgno_t, int);
static pgcnt_t howhard(preg_t *, as_shake_t *);
static void bumprcnt(preg_t *);
static pgcnt_t stealfrom(pas_t *, preg_t *, int, as_shake_t *);
static int getpages_epilogue(void);
static pgcnt_t spinreg(pas_t *, preg_t *, pgcnt_t, int);
static pgcnt_t howhardrss(pgcnt_t, pgcnt_t, pgcnt_t, pgcnt_t, pgcnt_t);
static void addpg(pas_t *, preg_t *, pde_t *, int, pgno_t, pfd_t *, int);
static void waitforswapdone(void);
static void freelist(pglst_t *, int, reg_t *);
static void memfree(pglst_t *, int, reg_t *, int);
static void getsanonpages(int);
static int test_toss_page(preg_t *, pde_t *, int);
int pagerealloc(pfd_t *);

/*
 * On mp systems, another process whose pages we are freeing could be
 * running at the same time.  We must ensure that the tlb is flushed
 * before the region is unlocked.  Instead of merely setting a bit
 * in tlbflushmask, we must force an immediate flush on all processors.
 * This is done by tlbsync, which must be called before releasing a
 * region whose pages have been modified.
 */

/* non-static for debugging */
struct pglsts pglsts[NPGLSTS];

/* mktables allocates these */
pglst_t *pglst;
gprgl_t	*gprglst;

static sema_t	swapwaitsema;
static int	gprgndx;
static int	swaplist = SPGLST0;	/* which swap list to use */
static lock_t	swapdonelock;		/* lock to protect swapdone code */

/*
 * data structures to keep track of order of processes that should have
 * pages stolen from first
 * This is sorted by priority
 */
prilist_t	*prilist;	/* sorted list of processes */
int		curprilist;
#ifdef DEBUG
struct ktrace	*vhand_trace;
static void idbg_vhand_trace(__psint_t);
static int	vhand_trace_level = 1;
#endif
pgno_t		youngpages;	/* pages not kicked due to LRU */
pgno_t		swapmin;	/* min # contiguous swap */
pgno_t		swapmax;	/* max # contiguous swap */


/*
 * Contains the cleaner list a base  page of a large page should be added.
 * Used in tosspages.
 */
static	int	large_page_pglst[btoct(MAX_PAGE_SIZE)];

extern mutex_t	sanon_lock;
extern char *	kvswap_map;	/* map of kvswappable pages in sysreg */

/*
 * return values for test_toss_page - most of these are for debugging only
 */
#define NOPFNLOCK	-1
#define YOUNG		-2
#define NOTVALID	-3
#define RAWORBAD	-4
#define ALREADYSWAP	-5
#define NOTLEAF		-6
#define NOMFILE		-7
#define PFDERROR	-8
#define MAXREASONS	9	/* 0-8 */

/*
 * Max value for rss kick lim.
 * This puts a cap on the rsskick lim variable and prevent swapping on very
 * large memory machines when there is several Gigs of free memory left.
 */
#define	MAX_KICK_LIM	btoc(100*1024*1024) /* 100m worth of pages */

/*	If the current region was not put
 *	into the list of pages to be swapped
 *	out, then unlock the region now.
 */
static void
getpages_denouement(register reg_t *rp)
{
	if (gprglst[gprgndx].gp_count == 0 ||
	    gprglst[gprgndx].gp_prp->p_reg != rp) {
		regrele(rp);
	}
}

/*
 * Epilogue code for getpages.
 * Push out any accumulated pages and unlock all of the locked regions.
 * Returns 0 if all ok
 *	else return from cleaners. Currently this implies that we ran
 *	     out of swap space
 */
static int
getpages_epilogue(void)
{
	register int i, err;

	err = 0;
	for (i = 0; i < NPGLSTS; i++) {
		/*
		 * skip non-current swap list
		 * This flushes current 'swaplist', and changes list to
		 * the other swap list
		 * Thus the wait below really waits on the original 'swaplist'
		 */
		if (i == ((swaplist == SPGLST0) ? SPGLST1 : SPGLST0))
			continue;
		if (pglsts[i].index)
			err += (pglsts[i].cleaner)(NULL);
	}
	waitforswapdone();
#ifdef DEBUG
	for (i = 0; i < NPGLSTS; i++)
		ASSERT(pglsts[i].index == 0);
#endif
	ASSERT(gprglst[gprgndx].gp_count == 0);
	return(err);
}



/*
 * pmaptrim_postpone
 *
 * This routine will attempt to cancel an outstanding "trim" request.
 * It returns a status that indicates if a trim request was outstanding.
 *
 * If a pass of getpages removes pages from a pmap, it sets
 * pmap_trimtime to a positive number. Trimming will occur later, usually
 * in another thread.
 *
 * Trimming is in progress if pmap_trim is < 0 (set in pmap_trim &
 * called by pas_vfault).  Trimming will occur ONLY
 * when pmap_trim sees a positive pmap_trimtime. pmap_trim atomically
 * sets pmap_trimtime to -1 while trimming is happening.
 *
 */

#define	PMAPTRIM_IN_PROGRESS	0
#define	PMAPTRIM_INACTIVE	1
#define	PMAPTRIM_CANCELED	2

int
pmaptrim_postpone(pmap_t *pmap)
{
	time_t pmaptrimtime;

	if((pmaptrimtime = pmap->pmap_trimtime) == 0){
		/* 
		 * It's still zero. So, will never race with sched.
		 */
		return PMAPTRIM_INACTIVE;
	}

	/*
	 * pmaptrimtime 
	 *	> 0 indicates we set this in some other pass,
	 * 	    of vhand. So, there could be a race..
	 *	-1 indicating pmap_trim is looking at it.
	 *
	 * If pmap_trim is looking at it, don't try to steal right
	 * now. Let pmap_trim finish with it. 
	 *
	 * If the value is > 0, set it atomically to 0 so that 
	 * pmap_trim does not look at this process while we are 
	 * rummaging through it. Once we are done, either if we
	 * decide to trim, or if pmaptrimtime was non-zero, set
	 * pmap_trimtime to lbolt.
	 */
	if (pmaptrimtime == -1) {
		/* 
		 * pmap_trim is busy with this process. can't delay trimming..
		 */
		return PMAPTRIM_IN_PROGRESS;
	}

	if (!compare_and_swap_int
		((int *)&pmap->pmap_trimtime, pmaptrimtime, 0)){
		/*
		 * pmap_trim raced with us to trim processe's page tables.
		 * Give up ageing pages from process for now.
		 */
		return PMAPTRIM_IN_PROGRESS;
	}
	/*
	 * Assertion should catch any race conditions
	 */
	ASSERT(pmap->pmap_trimtime == 0);
	ASSERT(pmaptrimtime > 0 );

	return PMAPTRIM_CANCELED;
}


/*	This process is awakened periodically by clock to update the
 *	system's idea of the working sets of all processes and to
 *	steal pages from processes if GLOBAL_FREEMEM is too low.
 *
 * Things to do:
 *	1) vhand has enough info to swap out people thus really getting
 *		rid of most of sched
 *	2) by looking at how many pages vhand kicks versus GLOBAL_FREEMEM one
 *		could get a good idea of whether swapping is necessary
 *	3) tuning the priority list - maybe it should be better sorted...
 */
void
pager_init(void)
{
	extern int vhand_pri;
	extern int shaked_pri;
	extern int exim_pri;

	sthread_create("vhand", 0, 2048*sizeof(void*), 0, vhand_pri, KT_PS,
			(st_func_t *)vhand, 0, 0, 0, 0);
	sthread_create("shaked", 0, 8192, 0, shaked_pri, KT_PS,
			(st_func_t *)shaked, 0, 0, 0, 0);
#if CELL
	sthread_create("exim", 0, 0, 0, exim_pri, KT_PS,
			(st_func_t *)unimport_page_demon, 0, 0, 0, 0);
#endif
}

static void
vhand(void)
{
	register prilist_t *mp;
	int		i, s;
	static int	notake = 0;
	extern int kmem_zone_shake();
	vasid_t vasid;
	int npscanned;
	/* REFERENCED */
	time_t slbolt;

	ASSERT(!issplhi(getsr()));
	/* init basic semaphores */
	initnsema(&vhandsema, 0, "vhand");
	initnsema(&swapwaitsema, 0, "swpwait");
	semahistoff(&vhandsema);
	spinlock_init(&swapdonelock, "swpdone");
	shake_register(SHAKEMGR_MEMORY, kmem_zone_shake);

	prilist = (prilist_t *) kern_malloc(sizeof(*prilist)*nproc);
#ifdef DEBUG
	vhand_trace = ktrace_alloc(1000, 0);
	idbg_addfunc("vhandtrace", idbg_vhand_trace);
#endif

	/*
	 * init vhand wait synchronizer
	 */
	spinlock_init(&vhandwaitlock, "vhwlock");
	sv_init(&rsswaitsema, SV_DEFAULT, "rssw");
	rsswaitcnt = 0;

	ASSERT(maxpglst > 0 && maxpglst <= MAXPGLST);

	pglsts[SPGLST0].cleaner = swapchunk;
	pglsts[SPGLST1].cleaner = swapchunk;
	pglsts[FPGLST].cleaner = freechunk;
	pglsts[DPGLST].cleaner = writechunk;

	for (i = 0, s = 0; i < NPGLSTS; i++) {
		pglsts[i].list = &pglst[s];
		s += maxpglst;
	}

	for (;;) {
		ASSERT(!issplhi(getsr()));

		/* protect against tune struct changes */
		/* XX */
		/*
		 * set the amount of memory before kicking vhand
		 * This is either the high water mark (gpgshi) OR
		 *	the amount of memory RSS hogs can use up before
		 *	we come in and trim them
		 */
		rsskicklim = maxmem - ((tune.t_rsshogfrac * maxmem) / 100);

		/*
		 * Put a cap on rsskicklim at 100 Mb.
		 */
		if (rsskicklim > MAX_KICK_LIM)
			rsskicklim = MAX_KICK_LIM;

		vhandkicklim = tune.t_gpgshi;

		/*	If tune.t_maxsc has been changed during the
		 *	last pass through the regions or while we
		 *	were sleeping, then use the new limit now.
		 */
		ASSERT(tune.t_maxsc <= maxpglst);
		ASSERT(tune.t_maxdc <= maxpglst);
		ASSERT(tune.t_maxfc <= maxpglst);

		ASSERT(pglsts[SPGLST0].index == 0);
		ASSERT(pglsts[SPGLST1].index == 0);
		pglsts[SPGLST0].limit = tune.t_maxsc;
		pglsts[SPGLST1].limit = tune.t_maxsc;

		/*	If tune.t_maxdc has been changed during the
		 *	last pass through the regions or while we
		 *	were sleeping, then use the new limit now.
		 */
		ASSERT(pglsts[DPGLST].index == 0);
		pglsts[DPGLST].limit = tune.t_maxdc;

		/*	If tune.t_maxfc has been changed during the
		 *	last pass through the regions or while we
		 *	were sleeping, then use the new limit now.
		 */
		ASSERT(pglsts[FPGLST].index == 0);
		pglsts[FPGLST].limit = tune.t_maxfc;

		/*
		 * Phase 1: Look at RSS - 
		 * run through all AS's and see if they have exceeded
		 * their RSS
		 */
		/*
		 * since RSS trimming doesn't do ageing, we
		 * don't waste time trying to do LRU too much
		 */
		vhandnrmask = GTPGS_MOSTRECENT-1;

		KTRACE6(vhand_trace,
				"START rss", GLOBAL_FREEMEM(),
				"lbolt", lbolt,
				"rsswaitcnt", rsswaitcnt);
		for ( ; vhandnrmask <= GTPGS_MOSTRECENT; vhandnrmask++) {
			as_scan(AS_RSSSCAN, AS_SCAN_LOCAL, NULL);
		}
		if (rsswaitcnt > 0) {
			/* if any RSS guys then wake up waiters */
			s = mp_mutex_spinlock(&vhandwaitlock);
			rsswaitcnt = 0;
			sv_broadcast(&rsswaitsema);
			mp_mutex_spinunlock(&vhandwaitlock, s);
		}

#ifdef DEBUG
		valid_reg_count();
#endif

		/*
		 *
		 * NOW STARTS DEMAND DRIVEN PAGING
		 *
		 */
		slbolt = lbolt;
		youngpages = 0;
		swapmin = -1;
		swapmax = -1;

                /*
                 * At this point we force a recalculation of the global
                 * system free memory to make sure we will not fall into
                 * the next phase when there's already enough global
                 * free memory available, but GLOBAL_FREEMEM hasn't been
                 * updated by the one-second tick yet.
                 */
                GLOBAL_FREEMEM_UPDATE();
		KTRACE12(vhand_trace,
				"start demand", GLOBAL_FREEMEM(),
				"lbolt", lbolt,
				"vhandkicklim", vhandkicklim,
				"gpgslo", tune.t_gpgslo,
				"gpgshi", tune.t_gpgshi,
				"gpgsmask", tune.t_gpgsmsk);
                
		/* no ageing, etc. unless lower than high watermark */

		if (
#ifdef MH_R10000_SPECULATION_WAR
		    (GLOBAL_FREEMEM() - smallk0_freemem) > vhandkicklim &&
                    ((pfn_lowmem == 0) || (smallk0_freemem > 0))
#else /* MH_R10000_SPECULATION_WAR */
		    GLOBAL_FREEMEM() > vhandkicklim
#endif /* MH_R10000_SPECULATION_WAR */
		    )
			goto vhandout;

		/*
		 * Phase 2 - kick system VM.
		 */
		if (syspreg.p_nvalid && creglock(&sysreg)) {
			pde_t *pt;
			auto pgno_t size, sz, rpn, skip, max;

			rpn = 0;
			pt = kvtokptbl(syspreg.p_regva);
			size = syssegsz;

			while (size) {
				/* Skip to the beginning of the next bunch
				 * of swappable pages.
				 */
				skip = bftstclr(kvswap_map, rpn, size);
				rpn += skip;
				pt += skip;
				size -= skip;

				if (size == 0)
					break;

				/*
				 * Find number of contiguous,
				 * swappable pages
				 */
				sz = bftstset(kvswap_map, rpn, size);

				/*
		 		 * Call tosspages directly -- kernel pmap
				 * is weird!
				 */
				max = sz;
				(void)tosspages(0, &syspreg, pt, rpn,
					sz, GTPGS_HARD, NULL, &max);

				rpn += sz;
				pt += sz;
				size -= sz;
			}

			/* if no pages queued then free sysreg */
			getpages_denouement(&sysreg);
			getpages_epilogue();
		}

		/*
		 * Phase 2a - shake any system memory from various
		 * pools (e.g. vnodes).
		 * Wake up the shake daemon.  The shake
		 * routines can now involve I/O which can require memory
		 * or buffers that we might need to free up from vhand.
		 */
		cvsema(&shaked_wait);

		if (
#ifdef MH_R10000_SPECULATION_WAR
		    (GLOBAL_FREEMEM() - smallk0_freemem) > vhandkicklim &&
                    ((pfn_lowmem == 0) || (smallk0_freemem > 0))
#else /* MH_R10000_SPECULATION_WAR */
		    GLOBAL_FREEMEM() > vhandkicklim
#endif /* MH_R10000_SPECULATION_WAR */
                    )
			goto vhandout;

		/*
		 * Toss unreferenced shared anon pages.
		 */
		getsanonpages(vhandkicklim);

		/*
		 * if we aged but took no pages last 'n' times, and
		 * we have > gpgslo pages then just forget everything
		 * If things get worse (memory goes < gpgslo) we'll have
		 * the aging info, otherwise why waste time when we're
		 * not really going to page anything??
		 */
		if ((notake > (GTPGS_MOSTRECENT - tune.t_gpgsmsk)) &&
#ifdef MH_R10000_SPECULATION_WAR
		    (GLOBAL_FREEMEM() - smallk0_freemem) > tune.t_gpgslo &&
		    ((pfn_lowmem == 0) || (smallk0_freemem > 0))
#else /* MH_R10000_SPECULATION_WAR */
		    GLOBAL_FREEMEM() > tune.t_gpgslo
#endif /* MH_R10000_SPECULATION_WAR */
		    ) {
			KTRACE4(vhand_trace,
				"NOTAKE so no age", GLOBAL_FREEMEM(),
				"notake", notake);
			goto vhandout;
		}

                /*
                 * Once more  we force a recalculation of the global
                 * system free memory to make sure we will not fall into
                 * the next phase when there's already enough global
                 * free memory available, but GLOBAL_FREEMEM hasn't been
                 * updated by the one-second tick yet.
                 */
                GLOBAL_FREEMEM_UPDATE();
                
		/*
		 * Phase 3 - standard ageing
		 */
		/*
		 * Typically, we would scan down the active region list
		 * here, throwing out pages right and left, ignoring everything
		 * but reference lists.  This doesn't work on a fast CPU.
		 * Consider the high priority process.  It gets to run, spins
		 * through it's page quickly, touches a new one, and Blam!
		 * into the fault code.  Along comes vhand(), notices that
		 * his pages haven't been touched, and scarfs some pages.
		 * This high priority process is thus effectively deadlocked.
		 *
		 * Instead of doing this, we will scan down the process
		 * table in several passes, stealing pages in a priority
		 * order fashion.  This should tend to keep wanted pages in
		 * core.
		 */
		KTRACE4(vhand_trace,
				"start ageing", GLOBAL_FREEMEM(),
				"lbolt", lbolt);

		as_scan(AS_AGESCAN, AS_SCAN_LOCAL, NULL);

		/*
		 * we might have some pages freed from stealing from tsave
		 * regions 
		 */
		getpages_epilogue();

		/* set up for reference bit faults */
		tlbsync(0LL, allclr_cpumask,  STEAL_PHYSMEM);

		/*
		 * If enough memory, then do no stealing.
		 */
		if (
#ifdef MH_R10000_SPECULATION_WAR
		    (GLOBAL_FREEMEM() - smallk0_freemem) >= tune.t_gpgslo &&
                    ((pfn_lowmem == 0) || (smallk0_freemem > 0))
#else /* MH_R10000_SPECULATION_WAR */
		    GLOBAL_FREEMEM() > tune.t_gpgslo
#endif /* MH_R10000_SPECULATION_WAR */
		    ) {
			notake++;
			goto vhandout;
		}

                /*
                 * This is the last time we force a recalculation of the global
                 * system free memory to make sure we will not fall into
                 * the next phase when there's already enough global
                 * free memory available, but GLOBAL_FREEMEM hasn't been
                 * updated by the one-second tick yet.
                 */
                GLOBAL_FREEMEM_UPDATE();
                
		/*
		 * Phase 4 - start stealing from normal processes
		 */
		notake = 0;

		vhandnrmask = tune.t_gpgsmsk;

		KTRACE4(vhand_trace,
				"start stealing", GLOBAL_FREEMEM(),
				"lbolt", lbolt);

		if (as_vhandscan(prilist, nproc) == 0) {
			/* nothing to do! */
			goto vhandout;
		}

		mp = &prilist[0];
		npscanned = 0;
		while (mp != 0) {
			/*
			 * Walk the tree in reverse order.  No recursive
			 * search here - we have to be a little uglier.
			 * State is kept through the right, vas, and pid
			 * variables in each record.  We clear 'right' after
			 * scanning the right path, we clear 'vas' after
			 * processing the current node, and remember that we
			 * walked the left branch by zeroing 'state' before
			 * descending.
			 */
			if (mp->vas == 0) /* processed this node already */
				goto chkleft;
			while (mp->right != 0)	/* find terminating node */
				mp = mp->right;

			vasid.vas_obj = mp->vas;
			vasid.vas_pasid = 0;
			/*
			 * see if enough mem
			 * XXX doesn't take into consideration that
			 * there may be up to PGLST pages that will be
			 * pushed when we call getpages_epilogue()
			 * It is dangerous to just add up the pglst indexes
			 * since the swap out or file write might fail
			 * If enough, don't steal - just unref.
			 */
			if (
#ifdef MH_R10000_SPECULATION_WAR
                            (GLOBAL_FREEMEM() - smallk0_freemem) <= vhandkicklim ||
			    ((pfn_lowmem != 0) && (smallk0_freemem == 0))
#else /* MH_R10000_SPECULATION_WAR */
			    GLOBAL_FREEMEM() <= vhandkicklim
#endif /* MH_R10000_SPECULATION_WAR */
			    ) {

#if DEBUG
				if (vhand_trace_level > 1)
					KTRACE10(vhand_trace,
						"walk-tree", GLOBAL_FREEMEM(),
						"vas:", mp->vas,
						"pri:", mp->sargs.as_shakepage_pri,
						"slptime:", mp->sargs.as_shakepage_slptime,
						"state:", mp->sargs.as_shakepage_state);
#endif

				/* steal some memory */
				VAS_SHAKE(vasid, AS_SHAKEPAGE, &mp->sargs);
				npscanned++;
			}
			as_rele_common(vasid);

			/*
			 * Now do the same thing on the left branch.
			 */
		    chkleft:
			mp->vas = 0;
			if (mp->left == 0 || mp->sargs.as_shakepage_state == 0) {
				/*
				 * Done with this node.
				 */
				mp = mp->parent;
				if (mp)
					mp->right = 0;
				continue;
			}
			mp->sargs.as_shakepage_state = 0; /* mark as done */
			mp = mp->left;
		}
		/* free accumulated pages & unlock regions */
		(void) getpages_epilogue();

vhandout:
		if (
#ifdef MH_R10000_SPECULATION_WAR
                    (GLOBAL_FREEMEM() - smallk0_freemem) <= vhandkicklim ||
                    ((pfn_lowmem != 0) && (smallk0_freemem == 0))
#else /* MH_R10000_SPECULATION_WAR */
		    GLOBAL_FREEMEM() <= vhandkicklim
#endif /* MH_R10000_SPECULATION_WAR */
                    )
			cvsema(&shaked_wait);

		/* kick the scheduler - only does anything if we have
		 * processes in sxbrk state & there is plenty of memory. */
		kicksched();

		/* Go to sleep until clock wakes us up again. */
		KTRACE16(vhand_trace,
				"END", GLOBAL_FREEMEM(),
				"scan time mS", ((lbolt - slbolt) * 1000) / HZ,
				"vscan", vscan,
				"kicked", vkickedpages,
				"young", youngpages,
				"npscanned", npscanned,
				"swapmin", swapmin,
				"swapmax", swapmax);

		vscan++;
#ifdef	DEBUG
		valid_reg_count();
#endif
		psema(&vhandsema, PSWP);
	}
}

/*
 * This is the code for the daemon which runs the shake routines
 * when the system is low on memory.  Running this from a process
 * other than vhand, pdflushd, and bdflush keeps us from deadlocking.
 */
long	shaked_wait_until;

void
shaked(void)
{
	long		shake_start;
	long		shake_end;
	long		shake_ticks;
	extern int	shaked_interval;


	initnsema(&shaked_wait, 0, "shaked");

	/*
	 * The idea here is to keep shaked from running too much.
	 * It runs at most once every shaked_interval seconds
	 * (default 1).  If the last call into
	 * the shake routines took longer than that, then we
	 * use the time it took as the time to wait until doing it
	 * again.  This should keep shaked from going wild.
	 */
	shaked_wait_until = lbolt + (shaked_interval * HZ);
	for (;;) {
		while (lbolt < shaked_wait_until) {
			psema(&shaked_wait, PSWP);
		}
		shake_start = lbolt;
		(void) shake_shake(SHAKEMGR_MEMORY);
		shake_end = lbolt;
		shake_ticks = shake_end - shake_start;
		shaked_wait_until = shake_end + MAX(shake_ticks,
						    (shaked_interval * HZ));
	}
}

/*
 * Steal pages from a address space until we've freed enough.
 * NOTE: various regions maybe left locked - getpages_epilogue MUST be called
 * Returns < 0 if didn't even try to do anything.
 * >=0 - # of pages stolen.
 */
static pgcnt_t
stealfrom(pas_t *pas, preg_t *prp, int flags, as_shake_t *args)
{
	pgcnt_t nsteal, rv;
	pgcnt_t nstolen = 0;

	for (; prp != 0; prp = PREG_NEXT(prp)) {
		if (prp->p_nvalid == 0)
			continue;
		/*
		 * Get the maximum number of pages to steal from this
		 * region.
		 * We use this to limit the number of pages we steal
		 * The intent of this is to avoid just 
		 * stealing everything in the region once we are below this
		 * threshold and to limit the per-region penalty.
		 */
		if (flags & GTPGS_HARD)
			nsteal = prp->p_pglen;
		else
			nsteal = howhard(prp, args);
		if ((rv = spinreg(pas, prp, nsteal, flags)) > 0)
			nstolen += rv;
	}
	return nstolen;
}

/*
 * Steal pages from a address space until we've freed enough.
 * NOTE: various regions maybe left locked - getpages_epilogue MUST be called
 * Returns < 0 if didn't even try to do anything.
 * >=0 - # of pages stolen.
 */
int
pas_spinas(pas_t *pas, as_shake_t *args)
{
	ppas_t *ppas;
	int rv, nstolen;

	vscan++; /* we're making progress */

	if (pas->pas_flags & PAS_ULI)
		return -1;


	/*
	 * Spin through the process pregions and steal from every
	 * region.
	 * By holding the AS pregion lock - we don't need the aspacelock.
	 * The list
	 * cannot change, and before we do anything to the region, we lock it
	 * We can't use the aspacelock here since it can be held for
	 * UPDATE and then the thread can go SXBRK.
	 * Also - aspacelock can be held for a long time - we'd really
	 * rather not wait..
	 *
	 * The process can't exit, nor could it be past exitting.
	 * The process could exec, once we grab preglock it can't remove
	 * anymore regions, so it would appear the worst that could
	 * happen is we steal pages from parts of the 'new' process.
	 */
	nstolen = 0;
	mutex_lock(&pas->pas_preglock, 0);
	if ((rv = stealfrom(pas, PREG_FIRST(pas->pas_pregions), 0, args)) > 0)
		nstolen += rv;
	mutex_unlock(&pas->pas_preglock);

	/*
	 * grabbing the listlock for access will keep anyone from
	 * being added or deleted - but the ref count could go to 0
	 */
	mraccess(&pas->pas_plistlock);
	mutex_lock(&pas->pas_preglock, 0);
	for (ppas = pas->pas_plist; ppas; ppas = ppas->ppas_next) {
		if (ppas->ppas_refcnt == 0)
			continue;
		/*
		 * never steal memory from isolated process
		 * shouldn't really ever get here except in races.
		 */
		if (ppas->ppas_flags & PPAS_ISO)
			continue;
		if ((rv = stealfrom(pas, PREG_FIRST(ppas->ppas_pregions), 0,
							args)) > 0)
			nstolen += rv;
	}
	mutex_unlock(&pas->pas_preglock);
	mrunlock(&pas->pas_plistlock);
	return nstolen;
}

/*
 * Steal any available pages from the given region. 
 *
 * Returns: < 0 - did NOT do something with region for some reason
 *	   >= 0 - number of pages taken
 */
static pgcnt_t
spinreg(pas_t *pas, preg_t *prp, pgcnt_t nsteal, int flags)
{
	reg_t *rp = prp->p_reg;
	pgcnt_t rv;
#if DEBUG
	int reasons[MAXREASONS];
	int *reasonp = reasons;
#else
	int *reasonp = NULL;
#endif

	vscan++; /* we're making progress */
	if (rp->r_flags & RG_PHYS)
		return(0);
	if (rp->r_flags & RG_ISOLATE)
		return(-1);

	if (!creglock(rp)) {
		KTRACE4(vhand_trace,
				"spinreg-nolock preg:", prp,
				"nsteal:", nsteal);
		return(-1);
	}
#if DEBUG
	if (vhand_trace_level > 2)
		KTRACE8(vhand_trace,
			"spinreg-start prp:", prp,
			"pghnd:", prp->p_pghnd,
			"nvalid:", prp->p_nvalid,
			"nsteal:", nsteal);
	bzero(reasons, sizeof(reasons));
#endif

	rv = getpages(pas, prp, flags, nsteal, reasonp);
#if DEBUG
	if (vhand_trace_level > 2) {
		KTRACE4(vhand_trace,
			"spinreg-end prp:", prp,
			"stolen", rv);
		KTRACE16(vhand_trace, 
			"spin-reasons nopfnlock", reasons[1],
			"young", reasons[2],
			"notvalid", reasons[3],
			"raworbad", reasons[4],
			"alreadyswap", reasons[5],
			"notleaf", reasons[6],
			"nomfile", reasons[7],
			"pfderror", reasons[8]);
	}
#endif

	/* if no pages queued then free rp */
	getpages_denouement(rp);

	return(rv);
}

/*
 * check pas for RSS excessivity and trim as necessary
 * XXX in future RSS will be sum of shared and private rss's
 * for now, if any rss is too high, we'll trim
 * Note that we really don't want to use aspacelock since we're called
 * from vhand and we don't want to sleep and aspacelock can be
 * held for a long time (like SXBRK)
 *
 * NOTE: in this routine we have a ref to the vas but not to any ppas.
 *
 * NOTE2: clearly the best thing to do is to steal the 'oldest' pages
 * but we don't really want to run ageing since its expensive (requires
 * site-wide TLB flush). SO we don't spend much time on ageing and instead
 * take a certain percentage from each pregion (very suboptimal).
 */
int
pas_shakerss(pas_t *pas)
{
	ppas_t *ppas;
	preg_t *prp;
	int isiso, over;
	pgcnt_t nsteal;

	if (pas->pas_flags & PAS_ULI)
		return 0;

	/* first check for overage && not isolated */
	mraccess(&pas->pas_plistlock);
	over = 0;
	isiso = 0;
	for (ppas = pas->pas_plist; ppas; ppas = ppas->ppas_next) {
		/* basically if any member is isolated just forget it */
		if (ppas->ppas_flags & PPAS_ISO) {
			isiso = 1;
			/* this is knock-out item */
			break;
		}
		if ((pas->pas_rss + ppas->ppas_rss) > pas->pas_maxrss)
			over = 1;
	}
	mrunlock(&pas->pas_plistlock);
	if (isiso || !over)
		return 0;

	/*
	 * ok, AS is over RSS - lets trim
	 */
	mutex_lock(&pas->pas_preglock, 0);
	prp = PREG_FIRST(pas->pas_pregions);

	for (; prp != 0; prp = PREG_NEXT(prp)) {
		nsteal = howhardrss(prp->p_pglen, prp->p_nvalid,
				pas->pas_rss, pas->pas_maxrss, pas->pas_size);
		ASSERT(!(prp->p_flags & PF_PRIVATE));
		spinreg(pas, prp, nsteal, 0);
	}
	mutex_unlock(&pas->pas_preglock);

	/*
	 * grabbing the listlock for access will keep anyone from
	 * being added or deleted - but the ref count could go to 0
	 */
	mraccess(&pas->pas_plistlock);
	mutex_lock(&pas->pas_preglock, 0);
	for (ppas = pas->pas_plist; ppas; ppas = ppas->ppas_next) {
		if (ppas->ppas_refcnt == 0)
			continue;
		prp = PREG_FIRST(ppas->ppas_pregions);
		for (; prp != 0; prp = PREG_NEXT(prp)) {
			nsteal = howhardrss(prp->p_pglen, prp->p_nvalid,
				ppas->ppas_rss, pas->pas_maxrss,
				ppas->ppas_size);
			ASSERT(prp->p_flags & PF_PRIVATE);
			spinreg(pas, prp, nsteal, 0);
		}
	}
	mutex_unlock(&pas->pas_preglock);
	mrunlock(&pas->pas_plistlock);

	if (getpages_epilogue() < 0) {
		/* we ran out of swap space while
		 * trying to trim this guy - kill it
		 * This is important since we may never
		 * memory deadlock in a situation where
		 * a process is exceeding its RSS,
		 * but there is plenty of GLOBAL_FREEMEM.
		 */
		pas_flagset(pas, PAS_NOSWAP);
	}

	/* recalc correct RSS - XXX pls kill me and maintain correct RSS */
	vrecalcrss(pas);
	return 0;
}

static pgcnt_t
ageloop(preg_t *prp, int isiso)
{
	reg_t *rp;
	pde_t *pt;
	pgno_t i;
	auto pgno_t size, sz;
	pgcnt_t totrss = 0;
	int  pmaptrim_state;
	auto uvaddr_t vaddr;

	for ( ; prp; prp = PREG_NEXT(prp)) {
		rp = prp->p_reg;

		/* SPT */
		if (((prp->p_nvalid == 0) && !pmap_spt(prp->p_pmap)) || 
		    (rp->r_flags & RG_PHYS))
			continue;

		totrss += prp->p_nvalid;

		if (isiso || (rp->r_flags & RG_ISOLATE))
			continue;

		if (creglock(rp) == 0)
			continue;

		vaddr = prp->p_regva;
		size = prp->p_pglen;


		/*
		 * If pmap_trimtime was set to > 0 in some earlier pass, right
		 * at this time, pmap_trim could be trying to steal this process's 
		 * page tables. To resolve the race condition with pmap_trim, 
		 * atomically set pmap_trimtime to zero. If successful, go ahead
		 * with the ageloop (true most of the time). If unsuccessful,
		 * try later.
		 */
		if ((pmaptrim_state = pmaptrim_postpone(prp->p_pmap)) == PMAPTRIM_IN_PROGRESS) {
			regrele(rp);
			continue;
		}
	

		while (size) {
			/*	pmap_probe returns a pointer to the first page
			 *	table entry mapped by (prp,vaddr,size) that
			 *	actually exists in the pmap module.
			 *	vaddr and size are updated to reflect entries
			 *	skipped (so vaddr corresponds to the returned
			 *	pt), and the number of contiguous, available
			 *	entries are returned indirectly.
			 */
			if ((pt = pmap_probe(prp->p_pmap, &vaddr, &size, 
						&sz)) == NULL)
				break;

			i = sz;
			vaddr += ctob(i);
			size -= i;

			while (i > 0) {
				/* 
				 * Implement counting reference bits.
				 * For referenced page, nr is set to "all ones".
				 * To age page, decrement nr field (stick at 0),
				 * and set vr to 0 to catch next reference.
				 * Skip reference-counting for locked pages --
				 * they shouldn't be tossed anyway.
				 */
				pg_pfnacquire(pt);
				if (pg_isvalid(pt)) {
					pfd_t *pfd = pdetopfdat(pt);
					if (!pfd->pf_rawcnt) {
						if (pg_getnr(pt) != 0) {
						       pg_decnr(pt);
						}
						pg_clrhrdvalid(pt);
					}
				}
				pg_pfnrelease(pt);
				pt++;
				i--;
			}

		}

		/*
		 * Restore pmap_trimtime if needed.
		 */
		if (pmaptrim_state == PMAPTRIM_CANCELED)
			prp->p_pmap->pmap_trimtime = lbolt;
		regrele(rp);
	}
	return totrss;
}

/*
 * Do page use bit ageing for the pages mapped in the AS
 */
int
pas_ageregion(pas_t *pas)
{
	preg_t *prp;
	reg_t *rp;
	ppas_t *ppas;

	vscan++; /* we're making progress */

	if (pas->pas_flags & PAS_ULI)
		return 0;

	mutex_lock(&pas->pas_preglock, 0);
	pas->pas_rss = ageloop(PREG_FIRST(pas->pas_pregions), 0);
	mutex_unlock(&pas->pas_preglock);

	/*
	 * Even though we may be a member of a share group with isolated
	 * members, we spin through the list anyway - to compute the
	 * RSS/SIZE. The RG_ISOLATED check will prevent us from actually
	 * aging the region.
	 */
	/*
	 * grabbing the listlock for access will keep anyone from
	 * being added or deleted - but the ref count could go to 0
	 */
	mraccess(&pas->pas_plistlock);
	mutex_lock(&pas->pas_preglock, 0);
	for (ppas = pas->pas_plist; ppas; ppas = ppas->ppas_next) {
		if (ppas->ppas_refcnt == 0)
			continue;
		ppas->ppas_rss = ageloop(PREG_FIRST(ppas->ppas_pregions),
				ppas->ppas_flags & PPAS_ISO);
	}
	mutex_unlock(&pas->pas_preglock);
	mrunlock(&pas->pas_plistlock);

	/*
	 * Now toss any tsave pregions - there really shouldn't be any
	 * since all good dynamic loaders will call syssgi(SGI_TOSSTSAVE)
	 * but since if for some reason they don't we'll have real pages
	 * that will never get reclaimed(until the process dies) lets
	 * do it here.
	 * XXX isolated processors - do we really care??
	 */
	if (pas->pas_tsave && mrtryaccess(&pas->pas_aspacelock)) {
		/*
		 * Check that the pas is not in a transient execing state
		 * where the pas can be deallocated, while vhand holds a
		 * reference to it in the page lists while stealing pages
		 * from it. Punt in this case. This leaves possibilities
		 * of a deadlock since vhand can not reach out and steal
		 * from the tsave regions, and the process might not be
		 * able to free tsave regions becuase it needs more
		 * resources while creating a new vas/pas.
		 */
		if (pas->pas_flags & PAS_EXECING) {
			mrunlock(&pas->pas_aspacelock);
			return 0;
		}
		for (prp = pas->pas_tsave; prp; prp = PREG_GET_NEXT_LINK(prp)) {
			rp = prp->p_reg;

			/* SPT */
			if ((prp->p_nvalid == 0) && !pmap_spt(prp->p_pmap))
				continue;

			if (creglock(rp) == 0)
				continue;
			(void) getpages(pas, prp, GTPGS_HARD, prp->p_pglen, NULL);
			/* if no pages queued then free rp */
			getpages_denouement(rp);
		}
		mrunlock(&pas->pas_aspacelock);
	}
	return 0;
}

/*	Swap out pages from region rp which is locked by our caller.
 *	If flags & GTPGS_HARD is set, take all valid pages,
 *	otherwise take only unreferenced pages.
 *	Returns # of pages stolen
 */
static pgcnt_t
getpages(pas_t *pas, preg_t *prp, int flags, pgcnt_t nsteal, int reasons[])
{
#ifdef DEBUG
	register reg_t *rp = prp->p_reg;
#endif /* DEBUG */
	register pgno_t	pghnd;
	pgcnt_t stolen = 0;
	register pde_t *pt;
	int  pmaptrim_state;
	auto pgno_t maxsteal, totsz, sz;
	auto char *vaddr;

	ASSERT(ismrlocked(&rp->r_lock, MR_UPDATE));
	ASSERT((rp->r_flags & RG_PHYS) == 0);


	if ((prp->p_pglen == 0) || 
	    ((prp->p_nvalid == 0) && !pmap_spt(prp->p_pmap)))	/* SPT */
		return 0;


	/*
	 * If p_pmaptrimtime was set to > 0 in some earlier pass, right
	 * at this time, pmap_trim could be trying to steal this process's 
	 * page tables. To resolve the race condition with pmap_trim, 
	 * atomically set pmap_trimtime to zero. If successful, go ahead
	 * with the ageloop (true most of the time). If unsuccessful,
	 * try later.
	 */
	if ((pmaptrim_state = pmaptrim_postpone(prp->p_pmap)) == PMAPTRIM_IN_PROGRESS)
		return 0;
	


	maxsteal = nsteal;

	/*	Look through all of the pages of the region.
	 *	Start at page table entry where we last left off,
	 *	and search to the end of the page tables...
	 */
	if (prp->p_pghnd >= prp->p_pglen)
		prp->p_pghnd = 0;
	
	pghnd = prp->p_pghnd;
	totsz = prp->p_pglen - pghnd;
	vaddr = prp->p_regva + ctob(pghnd);
	
	while (totsz) {
		register pgno_t i, omax;
		if ((pt = pmap_probe(prp->p_pmap, &vaddr, &totsz, 
						&sz)) == NULL)
			break;

		i = sz;
		ASSERT(i > 0 && i <= totsz);

		omax = maxsteal;
		pghnd += tosspages(pas, prp, pt, vtorpn(prp, vaddr), i,
				   flags, reasons, &maxsteal);

		stolen += (omax - maxsteal);
		if (maxsteal <= 0)
			goto return_stolen;

		totsz -= i;
		vaddr += ctob(i);
	}

	if (prp->p_pghnd <= 0) {
		ASSERT(prp->p_pghnd == 0);
		goto return_stolen;
	}

	ASSERT(prp->p_pglen - prp->p_pghnd > 0);

	pghnd = 0;
	vaddr = prp->p_regva;
	totsz = prp->p_pghnd;

	while (totsz) {
		register pgno_t i, omax;
		if ((pt = pmap_probe(prp->p_pmap, &vaddr, &totsz, 
						&sz)) == NULL)
			break;

		i = sz;
		ASSERT(i > 0 && i <= totsz);

		omax = maxsteal;
		pghnd += tosspages(pas, prp, pt, vtorpn(prp, vaddr), i,
				   flags, reasons, &maxsteal);
		stolen += (omax - maxsteal);
		if (maxsteal <= 0)
			break;

		vaddr += ctob(i);
		totsz -= i;
	}

return_stolen:
	prp->p_pghnd = pghnd;
	if (pmaptrim_state == PMAPTRIM_CANCELED || stolen && !prp->p_pmap->pmap_trimtime)
		prp->p_pmap->pmap_trimtime = lbolt;

	return stolen;
}

/*
 * Implement the heuristics of how many pages to steal from a particular
 * region.  In the future, we should take into account process priority,
 * desperation for memory, other things.
 */
static int	howfactor = 10;		/* 10 % */
static int	howinactivefactor = 3;	/* 33 % */
static int	howlowrtfactor = 1;	/* 100 % */
static int	hownormrtfactor = 10;	/* 10 % - probably should be same as
					 * normal pri guys */
static int	howhirtfactor = 100;	/* 1 % */
static int	howmin = 1;		/* min pages */
static int	howinactive = 30 * HZ;	/* seconds asleep before
					 * considered inactive
					 */
static int	howmaxfracnum = 2;	/* 66 % */
static int	howmaxfracdiv = 3;	/* 66 % */
static int	howrssmax = 5;		/* only trim up to 20% pages from a
					 * RSS hog at a time
					 * This keeps us from allowing
					 * a process to grow until
					 * freemem < rsskicklim then
					 * stealing 100's/1000's of pages to
					 * trim it back down, only to let
					 * it grow again
					 * If we make it a constant than the
					 * first RSS hog has an advantage over
					 * later ones
					 */

static pgcnt_t
howhard(preg_t *prp, as_shake_t *args)
{
	pgcnt_t howcount;
	pgcnt_t howfrac;
	pgcnt_t howmax;
	pgcnt_t nvalid = prp->p_nvalid;

	switch (args->as_shakepage_state) {
	case GTPGS_PRI_HI:
		howfrac = howhirtfactor;
		break;
	case GTPGS_PRI_NORM:
		howfrac = hownormrtfactor;
		break;
	case GTPGS_WEIGHTLESS:
		/* take as much of these guys as we want */
		howfrac = howlowrtfactor;
		howmaxfracnum = 1;
		howmaxfracdiv = 1;
		break;
	case GTPGS_NORM:
		if (args->as_shakepage_slptime > howinactive)
			howfrac = howinactivefactor;
		else
			howfrac = howfactor;
		break;
	}

	/* take computed fraction of valid pages */
	howcount = nvalid / howfrac;

	/* never take less than howmin */
	if (howcount < howmin)
		howcount = howmin;

	/*
	 * If we have enough memory - stop now
	 * Even if need alot of memory - don't take all at
	 * once from one region (if that region happens to be large)
	 * Never take more than howmaxfrac of memory needed to
	 * get to gpgshi - this will help guarantee that more than
	 * one large process will have some pages stolen.
	 */
#ifdef MH_R10000_SPECULATION_WAR
	howmax = (vhandkicklim - (GLOBAL_FREEMEM() - smallk0_freemem));
#else /* MH_R10000_SPECULATION_WAR */
	howmax = (vhandkicklim - GLOBAL_FREEMEM());
#endif /* MH_R10000_SPECULATION_WAR */
	if (howmax <= 0) {
#ifdef MH_R10000_SPECULATION_WAR
		if ((pfn_lowmem != 0) && (smallk0_freemem == 0))
                        return(1);
#endif /* MH_R10000_SPECULATION_WAR */
		return 0;
	} else
		howmax = (howmax * howmaxfracnum) / howmaxfracdiv;
	if (howcount > howmax)
		howcount = howmax;
	return(howcount);
}

/* 
 * XXX this is really quite bogus - percentage of all regions
 * being the same - there would appear to be 2 main reasonable things
 * to do - pick on the oldest pages or pick randomly..
 */
static pgcnt_t
howhardrss(
	pgcnt_t len,
	pgcnt_t nvalid,
	pgcnt_t rss,
	pgcnt_t rssmax,
	pgcnt_t psize)
{
	pgcnt_t howcount;

	/* doing RSS trimming */
	if (rss > rssmax) {
		register unsigned pctvalid;
		if (nvalid < 3)
			return(0);
		/*
		 * treat all regions equal - give each one
		 * the same percentage valid pages
		 */
		if (psize == 0) {
			howcount = MIN(rss - rssmax, len);
		} else {
			pctvalid = (pgcnt_t)(((long long)rssmax * 100) / psize);
			howcount = nvalid -
				((len * pctvalid) / 100);
		}
		howcount /= howrssmax;
		if (howcount < howmin)
			howcount = howmin;
		return(howcount);
	}
	return 0;
}

/*
 * Search through page tables for pages that can be tossed.
 * If we decide to return early, we return the index of the next page to search.
 * We also update maxsteal based on how many pages were actually stolen
 */
static int
tosspages(
	pas_t *pas,
	register preg_t *prp,
	register pde_t *pt,	/* pte of first page to look at */
	register pgno_t rpn,	/* region logical page number */
	register pgno_t npgs,
	int flags,
	int reasons[],		/* tracing - why pages were rejected */
	pgno_t *maxsteal)
{
	reg_t *rp = prp->p_reg;
	pfd_t *pfd;
	int j, i, k;
	int tmaxsteal = *maxsteal;
	int list;
	pgno_t	num_base_pages;	/* Number of base pages in the large page */
	pgno_t	num_ptes_scanned;/* Number of ptes scanned in one iteration */
	pde_t	*tmp_pt;
	struct pglsts *p; 
	int	pages_on_same_list;
	int	downgrade_page;

	/*
	 * Look through pages for appropriate pages to reclaim
	 * Large pages are swapped fully. The algorithm first goes through
	 * a loop trying to see if all the pages of a large page can be
	 * swapped. If so it then goes through another loop and enters the
	 * page into the appropriate list. Note that all the pages of a
	 * large page must belong to the same list. If a large page cannot
	 * be fully swapped all the ptes for that page are skipped. By ensuring
	 * that an entire large page is swapped, we don't have to downgrade the
	 * large page. 
	 *  
	 */
	for (j = 0; j < npgs; j += num_ptes_scanned) {

		num_base_pages =  PAGE_MASK_INDEX_TO_CLICKS(
				pg_get_page_mask_index(pt));
		 
		/* see if we've stolen enough pages already */
		if (tmaxsteal <= 0) {
			*maxsteal = tmaxsteal;
			ASSERT (j <= npgs);
			return j;
		}

		if (num_base_pages == btoct(NBPP)) {

			list = test_toss_page(prp, pt, flags);

			if (list < 0) {
				if (reasons)
					reasons[-list]++;
				pt++;
				rpn++;
				num_ptes_scanned = 1;
				continue;
			}

			pfd = pdetopfdat(pt);

			if (list == swaplist)
				pfd->pf_pchain = (pfd_t *) 0xaddL;

			/* put page on appropriate list */
			addpg(pas, prp, pt, list, rpntoapn(prp, rpn), pfd, 0);
			tmaxsteal--;
			pg_pfnrelease(pt);
			num_ptes_scanned = 1;
			pt++;
			rpn++;
		} else { /* Large page swapping */

			LPG_FLT_TRACE(VHAND_TOSS_PAGE, prp, pt, 
						num_base_pages, 0);

			/*
			 * If this is a large page check if this is a 
			 * beginning of a large page, if not skip to the end 
			 * of the large page and getpages() will get to it 
			 * next time.
			 */

			tmp_pt = LARGE_PAGE_PTE_ALIGN(pt, num_base_pages);
			if (pt != tmp_pt) {
				num_ptes_scanned = 
					num_base_pages - (pt - tmp_pt);
				pt += num_ptes_scanned;
				rpn += num_ptes_scanned;
				continue;
			}

			/*
			 * Ensure that there are at least enough pages
			 * to scan to cover the large page.
			 */
			if ((npgs -j) < num_base_pages) {
				num_ptes_scanned = npgs - j;
				pt += num_ptes_scanned;
				rpn += num_ptes_scanned;
				continue;
			}

			tmp_pt = pt;

			pages_on_same_list = TRUE;

			for (i = 0; i < num_base_pages; i++, tmp_pt++) {
				list = test_toss_page(prp, tmp_pt, flags);

				if (list < 0) {
					if (reasons)
						reasons[-list]++;
					break;
				}

				if ((pages_on_same_list) && 
					(i > 0) && 
					(list != large_page_pglst[i - 1]))
						pages_on_same_list = FALSE;

				large_page_pglst[i] = list;
			}

			/*
			 * Cannot fully swap a large page. Skip the large page 
			 * if all pages are locked. Otherwise downgrade the
			 * page and try again.
			 */

			if (i != num_base_pages) {

				/*
				 * If all pages are locked then do not
				 * downgrade the page as we cannot steal it
				 * any way.
				 */
				if (i == 0 ) {
					tmp_pt = pt;
					downgrade_page = 0;
					for (k = 0; k < num_base_pages; k++) {
						if (pdetopfdat(tmp_pt)->pf_rawcnt)
							continue;
						else {
							downgrade_page++;
							break;
						}
					}

					if (!downgrade_page) {
						pt += num_base_pages;
						rpn += num_base_pages;
						num_ptes_scanned = 
							num_base_pages;
						continue;
					}
				}
	
				tmp_pt = pt;

				/*
				 * Unlock all the pte locks which were 
				 * successfully acquired.
				 */

				for( ;i; i--)
					pg_pfnrelease(tmp_pt++);

				/*
				 * Downgrade the large page and try again.
				 * We have to do a tlbsync as we have no
				 * reference to the process.
				 */

				tlbsync(0LL, allclr_cpumask, STEAL_PHYSMEM);
				pmap_downgrade_lpage_pte_noflush(pt);
				num_ptes_scanned = 0;

				continue;
			}
			
			if (pages_on_same_list) {
				p = &pglsts[list];
				
				/*
				 * If we cannot swap in the large page
				 * in one full swoop, clean the list
				 * and try again.
				 */

				if ((p->limit - p->index) < num_base_pages) {
					p->cleaner(rp);

				}
			} else {
				/*
				 * The pages can be downgraded here without
				 * doing a tlbflush as we do  a tlbsync later
				 * when the pages are pushed out. The pages
			 	 * cannot be migrated either until the
				 * pages are pushed out.
				 */
				pmap_downgrade_lpage_pte_noflush(pt);
			}

			for (i = 0; i < num_base_pages; i++, pt++, rpn++) {
				pfd = pdetopfdat(pt);

				list = large_page_pglst[i];

				/*
				 * If page is to be swapped use the right
				 * one.
			 	 */
				
#define	SWAPLIST(list)		((list == SPGLST0) || (list == SPGLST1))

				if (SWAPLIST(list)) {
					pfd->pf_pchain = (pfd_t *) 0xaddL;
					list = swaplist;
				}

				ASSERT(rpntoapn(prp, rpn) == pfd->pf_pageno);

				/* put page on appropriate list */
				addpg(pas, prp, pt, list, rpntoapn(prp, rpn), pfd, 0);
				tmaxsteal--;
				pg_pfnrelease(pt);
			}

			num_ptes_scanned = num_base_pages;
		}  /* Large page swap */
	}
	ASSERT(ismrlocked(&rp->r_lock, MR_UPDATE));
	*maxsteal = tmaxsteal;
	ASSERT (j <= npgs);
	return(j);
}

/*
 * Test if the pte can be tossed. Also determine the list in which the
 * page is to inserted.
 * Returns < 0 if for some reason the page shouldn't be tossed
 * else return list id.
 */
static int
test_toss_page(
	preg_t *prp,	/* Pregion to which pte belongs */
	pde_t *pt,	/* Pte to be examined */
	int flags)
{
	register reg_t	*rp = prp->p_reg;
	register int banon = 0;	/* region handled/backed by anon manager */
	register int bfile = 0;	/* region backed by file */
	register int ro = 0;	/* region is read-only */
	register pfd_t *pfd;
	int	list; 		/* List where the page is to inserted */

	/*
	 * Find out if region is handled by the anonymous manager or
	 * is backed by a file
	 */
	if (rp->r_flags & RG_ANON)
		/* backed by anonymous manager */
		banon = 1;
	else if (prp->p_maxprots & PROT_W)
		/* backed by file */
		bfile = 1;
	else
		/* read only region - place all pages on the free list */
		ro = 1;


	/* First try and acquire the pfn lock before checking if it's
	 * valid or not. Different thread could be shooting down this
	 * page at this time.
	 */
	if (pg_pfncondacq(pt)) {
		/*
		 * We didn't get the lock.
		 * We just give up on this page.
		 */
		return NOPFNLOCK;
	}

	if (!pg_isvalid(pt)) {
		pg_pfnrelease(pt);
		return NOTVALID;
	}

	pfd = pdetopfdat(pt);
#ifdef DEBUG
	/*
	 * note that at this very moment, ptoss called via ptossvp from
	 * a file system could be turning off P_HASH and turning on
	 * P_BAD - they have the page cache lock and pfdat lock but 
	 * but not any region locks.
	 * So, in order to check any flag bits we must get pfdat lock.
	 */
	{
	int s = pfdat_lock(pfd);
	ASSERT(pfd->pf_flags & (P_HASH|P_BAD));
	pfdat_unlock(pfd, s);
	}
#endif

	/* Never swap locked pages */
	/* If a page is marked bad, then we are racing with a different
	 * thread trying to take this page out of the vnode page cache. 
	 * There is no need to do anything here..
	 * NOTE: pf_flags can get marked as bad, after this check.
	 * (We are not holding the pfdat lock in all of the following
	 * code). Correct sequence would be to hold the pfdat lock
	 * for a sequence of operation in tosspages. But that's too
	 * long a time to hold the lock, and is not worth the effort.
	 * In addition, we check for P_BAD again in addpg.  (refer to
	 * comments in the middle of addpg)
	 * If we run into race conditions due to this checks, we could
	 * change the tosspages code to hold pfdat lock as needed.
	 */
	if (pfd->pf_rawcnt || (pfd->pf_flags & P_BAD)) {
		pg_pfnrelease(pt);
		return RAWORBAD;
	}

	/*
	 * If the page is marked as having an error, dont attempt swapping it.
	 */
	if (pfdat_iserror(pfd)) {
		pg_pfnrelease(pt);
		return PFDERROR;
	}

	/*
	 * If the page is poisoned, don't try to swap it.
	 */
#ifdef	NUMA_BASE
	if (pfdat_ispoison(pfd)) {
		pg_pfnrelease(pt);
		return PFDERROR;
	}
#endif

	/* ignore if referenced recently */
	if ((pg_getnr(pt) > vhandnrmask) && !(flags & GTPGS_HARD)) {
		youngpages++; /* DEBUGGING */
		pg_pfnrelease(pt);
		return YOUNG;
	}

	if (ro) {
		list = FPGLST;
		MINFO.unmodfl++;
	} else if (bfile) {
		if (pg_isdirty(pt)) {
			/*
			 * a modified page of a mapped file - we need
			 * the inode
			 * Do not hang - since a process can go SXBRK
			 * with the inode locked
			 * This could conceivably lead to livelock...
			 * but at least in that case we'll shoot
			 * somebody via brkmemdead
			 */
			if (flags & GTPGS_NOMFILE) {
				pg_pfnrelease(pt);
				return NOMFILE;
			}

			list = DPGLST;
		} else {
			list = FPGLST;
			MINFO.unmodfl++;
		}
	} else if (banon) {
		/*
		 * First see if page has 'gone' anon! if not
		 * then it really still has backing file and we
		 * can just toss it
		 */
		/*
		 * If page is not on swap already then we must swap
		 * it since there is NO other representation
		 * of this page anywhere on disk.
		 * If it has been swapped and it hasn't been dirtied
		 * then just free the page.
		 * If it is dirty and on swap and is associated with
		 * a copy-on-write region (fork/dupreg) then
		 * the page must have been dirtied before the dup
		 * and not been on swap. Since that time one
		 * member of the COW family has swapped the page.
		 * Since at the time of the dup our mod bit
		 * was turned off, we couldn't have sullied the page
		 * any further. If the COW brother touched the
		 * page before swapping it out, it would have
		 * gotton a new page in pfault so its page wouldn't
		 * be the same as ours ...
		 *
		 * If it is dirty, on swap and the region is NOT
		 * COW then this is either a private region or
		 * a shared mem region. In either case we must swap.
		 *
		 * If its dirty, on swap and has a ref count > 1 -
		 * then there could be more than one process accessing
		 * this page. This happens
		 * with addrattach and /proc attaches
		 * There are 2 sub-cases here - if the page
		 * has been modified by any of the current
		 * processes sharing the region, the anon handle
		 * for the page will be the same as the region's anon
		 * handle. In this case, we need to swap out ALL
		 * copies - since there are multiple pmaps all pointing
		 * to the same page, the last one swapped out is
		 * the most up to date. Swapping out successive
		 * versions will replace the swap handles for the
		 * previous swapped out versions.
		 * Case 2: the page has never been modified
		 * since the last fork - we have nothing to do.
		 *
		 * XXX
		 * To satisfy the assertion that we must never modify
		 * a swap hndle in a non-leaf anon node, we make sure
		 * that in fork and attachreg we get all modified
		 * pages into the leaf anon-node. This is hard! XXX
		 * For now we simply don't swap pages unless they
		 * are at the leaf anon.
		 */
		if (!(pfd->pf_flags & P_ANON)) {
			/* we can just free page */
			ASSERT(!pg_ismod(pt));
			MINFO.unmodsw++;
			list = FPGLST;
		} else

		if (!(pfd->pf_flags & P_SWAP) ||
		    (pg_isdirty(pt) && !(rp->r_flags & RG_CW))) {
			/*
			 * ignore pages already being swapped by us --
			 * we identify these by the fact that their
			 * pchain is set
			 */
			if (pfd->pf_pchain) {
				pchainpages++;
				pg_pfnrelease(pt);
				return ALREADYSWAP;
			}
			list = swaplist;
		} else
		
		if (pg_isdirty(pt) && (rp->r_flags & RG_HASSANON)) {
			if (pfd->pf_tag != anon_tag(rp->r_anon)) {
				/* not at leaf - punt */
				pg_pfnrelease(pt);
				return NOTLEAF;
			}
			/*
			 * ignore pages already being swapped by us --
			 * we identify these by the fact that their
			 * pchain is set
			 */
			if (pfd->pf_pchain) {
				pchainpages++;
				pg_pfnrelease(pt);
				return ALREADYSWAP;
			}
			list = swaplist;
		} else {
			/* we can just free page */
			ASSERT(!pg_ismod(pt));
			MINFO.unmodsw++;
			list = FPGLST;
		}
	} else
		cmn_err(CE_PANIC, "tosspages - unknown page type!");

	return list;
}

/*
 * Swap out any unreferenced, shared anonymous pages.  These are pages 
 * belonging to shm and /dev/zero regions that are not currently referenced
 * by any process (but could be referenced again in the future).
 */
static void
getsanonpages(int kicklim)
{
	pfd_t	*pfd, *pfd_next;

	mutex_lock(&sanon_lock, PZERO);

	/*
	 * Get a list of pages to toss from the page manager.  The
	 * page manager logically allocates the pages to us (vhand),
	 * so we can just pagefree the page when we're done swapping it.
	 */
#ifdef MH_R10000_SPECULATION_WAR
	{
		int	cnt;

		cnt = kicklim - GLOBAL_FREEMEM();
		if (cnt <= 0)
			cnt = (kicklim / 4) + 1;
		pfd = pgetsanon(cnt);
	}
#else /* MH_R10000_SPECULATION_WAR */
	pfd = pgetsanon(kicklim - GLOBAL_FREEMEM());
#endif /* MH_R10000_SPECULATION_WAR */

	for ( ; pfd ; pfd = pfd_next) {
		pfd_next = pfd->pf_next;
		pfd->pf_pchain = (pfd_t *) 0xaddL;
		addpg(NULL, NULL, NULL, swaplist, pfd->pf_pageno, pfd, SANON_PAGE);
	}

	getpages_epilogue();
	mutex_unlock(&sanon_lock);
}

/*
 * Add an entry to a page list
 */
static void
addpg(
	pas_t *pas,
	register preg_t *prp,
	register pde_t *pt,
	int which,
	pgno_t apn,
	pfd_t *pfd,
	int flags)
{
	register struct pglsts *p = &pglsts[which];
	register reg_t *rp;
	register int curi;

        /*
         * The pfn field in pt must be locked when calling this routine
         * with a non-null pt.
         */

#ifdef DEBUG
        if (pt) {
                ASSERT(pg_pfnislocked(pt));
        }
#endif

	/*
	 * One last time, check if the page has been marked bad,
	 * before trying to hold the page. It makes no sense to
	 * do all this extra work, if the page is already marked
	 * bad.
	 * NOTE: Page could still get marked bad, after this
	 * point. There is not much we can do about that.
	 * writechunk (cleaner function) looks for this condition.
	 * There is a small window in debug kernels, where we could
	 * look for P_BAD and decide to do pageuseinc. In the
	 * mean time, some one turns P_BAD bit. (We would just
	 * hit the assertion in pageuseinc() code). If that's
	 * the case we will have to change this code to grab the
	 * pfdat lock, before looking at P_BAD.
	 * This race condition should NOT affect normal kernels.
	 */
	if (pfd->pf_flags & P_BAD)
		return;
	
        /*
         * Since we are going to insert this pfdat into
         * a swapping list, we increment the pfdat's
         * reference count. This additional reference
         * will also prevent migration.
         */
        pageuseinc(pfd);

        /*
         * Due to the fact that the extra reference is preventing
         * migration, we don't need the pfn lock anymore.
         */
        if (pt) {
                pg_pfnrelease(pt);
        }
        

#ifdef TILE_DATA
	/*
	 * This page might be on this list for a while.
	 * Since the tile allocator doesn't lock the region
	 * when relocating pages it could find this page and
	 * attempt to move it.  Bump use count while on the list
	 * so pageevict will know to keep its hands off.
	 */
	pageuseinc(pfd);
#endif /* TILE_DATA */

	if (prp)
		rp = prp->p_reg;
	else
		rp = NULL;

	ASSERT(p->index <= p->limit);
	ASSERT(rp == NULL || ismrlocked(&rp->r_lock, MR_UPDATE));

	if (p->index == p->limit) {
		/* clean out list */
		(p->cleaner)(rp);

		/* if swap list then change */
		if (which == SPGLST0)
			p = &pglsts[SPGLST1];
		else if (which == SPGLST1)
			p = &pglsts[SPGLST0];

		ASSERT(rp == NULL || ismrlocked(&rp->r_lock, MR_UPDATE));
	}

	curi = p->index;
	ASSERT(curi >= 0 && curi < p->limit);

	if (prp) {
		/*	Increment the count of pages from this region
		 *	which are on one of the lists.
		 */
		bumprcnt(prp);
		p->list[curi].gp_rlptr = &gprglst[gprgndx];
	} else {

		/*
		 * No prp means we're swapping a P_SQUEUE page
		 */
		p->list[curi].gp_rlptr = NULL;
	}

	p->list[curi].gp_pas = pas;
	p->list[curi].gp_ptptr = pt;
	p->list[curi].gp_apn = apn;
	p->list[curi].gp_pfd = pfd;
	p->list[curi].gp_flags = flags;
        
	ASSERT(p->list[curi].gp_status == 0);
	p->index++;
	return;
}

/*
 * swapchunk - cleaner function for swap list
 * Returns: 0 if everything ok
 *	   -1 if ran out of swap
 */
static int
swapchunk(reg_t *lockedreg)
{
	register struct pglsts *s = &pglsts[swaplist];
	register int i, swapped, noswapped;
	register pglst_t *sp, *es;
	sm_swaphandle_t sw;
	int retval = 0;
	auto int try;
	int ospl;

	ASSERT(s->index > 0 && s->index <= s->limit);

	/* Invalidate all of the pages. */
	es = &s->list[s->index];
	for (sp = &s->list[0]; sp < es; sp++)
		if (sp->gp_ptptr)
			pg_clrvalid(sp->gp_ptptr);

	tlbsync(0LL, allclr_cpumask, STEAL_PHYSMEM);

	/* wait for any previous swaps to complete */
	waitforswapdone();

	/*
	 * switch swaplists -
	 * swaplist now points to available list
	 * s points to list we're swapping out
	 * We MUST do this now, since once we start swapping
	 * a buffer may get done BEFORE we even finish
	 * starting all the transactions - we need to be sure
	 * that swapdone 'dones' the correct swaplist
	 */
	swaplist = (swaplist == SPGLST0) ? SPGLST1 : SPGLST0;
	ASSERT(pglsts[swaplist].index == 0);
	ASSERT(pglsts[swaplist].ntran == 0);

	swapped = 0;
	noswapped = s->index;
	s->ndone = 0;
	s->lockedreg = lockedreg;
	/* start optimistically - so swapdone won't vsema yet.. */
	s->ntran = s->index;

	while (noswapped) {
		/*
		 * While there are pages that haven't been swapped ...
		 * Don't swap more than maxdmasz worth of pages, because
		 * the drivers can't handle that.
		 */
		try = noswapped;
		if (try > v.v_maxdmasz-1) {
			try = v.v_maxdmasz-1;
		}
		sw = sm_alloc(&try, 0);
		if (try == 0) {
			outofswap++;
			break;
		}
#ifdef DEBUG
		if (try != noswapped) {
			if (swapmin == -1 || try < swapmin)
				swapmin = try;
			if (swapmax == -1 || try > swapmax)
				swapmax = try;
		}
#endif
		s->list[swapped].gp_sh = sw;
		if (sm_swap(&s->list[swapped], sw, try, B_WRITE|B_ASYNC, swapdone)) {
			s->list[swapped].gp_sh = NULL;
			sm_dealloc(sw, try);
			break;
		}
		noswapped -= try;
		swapped += try;
	}

	if (swapped != s->ntran) {

		/* 
		 * Reset the "in transit" count to the actually number of
		 * pages for which we sucessfully started the I/O.  Hold
		 * the swapdonelock here so we don't race with the
		 * swapdone() routine and end up doing the vsema twice.
		 */

		ospl = mutex_spinlock(&swapdonelock);
		s->ntran = swapped;
		s->index = swapped;	/* this also set to 0 if none were
					 * swapped
					 */
		/* actually finished all xfers BEFORE we got here... */
		if (swapped && s->ntran == s->ndone)
			vsema(&swapwaitsema);

		mutex_spinunlock(&swapdonelock, ospl);
	}


	/*
	 * Re-validate those that failed (due to lack of swap)
	 * In this case the spglist is zeroed (XXX maybe we should just keep
	 * them on the list)
	 * In the error case -1 is returned
	 */
	if (noswapped) {
		register struct pglst *sp;

		for (i = 0, sp = &s->list[swapped]; i < noswapped; i++, sp++) {
			if (sp->gp_ptptr) {
#if R4000 && JUMP_WAR
				pg_setsftval(sp->gp_ptptr);
#else
#ifdef MH_R10000_SPECULATION_WAR
                                if (IS_R10000())
                                        pg_setsftval(sp->gp_ptptr);
                                else
#endif /* MH_R10000_SPECULATION_WAR */
				pg_setvalid(sp->gp_ptptr);
#endif
				/*
				 * If we are operating on a large page
				 * downgrade the even and odd large ptes of the
				 * large page and its buddy to smaller page 
			 	 * sizes.
				 * As we have already done a tlbsync, just 
				 * changing the page size is sufficient.
				 */

				if (pg_get_page_mask_index(sp->gp_ptptr)) {
					pmap_downgrade_lpage_pte_noflush(
								sp->gp_ptptr);
				}
			}

			sp->gp_pfd->pf_pchain = NULL;
		}
		freelist(&s->list[swapped], noswapped, lockedreg);
		retval = -1;
	}

     

	vkickedpages += swapped;
	return(retval);
}

/*
 * waitforswapdone - wait for all swap transactions to complete
 */
static void
waitforswapdone(void)
{
	register struct pglsts *s;
	register pglst_t *sp;
	register int first, i, ntran;
	register pfd_t *pfd;
	
	s = &pglsts[swaplist == SPGLST0 ? SPGLST1 : SPGLST0];
	if ((ntran = s->ntran) == 0) {
		/* already waited for */
		return;
	}

	ADD_SYSWAIT(swap);

	/* wait for swap completion */
	psema(&swapwaitsema, PMEM);

	SUB_SYSWAIT(swap);

	/* handle done pages */
	for (i = 0, first = 0, sp = &s->list[0]; i < ntran; i++, sp++) {
		pfd = sp->gp_pfd;
		if (sp->gp_status) {
			/*
			 * error during swapout
			 */
			/* free up list already swapped */
			if (i != first) {
				memfree(&s->list[first], i - first,
					       s->lockedreg, 0);
			}
			/* set possible first good page past this bad one */
			first = i + 1;

			/* re-validate page */
			if (sp->gp_ptptr) {
#if R4000 && JUMP_WAR
				pg_setsftval(sp->gp_ptptr);
#else
#ifdef MH_R10000_SPECULATION_WAR
                                if (IS_R10000())
                                        pg_setsftval(sp->gp_ptptr);
                                else
#endif /* MH_R10000_SPECULATION_WAR */
				pg_setvalid(sp->gp_ptptr);
#endif
				/*
				 * If we are operating on a large page
				 * downgrade the even and odd large ptes of the
				 * large page and its buddy to smaller page 
			 	 * sizes.
				 * As we have already done a tlbsync, just 
				 * changing the page size is sufficient.
				 */
				if (pg_get_page_mask_index(sp->gp_ptptr)) {
					pmap_downgrade_lpage_pte_noflush(
								sp->gp_ptptr);
				}
			}

			/* null out BEFORE release region */
			ASSERT(pfd->pf_pchain);
			pfd->pf_pchain = NULL;
			freelist(sp, 1, s->lockedreg);
			sm_free(&sp->gp_sh, 1);
			sp->gp_rlptr = NULL;
			sp->gp_status = 0;
			sp->gp_sh = NULL;
		} else {
			/* successful swap out */
			ASSERT(pfd->pf_pchain);
			pfd->pf_pchain = NULL;
		}
	}

	if (i != first) {
		ASSERT(s->list[first].gp_sh);
		memfree(&s->list[first], i - first, s->lockedreg, 0);
	}

	s->ntran = 0;	/* mark as completed */
	s->index = 0;
}


#ifdef DEBUG
extern int swapwrinject;	/* inject swap write errors */
extern int swapwrinjectfreq;
#endif

/*
 * swapdone - the 'relse' function for swap transfers
 */
void
swapdone(register buf_t *bp)
{
	register struct pglsts *s;
	register pglst_t *sp;
	register int npgs;
	register int i;
	register int ospl;
	sm_swaphandle_t sw, swbuf;

	s = &pglsts[swaplist == SPGLST0 ? SPGLST1 : SPGLST0];
	npgs = btoc(bp->b_bufsize);
#ifdef DEBUG
	if (swapwrinject > 0 && --swapwrinject == 0) {
		/* make sure not a sync swap-to-file - must be
		 * a block dev here
		 */
		if (sm_makeswaphandle(bp->b_edev, bp->b_blkno) != 0) {
			swapwrinject = swapwrinjectfreq;
			bp->b_flags |= B_ERROR;
		} else
			swapwrinject++;
	}
#endif
#ifdef DEBUG
	for (i = 0, sp = &s->list[0]; i < npgs; i++, sp++) {
		ASSERT(sp->gp_pfd->pf_pchain);
	}
#endif
	ASSERT(bp->b_flags & B_PAGEIO);
	if (bp->b_flags & B_ERROR) {
		register int j;

		/*
		 * mark as errors
		 * Find swaphandle then find element in buf
		 * Since whole buf is in error there must be one element
		 * of the pagelist already initialized to the corresponding
		 * swap handle
		 */
		swbuf = sm_makeswaphandle(bp->b_edev, bp->b_blkno);
		ASSERT(swbuf);
		cmn_err_tag(282,CE_ALERT, "Swap out failed on logical swap %d blkno 0x%x for process [ vhand ]\n",
			    sm_getlswap(swbuf), bp->b_blkno);
		for (i = 0, sw = NULL; i < s->limit; i++) {
			sw = s->list[i].gp_sh;
			if (swbuf == sw)
				break;
		}
		ASSERT(i < s->limit);
		ASSERT((i + npgs) <= s->limit);
		for (j = 0, sp = &s->list[i]; j < npgs; j++, i++, sp++) {
			sp->gp_status = 1;
			sp->gp_sh = sw;
			sw = sm_next(sw);
		}
	}

	/*
	 * If we're on an MP, and we're swapping to multiple swap devices
	 * to handle the current list, then swapdone could be called
	 * simultaneously on multiple processors.  So we use a spin lock
	 * to prevent races while we update our progress.
	 */

	ospl = mutex_spinlock(&swapdonelock);
	s->ndone += npgs;
	if (s->ndone == s->ntran)
		vsema(&swapwaitsema);

	mutex_spinunlock(&swapdonelock, ospl);
	putphysbuf(bp);
}

/*
 * writechunk - cleaner function for disk page list
 */
static int
writechunk(reg_t *lockedreg)
{
	struct pglsts *d = &pglsts[DPGLST];
	register pglst_t *dp, *ed;
	register gprgl_t *rlptr;
	register reg_t *rp;
	register preg_t *prp;
	register pde_t *pt;
	register pfd_t *pfd;
	/* REFERENCED */
        int pfrval;

	if (d->index == 0)
		return(0);

	/*	Loop through all of the page tables entries
	 *	turning off the reference bits and then flush
	 *	the tlbs.  Since the
	 *	regions are locked AND page migration has been disabled
         *      for all pfdats in this list, this means that no more
	 *	modifications to the page table entries
	 *	can occur after the flush.
	 */
	ASSERT(d->index > 0 && d->index <= d->limit);

	ed = &d->list[d->index];
	for (dp = &d->list[0]; dp < ed; dp++)
		pg_clrvalid(dp->gp_ptptr);

	tlbsync(0LL, allclr_cpumask, STEAL_PHYSMEM);

	/* Now free up the actual pages.  */
	for (dp = &d->list[0]; dp < ed; dp++) {
		rlptr = dp->gp_rlptr;
		prp = rlptr->gp_prp;
		pt = dp->gp_ptptr;

		/*
		 * send to delwri cache, free, and remove from page table
		 */
		ASSERT(pg_isdirty(pt));
                
                pg_pfnacquire(pt);
                pfd = pdetopfdat(pt);

                /*
                 * We are efectively removing the
                 * pfdats from this swap list, so
                 * we need to decrement the pfdat
                 * reference count.
                 * Since we've acquired the pfn lock,
                 * migration is still disabled.
                 */
                pfrval = pagefree(pfd);
                /*
                 * This pagefree call must have only
                 * decremented the ref count.
                 */
                ASSERT(pfrval == 0);

		if (dp->gp_pas) {
			VPAG_UPDATE_RMAP_DELMAP(PAS_TO_VPAGG(dp->gp_pas),
				JOBRSS_DECREASE, pfd, pt);
		} else {
			DELETE_MAPPING(pfd, pt);
		}
		

#ifdef TILE_DATA
		pagefree(pfd);		/* drop our extra use */
#endif /* TILE_DATA */

		/*
		 * Pages can go P_BAD if a mapped file is truncated while
		 * it's mapped.  In this case, just free the page.
		 */
		if (!(pfd->pf_flags & P_BAD)) {
			pdinsert(pfd);
			/*
			 * XXX	The pagefree case below won't ever
			 * XXX	return true because the pdinsert manager
			 * XXX	now holds a reference to the page.
			 */
			(void)pagefree(pfd);
			if (pfd->pf_use <= 1)
				MINFO.freedpgs++;
		} else {
                        if (pagefree(pfd)) {
                                ASSERT(pfd->pf_rawcnt == 0);
                                MINFO.freedpgs++;
                        }
                }

		PREG_NVALID_DEC(prp, 1);		/* SPT */
		pg_clrpgi(pt); 
                pg_pfnrelease(pt);
		if (--rlptr->gp_count == 0 && (rp = prp->p_reg) != lockedreg)
			regrele(rp);
		dp->gp_rlptr = NULL;
                
	}
	vkickedpages += d->index;

	d->index = 0;
	return(0);
}

/*	This routine is called to process the free page list.  That is,
 *	the list of pages which can be freed without doing any
 *	swap I/O to create disk copies.
 */
static int
freechunk(reg_t *lockedreg)
{
	register struct pglsts *f = &pglsts[FPGLST];
	register pglst_t *fp, *ef;

	/*	Loop through all of the page tables entries
	**	turning off the valid bits and then flush
	**	the tlbs.  Since the
	**	regions are locked AND page migration has been
        **      disabled for all pfdats in the list,
        **      this means that no more
	**	modifications to the page table entries
	**	can occur after the flush.
	*/
	ASSERT(f->index > 0  &&  f->index <= f->limit);

	ef = &f->list[f->index];
	for (fp = &f->list[0]; fp < ef; fp++) {
		pg_clrvalid(fp->gp_ptptr);
        }

	tlbsync(0LL, allclr_cpumask, STEAL_PHYSMEM);
	
	/* Now free up the actual pages.  */
	memfree(f->list, f->index, lockedreg, 1);

	/* Free pages list is empty now.  */
	vkickedpages += f->index;

	f->index = 0;
	return(0);
}

/*
 * memfree - Free memory.
 */
/* ARGSUSED3 */
static void
memfree(register pglst_t *pglptr, int size, reg_t *lockedreg, int skipmod)
{
	register pfd_t		*pfd;
	register int		j, dorele;
	register gprgl_t	*rlptr;
	pde_t *pt;
	reg_t *rp;
	preg_t *prp;
	sm_swaphandle_t		swaphdl = NULL;
	/* REFERENCED */
        int pfrval;

	for (j = 0; j < size; j++, pglptr++) {

		pfd = pglptr->gp_pfd;
		dorele = 0;

                /*
                 * Effetively being removed from the
                 * swap list. We have to decrement the
                 * ref count, and also grab the pfn lock
                 * if the associated pte is not NULL.
                 */
                if (pglptr->gp_ptptr) {
                        pg_pfnacquire(pglptr->gp_ptptr);
                }

		pfrval = pagefree(pfd);

		/*
		 * The call to pagefree above could not
		 * have possibly really freed the page
		 * because we had the extra reference
		 * corresponding to the swap list.
		 */
		ASSERT(pfrval == 0);
#ifdef DEBUG
		/*
		 * note that at this very moment, ptoss called via ptossvp from
		 * a file system could be turning off P_HASH and turning on
		 * P_BAD - they have memlock, but not any region locks.
		 * So, in order to check any flag bits we must lock memlock
		 */
		{
			int s = pfdat_lock(pfd);
			ASSERT(pfd->pf_flags & (P_HASH|P_BAD));
			pfdat_unlock(pfd, s);
		}
#endif

#ifdef TILE_DATA
		pagefree(pfd);		/* drop our extra use */
#endif /* TILE_DATA */

		/* Put some swap hdl structs back in the pool if we
		 * need them and are able to use this page.
		 */

                /*
                 * It's necessary to invoke rmap_delmap() before pagefree
                 * gets called (due to an assertion in _pagefree added to
                 * catch pages being freed with reverse mapping.
                 * Once the assertion is removed, this code could be
                 * moved inside if ((rlptr = pglptr->....)
                 */
                if (pglptr->gp_ptptr ){
			if (pglptr->gp_pas) {
				VPAG_UPDATE_RMAP_DELMAP(PAS_TO_VPAGG(pglptr->gp_pas), JOBRSS_DECREASE,  pglptr->gp_pfd, pglptr->gp_ptptr);
			} else {
				DELETE_MAPPING(pfd, pglptr->gp_ptptr);
			}
                }

		/*
		 * Note that the gp_sh field is only set when we're freeing
		 * up anon pages that have just been swapped.  Also, this
		 * field is also only set for the first of a set of pages
		 * that have been swapped to contiguous blocks on the swap
		 * device.
		 */

		if (pglptr->gp_sh) {
			swaphdl = pglptr->gp_sh;
			pglptr->gp_sh = NULL;
		}

		if (swaphdl) {
			anon_setswap_and_pagefree(pfd, swaphdl,
						pglptr->gp_flags & SANON_PAGE);

			swaphdl = sm_next(swaphdl);

		} else if (pfd->pf_flags & P_ANON)
			anon_pagefree_and_cache(pfd);

		else if (scache_pool_pages_needed()) {
			if (pagerealloc(pfd)) {
				MINFO.freedpgs++;
				scache_pool_add(pfd);
			}

		} else {

			/* Put pages at end of queue since they
			 * represent disk blocks and we hope they
			 * will be used again soon.
			 */
			if (pagefree(pfd))
				MINFO.freedpgs++;
		}

		/*
		 * If the scache pool is low, try to get some more pages
		 * for it off the free list.  (Someone else may have freed
		 * some pages while we've been swapping, so grab them now
		 * if we can.)
		 */

		while (scache_pool_pages_needed()) {
			pfd_t *new_page;

			if ((new_page = pagealloc(0, VM_UNSEQ)) == NULL)
				break;

			scache_pool_add(new_page);
		}


		/* Disassociate pte from physical page except for P_SQUEUE
		 * pages since they aren't associated with a prp and have
		 * no page table entries 
		 */

		if ((rlptr = pglptr->gp_rlptr) != NULL) {
			prp = rlptr->gp_prp;
			rp = prp->p_reg;
			pt = pglptr->gp_ptptr;

			/* We can never get a page on fpglst that has been
			 * modified.  No page is ever put on that has the
			 * mod bit set and we hold the region lock.
			 */
			ASSERT(!(skipmod && pg_ismod(pt)));
			ASSERT(ismrlocked(&rp->r_lock, MR_UPDATE));

#ifdef MH_R10000_SPECULATION_WAR
                        if (iskvirpte(pt) &&
                            IS_R10000()) {
                                __psint_t vfn = kvirptetovfn(pt);

                                krpf_remove_reference(pdetopfdat(pt),
                                                      vfn);
                                pg_clrhrdvalid(pt);
                                unmaptlb(0,vfn);
                        }
#endif /* MH_R10000_SPECULATION_WAR */

			pg_clrpgi(pt);
			PREG_NVALID_DEC(prp, 1);		/* SPT */

			if (--rlptr->gp_count == 0 && rp != lockedreg)
				dorele = 1;
	
			pglptr->gp_rlptr = NULL;

  		}
                if (pglptr->gp_ptptr){
                        pg_pfnrelease(pglptr->gp_ptptr);
                }
		/*
		 * We want to release the region lock after the ptelock,
		 * else a hi prio thread might start spinning for the
		 * ptelock without letting us release it, since we do
		 * not block preemption while holding ptelock.
		 */
		if (dorele)
			regrele(rp);
	}
}

/*
 * Increment the count of pages for a region.  Either
 *	it is the current region or the region is not yet
 *	in the list because we process each region only
 *	once during each pass of vhand.
 */
static void
bumprcnt(register preg_t *prp)
{
	register int i;

	/*	If this region is not in the region list,
	 *	then add it now.  Otherwise, just increment
	 *	the count of pages being stolen from this
	 *	region.
	 */
	if (gprglst[gprgndx].gp_count == 0  ||
	    gprglst[gprgndx].gp_prp->p_reg != prp->p_reg) {

		/*	The region count list can get fragmented
		**	because we can process the free page list
		**	many time before we process the swap page
		**	list.  Therefore, we must search for a
		**	free slot in the region count list.  Note
		**	that the list is cleaned up and starts
		**	fresh at the start of a new pass in vhand.
		**	In addition, there should always be a
		**	free slot because we make the region count
		**	list big enough to hold a separate region
		**	for each entry on both page lists.
		*/

		if (gprglst[gprgndx].gp_count != 0) {
			i = gprgndx + 1;

			while (i != gprgndx) {
				if (i >= (NPGLSTS * maxpglst))
					i = 0;
				if (gprglst[i].gp_count == 0)
					break;
				i++;
			}

			if (i == gprgndx)
				cmn_err(CE_PANIC,
				  "bumprcnt - region count list overflow.");

			gprgndx = i;
		}
		gprglst[gprgndx].gp_prp = prp;
		gprglst[gprgndx].gp_count = 1;
	} else {
		gprglst[gprgndx].gp_count++;
	}
}

/*
 * freelist - unlock the regions of a list
 *	when the list does not really need to be processed.
 */
static void
freelist(register pglst_t *lp, register int count, register reg_t *lockedreg)
{
	register gprgl_t *rl;
	register reg_t *rp;
	register int i, dorele;
	/* REFERENCED */
        register int pfrval;

	ASSERT(count > 0);

	for (i = 0; i < count; i++, lp++) {
		dorele = 0;
		rl = lp->gp_rlptr;

                /*
                 * Effectively removing pfdat from swap list.
                 * We acquire the pfn lock to avoid migration after
                 * decrementing the reference count.
                 */
                if (lp->gp_ptptr) {
                        pg_pfnacquire(lp->gp_ptptr);
                }
                pfrval = pagefree(lp->gp_pfd);


                /*
                 * The pagefree call above could not have possibly
                 * really freed the page. We had the extra reference
                 * for the swap list.
                 */
                ASSERT(pfrval == 0);
                
		if (rl) {
			rp = rl->gp_prp->p_reg;
			if (--rl->gp_count == 0 && rp != lockedreg) {
				dorele = 1;
			}
		} else {

			/*
			 * This page was a P_SQUEUE page.  Since this page
			 * was logically allocated to vhand so it could be 
			 * swapped out, and it has not been swapped for some
			 * reason (swap I/O error or out of swap space), we
			 * must give it back to the page manager as an
			 * unreferenced anon page.
			 */

			ASSERT((lp->gp_pfd->pf_flags & (P_ANON|P_SWAP)) == P_ANON);
			pagefreesanon(lp->gp_pfd, 0);
		}

                if (lp->gp_ptptr) { 
                        pg_pfnrelease(lp->gp_ptptr);
                }
		/*
		 * We want to release the region lock after the ptelock,
		 * else a hi prio thread might start spinning for the
		 * ptelock without letting us release it, since we do
		 * not block preemption while holding ptelock.
		 */
		if (dorele)
			regrele(rp);
	}
}

/*
 * calculate RSS - use preglock rather than aspacelock
 * For use in places (like vhand) where we can't afford to grab
 * the aspacelock (since we could deadlock - a process could go SXBRK
 * while holding aspacelock for UPDATE).
 *
 * If the process is a share group member, it can't leave the share group
 * without grabbing the preglock (see detachshaddr). It can exit/exec
 * but it won't get far w/o preglock - it could be past all that in exit
 * but that should just mean that the address space was gone and the
 * lookup will fail.
 *
 * XXX currently computes rss for all members of AS
 */

void
vrecalcrss(pas_t *pas)
{
	preg_t	*prp;
	reg_t	*rp;
	pgcnt_t totrss = 0;
	int doingshd = 1;
	ppas_t *ppas;

	mutex_lock(&pas->pas_preglock, 0);
	prp = PREG_FIRST(pas->pas_pregions);
dopriv:
	while (prp) {
		rp = prp->p_reg;
		if (rp->r_flags & RG_PHYS) {
			prp = PREG_NEXT(prp);
			continue;
		}
		totrss += prp->p_nvalid;
		prp = PREG_NEXT(prp);
	}
	if (doingshd) {
		doingshd = 0;
		pas->pas_rss = totrss;
		totrss = 0;

		mraccess(&pas->pas_plistlock);
		ppas = pas->pas_plist;
		if (ppas == NULL) goto done;
		prp = PREG_FIRST(ppas->ppas_pregions);
		goto dopriv;
	} else {
		ppas->ppas_rss = totrss;
		if ((ppas = ppas->ppas_next) != NULL) {
			totrss = 0;
			prp = PREG_FIRST(ppas->ppas_pregions);
			goto dopriv;
		}
	}
done:
	/* all done */
	mrunlock(&pas->pas_plistlock);
	mutex_unlock(&pas->pas_preglock);
}

/*
 * Support for mapped files - these routines NOT directly called by
 * vhand
 */
int
getmpages(pas_t *pas, preg_t *prp, int flags, caddr_t vaddr, pgno_t npgs)
{
	register reg_t *rp = prp->p_reg;
	register pde_t *pt;
	auto pgno_t sz;
	int error;

	ASSERT(ismrlocked(&rp->r_lock, MR_UPDATE));

	/* SPT */
	if ((rp->r_flags & RG_PHYS)
	 || (prp->p_pglen == 0)
	 || ((prp->p_nvalid == 0) && !pmap_spt(prp->p_pmap)))
		return 0;

	while (npgs) {
		register pgno_t i;
		if ((pt = pmap_probe(prp->p_pmap, &vaddr, &npgs, 
						&sz)) == NULL)
			break;

		i = sz;
		ASSERT(i > 0 && i <= npgs);
		npgs -= i;

		error = tossmpages(pas, prp, pt, sz, flags);
		vaddr += ctob(i); 
		if (error)
			return error;
	}

	return 0;
}

/*
 *	A ``tosspages'' for mapped files.
 *
 *	Flags:
 *
 *	GTPGS_TOSS:  pages get freed.
 *	GTPGS_NOWRITE: toss without writing back
 *	GTPGS_NOLOCKS: return EBUSY if find a locked page
 *	0: dirty pages get pushed (synced) back to file system but are not freed
 *
 *	If called via mapdetach, 0.
 *	If called via msync, possibly TOSS or NOLOCKS.
 *	If called via truncate, TOSS and possibly NOWRITE.
 */
static int
tossmpages(
	register pas_t *pas,
	register preg_t *prp,
	register pde_t *pde,
	register pgno_t npgs,
	int flags)
{
	register pgno_t	j;
	register pde_t *pt;
	register pfd_t *pfd;
	register reg_t *rp = prp->p_reg;

	ASSERT(ismrlocked(&rp->r_lock, MR_UPDATE));
	ASSERT((flags & GTPGS_NOWRITE) == 0 || (flags & GTPGS_TOSS));
	ASSERT(rp->r_vnode);

	if ((flags & GTPGS_TOSS) && (rp->r_flags & RG_ISOLATE))
		return(EBUSY);

	/*	Force reference bit faults -- we want to
	 *	ensure that this region's process is forced
	 *	to vfault so it will wait on the region
	 *	lock while attempting to access the page.
	 */
	for (j = 0, pt = pde; j < npgs; j++, pt++)
		pg_clrhrdvalid(pt);

	if (flags & GTPGS_TOSS)
		tlbsync(0LL, allclr_cpumask, STEAL_PHYSMEM);

	/*
	 * Look through page table for valid pages to dump.
	 */
	for (j = 0, pt = pde; j < npgs; j++, pt++) {

                pg_pfnacquire(pt);

		if (!pg_isvalid(pt)){
			pg_pfnrelease(pt);
			continue;
		}


                pfd = pdetopfdat(pt);

		if (pfd->pf_flags & P_ANON) {
                        pg_pfnrelease(pt);
			continue;
                }

		if (pfd->pf_rawcnt && (flags & GTPGS_NOLOCKS)) {
                        pg_pfnrelease(pt);
			return(EBUSY);
                }

		/*
		 * One may wonder that if we are clearing the mod bit
		 * of a large page pte do we have to do the same to the other 
		 * ptes of the large page.
		 * For a large page we don't have to do anything here
		 * if a pte is dirty and is part of a large page
		 * then all the ptes in the large page are dirty too as
		 * pfault sets the mod bit on all the ptes for the large page.
		 * Therefore the ptes are consistent.
		 */

		if (pg_isdirty(pt) && !(pfd->pf_flags & P_BAD)) {

			if ((flags & GTPGS_NOWRITE) == 0) {
                                /*
                                 * Also increments reference count,
                                 * which prevents migration.
                                 */
				pdinsert(pfd);
                        }
			
			/*
			 * We are going to toss the pages. Clear the mod bit
			 * as we done a TLB sync.
			 */
			if (flags & GTPGS_TOSS) {
				pg_clrdirty(pt);
				pg_clrmod(pt);
			}
		}

		if (flags & GTPGS_TOSS) {
			if (pfd->pf_rawcnt) {
				/*
				 * If page is pinned, we can't go and
				 * remove pages from user address space. 
				 * Instead, just drop the extra reference
				 * we have, and let the page hang in.
				 * File system will either flush this
				 * page or simply toss the page. In either
				 * case, the page goes out of page cache.
				 */ 
                                pg_pfnrelease(pt);
                                continue;
			} else {
                                /*
                                 * XXX	pfd's page count will never (well, hardly ever)
                                 * XXX	go to 0 because the pdinsert above holds a
                                 * XXX	reference count.  Probably isn't worth fixing.
                                 * XXX	Would make more sense, though, to invert the
                                 * XXX	return value of pagefree to return the reference
                                 * XXX	count _after_ it was freed.
                                        if (pagefree(pfd))
                                             MINFO.freedpgs++;
                                 */
				if (pas) {
					/*
					 * msync case.
					 */
					VPAG_UPDATE_RMAP_DELMAP(PAS_TO_VPAGG(pas), JOBRSS_DECREASE, pfd, pt);
				} else {
					/*
					 * remapf case - it would be nice
					 * if we had the pas and did job
					 * couting only for miser ASs.
					 * Unfortunately, we do not store
					 * the AS in the vnode chain or in
					 * the pregion. It is also not easy
					 * to go from the pte->as, since the
					 * a/s might be in the middle of an
					 * exit or exec. remapf should be
					 * updated to force an exitter/execer
					 * to sleep until the remapf on the
					 * pregion has been completed (mostly
					 * achieved by reglock today, but
					 * not entirely - see remapf). Also,
					 * need to maintain pte->as properly,
					 * specially during an exec, by
					 * updating pointers properly during
					 * pmap_reserve/pmap_unreserve. This is
					 * one place where a miser job will
					 * have its page count screwed up.
					 */
					DELETE_MAPPING(pfd, pt);
				}

				(void)pagefree(pfd);
				if (pfd->pf_use <= 1)
					MINFO.freedpgs++;

				PREG_NVALID_DEC(prp, 1);	/* SPT */
				pg_clrpgi(pt);
			}
		}
                
                pg_pfnrelease(pt);
	}

	ASSERT(ismrlocked(&rp->r_lock, MR_UPDATE));
	return(0);
}

#ifdef	DEBUG

void
valid_reg_count()
{
	int	i;
	int	maxregs = maxpglst * NPGLSTS;

	for ( i = 0; i < maxregs; i++)
		if(gprglst[i].gp_count != 0)
			cmn_err(CE_PANIC,"Region count %d index %d addr 0x%x\n",
				gprglst[i].gp_count,i,&gprglst[i]);
}

#include "sys/idbgentry.h"

/*
 * Getpages list dump
 */
static void
idbg_dopglstnar(pglst_t *pglptr)
{
        register gprgl_t *rlst;

        qprintf("  0x%x:", (__psint_t)pglptr);
        if (rlst = pglptr->gp_rlptr)
                qprintf("preg 0x%x cnt %d", rlst->gp_prp, rlst->gp_count);
        qprintf(" ptptr 0x%x pfd 0x%x apn 0x%x sh 0x%x stat %d\n",
                pglptr->gp_ptptr, pglptr->gp_pfd, pglptr->gp_apn,
                pglptr->gp_sh, pglptr->gp_status);
}

void
match_pg_list(int listtype, gprgl_t *rlst)
{
	struct pglsts *s;
	int	i;


	s = &pglsts[listtype];

	if (listtype >= NPGLSTS) {
		qprintf("does not exist\n");
		return;
	}
	qprintf("Using listtype %d rlst %x\n",listtype, rlst);

	for ( i = 0; i < maxpglst; i++)
		if (s->list[i].gp_rlptr == rlst)
			idbg_dopglstnar(&s->list[i]);
}


static void
idbg_vhand_trace(__psint_t count)
{
	if (vhand_trace == NULL) {
		qprintf("vhand_trace buffer null\n");
		return;
	}
		
	ktrace_print_buffer(vhand_trace, 0, -1, count);
}
#endif /* DEBUG */
