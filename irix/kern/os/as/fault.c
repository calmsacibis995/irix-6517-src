/**************************************************************************
 *									  *
 *		 Copyright (C) 1989-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 3.463 $"

/*
 * This file is formatted to be best displayed and modified with a screen
 * that is over 80 characters wide, except for block comments.  In general,
 * please do not manually fold long lines, except in comments.
 */

#include <bstring.h>
#include <sys/types.h>
#include <sys/anon.h>
#include <ksys/as.h>
#include "as_private.h"
#include <sys/atomic_ops.h>
#include <sys/buf.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <ksys/exception.h>
#include <sys/fault_info.h>
#include <sys/kabi.h>
#include <sys/kmem.h>
#include <sys/getpages.h>
#include <sys/immu.h>
#include <sys/lock.h>
#include <sys/lpage.h>
#include <sys/nodepda.h>
#include <sys/numa.h>
#include <sys/par.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/pfdat.h>
#include "pmap.h"
#include <sys/prctl.h>
#include <sys/proc.h>
#include <procfs/prsystm.h>
#include <sys/psw.h>
#include <sys/reg.h>
#include <sys/resource.h>
#include <sys/rt.h>
#include <sys/rtmon.h>
#include <sys/sbd.h>
#include <sys/sema.h>
#include <sys/swap.h>
#include <sys/sysinfo.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/timers.h>
#include <sys/tuneable.h>
#include <sys/uio.h>
#include <sys/uthread.h>
#include <sys/vnode.h>
#include <sys/watch.h>
#ifdef _MEM_PARITY_WAR
#include <sys/var.h>
#endif /* _MEM_PARITY_WAR */
#include <ksys/vm_pool.h>
#include <ksys/vpag.h>
#include <ksys/vmmacros.h>
#include <sys/miser_public.h>
#include <ksys/vproc.h>
#include <ksys/cell.h>
#ifdef _R5000_CVT_WAR
#include <sys/mtext.h>
extern int R5000_cvt_war;
#endif /* _R5000_CVT_WAR */

extern int faultdebug;
extern int syssegsz;
extern int ensure_no_rawio_conflicts;
extern int isunreserved(reg_t *, pgno_t);
extern void map_prda(pfd_t *, caddr_t);
extern int dbe_recovery;

#define STATIC static

STATIC int autogrow(vnode_t *, off_t, uthread_t *);
STATIC void killpage(preg_t *rp, pde_t *pd);

STATIC  int     lpage_validate(pas_t *, uvaddr_t, attr_t *, preg_t *, size_t);
STATIC  int     lpage_vfault_test(pas_t *, uthread_t *, uvaddr_t, preg_t *,
						size_t, faultdata_t *);
STATIC  int	lpage_vfault(pas_t *, uthread_t *, uvaddr_t, preg_t *,
			pgcnt_t *, faultdata_t *, fault_info_t **);
STATIC  int     lpage_pfault(pas_t *, caddr_t, preg_t *, size_t, faultdata_t *, int *);
STATIC  void    lpage_fault_recover(pas_t *, fault_info_t *, caddr_t, int);
STATIC  void    lpage_tlbdropin(pas_t *, ppas_t *, uvaddr_t, size_t, pde_t *);
STATIC  int     lpage_swapin(pas_t *, uthread_t *, faultdata_t *, pgcnt_t, preg_t *, caddr_t);
STATIC	int	vfault_wait_sentry(pas_t *, pde_t *, reg_t *);
STATIC	int	vfault_wait_pagedone(pas_t *, pfd_t *, reg_t *);
STATIC	int	vfault_wait_lpage(pas_t *, reg_t *, size_t, long);
#ifdef SN0
STATIC  void    handle_fetchop_fault(pfd_t *, int);
#endif

#ifdef	DEBUG
STATIC	int	validate_tlb_entry(caddr_t, size_t);
#endif


#ifdef _VCE_AVOIDANCE
#ifdef R4600SC
extern int two_set_pcaches;
#endif /* R4600SC */

STATIC int preg_color_validation(struct pregion *,pfd_t *,caddr_t,int);
#define PREG_COLOR_VALIDATION(A,D,E,F) \
	(vce_avoidance ? preg_color_validation((A),(D),(E),(F)) : 0)
#else /* _VCE_AVOIDANCE */
#define PREG_COLOR_VALIDATION(A,D,E,F)
#endif /* _VCE_AVOIDANCE */

#if JUMP_WAR
extern int R4000_jump_war_correct;
extern int R4000_div_eop_correct;
extern int R4000_jump_war_always;
extern int R4000_jump_war_warn;
extern int R4000_jump_war_kill;
int R4000_debug_eop=0;
STATIC int end_of_page_bug(pde_t *, caddr_t);
#ifdef _R5000_JUMP_WAR
extern int R5000_jump_war_correct;
#endif /* _R5000_JUMP_WAR */
#endif

#if FAST_LOCORE_TFAULT
#if KSTKSIZE != 1
<< bomb  - uarea/kstack must be in same tlb as PDA page >>
#endif
#if _MIPS_SIM != _ABI64
<< bomb  - locore code only works with 64-bit kernels >>
#endif
#endif /* FAST_LOCORE_FAULT */

#if !NO_WIRED_SEGMENTS && !FAST_LOCORE_TFAULT
STATIC int tfaultnomem = 0;

#if TFP
<< bomb - TFP doesn't do tfault >>
#endif
#if CELL
<< bomb - This routine hasn't been virtualized >>
#endif

/*
 * tfault handles tlb misses to k2seg:
 *	User/kernel double tlb misses for accesses to kuseg.
 *	Kernel tlb misses for direct k2seg accesses.
 * NOTE: must hold region lock around ALL tlbdropins - scenario:
 * grab contents of pde, and about to call tlbdropin; then get an interrupt
 * and another cpu starts getpages, invalidates the pde in memory, tlbflushes
 * then you return and drop in a bad page!! Whew..
 */

#define PRIMARY_PTE(x)		(((caddr_t) x >= (caddr_t) KPTEBASE) && \
			   ((caddr_t) x < (caddr_t) (KPTEBASE + KPTE_USIZE)))
#define SECONDARY_PTE(x)	(((caddr_t) x >= (caddr_t) KPTE_SHDUBASE) && \
			   ((caddr_t) x < (caddr_t) (KPTE_SHDUBASE + KPTE_USIZE)))
#define ISPRDA(x)		(vfntova(vatovfn(x)) == (caddr_t) PRDAADDR)

/* ARGSUSED1 */
int
tfault(
	eframe_t	*ep,
	uint		code,
	caddr_t		vaddr,
	int		*type)
{
	pde_t		*pd;
	pde_t		*t_pde;
	reg_t		*rp = NULL;
	preg_t		*prp = NULL;
	caddr_t		uvaddr;
	uthread_t	*ut = curuthread;
	pde_t		pde2;
	k_machreg_t	ps = ep->ef_sr;
	char		*pvaddr;
	pgno_t		size = 1;
	pgno_t		sz;
	vasid_t		vasid;
	pas_t		*pas;
	ppas_t		*ppas;
	preg_t		*prpfirst;
	utas_t		*utas;
	uint		oddentry;	/* set if the faulting page is mapped by */
					/* the tlblo_1 entry of the tlb */
	tlbpde_t	*pdp;
	pde_t		savpde;

	int wireindx = -1;
	savpde.pgi = 0;

	VM_LOG_TSTAMP_EVENT(VM_EVENT_TFAULT_ENTRY, current_pid(), vaddr, code, 0);

	uvaddr = vaddr;	/* set early in case we goto segv label */
	if (!(PRIMARY_PTE(vaddr) || SECONDARY_PTE(vaddr))) {

		/* Following check is needed since 32-bit programs can
		 * actually generate a 64-bit address without
		 * generating an  ADE. If EPC is out of bounds it
		 * can't possibly be a BAD BADVADDR. Also note that
		 * a kernel panic may result if you call bad_badva
		 * on such an address (looks like kernel reference).
		 */
	  	if ((ep->ef_epc >= KUSIZE_32) || (!bad_badva(ep)))
			goto segv_nolock;

		/* Following check is needed for programs which
		 * perform a data reference outside the range of
		 * allowable 32-bit addresses.  We convert what might
		 * be a second level tlb badvaddr into the original
		 * address and see if it is a valid address for this epc.
		 * If so, it's not the MP_R4000_BAD_BADVADDR_WAR problem.
		 */
		ep->ef_badvaddr = (((caddr_t)vaddr - (caddr_t)KPTEBASE)
				     / sizeof(pde_t)) << PNUMSHFT;
		if (!bad_badva(ep)) {
			ep->ef_badvaddr = (k_machreg_t)vaddr;
			goto segv_nolock;
		}
		VM_LOG_TSTAMP_EVENT(VM_EVENT_TFAULT_EXIT, 0, 0, 0, 0);
		return 0;	/* really have BAD_BADVADDR */
	}
	as_lookup_current(&vasid);
	pas = VASID_TO_PAS(vasid);
	ppas = (ppas_t *)vasid.vas_pasid;
	mraccess(&pas->pas_aspacelock);
	utas = ppas->ppas_utas;
	ASSERT(utas == &ut->ut_as);
	ASSERT(ut == curuthread);
#if DEBUG
	if (!(PRIMARY_PTE(vaddr) || SECONDARY_PTE(vaddr)))
		goto segv;
#endif
	MINFO.tfault++;
	ut->ut_acct.ua_tfaults++;

	/* miss from utlbmiss handler */

	/*
	 * vaddr is the address in kpteseg that caused the fault.
	 * Calculate the corresponding user virtual address.
	 */

	if ((__psunsigned_t)uvaddr < (__psunsigned_t)KPTEBASE) {
#ifdef MP_R4000_BADVA_WAR
		/* We can get here (at least in the 64-bit world) due to
		 * a bad BADVADDR which is actually in KU space but which
		 * is not treated as KU space by tlbmiss() because the
		 * IS_KUSEG macro only recognizes valid 40-bit KU addresses.
		 * NOTE: Shouldn't be needed for 32-bit, but doesn't hurt.
		 */
		if ((__psunsigned_t)uvaddr < (__psunsigned_t)KPTE_SHDUBASE) {
			/* Verify the bug conditions */
			if ((((ep->ef_epc & POFFMASK) == (NBPP - 4)) ||
			     ((ep->ef_epc & POFFMASK) == 0x000)) &&
				bad_badva(ep))
				/* MP R4000 bug with bad badvaddr
				 * Bug requires MP intervention when code
				 * tlbmisses when executing through a
				 * page boundary.
				 */
				/* NOP */;
			else if (USERMODE(ep->ef_sr))
				/* This case should NOT occur */
				cmn_err(CE_WARN|CE_CPUID,
    "tfault(0): invalid badvaddr 0x%x epc 0x%llx sr 0x%llx, pid %d[%s]\n",
					vaddr, ep->ef_epc, ep->ef_sr,
					current_pid(), get_current_name());
			else
				cmn_err(CE_WARN|CE_CPUID,
    "tfault(1): invalid badvaddr 0x%x epc 0x%llx sr 0x%llx\n",
					vaddr, ep->ef_epc, ep->ef_sr);
			mrunlock(&pas->pas_aspacelock);
			VM_LOG_TSTAMP_EVENT(VM_EVENT_TFAULT_EXIT, 0, 0, 0, 0);
			return(0);
		}
#endif	/* MP_R4000_BADVA_WAR */
		ASSERT((__psunsigned_t)uvaddr >= (__psunsigned_t)KPTE_SHDUBASE);
		uvaddr += KPTE_USIZE;
	}
	uvaddr = (caddr_t)(((uvaddr - (caddr_t)KPTEBASE) / sizeof(pde_t)) << PNUMSHFT);

	/* now change vaddr into a base segment virtual address (in kpteseg) */
	oddentry = (__psint_t)vaddr & NBPP;

	/*
	 * Phase 1: find primary pmap.
	 *	if primary exists, and pgtbl entry not null, done.
	 *	if primary exists, but null pgtbl entry, try secondary.
	 *	if primary does not exist, try secondary ->
	 *		if no secondary -> findregion.
	 */
	if (!(pas->pas_flags & PAS_SHARED)) {
		ASSERT(PRIMARY_PTE(vaddr));
		/*
		 * Fast path. Usually pmap will contain required pte.
		 */
		pvaddr = uvaddr;
		size = 1;
		if ((pd = pmap_probe(pas->pas_pmap, &pvaddr, &size, &sz)) != 0) {
			t_pde = pd;
			goto foundpgtblp;
		}
		/* no pagetbl in primary */
		goto lookup_region;
	}

	/*
	 * A shared AS - for PRIMARY vaddrs, look in private list first
	 */
	if (PRIMARY_PTE(vaddr)) {
		if (ppas->ppas_pmap != NULL) {
			pvaddr = uvaddr;
			size = 1;
			if ((pd = pmap_probe(ppas->ppas_pmap, &pvaddr, &size, &sz)) != 0) {
				t_pde = pd;
				goto foundpgtblp;
			}
		}
		/* no pagetbl in primary */
	} else {
		ASSERT(SECONDARY_PTE(vaddr));
	}
	/*
	 * Phase 2: Try secondary pmap, either because fault was in
	 *		1. secondary (KPTE_SHDUBASE)
	 *		2. primary, but no pte in primary map.
	 */
	ASSERT(pas->pas_pmap);
	pvaddr = uvaddr;
	size = 1;
	pd = pmap_probe(pas->pas_pmap, &pvaddr, &size, &sz);
	if (pd != 0) {
		t_pde = pd;
		goto foundpgtblp;
	}

	/*
	 * Phase 3: Lookup region, either because no pgtblp, or no pmap.
	 * Either case, no region known yet.
	 */
 lookup_region:
	prp = findpreg(pas, ppas, uvaddr);
	if (prp == NULL) {
		if (ISPRDA(uvaddr)) {
			/* Special case: have to create region ... */
			int error;

			mrunlock(&pas->pas_aspacelock);
			mrupdate(&pas->pas_aspacelock);
			error = setup_prda(pas, ppas);
			mrunlock(&pas->pas_aspacelock);
			mraccess(&pas->pas_aspacelock);

			if (error) {
				cmn_err(CE_WARN, "Couldn't create prda for %s (pid %d)",
					get_current_name(), current_pid());
				goto segv;
			}
			prp = findpreg(pas, ppas, uvaddr);
			ASSERT(prp); /* just setup, should be there ... */
		} else  {
			/*
			 * Maybe the user faulted on the second of a pair of
			 * pages, and there is a region that starts with that
			 * second page. Let's take a look....
			 *
			 * If found, we'll go on the assumption that the user is
			 * accessing the second of a pair in a region that
			 * starts with the second page. We'll set up the tlb
			 * entry, and if it turns out that he really was
			 * accessing the invalid first page, he'll take a
			 * tlbinvalid fault, and be signalled by vfault.
			 */
			if ((prp = findpreg(pas, ppas,
				(__psunsigned_t) uvaddr & NBPP ? uvaddr - NBPP : uvaddr + NBPP)) == NULL)
				goto segv;
		}
	}
	ASSERT(prp); /* prp of either prda or second page */
	pvaddr = uvaddr; size = 1;
	pd = pmap_probe(prp->p_pmap, &pvaddr, &size, &sz);
	if (pd != 0) {
		t_pde = pd;
		goto foundpgtblp;
	}

	/*
	 * Create page table entry for looked-up region.
	 */
	rp = prp->p_reg;
	reglock_rd(rp);
	if ((pd = pmap_pte(pas, prp->p_pmap, uvaddr, VM_NOSLEEP)) == NULL) {
		tfaultnomem++;
		regrele(rp);
		rp = NULL;
		mrunlock(&pas->pas_aspacelock);
		setsxbrk();
		mraccess(&pas->pas_aspacelock);
		goto lookup_region;
	}

	t_pde = pd;

	/* If there's both private and shared (overlapping) address
	 * space for the process, any private page tables get mapped
	 * into the same KPTESEG space as the shared page tables,
	 * and any underlying shared tables get mapped in into the
	 * KPTE_USIZE virtual space immediately preceeding KPTEBASE.
	 *
	 * A KPTESEG page table access might occur in which the real
	 * target is a shared, overlapping user page.  In that case, we
	 * must be careful to deposit the _private_ page table in the
	 * KPTESEG space, not the shared page.
	 * The assumption here is that if there are any private mappings
	 * they all have the same pmap which is different than the shared one's
	 */
	prpfirst = PREG_FIRST(ppas->ppas_pregions);
	if (ppas->ppas_pmap &&
			prpfirst &&
			(__psunsigned_t)vaddr >= (__psunsigned_t)KPTEBASE) {
		pde_t *pde;

		ASSERT(pas->pas_flags & PAS_SHARED);
		ASSERT(prpfirst->p_pmap != prp->p_pmap);
		/* Guaranteed that if the spaces overlap in this segment,
		 * there will be a private, underlying page table page.
		 */
		pvaddr = uvaddr;
		size = 1;
		pde = pmap_probe(prpfirst->p_pmap, &pvaddr, &size, &sz);
		if (pde != NULL)
			t_pde = pde;
	}

 foundpgtblp:

	pg_setpgi(&pde2, mkpde(PG_VR|PG_M|pte_cachebits(), kvtophyspnum(t_pde)));

	vaddr = (caddr_t) ((__psunsigned_t)vaddr & TLBHI_VPNMASK);


	/*
	 * check that for some reason we already have the
	 * entry we faulted on.
	 * This can happen if trapstart (called from tlbmiss())
	 * actually faults something in
	 */
	{
		int	i;
		pde_t	*tpde;

		if (oddentry)
			tpde = &ut->ut_exception->u_ubptbl[0].pde_hi;
		else
			tpde = &ut->ut_exception->u_ubptbl[0].pde_low;
		for (i = 0; i < NWIREDENTRIES-TLBWIREDBASE; i++, tpde += 2) {
#if TLBKSLOTS == 1
			/* if TLBKSLOTS == 0, upage/kstack share a tlb entry
			 * with the pda */
			if (!USERMODE(ps) && (i == (UKSTKTLBINDEX-TLBWIREDBASE)))
				continue;
#endif
			if (ut->ut_exception->u_tlbhi_tbl[i] == (caddr_t)vaddr) {
				if (tpde->pgi == pde2.pgi)
					goto done;
				else {
					wireindx = i;
					goto wirethisindex;
				}
			}
		}
	}
#if TLBKSLOTS == 1
	if (!USERMODE(ps)) {
		/* if we came from the kernel (like from copyout)
		 * and we faulted on the pdes that should have
		 * been in the slot currently occupied by the
		 * upage and kernel stack, we must void that slot,
		 * and search for another place for them
		 */
		int tlb_idx;

		pdp = &ut->ut_exception->u_ubptbl[UKSTKTLBINDEX-TLBWIREDBASE];
		tlb_idx = UKSTKTLBINDEX-TLBWIREDBASE;
		if (vaddr == ut->ut_exception->u_tlbhi_tbl[tlb_idx]) {
			savpde.pgi = oddentry ? pdp->pde_low.pgi : pdp->pde_hi.pgi;
			pdp->pde_low.pgi = 0;
			pdp->pde_hi.pgi = 0;
			ut->ut_exception->u_tlbhi_tbl[tlb_idx] = (caddr_t)PHYS_TO_K0(0);
		}

		/* also do not want code below to try to wire into
		 * one of these slots
		 */
		if (ut->ut_exception->u_nexttlb == UKSTKTLBINDEX-TLBWIREDBASE)
			ut->ut_exception->u_nexttlb++;
		if (ut->ut_exception->u_nexttlb >= NWIREDENTRIES - TLBWIREDBASE)
			ut->ut_exception->u_nexttlb = 0;
	}
#endif	/* TLBKSLOTS == 1 */

	/*
	 * If the wired entry to which ut->ut_exception->u_nexttlb refers
	 * is valid, the wired tlb slots are full.  In that
	 * case, start up a segment table - this
	 * allows fast case code in exception- kmiss to fill
	 * in 2nd level misses.
	 *
	 * If process is already running in segment mode, but
	 * is using a borrowed global segment table, reset the
	 * segment table.  (The only time we can get here while
	 * already running in segment mode is if the pmap_pte
	 * call caused a page of page tables -- segment -- to be
	 * allocated when there wasn't one in the pmap already.)
	 *
	 * To permit persistence of access to particular
	 * segments to eventially get into the wired portion
	 * every tune_t_tlbdrop ticks in user space the process
	 * executes causes all the wired entries/segment table to
	 * be blown out and re-filled in.
	 */
	if (ut->ut_exception->u_ubptbl[ut->ut_exception->u_nexttlb].pde_low.pte.pg_vr ||
	    ut->ut_exception->u_ubptbl[ut->ut_exception->u_nexttlb].pde_hi.pte.pg_vr) {
		/*
		 * start up segment table
		 */
		initsegtbl(utas, pas, ppas);

		/* If fault came from kernel mode, then we need to do
		 * the dropin here since locore won't handle it.  Likewise,
		 * if an sproc process has a private segment for which
		 * there is no overlapping shared segment, then utlbmiss
		 * may be referencing a KPTE_SHDUBASE address for which there
		 * is no page.  So drop one in here.
		 */
		if (!USERMODE(ps) || utas->utas_utlbmissswtch & UTLBMISS_DEFER) {
			tlbdropin(utas->utas_tlbpid,
				  (caddr_t)((__psint_t)vaddr|oddentry),
				  pde2.pte);
		}
		goto donedropin;
	} else if (ut->ut_as.utas_segflags) {
		/* Should only get here if we setup a segment table,
		 * later moved the entries in the first two slots and
		 * still later faulted on an unallocated 2nd level
		 * map.  The 2nd level map has now been allocated, so
		 * return and let locore segment table code handle
		 * the actual tlbdropin.
		 */
		/* See comment in preceeding 'if' clause for conditions
		 * where tlbdropin is required.
		 */
		if (!USERMODE(ps) || utas->utas_utlbmissswtch & UTLBMISS_DEFER) {
			tlbdropin(utas->utas_tlbpid,
				  (caddr_t)((__psint_t)vaddr|oddentry),
				  pde2.pte);
		}
		goto donedropin;
	}
 wirethisindex:
	if (wireindx < 0)
		wireindx = ut->ut_exception->u_nexttlb;
	pdp = &ut->ut_exception->u_ubptbl[wireindx];
	if (savpde.pgi == 0) {
		savpde.pgi = oddentry ? pdp->pde_low.pgi : pdp->pde_hi.pgi;
	}

	if (oddentry) {
		pdp->pde_low.pgi = savpde.pgi;
		pdp->pde_hi.pgi = pde2.pgi;
	} else {
		pdp->pde_low.pgi = pde2.pgi;
		pdp->pde_hi.pgi = savpde.pgi;
	}
	ut->ut_exception->u_tlbhi_tbl[wireindx] = vaddr;

#if TLBKSLOTS == 1
	/*
	 * while running in kernel mode, 1 wired entry
	 * is really the kernel stack
	 * this will be restored on exit from kernel
	 */
	if (wireindx != UKSTKTLBINDEX-TLBWIREDBASE)
#endif
	{
		tlbwired(TLBWIREDBASE + wireindx,
			utas->utas_tlbpid,
			ut->ut_exception->u_tlbhi_tbl[wireindx],
			pdp->pde_low.pte,
			pdp->pde_hi.pte,
			TLBPGMASK_MASK);
	}
	if (wireindx == ut->ut_exception->u_nexttlb &&
	    ++ut->ut_exception->u_nexttlb >= NWIREDENTRIES - TLBWIREDBASE)
		ut->ut_exception->u_nexttlb = 0;
 donedropin:
	/* For R4000 it's much fast to let utlbmiss handler perform
	 * equivalent of tlbdropin, since that code doesn't need to
	 * do a probe (given that we've done setup of 2nd level tlb
	 * so that locore can drop it in -- or we've done the 2nd
	 * level droping here).
	 */

 done:
	if (rp)
		regrele(rp);
	mrunlock(&pas->pas_aspacelock);
	VM_LOG_TSTAMP_EVENT(VM_EVENT_TFAULT_EXIT, 0, 0, 0, 0);
	return(0);

 segv:
	mrunlock(&pas->pas_aspacelock);

 segv_nolock:

	/* fix up users reg's for trap for the kill */
	ep->ef_badvaddr = (__psint_t)uvaddr;

	ut->ut_code = EFAULT;
	*type = EFAULT;
	VM_LOG_TSTAMP_EVENT(VM_EVENT_TFAULT_EXIT, 0, 0, 0, SEXC_SEGV);
	return(SEXC_SEGV);
}
#endif /* !NO_WIRED_SEGMENTS && !FAST_LOCORE_TFAULT */


/* Copy on write
 * This routine returns 1 if a COW needs to be done for this
 * page and 0 otherwise.
 * If use is 1, and page is not from a file, steal it.
 * Otherwise, copy it.  On multiprocessor systems, ensure
 *    that other processors' tlbs won't run with stale
 *    entry for this (virtual) page.
 * If the page has ANON backing and its anon tag is
 * already == top level - then steal it regardless
 * of its reference count. This permits folks like
 * /proc to attach to a CW region and not get their own page
 * We never steal non-top level pages from sanon regions
 * since we can't tell how many processes logically have
 * references to the page, unless the page has been swapped.
 * Once it's swapped, anyone who needs the page can swap it
 * back in.
 *
 * To keep from re-swapping in the same page from swap
 * when a parent that is forking alot gets a page that it won't
 * touch again swapped out, we don't steal if:
 * 1) page is on swap
 * 2) we own only reference count (pfdat_held_oneuser(p) is true)
 * 3) there is lots of memory
 * 4) there might be someone else that could want the page
 *    (pf_tag !- anon_tag(rp))
 */
STATIC int
do_cow_fault(
	pfd_t	*pfd,
	reg_t	*rp,
	int	refgtone,
	int	tagssame)
{
	if ((pfd->pf_flags & P_ANON) == 0 ||
		(!tagssame && (refgtone || ((rp->r_flags & RG_HASSANON) &&
					    !(pfd->pf_flags & P_SWAP)))) ||
		((pfd->pf_flags & P_CW) && refgtone) ||
		((pfd->pf_flags & P_SWAP) &&
			!refgtone &&
			GLOBAL_FREEMEM() > tune.t_gpgshi &&
			!tagssame))
		return 1;
	else
		return 0;
}

STATIC int pfaultnomem = 0;

/* Protection fault handler
 *
 * pfault is called when you try to write a "protected page"
 * a) modify bit emulation
 * b) copy on write
 * c) out of range (error)
 * d) really read only region (error)
 * e) swap association that needs breaking
 * pfault can now be called with ep of 0 from within kernel
 * to break copy on write and for a safe subyte replacement
 *
 * If rw is W_WRITE we set the pgmod bit otherwise we don't - N.B.
 * calling pfault w/o W_WRITE is dangerous if the page might be
 * written by DMA or anything.
 */

