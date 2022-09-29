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

#ident "$Id: psession.c,v 1.7 1997/04/12 18:16:28 emb Exp $"

#include "sys/types.h"
#include "sys/param.h"
#include "sys/systm.h"
#include "sys/conf.h"
#include "sys/errno.h"
#include "sys/ksignal.h"
#include "sys/cred.h"
#include "pproc_private.h"
#include "ksys/vsession.h"
#include "sys/sema.h"
#include "sys/var.h"
#include "sys/debug.h"
#include "sys/sysinfo.h"
#include "sys/kmem.h"
#include "sys/cmn_err.h"
#include "ksys/pid.h"
#include "ksys/childlist.h"
#include <values.h>

#include <ksys/vproc.h>
#include "vsession_private.h"
#include "pproc.h"
#include <sys/vnode.h>

/*
 * Add a pgrp to a session
 */
/* ARGSUSED */
void
psession_join(
	bhv_desc_t	*bdp,
	vpgrp_t		*vpg)
{
	psession_t	*psp = BHV_TO_PSESSION(bdp);
	vsession_t	*vsp = BHV_TO_VSESSION(bdp);

	ASSERT(VSESSION_IS_VALID(vsp));
	SESSION_TRACE4("psession_join", vsp->vs_sid, "pgid", vpg->vpg_pgid);
	PSESSION_LOCK(psp);
	if (++psp->se_memcnt == 1)
		VSESSION_MEMBERSHIP(vsp, 1);
	PSESSION_UNLOCK(psp);
}


/*
 * Remove a pgrp from a session
 */
/*ARGSUSED*/
void
psession_leave(
	bhv_desc_t	*bdp,
	vpgrp_t		*vpg)
{
	psession_t	*psp = BHV_TO_PSESSION(bdp);
	/* REFERENCED */
	vsession_t	*vsp = BHV_TO_VSESSION(bdp);

	SESSION_TRACE4("psession_leave", vsp->vs_sid, "pgid", vpg->vpg_pgid);
	PSESSION_LOCK(psp);
	if (--psp->se_memcnt == 0)
		VSESSION_MEMBERSHIP(vsp, 0);
	PSESSION_UNLOCK(psp);
}


int
psession_ctty_alloc(
	bhv_desc_t	*bdp,
	vnode_t		*vp,
	void		(*ttycall)(void *, int),
	void		*ttycallarg)
{
	psession_t	*psp = BHV_TO_PSESSION(bdp);

	PSESSION_LOCK(psp);
	if (vp) {
		if (psp->se_ttyvp) {
			PSESSION_UNLOCK(psp);
			return EBUSY;
		}
		VN_HOLD(vp);
		psp->se_ttyvp = vp;
		ASSERT((psp->se_flag & SETTYCLOSE) == 0);
	}
	psp->se_ttycall = ttycall;
	psp->se_ttycallarg = ttycallarg;
	PSESSION_UNLOCK(psp);
	return 0;
}


void
psession_ctty_dealloc(
	bhv_desc_t	*bdp,
	int		mode)
{
	psession_t	*psp = BHV_TO_PSESSION(bdp);
	vsession_t	*vsp = BHV_TO_VSESSION(bdp);
	vnode_t		*vnp;

	PSESSION_LOCK(psp);
	if (psp->se_flag & SETTYCLOSE) {
		/* controlling tty already being closed */
		PSESSION_UNLOCK(psp);
		return;
	}
	if ((vnp = psp->se_ttyvp) != NULL) {
		psp->se_flag |= SETTYCLOSE;
		PSESSION_UNLOCK(psp);
		if (mode == SESSTTY_HANGUP)
			vn_kill(vnp);
		VSESSION_CTTY_WAIT(vsp, mode);
		PSESSION_LOCK(psp);
		/* a new ctty cannot be allocated until se_ttyvp == NULL */
		psp->se_ttyvp = NULL;
		VN_RELE(vnp);
		psp->se_flag &= ~SETTYCLOSE;
	}
	PSESSION_UNLOCK(psp);
}


int
psession_ctty_hold(
	bhv_desc_t	*bdp,
	vnode_t		**vpp)
{
	psession_t	*psp = BHV_TO_PSESSION(bdp);
	int		error = 0;

	PSESSION_LOCK(psp);
	if (vpp) {
		if (psp->se_flag & SETTYCLOSE || psp->se_ttyvp == NULL)
			error = ENOTTY;
		else {
			*vpp = psp->se_ttyvp;
			VN_HOLD(*vpp);
		}
	}
	if (error == 0)
		++psp->se_ttycnt;
	PSESSION_UNLOCK(psp);
	return error;
}


void
psession_ctty_rele(
	bhv_desc_t	*bdp,
	vnode_t		*vp)
{
	psession_t	*psp = BHV_TO_PSESSION(bdp);

	if (vp)
		VN_RELE(vp);

	PSESSION_LOCK(psp);
	if (--psp->se_ttycnt == 0)
		sv_broadcast(&psp->se_wait);
	PSESSION_UNLOCK(psp);
}


void
psession_ctty_wait(
	bhv_desc_t	*bdp,
	int		mode)
{
	psession_t	*psp = BHV_TO_PSESSION(bdp);
	void		(*ttycall)(void *, int);
	void		*ttycallarg;

	ttycall = psp->se_ttycall;
	ttycallarg = psp->se_ttycallarg;

	/*
	 * Get tty driver monitor.
	 * If this is a hangup, signal foreground process group.
	 */
	if (ttycall) {
		int cmds = TTYCB_GETMONITOR;

		if (mode == SESSTTY_HANGUP)
			cmds |= TTYCB_SIGFGPGRP;
		(*ttycall)(ttycallarg, cmds);
	}

	/*
	 * Wait for in-progress I/O to complete
	 */
	PSESSION_LOCK(psp);
	while (psp->se_ttycnt) {
		sv_wait(&psp->se_wait, PZERO, &psp->se_lock, 0);
		PSESSION_LOCK(psp);
	}
	PSESSION_UNLOCK(psp);

	/*
	 * Clean up tty driver session data structures and release
	 * monitor.
	 */
	if (ttycall)
		(*ttycall)(ttycallarg, TTYCB_RELSESS|TTYCB_RELMONITOR);
}


void
psession_ctty_devnum(
	bhv_desc_t	*bdp,
	dev_t		*devnump)
{
	int		error;
	psession_t	*psp = BHV_TO_PSESSION(bdp);
	vnode_t		*vnp;
	vattr_t		attr;

	PSESSION_LOCK(psp);
	if ((vnp = psp->se_ttyvp) == NULL) {
		*devnump = NODEV;
	} else {
		attr.va_mask = AT_RDEV;
		VOP_GETATTR(vnp, &attr, 0, sys_cred, error);
		if (error)
			*devnump = NODEV;
		else
			*devnump = attr.va_rdev;
	}
	PSESSION_UNLOCK(psp);
}
