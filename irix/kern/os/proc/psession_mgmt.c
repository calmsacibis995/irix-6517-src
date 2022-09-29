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

#ident "$Id: psession_mgmt.c,v 1.8 1997/10/08 03:59:17 cwj Exp $"

#include <sys/types.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <ksys/pid.h>
#include <ksys/vproc.h>
#include <ksys/vsession.h>
#include "pproc_private.h"
#include "vsession_private.h"

session_ops_t psession_ops = {
	BHV_IDENTITY_INIT_POSITION(VSESSION_POSITION_PS),
	psession_join,			/* VSESSION_JOIN */
	psession_leave,			/* VSESSION_LEAVE */
	psession_ctty_alloc,		/* VSESSION_CTTY_ALLOC */
	psession_ctty_dealloc,		/* VSESSION_CTTY_DEALLOC */
	psession_ctty_hold,		/* VSESSION_CTTY_HOLD */
	psession_ctty_rele,		/* VSESSION_CTTY_RELE */
	psession_ctty_wait,		/* VSESSION_CTTY_WAIT */
	psession_ctty_devnum,		/* VSESSION_CTTY_DEVNUM */
	psession_membership,		/* VSESSION_MEMBERSHIP (internal) */
	psession_destroy,		/* VSESSION_DESTROY (internal) */
};

/*
 * psession_init
 */
void
psession_init(void)
{
	psession_zone = kmem_zone_init(sizeof(psession_t), "Sessions");
}

#ifdef MH_R10000_SPECULATION_WAR
extern pfn_t   pfn_lowmem;
#endif

/*
 * psession_create - allocate a new psession entry.  
 */
void
psession_create(struct vsession *vsp)
{
	psession_t *psp;

	psp = kmem_zone_zalloc(psession_zone, KM_SLEEP|VM_DIRECT);
	ASSERT(IS_KSEG0(psp)
#ifdef MH_R10000_SPECULATION_WAR
				|| (kvatopfn(psp) < pfn_lowmem)
#endif
	);

	psession_struct_init(psp, vsp->vs_sid);

	/* Initialize and insert behavior descriptor. */
	bhv_desc_init(&psp->se_bhv, psp, vsp, &psession_ops);
	bhv_insert_initial(&vsp->vs_bhvh, &psp->se_bhv);

}


/*
 * This op is not called by distributed behaviors. It is called only
 * in the non-distributed case.
 */
void
psession_membership(
	bhv_desc_t	*bdp,
	int		memcnt)
{
	vsession_t	*vsp = BHV_TO_VSESSION(bdp);

	if (memcnt == 0) {
		/*
		 * No members remain
		 * - remove the vsession from the lookup table, and
		 * - deregister the pid.
		 */
		vsession_remove(vsp);

                PID_SESS_LEAVE(vsp->vs_sid);
	} else {
		/*
		 * The first member has joined
		 * - register the pid as a session id.
		 */
		PID_SESS_JOIN(vsp->vs_sid);
	}
}


void
psession_destroy(bhv_desc_t *bdp)
{
	struct vsession	*vsp = BHV_TO_VSESSION(bdp);
	struct psession	*psp = BHV_TO_PSESSION(bdp);

	ASSERT(!VSESSION_IS_VALID(vsp));
	ASSERT(psp->se_memcnt == 0);

	PSESSION_LOCKDESTROY(psp);
	sv_destroy(&psp->se_wait);

	bhv_remove(&vsp->vs_bhvh, &psp->se_bhv);
	kmem_zone_free(psession_zone, psp);
}

/* Set up the variety of locks, etc. associated with a psession
 * structure.
 */
void
psession_struct_init(psession_t *psp, pid_t sid)
{
	register int i = (int)sid;

	PSESSION_LOCKINIT(psp, i);
	init_sv(&psp->se_wait, SV_DEFAULT, "sessw", i);
}


void
psession_getstate(bhv_desc_t *bdp, int *nmemberp)
{
	struct psession	*psp = BHV_TO_PSESSION(bdp);

	*nmemberp = psp->se_memcnt;
}
