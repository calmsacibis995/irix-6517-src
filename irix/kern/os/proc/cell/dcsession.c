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
#ident "$Id: dcsession.c,v 1.12 1997/09/25 21:01:50 henseler Exp $"

/*
 * client-side object management for Session Management
 */

#include <sys/types.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/sema.h>
#include <sys/systm.h>
#include <sys/ksignal.h>
#include <values.h>

#include <ksys/behavior.h>
#include <ksys/cell/tkm.h>
#include <ksys/vproc.h>
#include <ksys/vsession.h>
#include <ksys/cell/wp.h>
#include <fs/cfs/cfs.h>

#include "pproc_private.h"

#include "dsession.h"
#include "invk_dssession_stubs.h"
#include "I_dcsession_stubs.h"
#include "vsession_private.h"

/*
 * Protos for client-side distributed layer.
 */
void	dcsession_join(bhv_desc_t *, vpgrp_t *);
void	dcsession_leave(bhv_desc_t *, vpgrp_t *);
int	dcsession_ctty_alloc(bhv_desc_t *, vnode_t *,
			     void (*)(void *, int), void *);
void	dcsession_ctty_dealloc(bhv_desc_t *, int);
int	dcsession_ctty_hold(bhv_desc_t *, vnode_t **);
void	dcsession_ctty_rele(bhv_desc_t *, vnode_t *);
void	dcsession_ctty_wait(bhv_desc_t *, int);
void	dcsession_ctty_devnum(bhv_desc_t *, dev_t *);
void	dcsession_membership(bhv_desc_t *, int);
void	dcsession_destroy(bhv_desc_t *);

session_ops_t dcsession_ops = {
	BHV_IDENTITY_INIT_POSITION(VSESSION_POSITION_DC),
	dcsession_join,			/* VSESSION_JOIN */
	dcsession_leave,		/* VSESSION_LEAVE */
	dcsession_ctty_alloc,		/* VSESSION_CTTY_ALLOC */
	dcsession_ctty_dealloc,		/* VSESSION_CTTY_DEALLOC */
	dcsession_ctty_hold,		/* VSESSION_CTTY_HOLD */
	dcsession_ctty_rele,		/* VSESSION_CTTY_RELE */
	dcsession_ctty_wait,		/* VSESSION_CTTY_WAIT */
	dcsession_ctty_devnum,		/* VSESSION_CTTY_DEVNUM */
	dcsession_membership,		/* VSESSION_MEMBERSHIP */
	dcsession_destroy,		/* VSESSION_DESTROY (internal) */
};

static void dcsession_tcif_obtain(void *, tk_set_t, tk_set_t, tk_disp_t,
				tk_set_t *);
static void dcsession_tcif_return(tkc_state_t, void *, tk_set_t, tk_set_t,
				tk_disp_t);
/* Made extern for vsession migration */
extern tkc_ifstate_t dcsession_tclient_iface = {
		dcsession_tcif_obtain,
		dcsession_tcif_return
};

/* ARGSUSED */
static int
dcsession_alloc(
	int		sid,
	service_t	svc,
	dcsession_t 	**dcpp)
{
	dcsession_t	*dcp;
	tk_set_t	already, refused;
	/* REFERENCED */
	tk_set_t	 granted;
	objid_t	 objid;
	/* REFERENCED */
	int		error;
	/* REFERENCED */
	int		msgerr;
	cell_t		relocated_to;

	/*
	 * Try for EXISTENCE token from server
	 * Call remote cell, looping if necessary to cope with redirection
	 * to a new cell if the psession has migrated from its origin -
	 * strictly its name service manager.
	 */
	for (;;) {
		msgerr = invk_dssession_obtain(svc, &error,
				      cellid(), sid,
				      VSESSION_EXISTENCE_TOKENSET,
				      TK_NULLSET, TK_NULLSET,
				      &already, &granted, &refused,
				      &objid, &relocated_to);
		ASSERT(!msgerr);
		if (error != EMIGRATED)
			break;
		SERVICE_TO_CELL(svc) = relocated_to;
	}
	ASSERT(error == 0 || refused == VSESSION_EXISTENCE_TOKENSET);
	ASSERT(error != 0
		|| (granted|already|refused) == VSESSION_EXISTENCE_TOKENSET);

	if (refused == VSESSION_EXISTENCE_TOKENSET) {
		/* server didn't know about id */
		return ESRCH;
	} else if (already != TK_NULLSET) {
		/* server already gave another thread the token */
		return EAGAIN;
	}

	dcp = kern_malloc(sizeof(dcsession_t));
	ASSERT(dcp);

	HANDLE_MAKE(dcp->dcsess_handle, svc, objid);
	tkc_create("dsession", dcp->dcsess_tclient, dcp,
		   &dcsession_tclient_iface, VSESSION_NTOKENS, granted,
		   granted, (void *)(__psint_t)sid);

	*dcpp = dcp;
	return 0;
}


