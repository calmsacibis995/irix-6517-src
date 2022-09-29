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
#ident "$Id: dssession.c,v 1.10 1997/09/25 21:01:58 henseler Exp $"

/*
 * Server side for the session subsystem
 */

#include <sys/types.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/ipc.h>
#include <sys/kmem.h>
#include <sys/sema.h>
#include <sys/systm.h>
#include <sys/ksignal.h>
#include <values.h>

#include <stdarg.h>

#include <ksys/cell/tkm.h>
#include <ksys/vproc.h>
#include <ksys/vsession.h>
#include <fs/cfs/cfs.h>

#include "pproc_private.h"

#include "dsession.h"
#include "I_dssession_stubs.h"
#include "invk_dcsession_stubs.h"
#include "vsession_private.h"

/*
 * Protos for server-side distributed layer.
 */
void	dssession_join(bhv_desc_t *, vpgrp_t *);
void	dssession_leave(bhv_desc_t *, vpgrp_t *);
int	dssession_ctty_alloc(bhv_desc_t *, vnode_t *,
			void (*)(void *, int), void *);
void	dssession_ctty_dealloc(bhv_desc_t *, int);
int	dssession_ctty_hold(bhv_desc_t *, vnode_t **);
void	dssession_ctty_rele(bhv_desc_t *, vnode_t *);
void	dssession_ctty_wait(bhv_desc_t *, int);
void	dssession_ctty_devnum(bhv_desc_t *, dev_t *);
void	dssession_membership(bhv_desc_t *, int);
void	dssession_destroy(bhv_desc_t *);

session_ops_t dssession_ops = {
	BHV_IDENTITY_INIT_POSITION(VSESSION_POSITION_DS),
	dssession_join,			/* VSESSION_JOIN */
	dssession_leave,		/* VSESSION_LEAVE */
	dssession_ctty_alloc,		/* VSESSION_CTTY_ALLOC */
	dssession_ctty_dealloc,		/* VSESSION_CTTY_DEALLOC */
	dssession_ctty_hold,		/* VSESSION_CTTY_HOLD */
	dssession_ctty_rele,		/* VSESSION_CTTY_RELE */
	dssession_ctty_wait,		/* VSESSION_CTTY_WAIT */
	dssession_ctty_devnum,		/* VSESSION_CTTY_DEVNUM */
	dssession_membership,		/* VSESSION_MEMBERSHIP */
	dssession_destroy,		/* VSESSION_DESTROY (internal) */
};

static void dssession_tsif_recall(void *, tks_ch_t, tk_set_t, tk_disp_t);
static void dssession_tsif_recalled(void *, tk_set_t, tk_set_t);
static void dssession_tsif_idle(void *, tk_set_t);

static tks_ifstate_t dssession_tserver_iface = {
		dssession_tsif_recall,
		dssession_tsif_recalled,
		dssession_tsif_idle,
};

/*
 * A remote client has requested access to a previously local-only vsession
 * set up and interpose the DS layer.
 */
void
vsession_interpose(vsession_t *vsp)
{
	dssession_t	*dsp;
	tk_set_t	granted;
	tk_set_t	already_obtained;
	tk_set_t	refused;
	tk_set_t	wanted;
	/* REFERENCED */ 
	int		error;
	int		nmembers;
	int		i;

	dsp = kern_malloc(sizeof(*dsp));

	tks_create("dsession", dsp->dssess_tserver, dsp,
		   &dssession_tserver_iface, VSESSION_NTOKENS,
		   (void *)(__psint_t)vsp->vs_sid);

	wanted = VSESSION_EXISTENCE_TOKENSET;
	tks_obtain(dsp->dssess_tserver, (tks_ch_t)cellid(),
		   wanted, &granted, &refused,
		   &already_obtained);
	ASSERT(granted == wanted);
	ASSERT(already_obtained == TK_NULLSET);
	ASSERT(refused == TK_NULLSET);

	/*
	 * Setting up the local client token state requires looking
	 * at the local session state and setting tokens accordingly.
	 */
	psession_getstate(VSESSION_TO_FIRST_BHV(vsp), &nmembers);
	tkc_create_local("dsession", dsp->dssess_tclient, dsp->dssess_tserver,
			 VSESSION_NTOKENS, wanted,
			 wanted, (void *)(__psint_t)vsp->vs_sid);

	dsp->dssess_notify_idle = nmembers;

	for (i = nmembers; i > 0; i--)
		tkc_acquire1(dsp->dssess_tclient, VSESSION_MEMBER_TOKEN);

	if (dsp->dssess_notify_idle) {
		/*
		 * Need callout when no member remains.
		 */
		tks_notify_idle(dsp->dssess_tserver, VSESSION_MEMBER_TOKENSET);
	}

	bhv_desc_init(&dsp->dssess_bhv, dsp, vsp, &dssession_ops);
	error = bhv_insert(&vsp->vs_bhvh, &dsp->dssess_bhv);
	ASSERT(!error);
}


