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
#ident "$Id: vsession_dp.c,v 1.12 1997/08/27 21:39:00 sp Exp $"

/*
 * Cell Session Management
 */

#include <sys/types.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/sema.h>

#include <ksys/cell/tkm.h>
#include <ksys/vsession.h>
#include <ksys/cell.h>
#include <ksys/cell/wp.h>

#include "dsession.h"
#include "dproc.h"
#include "invk_dssession_stubs.h"
#include "I_dcsession_stubs.h"
#include "vsession_private.h"

#include <ksys/cell/mesg.h>
#include <ksys/cell/subsysid.h>
#include "I_dssession_stubs.h"
#include "I_dcsession_stubs.h"

void
vsession_cell_init()
{
	mesg_handler_register(dssession_msg_dispatcher, DSSESSION_SUBSYSID);
	mesg_handler_register(dcsession_msg_dispatcher, DCSESSION_SUBSYSID);
}

/*
 * Lookup vsession by sid and optionally create it if the local cell is
 * responsible for it.
 */
vsession_t *
vsession_lookup(
	pid_t	sid)
{
	vsession_t	*vsp;
	int		error;
	int		retries = 0;

	/*
	 * First try our luck locally. We want to avoid calling White Pages
	 * quite yet.
	 */
retry:
	if ((vsp = vsession_lookup_local(sid)) != NULL)
		return(vsp);
	
	/*
	 * determine whether the local cell is
	 * responsible for this sid.
	 */
	if (sid_is_local(sid)) {
		/*
		 * We're responsible.
		 * This is lookup, it doesn't exist. Period.
		 */
		return(NULL);
	}

	/*
	 * We're doing a lookup on a remote vsession which may or may not
	 * exist - we'll have to ask
	 */
	error = dcsession_create(sid, sid_to_service(sid), &vsp);
	if (error == EAGAIN) {
		/*
		 * Must have lost a race with another thread to
		 * lookup this session. Try again.
		 */
		cell_backoff(++retries);
		goto retry;
	} else if (error) {
		/* Found nowhere */ 
		return(NULL);
	}

	return vsp;
}


vsession_t *
vsession_create(
	pid_t	sid)
{
	vsession_t	*vsp;
	int		error;
	int		retries = 0;

	/*
	 * First try our luck locally. We want to avoid calling White Pages
	 * quite yet.
	 */
retry:
	if (vsp = vsession_lookup_local(sid))
		return(vsp);
	
	/*
	 * determine whether the local cell is
	 * responsible for this sid.
	 */
	if (sid_is_local(sid)) {
		/*
		 * We're responsible.
		 * This is a create, and we can do it.
		 */
		return(vsession_create_local(sid));
	} 

	/* 
	 * We're attempting to create a vsession away from its origin
	 * cell - occurs if a process migrates/rexecs before
	 * creating is own session.
	 */
	vsp = dssession_create(sid);
	if (vsp != NULL)
		return(vsp);
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
	 * We're doing a lookup on a remote vsession which may or may not
	 * exist - we'll have to ask.
	 */
	error = dcsession_create(sid, sid_to_service(sid), &vsp);
	if (error == EAGAIN) {
		/*
		 * Must have lost a race with another thread to
		 * lookup this session. Try again.
		 */
		cell_backoff(++retries);
		goto retry;
	} else if (error) {
		/* Found nowhere */ 
		return(NULL);
	}

	return(vsp);
}
