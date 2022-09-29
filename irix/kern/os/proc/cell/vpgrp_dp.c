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
#ident "$Id: vpgrp_dp.c,v 1.24 1997/08/27 21:38:56 sp Exp $"

/*
 * Cell Process Group Management
 */

#include <sys/types.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/sema.h>

#include <ksys/pid.h>
#include <ksys/cell/tkm.h>
#include <ksys/vpgrp.h>
#include <ksys/vsession.h>
#include <ksys/cell/wp.h>

#include "dpgrp.h"
#include "invk_dspgrp_stubs.h"
#include "I_dcpgrp_stubs.h"
#include "vpgrp_private.h"
#include "pgrp_migrate.h"

#include <ksys/cell/mesg.h>
#include <ksys/cell/subsysid.h>
#include "I_dspgrp_stubs.h"
#include "I_dcpgrp_stubs.h"

void
vpgrp_cell_init()
{
	vpgrp_obj_init();

	mesg_handler_register(dcpgrp_msg_dispatcher, DCPGRP_SUBSYSID);
	mesg_handler_register(dspgrp_msg_dispatcher, DSPGRP_SUBSYSID);
}

/*
 * Lookup vpgrp by pgid and optionally create it if the local cell is
 * responsible for it.
 */
vpgrp_t *
vpgrp_lookup(
	pid_t	pgid)
{
	vpgrp_t		*vpg;
	int		error;
	int		retries = 0;

	/*
	 * First try our luck locally. We want to avoid calling White Pages
	 * quite yet.
	 */
retry:
	if (vpg = vpgrp_lookup_local(pgid))
		return(vpg);
	
	/*
	 * determine whether the local cell is
	 * responsible for this pgid.
	 */
	if (pgid_is_local(pgid)) {
		/*
		 * We're responsible.
		 * This is a lookup, it doesn't exist. Period.
		 */
		return(NULL);
	} 

	/*
	 * We're doing a lookup on a remote vpgrp which may or may not
	 * exist - we'll have to ask.
	 */
	error = dcpgrp_create(pgid, pgid_to_service(pgid), &vpg);
	if (error == EAGAIN) {
		/*
		 * Must have lost a race with another thread to
		 * lookup this pgrp. Try again.
		 */
		cell_backoff(++retries);
		goto retry;
	} else if (error) {
		/* Found nowhere */ 
		return(NULL);
	}

	return(vpg);
}

vpgrp_t *
vpgrp_create(
	pid_t	pgid,
	pid_t	sid)
{
	vpgrp_t		*vpg;
	int		error;
	int		retries = 0;

	PGRP_TRACE4("vpgrp_create", pgid, "sid", sid);

	/*
	 * First try our luck locally. We want to avoid calling White Pages
	 * quite yet.
	 */
retry:
	if (vpg = vpgrp_lookup_local(pgid)) {
		if (sid != vpg->vpg_sid) {
			VPGRP_RELE(vpg);
			return(NULL);
		}
		return(vpg);
	}
	
	/*
	 * determine whether the local cell is
	 * responsible for this pgid.
	 */
	if (pgid_is_local(pgid)) {
		/*
		 * We're responsible.
		 * If this is a lookup, it doesn't exist. Period.
		 * If this is a create, we can do it.
		 */
		vpg = vpgrp_create_local(pgid, sid);
		if (vpg) {
			if (sid != vpg->vpg_sid) {
				VPGRP_RELE(vpg);
				return(NULL);
			}
		}
		return(vpg);
	} 

	/* 
	 * We're attempting to create a vpgrp away from its origin
	 * cell - occurs if a process migrates/rexecs before
	 * creating is own pgrp.
	 */
	vpg = dspgrp_create(pgid, sid);
	if (vpg != NULL)
		return(vpg);
	/*
	 * We lost. This is either because another thread
	 * in this cell got there first, in which case we
	 * want to go back and do a local lookup, or
	 * because a thread in another cell got there first,
	 * in which case we want to try to create a dc.
	 * Unfortunately we can't tell the difference so
	 * we try to create the dc. If we lost the race to
	 * a local thread then this will fail and we go back
	 * to the start and do a local lookup, which should
	 * succeed by this time.
	 */
	
	/*
	 * We're doing a lookup on a remote vpgrp which may or may not
	 * exist - we'll have to ask.
	 */
	error = dcpgrp_create(pgid, pgid_to_service(pgid), &vpg);
	if (error == EAGAIN) {
		/*
		 * Must have lost a race with another thread to
		 * lookup this pgrp. Try again.
		 */
		cell_backoff(++retries);
		goto retry;
	} else if (error) {
		/* Found nowhere */ 
		return(NULL);
	}

	if (sid != vpg->vpg_sid) {
		VPGRP_RELE(vpg);
		return(NULL);
	}

	return(vpg);
}

/*
 * Called to return a vpgrp (and pgrp) structure, after the
 * vpgrp/pgrp has been fully instantiated.
 */
void
vpgrp_destroy(vpgrp_t *vpg)
{
	PGRP_TRACE2("vpgrp_destroy", vpg->vpg_pgid);

	/*
	 * Deregister pid as pgrp id.
	 */
	if (pgid_is_local(vpg->vpg_pgid)) {
		PID_PGRP_LEAVE(vpg->vpg_pgid);
	}

	/*
	 * Call down the behavior chain to destroy all behaviors.
	 */
	VPGRP_DESTROY(vpg);
	VPGRP_REFCNT_LOCK_DESTROY(vpg);
	bhv_head_destroy(&vpg->vpg_bhvh);
	kmem_zone_free(vpgrp_zone, vpg);
}
