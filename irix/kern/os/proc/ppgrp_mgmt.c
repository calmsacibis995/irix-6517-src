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

#ident "$Id: ppgrp_mgmt.c,v 1.29 1998/01/10 02:40:01 ack Exp $"

#include <sys/types.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/debug.h>
#include <ksys/pid.h>
#include <ksys/vproc.h>
#include <ksys/vpgrp.h>
#include <ksys/vsession.h>
#include "vpgrp_private.h"
#include <sys/space.h>

pgrp_ops_t ppgrp_ops = {
	BHV_IDENTITY_INIT_POSITION(VPGRP_POSITION_PP),
	ppgrp_getattr,		/* VPGRP_GETATTR */
	ppgrp_join_begin,		/* VPGRP_JOIN_BEGIN */
	ppgrp_join_end,		/* VPGRP_JOIN */
	ppgrp_leave,		/* VPGRP_LEAVE */
	ppgrp_detach,		/* VPGRP_DETACH */
	ppgrp_linkage,		/* VPGRP_LINKAGE */
	ppgrp_sendsig,		/* VPGRP_SENDSIG */
	ppgrp_sigseq,		/* VPGRP_SIGSEQ */
	ppgrp_sig_wait,		/* VPGRP_SIG_WAIT */
	ppgrp_setbatch,		/* VPGRP_SETBATCH */
	ppgrp_clearbatch,	/* VPGRP_CLEARBATCH */
	ppgrp_nice,		/* VPGRP_NICE (BSD setpriority/getpriority) */
	ppgrp_orphan,		/* VPGRP_ORPHAN (internal) */
	ppgrp_anystop,		/* VPGRP_ANYSTOP (internal) */
	ppgrp_membership,	/* VPGRP_MEMBERSHIP (internal) */
	ppgrp_destroy,		/* VPGRP_DESTROY (internal) */
};

#ifdef MH_R10000_SPECULATION_WAR
extern pfn_t   pfn_lowmem;
#endif

/*
 * ppgrp_init
 */
void
ppgrp_init(void)
{
	pgrp_zone = kmem_zone_init(sizeof(pgrp_t), "Process groups");
}

/*
 * ppgrp_create - allocate a new pgrp entry.  
 */
void
ppgrp_create(
	struct vpgrp	*vpg)
{
	pgrp_t *pg;

	pg = kmem_zone_zalloc(pgrp_zone, KM_SLEEP|VM_DIRECT);
	ASSERT(IS_KSEG0(pg)
#ifdef MH_R10000_SPECULATION_WAR
				|| (kvatopfn(pg) < pfn_lowmem)
#endif
	);

	pgrp_struct_init(pg, vpg->vpg_pgid);

	/* Initialize and insert behavior descriptor. */
	bhv_desc_init(&pg->pg_bhv, pg, vpg, &ppgrp_ops);
	bhv_insert_initial(&vpg->vpg_bhvh, &pg->pg_bhv);
}

/* Set up the variety of locks, etc. associated with a pgrp
 * structure.
 */
void
pgrp_struct_init(
	pgrp_t	*pg,
	pid_t	pgid)
{
	PGRP_LOCKINIT(pg, pgid);
	msinit(&pg->pg_lockmode, "pgrp_list");
	pg->pg_sigseq = 0;
	pg->pg_memcnt = 0;
	pg->pg_jclcnt = 0;
}

/*
 * This op is not called by distributed behaviors. It is called only
 * in the non-distributed case.
 */
void
ppgrp_membership(
	bhv_desc_t	*bdp,
	int		memcnt,
	pid_t		sid)
{
	struct vpgrp	*vpg = BHV_TO_VPGRP(bdp);

	if (memcnt == 0) {
		/*
		 * No members remain
		 * - remove the vpgrp from the lookup table.
		 * The pid deregistration and the vsession leave
		 * will happen in vpgrp_destroy.
		 */
		vpgrp_remove(vpg);
	}
}

void
ppgrp_destroy(bhv_desc_t *bdp)
{
	struct vpgrp	*vpg = BHV_TO_VPGRP(bdp);
	struct pgrp	*pg = BHV_TO_PPGRP(bdp);

	ASSERT(!VPGRP_IS_VALID(vpg));
	ASSERT(pg->pg_memcnt == 0);

	PGRP_LOCKDESTROY(pg);
	msdestroy(&pg->pg_lockmode);

	bhv_remove(&vpg->vpg_bhvh, &pg->pg_bhv);
	kmem_zone_free(pgrp_zone, pg);
}
