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

#include "sys/param.h"
#include "sys/types.h"
#include "ksys/as.h"
#include <os/as/as_private.h> /* XXX */
#include "sys/sysmacros.h"
#include "sys/signal.h"
#include "sys/immu.h"
#include "sys/sbd.h"
#include "ksys/pid.h"
#include "ksys/vproc.h"
#include "sys/proc.h"
#include "sys/prctl.h"
#include "sys/buf.h"
#include "os/as/pmap.h"
#include "sys/systm.h"
#include "sys/sysinfo.h"
#include "sys/tuneable.h"
#include "sys/getpages.h"
#include "sys/debug.h"
#include "sys/cmn_err.h"
#include "sys/sema.h"
#include "sys/runq.h"
#include "sys/pfdat.h"
#include "sys/schedctl.h"
#include "sys/sched.h"
#include "sys/pda.h"			/* for isolated proc */
#include "sys/swap.h"
#include "sys/errno.h"
#include "sys/anon.h"
#include "sys/var.h"
#include <sys/page.h>
#include <sys/rtmon.h>
#include <sys/numa.h>
#include <sys/atomic_ops.h>
#include "ksys/exception.h"
#include "ksys/vm_pool.h"
#include "ksys/vmmacros.h"
#include "ksys/kern_heap.h"
#include "os/proc/pproc_private.h"	/* XXX bogus */

#ifdef SWAPDEBUG
int swappr = 0;
#endif
#ifdef DEADDEBUG
int deadpr = 0;
#endif

extern unsigned int	sxbrkcnt;
#ifdef MH_R10000_SPECULATION_WAR
extern unsigned int sxbrkcnt_any; /* sxbrkcnt for any memory */
extern unsigned int sxbrkcnt_high; /* sxbrkcnt for high memory */
extern unsigned int sxbrkcnt_low;  /* sxbrkcnt for low memory */
#endif /* MH_R10000_SPECULATION_WAR */
extern ulong		freeswap;

/*	The scheduler sleeps on runout when there is no one
**	to swap in.  It sleeps on runin when it could not 
**	find space to swap someone in or after swapping
**	someone in.
*/

sv_t	runout;

struct scan_s {
	vproc_t	*s_vproc;
	int	s_maxbad;
};

static int	deadscan(proc_t *, void *, int);
static void	brkmemdead(void);
static int 	ismemdead(void);
static int	tossreg(pas_t *, preg_t *);

extern uint_t	vkickedpages;	/* # pages kicked loose by vhand          */
extern int	vscan;		/* vhand scan # */

extern mutex_t	sanon_lock;	/* used to prevent races between freeing     */
				/* sanon regions and vhand swapping them out */

#ifdef DEBUG
kthread_t	*schedthread;	/* to identify when sched is running */
#endif

#ifdef MH_R10000_SPECULATION_WAR
static int
do_sxbrk_wakeups(sv_t *svp, unsigned int *cnt_p, int wakecnt)
{
	if (wakecnt > 0) {
		if (wakecnt >= (*cnt_p)) {
			sxbrkcnt -= (*cnt_p);
			(*cnt_p) = 0;
			sv_broadcast(svp);
		} else {
			while (wakecnt) {
				ASSERT_ALWAYS(sxbrkcnt > 0 && (*cnt_p) > 0);
				--sxbrkcnt;
				--(*cnt_p);
				--wakecnt;
				sv_signal(svp);
			}
		}
		return(1);
	}
	return(0);
}
#endif /* MH_R10000_SPECULATION_WAR */


/*
 * Memory scheduler
 */