int
dssession_allocate_origin(
	vsession_t	*vsp)
{
	service_t	svc;
	tk_set_t	granted;
	tk_set_t	refused;
	tk_set_t	already_obtained;
	dssession_t	*dsp;
	int		error;
	/* REFERENCED */ 
	int		msgerr;

	dsp = BHV_TO_DSSESSION(VSESSION_TO_FIRST_BHV(vsp));

	/*
	 * Obtain an existence token for the origin cell.
	 */
	svc = sid_to_service(vsp->vs_sid);

	/*
	 * Call over to origin cell to create the dc on origin cell.
	 */
	msgerr = invk_dcsession_create_origin(svc, cellid(),
					      BHV_TO_OBJID(&dsp->dssess_bhv),
					      vsp->vs_sid,
					      VSESSION_EXISTENCE_TOKENSET,
					      &error);
	ASSERT(!msgerr);

	if (!error) {
		/*
		 * get the token once we know the operation succeeded.
		 */
		tks_obtain(dsp->dssess_tserver, (tks_ch_t) SERVICE_TO_CELL(svc),
			VSESSION_EXISTENCE_TOKENSET, &granted, &refused,
			&already_obtained);
		ASSERT(granted == VSESSION_EXISTENCE_TOKENSET);
		ASSERT(refused == TK_NULLSET);
		ASSERT(already_obtained == TK_NULLSET);
	}
		
	return(error);
}


/*
 * This is called to create a server away from the origin cell.
 * We need to contact the origin to register the sid in the namespace.
 * Semantics of setsid() at a higher level are expected to enforce
 * single-threading here.
 */
vsession_t *
dssession_create(
	pid_t		sid)
{
	vsession_t	*vsp;

	/*
	 * Allocate the virtual object for the process group for this cell.
	 * Splice in a ds.
	 */
	vsp = vsession_alloc(sid);
	ASSERT(vsp);
	SESSION_TRACE4("dssession_create", sid, "vsp", vsp);

	BHV_WRITE_LOCK(VSESSION_BHV_HEAD_PTR(vsp));
	vsession_interpose(vsp);
	BHV_WRITE_UNLOCK(VSESSION_BHV_HEAD_PTR(vsp));
	if (dssession_allocate_origin(vsp) != 0) {
		dssession_t	*dsp;
		tk_set_t	refused;

		dsp = BHV_TO_DSSESSION(VSESSION_TO_FIRST_BHV(vsp));
		tkc_release1(dsp->dssess_tclient, VSESSION_EXISTENCE_TOKEN);
		tks_recall(dsp->dssess_tserver, VSESSION_EXISTENCE_TOKENSET,
			   &refused);
		ASSERT(refused == TK_NULLSET);
		vsession_destroy(vsp);
		SESSION_TRACE2("dssession_create failed", sid);
		return(NULL);
	}

	/*
	 * Bump reference count and enter into lookup table.
	 */
	VSESSION_HOLD(vsp);
	vsession_enter(vsp);

	return(vsp);
}


/*
 * Client request to server to obtain tokens.
 * This means we potentially need to change from a local vsession to
 * a distributed vsession
 */
