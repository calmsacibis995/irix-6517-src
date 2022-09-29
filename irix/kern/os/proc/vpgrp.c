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

#ident "$Id: vpgrp.c,v 1.26 1997/06/27 18:48:36 cp Exp $"

#include <sys/types.h>
#include <sys/atomic_ops.h>
#include <sys/proc.h>
#include <ksys/pid.h>
#include <ksys/vpgrp.h>
#include <ksys/vsession.h>
#include <ksys/vproc.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <sys/idbgentry.h>
#include <sys/ktrace.h>
#include "vpgrp_private.h"

vpgrptab_t      *vpgrptab;
int             vpgrptabsz;
struct zone     *vpgrp_zone;
struct zone     *pgrp_zone;

#if DEBUG
void idbg_vpgrp_trace(__psint_t x);
#endif

extern int pghashmask;

#if DEBUG
struct ktrace	*vpgrp_trace;
pid_t		vpgrp_trace_id = -1;
#endif

void
vpgrp_init()
{
	int		i;
	vpgrptab_t	*vq;

#if DEBUG
	idbg_addfunc("vpgrptrace", idbg_vpgrp_trace);
#endif

	vpgrp_cell_init();		/* cell-specific init (if any) */

	vpgrp_zone = kmem_zone_init(sizeof(vpgrp_t), "vpgrp struct");

	/*
	 * Allocate and initialize the vpgrp hash table
	 */
	vpgrptabsz = pghashmask;
	vpgrptab = (vpgrptab_t *)kern_malloc(sizeof(vpgrptab_t) * vpgrptabsz);
	ASSERT(vpgrptab != 0);
	for (i = 0; i < vpgrptabsz; i++) {
		vq = &vpgrptab[i];
		kqueue_init(&vq->vpgt_queue);
		mrinit(&vq->vpgt_lock, "vpgt");
	}

	/*
	 * Initialize the base physical table.
	 */
	ppgrp_init();

#ifdef DEBUG
	vpgrp_trace =  ktrace_alloc(1000, 0);
#endif
}

vpgrp_t *
vpgrp_alloc(
	pid_t	pgid,
	pid_t	sid)
{
	vpgrp_t	*vpg;

	/*
	 * Allocate a vpgrp and pgrp structures.
	 */
	vpg = kmem_zone_zalloc(vpgrp_zone, KM_SLEEP);

	vpg->vpg_pgid = pgid;
	vpg->vpg_sid = sid;
	vpg->vpg_refcnt = 0;
	VPGRP_REFCNT_LOCK_INIT(vpg);

	bhv_head_init(&vpg->vpg_bhvh, "vpgrp");

	ppgrp_create(vpg);

	return vpg;

}

vpgrp_t *
vpgrp_qlookup_locked(
	kqueue_t	*kq,
	pid_t		pgid)
{
	vpgrp_t		*vpgrp;
	/*
	 * Step through the hash queue looking for the given id
	 */
	for (vpgrp = (vpgrp_t *)kqueue_first(kq);
	     vpgrp != (vpgrp_t *)kqueue_end(kq);
	     vpgrp = (vpgrp_t *)kqueue_next(&vpgrp->vpg_queue)) {
		if (pgid == vpgrp->vpg_pgid) {
			return vpgrp;
		}
	}
	return NULL;
}

/*
 * Look for a vpgrp structure with the given id in a hash table queue
 * The vpgrp returned is referenced.
 */
vpgrp_t *
vpgrp_lookup_local(
	pid_t		pgid)
{
	vpgrptab_t	*vq;
	vpgrp_t		*vpgrp;

	if (pgid < 0 || pgid > MAXPID)
		return NULL;

	/* Get the hash queue, lock it and do lookup. */
	vq = VPGRP_ENTRY(pgid);
	VPGRPTAB_LOCK(vq, MR_ACCESS);
	if (vpgrp = vpgrp_qlookup_locked(&vq->vpgt_queue, pgid)) {
		/*
		 * Got it. Add reference to vpgrp and return
		 */
		VPGRP_HOLD(vpgrp);
	} else
		vpgrp = NULL;

	VPGRPTAB_UNLOCK(vq);
	return vpgrp;
}