void
sched(void)
{
	int s;
	int passes_without_wakeup=0, wakecnt;
	int check_zone_bloat_period = KMEM_ZONE_PERIOD;
#ifdef MH_R10000_SPECULATION_WAR
	int wakecnt_low;	/* wakecnt for low memory only */
	int wakecnt_high;	/* wakecnt for high memory only */
	int cnt;
#endif /* MH_R10000_SPECULATION_WAR */
	spl0();

#ifdef DEBUG
	schedthread = curthreadp;
#endif
	/*
	 * Work in Progress:
	 *
	 * Must create mechanism to halt selected address-space clients
	 * when memory gets tight, and re-enable previously-halted AS
	 * clients over time or as memory becomes available.
	 * 
	 * NOTE: normally runs once a second. However, if we have threads
	 * in sxbrk state, we may get kicked early if vhand gets
	 * enough memory to satify its thresholds. This doesnt happen
	 * very much - not enough to worry about excessive calls to 
	 * kmem_zone_curb_zone_bloat (see below).
	 */

	for (;;) {
		if ((--check_zone_bloat_period) == 0) {
			check_zone_bloat_period = KMEM_ZONE_PERIOD;
			kmem_zone_curb_zone_bloat();
		}

		if (GLOBAL_FREEMEM() < tune.t_gpgslo) {	
			if (ismemdead())
				brkmemdead();
		}

		s = mutex_spinlock(&sxbrk_lock);
		if (sxbrkcnt) {
			/*
			 * Calculate how many threads to wake up. For now, wake up
			 * 1 thread for each "t_gpgslo" (rounded down) pages of 
			 * memory that is available. If amount of memory is less than
			 * t_gpgslo, but greater than t_gpgslo/2 OR if nothing
			 * has been woken up for 5 sec, wake up a thread anyway.
			 *	XXX - arbitrary
			 */
			wakecnt = GLOBAL_FREEMEM() / tune.t_gpgslo;
			if (wakecnt == 0) {
				passes_without_wakeup++;
				if (passes_without_wakeup > 5 || GLOBAL_FREEMEM() > (tune.t_gpgslo/2))
					wakecnt++;
			}
				
#ifdef MH_R10000_SPECULATION_WAR
			if (IS_R10000()) {
				if (sxbrkcnt_low > 0 &&
				    smallk0_freemem > 0) {
					wakecnt_low = (smallk0_freemem / 2);
					if (wakecnt_low == 0 &&
					    passes_without_wakeup > 5)
						wakecnt_low++;
					if (wakecnt_low > sxbrkcnt_low)
						wakecnt_low = sxbrkcnt_low;
				} else
					wakecnt_low = 0;
				if (sxbrkcnt_high > 0 &&
				    (GLOBAL_FREEMEM() - smallk0_freemem) > 0) {
					wakecnt_high = ((GLOBAL_FREEMEM() - smallk0_freemem) / tune.t_gpgslo);
					if (wakecnt_high == 0 &&
					    passes_without_wakeup > 5)
						wakecnt_high++;
					if (wakecnt_high > sxbrkcnt_high)
						wakecnt_high = sxbrkcnt_high;
				} else
					wakecnt_high = 0;
				if (sxbrkcnt_any > 0 &&
				    wakecnt > 0 &&
				    (sxbrkcnt_low + sxbrkcnt_high) > 0 &&
				    wakecnt < (sxbrkcnt_any +
					       wakecnt_low +
					       wakecnt_high)) {
					cnt = wakecnt -
						(wakecnt_low +
						 wakecnt_high);
					if (cnt <= 0)
						wakecnt = 1;
					else
						wakecnt = cnt;
				}
				if (wakecnt > sxbrkcnt_any)
					wakecnt = sxbrkcnt_any;
				if (do_sxbrk_wakeups(&sv_sxbrk_low,&sxbrkcnt_low,wakecnt_low) ||
				    do_sxbrk_wakeups(&sv_sxbrk_high,&sxbrkcnt_high,wakecnt_high) ||
				    do_sxbrk_wakeups(&sv_sxbrk,&sxbrkcnt_any,wakecnt))
					passes_without_wakeup = 0;
			} else 
#endif /* MH_R10000_SPECULATION_WAR */
			if (wakecnt >= sxbrkcnt || wakecnt > 5) {
				sxbrkcnt = 0;
				passes_without_wakeup = 0;
				sv_broadcast(&sv_sxbrk);
			} else if (wakecnt) {
				while (wakecnt) {
					--sxbrkcnt;
					--wakecnt;
					sv_signal(&sv_sxbrk);
				}
				passes_without_wakeup = 0;
			}
		}

		/*
		 * Delay for 1 second and look again later
		 */
		sv_wait(&runout, PSWP, &sxbrk_lock, s);
	}
}

void
kicksched(void)
{
	/*
	 * Wake up the scheduler if there are processes in sxbrk
	 * AND thre is plenty of memory. This routine is
	 * called only from vhand at the end of it's cycle.
	 */
	if (sxbrkcnt && GLOBAL_FREEMEM() >= tune.t_gpgslo)
		sv_signal(&runout);
}

