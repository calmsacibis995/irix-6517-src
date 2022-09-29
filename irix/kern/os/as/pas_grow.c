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

#ident	"$Revision: 3.110 $"

#include "sys/types.h"
#include "ksys/as.h"
#include "as_private.h"
#include "sys/cmn_err.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "sys/param.h"
#include "sys/immu.h"
#include "sys/lock.h"
#include "sys/kabi.h"
#include "sys/numa.h"
#include "sys/pmo.h"
#include "sys/mmci.h"
#include "sys/pfdat.h"
#include "sys/prctl.h"
#include "region.h"
#include "pmap.h"
#include "sys/resource.h"
#include "sys/sbd.h"
#include "sys/sema.h"
#include "sys/sysmacros.h"
#include "sys/systm.h"

int
pas_break(pas_t *pas, ppas_t *ppas, uvaddr_t nva)
{
	auto preg_t *prp;
	reg_t *rp, *new_rp;
	uvaddr_t ova, original_nva;
	int change;
	int error = 0;
	size_t page_size;
	struct pm	*pm;
#ifdef DEBUG
	vasid_t vasid;
#endif

	/*
	 * Only allow one sbreak call per process/share group at a time.  
	 * The aspacelock is not enough since it has to be released in the
	 * case of automatically locking down the new pages (which can
	 * fail and cause us to have to undo the sbreak).  So we
	 * use an additional lock to keep other processes out of the
	 * whole critical section.
	 */
	mutex_lock(&pas->pas_brklock, 0);

	/* lock global shared process update lock */
	mrupdate(&pas->pas_aspacelock);

	ova = pas->pas_brkbase + pas->pas_brksize;

	if (nva > pas->pas_hiusrattach)
		goto sbrk_err;

	
#if DEBUG
	as_lookup_current(&vasid);
#endif
	/* We are assuming pas_break called within the current process's context */
	ASSERT(VASID_TO_PAS(vasid) == pas);
	pm = aspm_getcurrentdefaultpm(pas->pas_aspm, MEM_DATA);
	page_size = PM_GET_PAGESIZE(pm);

	original_nva = nva;
	if (page_size > NBPP) 
		nva = LPAGE_ALIGN_ADDR((nva + 2*page_size), 2*page_size);

retry :
	change = btoc(nva) - btoc(ova);

	if (change == 0) {
		mrunlock(&pas->pas_aspacelock);
		mutex_unlock(&pas->pas_brklock);
		return 0;
	}

	if (change > 0 &&
	    ((chkpgrowth(pas, ppas, change) < 0) ||
	     (pas->pas_datamax != RLIM_INFINITY &&
	     (ctob(change) + pas->pas_brksize) > pas->pas_datamax))) {

		if (nva != original_nva) {
			nva = original_nva;
			goto retry;
		}  
		goto sbrk_err;
	}

	/* Find the processes data region.  */
	prp = findpreg(pas, ppas, ova - 1);

	if (change < 0) {
		if (prp == NULL || -change > prp->p_pglen)
			goto sbrk_err;
		newptes(pas, ppas, prp->p_flags & PF_SHARED);
	}

	new_rp = NULL;

	/*
	 * If there's nothing there, then the user has removed it with
	 * munmap.  Try to allocate a new region and grow it to the desired size.
	 * If there's a region there, but it's not a growable, anonymous,
	 * copy-on-write region, then try to put one there.  If there's 
	 * something in the way, then the user has done this with mmap and
	 * we give up.
	 * XXXjwag - what is a good definition of growable??
	 */

	if (prp == NULL ||
	   ((prp->p_flags & PF_DUP) == 0) ||
	   (prp->p_reg->r_flags & (RG_ANON|RG_CW)) != (RG_ANON|RG_CW)) {
                struct pm *pm;
		new_rp = allocreg(NULL, RT_MEM, RG_ANON|RG_CW);
                pm = aspm_getdefaultpm(pas->pas_aspm, MEM_DATA);
		error = vattachreg(new_rp, pas, ppas, ova, (pgno_t) 0,
				(pgno_t) 0, PT_MEM, PROT_RW, PROT_RWX,
				PF_DUP, pm, &prp);
                aspm_releasepm(pm);

		if (error) {
			freereg(pas, new_rp);
			goto sbrk_err;
		}

		rp = new_rp;
	} else {
		rp = prp->p_reg;
		reglock(rp);
	}

	if (error = vgrowreg(prp, pas, ppas, PROT_RW, change)) {
		if (new_rp)
			(void)vdetachreg(prp, pas, ppas, prp->p_regva, prp->p_pglen, 0);
		else
			regrele(rp);

		goto sbrk_err;
	}

	regrele(rp);
	pas->pas_brksize += ctob(change);
	mrunlock(&pas->pas_aspacelock);

	/* check if auto-lock of new growth is needed */
	if ((change > 0) &&
	     (pas->pas_lockdown & (AS_FUTURELOCK|AS_DATALOCK))) {
		mraccess(&pas->pas_aspacelock);
		if ((error = lockpages2(pas, ppas, ova, change, LPF_NONE)) != 0) {
			/* fail the lock, undo sbreak */
			mrunlock(&pas->pas_aspacelock);
			mrupdate(&pas->pas_aspacelock);
			reglock(rp);
			vgrowreg(prp, pas, ppas, 0, -change);
			pas->pas_brksize -= ctob(change);
			regrele(rp);
			goto sbrk_err;
		}
		mrunlock(&pas->pas_aspacelock);
	}
	
	/* Adjust brkbase if it was moved backwards (brksize negative) */
	if (change < 0 && (__psint_t)pas->pas_brksize < 0) {
		pas->pas_brkbase -= -pas->pas_brksize;
		pas->pas_brksize = 0;
	}

	mutex_unlock(&pas->pas_brklock);

	return 0;

sbrk_err:
	mrunlock(&pas->pas_aspacelock);

	mutex_unlock(&pas->pas_brklock);

	if (error)
		return error;
	else
		return ENOMEM;
}