/*
 * Create a distributed client for pgpr sid. This function can return
 * an error if the server either:
 * 1) doesn't know about sid
 * 2) thinks this client already knows about sid
 */
int
dcsession_create(
	int		sid,
	service_t	svc,
	vsession_t	**vspp)
{
	vsession_t	*vsp;
	dcsession_t	*dcp;
	int		error;

	/*
	 * Create the distributed object
	 * This fills in the key from the server
	 */
	error = dcsession_alloc(sid, svc, &dcp);
	if (error)
		return(error);

	/*
	 * If we get to this point, the server has blessed us with
	 * the existence token so we are the only thread able to create
	 * the v/dc chain.
	 */

	/*
	 * Allocate the virtual object for the session
	 * for this cell.
	 */
	vsp = vsession_alloc(sid);

	/* Initialize and insert behavior descriptor. */
	bhv_desc_init(&dcp->dcsess_bhv, dcp, vsp, &dcsession_ops);
	BHV_WRITE_LOCK(&vsp->vs_bhvh);
	bhv_insert(&vsp->vs_bhvh, &dcp->dcsess_bhv);
	BHV_WRITE_UNLOCK(&vsp->vs_bhvh);

	/*
	 * All set up - bump reference and insert it into the hash table.
	 */
	VSESSION_HOLD(vsp);
	vsession_enter(vsp);

	/*
	 * Return success and a pointer to the new virtual session object.
	 */
	SESSION_TRACE4("dcsession_create", vsp->vs_sid, "vsp", vsp);
	*vspp = vsp;
	return(0);
}


/*
 * client token callout to obtain a token.
 * Note that this is never called on the server since that will
 * always go through the local_client interface
 */
static void
dcsession_tcif_obtain(
	void		*obj,
	tk_set_t	to_be_obtained,
	tk_set_t	to_be_returned,
	tk_disp_t	dofret,
	tk_set_t	*refused)
{
	vsession_t	*vsp;
	dcsession_t	*dcp = (dcsession_t *)obj;
	/* REFERENCED */
	tk_set_t	granted, already;
	cell_t		relocated_to;
	int		error;
	/* REFERENCED */
	int		msgerr;

	ASSERT(BHV_IS_DCSESSION(&dcp->dcsess_bhv));
	vsp = BHV_TO_VSESSION(&dcp->dcsess_bhv);

	SESSION_TRACE6("dcsession_tcif_obtain", vsp->vs_sid, "vsp", 
		vsp, "to_be_obtained", to_be_obtained);

	/*
	 * Call remote cell, redirection to a new cell is not expected
	 * since the object handle is guaranteed at this point.
	 */
	msgerr = invk_dssession_obtain(DCSESSION_TO_SERVICE(dcp),
			      &error,
			      cellid(), vsp->vs_sid, to_be_obtained,
			      to_be_returned, dofret,
			      &already, &granted, refused,
			      &dcp->dcsess_handle.h_objid,
			      &relocated_to);
	ASSERT(!msgerr);
	ASSERT(already == TK_NULLSET);
	ASSERT(error == 0 || *refused == to_be_obtained);
	ASSERT(error != 0 || granted == to_be_obtained);

	SESSION_TRACE6("dcsession_tcif_obtain", vsp->vs_sid, "vsp", 
		vsp, "granted", granted);
}


/*
 * token client return callout
 */
/* ARGSUSED */
static void
dcsession_tcif_return(
	tkc_state_t	tclient,
	void		*obj,
	tk_set_t	to_be_returned,
	tk_set_t	unknown,
	tk_disp_t	why)
{
	/* REFERENCED */
	int			msgerr;
	dcsession_t		*dcp = (dcsession_t *)obj;

	ASSERT(tclient == dcp->dcsess_tclient);

	SESSION_TRACE6("dcsession_tcif_return",
		BHV_TO_VSESSION(&dcp->dcsess_bhv)->vs_sid,
		"obj", obj, "to_be_returned", to_be_returned);
	msgerr = invk_dssession_return(DCSESSION_TO_SERVICE(dcp),
			   cellid(), 
			   DCSESSION_TO_OBJID(dcp),
			   to_be_returned, unknown, why);
	ASSERT(!msgerr);
	tkc_returned(tclient, to_be_returned, TK_NULLSET);
}