void
I_dssession_obtain(
	int		*error,
	cell_t		sender,
	int		sid,
	tk_set_t	to_be_obtained,
	tk_set_t	to_be_returned,
	tk_disp_t	dofret,
	tk_set_t	*already_obtained,
	tk_set_t	*granted,
	tk_set_t	*refused,
	objid_t		*objid,
	cell_t		*redirect)
{
	vsession_t	*vsp;
	dssession_t	*dsp;
	bhv_desc_t	*bdp;

	ASSERT(to_be_returned == TK_NULLSET);
	*error = 0;

	SESSION_TRACE8("I_dssession_obtain", sid, "from", sender, "obtain",
		to_be_obtained, "objid", *objid);

	/*
	 * if we're going after the EXISTENCE token then we use the
	 * sid as a handle and lookup to see if we have this vsession
	 * If however we aren't looking for th EXISTENCE token then
	 * we use objid as our handle.
	 */
	if (!TK_IS_IN_SET(VSESSION_EXISTENCE_TOKENSET, to_be_obtained)) {
		/* this is simple */
		bhv_desc_t *bdp = OBJID_TO_BHV(*objid);
		dsp = BHV_TO_DSSESSION(bdp);

		if (to_be_returned != TK_NULLSET)
			tks_return(dsp->dssess_tserver, sender,
				   to_be_returned, TK_NULLSET, TK_NULLSET,
				   dofret);

		vsp = BHV_TO_VSESSION(bdp);
		ASSERT(VSESSION_IS_VALID(vsp));
		tks_obtain(dsp->dssess_tserver, sender,
			   to_be_obtained,
			   granted, refused, already_obtained);
		return;
	}

	if ((vsp = vsession_lookup_local(sid)) == NULL) {
		*refused = to_be_obtained;
		*granted = TK_NULLSET;
		*already_obtained = TK_NULLSET;
		*error = ENOENT;
		return;
	}

	/*
	 * Need the read lock at this stage to be assured that
	 * no migration is in progress.
	 */
	BHV_READ_LOCK(VSESSION_BHV_HEAD_PTR(vsp));
	bdp = VSESSION_TO_FIRST_BHV(vsp);

	/*
	 * need to lock out other threads that might also be
	 * in here - only one can transition us from local to distributed
	 */
	if (BHV_IS_PSESSION(bdp)) {
		/*
		 * We need to add a distributed layer
		 */
		BHV_READ_UNLOCK(VSESSION_BHV_HEAD_PTR(vsp));
		BHV_WRITE_LOCK(VSESSION_BHV_HEAD_PTR(vsp));
		/*
		 * re-set bdp to 'top' level (since another thread might
		 * have beat us to the lock and already done the interposition
		 * (and conceivably even migrated the thing elsewhere)
		 */
		bdp = VSESSION_TO_FIRST_BHV(vsp);
		if (BHV_IS_PSESSION(bdp))
			vsession_interpose(vsp);
		BHV_WRITE_TO_READ(VSESSION_BHV_HEAD_PTR(vsp));
		/* re-set bdp to 'top' level */
		bdp = VSESSION_TO_FIRST_BHV(vsp);
		SESSION_TRACE6("I_dssession_obtain after interpose - ",
			vsp->vs_sid, "vsp", vsp, "bdp", bdp);
	}

	/*
	 * Could reach this point but find that the dssession/psession has
	 * migrated away from its origin cell (here). But as origin (or
	 * surrogate if the origin cell went down) we know where the
	 * psession is. Our dcsession is guaranteed - so return redirection
	 * to new service (i.e. cell).
	 */
	if (BHV_IS_DCSESSION(bdp)) {
		dcsession_t	*dcp = BHV_TO_DCSESSION(bdp);
		*redirect = SERVICE_TO_CELL(dcp->dcsess_handle.h_service);
		BHV_READ_UNLOCK(VSESSION_BHV_HEAD_PTR(vsp));
		VSESSION_RELE(vsp);
		*error = EMIGRATED;
		return;
	}

	ASSERT(BHV_IS_DSSESSION(bdp));

	/*
	 * since we only have 2 levels max - dssession is always the head
	 * and we want to return the pointer to the behavior descriptor 
	 * that represents the dssession layer.
	 */
	*objid = bdp;

	dsp = BHV_TO_DSSESSION(bdp);

	if (to_be_returned != TK_NULLSET)
		tks_return(dsp->dssess_tserver, sender, to_be_returned, 
			   TK_NULLSET, TK_NULLSET, dofret);

	tks_obtain(dsp->dssess_tserver, sender,
		   to_be_obtained, granted, refused, already_obtained);

	BHV_READ_UNLOCK(VSESSION_BHV_HEAD_PTR(vsp));
	VSESSION_RELE(vsp);
	return;
}