int
pas_pfault(
	pas_t		*pas,
	ppas_t		*ppas,
	as_fault_t	*as,
	as_faultres_t	*asres)
{
	reg_t		*rp;
	preg_t		*prp;
	pde_t		*pd;		/* pde for faulting page */
	pfd_t		*pfd;
	attr_t		*attr;
	vnode_t		*vp = NULL;	/* used only for setting IUPD */
	uthread_t	*ut = as->as_ut;
	pgno_t		apn;
	int		did_reservemem;
	int		tagssame;
	int		err;
	/* REFERENCED */
	size_t		psz;
#ifdef MH_R10000_SPECULATION_WAR
	int		is_uncached = 0;
#endif /* MH_R10000_SPECULATION_WAR */
	int		tlbs_need_dirty = 0;
	void		*rtval;			/* real-time value */
	uvaddr_t	vaddr = as->as_uvaddr;
	faultdata_t	faultdata;
	size_t		desired_psize;		/* Desired page size for the region */
	size_t		previous_psize = 0;
	int		large_page_fault = 0;	/* is fault for large page? */
	uvaddr_t	lpage_start_addr;	/* Starting vaddr of the large page */
	pgcnt_t		npgs = 1;		/* Number of pages to fault */
	fault_info_t	*flt_infop;		/* Pointer to fault info entry */
	fault_info_t	*start_flt_infop;	/* Pointer to first fault info entry */
	int		i;
	uvaddr_t	cur_vaddr;
	int		retry = LARGE_PAGE_FAULT_RETRY;
	int		haveexc = !(as->as_flags & ASF_NOEXC);
	int		error;
	int		skip_validate = 0, skip_modify = 0;

	VM_LOG_TSTAMP_EVENT(VM_EVENT_PFAULT_ENTRY, current_pid(), vaddr, as->as_epc, 0);

	faultdata.fd_flags = FD_GET_PAGESIZE|FD_PAGE_WAIT;
	ASSERT(ut == NULL || &ut->ut_as == ppas->ppas_utas);

 pfault_startover:
	mraccess(&pas->pas_aspacelock);

	/*
	 * Get a pointer to the region which the faulting virtual address is in.
	 */
	if ((prp = findpreg(pas, ppas, vaddr)) == NULL) {
		#pragma mips_frequency_hint	NEVER
		mrunlock(&pas->pas_aspacelock);
		asres->as_code = EFAULT;
		VM_LOG_TSTAMP_EVENT(VM_EVENT_PFAULT_EXIT, 0, 0, 0, SEXC_SEGV);
		return(SEXC_SEGV);
	}
	rp = prp->p_reg;
	reglock(rp);

	if ((pd = pmap_pte(pas, prp->p_pmap, vaddr, VM_NOSLEEP)) == NULL) {
		#pragma mips_frequency_hint	NEVER
		pfaultnomem++;
		regrele(rp);
		mrunlock(&pas->pas_aspacelock);
		setsxbrk();
		goto pfault_startover;
	}

	ASSERT(pg_getpgi(pd) != SHRD_SENTINEL);

	VM_LOG_TSTAMP_EVENT(VM_EVENT_PFAULT_RLACQ, 0, 0, 0, 0);

	/* Check to see that the pde hasn't been modified
	 * while waiting for the lock.
	 */
#if JUMP_WAR
	if (!pg_ishrdvalid(pd) && !pg_iseop(pd))
#else
	if (!pg_ishrdvalid(pd))
#endif
	{
		#pragma mips_frequency_hint	NEVER
		VM_LOG_TSTAMP_EVENT(VM_EVENT_PFAULT_NOTHV, 0, 0, 0, 0);
		if (haveexc) {
			int rv;

			/* Check if this is a read-watched page, which always
			 * has cleared hrd valid bit in pte. In this case,
			 * the tlb should still have hrd valid bit on.
			 */
			if (ut->ut_watch) {
				#pragma mips_frequency_hint NEVER
				if (intlb(ut, vaddr, B_WRITE)) {
					skip_validate = 1;
					goto pfault_next;
				}
			}
			regrele(rp);
			mrunlock(&pas->pas_aspacelock);
			ASSERT(as->as_rw == W_WRITE);
			rv = pas_vfault(pas, ppas, as, asres);
			VM_LOG_TSTAMP_EVENT(VM_EVENT_PFAULT_EXIT, 0, 0, 0, rv);
			return rv;
		} else {
			/* Do a retry */
			regrele(rp);
			mrunlock(&pas->pas_aspacelock);
			VM_LOG_TSTAMP_EVENT(VM_EVENT_PFAULT_EXIT, 0, 0, 0, 0);
			return 0;
		}
	}

 pfault_next:

	/* With shared pmaps (threads) it is possible that
	 * the page we want was already modified while we were
	 * waiting to get into the kernel.
	 * If so just tlbdropin and return.
	 */
	if (pg_ismod(pd)) {
		#pragma mips_frequency_hint	NEVER
		ASSERT(pg_isdirty(pd));
		ASSERT(pfdat_inuse(pdetopfdat(pd)));
		VM_LOG_TSTAMP_EVENT(VM_EVENT_PFAULT_ISMOD, 0, 0, 0, 0);

		/*
		 * If this page is part of a large page set
		 * the necessary variables.
		 */
		npgs = PAGE_MASK_INDEX_TO_CLICKS(pg_get_page_mask_index(pd));
		if (npgs > 1) {
			desired_psize = ctob(npgs);
			lpage_start_addr = LPAGE_ALIGN_ADDR(vaddr, desired_psize);
			pd = LARGE_PAGE_PTE_ALIGN(pd, npgs);
		}
		goto out;
	}

#ifdef MH_R10000_SPECULATION_WAR
	is_uncached = (pg_cachemode(pd) == PG_TRUEUNCACHED);
#endif /* MH_R10000_SPECULATION_WAR */

	/* Now check for:
	 * A real protection error vs.
	 * Mod bit emulation	vs.
	 * Copy on write.
	 * since pfault is only called for tlbmod exceptions,
	 * we have to check the pregion attach type for permissions.
	 * There are no protection bits in the pde's.
	 */

	/* Real protection error */
	attr = findattr(prp, vaddr);
	if ((attr->attr_prot & PROT_W) == 0) {
		#pragma mips_frequency_hint	NEVER
		regrele(rp);
		mrunlock(&pas->pas_aspacelock);
		asres->as_code = EACCES;
		VM_LOG_TSTAMP_EVENT(VM_EVENT_PFAULT_EXIT, 0, 0, 0, SEXC_SEGV);
		return(SEXC_SEGV);
	}

	/* Do store watchpoint handling here.
	 */
	if (haveexc && attr->attr_watchpt) {
		#pragma mips_frequency_hint	NEVER
		int rv, type;

		/* Its a watched page. */
		if ((rv = chkwatch(vaddr, ut, as->as_flags,
				as->as_szmem, W_WRITE, &type)) > 0) {
			/* interested address */
			regrele(rp);

			if (rv == 2)
				ascancelskipwatch(pas, ppas);
			mrunlock(&pas->pas_aspacelock);

			/* Its debugger's job to clear watchpoint. */
			asres->as_code = type;
			ASSERT(asres->as_code == FAULTEDWATCH || asres->as_code == FAULTEDSCWATCH);
			VM_LOG_TSTAMP_EVENT(VM_EVENT_PFAULT_EXIT, 0, 0, 0, SEXC_WATCH);
			return(SEXC_WATCH);
		}

		skip_modify = 1;

		/* not interested - need to skip over watch point */
		if ((attr->attr_start + NBPP) != attr->attr_end) {
			char *va;
			va = (char *)ctob(btoct(vaddr));
			attr = attr_clip(attr, va, va + NBPP);
		}

		if (skipwatch(pas, ppas, ut, vaddr, as->as_flags,
					as->as_epc, as->as_rw, attr, rp) != 0) {

			/* The executing PC cannot be stepped
			 * over - we need to get a steppable region.
			 * We know we're not executing in the
			 * system, so we should now hold NO locks.
			 */
			ASSERT(IS_KUSEG(as->as_epc));
			regrele(rp);
			mrunlock(&pas->pas_aspacelock);
			mrupdate(&pas->pas_aspacelock);
			if (vgetprivatespace(pas, ppas, as->as_epc, NULL) != 0) {
				mrunlock(&pas->pas_aspacelock);
				asres->as_code = EACCES;
				VM_LOG_TSTAMP_EVENT(VM_EVENT_PFAULT_EXIT, 0, 0, 0, SEXC_SEGV);
				return(SEXC_SEGV);
			}
			mrunlock(&pas->pas_aspacelock);

			/* getprivatespace could have dup'd the region.
			 * start all over again...
			 */
			goto pfault_startover;
		}
	}

	if (retry == LARGE_PAGE_FAULT_RETRY) {
		#pragma mips_frequency_hint	NEVER
		if (faultdata.fd_flags & FD_GET_PAGESIZE) {
			desired_psize = PAGE_MASK_INDEX_TO_SIZE(pg_get_page_mask_index(pd));
		}
	}
	else
		desired_psize = NBPP;


	npgs = btoct(desired_psize);
	if (desired_psize > NBPP) {
		#pragma mips_frequency_hint	NEVER

		int code = lpage_pfault(pas, vaddr, prp, desired_psize,
			&faultdata, &npgs);

		/*
		 * We have to retry if the return value is nonzero.
		 */

		if (code == RETRY_LOWER_PAGE_SIZE) {
			previous_psize = desired_psize;
			desired_psize = lpage_lower_pagesize(desired_psize);
			faultdata.fd_flags |= FD_PAGE_WAIT;
			faultdata.fd_flags &= ~FD_GET_PAGESIZE;
			goto pfault_startover;
		} else if (code == RETRY_FAULT) {
			goto pfault_startover;
		}

		if (npgs <= MAX_LOCAL_FAULT_ENTRIES)
			flt_infop = faultdata.fd_small_list;
		else
			flt_infop = faultdata.fd_large_list;

		/*
		 * Free up the large list if ine was allocated.
		 */
		lpage_start_addr = LPAGE_ALIGN_ADDR(vaddr, desired_psize);

		if (npgs > 1) {
			/*
			 * Reset the vfault parameters.
			 */
			large_page_fault = TRUE;
			cur_vaddr = lpage_start_addr;
		} else {
			fault_info_t	*tmp_flt_infop;

			tmp_flt_infop = faultdata.fd_large_list;
			if (tmp_flt_infop) {

				/*
				 * Transfer the contents from the allocated
				 * list to the local list so that we can free
				 * allocated list. This is done for
				 * performance. Otherwise on every pfault
				 * whether it is a large page fault or not
				 * we have to check for the allocated list.
				 */

				*flt_infop = (*(tmp_flt_infop + btoct(vaddr - lpage_start_addr)));

				kmem_free(tmp_flt_infop, btoct(desired_psize) * sizeof(fault_info_t));
			} else {
				flt_infop += btoct(vaddr - lpage_start_addr);
			}

			flt_infop->pd = pd;
			cur_vaddr = vaddr;
		}
	} else {
		/*
		 * The downgrade would have been done in lpage_pfault but if
		 * the page size is base page size we skip calling lpage_pfault
		 * for performance reasons. So do the downgrade here.
		 */
		if (previous_psize > NBPP)
			pmap_downgrade_to_base_page_size(pas, vaddr, prp, previous_psize, PMAP_TLB_FLUSH);
		flt_infop = faultdata.fd_small_list;
		flt_infop->flags = 0;
		flt_infop->pd = pd;
		cur_vaddr = vaddr;
		ASSERT(npgs == 1);
	}

	start_flt_infop = flt_infop;

	/*
	 * Loop over every pfdat of a large page and try to fault them
	 * in. If one of them fails then try to recover from the other
	 * faults and retry.
	 */

	VM_LOG_TSTAMP_EVENT(VM_EVENT_PFAULT_STARTF, 0, 0, 0, 0);
	for (i = 0; i < npgs; i++, flt_infop++, cur_vaddr += NBPP) {

		apn = vtoapn(prp, cur_vaddr);
		ASSERT(apn == rpntoapn(prp,vtorpn(prp, cur_vaddr)));
		pd = flt_infop->pd;

		VFAULT_TRACE(PFAULT_FAULT, cur_vaddr, pd, large_page_fault, flt_infop);

		MINFO.pfault++;

		/*
		 * Get pfdat and hold against migration.
		 */
		pfd = pdetopfdat_hold(pd);
		if (pfd == NULL) { /* In process of migration */
			#pragma mips_frequency_hint	NEVER
			ASSERT(npgs == 1);
			regrele(rp);
			mrunlock(&pas->pas_aspacelock);
			goto pfault_startover;
		}

		ASSERT(pfdat_inuse(pfd));
		ASSERT(pfd->pf_flags & P_DONE);

		/* Mod bit emulation.
		 * unforked bss, data and stack,
		 * and mapped files come through here -
		 * they are never copy-on-write.
		 */
		if (!(rp->r_flags & RG_CW)) {
			VM_LOG_TSTAMP_EVENT(VM_EVENT_PFAULT_NOTCW, 0, 0, 0, 0);
			/*
			 * Mapped file pages should stay hashed to vp.
			 */
			if (pfd->pf_tag == (void *)rp->r_vnode) {
				ASSERT(rp->r_type == RT_MAPFILE);
				vp = rp->r_vnode;	/* set mtime */
				ASSERT(!pfd_replicated(pfd));
				if (pfd->pf_flags & P_HOLE) {
					/*
					 * Ensure that backing store is reserved for
					 * the page being dirtied.  We take an extra
					 * ref on the pfdat so it can be referenced
					 * after the region and address space locks
					 * are dropped.  This should be OK even if
					 * this ends up being the last ref to the
					 * page, because if it's dirty then whoever
					 * steals it from us must move it to the
					 * vnode's dirty pages list.  If it's not
					 * dirty then it doesn't matter.
					 */
					ASSERT(npgs == 1);
					pageuseinc(pfd);

					/*
					 * The reference increment above also
					 * prevents migration. We can either
					 * release the pfdat here or later
					 * before exiting this block.
					 */
					pfdat_release(pfd);
					regrele(rp);
					mrunlock(&pas->pas_aspacelock);

					VOP_ALLOCSTORE(vp, pfd->pf_pageno * NBPP,
						NBPP, get_current_cred(), err);
					if (err != 0) {
						pagefree(pfd);
						asres->as_code = err;
						VM_LOG_TSTAMP_EVENT(VM_EVENT_PFAULT_EXIT, 0, 0, 0, SEXC_BUS);
						return(SEXC_BUS);
					}
					pageflags(pfd, P_HOLE, 0);
					pagefree(pfd);
					/*
					 * Now that we've done the P_HOLE
					 * processing and cleared it, start
					 * over again from the top.
					 */
					goto pfault_startover;
				}
				pageflags(pfd, P_DIRTY, 1);
			} else {
				anon_modify(rp->r_anon, pfd, 0);
			}

			MINFO.steal++;
			if (skip_modify)
				pg_setdirty(pd);
			else
				pg_setmod(pd);	/* also sets pg_dirty bit */
			pfdat_release(pfd);
			flt_infop->flags |= FLT_DONE;
			continue;
		}

		/* Copy on write page.
		 *
		 * NOTE: currently, pfault cannot handle 2 processes with
		 * 2 regions pfaulting (e.g. copy-on-write)
		 * the same region, this only happens in 2 ways:
		 * 1) a shared process forks
		 * 2) a debugger forces an dup (which makes a region cw) then
		 *	attaches to it and writes it
		 *	The only reason this case works is that the dup'ed region
		 *	is read-only
		 * Therefore we cannot ASSERT a refcnt of 1
		 */

		ASSERT(rp->r_type != RT_MAPFILE);
		ASSERT(rp->r_anon);
		VM_LOG_TSTAMP_EVENT(VM_EVENT_PFAULT_CW, 0, 0, 0, 0);

		/*
		 * check if the page has an error. If so, check if the
		 * error is a SBE, from which we can recover. 
		 * If it's a hard error we need to kill the process
		 */
		if (pfdat_iserror(pfd)) {
			#pragma mips_frequency_hint	NEVER

			if (pfdat_ishwsbe(pfd)) {
				cmn_err(CE_NOTE,
					"COW page SBE, recovered for pid %d",
					current_pid());

			} else {

				pfdat_release(pfd);
				LPAGE_FAULT_RECOVER(pas, 
						    start_flt_infop, 
						    cur_vaddr, npgs);
				regrele(rp);
				mrunlock(&pas->pas_aspacelock);
				if (cur_vaddr != vaddr) {
					retry = BASE_PAGE_FAULT_RETRY;
					goto pfault_startover;
				}

				cmn_err(CE_NOTE, 
					"COW HW page error, killed pid %d\n",
						current_pid());
				asres->as_code = EFAULT;
				VM_LOG_TSTAMP_EVENT(
						VM_EVENT_PFAULT_EXIT, 
						0, 0, 0, SEXC_BUS);
				return (SEXC_BUS);
			}
		}

		/*
		 * For private autoreserve mappings, we reserve the
		 * availsmem on demand, i.e., as each page goes anon.
		 */
		if ((rp->r_flags & RG_AUTORESRV) && (pfd->pf_flags & P_ANON) == 0) {
			#pragma mips_frequency_hint	NEVER
			did_reservemem = 1;

			if (error = pas_reservemem(pas, 1, 0)) {
				pfdat_release(pfd);
				LPAGE_FAULT_RECOVER(pas, start_flt_infop, cur_vaddr, npgs);
				regrele(rp);
				mrunlock(&pas->pas_aspacelock);

				/*
				 * For miser jobs try retrying the fault as
				 * it may go critical and get transferred to
				 * another memory pool.
				 */
				if (error == EMEMRETRY) {
					if (vm_pool_wait(GLOBAL_POOL) == 0) {
						retry = BASE_PAGE_FAULT_RETRY;
						did_reservemem = 0;
						goto pfault_startover;
					}
				}
				if (cur_vaddr != vaddr) {
					retry = BASE_PAGE_FAULT_RETRY;
					goto pfault_startover;
				}

				asres->as_code = EAGAIN;
				nomemmsg("autoreserve for mmap'ed file");
				VM_LOG_TSTAMP_EVENT(VM_EVENT_PFAULT_EXIT, 0, 0, 0, SEXC_BUS);
				return (SEXC_BUS);
			}

			rp->r_swapres++;

		} else {
			did_reservemem = 0;
		}

		tagssame = pfd->pf_tag == anon_tag(rp->r_anon);

		/*
		 * Copy or steal page?
		 */
		if (anon_modify(rp->r_anon, pfd, rp->r_flags & RG_HASSANON) == COPY_PAGE) {
			pfd_t	*pfd2;
			uint	ckey;
#ifdef TILES_TO_LPAGES
			int	vm_mvok = attr->attr_lockcnt ? 0 : VM_MVOK;
#else
#ifdef MH_R10000_SPECULATION_WAR
			int	vm_mvok = 0;
#else /* MH_R10000_SPECULATION_WAR */
#define				vm_mvok	0
#endif /* MH_R10000_SPECULATION_WAR */
#endif /* TILE_TO_LPAGES */

			/*
			 * We need a new page.
			 * Get page from a queue head after computing
			 * page cache key.
			 * Since many processes will have a private copy of
			 * a shared object (like rld's heap, libc.so's data section)
			 * and all these will tend to link them at the same virtual
			 * address, we can easily get into a situation where
			 * we exhaust a page list corresponding to these oft
			 * used virtual address. This implies we should
			 * use the region as the key, not the vnode. But for
			 * read-only stuff and to first get in data pages
			 * we would rather hash on vnode (we read in based
			 * on logical block # and vnode, it would be nice
			 * if the next invocation of the program got a clean copy
			 * at the appropriate cache block). So we switch
			 * cachekeys when a process writes - the assumption
			 * being that a process normally will either write a bunch or
			 * read a bunch or all of this doesn't really matter.
			 *
			 * One bad case still exists - suppose a page in a shared
			 * object (often used virtual address) is written. The
			 * process will vfault it in, then steal the page on
			 * the pfault - thus we will once again constantly allocate
			 * from a single page list. We partially get around this
			 * by noticing in vfault whether the fault was a write.
			 * Note that we use the apn so that stacks work.
			 */
			ckey = vcache2(prp, attr, vtoapn(prp, vaddr));
#ifdef PGLISTDEBUG
			{
				extern rpcache[];
				rpcache[rp->r_gen & 0xff]++;
				trcpglist("pf", ckey, vtoapn(prp, vaddr), rp->r_gen, vaddr);
			}
#endif /* PGLISTDEBUG */
			psz = NBPP;

			if (flt_infop->flags & FLT_PGALLOC_DONE)
				pfd2 = flt_infop->pfd;
			else if (npgs > 1) {
				/*
				 * lpage_pfault() thought that there is no
				 * need to do copy on write on this page, but
				 * now we need to do a COW. So downgrade the
				 * page and try again.
				 */
				cmn_err(CE_NOTE, "Large page has to broken down due to COW fault pte %x\n", pd);
				LPAGE_FAULT_RECOVER(pas, start_flt_infop, cur_vaddr, npgs);
				retry = BASE_PAGE_FAULT_RETRY;
				pfdat_release(pfd);
				regrele(rp);
				mrunlock(&pas->pas_aspacelock);
				goto pfault_startover;
			} else {
#ifdef USE_PTHREAD_RSA
				/* This is a temporary kludge to get the
				 * correct phyiscal color for the PRDA.
				 */
				if (vfntova(vatovfn(vaddr)) == (caddr_t)PRDAADDR)
					pfd2 = PMAT_PAGEALLOC(attr, colorof(PRDAADDR), VM_PACOLOR, &psz, vaddr);
				else
#endif /* USE_PTHREAD_RSA  */
				pfd2 = (
#ifdef _VCE_AVOIDANCE
				vce_avoidance
				? PMAT_PAGEALLOC(attr, colorof(vaddr),VM_VACOLOR|vm_mvok, &psz, vaddr)
				:
#endif /* _VCE_AVOIDANCE */
				PMAT_PAGEALLOC(attr, ckey, vm_mvok, &psz, vaddr));
			}

			if (pfd2 == 0) {
				#pragma mips_frequency_hint	NEVER

				ASSERT(npgs == 1);
				/* Not enough memory. */
 pfault_nomem:
				pfaultnomem++;
				pfdat_release(pfd);

				if (did_reservemem) {
					pas_unreservemem(pas, 1, 0);
					rp->r_swapres--;
				}

				regrele(rp);
				mrunlock(&pas->pas_aspacelock);
#ifdef MH_R10000_SPECULATION_WAR
				setsxbrk_class(vm_mvok);
#else /* MH_R10000_SPECULATION_WAR */
				setsxbrk();
#endif /* MH_R10000_SPECULATION_WAR */

				/*
				 * While we slept, current page could have been
				 * taken away by vhand or another share-group
				 * member could have come in and pfaulted the
				 * page already.  Start from the beginning.
				 */
				goto pfault_startover;
			}

			ASSERT(psz == NBPP);
#ifdef _VCE_AVOIDANCE
			if (COLOR_VALIDATION(pfd2,colorof(vaddr),0,0)) {
				#pragma mips_frequency_hint	NEVER
				pagefree(pfd2);
				flt_infop->flags &= ~FLT_PGALLOC_DONE;
				LPAGE_FAULT_RECOVER(pas, start_flt_infop,  cur_vaddr, npgs);
				retry = BASE_PAGE_FAULT_RETRY;
				pfdat_release(pfd);

				if (did_reservemem) {
					pas_unreservemem(pas, 1, 0);
					rp->r_swapres--;
				}

				regrele(rp);
				mrunlock(&pas->pas_aspacelock);
				goto pfault_startover;
			}
#endif /* _VCE_AVOIDANCE */

			/*
			 * Since a new page is being associated with the pde in
			 * question, we dis-associate the pde from this page.
			 */
			VPAG_UPDATE_RMAP_DELMAP_RET(PAS_TO_VPAGG(pas), JOBRSS_INC_RELE, pfd, pd, error);
			if (error) {
				#pragma mips_frequency_hint NEVER
				/* stolen code from pfdat_iserror case */
				LPAGE_FAULT_RECOVER(pas, start_flt_infop, cur_vaddr, npgs);
				regrele(rp);
				mrunlock(&pas->pas_aspacelock);
				miser_inform(MISER_KERN_MEMORY_OVERRUN);
				asres->as_code = EFAULT;
				return (SEXC_SEGV);
			}

			/*
			 * The code below does not require the pfdat to be
			 * released because we are using the pf_use field to
			 * keep track of the holds, in addition to the ref
			 * count, of course.  Still, migration is avoided when
			 * either the pf_use count is different than the actual
			 * number of reverse links OR when the rawcnt is
			 * non-zero.  This is in addition to the indirect way
			 * of freezing, which is acquiring the pfn lock.
			 */

			/* If the process claims to have the page locked,
			 * transfer the lock to the process's new page.
			 * This can only happen when a locked text region
			 * has been made writable.  Other cases are handled
			 * in dupreg/copy_pmap.
			 */
			if (attr->attr_lockcnt) {
				#pragma mips_frequency_hint	NEVER
				int s = pfdat_lock(pfd);
				ASSERT(pfd->pf_rawcnt > 0);
				if (pfd->pf_rawcnt > 1) {
					pfdat_unlock(pfd, s);
					if (pas_reservemem(pas, 0, 1)) {
						VPAG_UPDATE_RMAP_ADDMAP(PAS_TO_VPAGG(pas), JOBRSS_INC_RELE_UNDO, pfd, pd, attr_pm_get(attr));
						flt_infop->flags &= ~FLT_PGALLOC_DONE;
						LPAGE_FAULT_RECOVER(pas, start_flt_infop, cur_vaddr, npgs);
						retry = BASE_PAGE_FAULT_RETRY;

						/*
						 * This pagefree is on pfd2,
						 * not the frozen pfdat.
						 */
						pagefree(pfd2);
						goto pfault_nomem;
					}
					/*
					 * since we unlocked the pfdat lock - the
					 * others sharing the page could have gone
					 * so if the count is 1 then we are last
					 * user which means we inherited the reserved
					 * memory page - we now have 2 so give back one.
					 * Otherwise, there is still someone else
					 * also locking the page so they inherit the
					 * reserved status for the old page and
					 * we now have a new reserved page for our
					 * new page
					 */
					s = pfdat_lock(pfd);
					if (pfdat_dec_rawcnt(pfd) == 0) {
						pfdat_unlock(pfd, s);
						/* oops, we're the last user */
						pas_unreservemem(pas, 0, 1);
					} else {
						pfdat_unlock(pfd, s);
					}
					PFD_WAKEUP_RAWCNT(pfd);
				} else {
					pfd->pf_rawcnt = 0;
					pfdat_unlock(pfd, s);
					PFD_WAKEUP_RAWCNT(pfd);
				}
				pfd2->pf_rawcnt = 1;
			}


			/*
			 * If region is shared through prctl(ATTACHADDR ...),
			 * (possible only in case of sproc processes),
			 * ensure that all members of sharegroup will see this
			 * new page frame (pfd2). Other members may have pointers to
			 * old page frame (pfd), due their prior access to the page.
			 */
			if (rp->r_refcnt > 1) {
				#pragma mips_frequency_hint	NEVER
				/*
				 * Cleanup needs to be performed only if page
				 * reference is > 1. If another process attached, but
				 * did not reference the page, then ref count
				 * will be 1 (for this pfaulting process).
				 * Also, care only about sproc processes; if
				 * we got here as a result of debug path (prusrio)
				 * the debug path will take care of detachreg()
				 * before return from syscall ... thus there will
				 * not be any hanging references as in the 'long-lived'
				 * attach of the prctl(...) call.
				 */

				if ((pas->pas_flags & PAS_SHARED) &&
						(pfdat_held_musers(pfd)))
					clnattachaddrptes(pas, prp, vaddr, pd);
			}

			/* If this process is sharing its address space,
			 * clean all other shared processes' tlb entries
			 * for vaddr on all machines now that it is
			 * guaranteed that the region cannot be released.
			 * Clearing valid bit ensures that any references
			 * by share-group will force full vfaults.
			 * XXX tlbclean_start... tlbclean_end (bcopy enclosed)
			 */
			if ((pas->pas_flags & PAS_SHARED) &&
			    (pas->pas_refcnt != 1) &&
			    (prp->p_flags & PF_SHARED)) {
				pg_clrvalid(pd);
				#pragma mips_frequency_hint	NEVER

				/*
				 * XXX Tlbclean had better push the clones!!!
				 * XXX pflip, tossreg, setwatch, cachectl need same!
				*/
				tlbclean(pas, btoct(vaddr), pas->pas_isomask);
			} else
				tlbs_need_dirty = 1;

			/*
			 * page_copy will perform copy using pages of correct color
			 * avoiding VCEs on both source and destination. Mapping is
			 * also necessary for large memory config ( > 256 MB).
			 */
			page_copy(pfd, pfd2, colorof(vaddr), colorof(vaddr));

			if (attr->attr_prot & PROT_X) {
				#pragma mips_frequency_hint	NEVER
				/*
				 * Since the primary dcache and icache are
				 * incoherent, we need to insure that there's
				 * no stale data sitting around in any primary
				 * line that we're going to execute.  It's not
				 * a problem on an R3000 since the caches are
				 * write-through.
				 *
				 * Note: The minimum needed here for correctness
				 * is a primary data cache writeback. Actually, I think
				 * that the invalidate is needed on the secondary cache in
				 * order to cause the HW to flush the primary icache.
				 */
				_bclean_caches((void *)ctob(btoct(vaddr)), NBPP,
					       pfdattopfn(pfd2),
					       CACH_ICACHE|CACH_DCACHE|CACH_WBACK|
					       CACH_INVAL|CACH_ICACHE_COHERENCY|CACH_VADDR);
			}

			if ((pfd->pf_flags & P_CW) && tagssame)
				(void) anon_uncache(pfd);

			/*
			 * We must release the pfdat, returning the
			 * pf_rawcnt to what it would be without the
			 * extra ref to prevent migration, before
			 * calling any form of pagefree.
			 * Fortunately, we've deleted the pfdat to pte
			 * reverse link, so the remaining extra reference
			 * will prevent migration until the page is
			 * freed, or its ref count decremented.
			 * pagefree is executed with the memory object's
			 * lock taken, so there's no race when decrementing
			 * the ref count between page free and the migration
			 * procedures.
			 */
			pfdat_release(pfd);

			if (!pg_isshotdn(pd)) {

				/* If page was not shot down free it here.
				 * otherwise, it's already been freed when the
				 * page got shotdown
				 */
				if (rp->r_flags & RG_HASSANON)
					pagefreesanon(pfd, 0);
				else if ((pfd->pf_flags & P_SWAP) && !tagssame) {
					/* don't unhash - might be useful! */
					anon_pagefree_and_cache(pfd);
				} else
					pagefreeanon(pfd); /* frees and unlocks page */
			} else {
				#pragma mips_frequency_hint	NEVER
				pg_clrshotdn(pd);
			}

			/* pfd could not have been hashed to rp->r_anon
			 * and have had a referenct count > 1 since dupreg
			 * gets a new r_anon key from anon_dup.
			 * So there can't be a pinsert dup.  Ahem. We hope.
			 */
			anon_insert(rp->r_anon, pfd2, apn, P_DONE);
			COLOR_VALIDATION(pfd2,colorof(vaddr),0,0);

			/* We don't need to explicitly hold pfd2 because we have
			 * an extra reference, acquired by pagealloc, which is
			 * preventing migration until we do the rmap_addmap.
			 */
			pg_setpfn(pd, pfdattopfn(pfd2));

#if JUMP_WAR
			if (pg_iseop(pd)) {
				#pragma mips_frequency_hint	NEVER
				pg_setsftval(pd);
			} else
#endif
			{
				if (skip_validate) {
					pg_setsftval(pd);
					pg_setndref(pd);
				} else
					pg_setvalid(pd);
			}

			VPAG_UPDATE_RMAP_ADDMAP(PAS_TO_VPAGG(pas), JOBRSS_INS_PFD, pfd2, pd, attr_pm_get(attr));

			MINFO.cw++;

		} else {
			/*
			 * The page has been stolen for us.
			 */
			if (flt_infop->flags & FLT_PGALLOC_DONE) {
				#pragma mips_frequency_hint	NEVER
				LPAGE_FAULT_RECOVER(pas, start_flt_infop, cur_vaddr, npgs);
				retry = BASE_PAGE_FAULT_RETRY;
				pfdat_release(pfd);
				regrele(rp);
				mrunlock(&pas->pas_aspacelock);
				goto pfault_startover;
			}

			pfdat_release(pfd);
			MINFO.steal++;
		}

		/*
		 * Set the modifiable and dirty bits here before
		 * the region is unlocked. We do this only if the 'rw'
		 * flag says W_WRITE.
		 */
		/* If called from lockpages2 PF_LCKNPROG will be set.
		 * Test added to fix PV 677990 but a more correct way may exist.
		 */
		if ((haveexc || (prp->p_flags & PF_LCKNPROG)) && (as->as_rw & W_WRITE)) {
			if (skip_modify)
				pg_setdirty(pd);
			else
				pg_setmod(pd);
			flt_infop->flags |= FLT_DONE;
#if DEBUG
			/*
			 * Since migration might be happening, we have to
			 * get pfn lock in pde in order to get the correct
			 * pfdat.
			 */
			pg_pfnacquire(pd);
			ASSERT((pdetopfdat(pd)->pf_flags & (P_HASH|P_ANON)) == (P_HASH|P_ANON));
			pg_pfnrelease(pd);
#endif /* DEBUG */
		}
	} /* For npgs */


#ifdef  VFAULT_TRACE_ENABLE
	if (pg_get_page_mask_index(pd))
		large_page_fault = 1;
#endif

	/*
	 * Set the page mask field of all the ptes of the large page.
	 */
	if (npgs > 1) {
		flt_infop = start_flt_infop;
#ifdef	DEBUG
		for (i = 0; i < npgs; i++, flt_infop++) {
			pd = flt_infop->pd;
			ASSERT(pg_get_page_mask_index(pd) == PAGE_SIZE_TO_MASK_INDEX(desired_psize));
		}
#endif
		pd = start_flt_infop->pd;

		if (faultdata.fd_large_list)
			kmem_free(start_flt_infop, npgs * sizeof(fault_info_t));
	}

 out:

	/*
	 * On MP systems, must avoid stale TLB entries.
	 * Purge via tlbdirty.  Must do this AFTER any
	 * call that might sleep and switch to another
	 * processor after setting the new pfn and before
	 * the tlbdropin.
	 * If tlbclean has already purged tlb entries,
	 * no need to do this via tlbdirty.
	 */
	if (IS_MP && tlbs_need_dirty) {
		rtval = rt_pin_thread();
		tlbdirty(ppas->ppas_utas);
	}

	/*
	 * this is the end for in-kernel COW processing - the rest of
	 * the code pertains only to threads that are writing
	 */
	if (!haveexc) {
		#pragma mips_frequency_hint	NEVER
		if (IS_MP && tlbs_need_dirty)
			rt_unpin_thread(rtval);
		regrele(rp);
		mrunlock(&pas->pas_aspacelock);
		VM_LOG_TSTAMP_EVENT(VM_EVENT_PFAULT_EXIT, 0, 0, 0, 0);
		return 0;
	}

	if (prp->p_flags & PF_FAULT) {
		#pragma mips_frequency_hint	NEVER
		prfaultlog(pas, ppas, prp, vaddr, W_WRITE);
	}

	if (npgs > 1) {
		#pragma mips_frequency_hint	NEVER
		LPG_FLT_TRACE(VFAULT_LPAGE_TLBDROPIN, vaddr, pd, desired_psize, 0);
		ASSERT(check_lpage_pte_validity(pd));
		INC_LPG_STAT(private.p_nodeid, PAGE_SIZE_TO_INDEX(desired_psize), LPG_PFAULT_SUCCESS);
		lpage_tlbdropin(pas, ppas, vaddr, desired_psize, pd);
	} else {
		pte_t   tmp_pte = pd->pte;

		pg_sethrdvalid((pde_t *)&tmp_pte);	/* skip_validate */
		if (as->as_rw & W_WRITE)
			pg_setmod((pde_t *)&tmp_pte);
		VFAULT_TRACE(VFAULT_NBPP_TLBDROPIN, vaddr, pd, NBPP, start_flt_infop);
		ASSERT(check_lpage_pte_validity(pd));
		tlbdropin(ut->ut_as.utas_tlbpid, vaddr, tmp_pte);
	}
	if (IS_MP && tlbs_need_dirty)
		rt_unpin_thread(rtval);

	regrele(rp);
	mrunlock(&pas->pas_aspacelock);

	/*
	 * If vp is non-null, set MOD and CHG times
	 */
	if (vp) {
		struct vattr va;
		/* REFERENCED */
		int rv;

		va.va_mask = AT_UPDMTIME | AT_UPDCTIME;
		VOP_SETATTR(vp, &va, ATTR_LAZY, sys_cred, rv);
	}
	VM_LOG_TSTAMP_EVENT(VM_EVENT_PFAULT_EXIT, 0, 0, 0, 0);
	return(0);
}

