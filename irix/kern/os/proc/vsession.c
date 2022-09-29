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

#ident "$Id: vsession.c,v 1.12 1997/06/05 21:55:29 sp Exp $"

#include <sys/types.h>
#include <sys/atomic_ops.h>
#include <sys/proc.h>
#include <ksys/pid.h>
#include <ksys/vsession.h>
#include <ksys/vproc.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <sys/idbgentry.h>
#include "vsession_private.h"

#if DEBUG
void idbg_vsession_trace(__psint_t x);
#endif

extern int sesshashmask;

#if DEBUG
struct ktrace	*vsession_trace;
pid_t		vsession_trace_id = -1;
long		vsession_limbo = 0;
#endif

vsessiontab_t   *vsessiontab;
int             vsessiontabsz;
struct zone     *vsession_zone;
struct zone     *psession_zone;

void
vsession_init()
{
	int		i;
	vsessiontab_t	*vq;

#if DEBUG
	idbg_addfunc("vsessiontrace", idbg_vsession_trace);
#endif

	vsession_cell_init();		/* cell-specific init (if any) */

	vsession_zone = kmem_zone_init(sizeof(vsession_t), "vsession struct");

	/*
	 * Allocate and initialize the vsession hash table
	 */
	vsessiontabsz = sesshashmask;
	vsessiontab = (vsessiontab_t *)kern_malloc(sizeof(vsessiontab_t) * vsessiontabsz);
	ASSERT(vsessiontab != 0);
	for (i = 0; i < vsessiontabsz; i++) {
		vq = &vsessiontab[i];
		kqueue_init(&vq->vsesst_queue);
		mrinit(&vq->vsesst_lock, "vsesst");
	}

	/*
	 * Initialize the base physical table.
	 */
	psession_init();

#ifdef DEBUG
	vsession_trace =  ktrace_alloc(1000, 0);
#endif
}

vsession_t *
vsession_alloc(pid_t sid)
{
	vsession_t	*vsp;

	/*
	 * Allocate a vsession and session structures.
	 */
	vsp = kmem_zone_zalloc(vsession_zone, KM_SLEEP);

	vsp->vs_sid = sid;
	vsp->vs_refcnt = 0;
	VSESSION_REFCNT_LOCK_INIT(vsp);

	bhv_head_init(VSESSION_BHV_HEAD(vsp), "vsession");

	psession_create(vsp);

	return vsp;

}

vsession_t *
vsession_qlookup_locked(
	kqueue_t	*kq,
	pid_t		sid)
{
	vsession_t		*vsession;
	/*
	 * Step through the hash queue looking for the given id
	 */
	for (vsession = (vsession_t *)kqueue_first(kq);
	     vsession != (vsession_t *)kqueue_end(kq);
	     vsession = (vsession_t *)kqueue_next(&vsession->vs_queue)) {
		if (sid == vsession->vs_sid) {
			return vsession;
		}
	}
	return NULL;
}

/*
 * Look for a vsession structure with the given id in a hash table queue
 * The vsession returned is referenced.
 */
vsession_t *
vsession_lookup_local(
	pid_t		sid)
{
	vsessiontab_t	*vq;
	vsession_t	*vsession;

	if (sid < 0 || sid > MAXPID)
		return NULL;

	/* Get the hash queue, lock it and do lookup. */
	vq = VSESSION_ENTRY(sid);
	VSESSIONTAB_LOCK(vq, MR_ACCESS);
	if (vsession = vsession_qlookup_locked(&vq->vsesst_queue, sid)) {
		/*
		 * Got it. Add reference to vsession and return
		 */
		VSESSION_HOLD(vsession);
	} else
		vsession = NULL;

	VSESSIONTAB_UNLOCK(vq);
	return vsession;
}

