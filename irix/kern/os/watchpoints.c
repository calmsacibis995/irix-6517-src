/**************************************************************************
 *									  *
 * 		 Copyright (C) 1991-1992 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.44 $"

#include "bstring.h"
#include "sys/types.h"
#include "ksys/as.h"
#include "sys/cmn_err.h"
#include "sys/debug.h"
#include "ksys/exception.h"
#include "sys/param.h"
#include "sys/errno.h"
#include "sys/kmem.h"
#include "sys/proc.h"
#include "sys/sema.h"
#include "sys/systm.h"
#include "sys/sysmacros.h"
#include "sys/uthread.h"
#include "sys/watch.h"
#include "sys/buf.h"

#define NULLWA	((caddr_t)-1L)

int pwa = 0;

/*
 * Watchpoints
 */
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
int
clrsyswatch(void)
{
	vasid_t vasid;
	as_watch_t asw;

	as_lookup_current(&vasid);
	return VAS_WATCH(vasid, AS_WATCHCLRSYS, &asw);
}

/*
 * clr3rdparty - if a 3rd party inflicted watchpoints on itself
 * by accessing another process'es watched address - it may wish to
 * reestablish those watchpoints
 * For now this only handles 3rd parties that hit watchpoints while in the
 * system (/proc)
 */
void
clr3rdparty(uthread_t *ut, vasid_t targvasid)
{
	register uthread_t *callu = curuthread;
	register watch_t *wp;
	register pwatch_t *pw;
	as_watch_t asw;

	if (callu->ut_flags & UT_WSYS) {
		ut_flagclr(callu, UT_WSYS);
		if (callu->ut_watch)
			callu->ut_watch->pw_flag &= ~WINSYS;
	}

	/*
	 * now go through target watchpoint list and reactivate 
	 * Caller better have targvasid's aspacelock for update
	 */
	if (pw = ut->ut_watch) {
		for (wp = pw->pw_list; wp; wp = wp->w_next) {
			asw.as_setclrwatch_where = *wp;
			VAS_WATCH(targvasid, AS_WATCHSET, &asw);
		}
	}
}

/*
 * cancelskipwatch - cancel an outstanding skipover of a watchpoint
 * This involves re-instating the 1 or 2 watchpoints slated to be skipped over
 */
void
cancelskipwatch(void)
{
	vasid_t vasid;
	as_watch_t asw;
	uthread_t *ut = curuthread;
	register pwatch_t *pw = ut->ut_watch;

	/*
	 * Sync the UT_STEP bit with the WSETSTEP bit.
	 */
	if (pw->pw_flag & WSETSTEP) {
		ut_flagclr(ut, UT_STEP);
		PCB(pcb_ssi.ssi_cnt) = 0;
	}
	ASSERT(PCB(pcb_ssi.ssi_cnt) == 0);
	as_lookup_current(&vasid);
	(void) VAS_WATCH(vasid, AS_WATCHCANCELSKIP, &asw);
}

/*
 * checkwatchstep - we want to skip/step a watchpoint, we need to
 * decide whether we need to set a single step or whether for other
 * reasons a single step has already been requested
 * We can easily get here with WSETSTEP already set - if for example
 *	we are stepping over page that does not yet have write permission.
 * Note that WSETSTEP is turned off whenever we stop (see sig.c/stop())
 *
 * Also, due to a signal or /proc intervention, the location the
 * user program is going back to may not correspond to where we
 * need to step. In this case, cancel any skipping (and re-activate any
 * skipped pages), and let the process hit the watchpoint again if
 * necessary.
 */
void
checkwatchstep(eframe_t *ep)
{
	uthread_t *ut = curuthread;
	register pwatch_t *pw = ut->ut_watch;

	ASSERT(pw->pw_skipaddr != NULLWA);

	/*
	 * Are we going back from a trap we took just before executing
	 * a sstep brk pt?
	 */
	if (PCB(pcb_ssi.ssi_cnt) != 0) {
		/*
		 * If UT_STEP got cleared in procfs, ssi_cnt would also.
		 */
		ASSERT(ut->ut_flags & UT_STEP);
		return;
	}
	if (pw->pw_skippc != (caddr_t)ep->ef_epc) {
#ifdef DEBUG
		if (pwa)
		cmn_err(CE_CONT, "skippc 0x%x != EPC 0x%x - ignoring skip\n",
			pw->pw_skippc, ep->ef_epc);
#endif
		/*
		 * It is possible that we are in the midst of a step, and
		 * took a second exception on the same instruction (trivially
		 * possible for sw). If we are not returning to the faulting
		 * instruction, take off the step bit in cancelskipwatch. 
		 * Bug 533231 has details.
		 */
		cancelskipwatch();
	} else if (!(ut->ut_flags & UT_STEP)) {
		ut_flagset(ut, UT_STEP);
		pw->pw_flag |= WSETSTEP;
	}
}