STATIC int vfaultagain = 0;
STATIC int vfaultnomem = 0;

/* Translation fault handler
 * vfault is called when the v bit is not set
 * a) reference detection
 * b) demand zero
 * c) demand from file or swap
 * c) illegal reference
 */

int
pas_vfault(
	pas_t		*pas,
	ppas_t		*ppas,
	as_fault_t	*as,
	as_faultres_t	*asres)
{
	pde_t		*pd;
	uthread_t	*ut = as->as_ut;
	pfd_t		*pfd;
	reg_t		*rp;
	preg_t		*prp;
	attr_t		*attr;
	struct pm	*pmp;
	pgno_t		wait_sentry;
	vnode_t		*vp;
	int		count;
	uvaddr_t	tmp_va;
	sm_swaphandle_t	onswap;
	pglst_t		swaplst[1];
	pgno_t		rpn, apn;
	off_t		fileoff;
	off_t		fsize;
	void		*id;
	int		retval = 0;
	int		error = 0;
	int		s;
	struct vattr	va;
	uio_t		auio;
	static int	wait_sentry_hand = WAIT_SENTRY_LO;
	iovec_t		aiov;
	int		update_atime = 0;
	int		try_again_delay = 1;
#define MAX_RETRY_DELAY 32
	/* REFERENCED */
	size_t		psz;
#ifdef MH_R10000_SPECULATION_WAR
	int		is_uncached = 0;
#endif /* MH_R10000_SPECULATION_WAR */
	faultdata_t	faultdata;		/* Fault info entry list */
	size_t		desired_psize;		/* Desired page size for the region */
	size_t		previous_psize = 0;
	int		large_page_fault;	/* is fault for large page? */
	uvaddr_t	lpage_start_addr;	/* Starting vaddr of the large page */
	pgcnt_t		npgs;			/* Number of pages to fault */
	fault_info_t	*flt_infop;		/* Pointer to fault info entry */
	fault_info_t	*start_flt_infop;	/* Pointer to first fault info entry */
	int		i;
	int		rw = as->as_rw;
	int		retry = LARGE_PAGE_FAULT_RETRY;
	uvaddr_t	vaddr = as->as_uvaddr;
	uvaddr_t	cur_vaddr = vaddr;	/* Current vaddr, loop variable */
	int		skip_validate = 0;
	vnode_t		*locked_vp = NULL;

	VM_LOG_TSTAMP_EVENT(VM_EVENT_VFAULT_ENTRY, current_pid(), vaddr, rw, as->as_epc);

	faultdata.fd_flags = FD_GET_PAGESIZE|FD_PAGE_WAIT;

    for ( ; ; ) { /* begin improper/non-standard indent */

	/* If process has been bad and is over its RSS, *and*
	 * we are beyond limits of rsshogs - slow down process.
	 * We still really need to let it go a bit -- if it
	 * locks down a lot of pages (up to RSS.cur) then attempts
	 * to (say) do a physio - we could deadlock trying to get
	 * pages we can never get.
	 * Skip this check if the fault occurred in kernel mode.
	 * Refer bug 325771 for more details.
	 * If we sent a SIG kill and the process is in kernel
	 * mode we will never be able to recover and crash
	 * in k_trap.
	 */
	if ((GLOBAL_FREEMEM() < rsskicklim) && (as->as_flags & ASF_FROMUSER)) {
		#pragma mips_frequency_hint	NEVER
		count = 10;
		while ((GLOBAL_FREEMEM() < rsskicklim) &&
		       ((pas->pas_rss + ppas->ppas_rss) >
			(pas->pas_maxrss + tune.t_rsshogslop)) &&
		       --count >= 0) {

			s = mp_mutex_spinlock(&vhandwaitlock);
			rsswaitcnt++;
			mp_sv_wait(&rsswaitsema, PZERO|TIMER_SHIFT(AS_MEM_WAIT), &vhandwaitlock, s);
			if (pas->pas_flags & PAS_NOSWAP) {
				if (locked_vp) {
					VOP_RWUNLOCK(locked_vp, VRWLOCK_READ);
					locked_vp = NULL;
				}
				asres->as_code = ENOSPC;
				VM_LOG_TSTAMP_EVENT(VM_EVENT_VFAULT_EXIT, 0, 0, 0, SEXC_KILL);
				return(SEXC_KILL);
			}
		}
	}

	mraccess(&pas->pas_aspacelock);

#if NO_WIRED_SEGMENTS || FAST_LOCORE_TFAULT
	/*
	 * When running with NO_WIRED_SEGMENTS (at least on TFP) we
	 * no longer enter tfault.  Instead we have the locore segment
	 * handling code dropin an invalid tlb entry, and come directly
	 * to vfault.  This means we might not enable segment table
	 * operation, so we do it here.
	 * NOTE: We do this holding aspacelock, just like tfault would.
	 */
	if (!(ut->ut_as.utas_segflags))
		initsegtbl(&ut->ut_as, pas, ppas);
#endif /* NO_WIRED_SEGMENTS */

	/*
	 * Look up the region that faulted -- we'll lock it later.
	 * The region can't change while we have aspacelock.
	 */
	if ((prp = findpreg(pas, ppas, vaddr)) == NULL) {
		#pragma mips_frequency_hint	NEVER

		/* See if we faulted on the PRDA.  If so,
		 * create one now and start over.
		 */
		if (vfntova(vatovfn(vaddr)) == (caddr_t) PRDAADDR) {
			mrunlock(&pas->pas_aspacelock);
			mrupdate(&pas->pas_aspacelock);
			if (setup_prda(pas, ppas) == 0) {
				mrunlock(&pas->pas_aspacelock);
				continue;
			} else {
				cmn_err(CE_WARN, "Couldn't create prda for %s (pid %d)",
					get_current_name(), current_pid());
			}
		}

		asres->as_code = EFAULT;
		mrunlock(&pas->pas_aspacelock);
		if (locked_vp) {
			VOP_RWUNLOCK(locked_vp, VRWLOCK_READ);
			locked_vp = NULL;
		}
		VM_LOG_TSTAMP_EVENT(VM_EVENT_VFAULT_EXIT, 0, 0, 0, SEXC_SEGV);
		return(SEXC_SEGV);
	}

	rpn = vtorpn(prp, vaddr);
	apn = rpntoapn(prp, rpn);
	rp = prp->p_reg;
	vp = rp->r_vnode;
	if (locked_vp && (locked_vp != vp)) {
		VOP_RWUNLOCK(locked_vp, VRWLOCK_READ);
		locked_vp = NULL;
	}

#ifdef R10000_SPECULATION_WAR
	/*
	 * Check to see if this fault is on a locked down page by
	 * user code.  If so, put the processes to sleep to avoid
	 * speculating to the DMA buffer.  If mpin_sleep_if_locked
	 * sleeps it releases the pas apacelock.
	 */
	if (rp->r_flags & RG_LOCKED) {
		#pragma mips_frequency_hint	NEVER
		if (as->as_flags & ASF_FROMUSER) {
			int rc;
			rc = mpin_sleep_if_locked(pas,ppas,prp,vaddr);
			if (rc == 1) {	/* woke-up, try vfault again */
				vfaultagain++;
				continue;
			}
			else if (rc != 0) {	/* interrupted */
				if (locked_vp) {
					VOP_RWUNLOCK(locked_vp, VRWLOCK_READ);
					locked_vp = NULL;
				}
				return 0;
			}
		}
#if DEBUG
		else
			cmn_err(CE_WARN, "vfault: kern touched IP28 war page!\n");
#endif
	}
#endif	/* R10000_SPECULATION_WAR */

	reglock(rp);

	VM_LOG_TSTAMP_EVENT(VM_EVENT_VFAULT_RLACQ, 0, 0, 0, 0);

	attr = findattr(prp, vaddr);
	if ((attr->attr_prot & PROT_R) == 0) {
		#pragma mips_frequency_hint	NEVER
		error = EACCES;
		goto sexc_segv;
	}
#ifdef MH_R10000_SPECULATION_WAR
	is_uncached = (attr->attr_cc == (PG_TRUEUNCACHED >> PG_CACHSHFT));
#endif /* MH_R10000_SPECULATION_WAR */

	/* Find the physical address of the faulting pte.
	 */
	if ((pd = pmap_pte(pas, prp->p_pmap, vaddr, VM_NOSLEEP)) == NULL) {
		#pragma mips_frequency_hint	NEVER
		vfaultnomem++;
		regrele(rp);
		mrunlock(&pas->pas_aspacelock);
		if (locked_vp) {
			VOP_RWUNLOCK(locked_vp, VRWLOCK_READ);
			locked_vp = NULL;
		}
		setsxbrk();
		continue;
	}


	if (faultdata.fd_flags & FD_GET_PAGESIZE)
		desired_psize = PMAT_GET_PAGESIZE(attr);

	/*
	 * Check to see if this address needs to fault in a
	 * large page. If so, verify that the address range is
	 * ok and that the page size is the same over the address
	 * range. If retry attempt does not want to try a large page
	 * then skip it.
	 */
	if ((desired_psize > NBPP) && (retry == LARGE_PAGE_FAULT_RETRY)) {
		#pragma mips_frequency_hint	NEVER
		INC_LPG_STAT(private.p_nodeid, PAGE_SIZE_TO_INDEX(desired_psize), LPG_VFAULT_ATT);
		desired_psize = lpage_validate(pas, vaddr, attr, prp, desired_psize);
	} else
		desired_psize = NBPP;

	if (desired_psize > NBPP)
		large_page_fault = TRUE;
	else
		large_page_fault = FALSE;

	/*
	 * Initialize the flt_info structures early.
	 */
	faultdata.fd_large_list = NULL;
	start_flt_infop = faultdata.fd_small_list;
	start_flt_infop->flags = 0;
	start_flt_infop->pd = pd;
	cur_vaddr = vaddr;
	npgs = 1;

	ASSERT(poff(rp->r_fileoff) == 0);
	fileoff = ctob(rpn) + rp->r_fileoff;
#if !TFP
	ASSERT(pd->pgi != SHRD_SENTINEL);
#endif
	if (rp->r_flags & RG_PHYS) {
		#pragma mips_frequency_hint	NEVER
		vhandl_t vt;

		if (pg_isvalid(pd)) {
			/* all done */
			start_flt_infop->flags |= FLT_DONE;
			break;
		}
		vt.v_preg = prp;
		vt.v_addr = prp->p_regva;
		ASSERT(rp->r_fault);
		/* XXX how much locking?? */
		regrele(rp);
		mrunlock(&pas->pas_aspacelock);
		if (rp->r_fault(&vt, rp->r_fltarg, vaddr, rw)) {
			if (locked_vp) {
				VOP_RWUNLOCK(locked_vp, VRWLOCK_READ);
				locked_vp = NULL;
			}
			asres->as_code = EFAULT;
			VM_LOG_TSTAMP_EVENT(VM_EVENT_VFAULT_EXIT, 0, 0, 0, SEXC_SEGV);
			return(SEXC_SEGV);
		}
		/* since unlocked - re-look up etc. */
		continue;
	}

	/*
	 * Watchpoint handling
	 */
	if (attr->attr_watchpt) {
		#pragma mips_frequency_hint	NEVER
		int type;
		if ((count = chkwatch(vaddr, ut, as->as_flags,
				as->as_szmem, rw, &type)) > 0) {
			/* interested address */
			regrele(rp);

			if (count == 2)
				ascancelskipwatch(pas, ppas);
			mrunlock(&pas->pas_aspacelock);
			if (locked_vp) {
				VOP_RWUNLOCK(locked_vp, VRWLOCK_READ);
				locked_vp = NULL;
			}

			/* Its debugger's job to clear watchpoint. */
			VM_LOG_TSTAMP_EVENT(VM_EVENT_VFAULT_EXIT, 0, 0, 0, SEXC_WATCH);
			asres->as_code = type;
			ASSERT(asres->as_code == FAULTEDWATCH || asres->as_code == FAULTEDSCWATCH);
			return(SEXC_WATCH);
		}

		skip_validate = 1;

		if ((attr->attr_start + NBPP) != attr->attr_end) {
			tmp_va = (char *)ctob(btoct(vaddr));
			attr = attr_clip(attr, tmp_va, tmp_va + NBPP);
		}

		if (skipwatch(pas, ppas, ut, vaddr, as->as_flags,
				as->as_epc, rw, attr, rp) != 0) {
			/*
			 * The executing PC cannot be stepped over --
			 * we need to get a steppable region.
			 * We know we're not executing in the
			 * system, so we should now hold NO locks.
			 */
			ASSERT(IS_KUSEG(as->as_epc));
			regrele(rp);
			mrunlock(&pas->pas_aspacelock);
			mrupdate(&pas->pas_aspacelock);
			if (vgetprivatespace(pas, ppas, as->as_epc, NULL) != 0) {
				mrunlock(&pas->pas_aspacelock);
				if (locked_vp) {
					VOP_RWUNLOCK(locked_vp, VRWLOCK_READ);
					locked_vp = NULL;
				}
				asres->as_code = EACCES;
				VM_LOG_TSTAMP_EVENT(VM_EVENT_VFAULT_EXIT, 0, 0, 0, SEXC_SEGV);
				return(SEXC_SEGV);
			}
			mrunlock(&pas->pas_aspacelock);

			/* getprivatespace could have dup'd the region.
			 * start all over again...
			 */
			continue;
		}
	}


	/* If the pfn is set, then another process is currently
	 * reading in this page.  So we wait for it to go P_DONE.
	 * By checking here, we can unlock the region across the
	 * lengthy read, thus allowing more than one process sharing
	 * a region to fault in pages simultaneously as well
	 * as up vhand's odds for getting the region lock.
	 */
	if (!pg_isvalid(pd)) {
		pg_pfnacquire(pd);
		wait_sentry = pg_getpfn(pd);
		if (wait_sentry != 0) {
			#pragma mips_frequency_hint	NEVER
			VFAULT_TRACE(VFAULT_WAIT_SENTRY, vaddr, prp, wait_sentry, 0);
			if (wait_sentry <= PG_SENTRY) {
				if (wait_sentry >= WAIT_SENTRY_LO &&
				    wait_sentry <= WAIT_SENTRY_HI) {
					pg_setwaiter(pd);
					pg_pfnrelease(pd);
					mrunlock(&pas->pas_aspacelock);
					/* ghostpagewait releases region */
					ghostpagewait(rp, wait_sentry);
					vfaultagain++;
					continue;
				}
				/* Should only get wait sentries here --
				 * to get a shrd sentinel here would be
				 * an error.
				 */
				ASSERT(0);
			}
			/* SPT */
			ASSERT(pmap_spt(prp->p_pmap) || prp->p_nvalid > 0);

			/* Page can go P_BAD if someone is msync'ing or
			 * truncating the file.  They'll just come along and
			 * invalidate this page later.  This is not a problem
			 * since it's just the usual truncate/reference race.
			 */

			pfd = pdetopfdat(pd);
			if (!(pfd->pf_flags & P_DONE)) {

				/* Bump reference so page doesn't go away.
				 */
				pageuseinc(pfd);

				/* The extra reference above is
				 * preventing migratiom.
				 */
				pg_pfnrelease(pd);
	wait_for_pfd:
				regrele(rp);
				mrunlock(&pas->pas_aspacelock);
				pagewait(pfd);
				pagefreeanon(pfd);

				/* since we unlocked region anything might have
				 * happened, so, we start over...
				 */
				vfaultagain++;
				continue;
			}

			/* NOTE that the page may not be valid - if the code
			 * below has marked P_DONE but hasn't yet validated it.
			 * We proceed to drop the pd in and return - we'll
			 * simply get a utlbmiss..
			 * This seems better than forcing the reglock below
			 * to be done early
			 */
#if JUMP_WAR
			/*
			 * XXX I don't understand the above comment, why can't
			 * XXX I set the valid here since I've still got the
			 * XXX region locked???
			 */

			if (attr->attr_prot & PROT_X) {
				/*
				 * Before we make a text page available,
				 * we have to look for the "jump-at-end-of-page"
				 * sequence, and take recovery action.
				 */
				if (end_of_page_bug(pd,vaddr)) {

					/*
					 * Generate an End-Of-Page exception.
					 */
					retval = SEXC_EOP;
				}
			}
#endif
			/*
			 * I don't know why we break out of the loop
			 * here without setting the valid bit. So
			 * if it is a large page downgrade the pte
			 * and drop it in.
			 */

			pg_pfnrelease(pd);

			/*
			 * We found the page and it is done
			 * Do a retry in the case of a large page.
			 * For a 16k page it just drops an invalid tlb
			 * entry.
			 */

			if (large_page_fault) {
				regrele(rp);
				mrunlock(&pas->pas_aspacelock);
				vfaultagain++;
				continue;
			} else {
				regrele(rp);
				mrunlock(&pas->pas_aspacelock);
				vfaultagain++;
				continue;
			}
		}
		pg_pfnrelease(pd);
	}
 vfault_next:

	if (large_page_fault) {
		#pragma mips_frequency_hint	NEVER
		int	ret;

		lpage_start_addr = LPAGE_ALIGN_ADDR(vaddr, desired_psize);

		npgs = btoc(desired_psize);

		ret = lpage_vfault(pas, ut, vaddr, prp, &npgs, &faultdata, &start_flt_infop);
		/*
		 * We have to retry if the return value is nonzero.
		 */
		if (ret == RETRY_LOWER_PAGE_SIZE) {
			previous_psize = desired_psize;
			desired_psize = lpage_lower_pagesize(desired_psize);
			faultdata.fd_flags |= FD_PAGE_WAIT;
			faultdata.fd_flags &= ~FD_GET_PAGESIZE;

			vfaultagain++;
			continue;
		} else if (ret == RETRY_FAULT) {
			vfaultagain++;
			continue;
		}

		if (npgs > 1) {
			/*
			 * Reset the vfault parameters.
			 */
			cur_vaddr = lpage_start_addr;
			rpn = vtorpn(prp, lpage_start_addr);
			apn = rpntoapn(prp, rpn);
		} else {
			/*
			 * Cannot fault a large page
			 */
			start_flt_infop->pd = pd;
			cur_vaddr = vaddr;
			ASSERT(npgs == 1);
			large_page_fault = 0;
		}
	} else {
		/*
		 * The downgrade would have been done in
		 * lpage_validate but if the page size is
		 * base page size we skip calling
		 * lpage_validate for performance reasons. So
		 * do the downgrade here.
		 */
		if (previous_psize > NBPP)
			pmap_downgrade_to_base_page_size(pas, vaddr, prp, previous_psize, PMAP_TLB_FLUSH);
	}

	flt_infop = start_flt_infop;

	/*
	 * Loop over every pfdat of a large page and try to fault them
	 * in. If one of them fails then try to recover from the other
	 * faults and retry.
	 */

	retry = NO_RETRY; /* Assume no retries to begin with */

	for (i = 0; i < npgs; i++, flt_infop++, rpn++, cur_vaddr += NBPP) {

		apn = rpntoapn(prp, rpn);

		if (attr->attr_end < cur_vaddr)
			attr = attr->attr_next;

		VFAULT_TRACE(VFAULT_FAULT, cur_vaddr, prp, large_page_fault, flt_infop);

		pd = flt_infop->pd;

		/* Check that the page has not been read in by
		 * another process (sharing the same page map)
		 * while we were waiting for it on the reglock above.
		 */
		if (pg_ishrdvalid(pd)) {
			flt_infop->flags |= FLT_DONE;
			continue; /* Go to next page */
		}

		/*
		 * Check for reference bit fault.
		 */
		if ((pfd = pdetopfdat_hold(pd)) && pg_isvalid(pd)) {
		#pragma mips_frequency_hint	NEVER
			VFAULT_TRACE(VFAULT_REFBIT, cur_vaddr, pd, large_page_fault, flt_infop);
#ifdef _MEM_PARITY_WAR
			/* If the page is P_BAD and not hashed, then
			 * it has seen a parity error in some other
			 * process.  We should just break our reference
			 * and refetch the page.  This can happen for
			 * any clean page, whether a mapped file page
			 * or an anonymous page which is a clean copy
			 * of a page on swap.
			 */
			if (!(pfd->pf_flags & P_HASH) && (pfd->pf_flags & P_BAD)) {
				/*
				 * We need to take exactly same
				 * steps as in the case of mappedfile
				 * getting truncated. i.e. we need
				 * to check if page is locked and
				 * send a signal if so otherwise
				 * toss the page etc..
				 * This code is right below this
				 * place. (under p_tupe == PT_MAPFILE)
				 * So, Just jump _INTO_
				 * the middle of an 'if' condition..
				 * as has been done so often in
				 * the vfault code!!.
				 */
				ASSERT(prp->p_nvalid > 0);
				goto handle_bad_page;
			}
#endif /* _MEM_PARITY_WAR */

			/* If its a mapped file page
			 * and the page isn't hashed, it must have been
			 * ``stolen'' from the page cache by a truncate or
			 * (nfs) attrcache invalidation operation.  If the
			 * page isn't locked, just toss it for the user.
			 *
			 * Otherwise, send SEGV and let user deal with it.
			 */
			if (prp->p_type == PT_MAPFILE) {
				if (!(pfd->pf_flags & P_HASH)) {
#ifdef	_MEM_PARITY_WAR
	handle_bad_page:
#endif
					if (pfd->pf_rawcnt) {
						pfdat_release(pfd);
						LPAGE_FAULT_RECOVER(pas, start_flt_infop, cur_vaddr, npgs);
						if (cur_vaddr == vaddr) {
							error = EBUSY;
							goto sexc_segv;
						} else {
							regrele(rp);
							mrunlock(&pas->pas_aspacelock);
							vfaultagain++;
							retry = BASE_PAGE_FAULT_RETRY;
							break;
						}
					} else {
						VPAG_UPDATE_RMAP_DELMAP(PAS_TO_VPAGG(pas), JOBRSS_DECREASE, pfd, pd);
						pfdat_release(pfd);
						if (!pg_isshotdn(pd))
							pagefree(pfd);

						/* pg_clrshotdn() is not
						 * called since pg_setpgi
						 * will clear entire pte.
						 */
						ASSERT(prp->p_nvalid>0);
						/* SPT */
						PREG_NVALID_DEC(prp, 1);
						pg_clrpgi(pd);
						LPAGE_FAULT_RECOVER(pas, start_flt_infop, cur_vaddr, npgs);
						goto vfault_next;
					}
				}
			}

			MINFO.rfault++;
#if JUMP_WAR
			if (pg_iseop(pd)) {
				retval = SEXC_EOP;
				flt_infop->flags |= FLT_DONE;
			} else
#endif
			{
#ifdef _VCE_AVOIDANCE
				if (PREG_COLOR_VALIDATION(prp,pfd,cur_vaddr,
						as->as_flags & ASF_FROMUSER)) {
					pfdat_release(pfd);
					LPAGE_FAULT_RECOVER(pas, start_flt_infop, cur_vaddr, npgs);
					regrele(rp);
					mrunlock(&pas->pas_aspacelock);
					retry = LARGE_PAGE_FAULT_RETRY;
					break; /* try again */
				}
#endif /* _VCE_AVOIDANCE */

				/* The valid bit is set on the pte
				 * and the end of the loop just
				 * before tlbdropin.
				 */
				flt_infop->flags |= FLT_DONE;

				/* pg_setvalid(pd); */
			}

			pfdat_release(pfd);
			continue; /* Go to next page */
		}

		/* If the page in the pte is migrated, it
		 * pdetopfdat_hold() will return NULL. Retry
		 * the operation.
		 */
		if ((pfd == NULL) && (pg_ismigrating(pd))) {
			#pragma mips_frequency_hint	NEVER
			regrele(rp);
			mrunlock(&pas->pas_aspacelock);
			vfaultagain++;
			retry = BASE_PAGE_FAULT_RETRY;
			break;
		}


		ASSERT(!pg_ismigrating(pd));
		MINFO.vfault++;
		ut->ut_acct.ua_vfaults++;

		/* Get the anonymous memory manager id for this page.
		 * Region lock guarantees that the id won't change.
		 */
		if (rp->r_anon) {

			/* check to see if this page is not reserved */
			if (isunreserved(rp, apn)) {
				LPAGE_FAULT_RECOVER(pas, start_flt_infop, cur_vaddr, npgs);
				if (cur_vaddr == vaddr) {
					error = EAGAIN;
					goto sexc_bus;
				} else {
					regrele(rp);
					mrunlock(&pas->pas_aspacelock);
					vfaultagain++;
					retry = BASE_PAGE_FAULT_RETRY;
					break;
				}
			}

			/* reusing count to save stack space */
			count = 0;

			/* If the anonymous page isn't in the page cache,
			 * it might be on swap.  If so, a page will be
			 * allocated for it and inserted in the cache.
			 */
			if (flt_infop->flags & FLT_PFIND) {
				pfd = flt_infop->pfd;
				id = flt_infop->anon_handle;
				onswap = flt_infop->swap_handle;
			} else {
 lookup_again:
				pfd = anon_pfind(rp->r_anon, apn, &id, &onswap);
			}

			VFAULT_TRACE(VFAULT_ANON, pfd, id, onswap, ((pfd) ? (pfd->pf_flags &P_DONE) : 0));

		       if (pfd && (!(flt_infop->flags & FLT_PGALLOC_DONE))) {

				if (!(pfd->pf_flags & P_DONE)) {
					/* cannot happen for large page faults */
					ASSERT(npgs == 1);
					goto wait_for_pfd;
				}

				/*
				 * This pfdat is now frozen because pfind
				 * incremented the ref count.  However, as soon 
				 * as we add the new map, the page may migrate.
				 * Therefore, we need to explicitely hold the
				 * pfdat to prevent migration until we're done
				 * with setting things up.  (handlepd case)
				 */
				pfdat_hold(pfd);

				/*
				 * Get rid of extra reference to the page.
				 */
				VPAG_UPDATE_RMAP_ADDMAP_RET(PAS_TO_VPAGG(pas), JOBRSS_INC_FOR_PFD, pfd, pd, attr_pm_get(attr), error);
				if (error) {
					error = ENOMEM;
					pfdat_release(pfd);
					if (npgs > 1)
						LPAGE_FAULT_RECOVER(pas, start_flt_infop, cur_vaddr, npgs);
					else
						pagefree(pfd);
					miser_inform(MISER_KERN_MEMORY_OVERRUN);
					goto sexc_segv;
				}

				MINFO.cache++;
				atomicAddLong(&ut->ut_pproxy->prxy_ru.ru_minflt, 1);

#ifdef _VCE_AVOIDANCE
				while (PREG_COLOR_VALIDATION(prp, pfd,cur_vaddr,
						as->as_flags & ASF_FROMUSER))
					;
#endif /* _VCE_AVOIDANCE */
				/* Insert in page table */
				pg_setpfn(pd, pfdattopfn(pfd));

				prp->p_nvalid++;
				++ppas->ppas_rss;

				ASSERT(!pg_isvalid(pd));

				/* We MUST setup CC bits before V bit, otherwise
				 * a shaddr process faulting on same page may
				 * have utlbmiss dropin a partially valid pte.
				 */
				pg_setccuc(pd, attr->attr_cc, attr->attr_uc);
				flt_infop->flags |= FLT_DONE;

				ASSERT(error == 0);
				error = handlepd(cur_vaddr, pfd, pd, prp, 1);
				if (error) {
					/*
					 * errors from handlepd override any other
					 */
					LPAGE_FAULT_RECOVER(pas, start_flt_infop, cur_vaddr, npgs);
					pfdat_release(pfd);
					retval = error;
					error = 0;
					retry = NO_RETRY;
					break;
				}

				if (npgs > 1) {
					pg_set_page_mask_index(pd, PAGE_SIZE_TO_MASK_INDEX(desired_psize));
				}
				pfdat_release(pfd);
				continue; /* Go to next page */
			}

			if (id == (void *)NULL) {
				goto try_demand_fill;
			}

			/* If anon memory manager claimed that the
			 * page was known to it, but it wasn't on
			 * swap, why couldn't we find it in the page
			 * cache?  Ans: the page was paged out (under
			 * the auspices of another region), freed
			 * and recycled between the anon_lookup and
			 * the pfind (holy interrupts, batman!).
			 * This can happen at most once since vhand
			 * will set the swap handle before freeing the
			 * page.  Otherwise a bug has occurred and we
			 * kill the process rather than go into an
			 * infinite lookup loop.
			 */

			if (!onswap) {
			    if (count) {
				cmn_err(CE_WARN, "second try at anon pfind failed, vaddr 0x%x", cur_vaddr);
				error = ENOENT;
#ifdef DEBUG_DOUBLE_PFIND
				debug(0);
#endif
				goto sexc_kill;
			    }

			    count++;
			    goto lookup_again;
			}
		} else
			onswap = NULL;

		/* Allocate a page now for the faulting address,
		 * before the region is released.
		 * If the data is to be read off of swap, or the
		 * page is demand-fill or demand-zero,
		 * it'll be used.
		 * If the page is to be replaced by the file page,
		 * the local page will be used to keep other
		 * threads in abeyance (just outside Portland).
		 */

		/* Only bother looking if anon_id said that
		 * the page was on swap.  Region lock ensures
		 * that onswap state won't change.
		 */
		if (onswap) {
			#pragma mips_frequency_hint	NEVER

			/* We will not come here and swap a large page. */
			ASSERT(npgs == btoct(NBPP));

#ifdef PGLISTDEBUG
			{
				extern rpcache[];
				rpcache[rp->r_gen & 0xff]++;
				trcpglist("v4", vcache2(prp, attr, apn), apn, rp->r_gen, cur_vaddr);
			}
#endif

			psz = NBPP;

			if ((pfd = (
#ifdef _VCE_AVOIDANCE
				   vce_avoidance
				   ? PMAT_PAGEALLOC(attr,
					    colorof(cur_vaddr),
					    VM_VACOLOR | VM_MVOK | VM_DMA_RD,
					    &psz, cur_vaddr)
				   :
#endif /* _VCE_AVOIDANCE */
				   PMAT_PAGEALLOC(attr, vcache2(prp, attr, apn),
#ifdef MH_R10000_SPECULATION_WAR
					VM_MVOK | VM_DMA_RD,
#else /* MH_R10000_SPECULATION_WAR */
					VM_MVOK,
#endif /* MH_R10000_SPECULATION_WAR */
					&psz, cur_vaddr))
			) == NULL) {
				/*
				 * No memory! - Release everything,
				 * go SXBRK, and start over.
				 */
				regrele(rp);
				vfaultnomem++;
				mrunlock(&pas->pas_aspacelock);
				if (locked_vp) {
					VOP_RWUNLOCK(locked_vp, VRWLOCK_READ);
					locked_vp = NULL;
				}
#ifdef MH_R10000_SPECULATION_WAR
				setsxbrk_class(VM_VACOLOR | VM_MVOK | VM_DMA_RD);
#else /* MH_R10000_SPECULATION_WAR */
				setsxbrk();
#endif /* MH_R10000_SPECULATION_WAR */
				retry = LARGE_PAGE_FAULT_RETRY;
				break;
			}

			ASSERT(psz == NBPP);

			if (anon_swapin(id, pfd, apn) == DUPLICATE) {
				pagefree(pfd);
				goto lookup_again;
			}

			COLOR_VALIDATION(pfd,colorof(cur_vaddr),0,0);

			if (pd->pte.pg_pfn && !pg_isvalid(pd))
				debug("ring");		/* SPT_DEBUG */

			/* Insert in page table */

			/*
			 * We need to explicitly hold the pfdat here,
			 * even though we already have an extra reference
			 * preventing migration, because we still need it
			 * frozen after we do the rmap_addmap.
			 * (handlepd case).
			 */
			pfdat_hold(pfd);
			pg_setpfn(pd, pfdattopfn(pfd));

			prp->p_nvalid++;

			/*
			 * Note that bumping rss here is correct but
			 * doesn't handle the case of share group members.
			 * The absolutely correct RSS is calculated in clock
			 * (if you're running) and vhand (when it steals pages).
			 */
			++ppas->ppas_rss;

			atomicAddLong(&ut->ut_pproxy->prxy_ru.ru_majflt, 1);
			MINFO.swap++;

			/*
			 * In case we are debugging the process, we go thru the
			 * VEC_tlbmiss()-soft_trap-trap()-dbMaybeCancelFault()
			 * stuff else we just return from VEC_tlbmiss.
			 */
			if (as->as_flags & ASF_PTRACED)
				retval = SEXC_PAGEIN;

			regrele(rp);

			swaplst[0].gp_pfd = pfd;
			if (sm_swap(swaplst, onswap, 1, B_READ, NULL)) {
				reglock(rp);
				/*
				 * pfdat released, but migration
				 * still disabled because of extra reference
				 * acquired by pagealloc.
				 */
				pfdat_release(pfd);
				killpage(prp, pd);
				error = EIO;
				goto sexc_kill;
			}

			reglock(rp);

			VFAULT_TRACE(VFAULT_SWAPIN, pfd, rp->r_anon, pd, flt_infop);

			/* Allocated page is guaranteed to be
			 * d- and i-cache clean, so there is
			 * no need to flush either cache here.
			 * For machines with writeback caches,
			 * we wrote back the dcache after bringing
			 * the page in.
			 */

			/* Mark the I/O done in the pfdat and
			 * awaken anyone who is waiting for it.
			 */
			pagedone(pfd);

			/* could have changed while region was unlocked
			 */
			attr = findattr(prp, cur_vaddr);
			VPAG_UPDATE_RMAP_ADDMAP_RET(PAS_TO_VPAGG(pas), JOBRSS_INC_FOR_PFD, pfd, pd, attr_pm_get(attr), error);
			if (error) {
				pfdat_release(pfd);
				killpage(prp, pd);
				error = ENOMEM;
				miser_inform(MISER_KERN_MEMORY_OVERRUN);
				goto sexc_segv;
			}

			ASSERT(!pg_isvalid(pd));
			ASSERT(pd->pte.pg_pfn != 0);	/* SPT_DEBUG */

			/* We MUST setup CC bits before V bit, otherwise
			 * a shaddr process faulting on same page may
			 * have utlbmiss dropin a partially valid pte.
			 */
			pg_setccuc(pd, attr->attr_cc, attr->attr_uc);
			flt_infop->flags |= FLT_DONE;

			ASSERT(error == 0);
			error = handlepd(vaddr, pfd, pd, prp, 1);
			if (error) {
				/*
				 * errors from handlepd override any other
				 */
				LPAGE_FAULT_RECOVER(pas, start_flt_infop, cur_vaddr, npgs);
				pfdat_release(pfd);
				retval = error;
				error = 0;
				retry = NO_RETRY;
				break;
			}
			pg_check_isfetchop_fault(pd, pfd, 0);
			pfdat_release(pfd);

			continue; /* Go to next page */
		}

 try_demand_fill:
		/* If there's no primary store (inode), page
		 * must be demand zero or demand fill.
		 * If page is valid region address (vtopreg says so)
		 * but is beyond r_maxfsize, must also be demand fill.
		 */
		if (!rp->r_vnode || fileoff >= rp->r_maxfsize) {
 demand_fill:
			/*
			 * availsmem is reserved for each anon page in
			 * private autoreserve regions as the pages go anon
			 */
			VM_LOG_TSTAMP_EVENT(VM_EVENT_VFAULT_DFILLSTART, 0, 0, 0, 0);

			if (rp->r_flags & RG_AUTORESRV) {
				if (error = pas_reservemem(pas, 1, 0)) {
					LPAGE_FAULT_RECOVER(pas, start_flt_infop, cur_vaddr, npgs);
					if (error == EMEMRETRY) {
						regrele(rp);
						mrunlock(&pas->pas_aspacelock);
						if (vm_pool_wait(GLOBAL_POOL) == 0) {
							cmn_err(CE_NOTE,"Retrying Autoreserve for batch jobs\n");
							vfaultagain++;
							retry = BASE_PAGE_FAULT_RETRY;
							break;
						}
						mraccess(&pas->pas_aspacelock);
						reglock(rp);
						error = EAGAIN;
						goto sexc_bus;
					}
					if (cur_vaddr == vaddr) {
						nomemmsg("autoreserve for mmap file");
						error = EAGAIN;
						goto sexc_bus;
					} else {
						regrele(rp);
						mrunlock(&pas->pas_aspacelock);
						vfaultagain++;
						retry = BASE_PAGE_FAULT_RETRY;
						break;
					}
				}

				rp->r_swapres++;
			}

			MINFO.demand++;

			/*
			 * These pages are always ANON - allocate
			 * them using the region pointer as the cache key
			 */
#ifdef PGLISTDEBUG
			{
				extern rpcache[];
				rpcache[rp->r_gen & 0xff]++;
				trcpglist("v1", vcache2(prp, attr, apn), apn, rp->r_gen, cur_vaddr);
			}
#endif
			/*
			 * If a pagelloc was already done, use
			 * that page.
			 */

			psz = NBPP;
#ifdef USE_PTHREAD_RSA
			/* This is a temporary kludge to get the
			 * correct phyiscal color for the PRDA.
			 */
			if (!(flt_infop->flags & FLT_PGALLOC_DONE) &&
			    (vfntova(vatovfn(vaddr)) == (caddr_t)PRDAADDR))
				pfd = (PMAT_PAGEALLOC(attr, colorof(PRDAADDR), VM_PACOLOR,
						&psz, (caddr_t)PRDAADDR));
			else
#endif /* USE_PTHREAD_RSA  */
			if (flt_infop->flags & FLT_PGALLOC_DONE)
				pfd = flt_infop->pfd;
			else pfd = (
#ifdef _VCE_AVOIDANCE
				vce_avoidance
				? PMAT_PAGEALLOC(attr, colorof(cur_vaddr), VM_VACOLOR|VM_MVOK,
						&psz, cur_vaddr)
				:
#endif
				PMAT_PAGEALLOC(attr, vcache2(prp, attr, apn), VM_MVOK,
						&psz, cur_vaddr));

			if (pfd == NULL) {
				#pragma mips_frequency_hint	NEVER
				/*
				 * No memory! - Release everything,
				 * go SXBRK, and start over.
				 */
				if (rp->r_flags & RG_AUTORESRV) {
					pas_unreservemem(pas, 1, 0);
					rp->r_swapres--;
				}

				regrele(rp);
				vfaultnomem++;
				mrunlock(&pas->pas_aspacelock);
				if (locked_vp) {
					VOP_RWUNLOCK(locked_vp, VRWLOCK_READ);
					locked_vp = NULL;
				}
				setsxbrk();
				retry = LARGE_PAGE_FAULT_RETRY;
				break;
			}

			if (VPAG_UPDATE_VM_RSS(PAS_TO_VPAGG(pas), JOBRSS_INC_BLIND, 0, 1)) {
				error = ENOMEM;
				LPAGE_FAULT_RECOVER(pas, start_flt_infop, cur_vaddr, npgs);
				miser_inform(MISER_KERN_MEMORY_OVERRUN);
				goto sexc_segv;
			}

			ASSERT(psz == NBPP);

			COLOR_VALIDATION(pfd,colorof(cur_vaddr),0,0);
			VFAULT_TRACE(VFAULT_DFILL, pfd, rp->r_anon, pd, flt_infop);

			/* Insert in page table */

			/*
			 * The extra reference acquired by pagealloc
			 * is sufficient here to keep the pfdat frozen.
			 * We don't need to explicitly hold it.
			 */
			pg_setpfn(pd,  pfdattopfn(pfd));

			prp->p_nvalid++;

			VM_LOG_TSTAMP_EVENT(VM_EVENT_VFAULT_DFILLEND, 0, 0, 0, 0);

			/*
			 * Note that bumping rss here is correct but
			 * doesn't handle the case of share group members.
			 * The absolutely correct RSS is calculated in clock
			 * (if you're running) and vhand when it steals pages.
			 */
			++ppas->ppas_rss;

			/* there exist anonymous page(s)...*/
			ASSERT(rp->r_anon);

			VM_LOG_TSTAMP_EVENT(VM_EVENT_VFAULT_ANONINS, 0, 0, 0, 0);
			if (npgs > 1) {
				anon_insert(rp->r_anon, pfd, apn, P_DONE);
				pg_set_page_mask_index(pd, PAGE_SIZE_TO_MASK_INDEX(desired_psize));
				if (!(rp->r_flags & RG_DFILL)) {
					page_zero(pfd, colorof(cur_vaddr), 0, NBPP);

					if (prp->p_type == PT_PRDA) {
						map_prda(pfd,cur_vaddr);
					}
				}
			} else {
				anon_insert(rp->r_anon, pfd, apn, 0);
				if (!(rp->r_flags & RG_DFILL)) {
					regrele(rp);
#ifdef SN0
					if (dbe_recovery) {
						page_zero_nofault(pfd, colorof(cur_vaddr), 0, NBPP);
					} else
#endif
					page_zero(pfd, colorof(cur_vaddr), 0, NBPP);
					reglock(rp);

#ifdef SN0
					if (dbe_recovery && pfdat_ishwbad(pfd)) {
					/*
					 * An error occurred during the 
					 * page_zero, undo connection to current
					 * page and retry.
					 */
#ifdef DEBUG
					   cmn_err(CE_NOTE,
					   "Error during vfault page alloc\n");
					   cmn_err(CE_NOTE,
					   "error on page, pfd = %x\n", pfd);
#endif /* DEBUG */
					   anon_pagebad(pfd);
					   pg_setpfn(pd, 0);
					   if (flt_infop->flags &
					       FLT_PGALLOC_DONE) {
						flt_infop->pfd = NULL;
						flt_infop->flags &=
						  !FLT_PGALLOC_DONE;
					   }
					   pagedone(pfd);
					   regrele(rp);
					   mrunlock(&pas->pas_aspacelock);
					   retry = BASE_PAGE_FAULT_RETRY;
					   break;
					}
#endif

					if (prp->p_type == PT_PRDA) {
						map_prda(pfd,cur_vaddr);
					}
				}
				pagedone(pfd);
			}

			/*
			 * could have changed while region was unlocked
			 */
			attr = findattr(prp, cur_vaddr);

			/*
			 * We MUST setup CC bits before V bit --
			 * otherwise a shaddr process faulting on
			 * the same page may have utlbmiss dropin
			 * a partially valid pte.  Valid bit will
			 * be set at the end of the for loop.
			 */
			pg_setccuc(pd, attr->attr_cc, attr->attr_uc);

			/*
			 * If a write request do pfault handling also.
			 * Simply turn on mod and dirty pte bits.
			 */
			if ((attr->attr_prot & PROT_W) && !attr->attr_watchpt) {
				#pragma mips_frequency_hint	NEVER
				MINFO.steal++;
				pg_setmod(pd);
			}

			VM_LOG_TSTAMP_EVENT(VM_EVENT_VFAULT_ADDMAP_START, 0, 0, 0, 0);

			VPAG_UPDATE_RMAP_ADDMAP(PAS_TO_VPAGG(pas), JOBRSS_INS_PFD, pfd, pd, attr_pm_get(attr));

			flt_infop->flags |= FLT_DONE;

			VM_LOG_TSTAMP_EVENT(VM_EVENT_VFAULT_ADDMAP_END, 0, 0, 0, 0);
			ASSERT(pd->pte.pg_pfn);		/* SPT_DEBUG */

			/* see if this is a fethcop page and if so update */
			pg_check_isfetchop_fault(pd, pfd, 1);

			continue; /* Go to next page */
		}

		/*
		 * use vn_pfind to get the page. vn_pfind knows about the
		 * replication mechanism, and invokes it appropriately.
		 */
		if (flt_infop->flags & FLT_PFIND)
			pfd = flt_infop->pfd;
		else if (flt_infop->flags & FLT_DO_FILE_IO)
			pfd = NULL; /* Force a file I/O */
		else {
 fastfind:
			/*
			 * Always fall into the VOP_READ path
			 * when the VREMAPPING bit is set.  This
			 * keeps us from grabbing references to
			 * pages in between calls to remapf() and
			 * vnode_flushinval_pages() or
			 * vnode_tosspages().
			 */
			if (rp->r_vnode->v_flag & VREMAPPING) {
				#pragma mips_frequency_hint	NEVER
				pfd = NULL;
			} else {
				pfd = vn_pfind(rp->r_vnode,
					rpn + offtoct(rp->r_fileoff), VM_ATTACH,
					(void *)attr_pm_get(attr));
			}
		}

		VFAULT_TRACE(VFAULT_VNODE, pfd, rp->r_vnode, rpn, flt_infop);

		if (pfd) {
			int rv;

			if ((pfd->pf_flags&(P_DONE|P_BAD)) != P_DONE) {

				/* XXX if regcnt == 1 and not shaddr,
				 * XXX wait closer to home, retry...
				 * XXX There is a race condition where 1) page
				 * XXX found in pcache, 2) then another thread marked
				 * XXX it BAD & DONE.
				 */
				ASSERT(npgs == 1);
				goto wait_for_pfd;
			}

			/* If mapping a partial last page,
			 * make sure the part beyond EOF is zeroed.
			 * Could also get hashed preallocated block(s)
			 * of an autogrow mapping.
			 */

			/* XXX With vnodes, what's to protect from
			 * XXX another process extending the object
			 * XXX just as we're zeroing it?
			 */

			va.va_mask = AT_SIZE;
			VOP_GETATTR(vp, &va, ATTR_LAZY, get_current_cred(), rv);
			if (!rv && fileoff + NBPP > va.va_size) {

				off_t cnt = fileoff + NBPP - va.va_size;

				/*
				 * If cnt is a page or more and this isn't
				 * an autogrow region, then it means the
				 * file has been truncated out from under us.
				 * This can happen with NFS, for example.
				 * Give the process a SIGBUS in this case.
				 */

				if (cnt >= NBPP && (rp->r_flags & RG_AUTOGROW) == 0) {
					if (flt_infop->flags & FLT_PFIND)
						LPAGE_FAULT_RECOVER(pas, start_flt_infop, cur_vaddr, npgs);
					else
						pagefree(pfd);
					if (cur_vaddr == vaddr) {
						error = EFAULT;
						goto sexc_bus;
					} else {
						regrele(rp);
						mrunlock(&pas->pas_aspacelock);
						vfaultagain++;
						retry = BASE_PAGE_FAULT_RETRY;
						break;
					}
				}

				ASSERT(cnt < NBPP || rp->r_flags & RG_AUTOGROW);

				if (rp->r_type == RT_MAPFILE &&
				    rp->r_flags & RG_AUTOGROW &&
				    va.va_size < rp->r_maxfsize) {
					if (flt_infop->flags & FLT_PFIND)
						LPAGE_FAULT_RECOVER(pas, start_flt_infop, cur_vaddr, npgs);
					else
						pagefree(pfd);
					if (locked_vp) {
						VOP_RWUNLOCK(locked_vp, VRWLOCK_READ);
						locked_vp = NULL;
					}
					goto try_autogrow;
				}

				if (locked_vp) {
					cnt = MIN(NBPP, cnt);
					page_zero(pfd, colorof(cur_vaddr), NBPP-cnt, cnt);
				} else {
					if (flt_infop->flags & FLT_PFIND)
						LPAGE_FAULT_RECOVER(pas, start_flt_infop, cur_vaddr, npgs);
					else
						pagefree(pfd);
					regrele(rp);
					mrunlock(&pas->pas_aspacelock);
					vfaultagain++;
					locked_vp = vp;
					VOP_RWLOCK(locked_vp, VRWLOCK_READ);
					retry = BASE_PAGE_FAULT_RETRY;
					break;
				}
			}
			if (locked_vp) {
				VOP_RWUNLOCK(locked_vp, VRWLOCK_READ);
				locked_vp = NULL;
			}

			/*
			 * Can't lock the vnode while holding the reglock,
			 * so remember to update access time when we're done.
			 */
			update_atime = 1;

			MINFO.cache++;
			atomicAddLong(&ut->ut_pproxy->prxy_ru.ru_minflt, 1);

#ifdef _VCE_AVOIDANCE
			while (PREG_COLOR_VALIDATION(prp, pfd,cur_vaddr,
					(as->as_flags & ASF_FROMUSER)))
				;
#endif /* _VCE_AVOIDANCE */
			/*
			 * This is another case where we need the pfdat
			 * frozen even after doing the rmap_addmap.
			 * Therefore, we explicitly hold it.
			 * (handlepd case).
			 */
			pfdat_hold(pfd);

			VPAG_UPDATE_RMAP_ADDMAP_RET(PAS_TO_VPAGG(pas), JOBRSS_INC_FOR_PFD, pfd, pd, attr_pm_get(attr), error);
			if (error) {
				error = ENOMEM;
				pfdat_release(pfd);
				if (npgs > 1)
					LPAGE_FAULT_RECOVER(pas, start_flt_infop, cur_vaddr, npgs);
				else
					pagefree(pfd);
				miser_inform(MISER_KERN_MEMORY_OVERRUN);
				goto sexc_segv;
			}

			/* Insert cached page in page table. */

			pg_setpfn(pd,  pfdattopfn(pfd));

			if (pg_isshotdn(pd)){

				/* If page was shotdown, then the page has
				 * already been accounted to this region.
				 * So, dont bump the number of valid pages.
				 * Shotdn bit gets set if the page mapped by
				 * this pte got shot down as it was a replica.
				 */
				pg_clrshotdn(pd);
			} else {
				prp->p_nvalid++;
			}

			/* We MUST setup CC bits before V bit, otherwise
			 * a shaddr process faulting on same page may
			 * have utlbmiss dropin a partially valid pte.
			 */
			pg_setccuc(pd, attr->attr_cc, attr->attr_uc);

			flt_infop->flags |= FLT_DONE;

			ASSERT(error == 0);
			error = handlepd(cur_vaddr, pfd, pd, prp, 1);

			if (error) {
				LPAGE_FAULT_RECOVER(pas, start_flt_infop, cur_vaddr, npgs);
				pfdat_release(pfd);
				retval = error;
				error = 0;
				retry = NO_RETRY;
				break;
			}

			if (npgs > 1) {
				pg_set_page_mask_index(pd, PAGE_SIZE_TO_MASK_INDEX(desired_psize));
			}

			pfdat_release(pfd);
			continue; /* Go to next page */
		}
#ifdef _R5000_CVT_WAR
		/*
		 * If this page is backed by an mtext vnode, check to see whether it
		 * is known to be clean (i.e., free of problem cvt instructions).
		 * If it is known to be clean, mtext_map_real_vnode() will create
		 * a new region_t with the real vnode backing it.
		 *
		 * REVIEW: Make sure not leaking any references to pfdats, etc.
		 */

		if (R5000_cvt_war && IS_MTEXT_VNODE(vp) && mtext_offset_is_clean(vp, fileoff)) {
			#pragma mips_frequency_hint	NEVER
			/*
			 * page is clean; create a substitute region referencing
			 * the underlying vnode.
			 */
			regrele(rp);
			mrunlock(&pas->pas_aspacelock);
			if (locked_vp) {
				VOP_RWUNLOCK(locked_vp, VRWLOCK_READ);
				locked_vp = NULL;
			}
			error = mtext_map_real_vnode(vp, pas, ppas, vaddr);
			if (error) {
				asres->as_code = error;
				return SEXC_BUS;
			}
			asres->as_code = 0;
			/*
			 * REVIEW: What to do here?  We want to force another
			 * fault so that the real vnode can be found...
			 */
			return 0;
		}
#endif /* _R5000_CVT_WAR */

		/* Try to read from file.
		 * A successful VOP_READ will insert the page
		 * into the page cache, which we will find later.
		 *
		 * For now, stick a ghost pfn into pd and mark
		 * it for waiters.
		 *
		 * At this point, the region nvalid count has
		 * been bumped, but the valid bit for the page
		 * is OFF: thus vhand will not touch it.
		 * The page table will not get swapped out when
		 * we release the region since pg_pfn != 0.
		 */
		ASSERT(npgs == 1);
		wait_sentry = wait_sentry_hand++;
		if (wait_sentry > WAIT_SENTRY_HI)
			wait_sentry = wait_sentry_hand = WAIT_SENTRY_LO;

		ASSERT((pg_getpfn(pd) == 0 && !pg_iswaiter(pd)) ||
		       (pg_getpgi(pd) == SHRD_SENTINEL));

		/*
		 * Migration can't happen on this pte.
		 * It has no associated pfdat!
		 */
		pg_setpfn(pd, wait_sentry);

		/*
		 * Before we drop the region lock, get a reference
		 * to the Policy Module structure in the attributes
		 * (if there is one) so we can pass it in to the
		 * file system and thus the chunk/page cache.
		 */
		pmp = attr_pm_get(attr);
		if (pmp != NULL) {
			pmo_incref(pmp, pmp);
		}

		regrele(rp);
		atomicAddLong(&ut->ut_pproxy->prxy_ru.ru_majflt, 1);

		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = fileoff;
		auio.uio_segflg = UIO_NOSPACE;
		aiov.iov_base = (caddr_t) 0xdeadbeefL;
		auio.uio_resid = aiov.iov_len = NBPP;
		auio.uio_pmp = pmp;
		auio.uio_pio = 0;
		auio.uio_readiolog = 0;
		auio.uio_writeiolog = 0;
		auio.uio_pbuf = 0;
		VOP_READ(vp, &auio,
			 (curuthread && (IS_THREADED(curuthread->ut_pproxy))) ? IO_MTTHREAD : 0,
			 get_current_cred(), &ut->ut_flid, error);
#ifdef DEBUG
		if (error && (error != EAGAIN))
			printf("Bad page read pid %d [%s] vp 0x%x resid %d error %d\n",
				current_pid(), get_current_name(), vp,
				auio.uio_resid, error);
#endif
		/*
		 * Now drop the reference to the Policy Module
		 * structure we took above.
		 */
		if (pmp != NULL) {
			pmo_decref(pmp, pmp);
		}

		reglock(rp);
		attr = findattr(prp, vaddr); /* could have changed */

		/*
		 * Still not possible to migrate
		 */
		ASSERT(wait_sentry == pg_getpfn(pd));
		pg_setpfn(pd, 0);

		if (pg_iswaiter(pd)) {
			pg_clrwaiter(pd);
			ghostpagedone(wait_sentry);
		}

		/* If less than NBPP was read, decide whether
		 *  1)	the file should be grown; or
		 *  2)	the page should be zero-filled; or
		 *  3)	there was an error reading (network problem
		 *	or file truncation).
		 */
		if (!error && auio.uio_resid > 0) {
			#pragma mips_frequency_hint	NEVER

			/* fsize is number of bytes actually read */
			ASSERT(NBPP >= auio.uio_resid);

			fsize = NBPP - auio.uio_resid;

			/* XXX For now, we'll chuck the autogrow
			 * XXX semantics that say segv on load,
			 * XXX grow on store.
			 */
			if ((rp->r_flags & RG_AUTOGROW) && (fileoff + fsize < rp->r_maxfsize)) {

				/* Don't grow mapped private.
				 * Just give it a page of zeros.
				 */
				ASSERT(prp->p_type == PT_MAPFILE);
				if (rp->r_type != RT_MAPFILE) {
					if (fsize == 0)
						goto demand_fill;
					else
						goto trace;
				}

				if (as->as_flags & ASF_PTRACED)
					retval = SEXC_PAGEIN;
 try_autogrow:
				/* Check here for general write
				 * permission.  Attr struct will
				 * protect individual page faults.
				 */
				if (!(prp->p_maxprots & PROT_W)) {
					error = ENXIO;
					goto sexc_segv;
				}

				/* Grow the file through this page.
				 */
				fsize = MIN(fileoff + NBPP, rp->r_maxfsize);
				regrele(rp);
				if (error = autogrow(rp->r_vnode, fsize, ut)) {
					reglock(rp);
					goto sexc_segv;
				}

				mrunlock(&pas->pas_aspacelock);

				/* region released, so all bets off */
				retry = LARGE_PAGE_FAULT_RETRY;
				break;
			}
		}

		/* If we got an EAGAIN error from the VOP_READ call
		 * it means that the filesystem detected a possible
		 * deadlock. In this case, drop all locks and retry
		 * the pagefault.
		 */
		if (error == EAGAIN) {
			regrele(rp);
			mrunlock(&pas->pas_aspacelock);
			retry = LARGE_PAGE_FAULT_RETRY;
			error = 0;
			delay(try_again_delay);
			if (try_again_delay < MAX_RETRY_DELAY)
				try_again_delay <<= 1;
			break;
		}

		/* If there was an error or we're past EOF and we didn't
		 * autogrow above, then give up now.  If we were able to
		 * read anything at all, then we'll find it when we
		 * loop-the-loop and do the pfind again.
		 */
		if (error) {
			/* There is no page to kill
			 * (and Mohammed is his prophet).
			 */
			goto sexc_bus;
		}

		if (auio.uio_resid == NBPP) {	/* hit EOF */
			error = EFAULT;
			goto sexc_bus;
		}

	trace:
		/*
		 * In case we are debugging the process, we go thru the
		 * VEC_tlbmiss()-soft_trap-trap()-dbMaybeCancelFault()
		 * stuff else we just return from VEC_tlbmiss.
		 */
		if (as->as_flags & ASF_PTRACED)
			retval = SEXC_PAGEIN;

		MINFO.file++;

		/* At this point, the page should be in the page cache.
		 * We unlocked the region, but the ghost pfn inserted in
		 * pd has kept all processes in abeyance (outside Portland).
		 * Thus it is safe to just jump to the pfind code.
		 */
		if (PMAT_GET_PAGESIZE(attr) > NBPP) {
			regrele(rp);
			mrunlock(&pas->pas_aspacelock);
			retry = LARGE_PAGE_FAULT_RETRY;
			break;
		}

		goto fastfind;
	} /* For npgs */

	/*
	 * If retry is false break out of the loop, else do another
	 * attempt.
	 */

	if ((retry != LARGE_PAGE_FAULT_RETRY) &&
	    (retry != BASE_PAGE_FAULT_RETRY))
			break;

    } /* end improper/non-standard indent */

	if (locked_vp) {
		VOP_RWUNLOCK(locked_vp, VRWLOCK_READ);
		locked_vp = NULL;
	}

	pg_setccuc(pd, attr->attr_cc, attr->attr_uc);

	VM_LOG_TSTAMP_EVENT(VM_EVENT_VFAULT_DROPIN, 0, 0, 0, 0);

	/*
	 * XXX While it might be faster on some machines to skip
	 * XXX the tlbdropin here and let utlbmiss handle it, there
	 * XXX is at least one weird case where that would not be
	 * XXX sufficient--  sproc process with private mappings
	 * XXX forks.  The private page of page tables will not
	 * XXX have the shared-sentinel markers to defer to the
	 * XXX shared pmap, and will always miss in utlbmiss any
	 * XXX shared mappings in the same segment.
	 */
#if JUMP_WAR
	ASSERT((retval != SEXC_EOP) || (pg_iseop(pd)));
	if (retval != SEXC_EOP) {
#endif
	/*
	 * Set the valid bit of all the ptes of the large page.
	 */
	flt_infop = start_flt_infop;
	for (i = 0; i < npgs; i++, flt_infop++) {
		ASSERT(flt_infop->flags & FLT_DONE);
		pd = flt_infop->pd;
		if (skip_validate) {
			pg_setsftval(pd);
			pg_setndref(pd);
		} else
			pg_setvalid(pd);
	}

	if (npgs > 1) {
		#pragma mips_frequency_hint	NEVER
		VFAULT_TRACE(VFAULT_LPAGE_TLBDROPIN, vaddr, pd, desired_psize, start_flt_infop);
		ASSERT(check_lpage_pte_validity(start_flt_infop->pd));

		INC_LPG_STAT(private.p_nodeid, PAGE_SIZE_TO_INDEX(desired_psize), LPG_VFAULT_SUCCESS);

		lpage_tlbdropin(pas, ppas, vaddr, desired_psize, start_flt_infop->pd);
		if (faultdata.fd_large_list)
			kmem_free(faultdata.fd_large_list, npgs * sizeof(fault_info_t));
	} else {
		int	stlbpid;
		utas_t	*utas = &ut->ut_as;
		pte_t	tmp_pte = pd->pte;

		pg_sethrdvalid((pde_t *)&tmp_pte);   /*skip_validate*/
		stlbpid = splhi();
		VFAULT_TRACE(VFAULT_NBPP_TLBDROPIN, vaddr, pd, NBPP, start_flt_infop);

		/* Check previous contents of tlb entry (return value)
		 * and if it's a shared sentinel, then switch tlbmiss
		 * handlers since this process has overlapping shared
		 * and private segments.
		 */
		ASSERT(check_lpage_pte_validity(pd));
		if (tlbdropin(utas->utas_tlbpid, vaddr, tmp_pte) == SHRD_SENTINEL) {
			/*
			 * Force a change in the utlbmiss handler the
			 * current CPU is using, but only if needed.
			 * Note that each process which requires the shared
			 * handler may come here TWICE on an R4000 since
			 * the tlb entry pair initially dropped in from the
			 * non-shared utlbmiss handler could have contained
			 * a pair of SHRD_SENTINEL entries.
			 * NOTE: Current code in swtch does not handle this
			 *       since we don't necessarily return to the
			 *       user code through swtch from here.
			 */
			if (!(utas->utas_utlbmissswtch & UTLBMISS_DEFER)) {

				/*
				 * Delayed large page tlbmiss switch.
				 * Make sure the tlbs are flushed for this
				 * process alone.
				 * XXX need lock??
				 */
				if (utas->utas_flag & UTAS_LPG_TLBMISS_SWTCH) {
					utas->utas_flag &= ~UTAS_LPG_TLBMISS_SWTCH;
					flushtlb_current();
				}

				s = utas_lock(utas);
				utas->utas_utlbmissswtch |= UTLBMISS_DEFER;
				utas->utas_flag |= UTAS_TLBMISS;
				utas_unlock(utas, s);
				utlbmiss_resume(utas);
			} else {
				ASSERT(utas->utas_flag & UTAS_TLBMISS);
				ASSERT(private.p_utlbmissswtch & UTLBMISS_DEFER);
			}
		}
		splx(stlbpid);
	}
#ifdef JUMP_WAR
	}
#endif

	if (prp->p_flags & PF_FAULT) {
		prfaultlog(pas, ppas, prp, vaddr, rw);
	}
	regrele(rp);

	/*
	 * If future locking is enabled for the process, lock down the
	 * region up through the faulted page.
	 */
	if ((pas->pas_lockdown & AS_FUTURELOCK) &&
	    !((prp->p_flags & PF_LCKNPROG) || (rp->r_flags & RG_PHYS))) {
		#pragma mips_frequency_hint	NEVER
		pgno_t npgs = btoc(vaddr - prp->p_regva + 1);
		ASSERT(npgs > 0);
		if (lockpages2(pas, ppas, prp->p_regva, npgs, LPF_VFAULT)) {
			asres->as_code = ENOMEM;
			retval = SEXC_SEGV;
		}
	}

	/*
	 * If vhand has stolen pages from p's pmap, see if we can trim the map.
	 * try_pmap_trim resets trimtime on success.
	 * NOTE when returns, we may have aspacelock for update
	 */
	if (prp->p_pmap->pmap_trimtime)
		try_pmap_trim(pas, ppas);

	mrunlock(&pas->pas_aspacelock);

	if (update_atime) {
		#pragma mips_frequency_hint	NEVER
		/* REFERENCED */
		int rv;

		va.va_mask = AT_UPDATIME;
		VOP_SETATTR(vp, &va, ATTR_LAZY, sys_cred, rv);
	}
	VM_LOG_TSTAMP_EVENT(VM_EVENT_VFAULT_EXIT, 0, 0, 0, retval);

	ASSERT(error == 0);
	return(retval);

 sexc_kill:
	retval = SEXC_KILL;
	goto bad;
 sexc_segv:
	retval = SEXC_SEGV;
	goto bad;
 sexc_bus:
	retval = SEXC_BUS;
	goto bad;
 bad:
	ASSERT(cur_vaddr == vaddr);
	ASSERT(error);
	asres->as_code = error;
	regrele(rp);
	mrunlock(&pas->pas_aspacelock);
	if (locked_vp) {
		VOP_RWUNLOCK(locked_vp, VRWLOCK_READ);
		locked_vp = NULL;
	}
	VM_LOG_TSTAMP_EVENT(VM_EVENT_VFAULT_EXIT, 0, 0, 0, SEXC_BUS);
	return(retval);
}