/*
 * server side of client returning tokens
 */
void
I_dssession_return(
	cell_t		sender,
	objid_t		objid,
	tk_set_t	to_be_returned,
	tk_set_t	unknown,
	tk_disp_t	why)
{
	bhv_desc_t	*bdp = OBJID_TO_BHV(objid);
	dssession_t	*dsp = BHV_TO_DSSESSION(bdp);
	vsession_t	*vsp = BHV_TO_VSESSION(bdp);
 
	SESSION_TRACE6("I_dssession_return", vsp->vs_sid, "from", sender,
		"tokens", to_be_returned);

	VSESSION_HOLD(vsp);
	tks_return(dsp->dssess_tserver, sender, to_be_returned,
		   TK_NULLSET, unknown, why);
	VSESSION_RELE(vsp);	/* Possibly final release */
}


static void
dssession_tsif_recall(
	void		*obj,
	tks_ch_t	client,
	tk_set_t	to_be_recalled,
	tk_disp_t	why)
{
	dssession_t	*dsp = (dssession_t *)obj;
	bhv_desc_t	*bdp = &dsp->dssess_bhv;
	vsession_t	*vsp = BHV_TO_VSESSION(bdp);

	if (client == cellid()) {
		tkc_recall(dsp->dssess_tclient, to_be_recalled, why);
	} else {
		service_t svc;
		int error;
		/* REFERENCED */
		int msgerr;

		SERVICE_MAKE(svc, (cell_t)client, SVC_SESSION);
		msgerr = invk_dcsession_recall(svc, &error, vsp->vs_sid,
				      to_be_recalled, why);
		ASSERT(!msgerr);
		if (error == ENOENT) {
			/* client didn't know what we were talking about! */
			tks_return(dsp->dssess_tserver, client, TK_NULLSET,
				   TK_NULLSET, to_be_recalled, why);
		}
	}
}


/*
 * This callout is not used using the current vsession allocation
 * scheme which does not cache on clients.
 */
/* ARGSUSED */
static void
dssession_tsif_recalled(void *obj, tk_set_t done, tk_set_t ndone)
{
	panic("dssession_tsif_recalled: not expected here");
}


/*
 * Callout taken when all tokens returned in requested set.
 * Only the MEMBER token uses this facility.
 */
/* ARGSUSED */
static void
dssession_tsif_idle(
	void		*obj,
	tk_set_t	idle)
{
	dssession_t	*dsp = (dssession_t *)obj;
	bhv_desc_t	*bdp = &dsp->dssess_bhv;
	vsession_t	*vsp = BHV_TO_VSESSION(bdp);
	/* REFERENCED */
	tk_set_t	refused;

	ASSERT(idle == VSESSION_MEMBER_TOKENSET);

	/*
	 * No members anywhere - remove from look-up table,
	 * and deregister the session id.
	 */
	vsession_remove(vsp);
	if (sid_is_local(vsp->vs_sid))
		PID_SESS_LEAVE(vsp->vs_sid);

	/*
	 * Recall all existence token and recall others.
	 */
	tkc_release1(dsp->dssess_tclient, VSESSION_EXISTENCE_TOKEN);
	tks_recall(dsp->dssess_tserver, VSESSION_EXISTENCE_TOKENSET, &refused);
	ASSERT(refused == TK_NULLSET);
}

/*
 * Server-side routines called from remote client cells:
 */

/* I_dssession_join not required. */

/* ARGSUSED */
void
I_dssession_leave(
	cell_t		client,
	objid_t		objid,
	pid_t		sid)
{
	bhv_desc_t	*bdp = OBJID_TO_BHV(objid);
	dssession_t	*dsp = BHV_TO_DSSESSION(bdp);
	vsession_t	*vsp = BHV_TO_VSESSION(bdp);

	ASSERT(vsp->vs_sid == sid);

	SESSION_TRACE2("I_dssession_leave", vsp->vs_sid);

	/*
	 * Here when a client is returning is member token
	 * because there no member process groups on that cell.
	 */
	VSESSION_HOLD(vsp);		/* Prevent premature eradication */
	tks_return(dsp->dssess_tserver, client,
		   VSESSION_MEMBER_TOKENSET,
		   TK_NULLSET,
		   TK_NULLSET,
		   TK_DISP_CLIENT_ALL);
	VSESSION_RELE(vsp);
}

