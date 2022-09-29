/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.22 $"

#include <sys/cmn_err.h>
#include <sys/types.h>
#include <sys/debug.h>
#include <sys/immu.h>
#include <sys/idbgentry.h>
#include <sys/kmem.h>
#include <sys/map.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/pfdat.h>
#include <sys/prctl.h>
#include <sys/proc.h>
#include <sys/reg.h>
#include <sys/sysinfo.h>
#include <sys/systm.h>	/* for prototypes */
#include <sys/timers.h>
#include <sys/uthread.h>
#include <sys/var.h>

extern int picache_size;

#ifdef DEBUG
static void dump_icacheinfo(int);
#endif

void tfp_flush_icache(void *, unsigned int);

static struct zone *icachepid_zone = NULL;

/*
 * Software structures for icache pid management.
 */
typedef struct icachepid_item_s icachepid_item_t;

struct icachepid_item_s {
	icachepid_item_t	*ti_next;
};

typedef struct icacheinfo {
	/* For icachepid management */
	int64_t			ti_lastuser[ICACHE_NPID];
	icachepid_item_t	*ti_tail;
	icachepid_item_t	ti_item[ICACHE_NPID];
	icachepid_item_t	*ti_prev[ICACHE_NPID];
	__psint_t		ti_flushaddr;
	int			ti_flushlines;
#ifdef ULI
	/* For private tlbpid allocation */
	int			ti_total_pids;
#endif

} icacheinfo_t;

#define NO_LASTUSER ((int64_t)-1)

/* Use highest icachepid value to indicate "no icachepid" */
#define ICACHEPID_NONE ((icachepid_t)(-1))


/*
 * Initialize icachepid management structures.
 */
void
icacheinfo_init(void)
{
	icacheinfo_t *tip;
	int i;

	tip = private.p_icacheinfo = 
		(icacheinfo_t *)kmem_alloc(sizeof(icacheinfo_t), KM_NOSLEEP);
	/* icacheinfo_init is called early on, so shouldn't fail */

	ASSERT(tip); 

	/* Set up LRU initial state */
	for (i=0; i<ICACHE_NPID; i++) {

		tip->ti_lastuser[i] = NO_LASTUSER;

		tip->ti_item[i].ti_next = &(tip->ti_item[i+1]);

		tip->ti_prev[i] = &(tip->ti_item[i-1]);
	}

	/* 0 is unused, so pull it off the list */
	tip->ti_item[ICACHE_NPID-1].ti_next = &(tip->ti_item[1]);
	tip->ti_prev[1] = &(tip->ti_item[ICACHE_NPID-1]);

	/* Good hygiene: clear unused fields */
	tip->ti_prev[0] = NULL;	
	tip->ti_item[0].ti_next = NULL;

	/* Verify a basic assumption about this algorithm */
	ASSERT(picache_size == ICACHE_SIZE);
	/* Only makes sense to flush full cache lines.
	 * On TFP systems with 16K icache and 32 byte linesize, we end up
	 * flushing 3 lines (or 96 bytes) each time. Since we have 254
	 * usable icache IASIDs, we flush about 24K before reusing a pid.
	 */
	tip->ti_flushlines = (ICACHE_SIZE/ICACHE_LINESIZE/(ICACHE_NPID-1))+1;
	tip->ti_flushaddr = (__psint_t)0;

	tip->ti_tail = &(tip->ti_item[ICACHE_NPID-1]);

#ifdef ULI
	/* Initialize private pid allocation data */
	tip->ti_total_pids = ICACHE_NPID - 1;
#endif

	if (icachepid_zone == NULL)
		icachepid_zone = kmem_zone_init(sizeof(icachepid_t) * maxcpus,
						"icache pids");

#if DEBUG
	if (cpuid() == 0)
		idbg_addfunc("icinfo", dump_icacheinfo);
#endif
}


/*
 * See if process has a valid icachepid which hasn't been stolen by some
 * other process running on this processor.
 */
int
icachepid_is_usable(utas_t *utas) 
{
	return((icachepid(utas) != ICACHEPID_NONE) && 
		(private.p_icacheinfo->ti_lastuser[icachepid(utas)] == utas->utas_tlbid) ||
		(icachepid(utas) == 0));
}


/*
 * Specified uthread will use the specified icachepid, so move icachepid to
 * end of LRU list.  Every couple of times through icachepid_use, zap one
 * random tlb entry.  This insures that by the time we cycle through
 * all the icachepids, the entire tlb will have been flushed.
 *
 * We don't do anything special to handle wired tlb entries, since the
 * resume code flushes these on every context switch.
 */
void
icachepid_use(utas_t *utas, icachepid_t icachepid)
{
	icacheinfo_t *tip; 
	icachepid_item_t *prevp, *myp, *tail;

	/*
	 * System processes use icachepid 0, which is not managed by this code.
	 */
	if (icachepid == 0)
		return;

	ASSERT(icachepid != ICACHEPID_NONE);

	tip = private.p_icacheinfo;
	tip->ti_lastuser[icachepid] = utas->utas_tlbid;

	prevp = tip->ti_prev[icachepid];
	myp = prevp->ti_next;

	/*
	 * If this icachepid is already at the tail of the LRU list, we
	 * leave the list alone.  Otherwise, move this icachepid to the end.
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

	/* Flush a portion of the icache each time */

	tfp_flush_icache((void *)tip->ti_flushaddr, tip->ti_flushlines);

	tip->ti_flushaddr = (__psint_t)(tip->ti_flushaddr +
				(tip->ti_flushlines * ICACHE_LINESIZE));

	if (tip->ti_flushaddr > (__psint_t)ICACHE_SIZE)
		tip->ti_flushaddr = (__psint_t)0;
}