/*
 * do cache/R4K WAR handling
 * if val == 0 do not validate page at all
 *	     1 set sftval
 *	     2 set valid
 */
/*ARGSUSED*/
int
handlepd(
	caddr_t	vaddr,
	pfd_t	*pfd,
	pde_t	*pd,
	preg_t	*prp,
	int	val)
{
	int	retval = 0;

#if R4000 || JUMP_WAR || R10000
	int	executable;

	/*
	 * Check if we could execute this page someday.
	 */
	if (findattr(prp, vaddr)->attr_prot & PROT_X)
		executable = 1;
	else
		executable = 0;
#endif

	ASSERT(pfdat_is_held(pfd));

#if JUMP_WAR
	/*
	 * Before we make a text page available,
	 * we have to look for the "jump-at-end-of-page"
	 * sequence, and take recovery action.
	 */
	if (executable && end_of_page_bug(pd,vaddr)) {
		/* Generate an End-Of-Page exception.  */
		retval = SEXC_EOP;
		if (val)
			pg_setsftval(pd);
	} else
#endif
		if (val == 2)
			pg_setvalid(pd);
		else if (val == 1)
			pg_setsftval(pd);

#if R4000 || R10000
	/*
	 * Since the primary dcache and icache are
	 * incoherent, we need to insure that there's
	 * no stale data sitting around in any primary
	 * line that we're going to execute.  It's not
	 * a problem on an R3000 since the caches are
	 * write-through. Not a problem on TFP since the
	 * primary dcache is write-through.
	 *
	 * Note: The minimum needed here for correctness
	 * is a primary data cache writeback.  As long
	 * as we're at it, we go ahead and do a full
	 * writeback-invalidate and avoid the VCEIs that
	 * would otherwise occur.
	 *
	 * NOTE: We check for PT_LIBDAT because the static
	 * shared libraries execute code out of the data
	 * section and it's only marked PROT_RW.
	 */
	if (executable && (pfd->pf_flags & P_CACHESTALE)) {
#if defined(_VCE_AVOIDANCE) && defined(DEBUG)
		if (vce_avoidance) {
			ASSERT(pfd_to_vcolor(pfd) == colorof(vaddr) ||
			       pfd->pf_flags & P_MULTI_COLOR);
		}
#endif /* _VCE_AVOIDANCE && DEBUG */
		_bclean_caches((void *)ctob(btoct(vaddr)), NBPP,
			  pfdattopfn(pfd),
			  CACH_ICACHE|CACH_DCACHE|CACH_WBACK|
			  CACH_INVAL|CACH_ICACHE_COHERENCY|CACH_VADDR);
		pageflags(pfd, P_CACHESTALE, 0);
	}
#endif /* R4000 */

	return retval;
}