#ifdef EVEREST
/* on very large systems, vhand can take a long time to complete a scan */
#define NDEADSECS	20
#else
#define NDEADSECS	10
#endif
/*
 * ismemdead - check for deadlock due to overcommittal of memory
 * resources
 * Algorithm:
 *	someone waiting for memory/ no swap space to swap things out
 *	no progress for NDEADSECS seconds getting pages from vhand/sched
 * With RSS limits, its possible that no one is waiting for memory but in fact
 * there be no more swap space and effectivly deadlock
 * We solve this in spingreg() - where if we run out of swap trying to
 * trim a process we cause it to die
 */
static int
ismemdead(void)
{
	static int lasttick = 0;
	static uint_t lastvpages = 0;
	static uint_t lastvscan = 0;
	int rv = 0;
	int totalkick;

	if ((sxbrkcnt == 0 && freeswap > 0) ||
	    GLOBAL_FREEMEM() > tune.t_gpgslo) {
		/* plenty of memory or no-one waiting */
		lasttick = 0;
		return(0);
	}

	if (lasttick == 0) {
#ifdef DEADDEBUG
		if (deadpr)
			cmn_err(CE_CONT, "Start deadlock detect global freemem %d freeswap %d\n",
				GLOBAL_FREEMEM(), freeswap);
#endif
		/* start looking for deadlock */
		lasttick = lbolt;
		lastvpages = vkickedpages;
		lastvscan = vscan;
	} else if (lbolt - lasttick > NDEADSECS*HZ) {
		lasttick = 0;

		/* see if memory getters are dead */
		totalkick = (vkickedpages-lastvpages);
		if (totalkick == 0)
			rv = 1;
		/*
		 * if the memory getters aren't kicking even enough to get
		 * to t_gpgslo in 'n' seconds, and there
		 * is no swap and hasn't been, lets put them out of their
		 * misery
		 */
		if (freeswap == 0 && totalkick < tune.t_gpgslo)
			rv = 1;

		/*
		 * if vhand gets stuck writing to a dead NFS server - it
		 * won't be able to scan - lets not kill things over that
		 */
		if (rv) {
			if (vscan == lastvscan) {
				cmn_err_tag(1802, CE_ALERT, "Paging Daemon (vhand) not running. NFS server down?\n");
				rv = 0;
			} else {
				pgno_t nswap, nvswap;

				getmaxswap(&nswap, &nvswap, NULL);
				if (nvswap > nswap)
					cmn_err_tag(1803, CE_ALERT,
						"Swap allocation overflow?\n");
			}
		}
#ifdef DEADDEBUG
		if (rv == 0 && deadpr)
			cmn_err(CE_CONT,
				"No deadlock vk %d lc %d vc %d\n",
				lastvpages+vkickedpages,
				lastvscan, vscan);
#endif
	}
	return(rv);
}

#define NREGLOCKRETRY	10
#define NREGLOCKWAIT	10	/* wait 10 ticks between tries */

/*
 * brkmemdead - break memory deadlock
 */
