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
#ident  "$Revision: 1.10 $"

#include <sys/types.h>
#include <ksys/as.h>
#include "as_private.h"
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include "pmap.h"
#include <sys/proc.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/uthread.h>
#include <sys/watch.h>

/*
 * watchpoint operations
 *
 * All these functions are below the AS switch - they are called from other
 * VASops.
 * They may only access uthread data if they are called on behalf of the
 * current-uthread. Any 3rd party uthread modifications must be done above
 * the AS switch.
 */
static int _clrsyswatch(pas_t *pas, ppas_t *ppas);
static int setwatch(pas_t *pas, ppas_t *ppas, watch_t *w);
static int clrwatch(pas_t *pas, ppas_t *ppas, watch_t *w);

#define NULLWA	((caddr_t)-1L)

extern int pwa;

/*
 * asvo_watch - watchpoint operations on an AS
 */
int
pas_watch(
	bhv_desc_t *bhv,
	aspasid_t pasid,	/* private address space id */
	as_watchop_t op,
	as_watch_t *watch)
{
	pas_t *pas = BHV_TO_PAS(bhv);
	ppas_t *ppas = (ppas_t *)pasid;
	int error = EINVAL;

	switch (op) {
	case AS_WATCHCLRSYS:
		error = _clrsyswatch(pas, ppas);
		break;
	case AS_WATCHCANCELSKIP:
		mraccess(&pas->pas_aspacelock);
		ascancelskipwatch(pas, ppas);
		error = 0;
		mrunlock(&pas->pas_aspacelock);
		break;
	case AS_WATCHSET:
		ASSERT(mrislocked_update(&pas->pas_aspacelock));
		error = setwatch(pas, ppas, &watch->as_setclrwatch_where);
		break;
	case AS_WATCHCLR:
		ASSERT(mrislocked_update(&pas->pas_aspacelock));
		error = clrwatch(pas, ppas, &watch->as_setclrwatch_where);
		break;
	}
	return error;
}

/*
 * setwatch - set a watchpoint in a uthread (not necessarily current)
 */
static int
setwatch(pas_t *pas, ppas_t *ppas, watch_t *w)
{
	register preg_t *prp;
	register reg_t *rp;
	register pde_t *pd;
	register uvaddr_t vaddr, end;
	register attr_t *attr;

	ASSERT(mrislocked_access(&pas->pas_aspacelock) || \
		(mrislocked_update(&pas->pas_aspacelock)));

	end = w->w_vaddr + w->w_length;

	for (vaddr = (uvaddr_t)ctob(btoct(w->w_vaddr)); vaddr < end; 
							vaddr += ctob(1)) {
		/* look up address */
		if ((prp = findpreg(pas, ppas, vaddr)) == NULL) {
			/* XXX clear already set pages */
			return(EINVAL);
		}
		/* XXX share groups ... */
		rp = prp->p_reg;
		reglock(rp);
		if (prp->p_type == PT_SHMEM || (rp->r_flags & RG_PHYS)) {
			regrele(rp);
			return(EINVAL);
		}

		pd = pmap_pte(pas, prp->p_pmap, vaddr, 0);
		attr = findattr_range(prp, vaddr, vaddr + ctob(1));

		/*
		 * Note that this sequence of tests insures that
		 * if we set a READ/EXEC watchpoint on a page
		 * that alreas has a WRITE watchpoint, we'll
		 * do the right thing
		 */
		if (w->w_mode == W_WRITE) {
			/* if write only, only turn off mod bit */
			attr->attr_watchpt = 1;
			if (IS_LARGE_PAGE_PTE(pd))
				pmap_downgrade_lpage_pte(pas,
						vaddr, pd);
			pg_clrmod(pd);
			regrele(rp);
			tlbclean(pas, btoct(vaddr), pas->pas_isomask);
		} else {
			attr->attr_watchpt = 1;
			if (IS_LARGE_PAGE_PTE(pd))
				pmap_downgrade_lpage_pte(pas,
						vaddr, pd); 
			pg_clrhrdvalid(pd);
			regrele(rp);
			tlbclean(pas, btoct(vaddr), pas->pas_isomask);
		}
	}
	return(0);
}