#ifdef SN0
#define FETCHOP_VAR_SIZE 64
/*********************************************************************
 * This define needs to be revisited when we move to cells, currently
 * it is enabling all nodes to be able to access this fetchop
 * variable key words: cellular irix, cells, protection
 *********************************************************************/
#define MSPEC_PROTECTION_MASK 0xffffffffffffffffULL
/*
 * mode is set if we are coming from a first time page fault
 * and clear if we are coming from a swap in
 */
void
handle_fetchop_fault(
	pfd_t	*pfd,
	int	mode)
{
	size_t	page_size = NBPP;
	caddr_t kvaddr;
	int	i;

	kvaddr = small_pfntova_K0(pfdattopfn(pfd));
	if (mode)
		bzero(kvaddr, page_size);

	/* Enable fetchop variables to be accessed by all nodes */
	for (i = 0; i < page_size; i += FETCHOP_VAR_SIZE) {
		*(__uint64_t *)(kvaddr + i) = MSPEC_PROTECTION_MASK;
	}

	PAGE_POISON(pfd);
}
#endif


/*
 * Clean up after a swap read error during vfault processing.
 * This code frees the previously allocated page, and marks
 * the pfdat as bad.
 */
STATIC void
killpage(
	preg_t	*prp,
	pde_t	*pd)
{
	pfd_t	*pfd = pdetopfdat(pd);

	/*
	 * This function must be called with the
	 * pfdat in a non-migratable state.
	 */

	ASSERT(pfd->pf_flags & (P_HASH|P_BAD));
	pagedone(pfd);
	(void) anon_uncache(pfd);

	/* the page has been yanked off the hash chain so we don't
	 * really have to worry about the P_ANON|P_SWAP bits ..
	 */
	pagefree(pfd);
	PREG_NVALID_DEC(prp, 1);		/* SPT */
	pg_clrpgi(pd);
}


STATIC int
autogrow(
	vnode_t		*vp,
	off_t		size,
	uthread_t	*ut)
{
	struct vattr	va;
	int		error;
	ssize_t		resid;
	char		lastbyte = 0;
	cred_t		*cr = get_current_cred();

	va.va_mask = AT_SIZE;
	VOP_GETATTR(vp, &va, 0, cr, error);
	if (error)
		return error;

	/* If file has already been grown, just return.
	 */
	if (va.va_size >= size)
		return 0;


	/* XXX File might have grown since the test of i_size
	 * XXX if locking is pushed down to VOP.  Will have to
	 * XXX fix race...
	 */
	error = vn_rdwr(UIO_WRITE, vp, &lastbyte, 1, size - 1, UIO_SYSSPACE, 0,
			getfsizelimit(),
			cr, &resid, &ut->ut_flid);
	if (error)
		return error;
	/* XXX short write? (resid != 0) */

	return(0);
}

/*
 * Attach to a user page.
 * Make a user's page copy-on-write and otherwise nail it down
 * so that a kernel buffer system can use the page directly.
 * This feature is used to avoid copying user data into a kernel
 * buffer.
 *
 * This implementation is a kludge for performance reasons.  The
 * "correct" way to implement this would have been to call anon_dup.
 * That way all the normal pfault code would do the right thing just
 * as if the page was being shared copy-on-write with another process.
 * Unfortunately, doing a lot of anon_dup's (one every time we want
 * attach a page into the kernel) is very expensive because of the
 * overhead of expanding and collapsing the anon tree.  The next best
 * way to do it would have been to use a special attribute in the
 * pregion.  These are expensive too, because attributes are expanded
 * and collapsed as well.  Therefore we use the least clean solution
 * of marking the pfdat with the P_CW flag.  This flag is a tip off
 * to pfault that tells it that it must make a copy of the page when
 * it normally would have stolen it.  Note that this single flag bit
 * is sufficient even if multiple processes are trying to pattach the
 * same anon page.  In this case, the page must either be from a file
 * (hasn't gone anon yet) or must be at a non-leaf anon node.  In either
 * case, pfault will make a copy of the page even if the P_CW flag is off.
 * The P_CW flag is only there to force another copy-on-write to occur
 * when the page is already private to the process.
 */
int
pas_pattach(
	pas_t		*pas,
	ppas_t		*ppas,
	uvaddr_t	vaddr,
	int		do_sync,
	caddr_t		*newkva)
{
	preg_t		*prp;
	reg_t		*rp;
	pde_t		*pd;
	pfd_t		*pfd;
	caddr_t		newva;
	unsigned	tmp, do_cow;
	attr_t		*pattr;
	extern int	mmap_async_write;

 pattach_startover:
	mraccess(&pas->pas_aspacelock);

	/* Check for isolated address space */
	if (CPUMASK_IS_NONZERO(pas->pas_isomask)) {
		mrunlock(&pas->pas_aspacelock);
		return(EINVAL);
	}

	/* Get a pointer to the region in which
	 * the faulting virtual address resides.
	 */
	if ((prp = findpreg(pas, ppas, vaddr)) == NULL) {
		mrunlock(&pas->pas_aspacelock);
		return(EINVAL);
	}
	rp = prp->p_reg;
	reglock(rp);

	/* Check for write access.  Also, only PF_DUP regions can be
	 * duplicated since we must make the user's page copy-on-write.
	 */
	pattr = findattr(prp, vaddr);

	/*
	 * Skip watched pages, else we will loop since we will
	 * never be able to mark pte valid.
	 */
	if (pattr->attr_watchpt) {
		regrele(rp);
		return(EINVAL);
	}

	do_cow = ((prp->p_flags & PF_DUP) != 0);
	if ((!do_cow && !mmap_async_write) ||
	    (do_cow && (pattr->attr_prot & PROT_W) == 0)) {
		regrele(rp);
		mrunlock(&pas->pas_aspacelock);
		return(EINVAL);
	}

	if ((pd = pmap_pte(pas, prp->p_pmap, vaddr, VM_NOSLEEP)) == NULL) {
		regrele(rp);
		mrunlock(&pas->pas_aspacelock);
		return(EINVAL);
	}

	/* Check to see that the pde hasn't been modified
	 * while waiting for the lock.
	 */
	pfd = pdetopfdat_hold(pd);
	if ((!pg_isvalid(pd)) || (pfd  == NULL)) {
		regrele(rp);
		mrunlock(&pas->pas_aspacelock);
		if (fubyte(vaddr) == -1)
			return(EINVAL);
		goto pattach_startover;
	}

	/* Fail if this page is involved with userland DMA - PV 763277.
	 * This is necessary to prevent a useracc/pattach/store/unuseracc
	 * scenario in which the pattach causes the original useracc'd page to
	 * be COW, the store breaks the COW, and thus unuseracc happens to
	 * the wrong (newly created) page.
	 * If the pfdat could be changed to include seperate locked page counts
	 * for raw I/O and pinned pages, we could do this in a more complete
	 * and correct manner.  But, alas, we cannot change pfdat at this time.
	 * P_USERDMA exists solely for this scenario.
	 */
	if (ensure_no_rawio_conflicts) {
		if ((pfd->pf_flags & P_USERDMA) || (prp->p_fastuseracc)) {
			pfdat_release(pfd);
			regrele(rp);
			mrunlock(&pas->pas_aspacelock);
			return(EINVAL);
		}
	}

	/* If physical memory is above 256 MB, need to get K2 addr.
	 * In any case we need virtual frame number.
	 */
	newva = page_mapin(pfd, 0, 0);

	if ((newva == 0) || reservemem(GLOBAL_POOL, 0, 1, 0)) {
		pfdat_release(pfd);
		regrele(rp);
		mrunlock(&pas->pas_aspacelock);
		if (newva)
			page_mapout(newva);
		return(EINVAL);
	}

	/*
	 * Add a reference to the page for the networking code.  Set P_CW
	 * to force copy-on-write even if this page is already in a leaf
	 * anon node.
	 */
	pageuseinc(pfd);

	if (do_cow) {
		pageflags(pfd, P_CW, 1);
		pfdat_release(pfd);

		/* If the page hasn't been modified, it is sufficient
		 * to just set the page copy-on-write (which clears the
		 * modified bit).  Any write by the user will force a pfault.
		 * do_sync == 1:
		 *	  If it has been modified, mark other tlb-s dirty (on
		 *	  MP) and replace local tlb entry.
		 * do_sync == 0:
		 *	  Don't deal with TLB synchronization here. caller must
		 *	  call psync before the kernel can reference the attached
		 *	  pages.
		 */
		if (do_sync)
			tmp = pg_ismod(pd);
		rp->r_flags |= RG_CW;
		pg_clrmod(pd);

		/*
		 * If this pte belongs to a large page we need to down grade the pte.
		 */
		if (IS_LARGE_PAGE_PTE(pd))
			pmap_downgrade_lpage_pte(pas, vaddr, pd);

		if (do_sync) {
			if ((pas->pas_flags & PAS_SHARED) && (pas->pas_refcnt != 1))
				tlbclean(pas, btoct(vaddr), allclr_cpumask);

			if (tmp) {
				int s;

				s = splhi();
				ON_MP(tlbdirty(ppas->ppas_utas));
				tlbdropin(ppas->ppas_utas->utas_tlbpid, vaddr, pd->pte);
				splx(s);
			}
		}
	} else
		pfdat_release(pfd);

	regrele(rp);
	mrunlock(&pas->pas_aspacelock);
	*newkva = newva;
	return(0);
}

/*
 * Release a page "attached" with pattach().
 */
void
prelease(caddr_t va)
{
	pfd_t	*pfd;
	pgno_t	vfn = vatovfn(va);

	/*
	 * The extra use reference acquired in
	 * pattach prevents any migration for
	 * this pfdat until the ref is released in
	 * pagefree.
	 */
	ASSERT(IS_VFN(vfn));
	pfd = vfntopfdat(vfn);
	page_mapout(vfntova(vfn));	/* release K2 mem, if allocated */

	ASSERT(pfd->pf_flags & P_CW);
	pagefreeanon(pfd);
	unreservemem(GLOBAL_POOL, 0, 1, 0);
}

/*
 * Flip a page to user space.
 * This is used to avoid copying data from the kernel to the user.
 * It takes a kernel virtual address and a user virtual address,
 * and tries to replace the physical memory represent the latter
 * with the former.
 *
 * On success, the kernel page has become part of user space
 * and the page that may have been there before has been released.
 *
 * Return 0 if unable to flip or kernel-mapped version of target
 */
int
pas_pflip(
	pas_t	*pas,
	ppas_t	*ppas,
	caddr_t	kern_va,		/* flip page this page */
	uvaddr_t tgt,			/* to this virtual address */
	int	do_sync,
	caddr_t	*newkva)
{
	preg_t	*prp;
	reg_t	*rp;
	pde_t	*tgt_pd;
	pfd_t	*tgt_pfd;
	pfd_t	*kern_pfd;
	/* REFERENCED */
	attr_t	*attr;
	pgno_t	kern_pfn;
	pgno_t	kern_vfn;
	int s;
#ifdef DEBUG
	extern int m_flip_off;
#endif
	mraccess(&pas->pas_aspacelock);

	/* Check for isolated address space */
	if (CPUMASK_IS_NONZERO(pas->pas_isomask)) {
		mrunlock(&pas->pas_aspacelock);
		return(EINVAL);
	}

	/* Get pointer to region containing target virtual address.  */
	if ((prp = findpreg(pas, ppas, tgt)) == 0) {
		mrunlock(&pas->pas_aspacelock);
		return(EINVAL);
	}
	rp = prp->p_reg;
	reglock(rp);

	/* Check for write access.  */
	if (((attr = findattr(prp, tgt))->attr_prot & PROT_W) == 0) {
		regrele(rp);
		mrunlock(&pas->pas_aspacelock);
		return(EINVAL);
	}

	/*
	 * SPT
	 * Page flipping does not work on SHM if more than one
	 * pmap has a reference on the page (TLB shutdown is a
	 * problem). The pf_use count on the page is used to detect
	 * multiple users (see test below). In case of shared PT,
	 * pf_use does not count the number of pmaps through which
	 * this page is mapped. It counts the number of PTE's through
	 * which this page is mapped. Since the PTE is shared among
	 * different pmaps, pf_use == 1 doesn't imply that only
	 * one pmap is referencing the page.
	 * For this reason we disallow page flipping if this page
	 * is mapped through a shared PTE.
	 */
	if (prp->p_type == PT_SHMEM && pmap_spt_check(prp->p_pmap, tgt, NBPP)) {
		regrele(rp);
		mrunlock(&pas->pas_aspacelock);
		return(EINVAL);
	}

	/* tgt_pd is the virtual address of the target pde in the
	 * k2seg CONTEXT map of the user's page tables.
	 */
	if ((tgt_pd = pmap_pte(pas, prp->p_pmap, tgt, VM_NOSLEEP)) == NULL) {
		regrele(rp);
		mrunlock(&pas->pas_aspacelock);
		return(EINVAL);
	}


	/* Give up if things are complicated.
	 *
	 * XXX One complication that would be nice to handle is reading
	 * XXX into an absent page.  We could pretend to fault it in.
	 *
	 * XXX Other complications might be easily handled or never occur.
	 * XXX Since this entire mechanism is justified by speed, nothing
	 * XXX that is slow belongs here.  However, it is nice to have
	 * XXX consistent performance, meaning it would be nice to make
	 * XXX apparently irrelevant complications not affect speed.
	 * XXX It would be good to handle all easy complications.
	 */
	tgt_pfd = pdetopfdat_hold(tgt_pd);

	if (!tgt_pfd) {
		regrele(rp);
		mrunlock(&pas->pas_aspacelock);
		return(EINVAL);
	}

	if (!pg_ishrdvalid(tgt_pd)
	    || (!pfdat_held_oneuser(tgt_pfd))
	    || (tgt_pfd->pf_flags & P_ANON) == 0
	    || tgt_pfd->pf_tag != anon_tag(rp->r_anon)
	    || (rp->r_type != RT_MEM)
	    || tgt_pfd->pf_rawcnt != 0
#ifdef DEBUG
	    || m_flip_off != 0
#endif
	    ) {
		pfdat_release(tgt_pfd);
		regrele(rp);
		mrunlock(&pas->pas_aspacelock);
		return(EINVAL);
	}

	/*
	 * Downgrade the pte if part of a large page.
	 */
	if (IS_LARGE_PAGE_PTE(tgt_pd))
		pmap_downgrade_lpage_pte(pas, tgt, tgt_pd);

	/* Do the work of a protection fault.  */
	if (pg_ismod(tgt_pd)) {
		ASSERT(!(tgt_pfd->pf_flags & P_SWAP));
	} else {
		/* any swap space will be freed in anon_flip() */
		MINFO.steal++;
		pg_setmod(tgt_pd);
	}

	/* Remove the existing pfd<->pd mapping */
	VPAG_UPDATE_RMAP_DELMAP(PAS_TO_VPAGG(pas), JOBRSS_FLIP_PFD_DEL, tgt_pfd, tgt_pd);

	ASSERT(0 == tgt_pfd->pf_pchain);
	ASSERT(!(tgt_pfd->pf_flags & P_QUEUE));


	/* kern_vfn must be a virtual page number.
	 * If it's in K2 (due to large memory config) then we must
	 * free the old K2 address.
	 */
	kern_vfn = vatovfn(kern_va);
	ASSERT(IS_VFN(kern_vfn));
	kern_pfn = vfntopfn(kern_vfn);
	if (IS_K2VFN(kern_vfn))
		kvfree(vfntova(kern_vfn),1);

	/* The source page is an anonymous kernel page.
	 */
	kern_pfd = pfntopfdat(kern_pfn);
	pfd_clrflags(kern_pfd, P_DUMP | P_BULKDATA);
	ASSERT(kern_pfd->pf_flags == 0 || (kern_pfd->pf_flags & P_DONE)
		|| (kern_pfd->pf_flags & P_CACHESTALE));
	ASSERT(kern_pfd->pf_hchain == 0);
	ASSERT(kern_pfd->pf_pchain == 0);

	/*
	 * Make the switch.  The old page of the user was known
	 * only to the region.  The new page is anonymous.  So we
	 * can just flip them.
	 * We also transfer the pfms state, and move the "hold"
	 * state from tgt_pfd to kern_pfd.
	 */
	anon_flip(tgt_pfd, kern_pfd);

#ifdef _VCE_AVOIDANCE
	while (PREG_COLOR_VALIDATION(prp,kern_pfd,tgt,1 /*USERMODE*/))
		;
#endif
	pg_setpfn(tgt_pd,  kern_pfn);

	VPAG_UPDATE_RMAP_ADDMAP(PAS_TO_VPAGG(pas), JOBRSS_FLIP_PFD_ADD, kern_pfd, tgt_pd, attr_pm_get(attr));

	/*
	 * anon_flip released tgt_pfd, and did a pfdat_hold
	 * on kern_pfd.
	 */
	pfdat_release(kern_pfd);

	/*
	 * case do_sync == 1:
	 *   For shaddr processes, actively unmap all processes'
	 *   mapping of this page on all processors.  Otherwise
	 *   take fast path (tlbdirty) to ensure tlb consistency
	 *   on MP machines, and always purge local tlb entry.
	 * case do_sync == 0:
	 *   Don't deal with TLB synchronization here. Caller must call
	 *   psync before it frees the old pre-flip pages.
	 */

	if (do_sync) {
		if ((pas->pas_flags & PAS_SHARED) &&
		    (pas->pas_refcnt != 1)) {
			tlbclean(pas, btoct(tgt), allclr_cpumask);
		} else {
			/* no migration */
			s = splhi();
			ON_MP(tlbdirty(ppas->ppas_utas));
			unmaptlb(ppas->ppas_utas->utas_tlbpid, btoct(tgt));
			splx(s);
		}
	}

	regrele(rp);			/* finished with the region */
	mrunlock(&pas->pas_aspacelock);

	/* Return the page to the mbuf code to replace its page.
	 * Must call page_mapin in case we have large memory config
	 * and tgt_pfd is in high end of memory.
	 */
	*newkva = page_mapin(tgt_pfd, 0, 0);
	return 0;
}

/*
 * Synchronize the TLBs after using pflip() or pattach().
 * psync() *must* be called before anything is permitted to access
 * the flipped or attached data, whether on this CPU or another.
 */
int
pas_psync(
	pas_t	*pas,
	ppas_t	*ppas)
{
	mraccess(&pas->pas_aspacelock);
	tlbcleansa(pas, ppas, allclr_cpumask);
	mrunlock(&pas->pas_aspacelock);
	return 0;
}


/*
 * Generic PHYS region fault handler
 * Simply returns 1 signifying unable to handle the fault
 */
/* ARGSUSED */
int
genphysfault(
	vhandl_t	*vt,
	void		*s,
	caddr_t		vaddr,
	int		rw)
{
	return(1);
}

#if JUMP_WAR

#if CELL
#error BOMB!! JUMP_WAR not virtualized.
#endif
/*
 * There is a bug in the R4000 Rev 2.2 (and previous revisions)
 * If certain bad sequences of instructions occurs at the end
 * of a page and the following page is not mapped by the tlb,
 * then the R4000
 *	1) Always fails to take a utlbmiss on the delay slot AND
 *	2) Executes a nop instead of the branch delay slot AND
 *	3) MAY jump to the wrong location(!)
 */
#include "sys/inst.h"
#include "sys/prctl.h"

extern int _unmaptlb(unsigned char *, __psint_t);

/*
 * Checks for integer load instructions.  Works only for
 * MIPS I and II opcodes.
 */
int
is_intload(inst_t instruction)
{
	uint	opcode;
	union mips_instruction inst;

	inst.word = instruction;
	opcode = inst.i_format.opcode;

	switch (opcode) {
	case ldl_op:
	case ldr_op:
	case lb_op:
	case lh_op:
	case lwl_op:
	case lw_op:
	case lbu_op:
	case lhu_op:
	case lwr_op:
	case lwu_op:
	case ll_op:
	case lld_op:
	case ld_op:
		return 1;
	}

	return 0;
}

/*
 * Checks for jumps through registers.
 */
int
is_jumpreg(inst_t instruction)
{
	union mips_instruction inst;

	inst.word = instruction;
	if (inst.f_format.opcode == spec_op) {
		switch(inst.f_format.func) {
			case jr_op:
				return(j_op);
			case jalr_op:
				return(jal_op);
		}
	}
	return(0);
}

/*
 * Checks for branches that compare 1 or 2 registers (conditional).
 */
int
is_condbranch(inst_t instruction)
{
	union mips_instruction inst;

	inst.word = instruction;
	switch(inst.i_format.opcode) {
		case bcond_op:
		case beq_op:
		case bne_op:
		case blez_op:
		case bgtz_op:
		case beql_op:
		case bnel_op:
		case blezl_op:
		case bgtzl_op:
			return(1);
		default:
			break;
	}

	return(0);
}

/*
 * Checks for integer divide instructions.
 */
int
is_intdiv(inst_t instruction)
{
	union mips_instruction inst;

	inst.word = instruction;
	if (inst.r_format.opcode == spec_op) {
		switch(inst.r_format.func) {
		case div_op:
		case divu_op:
		case ddiv_op:
		case ddivu_op:
			return(1);
		default:
			return(0);
		}
	}
	return 0;
}

/*
 * Given a load instruction, returns the general purpose
 * register used as a destination.  Returns 0, if it's
 * passed a floating point load instruction.
 */
int
loadinst_reg(inst_t load_instruction)
{
	union mips_instruction inst;

	inst.word = load_instruction;
	switch(inst.i_format.opcode) {
		case lwc1_op:
		case ldc1_op:
			return(0);
		default:
			return((uint)inst.i_format.rt);
	}
}

/*
 * Given a jump through register instruction, returns the
 * register used as source.
 */
int
jumpinst_reg(inst_t jump_instruction)
{
	union mips_instruction inst;

	inst.word = jump_instruction;
	return(inst.i_format.rs);
}

/*
 * Given a branch instruction, returns the first (or only) general
 * purpose register number used in the comparison.
 */
int
branchinst_reg1(inst_t branch_instruction)
{
	union mips_instruction inst;

	inst.word = branch_instruction;
	switch(inst.i_format.opcode) {
		case bcond_op:
		case beq_op:
		case bne_op:
		case blez_op:
		case bgtz_op:
		case beql_op:
		case bnel_op:
		case blezl_op:
		case bgtzl_op:
			return(inst.i_format.rs);
		default:
			break;
	}
	return(0);
}

/*
 * Given a branch instruction, returns the second general purpose
 * register number used in the branch comparison, if any.  If the
 * branch compares to zero, there is no "second" register, so we
 * return 0.
 */
int
branchinst_reg2(inst_t branch_instruction)
{
	union mips_instruction inst;

	inst.word = branch_instruction;
	switch(inst.i_format.opcode) {
		case beq_op:
		case bne_op:
		case beql_op:
		case bnel_op:
			return(inst.i_format.rt);

		case bcond_op:
		case blez_op:
		case bgtz_op:
		case blezl_op:
		case bgtzl_op:
			return(0);

		default:
			break;
	}
	return(0);
}

#ifdef _R5000_JUMP_WAR
/*
 * Checks for jumps and branches
 */
STATIC int
is_jump_or_branch(inst_t instruction)
{
	union mips_instruction inst;

	inst.word = instruction;
	switch(inst.i_format.opcode) {
	case spec_op:
		switch(inst.f_format.func) {
		case jr_op:
		case jalr_op:
			return(1);
		default:
			break;
		}
		break;

	case j_op:
	case jal_op:
	case beq_op:
	case bne_op:
	case blez_op:
	case bgtz_op:
	case beql_op:
	case bnel_op:
	case blezl_op:
	case bgtzl_op:
		return(1);

	case bcond_op:
		switch (inst.i_format.rt) {
		case bltz_op:
		case bgez_op:
		case bltzl_op:
		case bgezl_op:
		case bltzal_op:
		case bgezal_op:
		case bltzall_op:
		case bgezall_op:
			return(1);
		default:
			break;
		}
		break;

	default:
		break;
	}
	return(0);
}

/*
 * Checks for loads and stores
 */
STATIC int
is_load_or_store(inst_t instruction)
{
	union mips_instruction inst;

	inst.word = instruction;
	switch(inst.i_format.opcode) {
	case ldl_op:
	case ldr_op:
	case lb_op:
	case lh_op:
	case lwl_op:
	case lw_op:
	case lbu_op:
	case lhu_op:
	case lwr_op:
	case lwu_op:
	case sb_op:
	case sh_op:
	case swl_op:
	case sw_op:
	case sdl_op:
	case sdr_op:
	case swr_op:
	case ll_op:
	case lwc1_op:
	case lwc2_op:
	case pref_op:
	case lld_op:
	case ldc1_op:
	case ldc2_op:
	case ld_op:
	case sc_op:
	case swc1_op:
	case swc2_op:
	case scd_op:
	case sdc1_op:
	case sdc2_op:
	case sd_op:
		return(1);
	default:
		break;
	}
	return(0);
}

/*
 * dynamic_jump_war_check
 *
 * -- called from cacheflush() to check for pages becoming
 *    executable with jump-fault problems
 */

int
dynamic_jump_war_check(
	caddr_t		addr,
	uint		bcnt)
{
	uint		rem;
	pde_t		*pd;
	__psint_t	vaddr;
	preg_t		*prp;
	reg_t		*rp;
	attr_t		*attr;
	int		changed_entry = 0;
	vasid_t		vasid;
	pas_t		*pas;
	ppas_t		*ppas;

	/* quick check to exclude impossible cases */
	vaddr = (__psint_t) (ctob((btoct(addr) | 1)) + NBPC - 0xc);
	if (vaddr > ((__psint_t) (addr + bcnt)))
		return(0);

	/* Make sure that address range is in user space */
	if (!IS_KUSEG((__psunsigned_t)addr + bcnt - 1))
		return EFAULT;

	as_lookup_current(&vasid);
	pas = VASID_TO_PAS(vasid);
	ppas = (ppas_t *)vasid.vas_pasid;
	mraccess(&pas->pas_aspacelock);
	for (vaddr = ((__psint_t) addr);
	     bcnt > 0;
	     bcnt -= rem, vaddr += rem) {
		rem = MIN(bcnt, (NBPC - poff(vaddr)));
		/*
		 * skip this page, if not odd virtual page or if not flushing
		 * end of page or if no physical page assigned
		 */
		if ((vaddr & NBPC) == 0 ||
		    (poff(vaddr) + rem) < (NBPC - 0xc))
			continue;

		/*
		 * check if this is a problem page
		 */
		prp = findpreg(pas, ppas, (caddr_t)vaddr);
		if (prp == NULL)
			continue;
		rp = prp->p_reg;
		reglock(rp);

		attr = findattr(prp, (caddr_t) vaddr);
		if ((attr->attr_prot & PROT_R) == 0) {
			/* skip if not accessible */
			regrele(rp);
			continue;
		}

		if ((pd = pmap_pte(pas, prp->p_pmap, (caddr_t) vaddr, VM_NOSLEEP)) != NULL) {
			/* skip if already set */
			if (pg_iseop(pd)) {
				regrele(rp);
				continue;
			}
			/* if valid and newly eop, force a vfault */
			if (pg_isvalid(pd) &&
			     end_of_page_bug(pd,(caddr_t) vaddr)) {
				pg_clrhrdvalid(pd);
				changed_entry++;
			}
		}
		attr->attr_prot |= PROT_X;
		regrele(rp);
	}
	if (changed_entry)
		tlbsync(0LL, allclr_cpumask, STEAL_PHYSMEM);
	mrunlock(&pas->pas_aspacelock);

	return 0;
}
#endif /* _R5000_JUMP_WAR */

STATIC char *eop_vaddr;
/*
 * Detects the jump-at-end-of-page bug in R4000 Rev 2.0/2.1/2.2.
 * Returns 1 if the bug may occur
 *	   0 if it is guaranteed not to occur
 *
 * Someone can come up with some slick way using XORs to do this
 * faster someday.  For now, we brute force it.
 */
