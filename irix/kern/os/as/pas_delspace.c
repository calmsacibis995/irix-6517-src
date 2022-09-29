/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995-1996, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident  "$Revision: 1.12 $"

#include "sys/types.h"
#include "sys/anon.h"
#include "ksys/as.h"
#include "as_private.h"
#include "sys/atomic_ops.h"
#include "sys/cachectl.h"
#include "sys/cmn_err.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "sys/kmem.h"
#include "limits.h"
#include "sys/lock.h"
#include <sys/numa.h>
#include "sys/proc.h"
#include "pmap.h"
#include "region.h"
#include "sys/vnode.h"
#include "sys/sysmacros.h"
#include "sys/systm.h"
#include "sys/buf.h"


int
pas_delexit(pas_t *pas, ppas_t *ppas, as_deletespace_t *arg)
{
	preg_t *prp;
	int drv;
	int lastone;
	int s;

	mrupdate(&pas->pas_aspacelock);

	/* remove any watchpoints for non pthread apps*/
	deleteallwatch(pas, ppas, 0);
	/* unlock PRDA if it's locked */
	pas_unlockprda(pas, ppas, arg->as_vrelvm_prda);

	if (pas->pas_tsave)
		remove_tsave(pas, ppas, RF_NOFLUSH);

	/* remove private regions */
	while ((prp = PREG_FIRST(ppas->ppas_pregions)) != NULL) {
		reglock(prp->p_reg);
		drv = vdetachreg(prp, pas, ppas, prp->p_regva, prp->p_pglen,
				 RF_NOFLUSH);
		ASSERT_ALWAYS(drv == 0);
		ASSERT(!issplhi(getsr()));
	}

	/*
	 * if we're the last one owning the AS - detach all regions
	 */
	ASSERT(pas->pas_plist);
	ASSERT(pas->pas_refcnt > 0);
	if (arg->as_op == AS_DEL_VRELVM) {
		/*
		 * Hack - we want to detach all the pregions,
		 * but need the ref count.
		 */
		lastone = pas->pas_refcnt == 1;
	} else {
		lastone = atomicAddInt(&pas->pas_refcnt, -1) == 0;
	}
	/* remove any watchpoints for pthread apps*/
	deleteallwatch(pas, ppas, lastone + 1);
	if (lastone) {
		/* detach 'shared' regions */
		while ((prp = PREG_FIRST(pas->pas_pregions)) != NULL) {
			reglock(prp->p_reg);
			drv = vdetachreg(prp, pas, ppas, prp->p_regva,
					prp->p_pglen, RF_NOFLUSH);
			ASSERT_ALWAYS(drv == 0);
			ASSERT(!issplhi(getsr()));
		}
	} else if (arg->as_vrelvm_detachstk) {
		if (prp = findpreg(pas, ppas, ppas->ppas_stkbase)) {
			reg_t *rp;
			rp = prp->p_reg;
			reglock(rp);

			/*
			 * reset the stack allocator hint since we're
			 * freeing up a stack
			 */
			if (pas->pas_lastspbase < prp->p_regva)
				pas->pas_lastspbase = prp->p_regva + ctob(prp->p_pglen);

			/* detach region */
			drv = vdetachreg(prp, pas, ppas, prp->p_regva,
							prp->p_pglen, 0);
			ASSERT_ALWAYS(drv == 0);
		}
	}
	if (arg->as_op == AS_DEL_VRELVM) {
		/*
		 * we aren't going to as_rele yet, but we really
		 * want to shut down the pmaps
		 */
		if (pas->pas_pmap)
			pmap_destroy(pas->pas_pmap);
		if (ppas->ppas_pmap)
			pmap_destroy(ppas->ppas_pmap);
		pas->pas_pmap = NULL;
		ppas->ppas_pmap = NULL;
	}
	/*
	 * we are no longer permitted to access the utas
	 */
	s = mutex_spinlock(&ppas->ppas_utaslock);
	ppas->ppas_utas = NULL;
	mutex_spinunlock(&ppas->ppas_utaslock, s);
	mrunlock(&pas->pas_aspacelock);
	return 0;
}