static int
clrwatch(pas_t *pas, ppas_t *ppas, watch_t *w)
{
	register preg_t *prp;
	register uvaddr_t vaddr, end;
	register attr_t *attr;

	ASSERT(mrislocked_access(&pas->pas_aspacelock) || \
		(mrislocked_update(&pas->pas_aspacelock)));

	end = w->w_vaddr + w->w_length;

	for (vaddr = (uvaddr_t)ctob(btoct(w->w_vaddr)); vaddr < end; 
							vaddr += ctob(1)) {
		/* look up address */
		if ((prp = findpreg(pas, ppas, vaddr)) == NULL) {
			return(EINVAL);
		}
		reglock(prp->p_reg);
		attr = findattr_range(prp, vaddr, vaddr + ctob(1));

		attr->attr_watchpt = 0;
		regrele(prp->p_reg);
	}
	return(0);
}

/*
 * see if fault corresponds to a watched address
 * aspacelock(ACCESS) already set
 * Region that vaddr is in is also locked
 *
 * rw - set to read/write/execute
 * Returns: 0 - not interested in watchpoint
 *	    1 - user interested in watchpoint
 *	    2 - interested and has current skipped watchpoint that
 *		needs to be cleared
 *	    3 - not interested but instruction is a 'sc'
 * If watchpoint gets hit inside system, then always return 0, but set the
 *	pw_flag dependent on whether we're interested.
 *
 * It is possible that a process, that does not have watchpoints set hits
 * one - 1 legitimate way this can happen is if a /proc owner reads another
 * process'es space that contains watchpoints.
 *	If this occurrs in the system we still set the UT_WSYS bit so we know
 *	this happened, and return to skip over it.
 *	If it occurs in user space we assume that higher forces are dealing with
 * 	the issue and simply reactivate the proc - thus processes that really
 *	wanted to stop will not..
 * There are some exciting problems w.r.t store-conditionals.
 * If there is a watchpoint on a page and some piece of data on that
 * page is the subject of a ll/sc, we can't just skip over it since
 * by definition the sc will fail. In this case we return a FAULTSCWATCH.
 * If the kernel hits one of these we already just mark it and continue
 * so that shouldn't be a problem.
 *
 * If the watchpoint is on the page of instructions we need to do
 * the same thing if the skipped instruction is an sc. But this is hard
 * since we can't reliably read the instruction - the page obviously isn't
 * valid so we can't just fuiword... We need a solution to this someday...
 */
#if TFP
/*
 * On a TlbRefil/TlbInval exception on an instruction fetch, TFP does 
 * not set badvaddr = epc. It sets badvaddr to an ICACHE_LINESIZE
 * aligned address. EXEC watchpoint handling uses a workaround for
 * this which can interfere if an user also has READ watchpoints on
 * an instruction word. This is because there is no way to tell whether
 * the read miss is due to an instruction fetch or a data fetch.
 */
#include <sys/tfp.h>
#include <sys/reg.h>
#include <ksys/exception.h>
#endif