vpgrp_t *
vpgrp_create_local(
	pid_t		pgid,
	pid_t		sid)
{
	vpgrptab_t	*vq;
	vpgrp_t		*vpgrp;
	vsession_t	*vsp;

	if (pgid < 0 || pgid > MAXPID)
		return NULL;

	/* Get the hash queue, lock it and do lookup. */
	vq = VPGRP_ENTRY(pgid);
	VPGRPTAB_LOCK(vq, MR_UPDATE);
	if (vpgrp = vpgrp_qlookup_locked(&vq->vpgt_queue, pgid)) {
		/*
		 * Got it. Add reference to vpgrp and return
		 */
		VPGRP_HOLD(vpgrp);
		VPGRPTAB_UNLOCK(vq);
		return vpgrp;
	}

	/*
	 * Create it and register all aspects.
	 */
	vpgrp = vpgrp_alloc(pgid, sid);
	VPGRP_HOLD(vpgrp);

	/*
	 * Register the pid as a process group id,
	 * and join the session to which it belongs.
	 */
	PID_PGRP_JOIN(pgid);
	vsp = VSESSION_LOOKUP(sid);
	ASSERT(vsp);
	VSESSION_JOIN(vsp, vpgrp);

	kqueue_enter(&vq->vpgt_queue, &vpgrp->vpg_queue);

	VPGRPTAB_UNLOCK(vq);
	return vpgrp;
}

void
vpgrp_remove(
	vpgrp_t *vpgrp)
{
	vpgrptab_t	*vq;

	ASSERT(!kqueue_isnull(&vpgrp->vpg_queue));

	PGRP_TRACE6("vpgrp_remove", vpgrp->vpg_pgid, "vpgrp", vpgrp,
			"\n\tcalled from", __return_address);

	/* Get the hash queue and lock it. */
	vq = VPGRP_ENTRY(vpgrp->vpg_pgid);
	VPGRPTAB_LOCK(vq, MR_UPDATE);
	kqueue_remove(&vpgrp->vpg_queue);
	kqueue_null(&vpgrp->vpg_queue);
	VPGRPTAB_UNLOCK(vq);
}

void
vpgrp_enter(vpgrp_t *vpgrp)
{
	vpgrptab_t	*vq;

	PGRP_TRACE6("vpgrp_enter", vpgrp->vpg_pgid, "vpgrp", vpgrp,
			"\n\tcalled from", __return_address);

	/* Get the hash queue and lock it. */
	vq = VPGRP_ENTRY(vpgrp->vpg_pgid);
	VPGRPTAB_LOCK(vq, MR_UPDATE);
	ASSERT(vpgrp->vpg_refcnt > 0);
	ASSERT(!vpgrp_qlookup_locked(&vq->vpgt_queue, vpgrp->vpg_pgid));
	kqueue_enter(&vq->vpgt_queue, &vpgrp->vpg_queue);
	VPGRPTAB_UNLOCK(vq);

}


void
vpgrp_ref(vpgrp_t *vpg)
{
	int	spl;
	
	spl = VPGRP_REFCNT_LOCK(vpg);
	vpg->vpg_refcnt++;
	VPGRP_REFCNT_UNLOCK(vpg, spl);
}

void
vpgrp_unref(vpgrp_t *vpg)
{
	int	spl;
	
	spl = VPGRP_REFCNT_LOCK(vpg);
	ASSERT(vpg->vpg_refcnt > 0);
	if (--vpg->vpg_refcnt == 0 && !VPGRP_IS_VALID(vpg)) {
		VPGRP_REFCNT_UNLOCK(vpg, spl);
		vpgrp_destroy(vpg);
	} else 
		VPGRP_REFCNT_UNLOCK(vpg, spl);
}

void
vpgrp_iterate(
	void (*func)(vpgrp_t *, void *),
	void *	arg)
{
	int		i;
	kqueue_t	*kq;
	vpgrptab_t	*vq;
	vpgrp_t		*vpgrp;


	/*
	 * Scan through the hash queues locking as we go to prevent
	 * entries changing and calling out to the specified routine.
	 */
	for (i = 0; i < vpgrptabsz; i++) {
		vq = &vpgrptab[i];
		VPGRPTAB_LOCK(vq, MR_ACCESS);
		kq = &vq->vpgt_queue;
		for (vpgrp = (vpgrp_t *)kqueue_first(kq);
		     vpgrp != (vpgrp_t *)kqueue_end(kq);
		     vpgrp = (vpgrp_t *)kqueue_next(&vpgrp->vpg_queue)) {
			vpgrp_ref(vpgrp);
			(*func)(vpgrp, arg);
			vpgrp_unref(vpgrp);
		}
	}

	/*
	 * Make second pass to release locks.
	 * This 2 pass technique is adequate for a consistent snapshot. 
	 */
	for (i = 0; i < vpgrptabsz; i++) {
		vq = &vpgrptab[i];
		VPGRPTAB_UNLOCK(vq);
	}
}

#ifdef DEBUG
void
idbg_vpgrp_trace(__psint_t x)
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
		qprintf("Displaying entries for pgid %d\n", x);
		idx = 1;
		id = x;
		count = 0;
	}
		
	ktrace_print_buffer(vpgrp_trace, id, idx, count);
}
#endif