vsession_t *
vsession_create_local(
	pid_t		sid)
{
	vsessiontab_t	*vq;
	vsession_t	*vsession;

	if (sid < 0 || sid > MAXPID)
		return NULL;

	/* Get the hash queue, lock it and do lookup. */
	vq = VSESSION_ENTRY(sid);
	VSESSIONTAB_LOCK(vq, MR_UPDATE);
	if (vsession = vsession_qlookup_locked(&vq->vsesst_queue, sid)) {
		/*
		 * Got it. Add reference to vsession and return
		 */
		VSESSION_HOLD(vsession);
		VSESSIONTAB_UNLOCK(vq);
		return vsession;
	}

	/*
	 * Can't find it. Create and enter it into the
	 * hash table.
	 */
	vsession = vsession_alloc(sid);
	VSESSION_HOLD(vsession);
	kqueue_enter(&vq->vsesst_queue, &vsession->vs_queue);

	VSESSIONTAB_UNLOCK(vq);
	return vsession;
}


/*
 * Called to return a vsession (and psession) structure, after the
 * vsession/psession has been fully instantiated. This vsession cannot
 * be looked-up at this point.
 */
void
vsession_destroy(vsession_t *vsp)
{
	ASSERT(vsp->vs_refcnt == 0);

	VSESSION_DESTROY(vsp);
	VSESSION_REFCNT_LOCK_DESTROY(vsp);
	bhv_head_destroy(&vsp->vs_bhvh);
	kmem_zone_free(vsession_zone, vsp);
}


void
vsession_remove(
	vsession_t *vsp)
{
	vsessiontab_t	*vq;

	ASSERT(!kqueue_isnull(&vsp->vs_queue));

	SESSION_TRACE6("vsession_remove", vsp->vs_sid, "vsp", vsp,
			"\n\tcalled from", __return_address);

	/* Get the hash queue and lock it. */
	vq = VSESSION_ENTRY(vsp->vs_sid);
	VSESSIONTAB_LOCK(vq, MR_UPDATE);
	kqueue_remove(&vsp->vs_queue);
	kqueue_null(&vsp->vs_queue);
	VSESSIONTAB_UNLOCK(vq);
#if DEBUG
	atomicAddLong(&vsession_limbo, 1);
#endif
}


void
vsession_enter(vsession_t *vsession)
{
	vsessiontab_t	*vq;

	/* Get the hash queue and lock it. */
	vq = VSESSION_ENTRY(vsession->vs_sid);
	VSESSIONTAB_LOCK(vq, MR_UPDATE);
	ASSERT(vsession->vs_refcnt > 0);
	ASSERT(!vsession_qlookup_locked(&vq->vsesst_queue, vsession->vs_sid));
	kqueue_enter(&vq->vsesst_queue, &vsession->vs_queue);
	VSESSIONTAB_UNLOCK(vq);

}


/* ARGSUSED */
void
vsession_ref(vsession_t *vsp)
{
	int	spl;

	spl = VSESSION_REFCNT_LOCK(vsp);
	vsp->vs_refcnt++;
	VSESSION_REFCNT_UNLOCK(vsp, spl);
}

/* ARGSUSED */
void
vsession_unref(vsession_t *vsp)
{
	int	spl;
	int	refcnt;

	spl = VSESSION_REFCNT_LOCK(vsp);
	ASSERT(vsp->vs_refcnt > 0);
	refcnt = --vsp->vs_refcnt;
	VSESSION_REFCNT_UNLOCK(vsp, spl);
	if (refcnt == 0 && !VSESSION_IS_VALID(vsp)) {
		/*
		 * Last use - tear down.
		 * Once removed from the hash table,
		 * noone can be referencing it.
		 */
		vsession_destroy(vsp);
#if DEBUG
		atomicAddLong(&vsession_limbo, -1);
#endif
	}
}

#if DEBUG
void
idbg_vsession_trace(__psint_t x)
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
		qprintf("Displaying entries for sid %d\n", x);
		idx = 1;
		id = x;
		count = 0;
	}
		
	ktrace_print_buffer(vsession_trace, id, idx, count);
}
#endif	/* DEBUG */