static void
brkmemdead(void)
{
	struct scan_s	sb;
	vproc_t		*vp;
	proc_t		*p;
	preg_t		*prp;
	int		rv;
	int		errors = 0;
	int		doingshd = 0;
	vasid_t		vasid;
	pas_t		*pas;
	ppas_t		*ppas;
	int		nretry = NREGLOCKRETRY;

#ifdef DEBUG
	cmn_err(CE_CONT, "Memory deadlock global freemem %d freeswap %d rmem %d smem %d\n",
		GLOBAL_FREEMEM(), freeswap, GLOBAL_AVAILRMEM(), GLOBAL_AVAILSMEM());

	/* we should NEVER get here if there is NO virtual swap */
	{
	pgno_t nvswap;

	getmaxswap(NULL, &nvswap, NULL);
	if (nvswap == 0) {
		cmn_err(CE_CONT, "IGNORING: no virtual swap so shouldn't ever get here!\n");
		return;
	}
	}
#endif

	/*
	 * returns process locked(noexit)
	 */
	procscan(deadscan, &sb);
	vp = sb.s_vproc;
	if (vp == NULL) {
		cmn_err_tag(1804, CE_ALERT, "Memory Deadlock with no one to kill!\n");
		return;
	}

	VPROC_GET_PROC(vp, &p);
	uscan_hold(&p->p_proxy);

	/*
	 * grab AS - if they don't have an AS, that's a transient
	 * condition and either they won't be picked next time or
	 * next time they will have an AS
	 */
	if (as_lookup(prxy_to_thread(&p->p_proxy)->ut_asid, &vasid)) {
		uscan_rele(&p->p_proxy);
		VPROC_RELE(vp);
		return;
	}

	/*
	 * lets kill it!
 	 * If its a shared address process must kill ALL of them - otherwise
	 * the taking away of memory will wreak havoc on the still running
	 * ones
	 */
	if (IS_SPROC(&p->p_proxy)) {
		register shaddr_t *sa = p->p_shaddr;
		register proc_t *pp;
		int anyrun;

		mutex_lock(&sa->s_listlock, 0);
		for (pp = sa->s_plink; pp; pp = pp->p_slink) {
			sigtopid(pp->p_pid, SIGKILL, SIG_ISKERN|SIG_NOSLEEP,
				0, 0, 0);
                         cmn_err(CE_ALERT,
                         "!%s[%d] was killed due to a memory deadlock condition.",
                         proc_name((proc_handl_t *)pp),pp->p_pid);
		}
		mutex_unlock(&sa->s_listlock);

		/* now that they all have KILL signals,
		 * we need wait till
		 * none of them are actually running.
		 * At that point, we can steal the memory,
		 * without worrying
		 * whether they will ever access it again
		 */
		do {
			anyrun = 0;
			mutex_lock(&sa->s_listlock, 0);
			for (pp = sa->s_plink; pp; pp = pp->p_slink) {
				if (UT_TO_KT(prxy_to_thread(&pp->p_proxy))->k_sonproc >= 0) {
#ifdef DEADDEBUG
					if (deadpr)
						cmn_err(CE_CONT, "brkmemdead waiting for 0x%x\n", pp);
#endif
					anyrun++;
					break;
				}
			}
			mutex_unlock(&sa->s_listlock);
			if (anyrun)
				delay(1);
		} while (anyrun);
	} else {
		/* don't kill isolated process XXX ?*/
		sigtopid(p->p_pid, SIGKILL, SIG_ISKERN|SIG_NOSLEEP, 0,0,0);
                cmn_err(CE_ALERT,
                  "!%s[%d] was killed to end a memory deadlock condition.",
                  proc_name((proc_handl_t *)p),p->p_pid);
		while (UT_TO_KT(prxy_to_thread(&p->p_proxy))->k_sonproc >= 0) {
#ifdef DEADDEBUG
			if (deadpr)
				cmn_err(CE_CONT,
					"brkmemdead waiting for 0x%x\n", p);
#endif
			delay(1);
		}
	}

	/* just in case, give it a high priority */
	setinfoRunq(UT_TO_KT(prxy_to_thread(&p->p_proxy)), &p->p_proxy.prxy_sched, RQRTPRI, NDPHIMAX);

	/* except, if its SXBRK or swapped out, how will it run??
	 * so we attempt to strip it of its memory, and hope thats
	 * enough so that it can run
	 *
	 * It doesn't seem to matter if the process can in fact start to
	 * run while we're stealing pages from it. It can never run in user-
	 * land again (cause we sent it a signal and waited for it to go
	 * non-running) and we obey all the locking protocols on regions,
	 * etc.
	 */

	cmn_err_tag(1805, CE_ALERT,
		"Process [%s] pid %d killed due to insufficient memory/swap.\n",
		p->p_comm, p->p_pid);
	/*
	 * we only need access lock, if someone has it for UPD,
	 * it doesn't seem like they'll hold it forever
	 * However, they may be SXBRK which could result from a grow()
	 * which then would have the aspacelock(UPD)
	 * XXX Think we can replace all this with a simple preglock()
	 */
	while (--nretry >= 0) {
		if (VAS_TRYLOCK(vasid, AS_SHARED))
			break;
		delay(NREGLOCKWAIT);
	}
	if (nretry < 0) {
#ifdef DEADDEBUG
		if (deadpr)
			cmn_err(CE_CONT,
				"Skipped all regions! (shd locked) 0x%x\n");
#endif
		/* should we bump errors here and keep picking this guy? */
		/*errors++;*/
		goto giveup;
	}
	pas = VASID_TO_PAS(vasid);
	ppas = (ppas_t *)vasid.vas_pasid;
	prp = PREG_FIRST(ppas->ppas_pregions);
	doingshd = 0;
doshd:
	for (; prp; prp = PREG_NEXT(prp)) {

		rv = tossreg(pas, prp);
		if (rv < 0)
			/* couldn't get region locks, etc. */
			errors++;
	}
	if (!doingshd) {
		doingshd++;
		prp = PREG_FIRST(pas->pas_pregions);
		goto doshd;
	}
	VAS_UNLOCK(vasid);

giveup:
	if (errors == 0) {
		/* record so don't get picked again */
		p_flagset(p, SBBLST);
	}

	/* release ref on address space */
	as_rele(vasid);
	/* allow to exit */
	uscan_rele(&p->p_proxy);
	VPROC_RELE(vp);
}