int
chkwatch(
	uvaddr_t vaddr,
	uthread_t *ut,
	int flags,
	int size,	/* # bytes in memory access */
	ulong rw,
	int *type)
{
	watch_t *wp;
	int rv;
	pwatch_t *pw = ut->ut_watch;

#if TFP
	eframe_t 	*ep = &(ut->ut_exception->u_eframe);
	uvaddr_t	epc = (caddr_t)ep->ef_epc;
	uvaddr_t	badva = vaddr;

	if (epc > badva)
		if ((epc - badva) < ICACHE_LINESIZE) {
			vaddr = epc;
			rw = W_EXEC;
		}
#endif	/* TFP */

	ASSERT(IS_KUSEG(vaddr));
	if (!(flags & ASF_FROMUSER)) {
		/*
		 * Optimization - are we here because we are trying
		 * to install a wpt sstep on a page which we are
		 * already stepping over? This would happen in case
		 * of EXEC wpts when we try to write a sstep at the
		 * next instruction. Be careful that we are not breaking
		 * the case of a data wpt tripping on an EXEC wpt page,
		 * which does an extraneous install_bp at the end of
		 * the data v/pfault.
		 */
		if (pw) {
			if ((pnum(vaddr) == pnum(pw->pw_skippc)) &&
				(pw->pw_skippc == pw->pw_skipaddr)) {
				return(0);
			}
			pw->pw_flag |= WINSYS;
		}
		/* we ALWAYS need to re-establish watchpoints if we ever
		 * get one in the system since we are going to silently
		 * step over it.
		 * We set this bit so that when the process goes back to user
		 * land we have a hook to get these stepped over watchpoints
		 * reset
		 */
		ut_flagset(ut, UT_WSYS);
	}

	/*
	 * if noone tracing us, then we're definitely not interested
	 * in watchpoint
	 */
	if (!(flags & ASF_PTRACED) || pw == NULL)
		return(0);

	/*
	 * allow sections of system code to guarantee that they won't
	 * cause an 'interesting' watchpoint fault
	 */
	if (pw->pw_flag & WIGN)
		return(0);

	if (size < 0)
		return 0;
	ASSERT(size);
	rv = 0;
	for (wp = pw->pw_list; wp; wp = wp->w_next) {
		if ((rw & wp->w_mode) == 0)
			continue;
		if ((vaddr + size) > wp->w_vaddr &&
		    vaddr < (wp->w_vaddr + wp->w_length)) {
			/* found one! */
			rv = 1;
			/* save info for /proc */
			pw->pw_curmode = rw;
			pw->pw_curaddr = vaddr;
			pw->pw_cursize = size;
			*type = FAULTEDWATCH;

			/* if we're currently stepping over an uninterested
			 * watchpoint, and now hit one we're interested in,
			 * give back indication so that the skipped
			 * watchpoint can be properly re-instated
			 */
			if (pw->pw_skipaddr != NULLWA) {
				rv = 2;
			}

			/* if from kernel mode just set bit */
			if (!(flags & ASF_FROMUSER)) {
				pw->pw_flag |= WINTSYS;
				/* remember 1st one */
				if (pw->pw_firstsys.w_length == 0) {
					pw->pw_firstsys.w_vaddr = vaddr;
					pw->pw_firstsys.w_length = size;
					pw->pw_firstsys.w_mode = rw;
				}
				return(0);
			}
			return(rv);
		}
	}
	/*
	 * Not interested, but if the instruction was an sc we
	 * need help so return a special indicator
	 */
	if ((flags & ASF_ISSC) && (flags & ASF_FROMUSER)) {
		rv = 1;
		*type = FAULTEDSCWATCH;
		/* save info for /proc */
		pw->pw_curmode = rw;
		pw->pw_curaddr = vaddr;
		pw->pw_cursize = size;
		ASSERT(pw->pw_skipaddr == NULLWA); 
		return rv;
	}
	return(0);
}

/*
 * clrsyswatch - clear out any watchpoints encountered in the system
 * Returns - 1 if watchpoint is interesting
 *	     0 if not (/proc should not be informed)
 * We could be here cause we're trying to step over a watchpoint,
 * and the single step breakpoints landed in a watched text page, thus the
 * WINSYS bit would be on, but we sure shouldn't re-activate the watchpoint!
 *
 * If we're not in the system then
 *      we re-establish ALL watchpoints, since we may
 *	have silently deactivated any number of them while in the
 *	system (either due to a system call such as read() or simply
 *	because we set a couple of single step breakpoints)
 */
static int
_clrsyswatch(pas_t *pas, ppas_t *ppas)
{
	register int rv = 0;
	uthread_t *ut = curuthread;
	register pwatch_t *pw = ut->ut_watch;
	register watch_t *wp;

	if (pw && ((pw->pw_flag & WSTEP) != 0)) {
		/* defer any WINSYS stuff until after step */
#ifdef DEBUG
		if (pwa)
		cmn_err(CE_CONT, "clrsys while stepping over 0x%x\n",
				pw->pw_skipaddr);
#endif
		return(0);
	}
	ut_flagclr(ut, UT_WSYS);
	if (!pw)
		return(0);
	if (pw->pw_flag & WINTSYS) {
		/* got a watchpoint that we're interested in */
		ASSERT(pw->pw_firstsys.w_length);
		pw->pw_curmode = pw->pw_firstsys.w_mode;
		pw->pw_curaddr = pw->pw_firstsys.w_vaddr;
		pw->pw_cursize = pw->pw_firstsys.w_length;
		pw->pw_firstsys.w_length = 0;
		rv = 1;
	} else {
		/* got a watchpoint that we're not interested in */
		ASSERT(pw->pw_firstsys.w_length == 0);
	}
	pw->pw_flag &= ~(WINSYS|WINTSYS);

	/*
	 * If we're going to run process
	 * go through all watchpoints and re-instate them
	 */
	mraccess(&pas->pas_aspacelock);
	for (wp = pw->pw_list; wp; wp = wp->w_next) {
		setwatch(pas, ppas, wp);
		ASSERT((pw->pw_flag & WSTEP) == 0);
	}
	mrunlock(&pas->pas_aspacelock);
	return(rv);
}

