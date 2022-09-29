/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.39 $"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <ksys/vproc.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include "vproc_private.h"
#include <sys/idbgentry.h>
#include <sys/exec.h>
#include "proc_trace.h"

struct zone *vproc_zone;

#ifdef PROC_TRACE

void		idbg_vproc_trace(__psint_t);	/* Forward */
struct ktrace	*proc_trace;
pid_t		proc_trace_pid = -1;
unsigned	proc_trace_mask = 0x1f;
#endif

void
vproc_startup(void)
{
	vproc_zone = kmem_zone_init(sizeof(vproc_t), "vproc struct");

#ifdef PROC_TRACE
	proc_trace =  ktrace_alloc(1000, 0);
	idbg_addfunc("vproctrace", idbg_vproc_trace);
#endif
}

vproc_t *
vproc_create(void)
{
	vproc_t	*vp;

	vp = kmem_zone_zalloc(vproc_zone, KM_SLEEP);
	ASSERT(vp);

	VPROC_REFCNT_LOCK_INIT(vp, "vproc");
	vp->vp_refcnt = 0;

	vp->vp_state = SRUN;
	vp->vp_needwakeup = 0;

	/*
	 * Allocate a proc structure.
	 * Initialize behavior head
	 */
	bhv_head_init(VPROC_BHV_HEAD(vp), "vproc");
	return vp;
}

/* Called for failures in procnew, before the vproc is fully
 * instantiated.
 */
void
vproc_return(vproc_t *vp)
{
	ASSERT(vp->vp_refcnt == 0);
	VPROC_REFCNT_LOCK_DESTROY(vp);
	bhv_head_destroy(VPROC_BHV_HEAD(vp));
	kmem_zone_free(vproc_zone, vp);
}

void
vproc_quiesce(vproc_t *vp)
{
	int	spl;

	ASSERT(vp->vp_refcnt >= 1);
	if (vp->vp_refcnt != 1) {
		spl = VPROC_REFCNT_LOCK(vp);
		while (vp->vp_refcnt != 1) {
			vp->vp_needwakeup = 1;
			VPROC_REFCNT_WAIT(vp, spl);
			spl = VPROC_REFCNT_LOCK(vp);
		}
		VPROC_REFCNT_UNLOCK(vp, spl);
	}
}

/* Called to return a vproc and chained behavior structures, after the
 * vproc has been fully instantiated. At this point the vproc must not be
 * able to be looked up.
 */
void
vproc_destroy(vproc_t *vp)
{
	/*
	 * This is used to synchronize with vproc_release below
	 */
	vproc_quiesce(vp);

	PROC_TRACE2(PROC_TRACE_EXISTENCE, "vproc_destroy", vp->vp_pid);

	VPROC_DESTROY(vp);			/* clear up behaviors */

	VPROC_REFCNT_LOCK_DESTROY(vp);
	bhv_head_destroy(VPROC_BHV_HEAD(vp));
	kmem_zone_free(vproc_zone, vp);
}

int
vproc_reference(vproc_t *vp, int flag)
{
	ASSERT(vp->vp_refcnt >= 0);
	/*
	 * Deal with the common case first
	 */
	if (flag == ZNO) {
		if (vp->vp_state == SRUN) {
			vp->vp_refcnt++;
			return(1);
		} else
			return(0);
	}

	if (vp->vp_state == SINVAL)
		return(0);

	ASSERT((vp->vp_state == SRUN) || (vp->vp_state == SZOMB));
	ASSERT(flag == ZYES);

	vp->vp_refcnt++;
	return(1);
}

int
vproc_reference_exclusive(vproc_t *vp, int *spl)
{
	ASSERT(vp->vp_refcnt >= 0);

	if (vp->vp_state == SINVAL)
		return(0);

	ASSERT((vp->vp_state == SRUN) || (vp->vp_state == SZOMB));

	VPROC_REFCNT_UNLOCK(vp, *spl);
	BHV_WRITE_LOCK(VPROC_BHV_HEAD(vp));
	*spl = VPROC_REFCNT_LOCK(vp);

	vp->vp_refcnt++;
	return(1);
}

int
vproc_hold(
	vproc_t	*vp,
	int	flag)
{
	int	ref;
	int	spl;
	
	spl = VPROC_REFCNT_LOCK(vp);
	ref = vproc_reference(vp, flag);
	VPROC_REFCNT_UNLOCK(vp, spl);

	PROC_TRACE12(PROC_TRACE_REFERENCE, "vproc_hold", vp->vp_pid,
			"by", current_pid(),
			"refcnt", vp->vp_refcnt,
			"\n\tcalled from", __return_address,
			"flag", flag,
			"success", ref);

	return(ref ? 0 : ESRCH);
}

void
vproc_release(vproc_t *vp)
{
	int	dowake;
	int	spl;

	spl = VPROC_REFCNT_LOCK(vp);

	ASSERT(vp->vp_refcnt > 0);
	vp->vp_refcnt--;
	if (dowake = vp->vp_needwakeup)
		vp->vp_needwakeup = 0;
	VPROC_REFCNT_UNLOCK(vp, spl);

	PROC_TRACE10(PROC_TRACE_REFERENCE, "vproc_release", vp->vp_pid,
			"by", current_pid(),
			"refcnt", vp->vp_refcnt,
			"\n\tcalled from", __return_address,
			"waiter", dowake);

	if (dowake)
		VPROC_REFCNT_SIGNAL(vp);
}

void
vproc_set_state(
	vproc_t	*vp,
	int	state)
{
	int	spl;

	spl = VPROC_REFCNT_LOCK(vp);

	ASSERT(vp->vp_refcnt >= 1);

	while (vp->vp_refcnt != 1) {
		vp->vp_needwakeup = 1;
		VPROC_REFCNT_WAIT(vp, spl);
		spl = VPROC_REFCNT_LOCK(vp);
	}

	vp->vp_state = state;

	PROC_TRACE6(PROC_TRACE_STATE, "vproc_set_state", vp->vp_pid,
			"by", current_pid(),
			"state", state);

	VPROC_REFCNT_UNLOCK(vp, spl);
}

#ifdef	PROC_TRACE

void
idbg_vproc_trace(__psint_t x)
{
	__psint_t	id;
	int		idx;
	int		count;

	if (x == -1) {
		qprintf("Displaying all entries\n");
		idx = -1;
		count = 0;
	} else if (x < 0) {
		idx = -1;
		count = (int)-x;
		qprintf("Displaying last %d entries\n", count);
	} else {
		qprintf("Displaying entries for pid %d\n", x);
		idx = 1;
		id = x;
		count = 0;
	}
		
	ktrace_print_buffer(proc_trace, id, idx, count);
}

#endif