STATIC int
end_of_page_bug(
	pde_t	*pd,
	caddr_t	vaddr)
{
	caddr_t	textvaddr;
	int	whichreg;
	uint	ep1_inst;
	uint	ep2_inst;
	uint	ep3_inst;
	caddr_t	npage;
	pde_t	eop_pde;
	int	ospl;


	if (!R4000_jump_war_correct && !R4000_div_eop_correct
#ifdef _R5000_JUMP_WAR
	    &&
	    !R5000_jump_war_correct
#endif /* _R5000_JUMP_WAR */
	    )
		return(0);
	/*
	 * If this page is already marked as having an
	 * EOP problem, no need to rescan.
	 *
	 * It might be nicer to have a bit in the pfdat, but
	 * we're all out of bits there.
	 */
	if (pg_iseop(pd))
		return(1);

#define INSTSIZE sizeof(union mips_instruction)

/* Defines for 1/2/3 instruction from end of page */
#define ENDPAGE1 textvaddr
#define ENDPAGE2 (ENDPAGE1-INSTSIZE)
#define ENDPAGE3 (ENDPAGE2-INSTSIZE)
	/* eop_vaddr -- set of virtual addresses used to access the page of
	 * instructions.  Same set of addresses used by all processors.
	 * No need to flush tlb since these addresses are only used here and
	 * we do a tlbdropin to allow access.  Mapping stays in tlb while we
	 * access instructions since we block all interrupts. If any access
	 * were to occur,
	 * tlbmiss would not find a valid mapping since we don't update pte
	 * in kptbl.  Fills two needs -- avoids VCEs and properly handles
	 * large memory configurations which need to perform this mapping
	 * in order to access the page at all.
	 */
	if (!eop_vaddr) {
		eop_vaddr = (char *)kvalloc(cachecolormask+1, VM_VACOLOR, 0);
		ASSERT(eop_vaddr);
	}
	npage = (caddr_t)(eop_vaddr+NBPP*colorof(vaddr));
	eop_pde.pgi = mkpde(PG_VR|PG_G|pte_cachebits(), pdetopfn(pd));
	ospl = spl7();
	tlbdropin(0, npage, eop_pde.pte);
	textvaddr = (caddr_t)npage + NBPP - INSTSIZE;

	ep1_inst = *(uint *)ENDPAGE1;
	ep2_inst = *(uint *)ENDPAGE2;
	ep3_inst = *(uint *)ENDPAGE3;
	splx(ospl);
#undef ENDPAGE1
#undef ENDPAGE2
#undef ENDPAGE3
#undef INSTSIZE

#ifdef _R5000_JUMP_WAR
	if (R5000_jump_war_correct) {
		if ((pnum(vaddr) & 1) &&
		    is_jump_or_branch(ep2_inst) &&
		    is_load_or_store(ep1_inst))
			goto eop_bug;
		return(0);
	}
#endif /* _R5000_JUMP_WAR */

	/* Handle the div_eop bug. Note that this test occurs before
	 * jump-at-eop processing, since the div-at-eop is present
	 * in 2.2 and 3.0 rev R4000's, but the jump-at-eop is only
	 * present in the 2.2 parts. Hence, 3.0-based systems will
	 * have R4000_div_eop_correct set and R4000_jump_war_correct
	 * clear.
	 */
	if (is_intdiv(ep1_inst))
		goto eop_bug;

	if (!R4000_jump_war_correct) {
		/* Must be a 3.0 part */
		return 0;
	}

	if (is_jumpreg(ep1_inst)) {
		whichreg = jumpinst_reg(ep1_inst);
		if (is_intload(ep2_inst) && (loadinst_reg(ep2_inst) == whichreg))
				goto eop_bug;

		if (is_intload(ep3_inst) && (loadinst_reg(ep3_inst) == whichreg))
				goto eop_bug;
	}

	if (is_condbranch(ep1_inst)) {
		if (is_intload(ep2_inst)) {
			whichreg = loadinst_reg(ep2_inst);
			if (branchinst_reg1(ep1_inst) == whichreg)
				goto eop_bug;
			if (branchinst_reg2(ep1_inst) == whichreg)
				goto eop_bug;
		}
		if (is_intload(ep3_inst)) {
			whichreg = loadinst_reg(ep3_inst);
			if (branchinst_reg1(ep1_inst) == whichreg)
				goto eop_bug;
			if (branchinst_reg2(ep1_inst) == whichreg)
				goto eop_bug;
		}
	}

	if (is_branch(ep1_inst)) {
		if (is_load(ep3_inst) || is_store(ep3_inst)) {
			if (is_intdiv(ep2_inst))
				goto eop_bug;
		}

		if (R4000_jump_war_always)
			goto eop_bug;
	}
	return(0);

 eop_bug:
	pg_seteop(pd);
	if (R4000_debug_eop)
		cmn_err(CE_CONT,"eop bug, command: %s  vaddr 0x%x\n",
			get_current_name(), vaddr);
	return(1);
}

STATIC void
flush_jump_war_set(int set)
{
	int i;

	ASSERT(curuthread != NULL);
	ASSERT(current_pid() != 0);
	ASSERT(curuthread->ut_id == private.p_jump_war_uthreadid &&
		current_pid() == private.p_jump_war_pid);

	/* Flush out old wired entries, if any, in this set. */
	for (i = 0; i < NWIREDJUMP; i++) {
		_invaltlb(NWIREDENTRIES + NWIREDJUMP * set + i);

	}
}


/*
 * Software exception: end-of-page handler.
 * Returns
 *	0 if all necessary pages are in (and wired down)
 *	1 if could not get everything wired in
 */

/*
 * Permit JUMP_WAR_SETS sets of wired entries for use by the jump
 * workaround code.  Each set has up to NWIREDJUMP dual-tlb entries.
 * We can accomodate any program that has 2*NWIREDJUMP-1 or fewer
 * contiguous pages with End-Of-Page jump problems.  If there are
 * more than JUMP_WAR_SETS such sets of contiguous pages, performance
 * will suffer.  Note that JUMP_WAR_SETS must be at least 2 to permit
 * a program executing on an "bad" page to read a distant text page
 * which is also bad.
 */

int
sexc_eop(
	eframe_t	*ep,
	uint		code,
	int		*suberror)
{
	__psint_t	vaddr;
	uthread_t	*ut;
	int	rval = 0;
	int	jump_contig;
	int	i;
	int	max_jump_pages;
	int	wired_slots;
	int	fault_flags;
	preg_t	*prp;
	reg_t	*rp;
	vasid_t	vasid;
	pas_t	*pas;
	ppas_t	*ppas;
	utas_t	*utas;
	int	set;
	int	s;
	int	inval_start_ok = 0;
	int	inval_end_ok = 0;
	pde_t	tpde[2 * NWIREDJUMP];

	ASSERT(curuthread != NULL);
	ut = curuthread;
	utas = &ut->ut_as;

	/*
	 * End-Of-Page bug encountered.
	 *
	 * We need to fault in each page before trying to read it.
	 * Otherwise, we might take page read faults, and vfault would
	 * do EOP processing.  Pretty soon, we'd recurse off the kernel
	 * stack.  Note that it's OK to take tlbmisses, just not actual
	 * page faults.
	 */
	vaddr = (__psint_t)ep->ef_badvaddr;
	jump_contig = 1;	/* original badvaddr page */

	max_jump_pages = vaddr & NBPP ? (2 * NWIREDJUMP) - 1 : 2 * NWIREDJUMP;

	fault_flags = (code == EXC_WMISS) ?
			W_WRITE : (vaddr == ep->ef_epc) ?
			W_EXEC : W_READ;

	vaddr = ctob(btoct(vaddr));

	rval = SEXC_EOP;
	while (rval == SEXC_EOP && jump_contig <= max_jump_pages) {
		vaddr += NBPP;
		jump_contig++;

		rval = vfault((caddr_t)vaddr, fault_flags, ep, suberror);
	}

	if (rval && rval != SEXC_PAGEIN && rval != SEXC_EOP) {
		/*
		 * Couldn't bring in a page we needed.
		 */
		if (R4000_jump_war_warn)
			cmn_err(CE_WARN,
				"For R4000, rval %x @%llx trying to fix branch "
				"at end of page (%x) [%s] pid=%d\n",
				rval, ep->ef_badvaddr, vaddr,
				get_current_name(), current_pid());

		if (R4000_jump_war_kill)
			return 1;

		rval = 0;
	} else if (rval == SEXC_PAGEIN) {
		rval = 0;
	}

	if (jump_contig > max_jump_pages) {
		if (R4000_jump_war_warn) {
#ifdef _R5000_JUMP_WAR
			if (R5000_jump_war_correct)
				cmn_err(CE_WARN,
					"Too many contiguous end-of-page jump "
					"problems in [%s] pid=%d\n",
					get_current_name(), current_pid());
			else
#endif /* _R5000_JUMP_WAR */

			if (R4000_div_eop_correct && !R4000_jump_war_correct)
				cmn_err(CE_WARN,
					"Too many contiguous end-of-page "
					"divide problems in [%s] pid=%d\n",
					get_current_name(), current_pid());
			else
				cmn_err(CE_WARN,
					"Too many contiguous end-of-page jump "
					"or divide problems in [%s] pid=%d\n",
					get_current_name(), current_pid());
		}

		if (R4000_jump_war_kill)
			return 1;

		rval = 0;
		jump_contig = max_jump_pages;
	}

	ASSERT(rval == 0);

	vaddr = ep->ef_badvaddr;

	as_lookup_current(&vasid);
	pas = VASID_TO_PAS(vasid);
	ppas = (ppas_t *)vasid.vas_pasid;
	mraccess(&pas->pas_aspacelock);

	prp = findpreg(pas, ppas, (caddr_t)vaddr);
	ASSERT(prp != NULL);
	rp = prp->p_reg;
	reglock(rp);

	/*
	 * Flush out any previous entries for the pages that
	 * we're about to wire down.  unmaptlb wipes out both
	 * entries in the pair, so we're flushing a bit more than
	 * we really need to.
	 */
	for (i = 0; i < jump_contig; i++, vaddr += NBPP) {
		int which = _unmaptlb(utas->utas_tlbpid, btoct(vaddr));

		/*
		 * If we're wiping out a piece of another set of EOP
		 * pages, we need to wipe out the whole thing.  This
		 * happens if we have an odd page with a problem followed
		 * by a clean even number page followed by another bad
		 * odd page.
		 *
		 * We could just clear the one affected set from the
		 * tlb, but we punt here and blast all the jump_war
		 * wired entries.
		 */
		if (which && which < NWIREDENTRIES + ut->ut_max_jump_war_wired) {
			clr_jump_war_wired();
			ut->ut_max_jump_war_wired = 0;
			ut->ut_jump_war_set = 0;
		}
	}

#ifdef DEBUG
	ut->ut_jump_war_stats[jump_contig-1]++;
#endif

	/* If vaddr is on an odd-numbered page, decrement vaddr by
	 * by pagesize, so that the even page gets included when
	 * we wire the entries into the tlb.
	 */
	vaddr = ep->ef_badvaddr;
	if (vaddr & NBPP) {
		jump_contig++;
		inval_start_ok = 1;
		vaddr -= NBPP;
	}

	/* Similarly, if the workaround ends on an even-numbered page,
	 * extend the set to include the odd page of that tlb pair.
	 */
	if (jump_contig & 1) {
		jump_contig++;
		inval_end_ok = 1;
	}

	wired_slots = jump_contig / 2;

	bzero(tpde, sizeof(tpde));

	for (i = 0; i < jump_contig; i++, vaddr += NBPP) {
		pde_t	*pd;

		pd = pmap_pte(pas, prp->p_pmap, (caddr_t)vaddr, 0);

		if (!pg_isvalid(pd)) {
			/*
			 * While we faulted in pages, vhand must have stolen
			 * some of the other ones.  Better not wire anything
			 * in now.  We'll just go back and let the user fault
			 * again.
			 */
			if ((i == 0 && inval_start_ok) ||
			    (i == jump_contig - 1 && inval_end_ok)) {

				/* We bumped vaddr down 1 page to try
				 * to include this page in our wired
				 * set - since this page does not have
				 * an EOP problem (or may not have been
				 * referenced yet), it is OK for the page
				 * to not be in the set.
				 *
				 * Same thing for the ending page case: if
				 * we expanded our set to include an odd
				 * numbered page, it is Ok to find it invalid.
				 */
				continue;
			}
#ifdef DEBUG
			ut->ut_jump_war_stolen++;
#endif

			regrele(rp);
			mrunlock(&pas->pas_aspacelock);
			return rval;
		}

		ASSERT((i == jump_contig - 1) ||
		       (i == 0 && inval_start_ok) ||
		       (i == jump_contig - 2 && inval_end_ok) ||
			!pg_ishrdvalid(pd));
		ASSERT((i == jump_contig - 1) ||
		       (i == 0 && inval_start_ok) ||
		       (i == jump_contig - 2 && inval_end_ok) ||
			pg_iseop(pd));

		tpde[i].pgi = pd->pgi;
		pg_sethrdvalid(&tpde[i]);
	}

	s = splhi();

	/*
	 * Committed to wiring in entries now.
	 */

	if (private.p_jump_war_pid == current_pid() &&
	    private.p_jump_war_uthreadid == ut->ut_id) {
		set = ut->ut_jump_war_set;
		flush_jump_war_set(set);
	} else {
		set = 0;
		clr_jump_war_wired();
		ut->ut_jump_war_set = 0;
		ut->ut_max_jump_war_wired = 0;
	}

#ifdef DEBUG
	ut->ut_jump_war_set_stats[set]++;
#endif

	if (NWIREDJUMP * set + wired_slots > ut->ut_max_jump_war_wired)
		ut->ut_max_jump_war_wired = NWIREDJUMP * set + wired_slots;

	private.p_jump_war_pid = current_pid();
	private.p_jump_war_uthreadid = ut->ut_id;

	/* R4000 chip (at least rev 2.2) seems to allow C0_RAND to become
	 * C0_TLBWIRED - 1 (at least when reading C0_RAND) so set it one
	 * greater.  This was causing the segment table handler in locore
	 * to sometimes wipeout the last tlb entry causing a jump problem.
	 */
	setwired(NWIREDENTRIES + ut->ut_max_jump_war_wired);

	vaddr = ep->ef_badvaddr;

	for (i = 0; i < wired_slots; i++, vaddr += 2 * NBPP) {
		tlbwired(NWIREDENTRIES + NWIREDJUMP * set + i,
			utas->utas_tlbpid,
			(caddr_t)vaddr,
			tpde[2*i].pte,
			tpde[2*i+1].pte,
			TLBPGMASK_MASK);
	}

	splx(s);

	regrele(rp);
	mrunlock(&pas->pas_aspacelock);

	/* Bump jump_war_set for next time */
	ut->ut_jump_war_set = (set + 1) % JUMP_WAR_SETS;

	return rval;
}

void
unmaptlb(
	unsigned char	*pid,
	__psunsigned_t	vpage)
{
	int		idx;

	if ((idx = _unmaptlb(pid, vpage)) &&
	    idx >= NWIREDENTRIES &&
	    idx < getwired()) {
#ifdef NOTDEF
		OK, were not going to attempt to detect 'trapped' tlb entries
		anymore, since the jump_war data is now uthread-specific.
		If a random entry is 'trapped', we'll just blow down the
		whole jump_war set.

		int war_set, war_reg;

		/* A random entry could be here, if it's at the end
		 * of a set.  We only set wiredbase to last used register
		 * within the set, so unused regs may get random entries.
		 * Then when we skip to the next set (for another EOP) we
		 * set the wired base above that entry "trapping" it
		 * within the EOP wired region.  Detect this case and
		 * don't complain.
		 */
		war_set = (idx - NWIREDENTRIES)/NWIREDJUMP;
		war_reg = (idx - NWIREDENTRIES) % NWIREDJUMP;
		if ((jump_war_pte[war_set][2*war_reg] == 0) &&
		    (jump_war_pte[war_set][2*war_reg+1] == 0))
			return;		/* just a random tlb entry */
#endif
		/* If we're unmapping one of the JUMP_WAR wired entries, then
		 * we must unmap them all in order to maintain correctness of
		 * the JUMP_WAR
		 */
		clr_jump_war_wired();
	 }
}

void invaltlb(int idx)
{
	if (idx >= NWIREDENTRIES && idx < getwired()) {
#if DEBUG
		cmn_err(CE_WARN,"invaltlb: idx 0x%x within EOP fix\n", idx);
#endif
		clr_jump_war_wired();
	} else
		_invaltlb(idx);
}
#endif	/* JUMP_WAR */


#ifdef _VCE_AVOIDANCE
void
pfd_set_vcolor(
	pfd_t	*pfd,
	int	color)
{
	pfde_t	*pfde;

	ASSERT(!(!vce_avoidance && color == -1));

	pfde = pfd_to_pfde(pfd);
	pfde_set_vcolor(pfde,color);
}


int
pfd_wait_rawcnt(
	pfd_t		*pfd,
	int		flags)
{
	int		s, slept;
	struct vnode	*vp;
	struct pregion	*prp;
	off_t		preg_off;
	attr_t		*preg_attr;

	slept = 0;
	s = splhi();
	while (pfd->pf_rawcnt > 0) {
		int lock_count=0;

		vp = pfd->pf_vp;
		if (vp != NULL) {
			for (prp = vp->v_mreg;
			     prp != (preg_t *)vp;
			     prp = prp->p_vchain) {
				if (prp->p_reg == NULL)
					continue;
				preg_off = pfd->pf_pageno - (prp->p_offset +
					 offtoct(prp->p_reg->r_fileoff));
				if (preg_off >= prp->p_pglen)
					continue;
				preg_attr = findattr(prp, prp->p_regva + ctob(preg_off));
				if (preg_attr == NULL)
					continue;
				lock_count += preg_attr->attr_lockcnt;
			}
		}

		if (lock_count >= pfd->pf_rawcnt) {
			break;
		}
		if (flags & VM_NOSLEEP)
			return(-1);
		slept = 1;
		pfde_set_rawwait(pfd_to_pfde(pfd));
		sleep(pfd,PSWP + 1);
	}
	splx (s);

	return (slept);
}


void
pfd_wakeup_rawcnt(pfd_t *pfd)
{
	int	s;
	pfde_t	*pfde;

	if (!vce_avoidance)
		return;
	s = splhi();
	pfde = pfd_to_pfde(pfd);
	if (pfde_to_rawwait(pfde)) {
		pfde_clear_rawwait(pfde);
		wakeup((caddr_t) pfd);
	}
	splx(s);
}


/*
 * XXX Note:  this code is based on uniprocessor systems.
 * XXX It will need to be modified with appropriate locking
 * XXX for multiprocessor systems in order to replace
 * XXX the splhi()'s which were inserted in preg_color_validation()
 * XXX and bp_preg_color_validation() for a quick fix.
 */
int color_validation_nflush = 0;
int color_validation_full_nflush = 0;
#define CACH_SELECT_MASK 0x3

#ifndef apntorpn
#define apntorpn(P,N)  rpntoapn(P,N)
#endif

/*
 * is used when a process gets a vfault() on
 * a page, which is software-valid, but not hardware-valid, to assure
 * that the current "color" of the page matches that required by the
 * faulting process.  This may involve breaking the access of other users
 * of the page (by invalidating their PTEs and TLB entries for the page).
 * It is also used in pflip() for the same purpose, but here the reference
 * is generated by the kernel.
 *
 * In general, pfdat->pf_vcolor holds the virtual color for the mapping of
 * the physical page.  Another way of looking at it is pfdat->pf_vcolor is
 * the color in the primary cache at which you might find cached data for
 * this page.  pf_vcolor of -1 means this page has not been mapped yet so
 * there are no lines in the cache to worry about.
 *
 * Most of the time a page has only one current color.  If there are
 * multiple mappings for a page at different colors then only one mapping
 * is valid at a given time.  When a process first references the page at
 * a second color B, all mappings at current color A are invalidated and
 * the page is writeback-invalidated from the cache.  Then the reference
 * at color B is allowed and B becomes the new color.  This is the
 * preg_color_validation() stuff.
 *
 * That funky (P_HASH && !P_ANON && pf_vp) test is used to determine
 * if this page is a mapped file page.  If it is we use the color of
 * the file offset for the mapping just like the buffer cache would do
 * if it mapped the buffer.
 *
 * In 6.3 we added the P_MULTI_COLOR pf_flags bit to indicate that
 * read-only mappings for this page exist at multiple colors.  You might
 * have cache lines for this page in the cache at different colors but no
 * one will see stale data since all mappings are read-only.  If you need
 * to invalidate the cache for a page with P_MULTI_COLOR set then you need
 * to invalidate the entire cache since we don't keep track of all the
 * colors at which the page is mapped.
 *
 * Pages returned from pagealloc are cache clean (unless VM_STALE was
 * specified and a stale page was found) and pfdat->pf_vcolor == -1.
 */

STATIC int
preg_color_validation(
	struct pregion	*prp,
	pfd_t		*c,
	caddr_t		uvaddr,
	int		is_usermode)
{
	int	slept = 0;
	int	old_color;
	preg_t	*nprp;
	vnode_t *vp;
	pgno_t	nprp_base;
	pde_t	*pde;
	int	changed_entry;
	caddr_t	vaddr;
	pgno_t	page_num;
	int	ref_count = 0;
	int	is_anon = (c->pf_flags & P_ANON);
	int	need_ownership = 0;
	int	color = colorof(uvaddr);
	int	vmflags = 0;
	caddr_t	pvaddr;
	pgno_t	sizein;
	pgno_t	sizeout;
	int	s;

	/*
	 * If this is a file page, set the native color to the file page
	 * color, if not already set.  If set, assure that the native color
	 * matches the file page color.
	 */
	old_color = pfd_to_vcolor(c);
	if (!is_anon) {
		if (old_color < 0) {
			old_color = vcache_color(c->pf_pageno);
			pfd_set_vcolor(c,old_color);
		} else if (old_color != vcache_color(c->pf_pageno)) {
#if DEBUG || defined(_VCE_AVOIDANCE_DEBUG)
#ifndef DEBUG
			if (showconfig)
#endif
			cmn_err(CE_WARN, "preg_color_validation: virtual color mismatch (%d, not %d) for page 0x%x",
				old_color,
				vcache_color(c->pf_pageno),
				pfdattopfn(c));
#endif
			slept |= color_validation(c, vcache_color(c->pf_pageno), 0, 0);
			if (slept == -1)
				return(-1);
			old_color = pfd_to_vcolor(c);
		}

		/*
		 * check if we need ownership (read-write mapping)
		 */
		if ((findattr(prp,uvaddr)->attr_prot & PROT_W) &&
		    (!(prp->p_reg->r_flags & RG_CW)))
			need_ownership = 1;

		pageuseinc(c);
		ASSERT(pfdat_is_held(c));

		/*
		 * If necessary, break any references with conflicting colors
		 */
		if (need_ownership &&
		    ((old_color != color) || (c->pf_flags & P_MULTI_COLOR)) &&
		    pfdat_held_musers(c)) {
			slept = pfd_wait_rawcnt(c,0);
			if (slept == -1) {
				pagefree(c);
				return(-1);
			}

			page_num = pfdattopfn(c);

			changed_entry = 0;
			s = splhi();
			vp = c->pf_vp;
			if (vp != NULL) {
			    for (nprp = vp->v_mreg;
				 nprp != (preg_t *)vp;
				 nprp = nprp->p_vchain) {
				if (prp == nprp)
				    continue;
				if (nprp_base >= nprp->p_offset &&
				    nprp_base < (nprp->p_offset +
						 nprp->p_pglen)) {
				    /*
				     * XXX need locking here?
				     */
				    vaddr = nprp->p_regva +
					    ctob(nprp_base - nprp->p_offset);
				    pvaddr = vaddr;
				    sizein = 1;
				    pde = pmap_probe(nprp->p_pmap, &pvaddr, &sizein, &sizeout);
				    if (pde == NULL || pde->pte.pg_pfn != page_num)
					continue;
				    ref_count++;
				    if (!pg_ishrdvalid(pde))
					continue;
#if DEBUG || defined(_VCE_AVOIDANCE_DEBUG)
				    if (colorof(vaddr) != old_color &&
					!(c->pf_flags & P_MULTI_COLOR))
#ifndef DEBUG
					if (showconfig)
#endif
					cmn_err(CE_WARN, "preg_color_validation: valid vaddr 0x%x does not have current color %d",
						vaddr, old_color);
#endif
				    pg_clrhrdvalid(pde);
				    changed_entry++;
				}
			    }
			}
			splx(s);
			if (changed_entry)
				tlbsync(0LL, allclr_cpumask, STEAL_PHYSMEM);

			ASSERT(pfdat_is_held(c));
			pagefree(c);

			if (c->pf_use > (ref_count + 1) &&
			    color != old_color &&
			    is_usermode &&
			    !need_ownership) {
				(void) chunk_vrelse(c->pf_vp,
						    ctob(c->pf_pageno),
						    ctob(c->pf_pageno + 1));
				slept = -1;
			}
		} else {
		    pagefree(c);
		}
	}
	if (color == old_color) {
		if (!(need_ownership && (c->pf_flags & P_MULTI_COLOR)))
			return(slept);
	} else if (old_color >= 0 && (!is_anon) && (!need_ownership))
		vmflags |= VM_OTHER_VACOLOR;

	return(slept | color_validation(c,color,0,vmflags));
}

/*
 * bp_color_validation
 *
 * is comparable (to preg_color_validation), but it applies
 * when a caller wants to map a page in a disk buffer cache
 * buffer (in the kernel address space).
 *
 * Make current virtual color for a page match its file page virtual color,
 * breaking any conflicting mappings.  Since any conflicting mappings must
 * read-only (in that mmap() prohibits read-write mappings for which the
 * virtual color not congruent to file page virtual color), we only need
 * to invalidate any other primary cache colors.  Moreover, we only have
 * to look for other mappings if P_MULTI_COLOR is set.
 */

int
bp_color_validation(
	pfd_t	*c)
{
	int	slept = 0;
	int	old_color;
	preg_t	*nprp;
	vnode_t *vp;
	pgno_t	nprp_base;
	pde_t	*pde;
	int	changed_entry;
	caddr_t	vaddr;
	pgno_t	rpn;
	pgno_t	page_num;
	int	ref_count = 0;
	int	color;
	int	vmflags = 0;
	caddr_t	pvaddr;
	pgno_t	sizein;
	pgno_t	sizeout;
	int	s;

	/*
	 * In the swap to/from file case, we get anon pages and
	 * ublock pages here and we don't want the swap file
	 * color for these.
	 */
	if (c->pf_flags & P_ANON || (c->pf_flags & P_HASH) == 0)
		return(0);

	/*
	 * This is a file page.  Set color to file color if not set.
	 */

	color = vcache_color(c->pf_pageno);
	old_color = pfd_to_vcolor(c);
	if (old_color < 0) {
		old_color = color;
		pfd_set_vcolor(c,color);
	}

	/*
	 * If color already matches, just return.
	 */
	if (color == old_color && !(c->pf_flags & P_MULTI_COLOR))
		return(0);

	/*
	 * If necessary, break any references with conflicting colors
	 */
	/* PRE-emption safe? to check the pf_use count
	 */

	pageuseinc(c);
	ASSERT(pfdat_is_held(c));

	if (pfdat_held_musers(c)) {
		slept = pfd_wait_rawcnt(c,vmflags);
		if (slept == -1) {
			pagefree(c);
			return(-1);
		}

		page_num = pfdattopfn(c);
		rpn = c->pf_pageno;

		changed_entry = 0;
		s = splhi();
		vp = c->pf_vp;
		if (vp != NULL) {
		    for (nprp = vp->v_mreg;
			 nprp != (preg_t *)vp;
			 nprp = nprp->p_vchain) {

			if (nprp->p_reg == NULL)
			    continue;

			nprp_base = rpn - offtoct(nprp->p_reg->r_fileoff);
			if (nprp_base >= nprp->p_offset &&
			    nprp_base < (nprp->p_offset + nprp->p_pglen)) {
			    /*
			     * XXX need locking here?
			     */
			    vaddr = nprp->p_regva + ctob(nprp_base - nprp->p_offset);
			    pvaddr = vaddr;
			    sizein = 1;
			    pde = pmap_probe(nprp->p_pmap, &pvaddr, &sizein, &sizeout);
			    if (pde == NULL || pde->pte.pg_pfn != page_num)
				continue;
			    ref_count++;
			    if (!pg_ishrdvalid(pde))
				continue;
#if DEBUG || defined(_VCE_AVOIDANCE_DEBUG)
			    if (colorof(vaddr) != old_color && !(c->pf_flags & P_MULTI_COLOR))
#ifndef DEBUG
				    if (showconfig)
#endif
				    cmn_err(CE_WARN, "bp_color_validation: valid vaddr 0x%x does not have current color %d", vaddr, old_color);
#endif
			    pg_clrhrdvalid(pde);
			    changed_entry++;
			}
		    }
		}
		splx(s);
		if (changed_entry)
			tlbsync(0LL, allclr_cpumask, STEAL_PHYSMEM);
	}
	ASSERT(pfdat_is_held(c));
	pagefree(c);

	return(slept | color_validation(c,color,0,vmflags));
}
int	color_validation_force_flush = 0;

/*
 * color_validation
 *
 * does the low level cache flushing required on a change of color.
 */
int
color_validation(
	pfd_t	*pfd,
	int	color,
	int	flags,
	int	vmflags)
{
	int	old_color, slept;
	caddr_t	phys;
	int	pfn;

	slept = 0;
	old_color = pfd_to_vcolor(pfd);
	if (old_color != color ||
	    ((pfd->pf_flags & P_MULTI_COLOR) && !(vmflags & VM_OTHER_VACOLOR))) {
		if (old_color >= 0 && (private.p_scachesize == 0 ||
#ifdef R4600SC
		    two_set_pcaches ||
#endif /* R4600SC */
		    color_validation_force_flush)) {
			pfn = pfdattopfn(pfd);
			color_validation_nflush++;

			ASSERT(old_color <= cachecolormask);

			flags = (CACH_WBACK | CACH_VADDR | CACH_PRIMARY | CACH_AVOID_VCES);
			if (!(vmflags & VM_OTHER_VACOLOR)) {
				flags |= CACH_INVAL;
				if ((pfd->pf_flags & P_MULTI_COLOR) &&
				    old_color == color)
					flags |= CACH_OTHER_COLORS;
			}
			phys = page_mapin_pfn(pfd, (VM_VACOLOR | (vmflags & VM_NOSLEEP)),
					      old_color, pfn);
			if (phys == NULL) {
				color_validation_full_nflush++;
				flush_cache();
			} else {
				cache_operation(phys, NBPC, flags);
				page_mapout(phys);
			}
		}
		if (vmflags & VM_OTHER_VACOLOR) {
			if (color != old_color)
				pageflags(pfd,P_MULTI_COLOR,1);
		} else {
			if (pfd->pf_flags & P_MULTI_COLOR)
				pageflags(pfd,P_MULTI_COLOR,0);
			pfd_set_vcolor(pfd,color);
		}
	}

	return (slept);
}

#endif /* _VCE_AVOIDANCE */

#ifdef _MEM_PARITY_WAR
#include "os/proc/pproc_private.h"

#if CELL
#error BOMB!! _MEM_PARITY_WAR not virtualized.
#endif
/*
 * apply_to_pdes
 */

STATIC int apply_to_pregion_pdes(
	pas_t *,
	struct proc *,
	struct pregion *,
	pfn_t,
	int (*) (pas_t *, pfn_t, struct proc *, struct pregion *, char *, pde_t *, void *),
	void *);