/*
 * skipwatch - skip over a watched page
 * aspacelock(ACCESS) already set
 * and region is locked
 *
 * 1) validate page
 * 2) single step
 * 3) invalidate page
 * If fault is from system mode, simply return - we'll pick it up when
 *		we leave the system
 * If EPC is not in a steppable region, return -1,
 * 	the caller must deal with it.
 * Can have 2 watchpoints to step over - one for the instruction
 *	one for the data its touching
 * Returns: 0 - if ok to proceed
 *	   -1 - if cannot plant a single step at EPC
 */
/* ARGSUSED */
int
skipwatch(pas_t *pas,
	ppas_t *ppas,
	uthread_t *ut,
	uvaddr_t vaddr,
	int flags,
	uvaddr_t epc,
	ulong rw,
	attr_t *attr,
	reg_t *locked_rp)
{
	register pwatch_t *pw = ut->ut_watch;

	ASSERT(attr->attr_end == attr->attr_start + NBPP);

	ASSERT(mrislocked_access(&pas->pas_aspacelock) || \
		(mrislocked_update(&pas->pas_aspacelock)));

	/* if from kernel mode just turn off watch bit & return */
	if (!(flags & ASF_FROMUSER)) {
		ASSERT(pw == NULL || pw->pw_flag & WINSYS || 
			((pw->pw_skippc == pw->pw_skipaddr) 
				&& (pnum(vaddr) == pnum(pw->pw_skippc))));
		ASSERT(attr->attr_watchpt == 1);
		return(0);
	}

	/* if no longer traced, slowly get rid of watchpoints */
	if (!(flags & ASF_PTRACED) || pw == NULL) {
		attr->attr_watchpt = 0;
		return(0);
	}

	if (!ispcwriteable(pas, ppas, epc, locked_rp)) {
		return(-1);
	}

	/*
	 * It is possible that after we drop in a tlb entry for this 
	 * false wpt, vhand runs and clears off the entry before we 
	 * have had a chance to execute the instruction. In that case,
	 * we will re-enter this code with the same value of epc and
	 * badvaddr. The code below handles that eventuality. Another
	 * reason we might have to reenter this code is that the tlb
	 * that we dropped in was randomly replaced by someone else
	 * before we had a chance to execute, and the utlbmiss handler
	 * could not pull in a valid entry from the page table. Whether
	 * the access was from user or kernel mode, the accessing code
	 * might have to fault again if the tlb entry has disappeared.
	 * Another reason for faulting multiple times on the same
	 * instruction is that the instruction is a "sw", which might
	 * first lead to a tlbmiss, and thence to a tlbmod.
	 */

	pw->pw_flag |= WSTEP;

	ASSERT(attr->attr_watchpt == 1);

	if (pw->pw_skipaddr == vaddr) {
	} else if (pw->pw_skipaddr == NULLWA) {
		pw->pw_skipaddr = vaddr;
		pw->pw_skippc = epc;
	} else {
		pw->pw_skipaddr2 = vaddr;
#ifdef DEBUG
		if (pwa)
		cmn_err(CE_CONT, "skipover 2 watchpoints 0x%x & 0x%x\n",
			pw->pw_skipaddr, pw->pw_skipaddr2);
#endif
	}
	return(0);
}

/*
 * reset watchpoint - ignore errors
 * We can get away with claiming only 1 byte since all we really
 * want to do is re-watch the current page
 * We now treat WRITE watchpoints differently by just turning off
 * the mod bit - so when reestablishing watchpoints on a page we
 * need to know what kind of watchpoints have been set.
 * If just a WRITE watchpoint, then just turning off mod bit is
 * sufficient. However, if there are any READ/EXEC watchpoints also
 * on the page, we need to mark the page invalid.
 */