/*
 * stepoverwatch - we have single stepped over a watchpoint - re-instate it
 * Returns - 1 if we set the single step just to pass a watchpoint and therefore
 * no signal should be generated.
 */
int
stepoverwatch(void)
{
	uthread_t *ut = curuthread;
	register pwatch_t *pw = ut->ut_watch;
	int retval = 0;

	ASSERT((pw->pw_flag & WINTSYS) == 0);
	ASSERT(pw->pw_skipaddr != NULLWA);
	if (pw->pw_flag & WSETSTEP) {
		retval = 1;	/* we set single stepping */
	}
	cancelskipwatch();
	/* since we're done stepping, reinstate any system encountered
	 * watchpoints
	 */
	if (clrsyswatch())
		cmn_err_tag(94,CE_PANIC, "stepover Interested watchpoint!");
	return(retval);
}

/*
 * Note on multithread/pthread watchpoints: For the first watchpoint
 * set, all existing members must get a per thread structure in their
 * ut_watch. All new members must also get a per thread structure (done
 * by passwatch). All the thread watchpoints have the same pw_list, 
 * which must persist till the last thread's life. The per thread
 * structure is freed in deleteallwatch, which also frees the shared
 * watch list depending on the last address space reference count.
 * For this reason, whenever the per thread data is being allocated
 * for each pthread (addwatch), or the pw_list is being modified
 * (addwatch/deletewatch), the uscan_access lock is held. The
 * aspacelock protects against watch list update and scanning by
 * a faulted thread.
 */
/*
 * addwatch - add a watchpoint to a process
 * the aspacelock(UPDATE) protects the watchpoint list
 * If process currently stepping over a watchpoint - return EBUSY
 * If we permitted this, we could end up resetting the page we're
 * trying to step over back invalid again..
 */
int
addwatch(uthread_t *ut, caddr_t vaddr, ulong size, ulong mode)
{
	register watch_t *wp;
	register int i;
	register pwatch_t *pw;
	register int err = 0;
	vasid_t vasid;
	as_watch_t asw;
	proc_proxy_t	*prxy = ut->ut_pproxy;
	uthread_t	*tut;

	if (as_lookup(ut->ut_asid, &vasid))
		return ENOMEM;

	VAS_LOCK(vasid, AS_EXCL);
	uscan_access(prxy);

	/*
	 * For each thread in the proc, allocate a watch data struct
	 */
	if ((pw = ut->ut_watch) == NULL) {
		uscan_forloop(prxy, tut) {
			if ((pw = kmem_zalloc(sizeof(*pw), KM_SLEEP)) 
								== NULL) {
				/*
				 * On a midway failure, free all allocated
				 * till now.
				 */
				uscan_forloop(prxy, tut) {
					if (tut->ut_watch) {
						kmem_free(tut->ut_watch, 
								sizeof(*pw));
						tut->ut_watch = NULL;
					}
				}
				err = ENOMEM;
				goto out;
			}
			pw->pw_skipaddr = NULLWA;
			pw->pw_skipaddr2 = NULLWA;
			tut->ut_watch = pw;
		}
	} else {
		/*
		 * check for overlapping watchpoints - these are illegal
		 * Also count em up
		 */
		for (i = 0, wp = pw->pw_list; wp; i++, wp = wp->w_next) {
			if ((vaddr + size) > wp->w_vaddr &&
			    vaddr < (wp->w_vaddr + wp->w_length)) {
				err = EINVAL;
				goto out;
			}
		}
		if (i >= maxwatchpoints) {
			err = E2BIG;
			goto out;
		}
	}
	if ((wp = kmem_alloc(sizeof *wp, KM_SLEEP)) == NULL) {
		err = ENOMEM;
		goto out;
	}

	wp->w_vaddr = vaddr;
	wp->w_length = size;
	wp->w_mode = mode;

	asw.as_setclrwatch_where = *wp;
	/* process better be stopped! */
	if ((err = VAS_WATCH(vasid, AS_WATCHSET, &asw)) != 0) {
		/* failed */
		kmem_free(wp, sizeof *wp);
		goto out;
	}
	wp->w_next = pw->pw_list;
	uscan_forloop(prxy, tut) {
		tut->ut_watch->pw_list = wp;
	}
out:
	uscan_unlock(prxy);
	VAS_UNLOCK(vasid);
	as_rele(vasid);
	return(err);
}

/*
 * deletewatch - delete a watchpoint to a process
 * Could be deleting watchpoint that
 *	a) target proc is just hitting
 *		since we hold the aspacelock(UPDATE) the target
 *		process will stop before realizing its a watched page
 *	b) target proc is stepping over (cause it hit the same page)
 * Returns - 0 if deleted anything,
 *		EINVAL if process has no watchpoints
 * 		ESRCH if couldn't find it
 *		EBUSY if process currently stepping over watchpoint
 */