/*
 * deadscan - look for the best guy to kill
 * Algorithm:
 *
 *	weighted sum of:
 *		process priority
 *		process size
 *		process start time - its bad to be started within the last
 *				last 5 minutes
 *		whether process is parent or not
 */
#define WPRI	0
#define WSTART	1
#define WSIZE	2
#define WPARENT	3

uint wk[] = {
	10,	/* range 0 - 255 */
	1000,	/* range 0 - 1 */
	1,	/* range 1 - 64k */
	1000,	/* range 0 - 1 */
};
	
static int
deadscan(register proc_t *p, void *arg, register int mode)
{
	register struct scan_s *sbp = arg;
	extern time_t time;
	register uint wkill;
	register int s;
	vproc_t *vpr;
	vasid_t vasid;
	pas_t *pas;
	ppas_t *ppas;
	pgcnt_t psize;

	switch (mode) {
	case 0:
		sbp->s_vproc = VPROC_NULL;
		sbp->s_maxbad = 0;
		return 0;
	case 1:
		/* prevent proc from exiting */
		if ((vpr = VPROC_LOOKUP(p->p_pid)) == NULL)
			return 0;

		s = p_lock(p);
		if ((p->p_stat == SZOMB) ||
		    (p->p_flag & SBBLST) ||
		    (p->p_pid == INIT_PID)) {
			p_unlock(p, s);
			VPROC_RELE(vpr);
			return(0);
		}
		p_unlock(p, s);

		uscan_hold(&p->p_proxy);
		/*
		 * grab a ref to the address space
		 */
		if (as_lookup(prxy_to_thread(&p->p_proxy)->ut_asid, &vasid)) {
			uscan_rele(&p->p_proxy);
			VPROC_RELE(vpr);
			return 0;
		}

		if (!VAS_TRYLOCK(vasid, AS_SHARED)) {
			uscan_rele(&p->p_proxy);
			as_rele(vasid);
			VPROC_RELE(vpr);
			return (0);
		}
		pas = VASID_TO_PAS(vasid);
		ppas = (ppas_t *)vasid.vas_pasid;
		if ((ppas->ppas_flags & PPAS_ISO) ||
					CPUMASK_IS_NONZERO(pas->pas_isomask)) {
			uscan_rele(&p->p_proxy);
			VAS_UNLOCK(vasid);
			as_rele(vasid);
			VPROC_RELE(vpr);
			return (0);
		}
		/*
		 * note that as soon as we release the AS reference the
		 * process could go isolated - but so?
		 */
		psize = pas->pas_size + ppas->ppas_size;
		VAS_UNLOCK(vasid);
		as_rele(vasid);

		wkill = wk[WPRI] * INVERT_PRI(kt_pri(UT_TO_KT(prxy_to_thread(&p->p_proxy)))) +
			wk[WSTART] * ((time - p->p_start) > 5*60*HZ ? 0 : 1) +
			wk[WSIZE] * psize +
			wk[WPARENT] * (p->p_childpids ? 0 : 1);
		uscan_rele(&p->p_proxy);
		if (sbp->s_maxbad < wkill) {
			/* let previous bad-guy process exit */
			if (sbp->s_vproc) {
				VPROC_RELE(sbp->s_vproc);
			}
			/* this one will be blocked from exitting */
			sbp->s_vproc = vpr;
			sbp->s_maxbad = wkill;
		} else {
			VPROC_RELE(vpr);
		}
		return 0;
	case 2:
		return 0;
	}
	/* NOTREACHED */
	return(0);
}

/*
 * free up memory associated with a region.
 * Returns:
 *	0 if ok
 * 	-1 if couldn't get the region locks
 *	>0 # of locked pages
 * The tricky part here is that a number of places could be sleeping
 * with references to pte that we are going to change.
 */