/*
 * server initiated message to client to return a token - client side
 */
void
I_dcsession_recall(
	int		*error,
	int		sid,
	tk_set_t	to_be_revoked,
	tk_disp_t	why)
{
	vsession_t	*vsp;
	bhv_desc_t	*bdp;
	dcsession_t	*dcp;

	/*
	 * it's possible that we are getting a revoke for something
	 * we haven't yet got in our tables - this can happen if the
	 * revoke and obtain pass in the night
	 */
	vsp = vsession_lookup_local(sid);
	if (vsp == NULL) {
		*error = ENOENT;
		return;
	}

	bdp = VSESSION_TO_FIRST_BHV(vsp);
	dcp = BHV_TO_DCSESSION(bdp);

	tkc_recall(dcp->dcsess_tclient, to_be_revoked, why);
	if (TK_IS_IN_SET(VSESSION_EXISTENCE_TOKENSET, to_be_revoked)) {
		vsession_remove(vsp);
		if (sid_is_local(sid))
			PID_SESS_LEAVE(sid);
		tkc_release1(dcp->dcsess_tclient, VSESSION_EXISTENCE_TOKEN);
	}
	*error = 0;
	/* drop reference that lookup_id_local gave us */
	VSESSION_RELE(vsp);	/* This may cause the vsession to vaporize */
	return;
}

/*
 * This message is used to create dc on the origin cell of a
 * session created by a session leader process away from its home.
 */
void
I_dcsession_create_origin(
	cell_t		server,
	objid_t		objid,
	pid_t		sid,
	tk_set_t	granted,
	int		*error)
{
	dcsession_t	*dcp;
	service_t	svc;
	vsession_t	*vsp;
	vsessiontab_t	*vq;

	ASSERT(sid_is_local(sid));

	/*
	 * Allocate a vsession.
	 */
	vsp = vsession_alloc(sid);

	vq = VSESSION_ENTRY(sid);
	VSESSIONTAB_LOCK(vq, MR_UPDATE);
	if (vsession_qlookup_locked(&vq->vsesst_queue, sid)) {
		/*
		 * Oops, someone beat us to it. Undo what we have done
		 * and return an error.
		 */
		VSESSIONTAB_UNLOCK(vq);
		vsession_destroy(vsp);

		*error = EAGAIN;
		SESSION_TRACE4("I_dcsession_create_origin", sid,
			       "error", *error);
		return;
	}

	/* Create dc and initialize client token state. */
	dcp = kern_malloc(sizeof(dcsession_t));
	ASSERT(dcp);
	SERVICE_MAKE(svc, server, SVC_SESSION);
	HANDLE_MAKE(dcp->dcsess_handle, svc, objid);
	tkc_create("dsession", dcp->dcsess_tclient, dcp,
		   &dcsession_tclient_iface,
		   VSESSION_NTOKENS, granted, granted,
		   (void *)(__psint_t)sid);

	/* Initialize and insert behavior descriptor. */
	bhv_desc_init(&dcp->dcsess_bhv, dcp, vsp, &dcsession_ops);
	BHV_WRITE_LOCK(&vsp->vs_bhvh);
	bhv_insert(&vsp->vs_bhvh, &dcp->dcsess_bhv);
	BHV_WRITE_UNLOCK(&vsp->vs_bhvh);

	kqueue_enter(&vq->vsesst_queue, &vsp->vs_queue);
	PID_SESS_JOIN(sid);

	VSESSIONTAB_UNLOCK(vq);

	/*
	 * Return success and a pointer to the new virtual session object.
	 */
	SESSION_TRACE4("I_dcsession_create_origin", vsp->vs_sid, "vsp", vsp);
	*error = 0;
}


void
I_dcsession_ctty_wait(
	pid_t	sid,
	int	mode)
{
	bhv_desc_t	*bdp;
	vsession_t	*vsp;

	if ((vsp = vsession_lookup_local(sid)) != NULL) {
		bdp = VSESSION_TO_FIRST_BHV(vsp);
		ASSERT(BHV_IS_PSESSION(BHV_NEXT(bdp)));
		psession_ctty_wait(BHV_NEXT(bdp), mode);
		VSESSION_RELE(vsp);
	}

	SESSION_TRACE4("I_dcsession_ctty_wait", sid, "ishangup", mode);
}