int
apply_to_process_pdes(
	pfn_t		pfn,
	struct proc	*pp,
	int		(*fn) (pas_t *, pfn_t, struct proc *, struct pregion *, char *, pde_t *, void *),
	void		*fn_arg)
{
	preg_t	*prp;
	int	status = 0;
	vasid_t	vasid;
	pas_t	*pas;
	ppas_t	*ppas;

	/*
	 * The locking here is a real mess - this just mimics what was
	 * there and certainly doesn't work for MP machines ...
	 * For current process we lock EXCL, for others we do NO locking!
	 */
	if (pp == curprocp) {
		as_lookup_current(&vasid);
		VAS_LOCK(vasid, AS_EXCL);
	} else {
		/*
		 * Make sure we see a consistent picture of prxy_threads.
		 * An as_lookup is not strictly needed as the uthread can
		 * not release its vm any more after the uscan_hold.
		 */
		uscan_hold(&pp->p_proxy);
		if ((prxy_to_thread(&pp->p_proxy) == NULL) ||
		    as_lookup(prxy_to_thread(&pp->p_proxy)->ut_asid, &vasid)) {
			uscan_rele(&pp->p_proxy);
			return(1);
		}
	}
	pas = VASID_TO_PAS(vasid);
	ppas = (ppas_t *)vasid.vas_pasid;

	for (prp = PREG_FIRST(ppas->ppas_pregions); prp; prp = PREG_NEXT(prp)) {
		status = apply_to_pregion_pdes(pas, pp, prp, pfn, fn, fn_arg);
		if (status)
			break;
	}

	if (status == 0) {
		for (prp = PREG_FIRST(pas->pas_pregions);
		     prp; prp = PREG_NEXT(prp)) {
			status = apply_to_pregion_pdes(pas, pp, prp, pfn, fn, fn_arg);
			if (status)
				break;
		}
	}

	if (pp == curprocp)
		VAS_UNLOCK(vasid);
	else {
		as_rele(vasid);
		uscan_rele(&pp->p_proxy);
	}
	return(status);
}

struct pdearg {
	pfn_t	pfn;
	int	(*fn) (pas_t *, pfn_t, proc_t *, struct pregion *, char *, pde_t *, void *);
	void	*fn_arg;
};

int
proc_apply_to_pdes(
	proc_t	*p,
	void	*arg,
	int	ctl)
{
	struct pdearg *a;
	int	status;

	if (ctl == 1) {
		a = (struct pdearg *)arg;
		status = apply_to_process_pdes(a->pfn, p, a->fn, a->fn_arg);
		if (status)
			return(status);
	}
	return(0);
}

int
apply_to_pdes(
	pfn_t	pfn,
	int	(*fn) (pas_t *, pfn_t, struct proc *, struct pregion *, char *, pde_t *, void *),
	void	*fn_arg)
{
	struct pdearg	parg;
	int	status;

	/*
	 * Go through all active processes
	 */
	parg.pfn = pfn;
	parg.fn = fn;
	parg.fn_arg = fn_arg;
	status = procscan(proc_apply_to_pdes, &parg);
	return(status);
}

STATIC int
apply_to_pregion_pdes(
	pas_t		*pas,
	struct proc	*pp,
	struct pregion	*prp,
	pfn_t		pfn,
	int		(*fn) (pas_t *, pfn_t, struct proc *, struct pregion *, char *, pde_t *, void *),
	void		*fn_arg)
{
	pde_t		*pt;
	int		i;
	auto pgno_t	sz, totsz;
	auto char	*vaddr;
	int		status;

	/* Look at all of the mappings in the pregion
	 */

	vaddr = prp->p_regva;
	totsz = prp->p_pglen;

	while (totsz) {
		pt = pmap_probe(prp->p_pmap, &vaddr, &totsz, &sz);
		if (pt == NULL)
			return(0);

		totsz -= sz;

		for (i = sz; i > 0; vaddr += NBPP, pt++, i --) {
			if (!pg_isvalid(pt) || pdetopfn(pt) != pfn)
				continue;
			status = (*fn)(pas,pfn,pp,prp,vaddr,pt,fn_arg);
			if (status)
				return(status);
		}
	}
	return(0);
}

#endif /* _MEM_PARITY_WAR */

/* LARGE PAGE FAULT HANDLING CODE.*/

/*
 * Return true if the range can support a large page. If not return FALSE.
 * Since there will be only one tlb which will be mapping a range of
 * base size pages we have to ensure that the protections and cc bits
 * for all the base pages covered by the large page are the same.
 * If the pregion does not span 2* page_size we use base page even though
 * it is possible to have a large page  at the even address and nothing is
 * mapped at the odd address. The disadvantage of this is that when a new
 * region is created at the odd address we have to check to see if the
 * adjacent even address contains a large page and downgrade it. This check
 * will have to be done every time when a new region or attribute is added
 * or changed and thus will affect the performance of code for base page size.
 * even if large pages are not used.
 */
STATIC int
lpage_validate(
	pas_t	*pas,
	uvaddr_t vaddr,
	attr_t	*attr,
	preg_t	*prp,
	size_t	start_page_size)
{
	uvaddr_t lpage_start_addr, end_addr;
	int	large_page_fault;
	mprot_t	page_prot;
	uint	page_cc;
	size_t	desired_psize;
	size_t	pm_pagesize = PMAT_GET_PAGESIZE(attr);

	desired_psize = start_page_size;
	while (desired_psize > NBPP) {
		lpage_start_addr = LPAGE_ALIGN_ADDR(vaddr, NUM_TRANSLATIONS_PER_TLB_ENTRY * desired_psize);
		end_addr = lpage_start_addr + NUM_TRANSLATIONS_PER_TLB_ENTRY * desired_psize;

		/*
		 * The attributes should be the same for the entire range of
		 * virtual addresses covered by the large page. We do a fast
		 * path if a single attribute covers the whole range. We also
		 * have to make sure that the address range does not cross
		 * pregions.
		 */
		large_page_fault = TRUE;
		if ((lpage_start_addr >= attr->attr_start) &&
		    (attr->attr_end >= end_addr) &&
		    (PMAT_GET_PAGESIZE(attr) >= desired_psize) &&
		    (!(attr->attr_watchpt)))
			large_page_fault = TRUE;
		else
		if ((prp->p_regva > lpage_start_addr) ||
		    ((prp->p_regva + ctob(prp->p_pglen)) < end_addr))
			large_page_fault = FALSE;
		else {
			/*
			 * We are calling findattr_range_noclip() as we don't
			 * want the attr structures to be duplicated for every
			 * large page. We want to test the range that
			 * covers both the even and odd entries of the tlb.
			 */
			attr = findattr_range_noclip(prp, lpage_start_addr, end_addr);

			page_prot = attr->attr_prot;
			page_cc	= attr->attr_cc;

			for (;; attr = attr->attr_next) {
				ASSERT(attr);
				if ((PMAT_GET_PAGESIZE(attr) < desired_psize) ||
				    (page_prot != attr->attr_prot) ||
				    (page_cc != attr->attr_cc)) {

					large_page_fault = FALSE;
					break;
				}

				/*
				 * If the range has watchpoints don't use
				 * large pages.
				 */
				if (attr->attr_watchpt) {
					large_page_fault = FALSE;
					break;
				}
				if (attr->attr_end >= end_addr)
					break;
			}
		}
		if (large_page_fault)
			break;
		desired_psize = lpage_lower_pagesize(desired_psize);
	}

	if (desired_psize == pm_pagesize)
		return desired_psize;

	pmap_downgrade_page_size(pas, vaddr, prp, start_page_size, desired_psize);
	return desired_psize;
}


/*
 * Tests if a large page can be faulted at the address lpage_start_addr. It loops
 * through each base page of the large page. The following conditions must be
 * true if the page must be a large page.
 * a. If ptes have already been faulted and the page size == desired_psize
 * b. All ptes should have the same bits (valid, ref bit etc.) set.
 * c. If ptes are not faulted in, and the pages exist they should be contiguos
 * d. The buddy page for this address (even or odd tlb entry) should also be a
 *    large page or should not exist at all. This is a architectural
 *    requirement on r4000 and r10000 in which each tlb entry is a pair and
 *    both the odd and even tlb entries should be of the same page size.
 * e. If no pages are present and it is an anon page, we try to allocate a
 *    large page.
 * The fault info list has an entry for each base page of the large page. Each
 * fault info entry records the work done by this routine for that page. The
 * entries are used by vfault() in faulting in the base pages. For example,
 * if a pfind() is done by this routine, the FLT_PFIND flag is set in the
 * corresponding entry and the pfd is stored in the entry. vfault() checks
 * for this flag and avoids doing a pfind.
 */

STATIC int
lpage_vfault_test(
	pas_t		*pas,
	uthread_t	*ut,
	uvaddr_t	vaddr,
	preg_t		*prp,
	size_t		desired_psize,
	faultdata_t	*faultdata)
{
	uvaddr_t	lpage_start_addr;
	pgno_t		rpn;
	pgno_t		start_rpn;
	pgno_t		start_apn;
	pgno_t		apn;
	pgno_t		npgs;
	pde_t		*pt, *pd;
	pfn_t		pfn;
	fault_info_t	*flt_infop;
	pfd_t		*prev_pfd = (pfd_t *)0;
	reg_t		*rp;
	pfd_t		*lpfd;
	int		i;
	int		num_pages_hrd_valid = 0;
	int		num_pages_soft_valid = 0;
	int		num_pages_on_swap = 0;
	attr_t		*attr;
	uvaddr_t	cur_addr;
	off_t		fileoff;
	size_t		allocated_psize;
	fault_info_t	*start_flt_infop;
	int		wait_timeout;
	/* Number of pages of the large page found in the page cache */
	int		num_pages_in_page_cache = 0;

	npgs = btoct(desired_psize);

	if (npgs > MAX_LOCAL_FAULT_ENTRIES)
		start_flt_infop = faultdata->fd_large_list;
	else
		start_flt_infop = faultdata->fd_small_list;

	flt_infop = start_flt_infop;
	lpage_start_addr = LPAGE_ALIGN_ADDR(vaddr, desired_psize);

	rpn = vtorpn(prp, lpage_start_addr);

	start_rpn = rpn;
	start_apn = rpntoapn(prp, rpn);

	pt = pmap_ptes(pas, prp->p_pmap, lpage_start_addr, &npgs, VM_NOSLEEP);

	/*
	 * Note that the page table should exist. The pt was allocated
	 * at the beginning of vfault. Also note that all the ptes of a large
	 * page should be in the same page table.
	 */
	ASSERT(pt);
	ASSERT(npgs == btoct(desired_psize));

	apn = start_apn;

	rp = prp->p_reg;

	attr = findattr_range_noclip(prp, lpage_start_addr, lpage_start_addr + desired_psize);
	cur_addr = lpage_start_addr;

	if (rp->r_type == RT_MAPFILE && rp->r_flags & RG_AUTOGROW)
		return USE_BASE_PAGE;


	for (pd = pt, i = 0;
	     i < npgs;
	     pd++, flt_infop++, i++, rpn++, cur_addr += NBPP) {

		fileoff = ctob(rpn) + rp->r_fileoff;

		apn = rpntoapn(prp, rpn);

		if (attr->attr_end < cur_addr)
			attr = attr->attr_next;

		pfn = pg_getpfn(pd);
		flt_infop->pd = pd;

		/*
		 * The pte lock is not acquired here as the pages cannot
		 * be migrated. Migration checks if the pte's page size > NBPP.
		 * The code below punts large pages if the pte is a base page
		 * pte.
		 */
		if (pfn) {

			flt_infop->pfd = pfntopfdat(pfn);

			if (pfn <= PG_SENTRY) {
				LPG_FLT_TRACE(FLT_FAIL_SENTRY, i, pd, flt_infop, lpage_start_addr + ctob(i));
				return (vfault_wait_sentry(pas, pd, rp));
			}

			if (pg_get_page_mask_index(pd) != PAGE_SIZE_TO_MASK_INDEX(desired_psize)) {
				LPG_FLT_TRACE(FLT_FAIL_HDR_VALID, i, pd, flt_infop, lpage_start_addr + ctob(i));
				return USE_BASE_PAGE;
			}

			if (pg_ishrdvalid(pd)) {
				num_pages_hrd_valid++;
				continue;
			}

			if (pg_isvalid(pd)) {
				flt_infop->flags |= FLT_SOFT_VALID;
				num_pages_soft_valid++;
			}

		} else { /* No pfn */
			if (rp->r_anon) {
				flt_infop->pfd = anon_pfind(rp->r_anon, apn,
						&flt_infop->anon_handle,
						&flt_infop->swap_handle);

				LPG_FLT_TRACE(FLT_ANON_PFIND, flt_infop->pfd, flt_infop->anon_handle, flt_infop->swap_handle, 0);

				if ((flt_infop->pfd == NULL) &&
				    flt_infop->anon_handle &&
				    flt_infop->swap_handle) {
					num_pages_on_swap++;
				}
				ASSERT((!(flt_infop->anon_handle)) ||
					(flt_infop->pfd) ||
					(flt_infop->swap_handle));
			}

			if ((!(flt_infop->anon_handle)) &&
			    rp->r_vnode && (fileoff < rp->r_maxfsize)) {
				ASSERT(flt_infop->pfd == NULL);
				ASSERT(flt_infop->swap_handle == NULL);
				/*
				 * Always fail the vn_pfind when the
				 * VREMAPPING bit is set.  This
				 * keeps us from grabbing references to
				 * pages in between calls to remapf() and
				 * vnode_flushinval_pages() or
				 * vnode_tosspages().
				 */
				if (rp->r_vnode->v_flag & VREMAPPING) {
					flt_infop->pfd = NULL;
				} else {
					flt_infop->pfd =
						vn_pfind((void *)rp->r_vnode,
						rpn + offtoct(rp->r_fileoff),
						VM_ATTACH,
						(void *)attr_pm_get(attr));
					if (flt_infop->pfd)
						ASSERT(flt_infop->pfd->pf_use > 0);
				}
			}

			flt_infop->flags |= FLT_PFIND;
		}

		/*
		 * If this is the first pfdat ensure that it is correctly aligned.
		 */
		if ((i == 0) && (flt_infop->pfd)) {
			lpfd = pfdat_to_large_pfdat(flt_infop->pfd, npgs);
			if (lpfd != flt_infop->pfd) {
				LPG_FLT_TRACE(FLT_FAIL_PFD_ALIGN, flt_infop->pfd, prp, lpage_start_addr + ctob(i), 0);
				return USE_BASE_PAGE;
			}
		}

		/*
		 * Make sure pfdats are contiguous.
		 */
		if ((i > 0) && (flt_infop->pfd || prev_pfd) &&
		    (flt_infop->pfd != (prev_pfd + 1))) {

			LPG_FLT_TRACE(FLT_FAIL_PFD, prev_pfd, flt_infop->pfd, flt_infop, lpage_start_addr + ctob(i));
			return USE_BASE_PAGE;
		}

		prev_pfd = flt_infop->pfd;

		if (prev_pfd) {
			if (!(prev_pfd->pf_flags & P_DONE)) {

				LPG_FLT_TRACE(FLT_FAIL_PDONE, flt_infop->pfd, flt_infop, lpage_start_addr + ctob(i), prev_pfd->pf_use);

				return (vfault_wait_pagedone(pas, prev_pfd, rp));
			}
			num_pages_in_page_cache++;
		}
	}

	if (pmap_check_lpage_buddy(pas, lpage_start_addr, prp, desired_psize) == SMALL_PAGES_PRESENT) {
		LPG_FLT_TRACE(FLT_BUDDY_SMALL, lpage_start_addr, prp, desired_psize, 0);
		return USE_BASE_PAGE;
	}

	if ((num_pages_hrd_valid  + num_pages_soft_valid) == npgs)
		return USE_LARGE_PAGE;
	if ((num_pages_hrd_valid  + num_pages_soft_valid) > 0)
		return USE_BASE_PAGE;


	/*
	 * If we can find all the pages in the page cache and they are all
	 * contiguous then the goal is accomplished. Return USE_LARGE_PAGE.
	 */
	if (num_pages_in_page_cache == npgs) {

		if (rp->r_type == RT_MAPFILE && rp->r_flags & RG_AUTOGROW)
			return USE_BASE_PAGE;

		LPG_FLT_TRACE(FLT_SUCC_PFD, num_pages_in_page_cache, flt_infop, npgs, 0);
		return USE_LARGE_PAGE;
	}
	if (num_pages_in_page_cache > 0) {
		return USE_BASE_PAGE;
	}

	/*
	 * At this point there are no pages in the page cache. So
	 * we have to allocate a large page. This can be done only for
	 * anon pages. For vnodes the buffer cache allocates pages.
	 * So we punt here. We avoid doing the downgrade for the buddy
	 * large page as the buffer cache might succeed in allocating a large
	 * page for this range. We force a FILE IO here. vfault() will be
	 * force to do a VOP_READ() and will retry the whole fault.
	 */
	fileoff = ctob(start_rpn) + rp->r_fileoff;

	if (rp->r_vnode && (fileoff < rp->r_maxfsize)) {
		LPG_FLT_TRACE(FLT_FAIL_VNODE, npgs, flt_infop, rp, rp->r_vnode);
		flt_infop = start_flt_infop + btoct(vaddr - lpage_start_addr);
		flt_infop->flags |= FLT_DO_FILE_IO;
		return USE_BASE_PAGE_SKIP_DOWNGRADE;
	}

	/*
	 * All pages on swap. Try allocating a large page and insert them
	 * in the page cache. If some pages are on swap and some are not
	 * then use base page size.
	 */
	if (num_pages_on_swap == npgs)
		return (lpage_swapin(pas, ut, faultdata, npgs, prp,
					lpage_start_addr));
	else if (num_pages_on_swap > 0)
			return USE_BASE_PAGE;

	/*
	 * Allocate a large page.
	 */
	allocated_psize = desired_psize;

#ifdef	_VCE_AVOIDANCE
	lpfd= vce_avoidance ?
		PMAT_PAGEALLOC(attr, colorof(lpage_start_addr), VM_VACOLOR| VM_MVOK,
				&allocated_psize, lpage_start_addr)
		: PMAT_PAGEALLOC(attr, vcache2(prp, attr, start_apn), VM_MVOK,
				&allocated_psize, lpage_start_addr);
#else
	lpfd = PMAT_PAGEALLOC(attr, vcache2(prp, attr, start_apn), VM_MVOK,
				&allocated_psize, lpage_start_addr);
#endif

	/*
	 * We cannot allocate a page.
	 */
	if (lpfd == NULL) {
		wait_timeout = PM_PAGE_ALLOC_WAIT_TIMEOUT(attr->attr_pm);
		if (wait_timeout && faultdata->fd_flags & FD_PAGE_WAIT) {
			faultdata->fd_flags &= ~FD_PAGE_WAIT;
			if (vfault_wait_lpage(pas, rp, desired_psize, wait_timeout)) {
				INC_LPG_STAT(private.p_nodeid, PAGE_SIZE_TO_INDEX(desired_psize), LPG_VFAULT_RETRY_LOWER_PGSZ);
				return RETRY_LOWER_PAGE_SIZE;
			} else {
				INC_LPG_STAT(private.p_nodeid, PAGE_SIZE_TO_INDEX(desired_psize), LPG_VFAULT_RETRY_SAME_PGSZ);
				return RETRY_FAULT;
			}
		} else {
			LPG_FLT_TRACE(FLT_FAIL_BREAK, npgs, flt_infop, prp, 0);
			INC_LPG_STAT(private.p_nodeid, PAGE_SIZE_TO_INDEX(desired_psize), LPG_PAGEALLOC_FAIL);
			return USE_BASE_PAGE;
		}
	}

	/*
	 * If pagealloc cannot allocate a large page it allocates a base
	 * page instead. So put the information into the appropriate
	 * flt_infop.
	 */
	if (allocated_psize == NBPP) {
		flt_infop = start_flt_infop + btoct(vaddr - lpage_start_addr);
		flt_infop->pfd = lpfd;
		flt_infop->flags |= FLT_PGALLOC_DONE;
		INC_LPG_STAT(private.p_nodeid, PAGE_SIZE_TO_INDEX(desired_psize), LPG_PAGEALLOC_ONEP);
		LPG_FLT_TRACE(FLT_FAIL_NBPP, npgs, flt_infop, prp, 0);

		return USE_BASE_PAGE;
	}
	ASSERT(allocated_psize == desired_psize);

	flt_infop = start_flt_infop;

	for (i = 0; i < npgs; i++) {
		flt_infop->pfd = lpfd++;
		flt_infop->flags |= FLT_PGALLOC_DONE;
		flt_infop++;
	}

	INC_LPG_STAT(private.p_nodeid, PAGE_SIZE_TO_INDEX(desired_psize), LPG_PAGEALLOC_SUCCESS);
	LPG_FLT_TRACE(FLT_SUCC_NEWPFD, lpfd, lpage_start_addr, prp, flt_infop);

	return USE_LARGE_PAGE;
}

/*
 * This routine checks to see if a large page can be faulted in for the
 * virtual address 'lpage_start_addr. npgs is number of base pages in the
 * large page.
 * If a large page can be faulted, npgs is not changed and  the fault info
 * entries for all the pages that are to be faulted is returned.
 * If a large page cannot be faulted, npgs is set to 1 which indicates a base
 * page needs to be faulted in. The flt_infop entry that is returned corresponds
 * to the base page for the virtual address vaddr. The routine also breaks
 * down the buddy for the large page if one exists. The fault info list
 * that is passed in has space only for a limited number of entries as it is
 * allocated on the stack in vfault. If the number of base pages in the large
 * page exceeds this size, we allocate a fresh list using kmem_zalloc(). This
 * is a performance enhancment as most faults will be for small pages (4 times
 * base page size) and very large page faults will be few in number.
 */
STATIC int
lpage_vfault(
	pas_t		*pas,
	uthread_t	*ut,
	uvaddr_t	vaddr,
	preg_t		*prp,
	pgcnt_t		*npgs,
	faultdata_t	*faultdata,
	fault_info_t	**cur_flt_infop)
{
	fault_info_t	*flt_infop, *start_flt_infop;
	size_t		desired_psize = ctob(*npgs);
	caddr_t		lpage_start_addr;
	int		ret;
	int		i;

	lpage_start_addr = LPAGE_ALIGN_ADDR(vaddr, desired_psize);

	faultdata->fd_large_list = NULL;

	if ((*npgs) > MAX_LOCAL_FAULT_ENTRIES) {
		start_flt_infop = kmem_zalloc((*npgs) * sizeof(fault_info_t), KM_SLEEP);
		faultdata->fd_large_list = start_flt_infop;
	} else  {
		start_flt_infop = faultdata->fd_small_list;
		bzero(start_flt_infop, MAX_LOCAL_FAULT_ENTRIES * sizeof(fault_info_t));
	}

	ret = lpage_vfault_test(pas, ut, vaddr, prp, desired_psize, faultdata);

	if (ret  == USE_LARGE_PAGE) {
		*cur_flt_infop = start_flt_infop;

	} else if (ret == USE_BASE_PAGE || ret == USE_BASE_PAGE_SKIP_DOWNGRADE) {
		/*
		 * Compute the right flt_info entry for the faulted page.
		 */
		*cur_flt_infop = start_flt_infop  + (btoct(vaddr - lpage_start_addr));

		/*
		 * As pfind was called with VM_ATTACH the pages will have
		 * acquired an extra referenced. Free the unused pages here.
		 */
		flt_infop  = start_flt_infop;
		for (i = 0; i < (*npgs); i++, flt_infop++)
			if (((flt_infop->flags & FLT_PFIND) == FLT_PFIND) &&
			    (flt_infop->pfd) &&
			    (flt_infop != *cur_flt_infop)) {
				ASSERT(flt_infop->pfd->pf_use > 0);
				pagefreeanon(flt_infop->pfd);
			}

		/*
		 * If the fault info structures was allocated, then free it
		 * here after copying the fault info values into the list
		 * allocated on the stack.
		 */
		flt_infop = start_flt_infop;
		if ((*npgs) > MAX_LOCAL_FAULT_ENTRIES) {
			start_flt_infop = faultdata->fd_small_list;

			/*
			 * Structure copy.
			 */
			(*start_flt_infop) = (**cur_flt_infop);
			*cur_flt_infop = start_flt_infop;
			kmem_free(flt_infop, (*npgs) * sizeof(fault_info_t));
			faultdata->fd_large_list = NULL;
		}

		/*
		 * Break down the buddy if one exists.
		 */
		LPG_FLT_TRACE(VFAULT_LPG_FAILURE, vaddr, *cur_flt_infop, 0, (*cur_flt_infop)->flags);
		if (ret == USE_BASE_PAGE)
			pmap_downgrade_range_to_base_page_size(pas, lpage_start_addr,
				 prp, desired_psize);
		*npgs = btoct(NBPP);
	} else { /* Retry fault */
		/*
		 * As pfind was called with VM_ATTACH the pages will have
		 * acquired an extra referenced. Free the unused pages here.
		 */
		flt_infop  = start_flt_infop;
		for (i = 0; i < (*npgs); i++, flt_infop++)
			if (((flt_infop->flags & FLT_PFIND) == FLT_PFIND) &&
			    (flt_infop->pfd)) {
				ASSERT(flt_infop->pfd->pf_use > 0);
				pagefreeanon(flt_infop->pfd);
			}

		if ((*npgs) > MAX_LOCAL_FAULT_ENTRIES) {
			kmem_free(start_flt_infop, (*npgs) * sizeof(fault_info_t));
			faultdata->fd_large_list = NULL;
		}
	}
	return ret;
}

/*
 * Free any pages that have been allocated but not used.
 * Also release references to any pfdats which have been
 * held due to vn_pfind(.., VM_ATTACH).
 */
STATIC void
lpage_fault_recover(
	pas_t		*pas,
	fault_info_t	*start_flt_infop,
	caddr_t		vaddr,
	pgcnt_t		npgs)
{
	int		i;
	fault_info_t	*flt_infop;

	LPG_FLT_TRACE(FAULT_LPG_RECOVER, start_flt_infop, npgs, 0, 0);
	flt_infop = start_flt_infop;
	pmap_downgrade_lpage_pte(pas, vaddr, flt_infop->pd);
	for (i = 0; i < npgs; i++, flt_infop++) {
		if (!(flt_infop->flags & FLT_DONE))  {
			if (flt_infop->flags & (FLT_PGALLOC_DONE|FLT_PFIND)) {
				ASSERT((flt_infop->flags & FLT_PGALLOC_DONE) &&
					(!(flt_infop->pfd->pf_flags & P_ANON)));
				pagefreeanon(flt_infop->pfd);
			}
		}
	}
	if (npgs > MAX_LOCAL_FAULT_ENTRIES)
		kmem_free(start_flt_infop, npgs * sizeof(fault_info_t));
}

/*
 * Large page pfault checks. This routine checks if a large page can be
 * pfaulted. In case of failure downgrade down the even and odd large page
 * pte entries to the base page size. Returns the number of base pages that
 * need to be faulted.
 */
STATIC int
lpage_pfault(
	pas_t		*pas,
	caddr_t		vaddr,
	preg_t		*prp,
	size_t		desired_psize,
	faultdata_t	*faultdata,
	int		*npgs_to_fault)
{
	pgno_t		rpn;
	pgno_t		start_apn;
	pgno_t		npgs;
	pde_t		*pt, *pd;
	pfn_t		pfn;
	fault_info_t	*flt_infop;
	pfd_t		*pfd;
	reg_t		*rp;
	pfd_t		*lpfd;
	int		i;
	fault_info_t	*start_flt_infop;
	int		num_pages_valid = 0;	/* Number of valid pages */
	int		num_cow_pages = 0;	/* Number of  COW pages */
	caddr_t		lpage_start_addr;
	size_t		allocated_psize;
	/* REFERENCED */
	attr_t		*attr;
	int		wait_timeout;

	INC_LPG_STAT(private.p_nodeid, PAGE_SIZE_TO_INDEX(desired_psize), LPG_PFAULT_ATT);

	lpage_start_addr = LPAGE_ALIGN_ADDR(vaddr, desired_psize);

	rpn = vtorpn(prp, lpage_start_addr);
	start_apn = rpntoapn(prp, rpn);

	npgs = btoct(desired_psize);

	attr = findattr(prp, lpage_start_addr);

	pt = pmap_ptes(pas, prp->p_pmap, lpage_start_addr, &npgs, VM_NOSLEEP);

	if (pg_get_page_mask_index(pt) > PAGE_SIZE_TO_MASK_INDEX(desired_psize)) {
		pmap_downgrade_pte_to_lower_pgsz(pas, lpage_start_addr, pt, desired_psize);
	}

	ASSERT(npgs == btoct(desired_psize));
	if (npgs > MAX_LOCAL_FAULT_ENTRIES) {
		flt_infop = kmem_zalloc(npgs*sizeof(fault_info_t), KM_SLEEP);
		faultdata->fd_large_list = flt_infop;
	} else {
		flt_infop = faultdata->fd_small_list;
		bzero(flt_infop, MAX_LOCAL_FAULT_ENTRIES*sizeof(fault_info_t));
		faultdata->fd_large_list = NULL;
	}

	start_flt_infop = flt_infop;

	rp = prp->p_reg;
	for (pd = pt, i = 0; i < npgs; pd++, flt_infop++, i++) {
		pfn = pg_getpfn(pd);
		flt_infop->pd = pd;
		flt_infop->flags = 0;
		pfd = pfntopfdat(pfn);

		/*
		 * All pages should already be valid and the right size.
		 */
		if (pg_ishrdvalid(pd)) {
			if (pg_get_page_mask_index(pd) != PAGE_SIZE_TO_MASK_INDEX(desired_psize)) {
				LPG_FLT_TRACE(FLT_FAIL_HDR_VALID, i, pd, flt_infop, lpage_start_addr + ctob(i));

				*npgs_to_fault = btoct(NBPP);
				return USE_BASE_PAGE;
			}
			num_pages_valid++;
		} else
			return USE_BASE_PAGE;

		if (!(rp->r_flags & RG_CW)) {
			if ((pfd->pf_tag == (void *)rp->r_vnode) && (pfd->pf_flags & P_HOLE)) {
				LPG_FLT_TRACE(FLT_PAGE_HOLE, rp, 0, 0, flt_infop);

				pmap_downgrade_to_base_page_size(pas, lpage_start_addr, prp, desired_psize, PMAP_TLB_FLUSH);
				*npgs_to_fault = btoct(NBPP);
				return USE_BASE_PAGE;
			}
		} else {
			/* Test for COW */
			if (do_cow_fault(pfd, rp, pfdat_is_held(pfd), pfd->pf_tag == anon_tag(rp->r_anon)))
				num_cow_pages++;

		}
	}

	ASSERT(pmap_check_lpage_buddy(pas, lpage_start_addr, prp, desired_psize) != SMALL_PAGES_PRESENT);
	ASSERT(num_pages_valid == npgs);

	if (!(rp->r_flags & RG_CW)) {
		LPG_FLT_TRACE(FLT_NON_COW,rp, 0, 0, 0);
		*npgs_to_fault = npgs;
		return USE_LARGE_PAGE;
	}

	/*
	 * This is a COW region but we need not do a COW.
	 * Just set the mod bit on the ptes.
	 */
	if (!num_cow_pages) {
		*npgs_to_fault = npgs;
		return USE_LARGE_PAGE;
	}

	/*
	 * Only some pages need to be COW.
	 */
	if (num_cow_pages < npgs) {
		pmap_downgrade_to_base_page_size(pas, lpage_start_addr, prp, desired_psize, PMAP_TLB_FLUSH);
		*npgs_to_fault = btoct(NBPP);
		return USE_BASE_PAGE;
	}

	/*
	 * All pages need to be copied. So allocate a new large page.
	 */
	allocated_psize = desired_psize;

#ifdef	_VCE_AVOIDANCE
	lpfd= vce_avoidance ?
		PMAT_PAGEALLOC(attr, colorof(lpage_start_addr), VM_VACOLOR| VM_MVOK,
				&allocated_psize, lpage_start_addr)
		: PMAT_PAGEALLOC(attr, vcache2(prp, attr, start_apn), VM_MVOK,
				&allocated_psize, lpage_start_addr);
#else
	lpfd = PMAT_PAGEALLOC(attr, vcache2(prp, attr, start_apn), VM_MVOK,
				&allocated_psize, lpage_start_addr);
#endif

	/*
	 * We cannot allocate a page.
	 */
	if (lpfd == NULL) {
		wait_timeout = PM_PAGE_ALLOC_WAIT_TIMEOUT(attr->attr_pm);
		if (wait_timeout && faultdata->fd_flags & FD_PAGE_WAIT) {
			faultdata->fd_flags &= ~FD_PAGE_WAIT;
			if (vfault_wait_lpage(pas, rp, desired_psize, wait_timeout)) {
				INC_LPG_STAT(private.p_nodeid, PAGE_SIZE_TO_INDEX(desired_psize), LPG_PFAULT_RETRY_LOWER_PGSZ);
				*npgs_to_fault = 0;
				return RETRY_LOWER_PAGE_SIZE;
			} else {
				INC_LPG_STAT(private.p_nodeid, PAGE_SIZE_TO_INDEX(desired_psize), LPG_PFAULT_RETRY_SAME_PGSZ);
				*npgs_to_fault = 0;
				return RETRY_FAULT;
			}
		}
		LPG_FLT_TRACE(FLT_FAIL_BREAK, npgs, flt_infop, prp, 0);

		INC_LPG_STAT(private.p_nodeid, PAGE_SIZE_TO_INDEX(desired_psize), LPG_PAGEALLOC_FAIL);
		pmap_downgrade_to_base_page_size(pas, lpage_start_addr, prp, desired_psize, PMAP_TLB_FLUSH);
		*npgs_to_fault = btoct(NBPP);
		return USE_BASE_PAGE;
	}

	flt_infop = start_flt_infop;

	/*
	 * If we can allocate only one page add the pfd to the fault_info.
	 */
	if (allocated_psize == NBPP) {
		flt_infop += btoct(vaddr - lpage_start_addr);
		flt_infop->pfd = lpfd;
		flt_infop->flags |= FLT_PGALLOC_DONE;
		LPG_FLT_TRACE(FLT_FAIL_NBPP, npgs, flt_infop, prp, 0);
		pmap_downgrade_to_base_page_size(pas, lpage_start_addr, prp, desired_psize, PMAP_TLB_FLUSH);
		*npgs_to_fault = btoct(NBPP);
		return USE_BASE_PAGE;
	}

	for (i = 0; i < npgs; i++) {
		flt_infop->pfd = lpfd++;
		flt_infop->flags |= FLT_PGALLOC_DONE;
		flt_infop++;
	}

	LPG_FLT_TRACE(FLT_SUCC_NEWPFD, lpfd, lpage_start_addr, prp, flt_infop);
	INC_LPG_STAT(private.p_nodeid, PAGE_SIZE_TO_INDEX(desired_psize), LPG_PAGEALLOC_SUCCESS);

	*npgs_to_fault = npgs;
	return USE_LARGE_PAGE;
}