#ifdef ULI
/* allocate a private icachepid for the exclusive use of one process
 * NOTE must be freed when process exits or we lose it forever!
 */

#ifdef DPRINTF
#define dprintf(x) printf x
#else
#define dprintf(x)
#endif

icachepid_t
alloc_private_icachepid(void)
{
    icacheinfo_t *tip;
    icachepid_item_t *itemp, *prevp;
    int s, pid;

    tip = private.p_icacheinfo;

    /* Let's leave some reasonable number of pids so we don't get down
     * to the point where we spend all of our time flushing the icache
     */
    if (tip->ti_total_pids <= 128) {
	dprintf(("out of icachepids %d\n", tip->ti_total_pids));
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
    tip->ti_flushlines = (ICACHE_SIZE/ICACHE_LINESIZE/(tip->ti_total_pids))+1;

    splx(s);
    return(pid);
}

void
free_private_icachepid(icachepid_t pid)
{
    icachepid_item_t *itemp, *nextp;
    icacheinfo_t *tip;
    int s;

    tip = private.p_icacheinfo;

    itemp = &tip->ti_item[pid];
    
    s = splhi();

    /* insert at tail of LRU list so it won't be used again
     * until we've been through the entire list
     */
    nextp = tip->ti_tail->ti_next;
    itemp->ti_next = nextp;
    tip->ti_prev[pid] = tip->ti_prev[nextp - tip->ti_item];
    tip->ti_prev[pid]->ti_next = itemp;
    tip->ti_prev[nextp - tip->ti_item] = itemp;
    tip->ti_tail = itemp;

    tip->ti_total_pids++;
    tip->ti_flushlines = (ICACHE_SIZE/ICACHE_LINESIZE/(tip->ti_total_pids))+1;

    splx(s);
}
#endif

/*
 * ICACHE PIDs are managed on a per-processor basis.
 * See tlbmgr.c for algorithm
 */
void
new_icachepid(struct utas_s *utas, int flag)
{
	register        int i, maxc = maxcpus;
	register	unsigned char *s;
	register 	icacheinfo_t *tip;
	int		s1;

	if (flag & VM_TLBINVAL) {
		s = utas->utas_icachepid;
		for (i = maxc; i > 0; i--)
			*s++ = ICACHEPID_NONE;
	} else {
		/*
		 * Just to make sure that whoever calls new_icachepid()
		 * understands the flag argument, and isn't passing
		 * ``0'' because its fashionable.
		 */
		ASSERT(flag & VM_TLBVALID);
	}

	/*
	 * since we can be called from clock, we need to protect ourselves.
	 * if we remove the stuff from clock - remove these spls!
	 * this makes the code preemption safe too.
	 */
	s1 = splhi();
	tip = private.p_icacheinfo;
	icachepid(utas) = tip->ti_tail->ti_next - tip->ti_item;
	ASSERT(icachepid(utas) != ICACHEPID_NONE);
	ASSERT(icachepid(utas) < ICACHE_NPID);

	icachepid_use(utas, icachepid(utas));

	/*
	 * set_icachepid sets current icachepid.
	 * Note that new_icachepid is sometimes
	 * called on a non-current thread
	 */
	if (curuthread && &curuthread->ut_as == utas && !(flag & VM_TLBNOSET)) {
		ASSERT(UT_TO_KT(curuthread)->k_sonproc == cpuid());
		set_icachepid(icachepid(utas));
	}
	splx(s1);
}


#ifdef DEBUG
/*
 * For idbg, dump icacheinfo information.
 * If flag is set, dump full information; otherwise, just dump the
 * executive summary.
 */
static void
dump_icacheinfo(int cpu)
{
	icacheinfo_t *tip;
	icachepid_item_t *ti;
	int i;

	if ((cpu < 0) || (cpu > maxcpus))
		cpu = cpuid();

 	tip = pdaindr[cpu].pda->p_icacheinfo;
	qprintf("  icache management info for cpu %d (0x%x)\n", cpu, tip);
	qprintf("flushaddr=0x%x flushlines=0x%x\n",
		tip->ti_flushaddr, tip->ti_flushlines);


	qprintf("icachepid (+lastuser) LRU list:\n");
	for (i=1, ti=tip->ti_tail->ti_next; i<ICACHE_NPID; i++, ti=ti->ti_next) {
		qprintf("%d (%d) ", ti-tip->ti_item, 
			tip->ti_lastuser[ti-tip->ti_item]);
		if ((i % 8) == 0)
			qprintf("\n");
	}
	qprintf("\n");
}
#endif	/* DEBUG */

icachepid_t *
icachepid_memalloc(void)
{
	int i;
	icachepid_t *ip = kmem_zone_alloc(icachepid_zone, KM_SLEEP);
	icachepid_t *ipt;

	for (i = maxcpus, ipt = ip; i > 0; i--, ipt++)
		*ipt = ICACHEPID_NONE;

	return ip;
}

void
icachepid_memfree(icachepid_t *ip)
{
	kmem_zone_free(icachepid_zone, ip);
}