int
deletewatch(uthread_t *ut, caddr_t vaddr)
{
	register watch_t *wp, *pwp;
	register pwatch_t *pw = ut->ut_watch;
	vasid_t vasid;
	as_watch_t asw;
	proc_proxy_t    *prxy = ut->ut_pproxy;
	uthread_t       *tut;

	if (as_lookup(ut->ut_asid, &vasid))
		return ENOMEM;

	if (pw == NULL) {
		as_rele(vasid);
		return(EINVAL);
	}

	VAS_LOCK(vasid, AS_EXCL);
	for (wp = pw->pw_list, pwp = NULL; wp; pwp = wp, wp = wp->w_next) {
		if (wp->w_vaddr == vaddr) {
			/*
			 * found it!
			 * Lets not worry about errors - its possible
			 * with share groups that the region is gone
			 * but the watchpoint isn't - lets just let
			 * folks clean up.
			 */
			asw.as_setclrwatch_where = *wp;
			VAS_WATCH(vasid, AS_WATCHCLR, &asw);
			if (pwp)
				pwp->w_next = wp->w_next;
			else {
				uscan_access(prxy);
				uscan_forloop(prxy, tut)
					tut->ut_watch->pw_list = wp->w_next;
				uscan_unlock(prxy);
			}
			kmem_free(wp, sizeof *wp);

			/*
			 * clrwatch simply shuts off the watch bits for
			 * all pages associated with wp - there may be
			 * other watchpoints that cross the same pages -
			 * we must re-validate them. Note that the process
			 * MUST be stopped for this to work or it could
			 * miss a watchpoint between the clrwatch above
			 * and here
			 */
			for (wp = pw->pw_list; wp; wp = wp->w_next) {
				asw.as_setclrwatch_where = *wp;
				VAS_WATCH(vasid, AS_WATCHSET, &asw);
			}
			VAS_UNLOCK(vasid);
			as_rele(vasid);
			return(0);
		}
	}
	VAS_UNLOCK(vasid);
	as_rele(vasid);
	return(ESRCH);
}

/*
 * getwatch - copyout watchpoint list
 * Sets count to # watchpoints copied out 
 */
int
getwatch(
	uthread_t *ut,
	int (*copyfunc)(caddr_t, ulong, ulong, caddr_t *),
	caddr_t targvaddr,
	int *count)
{
	register watch_t *wp;
	register int i;
	register pwatch_t *pw = ut->ut_watch;
	register int error;
	vasid_t vasid;

	if (as_lookup(ut->ut_asid, &vasid))
		return ENOMEM;

	if (pw == NULL) {
		as_rele(vasid);
		*count = 0;
		return(0);
	}
	VAS_LOCK(vasid, AS_SHARED);
	for (i = 0, wp = pw->pw_list; wp; i++, wp = wp->w_next)
		if (error = copyfunc(wp->w_vaddr, wp->w_length, wp->w_mode,
							&targvaddr)) {
			VAS_UNLOCK(vasid);
			as_rele(vasid);
			return(error);
		}
	VAS_UNLOCK(vasid);
	as_rele(vasid);
	*count = i;
	return(0);
}

/*
 * When a new pthread is created, pass on to the child any watch point
 * list from the parent. The uscan lock is held, so the parent will
 * have a stable list. The pwatch is freed in deleteallwatch().
 */
int
passwatch(uthread_t *put, uthread_t *cut)
{
	register pwatch_t 	*pw;
	int			error = 0;
#ifdef DEBUG
	proc_proxy_t    	*prxy = put->ut_pproxy;
#endif

	ASSERT(put->ut_flags & UT_PTHREAD);
	ASSERT(uscan_update_held(prxy));
	if (put->ut_watch) {
		if ((pw = kmem_zalloc(sizeof(*pw), KM_SLEEP)) == NULL) {
			error = ENOMEM;
		} else {
			pw->pw_skipaddr = NULLWA;
			pw->pw_skipaddr2 = NULLWA;
			pw->pw_list = put->ut_watch->pw_list;
			cut->ut_watch = pw;
		}
	}
	return(error);
}

int
intlb(uthread_t *ut, uvaddr_t vaddr, int rw)
{
	pde_t  pte1, pte2, pte;
	int   ret, s;
	extern int tlbfind(int, uvaddr_t, pde_t *, pde_t *);

	/*
	 * Protect against kernel preemption which
	 * might change tlbpid.
	 */
	s = splhi();
	ret = tlbfind(tlbpid(&ut->ut_as), vaddr, &pte1, &pte2);
	splx(s);
	if (ret != -1) {
#if TFP
		pte = pte1;
#else
		if ((btoct(vaddr)%2) == 0) pte = pte1;
		else pte = pte2;
#endif
		if (pg_ishrdvalid(&pte)) {
			if ((rw == B_WRITE) || (pg_ismod(&pte)))
				return (1);
		}
	}
	return(0);
}