static int
tossreg(pas_t *pas, preg_t *prp)
{
	register reg_t	*rp;	/* pointer to a locked region */
	register int	nretry = NREGLOCKRETRY;
	auto pgno_t	size;
	auto pgno_t	tsize;
	auto char	*vaddr;
#ifdef DEADDEBUG
	int		nfree = 0;
#endif
	int		nlocked = 0;

	rp = prp->p_reg;

	while (--nretry >= 0) {
		if (creglock(rp))
			break;
		delay(NREGLOCKWAIT);
	}
	if (nretry < 0) {
#ifdef DEADDEBUG
		if (deadpr)
			cmn_err(CE_CONT, "Skipped region (locked) 0x%x #pages %d\n",
				rp, rp->r_pgsz);
#endif
		return(-1);
	}

	if ((rp->r_flags & RG_PHYS) || (rp->r_refcnt != 1)) {
		/* very anti-social to trash shared regions */
#ifdef DEADDEBUG
		if ((deadpr) && (rp->r_refcnt != 1))
			cmn_err(CE_CONT, 
				"Skipped region (use > 1) 0x%x #pages %d\n", 
				rp, rp->r_pgsz);
#endif
		regrele(rp);
		return(0);
	}

	vaddr = prp->p_regva;
	tsize = prp->p_pglen;

	while (tsize) {
		register pde_t	*pt;

		pt = pmap_probe(prp->p_pmap, &vaddr, &tsize, &size);
		if (pt == NULL)
			break;

		tsize -= size;

		for ( ; --size >= 0; pt++, vaddr += NBPP) {
			pfd_t *pfd;

                        pg_pfnacquire(pt);

			/* Got to check  for valid bit only after acquiring
			 * pte lock. Otherwise, it may become invalid between
			 * checking for value, and getting lock.
			 */
			if (!pg_isvalid(pt)){
				pg_pfnrelease(pt);
				continue;
			}
                        
			pfd = pfntopfdat(pg_getpfn(pt));
                        
			/*
			 * free unlocked pages
			 */
			if (pfd->pf_rawcnt == 0) {
				ASSERT(pg_isshotdn(pt) == 0);
				prp->p_nvalid--;

				if (IS_LARGE_PAGE_PTE(pt))
					pmap_downgrade_lpage_pte(pas,
								vaddr, pt);
                                /*
                                 * Get rid of reverse map
                                 */
				VPAG_UPDATE_RMAP_DELMAP(PAS_TO_VPAGG(pas),
					JOBRSS_DECREASE, pfd, pt);
                                pg_pfnrelease(pt);
                                
				/* Change to zero pte's. */
                                pg_clrpgi(pt);
                                
				/*
				 * Free pages that aren't being used
				 * by anyone else.
				 * First make sure process
				 * cannot access page via tlb
				 * XXX Ouch!  This is expensive!
				tlbsync(0, STEAL_PHYSMEM);
				 */
				tlbclean(pas, btoct(vaddr), allclr_cpumask);
				pagefreeanon(pfd);
#ifdef DEADDEBUG
				nfree++;
#endif
			} else {
                                pg_pfnrelease(pt);
                                nlocked++;
                        }
		}
	}

	if (nlocked == 0) {
		if (rp->r_flags & RG_ANON) {

			/* 
			 * Free the anon resources to free up the swap space.
			 * Keep the empty anon around by doing a hold on it 
			 * in case someone just started a page fault or 
			 * something when we sent them the SIGKILL. The
			 * victim process itself or a shaddr member might
			 * be faulting in.
			 */

			ASSERT(rp->r_anon);
			anon_hold(rp->r_anon);
			if (rp->r_flags & RG_HASSANON) {
	
			    /*
			     * Make sure vhand isn't swapping sanon pages while
			     * we're trying to free them.  We use one global
			     * lock to cover all sanon regions since vhand
			     * doesn't have access to the individual region
			     * locks.  It only has a list of unreferenced sanon
			     * pages.
			     */
    
			    mutex_lock(&sanon_lock, PZERO);
			    anon_free(rp->r_anon);
			    mutex_unlock(&sanon_lock);
			} else
			    anon_free(rp->r_anon);

		}
	}

	regrele(rp);
#ifdef DEADDEBUG
	if (deadpr)
		cmn_err(CE_CONT, "Reclaimed region 0x%x #pages %d #locked %d\n",
				rp, nfree, nlocked);
#endif
	return(nlocked);
}