/* ARGSUSED */
void
I_dssession_ctty_alloc(
	objid_t		objid,
	pid_t		sid,
	cfs_handle_t	*ctty_handlep,
	int		*error)
{
	bhv_desc_t	*bdp = OBJID_TO_BHV(objid);
	/* REFERENCED */
	vsession_t	*vsp = BHV_TO_VSESSION(bdp);
	vnode_t		*ctty_vnp;

	/*
	 * Map from ctty_handlep to a vnode
	 */
	cfs_vnimport(ctty_handlep, &ctty_vnp);
	if (ctty_vnp == NULL) {
		*error = EMIGRATED;
		return;
	}

	ASSERT(sid == vsp->vs_sid);
	ASSERT(BHV_IS_PSESSION(BHV_NEXT(bdp)));
	*error = psession_ctty_alloc(BHV_NEXT(bdp), ctty_vnp, NULL, NULL);
	VN_RELE(ctty_vnp);
	SESSION_TRACE6("I_dssession_ctty_alloc", vsp->vs_sid,
		       "ctty_vnp", ctty_vnp, "error", *error);
}


/* ARGSUSED */
void
I_dssession_ctty_dealloc(
	objid_t		objid,
	pid_t		sid,
	int		mode)
{
	bhv_desc_t	*bdp = OBJID_TO_BHV(objid);
	/* REFERENCED */
	vsession_t	*vsp = BHV_TO_VSESSION(bdp);

	ASSERT(sid == vsp->vs_sid);
	ASSERT(BHV_IS_PSESSION(BHV_NEXT(bdp)));
	psession_ctty_dealloc(BHV_NEXT(bdp), mode);
	SESSION_TRACE4("I_dssession_ctty_dealloc", vsp->vs_sid,
		       "mode", mode);
}


/* I_dssession_ctty_hold not required */


/* ARGSUSED */
void
I_dssession_ctty_getvnode(
	objid_t		objid,
	pid_t		sid,
	cfs_handle_t	*ctty_handlep,
	int		*error)
{
	bhv_desc_t	*bdp = OBJID_TO_BHV(objid);
	/* REFERENCED */
	vsession_t	*vsp = BHV_TO_VSESSION(bdp);
	vnode_t		*vnp;

	ASSERT(sid == vsp->vs_sid);
	ASSERT(BHV_IS_PSESSION(BHV_NEXT(bdp)));
	if ((*error = psession_ctty_hold(BHV_NEXT(bdp), &vnp)) == 0) {
		cfs_vnexport(vnp, ctty_handlep);
		psession_ctty_rele(BHV_NEXT(bdp), vnp);
	}
	SESSION_TRACE6("I_dssession_ctty_getvnode", vsp->vs_sid,
		       "ctty_handlep", ctty_handlep, "error", *error);
}


/* I_dssession_ctty_rele not required */


/* I_dssession_ctty_wait not required */


/* ARGSUSED */
void
I_dssession_ctty_devnum(
	objid_t		objid,
	pid_t		sid,
	dev_t		*devnump)
{
	bhv_desc_t	*bdp = OBJID_TO_BHV(objid);
	/* REFERENCED */
	vsession_t	*vsp = BHV_TO_VSESSION(bdp);

	ASSERT(sid == vsp->vs_sid);
	ASSERT(BHV_IS_PSESSION(BHV_NEXT(bdp)));
	psession_ctty_devnum(BHV_NEXT(bdp), devnump);
	SESSION_TRACE4("I_dssession_ctty_devnum", vsp->vs_sid,
		"devnump", devnump);
}


