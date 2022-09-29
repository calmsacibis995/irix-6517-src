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

#ident "$Id: vpgrp_lp.c,v 1.10 1997/05/15 15:04:49 cp Exp $"

#include <sys/types.h>
#include <ksys/pid.h>
#include <ksys/vpgrp.h>
#include <ksys/vsession.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include "vpgrp_private.h"


void
vpgrp_cell_init()
{
}

/*
 * Look for a vpgrp structure with the given id in a hash table queue
 * If not found, a vpgrp will be created if so requested. The vpgrp returned
 * is referenced.
 */
vpgrp_t *
vpgrp_lookup(
	pid_t		pgid)
{
	return vpgrp_lookup_local(pgid);
}

vpgrp_t *
vpgrp_create(
	pid_t	pgid,
	pid_t	sid)
{
	return vpgrp_create_local(pgid, sid);
}

/*
 * Called to return a vpgrp (and pgrp) structure, after the
 * vpgrp/pgrp has been fully instantiated. This vpgrp cannot
 * be looked-up at this point.
 */
void
vpgrp_destroy(vpgrp_t *vpg)
{
	vsession_t	*vsp;

	vsp = VSESSION_LOOKUP(vpg->vpg_sid);
	ASSERT(vsp);
	VSESSION_LEAVE(vsp, vpg);
	VSESSION_RELE(vsp);
	VSESSION_RELE(vsp);

	PID_PGRP_LEAVE(vpg->vpg_pgid);

	/*
	 * Call down the behavior chain to destroy all behaviors.
	 */
	VPGRP_DESTROY(vpg);

	VPGRP_REFCNT_LOCK_DESTROY(vpg);
	bhv_head_destroy(&vpg->vpg_bhvh);
	kmem_zone_free(vpgrp_zone, vpg);
}