int
pas_delexec(pas_t *pas, ppas_t *ppas, as_deletespace_t *arg)
{
	preg_t *prp;
	int drv, lastone;
	int doingshd = 0;
	preg_t **psave;

	/*
	 * make life easy - if on an isolated processor - don't bother
	 * savings pregions (see below when we go through all private
	 * regions and save (mostly text regions) for the execing
	 * process.
	 *
	 * If an sproc process is doing the exec, then we can run into
	 * trouble when re-using the pmap due to saved regions.
	 * Consider a segemnt S, which has both a large shared region,
	 * (A) and a small local region B, which overlaps A.
	 * A will be in shaddr, B will be in local list (proc_t).
	 * When A is not saved (if A is
	 * a data region), then detachreg(A) will be called. To handle
	 * normal sharing cases, in detachreg(A), the proc_t->pmap
	 * (the page of pte's corresponding to S)
	 * of the process will be marked with sentinels so that the
	 * shaddr->pmap (backing B) will be correctly used.
	 * Thus, if we save regions, then if the exec'ed process
	 * attaches a region at A, then vault()'s in A will see
	 * sentinel values ---
	 * this is not desirable, as only utlbmiss() is supposed to
	 * know about sentinels at all ... 
	 *
	 * Ideally, in case of exec'ing sproc, we would want to go
	 * through the pmap and "take" with the exec'ed process
	 * only pte's corresponding
	 * to the saved regions ... but this may be too expensive.
	 * For now, do not handle such cases (tsave is only a
	 * performance feature, not required for semantic correctness
	 * [of exec]) ...
	 *	[Cross ref. Bug #227235]
	 *
	 * In the world of AS, this translates into - we only do tsave
	 * if the address space has one member and that member has only
	 * one pmap. (note this handles the isolated case where only one
	 * thread is isolated also).
	 *
	 * We also don't save anything if we're exec'ing a different
	 * style ABI or if the job is a miser job.
	 */
	mrupdate(&pas->pas_aspacelock);

	psave = &pas->pas_tsave;
	if (arg->as_exec_rmp || (ppas->ppas_flags & PPAS_ISO) ||
			(pas->pas_flags & PAS_NOMEM_ACCT) || 
			(pas->pas_refcnt != 1) ||
			(ppas->ppas_pmap &&
			 (pas->pas_pmap != ppas->ppas_pmap)))
		psave = NULL;

	/* remove any watchpoints for non pthread apps */
	deleteallwatch(pas, ppas, 0);
	/* unlock prda if it's locked */
	pas_unlockprda(pas, ppas, arg->as_exec_prda);
	
	prp = PREG_FIRST(ppas->ppas_pregions);
shd:
	while (prp) {
		preg_t *nprp = PREG_NEXT(prp);

		/* assertion for isolated processes */
		ASSERT((prp->p_reg->r_flags & RG_ISOLATE) ||
				!(ppas->ppas_flags & PPAS_ISO));

		reglock(prp->p_reg);

		if (psave && (prp->p_reg->r_flags & RG_TEXT)) {
			unattachpreg(pas, ppas, prp);
			regrele(prp->p_reg);
			PREG_SET_NEXT_LINK(prp, *psave);
			*psave = prp;
		} else {
			drv = vdetachreg(prp, pas, ppas, prp->p_regva,
					  prp->p_pglen, RF_NOFLUSH);
			ASSERT(drv == 0);
		}
		prp = nprp;
	}

	/*
	 * SPT
	 * Remove PMAPFLAG_SPT from pmap, if this process
	 * had shared PT's, detachreg() (above) should have
	 * taken care of them.
	 */
	if (psave && pmap_spt(ppas->ppas_pmap))
		pmap_spt_unset(ppas->ppas_pmap);

	if (!doingshd && (lastone = (atomicAddInt(&pas->pas_refcnt, -1)==0))) {
		/* detach all 'shared' regions if we're last member */
		/* remove any watchpoints for pthread apps */
		deleteallwatch(pas, ppas, lastone + 1);
		doingshd = 1;
		prp = PREG_FIRST(pas->pas_pregions);
		goto shd;
	}
	if (!doingshd && arg->as_exec_detachstk) {
		if (prp = findpreg(pas, ppas, ppas->ppas_stkbase)) {
			reg_t *rp;
			rp = prp->p_reg;
			reglock(rp);

			/*
			 * reset the stack allocator hint since we're
			 * freeing up a stack
			 */
			if (pas->pas_lastspbase < prp->p_regva)
				pas->pas_lastspbase = prp->p_regva +
						ctob(prp->p_pglen);

			/* detach region */
			drv = vdetachreg(prp, pas, ppas, prp->p_regva,
							prp->p_pglen, 0);
			ASSERT_ALWAYS(drv == 0);
		}
	}

	pas_flagset(pas, PAS_EXECING);
	mrunlock(&pas->pas_aspacelock);
	return 0;
}