void
dcsession_join(
	bhv_desc_t	*bdp,
	vpgrp_t		*vpg)
{
	/* REFERENCED */
	vsession_t	*vsp = BHV_TO_VSESSION(bdp);
	dcsession_t	*dcp = BHV_TO_DCSESSION(bdp);
	/* REFERENCED */
	int		error;

	SESSION_TRACE4("dcsession_join", vsp->vs_sid, "pgid", vpg->vpg_pgid);

	error = tkc_acquire1(dcp->dcsess_tclient, VSESSION_MEMBER_TOKEN);
	ASSERT(error == 0);	/* cannot be refused */
	/*
	 * Here if process group vpg is joining a session served
	 * remotely. There will be a (possibly empty) psession
	 * behavior beneath.
	 */
	ASSERT(BHV_IS_PSESSION(BHV_NEXT(bdp)));
	psession_join(BHV_NEXT(bdp), vpg);
}


void
dcsession_leave(
	bhv_desc_t	*bdp,
	vpgrp_t		*vpg)
{
	vsession_t	*vsp = BHV_TO_VSESSION(bdp);
	dcsession_t	*dcp = BHV_TO_DCSESSION(bdp);
	tk_set_t	to_return;
	tk_disp_t	why;
	/* REFERENCED */
	int		msgerr;

	SESSION_TRACE4("dcsession_leave", vsp->vs_sid, "pgid", vpg->vpg_pgid);
	/*
	 * Here if process group is leaving a session served
	 * remotely. There will be a (non-empty) psession
	 * behavior beneath.
	 */
	ASSERT(BHV_IS_PSESSION(BHV_NEXT(bdp)));
	psession_leave(BHV_NEXT(bdp), vpg);

	tkc_release1(dcp->dcsess_tclient, VSESSION_MEMBER_TOKEN);

	/*
	 * Attempt to return the member token. The token module
	 * will allow this only if this token is not busy - i.e.
	 * there are no members remaining locally.
	 */
	tkc_returning(dcp->dcsess_tclient, VSESSION_MEMBER_TOKENSET,
		      &to_return, &why, 0);
	if (to_return == VSESSION_MEMBER_TOKENSET) {
		msgerr = invk_dssession_leave(DCSESSION_TO_SERVICE(dcp),
				cellid(), DCSESSION_TO_OBJID(dcp), vsp->vs_sid);
		ASSERT(!msgerr);
		tkc_returned(dcp->dcsess_tclient,
			     VSESSION_MEMBER_TOKENSET, TK_NULLSET);
	}
}


int
dcsession_ctty_alloc(
	bhv_desc_t	*bdp,
	vnode_t		*vnp,
	void		(*ttycall)(void *, int),
	void		*ttycallarg)
{
	dcsession_t	*dcp = BHV_TO_DCSESSION(bdp);
	vsession_t	*vsp = BHV_TO_VSESSION(bdp);
	cfs_handle_t	ctty_handle;
	int		try_cnt, error;
	/* REFERENCED */
	int		msgerr;

	try_cnt = 0;
again:
	cfs_vnexport(vnp, &ctty_handle);
	/*
	 * Split the input information between the local
	 * psession and the server.  The callback arguments
	 * stay local since they are direct pointers to
	 * tty driver data structures and must remain local
	 * to the cell on which the tty driver runs.  The
	 * vnode goes to the server.  (Perhaps we should also
	 * cache it here as well?)
	 */
	msgerr = invk_dssession_ctty_alloc(DCSESSION_TO_SERVICE(dcp),
				  DCSESSION_TO_OBJID(dcp),
				  vsp->vs_sid, &ctty_handle, &error);
	ASSERT(!msgerr);
	if (error == EMIGRATED) {
		/*
		 * The tty vnode must have migrated since we called
		 * dvn_export.  Retry.
		 */
		cell_backoff(++try_cnt);
		goto again;
	}

	if (error == 0) {
		ASSERT(BHV_IS_PSESSION(BHV_NEXT(bdp)));
		error = psession_ctty_alloc(BHV_NEXT(bdp), NULL,
					    ttycall, ttycallarg);
		ASSERT(error == 0);
	}

	SESSION_TRACE10("dcsession_ctty_alloc", vsp->vs_sid,
		"vnp", vnp, "ttycall", ttycall, "ttycallarg", ttycallarg,
		"error", error);

	return error;
}