/*
 * Dropin the tlb to map a large page of a specific page size. Also switch the
 * utlbmiss handler for the process to support large page tlbmisses if its not
 * been done already. Note that vaddr is the actual address which was
 * faulted upon. So we pass vaddr to tlbdropin_lpgs() which overwrite that tlb
 * entry. There is an issue with sprocs. If a bunch of sprocs are running and
 * one of them drops in large page tlb entries it has to inform the other sprocs
 * to switch to the large page utlbmiss handler. Until then the other sprocs
 * will drop the large page tlb entries as base page tlb entries. The
 * communication is done by setting the UTAS_LPG_TLBMISS_SWTCH flag in the utas
 * structure. When an sproc/uthread encounters UTAS_LPG_TLBMISS_SWTCH
 * (in swtch or other
 * places where it can do a utlbmiss_resume), it flushes its tlb entries to
 * remove the 16k tlb entries. Future sprocs/uthreads do not have this problem
 * as the utlbmiss switch flags are inherited on fork.
 */
STATIC void
lpage_tlbdropin(
	pas_t		*pas,
	ppas_t		*ppas,
	uvaddr_t	vaddr,
	size_t		page_size,
	pde_t		*pd)
{
	int		s;
	pde_t		*buddy_pd;
	pde_t		*private_pd;
	ppas_t		*tppas;
	utas_t		*utas = ppas->ppas_utas;

	/*
	 * Pmap_probe parameters.
	 */
	uvaddr_t	tmp_vaddr;
	pgno_t		sizein;
	pgno_t		sizeout;

	ASSERT(mrislocked_access(&pas->pas_aspacelock));
	/*
	 * If another sproc/uthread has already dropped in large page tlbs
	 * then do a flush here.
	 */
	if (utas->utas_flag & UTAS_LPG_TLBMISS_SWTCH) {
		s = utas_lock(utas);
		if (utas->utas_flag & UTAS_LPG_TLBMISS_SWTCH) {
			utas->utas_flag &= ~UTAS_LPG_TLBMISS_SWTCH;
			LPG_FLT_TRACE(FLT_UTLBMISS_SWTCH, vaddr, tlbpid(utas), utas->utas_utlbmissswtch, 0);
			ASSERT(utas->utas_utlbmissswtch & UTLBMISS_LPAGE);
			utas_unlock(utas, s);
			utlbmiss_resume(utas);
			flushtlb_current();
		} else
			utas_unlock(utas, s);
	}

	if (!(utas->utas_utlbmissswtch & UTLBMISS_LPAGE)) {
		s = utas_lock(utas);
		/*
		 * Switch the utlbmiss handler.
		 */
		utas->utas_utlbmissswtch |= UTLBMISS_LPAGE;
		utas->utas_flag |= UTAS_TLBMISS;

		/*
		 * We are going to flush the tlb
		 * so just clear this flag if some other sproc/uthread set it.
		 */
		utas->utas_flag &= ~UTAS_LPG_TLBMISS_SWTCH;

		utas_unlock(utas, s);
		utlbmiss_resume(utas);

		/*
		 * If its a shared process, set the flag UTLBMISS_LPAGE in
		 * the utas_utlbmissswtch and also set UTAS_LPG_TLBMISS_SWTCH.
		 * UTAS_LPG_TLBMISS_SWTCH indicates that when this process is
		 * switched in, it will flush its tlb entries and use the
		 * large page tlb miss handler.
		 */
		if (pas->pas_flags & PAS_SHARED) {
			utas_t *tutas;
			/*
			 * It is possible that other sprocs have started
			 * but were not able to inform this process as it
			 * was not entered into the AS ppas list yet. So this
			 * process has to flush its 16k tlb
			 * entries out. This is the window between inheriting
			 * utas_flag flags and entering the AS in ppas list
			 */
			flushtlb_current();

			LPG_FLT_TRACE(FLT_SET_UTLBMISSWTCH, pas, vaddr, pas->pas_plist, 2);

			mraccess(&pas->pas_plistlock);
			for (tppas = pas->pas_plist; tppas; tppas = tppas->ppas_next) {
				if (tppas == ppas)
					continue;
				tutas = tppas->ppas_utas;
				s = utas_lock(utas);
				if (tutas->utas_utlbmissswtch & UTLBMISS_LPAGE) {
					ASSERT(tutas->utas_flag & UTAS_TLBMISS);
					LPG_FLT_TRACE(FLT_SET_UTLBMISSWTCH, pas, vaddr, tppas, 1);
					utas_unlock(utas, s);
					continue;
				}

				tutas->utas_flag |= (UTAS_LPG_TLBMISS_SWTCH|UTAS_TLBMISS);
				tutas->utas_utlbmissswtch |= UTLBMISS_LPAGE;
				utas_unlock(utas, s);
				LPG_FLT_TRACE(FLT_SET_UTLBMISSWTCH, pas, vaddr, tppas, 0);
			}
			mrunlock(&pas->pas_plistlock);
		}
	} else {
		ASSERT(utas->utas_flag & UTAS_TLBMISS);
		ASSERT(private.p_utlbmissswtch & UTLBMISS_LPAGE);
	}

	/*
	 * Align the virtual address to the nearest page boundary
	 * as tlbdropin_lpgs requires that the page offset is 0.
	 */
	vaddr = LPAGE_ALIGN_ADDR(vaddr, NBPP);

	/*
	 * For the PAS_SHARED case, if there is an overlap of local and shared
	 * mappings, the utlbmiss handler drops in a base page tlb
	 * entry on the prescence of a SENTINEL. Read comments for the
	 * lpg_overlay_utlbmiss handler. If such an sproc takes a vfault,
	 * we should just dropin a base page tlb entry. We do a probe here
	 * for that. Also if this is the first SENTINEL fault switch to
	 * the new utlbmiss handler.
	 */
	if ((pas->pas_flags & PAS_SHARED) && ppas->ppas_pmap) {
		tmp_vaddr = vaddr;
		sizein = 1;

		/*
		 * Do a probe of the private pmap to see if we are replacing
		 * a SENTINEL pte. In this case we just dropin a base page.
		 * The process's pmap is used as it always is the private pmap.
		 * This code assumes that there is only one private PMAP per
		 * process.
		 */
		private_pd = pmap_probe(ppas->ppas_pmap, &tmp_vaddr, &sizein, &sizeout);

		if (((private_pd != NULL) && (tmp_vaddr == vaddr)) &&
			(pg_getpfn(private_pd) == PDEFER_SENTRY)) {

			tmp_vaddr = LPAGE_ALIGN_ADDR(vaddr, page_size);
			pd += btoct(vaddr - tmp_vaddr);

			if (tlbdropin(utas->utas_tlbpid, vaddr, pd->pte) == SHRD_SENTINEL) {
				if (!(utas->utas_utlbmissswtch & UTLBMISS_DEFER)) {

					s = utas_lock(utas);
					utas->utas_utlbmissswtch |= UTLBMISS_DEFER;
					utas->utas_flag |= UTAS_TLBMISS;
					utas_unlock(utas, s);
					utlbmiss_resume(utas);
				} else {
					ASSERT(utas->utas_flag & UTAS_TLBMISS);
					ASSERT(private.p_utlbmissswtch & UTLBMISS_DEFER);
				}
			}
			return;
		}
	}

	ASSERT(validate_tlb_entry(vaddr, page_size));

	/*
	 * This vfault might have occurred on another cpu and so there can
	 * be an invalid (16K) tlb entry lurking on those cpus due to the
	 * fast locore tfault. This might conflict with a large page tlb
	 * entry. So flush them out by changing the tlb pids on all other cpus
	 * except the current cpu.
	 */

	/* no migration */
	s = splhi();

	tlbinval_skip_my_cpu(utas);

	pd = LARGE_PAGE_PTE_ALIGN(pd, NUM_TRANSLATIONS_PER_TLB_ENTRY * btoct(page_size));
	buddy_pd = pd + btoct(page_size);

	tlbdropin_lpgs(tlbpid(utas), vaddr, pd->pte, buddy_pd->pte, page_size);
	splx(s);
}

/*
 * Stub routine for low end systems. For now tlbdropin_lpgs uses register
 * to pass the fifth argument.  Currently large pages are not supported
 * for low end systems.
 */
#if	(!defined(PTE_64BIT))
/* ARGSUSED */
void
tlbdropin_lpgs(unsigned char pid, caddr_t addr, pte_t even, pte_t odd, uint page_size)
{
	ASSERT(0);
}
#endif

/*
 * This routine tries to swapin a large page. It first checks to see
 * if all pages are in the swap and allocates a large page and swaps all
 * the pages in. We might later use a special swap interface to swap a
 * large page in one I/O request It will return a failure if the swap
 * cannot be done and vfault tries to fault in a base page for the fault
 * address.
 */
STATIC int
lpage_swapin(
	pas_t		*pas,
	uthread_t	*ut,
	faultdata_t	*faultdata,
	pgcnt_t		npgs,
	preg_t		*prp,
	caddr_t		lpage_start_addr)
{
	fault_info_t	*flt_infop;
	fault_info_t	*start_flt_infop;
	int	num_pages_on_swap = 0;
	int	swapfail, i;
	pgno_t	start_rpn, rpn, apn, start_apn;
	reg_t	*rp = prp->p_reg;
	pglst_t	swaplst[1];
	pfd_t	*pfd, *lpfd;
	/* REFERENCED */
	size_t	page_size = ctob(npgs);
	size_t	allocated_psize;
	attr_t	*attr = findattr(prp, lpage_start_addr);
	int	wait_timeout;

	start_rpn = vtorpn(prp, lpage_start_addr);
	start_apn = rpntoapn(prp, start_rpn);

	if (npgs > MAX_LOCAL_FAULT_ENTRIES)
		start_flt_infop = faultdata->fd_large_list;
	else
		start_flt_infop = faultdata->fd_small_list;

	flt_infop = start_flt_infop;
	rpn = start_rpn;
	for (i = 0; i < npgs; i++, flt_infop++, rpn++) {

		apn = rpntoapn(prp, rpn);

		flt_infop->pfd = anon_pfind(rp->r_anon, apn,
						&flt_infop->anon_handle,
						&flt_infop->swap_handle);
		LPG_FLT_TRACE(FLT_ANON_PFIND, flt_infop->pfd, flt_infop->anon_handle, flt_infop->swap_handle, 0);

		ASSERT((!(flt_infop->anon_handle)) || (flt_infop->pfd) || (flt_infop->swap_handle));

		if ((flt_infop->pfd == NULL) && (flt_infop->anon_handle) &&
		    (flt_infop->swap_handle))
			num_pages_on_swap++;
		flt_infop->flags |= FLT_PFIND;
	}

	if (num_pages_on_swap == npgs) {
		int nswappedin = 0;

		allocated_psize = page_size;

		if (VPAG_UPDATE_VM_RSS(PAS_TO_VPAGG(pas), JOBRSS_INC_BLIND, 0, npgs)) {
			return USE_BASE_PAGE;
		}
#ifdef  _VCE_AVOIDANCE
		lpfd= vce_avoidance ?
			PMAT_PAGEALLOC(attr, colorof(lpage_start_addr), VM_VACOLOR| VM_MVOK,
					&allocated_psize, lpage_start_addr)
			: PMAT_PAGEALLOC(attr, vcache2(prp, attr, start_apn), VM_MVOK,
					&allocated_psize, lpage_start_addr);
#else
		lpfd = PMAT_PAGEALLOC(attr, vcache2(prp, attr, start_apn), VM_MVOK,
					&allocated_psize, lpage_start_addr);
#endif
		if (lpfd == NULL) {
			VPAG_UPDATE_VM_RSS(PAS_TO_VPAGG(pas), JOBRSS_INC_BLIND, 0, -npgs);
			wait_timeout = PM_PAGE_ALLOC_WAIT_TIMEOUT(attr->attr_pm);
			if (wait_timeout &&
			    faultdata->fd_flags & FD_PAGE_WAIT) {
				faultdata->fd_flags &= ~FD_PAGE_WAIT;
				if (vfault_wait_lpage(pas, rp, page_size, wait_timeout)) {
					INC_LPG_STAT(private.p_nodeid, PAGE_SIZE_TO_INDEX(page_size), LPG_SWAPIN_RETRY_LOWER_PGSZ);
					return RETRY_LOWER_PAGE_SIZE;
				} else {
					INC_LPG_STAT(private.p_nodeid, PAGE_SIZE_TO_INDEX(page_size), LPG_SWAPIN_RETRY_SAME_PGSZ);
					return RETRY_FAULT;
				}
			}
			LPG_FLT_TRACE(FLT_FAIL_BREAK, npgs, flt_infop, prp, 0);
			INC_LPG_STAT(private.p_nodeid, PAGE_SIZE_TO_INDEX(page_size), LPG_PAGEALLOC_FAIL);
			return USE_BASE_PAGE;
		}

		/*
		 * If we could got allocated a base page just free it
		 * and let vfault() handle it.
		 */

		if (allocated_psize == NBPP) {
			VPAG_UPDATE_VM_RSS(PAS_TO_VPAGG(pas), JOBRSS_INC_BLIND, 0, -npgs);
			LPG_FLT_TRACE(FLT_FAIL_NBPP, npgs, flt_infop, prp, 0);
			pagefree(lpfd);
			return USE_BASE_PAGE;
		}

		/*
		 * Insert all the pfdats into the anon page cache, before
		 * we unlock the region lock.
		 */

		flt_infop = start_flt_infop;
		pfd = lpfd;
		rpn = start_rpn;

		for (i = 0; i < npgs; i++, pfd++, flt_infop++, rpn++) {
			apn = rpntoapn(prp, rpn);

			if (anon_swapin(flt_infop->anon_handle, pfd, apn) == DUPLICATE) {
				/*
				 * Drat!  Someone else swapped in one or more
				 * of the pages after we did the pfind above.
				 * So undo everything, free the large page
				 * we allocated, and start over again.  This
				 * case should be extremely rare.
				 */

				for (; --i >= 0;) {
					pfd--;
					flt_infop--;
					pagedone(pfd);
					anon_uncache(pfd);
					flt_infop->pfd = NULL;
					pg_clrpgi(flt_infop->pd);
				}

				pagefree_size(lpfd, allocated_psize);

				regrele(rp);
				mrunlock(&pas->pas_aspacelock);
				VPAG_UPDATE_VM_RSS(PAS_TO_VPAGG(pas), JOBRSS_INC_BLIND, 0, -npgs);
				return RETRY_FAULT;
			}

			flt_infop->pfd = pfd;
			pg_setpfn(flt_infop->pd, pfdattopfn(pfd));
			pg_set_page_mask_index(flt_infop->pd, PAGE_SIZE_TO_MASK_INDEX(page_size));
		}

		prp->p_nvalid += npgs;
		atomicAddLong(&ut->ut_pproxy->prxy_ru.ru_majflt, npgs);
		MINFO.swap += npgs;

		regrele(rp);

		pfd = lpfd;
		flt_infop = start_flt_infop;

		/*
		 * Try to swap all the pages back into memory.
		 */
		swapfail = 0;
		for (i = 0; i < npgs; i++, pfd++, flt_infop++) {
			swaplst[0].gp_pfd = pfd;
			if (sm_swap(swaplst, flt_infop->swap_handle, 1, B_READ, NULL)) {
				ASSERT(pfd->pf_flags & (P_HASH|P_BAD));
				killpage(prp, flt_infop->pd);
				swapfail++;
			} else {
				flt_infop->flags |= FLT_DONE;
				nswappedin++;
			}
		}

		/*
		 * As the region lock is unlocked, we will have to
		 * retry.
		 */
		reglock(rp);

		LPG_FLT_TRACE(FLT_LPAGE_SWAPIN, lpfd, rp, start_flt_infop->pd, swapfail);

		pfd = lpfd;
		flt_infop = start_flt_infop;

		/*
		 * Set the page done bit. Also insert the page into
		 * the rmap and set the soft valid bit.
		 * We will do a retry as we have unlocked the region lock.
		 * If nobody has downgraded the page by this time we will
		 * just do reference bit faults and do a tlb dropin.
		 */
		for (i = 0; i < npgs; i++, pfd++, flt_infop++) {
			/*
			 * If swap failed downgrade the page size.
			 */
			if (swapfail)
				pg_set_page_mask_index(flt_infop->pd, MIN_PAGE_MASK_INDEX);

			if (flt_infop->flags & FLT_DONE) {
				/*
				 * Clear the fault pfind flag so that
				 * lpage_vfault will not release the page.
				 */
				flt_infop->flags &= ~FLT_PFIND;
				pagedone(pfd);
				pg_setsftval(flt_infop->pd);
				pg_setccuc(flt_infop->pd, attr->attr_cc, attr->attr_uc);
				VPAG_UPDATE_RMAP_ADDMAP(PAS_TO_VPAGG(pas), JOBRSS_INS_PFD, pfd, flt_infop->pd, attr_pm_get(attr));
			}

		}

		/*
		 * Release the job rss count for which we did not do above
		 * rmap_addmap.
		 */
		if (npgs - nswappedin) {
			VPAG_UPDATE_VM_RSS(PAS_TO_VPAGG(pas), JOBRSS_INC_BLIND, 0, (nswappedin - npgs));
		}

		/*
		 * Unlock everything and retry as we have unlocked the
		 * region lock once.
		 */
		regrele(rp);
		mrunlock(&pas->pas_aspacelock);
		return RETRY_FAULT;

	} else {
		return USE_BASE_PAGE;
	}
}

/*
 * vfault_wait_lpage:
 * Wait for coalesced to form a large page. Returns -1 if woken by a signal
 * -2 if timedout. 0 if normal wakeup.
 */
STATIC int
vfault_wait_lpage(
	pas_t	*pas,
	reg_t	*rp,
	size_t	page_size,
	long	wait_time)
{
	timespec_t	wait_ts;

	regrele(rp);
	mrunlock(&pas->pas_aspacelock);

	coalesce_daemon_wakeup(cnodeid(), CO_DAEMON_IDLE_WAIT|
				CO_DAEMON_TIMEOUT_WAIT|CO_DAEMON_FULL_SPEED);
	wait_ts.tv_sec = wait_time;
	wait_ts.tv_nsec = 0;
	return lpage_size_timed_wait(page_size, PZERO, &wait_ts);
}

/*
 * If page is a sentry waits for it to complete.
 * The pte lock is acquired to prevent migration until the test is complete.
 */
STATIC int
vfault_wait_sentry(
	pas_t	*pas,
	pde_t	*pd,
	reg_t	*rp)
{
	pgno_t	pfn;

	pg_pfnacquire(pd);
	pfn = pg_getpfn(pd);

	if (pfn > PG_SENTRY) {
		pg_pfnrelease(pd);
		return USE_BASE_PAGE;
	}

	pg_setwaiter(pd);
	pg_pfnrelease(pd);
	mrunlock(&pas->pas_aspacelock);
	ghostpagewait(rp, pfn);
	return RETRY_FAULT;
}

/*
 * Waits for page I/O to complete.
 * The pte lock is acquired to prevent migration until the test is complete.
 */
STATIC int
vfault_wait_pagedone(
	pas_t	*pas,
	pfd_t	*pfd,
	reg_t	*rp)
{

	if (!(pfd->pf_flags & P_DONE)) {
		regrele(rp);
		mrunlock(&pas->pas_aspacelock);
		pagewait(pfd);
		ASSERT(pfd->pf_use > 0);
		return RETRY_FAULT;
	} else
		return USE_BASE_PAGE;
}



#ifdef	DEBUG
/* ARGSUSED */
int
validate_tlb_entry(
	caddr_t		vaddr,
	size_t		page_size)
{
#ifndef	TFP
	caddr_t		addr;
	uint		page_mask;
	int		i;
	__psint_t	tlblo;

	addr = LPAGE_ALIGN_ADDR(vaddr, 2*page_size);
	for (i = 0; i < 2*btoct(page_size); i++, addr += NBPP) {
		tlblo = tlb_probe(tlbpid(&curuthread->ut_as), addr, &page_mask);

		if ((tlblo > 0) && (page_mask == 0x6000) && ((uint)tlblo & TLBLO_V))
			cmn_err(CE_PANIC, "lpage_tlbdropin addr %x page_mask %x uthread %x tlblo %x\n",
				addr, page_mask, curuthread, tlblo);
	}
#endif
	return 1;
}
#endif

#ifdef	VFAULT_TRACE_ENABLE

#include	<sys/idbgentry.h>

#define	MAX_VFAULT_TRC_ENTRIES	4096

struct vflt_trc {
	int	event;
	cpuid_t	cpu;
	ulong	pid;
	ulong	p1;
	ulong	p2;
	ulong	p3;
	ulong	p4;
} vflt_trc_buf[MAX_VFAULT_TRC_ENTRIES];

int	vflt_trc_buf_idx = 0;


char	*vfault_trc_event[] = {
	"Nothing",
	"Wait sentry",
	"Entering fault",
	"Ref bit fault",
	"Anon fault",
	"Swapin 16k page",
	"Demand fill",
	"Vnode fault",
	"Lpage tlb dropin",
	"16Kpage tlb dropin",
	"Pte header valid failure",
	"Pte has sentry failure",
	"Pfds not contiguous",
	"All pages present and contiguos",
	"No pages present for vnode",
	"Large page alloc failed",
	"Large page alloc success",
	"Pfault entry",
	"Autoreserve region failure",
	"COW check failure",
	"Non-COW region success",
	"Region has page hole failure",
	"lpage_vfault anon pfind",
	"Vhand toss large page",
	"pmap downgrade large page",
	"pmap downgrade large page pte",
	"pmap downgrade addr boundary",
	"Buddy pte is small page failure",
	"lpage_vfault failure",
	"pmap downgrade pte noflush",
	"Utlbmiss flush",
	"Utlbmiss set flush",
	"Fault fail PDONE",
	"Large page swapin",
	"Fault fail page align",
	"Fault fail NBPP page",
	"Fault lpage recover"

};



int	do_not_print = 1;
void
vfault_trace(
	int	event,
	ulong	p1,
	ulong	p2,
	ulong	p3,
	ulong	p4)
{
	int	index;
	struct vflt_trc	*p;

	index = atomicIncWithWrap(&vflt_trc_buf_idx, MAX_VFAULT_TRC_ENTRIES);
	p = &vflt_trc_buf[index];
	p->event = event;
	p->p1 = p1;
	p->p2 = p2;
	p->p3 = p3;
	p->p4 = p4;
	p->cpu = cpuid();
	p->pid = curvprocp ? current_pid() : curthreadp->k_id;
	if (do_not_print == 2)
		cmn_err(CE_CONT, "[%d] cpu %d pid %d %s 0x%lx 0x%lx 0x%lx 0x%lx\n",
				index, p->cpu, p->pid,
				vfault_trc_event[p->event],
				p->p1, p->p2, p->p3, p->p4);
}

int vfault_trace_print_num = 20;

void
vfault_print(
	int	indx,
	int	num)
{
	struct vflt_trc	*p;

	if (indx == -1)
		indx = vflt_trc_buf_idx;

	if (num == 0)
		num = 20;

	while(num) {
		indx--;
		if (indx == -1)
			indx = MAX_VFAULT_TRC_ENTRIES - 1;
		p = &vflt_trc_buf[indx];
		qprintf("[%d] cpu %d pid %d %s 0x%lx 0x%lx 0x%lx 0x%lx\n", indx,
					p->cpu, p->pid,
					vfault_trc_event[p->event],
					p->p1, p->p2, p->p3, p->p4);
		num--;
	}
}

#endif	/* VFAULT_TRACE_ENABLE */

#ifdef MH_R10000_SPECULATION_WAR
#include <os/vm/anon_mgr.h>

extern int is_in_pfdat(pgno_t);

/*
 * invalidate_page_references
 *
 * This routine visits all known references to a physical page, and
 * turns off the hardware-valid bit for all references, and turns off
 * the hardware-modified bit for all kernel references.  It does not
 * affect the software valid bit.
 *
 * It returns 0 if the TLB is consistent and 1 if a tlbsync() by the
 * caller is needed.
 */

extern is_in_pfdat(pgno_t pfn);

void
invalidate_kptbl_entry(
	void		*counts,
	pfd_t		*pfd,
	__psint_t	kvfn)
{
	pde_t		*pde;
	int		readonly = ((int *) counts)[2];

	pde = kptbl + (kvfn - K2MEM_VFNMIN);
	if (!pg_isvalid(pde))
	    return;
	if ((pg_ismod(pde) || ((!readonly) && pg_ishrdvalid(pde))) && !pg_isnoncache(pde)) {
		pg_clrmod(pde);
		if (readonly)
			tlbdropin(0, (caddr_t)ctob(kvfn), pde->pte);
		else {
			pg_clrhrdvalid(pde);
			unmaptlb(0, kvfn);
		}
		((int *) counts)[1]++;
	}
	((int *) counts)[0]++;
}

STATIC int
invalidate_uptbl_entry(
	pde_t	*pde,
	preg_t	*prp,
	void	*counts)
{
    pfd_t	*pfd;

    if (!pg_isvalid(pde) || pg_isnoncache(pde))
	return 0;

    if (pg_ismod(pde) && is_in_pfdat(pdetopfn(pde))) {
	    pfd = pdetopfdat(pde);
	    pfd->pf_flags |= P_DIRTY;
    }
    if (pg_ishrdvalid(pde)) {
	    pg_clrhrdvalid(pde);
	    ((int *)counts)[1]++;
    }
    ((int *)counts)[0]++;
    return 0;
}

extern int foreach_pde(pfd_t *, int (*)(pde_t *,preg_t *,void *), void *);

int
invalidate_user_page_references(
	pfd_t	*pfd,
	int	invflags)
{
	int     pde_counts[3], s;

	if (pfd->pf_flags & (P_QUEUE | P_SQUEUE))
		return(0);

	pde_counts[0] = 0;
	pde_counts[1] = 0;
	pde_counts[2] = invflags;

	if (private.p_kstackflag == PDA_CURINTSTK) {
		if (showconfig)
			cmn_err(CE_WARN, "invalidate_user_page_references:  "
				"user references at interrupt level");
		return(0);
	}

	s = RMAP_LOCK(pfd);
	rmap_scanmap(pfd, RMAP_MH_SPECULATION_WAR, (void *)pde_counts);
	RMAP_UNLOCK(pfd, s);

	if (pde_counts[1] > 0)
		tlbsync(0LL, allclr_cpumask, STEAL_PHYSMEM);

	return(pde_counts[1] > 0);
}

int
invalidate_page_references(
	pfd_t	*pfd,
	int	cache_flags,
	int	readonly)
{
	int     pde_counts[3], s;

	if (!IS_R10000() || pfd->pf_flags & (P_QUEUE | P_SQUEUE))
		return(0);

	pde_counts[0] = 0;
	pde_counts[1] = 0;
	pde_counts[2] = readonly & INV_REF_READONLY;

	krpf_visit_references(pfd, invalidate_kptbl_entry, pde_counts);

	if (pde_counts[0] == pfd->pf_use ||
	    (!(pfd->pf_flags & P_HASH)) ||
	    (readonly & INV_REF_FAST))
		return(0);

	if (private.p_kstackflag == PDA_CURINTSTK) {
		if (showconfig)
			cmn_err(CE_WARN, "invalidate_page_references:  user references on intstack");
		return(0);
	}

	pde_counts[1] = 0;

	s = RMAP_LOCK(pfd);
	rmap_scanmap(pfd, RMAP_MH_SPECULATION_WAR, (void *)pde_counts);
	RMAP_UNLOCK(pfd, s);

	return(pde_counts[1] > 0);
}

int
invalidate_range_references(
	void	*addr,
	int	bcnt,
	int	cache_flags,
	int	readonly)
{
	__psunsigned_t	vaddr;
	__psunsigned_t	vlimit;
	pfd_t	*pfd;
	pde_t	pde;
	int     need_sync = 0;
	int     cnt;

	for (vaddr = ((__psunsigned_t) addr),
	     vlimit = ((__psunsigned_t) addr) + bcnt;
	     vaddr < vlimit;
	     vaddr += (_PAGESZ - poff(vaddr))) {
		if (iskvir(vaddr)) {
			pfd = kvatopfdat(vaddr);
		} else if (IS_KUSEG(vaddr) && curuthread != NULL) {
			if (private.p_kstackflag == PDA_CURINTSTK) {
				if (showconfig)
					cmn_err(CE_WARN,"invalidate_range_references:  user references at interrupt level");
				continue;
			}
			if (!vtopv((caddr_t) vaddr,1, (pfn_t *) &pde.pgi,sizeof(pde),
				   PTE_PFNSHFT, PG_SV|PG_VR|PG_G) ||
			    pdetopfn(&pde) == 0)
				continue;
			pfd = pdetopfdat(&pde);
		} else if (IS_KSEGDM(vaddr) && is_in_pfdat(KDM_TO_PHYS(vaddr)))
			pfd = pfntopfdat(KDM_TO_PHYS(vaddr));
		else
			continue;
		need_sync |= invalidate_page_references(pfd,cache_flags,readonly);
		((cache_flags & CACH_ICACHE) ? _bclean_caches : clean_dcache)
			((void *) poff(vaddr),
			 ((btoct(vlimit) > btoct(vaddr))
			  ? (_PAGESZ - poff(vaddr))
			  : (vlimit - vaddr)),
			 pfdat_to_eccpfn(pfd),
			 cache_flags | CACH_NOADDR);
	}
	if (need_sync)
		tlbsync(0LL, allclr_cpumask, STEAL_PHYSMEM);
	return(need_sync);
}

#endif /* MH_R10000_SPECULATION_WAR */