/*
 * unmap a portion of address space.
 */
int
pas_unmap(
	pas_t *pas,
	ppas_t *ppas,
	uvaddr_t vaddr,
	size_t len,
	int flags)
{
	preg_t *prp;
	reg_t *rp;
	pgcnt_t	npgs, sz;
	uvaddr_t end;
	int error;
	int doingprivate;
	uvaddr_t start;

	ASSERT(mrislocked_update(&pas->pas_aspacelock));

	/* deal with vaddr not being on a page boundary */
	start = (uvaddr_t)((__psunsigned_t)vaddr & ~POFFMASK);
	len += (vaddr - start);

	vaddr = start;
	npgs = btoc(len);
	end = vaddr + ctob(npgs);
	doingprivate = 1;
doshd:
	while (npgs) {
		/*
		 * Go through the address range a region at a time
		 */
		if ((prp = findfpreg_select(pas, ppas, vaddr, end, doingprivate)) == NULL)
			break;

		ASSERT(!doingprivate || (prp->p_flags & PF_PRIVATE));
		if (prp->p_type == PT_PRDA) {
			/*
			 * XXX there is no interface to unlock the prda
			 * once its been locked
			 */
			if ((pas->pas_flags & PAS_LOCKPRDA) ||
					(ppas->ppas_flags & PPAS_LOCKPRDA)) {
				return EBUSY;
			}
		}

		/*
		 * update vaddr and npgs if we moved past a hole in the
		 * address space.
		 */
		if (vaddr < prp->p_regva) {
			npgs -= btoc(prp->p_regva - vaddr);
			vaddr = prp->p_regva;
		}

		sz = MIN(npgs, pregpgs(prp, vaddr));
		rp = prp->p_reg;
		reglock(rp);
		if (error = vdetachreg(prp, pas, ppas, vaddr, sz, 0))
			return error;

		/*
		 * reset nextaalloc if easy - this avoids problems
		 * with simple programs that mmap/munmap lots of stuff
		 * fragmenting their address space too much
		 */
		if (pas->pas_flags & PAS_64) {
			if (vaddr >= LOWDEFATTACH64)
				pas->pas_nextaalloc = vaddr;
		} else {
			if (vaddr >= LOWDEFATTACH)
				pas->pas_nextaalloc = vaddr;
		}
		vaddr += ctob(sz);
		npgs -= sz;
	}

	if (!(flags & DEL_MUNMAP_LOCAL) && doingprivate) {
		doingprivate = 0;
		vaddr = start;
		npgs = btoc(len);
		goto doshd;
	}

	return 0;
}

/*
 * delete shm segment
 */
int
pas_delshm(pas_t *pas, ppas_t *ppas, uvaddr_t vaddr, as_deletespaceres_t *res)
{
	reg_t *rp;
	preg_t *prp;
	int error;

	/*
	 * Find matching shmem address in process region list
	 */
	if ((prp = findpreg(pas, ppas, vaddr)) == NULL ||
					    prp->p_type != PT_SHMEM ||
					    prp->p_regva != vaddr) {
		return EINVAL;
	}
	rp = prp->p_reg;
	reglock(rp);
	/* return shmid which was stored on alloc */
	res->as_shmid = rp->r_shmid;

	/* Detach region from process */
	ASSERT(pas_getnshm(pas, ppas) > 0);
	if (error = vdetachreg(prp, pas, ppas, prp->p_regva, prp->p_pglen, 0)) {
		return error;
	}
	/* vdetachreg releases rp */

	return 0;
}