void
dssession_join(
	bhv_desc_t	*bdp,
	vpgrp_t		*vpg)
{
	/* REFERENCED */
	vsession_t	*vsp = BHV_TO_VSESSION(bdp);
	dssession_t	*dsp = BHV_TO_DSSESSION(bdp);

	SESSION_TRACE4("dssession_join", vsp->vs_sid, "pgid", vpg->vpg_pgid);

	/*
	 * Serialize member token acquisition
	 */
	ASSERT(VSESSION_IS_VALID(vsp));
	tkc_acquire1(dsp->dssess_tclient, VSESSION_MEMBER_TOKEN);

	/*
	 * Here if process group vpg is joining a session served
	 * remotely. There will be a (possibly empty) psession
	 * behavior beneath.
	 */
	ASSERT(BHV_IS_PSESSION(BHV_NEXT(bdp)));
	psession_join(BHV_NEXT(bdp), vpg);
}


void
dssession_leave(
	bhv_desc_t	*bdp,
	vpgrp_t		*vpg)
{
	/* REFERENCED */
	vsession_t	*vsp = BHV_TO_VSESSION(bdp);
	dssession_t	*dsp = BHV_TO_DSSESSION(bdp);
	tk_set_t	to_return;
	tk_disp_t	why;

	SESSION_TRACE4("dssession_leave", vsp->vs_sid, "pgid", vpg->vpg_pgid);

	/*
	 * Here if process group vpg is leaving a session served
	 * remotely. There will be a (non-empty) psession
	 * behavior beneath.
	 */
	ASSERT(BHV_IS_PSESSION(BHV_NEXT(bdp)));
	psession_leave(BHV_NEXT(bdp), vpg);

	tkc_release1(dsp->dssess_tclient, VSESSION_MEMBER_TOKEN);
	tkc_returning(dsp->dssess_tclient, VSESSION_MEMBER_TOKENSET,
		      &to_return, &why, 0);
	if (TK_IS_IN_SET(VSESSION_MEMBER_TOKENSET, to_return)) {
		tks_return(dsp->dssess_tserver, cellid(),
			   to_return, TK_NULLSET, TK_NULLSET, why);
		tkc_returned(dsp->dssess_tclient, to_return, TK_NULLSET);
		/*
		 * If the above return was the last member to leave,
		 * the idle callout will have been made and the vsession
		 * will have been invalidated (removed from the lookup list)
		 * and all existence tokens recalled. The vsession will
		 * be on the verge of destruction, disapperaing when
		 * the ref count gets to 0.
		 */
	}
}


int
dssession_ctty_alloc(
	bhv_desc_t	*bdp,
	vnode_t		*vnp,
	void		(*ttycall)(void *, int),
	void		*ttycallarg)
{
	int		error;
	/* REFERENCED */
	vsession_t	*vsp = BHV_TO_VSESSION(bdp);

	ASSERT(BHV_IS_PSESSION(BHV_NEXT(bdp)));
	error = psession_ctty_alloc(BHV_NEXT(bdp), vnp, ttycall, ttycallarg);
	SESSION_TRACE10("dssession_ctty_alloc", vsp->vs_sid,
		"vnp", vnp, "ttycall", ttycall, "ttycallarg", ttycallarg,
		"error", error);
	return error;
}


void
dssession_ctty_dealloc(
	bhv_desc_t	*bdp,
	int		mode)
{
	/* REFERENCED */
	vsession_t	*vsp = BHV_TO_VSESSION(bdp);

	ASSERT(BHV_IS_PSESSION(BHV_NEXT(bdp)));
	psession_ctty_dealloc(BHV_NEXT(bdp), mode);
	SESSION_TRACE4("dssession_ctty_dealloc", vsp->vs_sid,
		       "mode", mode);
}


int
dssession_ctty_hold(
	bhv_desc_t	*bdp,
	vnode_t		**vnpp)
{
	int		error;
	/* REFERENCED */
	vsession_t	*vsp = BHV_TO_VSESSION(bdp);

	ASSERT(BHV_IS_PSESSION(BHV_NEXT(bdp)));
	error = psession_ctty_hold(BHV_NEXT(bdp), vnpp);
	SESSION_TRACE4("dssession_ctty_hold", vsp->vs_sid, "vnpp", vnpp);
	return error;
}