static void
resetwatch(pas_t *pas, ppas_t *ppas, caddr_t vaddr, caddr_t vaddr2)
{
	/* REFERENCED */
	int rv;
	int toset;
	register watch_t *wp;
	pgno_t vpn, vpn2;
	uthread_t *ut = curuthread;
	register pwatch_t *pw = ut->ut_watch;

	ASSERT(mrislocked_access(&pas->pas_aspacelock) || \
		(mrislocked_update(&pas->pas_aspacelock)));
	if (vaddr2 != NULLWA)
		toset = 2;
	else
		toset = 1;
	vpn = btoct(vaddr);
	vpn2 = btoct(vaddr2);
	for (wp = pw->pw_list; toset && wp; wp = wp->w_next) {
		pgno_t wvpn = btoct(wp->w_vaddr);
		pgno_t wvpne = btoct(wp->w_vaddr + wp->w_length);

		if (vpn >= wvpn && vpn <= wvpne) {
			toset--;
			rv = setwatch(pas, ppas, wp);
#ifdef DEBUG
			if (rv)
				cmn_err(CE_WARN,
				"Cannot reset watchpoint for uthread 0x%x @ 0x%x",
				ut, wp->w_vaddr);
#endif
		}
		if ((vaddr2 != NULLWA) && vpn2 >= wvpn && vpn2 <= wvpne) {
			toset--;
			rv = setwatch(pas, ppas, wp);
#ifdef DEBUG
			if (rv)
				cmn_err(CE_WARN,
				"Cannot reset watchpoint for uthread 0x%x @ 0x%x",
				ut, wp->w_vaddr);
#endif
		}
		ASSERT((pw->pw_flag & WSTEP) == 0);
	}
	/*
	 * It is entirely possible that one thread is skipping over a 
	 * watchpoint, whereas another thread has cancelled the wpt.
	 * It is not neccesary to crib about this condition.
	 */
}

/*
 * cancelskipwatch - cancel an outstanding skipover of a watchpoint
 * This involves re-instating the 1 or 2 watchpoints slated to be skipped over
 */
void
ascancelskipwatch(pas_t *pas, ppas_t *ppas)
{
	uthread_t *ut = curuthread;
	register pwatch_t *pw = ut->ut_watch;

	ASSERT(mrislocked_access(&pas->pas_aspacelock) || \
		(mrislocked_update(&pas->pas_aspacelock)));
	if (pw->pw_skipaddr == NULLWA)
		return;

	pw->pw_flag &= ~(WSTEP|WSETSTEP);
	resetwatch(pas, ppas, pw->pw_skipaddr, pw->pw_skipaddr2);
	pw->pw_skipaddr = NULLWA;
	pw->pw_skipaddr2 = NULLWA;
}

/*
 * deleteallwatch - remove all watchpoint from a thread
 * We may be in the process of stepping - since we are called from exit
 * that may have resulted from fatal signal
 */
void
deleteallwatch(pas_t *pas, ppas_t *ppas, int lastone)
{
	uthread_t *ut = curuthread;
	register pwatch_t *pw = ut->ut_watch;
	register watch_t *wp, *nwp;

	ASSERT(mrislocked_update(&pas->pas_aspacelock));
	if (!pw)
		return;
	/*
	 * Non pthread apps do the freeing at lastone = 0.
	 */
	ASSERT((pas->pas_flags & PAS_PSHARED) || (lastone == 0));

	/*
	 * Pthread apps wait till lastone = 1 or 2 to do freeing.
	 */
	if (pas->pas_flags & PAS_PSHARED) {
		if (lastone == 0) return;
		else lastone--;
	}
	if (!(pas->pas_flags & PAS_PSHARED) || (lastone)) {
		ascancelskipwatch(pas, ppas);
		for (wp = pw->pw_list; wp; wp = nwp) {
			(void) clrwatch(pas, ppas, wp);
			nwp = wp->w_next;
			kmem_free(wp, sizeof *wp);
		}
	}
	ut_flagclr(ut, UT_WSYS);
	ut->ut_watch = NULL;
	kmem_free(pw, sizeof *pw);
}