#undef STKDEBUG
#ifdef STKDEBUG
int stkdebug = 1;
int stkdrop = 0;
#endif
/*
 * grow the stack to include the SP (Stacks grow down!)
 *
 * Returns: 0 on success or one of the following errors in case of failure:
 *
 *	    EEXIST  if sp already valid
 *	    ENOSPC  if couldn't grow cause process is too large
 *	    EACCES  if don't have no stack
 *	    ENOMEM  couldn't lock down stack pages
 *	    EAGAIN  if couldn't grow for other reasons
 */
int
pas_stackgrow(pas_t *pas, ppas_t *ppas, uvaddr_t sp)
{
	auto preg_t *prp;
	reg_t *rp, *new_rp;
	ssize_t si;
	rlim_t rcur;
	int rv, extra = 1;
	uvaddr_t stkbase;
	size_t stksize;
	size_t	page_size;
	struct pm *pm;
	uvaddr_t original_sp;
	/* REFERENCED */
	int error;

	/* lock global shared process update lock */
	mrupdate(&pas->pas_aspacelock);

	/*
	 * Only sprocs stash stack info in ppas -- others,
	 * including multi-threaded processes, use pas.
	 */
	if (ppas->ppas_flags & PPAS_STACK) {
		stkbase = ppas->ppas_stkbase;
		stksize = ppas->ppas_stksize;
		rcur = ppas->ppas_stkmax;
	} else {
		stkbase = pas->pas_stkbase;
		stksize = pas->pas_stksize;
		rcur = pas->pas_stkmax;
	}
	if (stkbase == NULL) {
		mrunlock(&pas->pas_aspacelock);
		return EACCES;
	}

	/* change is current lowest stack address minus wanted sp */
	original_sp = sp;
	pm = aspm_getdefaultpm(pas->pas_aspm, MEM_STACK);
	page_size = PM_GET_PAGESIZE(pm);
	aspm_releasepm(pm);

	/*
 	 * If large pages are needed grow the stack by 2*page_size.
	 */
	if (page_size > NBPP) {
		sp = LPAGE_ALIGN_ADDR(sp, 2*page_size);
		ASSERT(sp <= original_sp);
	}
	si = stkbase - (uvaddr_t)sp;
	if (si <= 0) {
		rv = EEXIST;
		goto bad;
	}

	/* give a little more */
	if (page_size == NBPP)
		si += ctob(SINCR);

	if (chkpgrowth(pas, ppas, btoc(si)) < 0 ||
		(rcur != RLIM_INFINITY && ((stksize + si) > rcur))) {
		/* ok, don't give a little extra */
		if (page_size == NBPP) {
			si -= ctob(SINCR);
		} else {
			/*
			 * If aligned for large pages decrement the 
			 * the extra growth needed to accomodate large pages.
			 */
			ASSERT(original_sp > sp);
			si -= (original_sp - sp);
		}
		extra = 0;
		if (chkpgrowth(pas, ppas, btoc(si)) < 0 ||
		    (rcur != RLIM_INFINITY && ((stksize + si) > rcur))) {
			rv = ENOSPC;
			goto bad;
		}
	}

	new_rp = NULL;

	/* Get the processes stack region. */
	prp = findpreg(pas, ppas, stkbase);

	/*
	 * If there's nothing there, then the user has either removed it with
	 * munmap or has done a setcontext() system call and put the stack
	 * in new location.  In either case, try to allocate a new stack
	 * region and grow it to the desired size.
	 */
	if (prp == NULL || prp->p_type != PT_STACK) {
		/*
		 * Note that we may be holding a region lock if there is
		 * a region immediately following the stack (i.e. the high
		 * stack address is the base of the next region) so that
		 * the above "findreg" obtains a prp but it is not of type
		 * PT_STACK.  The following allocreg & attachreg will place
		 * a new stack region into position, but we need to release
		 * the lock for the subsequent pregion.
		 */
                struct pm *pm;

		rp = allocreg(NULL, RT_MEM, RG_ANON);
                pm = aspm_getdefaultpm(pas->pas_aspm, MEM_STACK);
		if (error = vattachreg(rp, pas, ppas, stkbase, (pgno_t) 0,
				(pgno_t) 0, PT_STACK, PROT_RW,
				PROT_RWX, PF_DUP, pm, &prp)) {
                        aspm_releasepm(pm);
			freereg(pas, rp);
			rv = EAGAIN;
			goto bad;
		}
		ASSERT(prp->p_reg == rp);
                aspm_releasepm(pm);
		rp->r_maxfsize = ctob(btoc64(rcur));
		new_rp = rp;
	} else {
		rp = prp->p_reg;
		reglock(rp);
	}
	ASSERT(prp->p_type == PT_STACK);

	if (error = vgrowreg(prp, pas, ppas, PROT_RW, btoc(si))) {
		si -= ctob(SINCR);
		if (!extra ||
		    	(error = vgrowreg(prp, pas, ppas, PROT_RW, btoc(si)))) {
			if (new_rp)
				(void) vdetachreg(prp, pas, ppas, prp->p_regva,
							prp->p_pglen, 0);
			else
				regrele(rp);

			rv = error;
			goto bad;
		}
	}

	if (ppas->ppas_flags & PPAS_STACK) {
		ppas->ppas_stkbase = prp->p_regva;
		ppas->ppas_stksize += si;
	} else {
		pas->pas_stkbase = prp->p_regva;
		pas->pas_stksize += si;
	}
	regrele(rp);
	mrunlock(&pas->pas_aspacelock);
	
	/*
	 * check if auto-lock of new growth is needed --
	 * got here only if growreg works
	 */
	if (pas->pas_lockdown & (AS_DATALOCK | AS_FUTURELOCK)) {
		mraccess(&pas->pas_aspacelock);
		if (error = lockpages2(pas, ppas, prp->p_regva, btoc(si), LPF_NONE)) {
			/*
			 * don't bother undo the grow, since user will get
			 * a SIGSEGV
			 */
			rv = ENOMEM;
			goto bad;
		}
		mrunlock(&pas->pas_aspacelock);
	}

	return 0;

bad:

#ifdef STKDEBUG
	if (stkdebug && kdebug && (si > 0)) {
		cmn_err(CE_CONT,
			"Bad stack growth for thread %ld[%s] sz:%d vaddr:%x rv %d stkbase 0x%x error %w32d\n",
			get_thread_id(), get_current_name(), si, sp, rv,
			ppas->ppas_stkbase, error);
		if (stkdrop)
			debug(0);
	}
#endif
	mrunlock(&pas->pas_aspacelock);

	return(rv);
}