void
dssession_ctty_rele(
	bhv_desc_t	*bdp,
	vnode_t		*vp)
{
	/* REFERENCED */
	vsession_t	*vsp = BHV_TO_VSESSION(bdp);

	ASSERT(BHV_IS_PSESSION(BHV_NEXT(bdp)));
	psession_ctty_rele(BHV_NEXT(bdp), vp);
	SESSION_TRACE4("dssession_ctty_rele", vsp->vs_sid, "vp", vp);
}


/* ARGSUSED */
static tks_iter_t
ctty_wait_iterator(
	void		*obj,
	tks_ch_t	client,
	tk_set_t	tokens_owned,
	va_list		args)
{
	dssession_t	*dsp = (dssession_t *)obj;
	bhv_desc_t	*bdp = &dsp->dssess_bhv;
	vsession_t	*vsp = BHV_TO_VSESSION(bdp);
	service_t	svc;
	int		mode;
	/* REFERENCED */
	int		msgerr;

	ASSERT(TK_IS_IN_SET(VSESSION_EXISTENCE_TOKENSET, tokens_owned));

	if (client != cellid()) {
		mode = va_arg(args, int);
		SERVICE_MAKE(svc, (cell_t)client, SVC_SESSION);
		msgerr = invk_dcsession_ctty_wait(svc, vsp->vs_sid, mode);
		ASSERT(!msgerr);
	}
	return TKS_CONTINUE;
}


void
dssession_ctty_wait(
	bhv_desc_t	*bdp,
	int		mode)
{
	/* REFERENCED */
	tks_iter_t	result;
	dssession_t	*dsp = BHV_TO_DSSESSION(bdp);
	/* REFERENCED */
	vsession_t	*vsp = BHV_TO_VSESSION(bdp);

	ASSERT(BHV_IS_PSESSION(BHV_NEXT(bdp)));
	result = tks_iterate(dsp->dssess_tserver,
			     VSESSION_EXISTENCE_TOKENSET,
			     TKS_STABLE,
			     ctty_wait_iterator,
			     mode);
	ASSERT(result == TKS_CONTINUE);

	psession_ctty_wait(BHV_NEXT(bdp), mode);
	SESSION_TRACE4("dssession_ctty_wait", vsp->vs_sid,
		       "mode", mode);
}


void
dssession_ctty_devnum(
	bhv_desc_t	*bdp,
	dev_t		*devnump)
{
	/* REFERENCED */
	vsession_t	*vsp = BHV_TO_VSESSION(bdp);

	ASSERT(BHV_IS_PSESSION(BHV_NEXT(bdp)));
	psession_ctty_devnum(BHV_NEXT(bdp), devnump);
	SESSION_TRACE4("dssession_ctty_devnum", vsp->vs_sid,
		"devnump", devnump);
}


void
dssession_membership(
	bhv_desc_t	*bdp,
	int		count)
{
	dssession_t	*dsp = BHV_TO_DSSESSION(bdp);
	/* REFERENCED */
	vsession_t	*vsp = BHV_TO_VSESSION(bdp);

	SESSION_TRACE4("dssession_membership", vsp->vs_sid, "count", count);

	if (count == 0) {
		/*
		 * No local members. When member tokens idle, we'll invalidate
		 * the vsession and recall existence tokens. But there's
		 * nothing to be done at this point.
		 */
		return;
	}

	if (!dsp->dssess_notify_idle) {
		/*
		 * Need callout when no member remains.
		 */
		tks_notify_idle(dsp->dssess_tserver, VSESSION_MEMBER_TOKENSET);

		dsp->dssess_notify_idle = 1;
	}
}


void
dssession_destroy(
	bhv_desc_t	*bdp)
{
	dssession_t	*dsp = BHV_TO_DSSESSION(bdp);
	vsession_t	*vsp = BHV_TO_VSESSION(bdp);

	ASSERT(!VSESSION_IS_VALID(vsp));

	SESSION_TRACE2("dssession_destroy", vsp->vs_sid);

	ASSERT(BHV_IS_PSESSION(BHV_NEXT(bdp)));
	psession_destroy(BHV_NEXT(bdp));

	tkc_destroy_local(dsp->dssess_tclient);
	tks_destroy(dsp->dssess_tserver);

	bhv_remove(&vsp->vs_bhvh, &dsp->dssess_bhv);
	kern_free(dsp);
}