void
dcsession_ctty_dealloc(
	bhv_desc_t	*bdp,
	int		mode)
{
	/* REFERENCED */
	int		msgerr;
	dcsession_t	*dcp = BHV_TO_DCSESSION(bdp);
	vsession_t	*vsp = BHV_TO_VSESSION(bdp);

	msgerr = invk_dssession_ctty_dealloc(DCSESSION_TO_SERVICE(dcp),
				    DCSESSION_TO_OBJID(dcp),
				    vsp->vs_sid,
				    mode);
	ASSERT(!msgerr);

	SESSION_TRACE4("dcsession_ctty_dealloc", vsp->vs_sid,
		       "mode", mode);
}


int
dcsession_ctty_hold(
	bhv_desc_t	*bdp,
	vnode_t		**vnpp)
{
	int		error = 0;
	dcsession_t	*dcp = BHV_TO_DCSESSION(bdp);
	vsession_t	*vsp = BHV_TO_VSESSION(bdp);
	cfs_handle_t	ctty_handle;
	int		try_cnt;
	/* REFERENCED */
	int		msgerr;

	ASSERT(BHV_IS_PSESSION(BHV_NEXT(bdp)));
	psession_ctty_hold(BHV_NEXT(bdp), NULL);
	if (vnpp != NULL) {
		try_cnt = 0;
again:
		msgerr = invk_dssession_ctty_getvnode(DCSESSION_TO_SERVICE(dcp),
					     DCSESSION_TO_OBJID(dcp),
					     vsp->vs_sid, &ctty_handle,
					     &error);
		ASSERT(!msgerr);
		if (error == 0) {
			cfs_vnimport(&ctty_handle, vnpp);
			if (*vnpp == NULL) {
				cell_backoff(++try_cnt);
				goto again;
			}
		}
	}
	if (error)
		psession_ctty_rele(BHV_NEXT(bdp), NULL);

	SESSION_TRACE6("dcsession_ctty_hold", vsp->vs_sid, "vnpp", vnpp,
			"error", error);
	return error;
}


void
dcsession_ctty_rele(
	bhv_desc_t	*bdp,
	vnode_t		*vp)
{
	/* REFERENCED */
	vsession_t	*vsp = BHV_TO_VSESSION(bdp);

	ASSERT(BHV_IS_PSESSION(BHV_NEXT(bdp)));
	psession_ctty_rele(BHV_NEXT(bdp), vp);
	SESSION_TRACE4("dcsession_ctty_rele", vsp->vs_sid, "vp", vp);
}


/* ARGSUSED */
void
dcsession_ctty_wait(
	bhv_desc_t	*bdp,
	int		mode)
{
	/* This function exists to be called by the server only */
	panic("dcsession_ctty_wait");
}


void
dcsession_ctty_devnum(
	bhv_desc_t	*bdp,
	dev_t		*devnump)
{
	/* REFERENCED */
	int		msgerr;
	dcsession_t	*dcp = BHV_TO_DCSESSION(bdp);
	vsession_t	*vsp = BHV_TO_VSESSION(bdp);

	msgerr = invk_dssession_ctty_devnum(DCSESSION_TO_SERVICE(dcp),
				   DCSESSION_TO_OBJID(dcp),
				   vsp->vs_sid, devnump);
	ASSERT(!msgerr);

	SESSION_TRACE4("dcsession_ctty_devnum", vsp->vs_sid,
		"devnump", devnump);
}


/* ARGSUSED */
void
dcsession_membership(
	bhv_desc_t	*bdp,
	int		count)
{
	/* REFERENCED */
	vsession_t	*vsp = BHV_TO_VSESSION(bdp);

	SESSION_TRACE4("dcsession_membership", vsp->vs_sid, "count", count);

	/*
	 * Local session has no members.
	 * We do nothing so that the session remains in the lookup table
	 * and cached. The eventual recall of the existence token will
	 * trigger its final destruction.
	 */ 
	return;
}


void
dcsession_destroy(
	bhv_desc_t	*bdp)
{
	dcsession_t	*dcp = BHV_TO_DCSESSION(bdp);
	vsession_t	*vsp = BHV_TO_VSESSION(bdp);

	SESSION_TRACE2("dcsession_destroy", vsp->vs_sid);

	/*
	 * Must have been removed from the lookup lists
	 * to be destroyed.
	 */
	ASSERT(!VSESSION_IS_VALID(vsp));

	ASSERT(BHV_IS_PSESSION(BHV_NEXT(bdp)));
	psession_destroy(BHV_NEXT(bdp));

	tkc_destroy(dcp->dcsess_tclient);

	bhv_remove(&vsp->vs_bhvh, &dcp->dcsess_bhv);
	kern_free(dcp);
}
