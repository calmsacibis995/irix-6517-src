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
#ident "$Id: dspgrp.c,v 1.40 1998/01/10 02:40:01 ack Exp $"

/*
 * Server side for the process group subsystem
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
#include <ksys/vpgrp.h>
#include <ksys/vsession.h>
#include <ksys/cred.h>

#include "pproc_private.h"

#include "dpgrp.h"
#include "I_dspgrp_stubs.h"
#include "invk_dcpgrp_stubs.h"
#include "vpgrp_private.h"
#include "pproc_private.h"

#include <sys/ktrace.h>

/*
 * Protos for server-side distributed layer.
 */
static void dspgrp_getattr(bhv_desc_t *, pid_t *, int *, int *);
static int  dspgrp_join_begin(bhv_desc_t *);
static void dspgrp_join_end(bhv_desc_t *, struct proc *, int);
static void dspgrp_leave(bhv_desc_t *, struct proc *, int);
static void dspgrp_detach(bhv_desc_t *, struct proc *);
static void dspgrp_linkage(bhv_desc_t *, struct proc *, pid_t, pid_t);
static void dspgrp_orphan(bhv_desc_t *, int, int);
static int  dspgrp_sendsig(bhv_desc_t *, int, int, pid_t,
			   cred_t *, k_siginfo_t *);
static sequence_num_t dspgrp_sigseq(bhv_desc_t *);
static int  dspgrp_sig_wait(bhv_desc_t *, sequence_num_t);
static void dspgrp_anystop(bhv_desc_t *, int, int*);
static void dspgrp_membership(bhv_desc_t *, int, pid_t);
static void dspgrp_destroy(bhv_desc_t *);
static int  dspgrp_nice(bhv_desc_t *, int, int *, int *, cred_t *);
static int dspgrp_setbatch(bhv_desc_t *);
static int dspgrp_clearbatch(bhv_desc_t *);

pgrp_ops_t dspgrp_ops = {
	BHV_IDENTITY_INIT_POSITION(VPGRP_POSITION_DS),
	dspgrp_getattr,
	dspgrp_join_begin,
	dspgrp_join_end,
	dspgrp_leave,
	dspgrp_detach,
	dspgrp_linkage,
	dspgrp_sendsig,
	dspgrp_sigseq,
	dspgrp_sig_wait,
	dspgrp_setbatch,
	dspgrp_clearbatch,
	dspgrp_nice,
	dspgrp_orphan,
	dspgrp_anystop,
	dspgrp_membership,
	dspgrp_destroy,
};

static void dspgrp_tsif_recall(void *, tks_ch_t, tk_set_t, tk_disp_t);
static void dspgrp_tsif_recalled(void *, tk_set_t, tk_set_t);
static void dspgrp_tsif_idle(void *, tk_set_t);

tks_ifstate_t dspgrp_tserver_iface = {
		dspgrp_tsif_recall,
		dspgrp_tsif_recalled,
		dspgrp_tsif_idle,
};

/*
 * A remote client has requested access to a previously local-only vpgrp
 * set up and interpose the DS layer.
 */
void
vpgrp_interpose(vpgrp_t *vpgrp)
{
	dspgrp_t	*dsp;
	tk_set_t	granted;
	tk_set_t	already_obtained;
	tk_set_t	refused;
	tk_set_t	wanted;
	/* REFERENCED */ 
	int		error;
	int		nmembers;
	int		is_orphaned;
	int		i;

	dsp = kern_malloc(sizeof(*dsp));

	tks_create("dpgrp", dsp->dspg_tserver, dsp, &dspgrp_tserver_iface,
		   VPGRP_NTOKENS, (void *)(__psint_t)vpgrp->vpg_pgid);

	wanted = VPGRP_EXISTENCE_TOKENSET;
	tks_obtain(dsp->dspg_tserver, (tks_ch_t)cellid(),
		   wanted, &granted, &refused,
		   &already_obtained);
	ASSERT(granted == wanted);
	ASSERT(already_obtained == TK_NULLSET);
	ASSERT(refused == TK_NULLSET);

	DSPGRP_MEM_LOCK_INIT(dsp);
	OBJ_STATE_INIT(&dsp->dspg_obj_state,  &vpgrp->vpg_bhvh);

	/*
	 * Setting up the local client token state requires looking
	 * at the local pgrp state and setting tokens accordingly.
	 */
	ppgrp_getstate(VPGRP_TO_FIRST_BHV(vpgrp), &nmembers, &is_orphaned);
	tkc_create_local("dpgrp", dsp->dspg_tclient, dsp->dspg_tserver,
		   VPGRP_NTOKENS, wanted,
		   wanted, (void *)(__psint_t)vpgrp->vpg_pgid);

	dsp->dspg_notify_idle = nmembers;

	for (i = nmembers; i > 0; i--)
		tkc_acquire1(dsp->dspg_tclient, VPGRP_MEMBER_TOKEN);

	if (dsp->dspg_notify_idle) {
		 /*
		 * Need callout when no member remains.
		 */
		tks_notify_idle(dsp->dspg_tserver, VPGRP_MEMBER_TOKENSET);
	}

	if (!is_orphaned)
		tkc_acquire1(dsp->dspg_tclient, VPGRP_NONORPHAN_TOKEN);

	bhv_desc_init(&dsp->dspg_bhv, dsp, vpgrp, &dspgrp_ops);
	error = bhv_insert(&vpgrp->vpg_bhvh, &dsp->dspg_bhv);
	ASSERT(!error);
}


int
dspgrp_allocate_origin(
	vpgrp_t	*vpgrp,
	pid_t	sid)
{
	service_t	svc;
	tk_set_t	granted;
	tk_set_t	refused;
	tk_set_t	already_obtained;
	dspgrp_t	*dsp;
	int		error;
	/* REFERENCED */ 
	int		msgerr;

	dsp = BHV_TO_DSPGRP(VPGRP_TO_FIRST_BHV(vpgrp));

	/*
	 * Obtain an existence token for the origin cell.
	 */
	svc = pgid_to_service(vpgrp->vpg_pgid);

	/*
	 * Call over to origin cell to create the dc on origin cell.
	 */
	msgerr = invk_dcpgrp_create_origin(svc, cellid(),
					   BHV_TO_OBJID(&dsp->dspg_bhv),
					   vpgrp->vpg_pgid,
					   sid,
					   VPGRP_EXISTENCE_TOKENSET, &error);
	ASSERT(!msgerr);

	if (!error) {
		/*
		 * get the token once we know the operation succeeded.
		 */
		tks_obtain(dsp->dspg_tserver, (tks_ch_t) SERVICE_TO_CELL(svc),
			VPGRP_EXISTENCE_TOKENSET, &granted, &refused,
			&already_obtained);
		ASSERT(granted == VPGRP_EXISTENCE_TOKENSET);
		ASSERT(refused == TK_NULLSET);
		ASSERT(already_obtained == TK_NULLSET);
	}
		
	return(error);
}

/*
 * This is called to create a server away from the origin cell.
 * We need to contact the origin to register the pgid in the namespace.
 * Semantics of setpgid() at a higher level are expected to enforce
 * single-threading here.
 */
vpgrp_t *
dspgrp_create(
	pid_t		pgid,
	pid_t		sid)
{
	vpgrp_t		*vpg;
	vsession_t	*vsp;

	/*
	 * Allocate the virtual object for the process group for this cell.
	 * Splice in a ds.
	 */
	vpg = vpgrp_alloc(pgid, sid);
	ASSERT(vpg);
	PGRP_TRACE4("dspgrp_create", pgid, "vpgrp", vpg);

	BHV_WRITE_LOCK(VPGRP_BHV_HEAD_PTR(vpg));
	vpgrp_interpose(vpg);
	BHV_WRITE_UNLOCK(VPGRP_BHV_HEAD_PTR(vpg));
	if (dspgrp_allocate_origin(vpg, sid) != 0) {
		dspgrp_t	*dsp;
		tk_set_t	refused;

		dsp = BHV_TO_DSPGRP(VPGRP_TO_FIRST_BHV(vpg));
		tkc_release1(dsp->dspg_tclient, VPGRP_EXISTENCE_TOKEN);
		tks_recall(dsp->dspg_tserver, VPGRP_EXISTENCE_TOKENSET,
				&refused);
		ASSERT(refused == TK_NULLSET);
		vpgrp_destroy(vpg);
		PGRP_TRACE2("dspgrp_create failed", pgid);
		return(NULL);
	}

	vsp = VSESSION_LOOKUP(sid);
	ASSERT(vsp);
	VSESSION_JOIN(vsp, vpg);

	/*
	 * Bump reference count and enter into lookup table.
	 */
	VPGRP_HOLD(vpg);
	vpgrp_enter(vpg);

	return(vpg);
}

/*
 * Client request to server to obtain tokens.
 * This means we potentially need to change from a local vpgrp to
 * a distributed vpgrp
 */
void
I_dspgrp_obtain(
	int		*error,
	cell_t		sender,
	int		pgid,
	tk_set_t	to_be_obtained,
	tk_set_t	to_be_returned,
	tk_disp_t	dofret,
	tk_set_t	*already_obtained,
	tk_set_t	*granted,
	tk_set_t	*refused,
	objid_t		*objid,
	pid_t		*sid,
	int		*is_batch,
	sequence_num_t	*sigseq,
	cell_t		*redirect)
{
	vpgrp_t		*vpgrp;
	dspgrp_t	*dsp;
	bhv_desc_t	*bdp;

	ASSERT(to_be_returned == TK_NULLSET);
	*error = 0;

	PGRP_TRACE8("I_dspgrp_obtain", pgid, "from", sender, "obtain",
		to_be_obtained, "objid", *objid);

	/*
	 * if we're going after the EXISTENCE token then we use the
	 * pgid as a handle and lookup to see if we have this vpgrp
	 * If however we aren't looking for th EXISTENCE token then
	 * we use objid as our handle.
	 */
	if (!TK_IS_IN_SET(VPGRP_EXISTENCE_TOKENSET, to_be_obtained)) {
		/* this is simple */
		bhv_desc_t *bdp = OBJID_TO_BHV(*objid);
		dsp = BHV_TO_DSPGRP(bdp);

		if (to_be_returned != TK_NULLSET)
			tks_return(dsp->dspg_tserver, sender, to_be_returned, 
				   TK_NULLSET, TK_NULLSET, dofret);

		if (!TK_IS_IN_SET(VPGRP_MEMBER_TOKENSET, to_be_obtained)) {
			tks_obtain(dsp->dspg_tserver, sender, to_be_obtained,
				   granted, refused, already_obtained);
		} else {
			vpgrp = BHV_TO_VPGRP(bdp);
			DSPGRP_MEM_LOCK(dsp);
			if (VPGRP_IS_VALID(vpgrp)) {
				/*
				 * Obtain member token and current signal seq.
				 */
				tks_obtain(dsp->dspg_tserver, sender,
					   to_be_obtained,
					   granted, refused, already_obtained);
				*sigseq = ppgrp_sigseq(BHV_NEXT(bdp));
			} else {
				/* Pgrp being torn-down, membership refused. */
				*error = ENOENT;
			}
			DSPGRP_MEM_UNLOCK(dsp);
		}
		return;
	}

	if ((vpgrp = vpgrp_lookup_local(pgid)) == NULL) {
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
	BHV_READ_LOCK(VPGRP_BHV_HEAD_PTR(vpgrp));
	bdp = VPGRP_TO_FIRST_BHV(vpgrp);

	/*
	 * need to lock out other threads that might also be
	 * in here - only one can transition us from local to distributed
	 */
	if (BHV_POSITION(bdp) == VPGRP_POSITION_PP) {
		/*
		 * We need to add a distributed layer
		 */
		BHV_READ_UNLOCK(VPGRP_BHV_HEAD_PTR(vpgrp));
		BHV_WRITE_LOCK(VPGRP_BHV_HEAD_PTR(vpgrp));
		/*
		 * re-set bdp to 'top' level (since another thread might
		 * have beat us to the lock and already done the interposition
		 * (and conceivably even migrated the thing elsewhere)
		 */
		bdp = VPGRP_TO_FIRST_BHV(vpgrp);
		if (BHV_POSITION(bdp) == VPGRP_POSITION_PP)
			vpgrp_interpose(vpgrp);
		BHV_WRITE_TO_READ(VPGRP_BHV_HEAD_PTR(vpgrp));
		/* re-set bdp to 'top' level */
		bdp = VPGRP_TO_FIRST_BHV(vpgrp);
		PGRP_TRACE6("I_dspgrp_obtain after interpose - ",
			vpgrp->vpg_pgid, "vprgp", vpgrp, "bdp", bdp);
	}

	/*
	 * Could reach this point but find that the dspgrp/ppgrp has migrated
	 * away from its origin cell (here). But as origin (or surrogate
	 * if the origin cell went down) we know where the ppgrp is. Our
	 * dcpgrp is guaranteed - so return redirection to new service (i.e.
	 * cell).
	 */
	if (BHV_POSITION(bdp) == VPGRP_POSITION_DC) {
		dcpgrp_t	*dcp = BHV_TO_DCPGRP(bdp);
		*redirect = SERVICE_TO_CELL(dcp->dcpg_handle.h_service);
		BHV_READ_UNLOCK(VPGRP_BHV_HEAD_PTR(vpgrp));
		VPGRP_RELE(vpgrp);
		*error = EMIGRATED;
		return;
	}

	ASSERT(BHV_OPS(bdp) == &dspgrp_ops);

	/*
	 * since we only have 2 levels max - dspgrp is always the head
	 * and we want to return the pointer to the behavior descriptor 
	 * that represents the dspgrp layer.
	 */
	*objid = bdp;

	dsp = BHV_TO_DSPGRP(bdp);

	if (to_be_returned != TK_NULLSET)
		tks_return(dsp->dspg_tserver, sender, to_be_returned, 
			   TK_NULLSET, TK_NULLSET, dofret);

	tks_obtain(dsp->dspg_tserver, sender,
		   to_be_obtained, granted, refused, already_obtained);

	/*
	 * Return the session id, and batch attrs to be cached at the client.
	 */
	ppgrp_getattr(BHV_NEXT(bdp), sid, NULL, is_batch);

	BHV_READ_UNLOCK(VPGRP_BHV_HEAD_PTR(vpgrp));
	VPGRP_RELE(vpgrp);
	return;
}

/*
 * server side of client returning tokens
 */
void
I_dspgrp_return(
	cell_t		sender,
	objid_t		objid,
	tk_set_t	to_be_returned,
	tk_set_t	unknown,
	tk_disp_t	why)
{
	bhv_desc_t	*bdp = OBJID_TO_BHV(objid);
	dspgrp_t	*dsp = BHV_TO_DSPGRP(bdp);
	/* REFERENCED */
	vpgrp_t		*vpg = BHV_TO_VPGRP(bdp);
 
	PGRP_TRACE8("I_dspgrp_return", vpg->vpg_pgid, "from", sender,
		"returned", to_be_returned, "unknown", unknown);

	VPGRP_HOLD(vpg);
	if (TK_IS_IN_SET(VPGRP_MEMBER_TOKENSET, to_be_returned)) {
		DSPGRP_MEM_LOCK(dsp);
		tks_return(dsp->dspg_tserver, sender, to_be_returned,
			   TK_NULLSET, unknown, why);
		DSPGRP_MEM_UNLOCK(dsp);
	} else
		tks_return(dsp->dspg_tserver, sender, to_be_returned,
			   TK_NULLSET, unknown, why);
	VPGRP_RELE(vpg);	/* Possibly final release */
}


static void
dspgrp_tsif_recall(
	void		*obj,
	tks_ch_t	client,
	tk_set_t	to_be_recalled,
	tk_disp_t	why)
{
	/* REFERENCED */
	int		msgerr;
	dspgrp_t	*dsp = (dspgrp_t *)obj;
	bhv_desc_t	*bdp = &dsp->dspg_bhv;
	vpgrp_t		*vpg = BHV_TO_VPGRP(bdp);

	if (client == cellid()) {
		tkc_recall(dsp->dspg_tclient, to_be_recalled, why);
	} else {
		service_t svc;
		int error;

		SERVICE_MAKE(svc, (cell_t)client, SVC_PGRP);
		msgerr = invk_dcpgrp_recall(svc, &error, vpg->vpg_pgid,
				   to_be_recalled, why);
		ASSERT(!msgerr);
		if (error == ENOENT) {
			/* client didn't know what we were talking about! */
			tks_return(dsp->dspg_tserver, client,
				   TK_NULLSET, TK_NULLSET, to_be_recalled, why);
		}
	}
}

/*
 * This callout is not used using the current vpgrp allocation
 * scheme which does not cache on clients.
 */
/* ARGSUSED */
static void
dspgrp_tsif_recalled(void *obj, tk_set_t done, tk_set_t ndone)
{
	panic("dspgrp_tsif_recalled: not expected here");
}

/*
 * Callout taken when all tokens returned in requested set.
 * Only the MEMBER token uses this facility.
 */
/* ARGSUSED */
static void
dspgrp_tsif_idle(
	void		*obj,
	tk_set_t	idle)
{
	dspgrp_t	*dsp = (dspgrp_t *)obj;
	bhv_desc_t	*bdp = &dsp->dspg_bhv;
	vpgrp_t		*vpgrp = BHV_TO_VPGRP(bdp);
	/* REFERENCED */
	tk_set_t	refused;

	ASSERT(idle == VPGRP_MEMBER_TOKENSET);
	ASSERT(DSPGRP_MEM_IS_LOCKED(dsp));

	/*
	 * No members anywhere - remove from look-up table.
	 */
	vpgrp_remove(vpgrp);

	/*
	 * Recall all existence token and recall others.
	 */
	tkc_release1(dsp->dspg_tclient, VPGRP_EXISTENCE_TOKEN);
	tks_recall(dsp->dspg_tserver, VPGRP_EXISTENCE_TOKENSET, &refused);
	ASSERT(refused == TK_NULLSET);
}

/*
 * Server-side routines called from remote client cells:
 */

/*
 * Here to discover whether the process group is orphaned/batch.
 */
/* ARGSUSED */
void
I_dspgrp_getattr(
	objid_t		objid,
	pid_t		pgid,
	pid_t		*sid,
	int		*is_orphaned,
	int		*is_batch)
{
	bhv_desc_t	*bdp = OBJID_TO_BHV(objid);
	dspgrp_t	*dsp = BHV_TO_DSPGRP(bdp);
	/* REFERENCED */
	vpgrp_t		*vpg = BHV_TO_VPGRP(bdp);

	DSPGRP_RETARGET_CHECK(dsp);

	ASSERT(vpg->vpg_pgid == pgid);

	dspgrp_getattr(bdp, sid, is_orphaned, is_batch);

	PGRP_TRACE8("I_dspgrp_getattr", vpg->vpg_pgid, "sid", sid,
		"is_orphaned", *is_orphaned, "is_batch", *is_batch);
}

/* I_dspgrp_join_* not required. */

/* ARGSUSED */
void
I_dspgrp_leave(
	cell_t		client,
	objid_t		objid,
	pid_t		pgid)
{
	bhv_desc_t	*bdp = OBJID_TO_BHV(objid);
	dspgrp_t	*dsp = BHV_TO_DSPGRP(bdp);
	vpgrp_t		*vpg = BHV_TO_VPGRP(bdp);

	DSPGRP_RETARGET_CHECK(dsp);

	ASSERT(vpg->vpg_pgid == pgid);

	PGRP_TRACE2("I_dspgrp_leave", vpg->vpg_pgid);

	/*
	 * Here when a client is returning is member token
	 * because there no member processes on that cell.
	 */
	VPGRP_HOLD(vpg);		/* Prevent premature eradication */
	DSPGRP_MEM_LOCK(dsp);
	tks_return(dsp->dspg_tserver, client,
		   VPGRP_MEMBER_TOKENSET,
		   TK_NULLSET,
		   TK_NULLSET,
		   TK_DISP_CLIENT_ALL);
	DSPGRP_MEM_UNLOCK(dsp);
	VPGRP_RELE(vpg);
}

/* I_dspgrp_detach not required. */

/* ARGSUSED */
static tks_iter_t
sendsig_iterator(
	void		*obj,
	tks_ch_t	client,
	tk_set_t	tokens_owned,
	va_list		args)
{
	dspgrp_t	*dsp = (dspgrp_t *)obj;
	bhv_desc_t	*bdp = &dsp->dspg_bhv;
	vpgrp_t		*vpg = BHV_TO_VPGRP(bdp);
	service_t	svc;
	cell_t		excluded_cell;
	sig_arg_t	*argp;

	if (!TK_IS_IN_SET(VPGRP_MEMBER_TOKENSET, tokens_owned))
		return TKS_CONTINUE;

	excluded_cell = va_arg(args, cell_t);
	argp = va_arg(args, sig_arg_t *);

	if (client != excluded_cell && client != cellid()) {	
		int	remote_err;
		/* REFERENCED */
		int	msgerr;

		SERVICE_MAKE(svc, (cell_t)client, SVC_PGRP)
		msgerr = invk_dcpgrp_sendsig(svc, vpg->vpg_pgid,
				    argp->sig, argp->opts, argp->sid,
				    CRED_GETID(argp->credp),
				    argp->infop, argp->infop ? 1 : 0,
				    &remote_err);
		ASSERT(!msgerr);
		if (remote_err)
			argp->error = remote_err;
	}

	return TKS_CONTINUE;
}

/*
 * Here to send a signal to all members of the pgrp.
 * We iterate over cells with MEMBER tokens (with the
 * exception of the calling client).
 */
/* ARGSUSED */
void
I_dspgrp_sendsig(
	cell_t		client,
	objid_t		objid,
	pid_t		pgid,
	int		sig,
	int		options,
	pid_t		sid,
	credid_t	credid,
	k_siginfo_t	*infop,
	size_t		ninfo,
	int		*errorp)
{
	bhv_desc_t	*bdp = OBJID_TO_BHV(objid);
	dspgrp_t	*dsp = BHV_TO_DSPGRP(bdp);
	/* REFERENCED */
	tks_iter_t	result;
	/* REFERENCED */
	vpgrp_t		*vpg = BHV_TO_VPGRP(bdp);
	sig_arg_t	arg;
	cred_t		*credp;
	int		error;

	DSPGRP_RETARGET_CHECK(dsp);

	ASSERT(vpg->vpg_pgid == pgid);

	PGRP_TRACE6("I_dspgrp_sendsig", vpg->vpg_pgid, "sig", sig,
		"options", options);

	credp = CREDID_GETCRED(credid);

	/* Initialize argument block */
	arg.sig   = sig;
	arg.opts  = options;
	arg.sid   = sid;
	arg.credp = credp;
	arg.infop = ninfo ? infop : NULL;
	arg.error = 0;

	/*
	 * Ensure we can safely iterate.
	 */
	error = tkc_acquire1(dsp->dspg_tclient,
			VPGRP_NO_MEMBER_RELOCATING_TOKEN);
	ASSERT(!error);

	/*
	 * To ensure that signal sequence counting is accurate,
	 * we take a write lock prevent new member cells being
	 * added until this signal is distributed.
	 */
	DSPGRP_MEM_LOCK(dsp);
	*errorp = ppgrp_sendsig(BHV_NEXT(bdp), sig, options, sid, credp, infop);
	result = tks_iterate(dsp->dspg_tserver,
			     VPGRP_MEMBER_TOKENSET,
			     TKS_STABLE,
			     sendsig_iterator,
			     client,
			     &arg);
	ASSERT(result == TKS_CONTINUE);
	DSPGRP_MEM_UNLOCK(dsp);
	if (arg.error)
		*errorp = arg.error;

	tkc_release1(dsp->dspg_tclient, VPGRP_NO_MEMBER_RELOCATING_TOKEN);

	if (credp)
		crfree(credp);
}

/* ARGSUSED */
static tks_iter_t
nonorphan_iterator(
	void		*obj,
	tks_ch_t	client,
	tk_set_t	tokens_owned,
	va_list		ap)
{
	dspgrp_t	*dsp = (dspgrp_t *)obj;
	bhv_desc_t	*bdp = &dsp->dspg_bhv;
	vpgrp_t		*vpg = BHV_TO_VPGRP(bdp);
	service_t	svc;
	pid_t		sid;
	int		is_orphaned;
	int		is_batch;
	/* REFERENCED */
	int		msgerr;

	if (!TK_IS_IN_SET(VPGRP_MEMBER_TOKENSET, tokens_owned))
		return TKS_CONTINUE;

	if (client == cellid())
		/* Handled explicitly outside iteration and ignored here. */
		return TKS_CONTINUE;
	
	/* XXX - Can't we rely on the token here? Why visit the client? */
	SERVICE_MAKE(svc, (cell_t)client, SVC_PGRP)
	msgerr = invk_dcpgrp_getattr(svc, vpg->vpg_pgid, &sid,
			&is_orphaned, &is_batch);
	ASSERT(!msgerr);

	/*
	 * If this client is actually orphaned, there was a race 
	 * so we continue looking. If this client is indeed not
	 * orphaned, the entire pgrp is, so we can stop.
	 */
	return (is_orphaned ? TKS_CONTINUE : TKS_STOP);
}

/*
 * Here when a client cell is returning its NONORPHAN token
 * and hence potentially orphaning the entire pgrp.
 */
/* ARGSUSED */
void
I_dspgrp_orphan(
	cell_t		client,
	objid_t		objid,
	pid_t		pgid,
	int		is_exit)
{
	bhv_desc_t	*bdp = OBJID_TO_BHV(objid);
	dspgrp_t	*dsp = BHV_TO_DSPGRP(bdp);
	pid_t		sid;
	int		is_orphaned;
	/* REFERENCED */
	vpgrp_t		*vpg = BHV_TO_VPGRP(bdp);

	DSPGRP_RETARGET_CHECK(dsp);

	ASSERT(vpg->vpg_pgid == pgid);

	/*
	 * Return token.
	 */
	tks_return(dsp->dspg_tserver,
		   client,
		   VPGRP_NONORPHAN_TOKENSET,
		   TK_NULLSET, TK_NULLSET, TK_DISP_CLIENT_ALL);
	
	/*
	 * Make orphan check - first locally.
	 */
	ppgrp_getattr(BHV_NEXT(bdp), &sid, &is_orphaned, NULL);
	if (!is_orphaned) {
		PGRP_TRACE2("I_dspgrp_orphan - not orphaned at server",
			vpg->vpg_pgid);
	} else {
		tks_iter_t	result;
		int		error;

		/*
		 * Ensure we can safely iterate.
		 */
		error = tkc_acquire1(dsp->dspg_tclient,
				VPGRP_NO_MEMBER_RELOCATING_TOKEN);
		ASSERT(!error);

		/*
		 * See whether any other cell is preventing the pgrp
		 * from being orphaned;
		 */
		result = tks_iterate(dsp->dspg_tserver,
				     VPGRP_NONORPHAN_TOKENSET,
				     TKS_STABLE,
				     nonorphan_iterator);
		ASSERT(result == TKS_STOP || result == TKS_CONTINUE);
		if (result == TKS_CONTINUE) {
			/*
			 * The pgrp is globally orphaned.
			 */
			PGRP_TRACE4("I_dspgrp_orphan - orphaned",
				    vpg->vpg_pgid, "is_exit", is_exit);
			ppgrp_orphan(BHV_NEXT(bdp), 1, is_exit);
		} else {
			PGRP_TRACE2("I_dspgrp_orphan - not orphaned",
				    vpg->vpg_pgid);
		}
		tkc_release1(dsp->dspg_tclient,
			VPGRP_NO_MEMBER_RELOCATING_TOKEN);
	}
	return;
}
 
/* ARGSUSED */
static tks_iter_t
anystop_iterator(
	void		*obj,
	tks_ch_t	client,
	tk_set_t	tokens_owned,
	va_list		args)
{
	dspgrp_t	*dsp = (dspgrp_t *)obj;
	bhv_desc_t	*bdp = &dsp->dspg_bhv;
	vpgrp_t		*vpg = BHV_TO_VPGRP(bdp);
	service_t	svc;
	cell_t		excluded_cell;
	pid_t		pid;
	int		is_stopped;

	if (!TK_IS_IN_SET(VPGRP_MEMBER_TOKENSET, tokens_owned))
		return TKS_CONTINUE;
	
	excluded_cell = va_arg(args, cell_t);
	pid = va_arg(args, int);
	if (client == excluded_cell || client == cellid())
		return TKS_CONTINUE;
	else {
		/* REFERENCED */
		int	msgerr;

		SERVICE_MAKE(svc, (cell_t)client, SVC_PGRP)
		msgerr = invk_dcpgrp_anystop(svc, vpg->vpg_pgid, pid,
					&is_stopped);
		ASSERT(!msgerr);
		return (is_stopped ? TKS_STOP : TKS_CONTINUE);
	}
}

/* ARGSUSED */
void
I_dspgrp_anystop(
	cell_t		client,
	objid_t		objid,
	pid_t		pgid,
	pid_t		pid,
	int		*is_stopped)
{
	bhv_desc_t	*bdp = OBJID_TO_BHV(objid);
	dspgrp_t	*dsp = BHV_TO_DSPGRP(bdp);
	tks_iter_t	result;
	/* REFERENCED */
	vpgrp_t		*vpg = BHV_TO_VPGRP(bdp);
	int		error;

	DSPGRP_RETARGET_CHECK(dsp);

	ASSERT(vpg->vpg_pgid == pgid);

	/*
	 * Ensure we can safely iterate.
	 */
	error = tkc_acquire1(dsp->dspg_tclient,
			VPGRP_NO_MEMBER_RELOCATING_TOKEN);
	ASSERT(!error);

	/*
	 * Here from a client to determine whether there is any
	 * stopped member of the pgrp.
	 * Try here first.
	 */
	ppgrp_anystop(BHV_NEXT(bdp), pid, is_stopped);
	if (*is_stopped) {
		tkc_release1(dsp->dspg_tclient,
				VPGRP_NO_MEMBER_RELOCATING_TOKEN);
		PGRP_TRACE6("I_dspgrp_anystop", vpg->vpg_pgid, "pid", pid,
			 "is_stopped here", *is_stopped);
		return;
	}

	result = tks_iterate(dsp->dspg_tserver,
			     VPGRP_MEMBER_TOKENSET,
			     TKS_STABLE,
			     anystop_iterator,
			     client,
			     pid);
	*is_stopped = (result == TKS_STOP);
	PGRP_TRACE6("I_dspgrp_anystop", vpg->vpg_pgid, "pid", pid, "is_stopped",
		*is_stopped);

	tkc_release1(dsp->dspg_tclient, VPGRP_NO_MEMBER_RELOCATING_TOKEN);

}

/* ARGSUSED */
static tks_iter_t
nice_iterator(
	void		*obj,
	tks_ch_t	client,
	tk_set_t	tokens_owned,
	va_list		args)
{
	dspgrp_t	*dsp = (dspgrp_t *)obj;
	bhv_desc_t	*bdp = &dsp->dspg_bhv;
	vpgrp_t		*vpg = BHV_TO_VPGRP(bdp);
	service_t	svc;
	cell_t		excluded_cell;
	int		flags;
	int		*nice;
	int		*count;
	cred_t		*scred;
	int		*error;
	int		lcnt;
	int		lnice;
	/* REFERENCED */
	int		msgerr;

	if (!TK_IS_IN_SET(VPGRP_MEMBER_TOKENSET, tokens_owned))
		return TKS_CONTINUE;
	
	excluded_cell = va_arg(args, cell_t);
	if (client == excluded_cell || client == cellid())
		return TKS_CONTINUE;

	flags = va_arg(args, int);
	nice = va_arg(args, int *);
	count = va_arg(args, int *);
	scred = va_arg(args, cred_t *);
	error = va_arg(args, int *);

	ASSERT(*error == 0);

	lnice = *nice;
	lcnt = 0;

	SERVICE_MAKE(svc, (cell_t)client, SVC_PGRP)
	msgerr = invk_dcpgrp_nice(svc, vpg->vpg_pgid, flags,
			 &lnice, &lcnt, CRED_GETID(scred), error);
	ASSERT(!msgerr);

	if (*error)
		return TKS_STOP;

	if ((flags & VPG_GET_NICE) && lnice > *nice)
		*nice = lnice;
	
	*count += lcnt;

	return TKS_CONTINUE;
}

/* ARGSUSED */
void
I_dspgrp_nice(
	cell_t		client,
	objid_t		objid,
	pid_t		pgid,
	int		flags,
	int		*nice,
	int		*count,
	credid_t	scredid,
	int		*error)
{
	bhv_desc_t	*bdp = OBJID_TO_BHV(objid);
	dspgrp_t	*dsp = BHV_TO_DSPGRP(bdp);
	/* REFERENCED */
	tks_iter_t	result;
	/* REFERENCED */
	vpgrp_t		*vpg = BHV_TO_VPGRP(bdp);
	int		lpri, rpri;
	int		lcnt, rcnt;
	int		err;
	cred_t		*scred;

	DSPGRP_RETARGET_CHECK(dsp);

	ASSERT(vpg->vpg_pgid == pgid);

	/* BSD setpriority and getpriority system calls.
	 * 'flag' tells which we are doing.
	 * For setpriority, set all members of the pgroups priority
	 * to *nice.
	 * For getpriority, return 'best' priority of all pgroup
	 * members in *nice.
	 *
	 * Return EPERM for permission violations. Set *count to number
	 * of processes acted upon so caller can determine if ESRCH.
	 */

	/* Perform the operation first locally. errors
	 * terminate the operation.
	 */

	if (flags & VPG_GET_NICE)
		rpri = lpri = 0;
	else
		rpri = lpri = *nice;

	/*
	 * Ensure we can safely iterate.
	 */
	err = tkc_acquire1(dsp->dspg_tclient, VPGRP_NO_MEMBER_RELOCATING_TOKEN);
	ASSERT(!err);
	
	scred = CREDID_GETCRED(scredid);
	err = ppgrp_nice(BHV_NEXT(bdp), flags, &lpri, &lcnt, scred);
	if (scred)
		crfree(scred);

	if (error) {
		tkc_release1(dsp->dspg_tclient,
			VPGRP_NO_MEMBER_RELOCATING_TOKEN);
		PGRP_TRACE8("I_dspgrp_nice", vpg->vpg_pgid, "flags", flags,
			"nice", nice, "err", error);
		*error = err;
		return;
	}

	result = tks_iterate(dsp->dspg_tserver,
			     VPGRP_MEMBER_TOKENSET,
			     TKS_STABLE,
			     nice_iterator,
			     client,
			     flags,
			     &rpri,
			     &rcnt,
			     scredid,
			     &err);

	ASSERT(result == TKS_CONTINUE || err != 0);

	tkc_release1(dsp->dspg_tclient, VPGRP_NO_MEMBER_RELOCATING_TOKEN);

	PGRP_TRACE8("I_dspgrp_nice", vpg->vpg_pgid, "flags", flags, "nice", nice,
		"err", error);

	if (err) {
		*error = err;
		return;
	}

	if (flags & VPG_GET_NICE)
		*nice = MAX(lpri, rpri);

	*count = lcnt + rcnt;
}

/* ARGSUSED */
void
I_dspgrp_clearbatch(
	cell_t		client,
	objid_t		objid,
	pid_t		pgid)
{
	bhv_desc_t	*bdp = OBJID_TO_BHV(objid);
	dspgrp_t	*dsp = BHV_TO_DSPGRP(bdp);
	/* REFERENCED */
	vpgrp_t		*vpg = BHV_TO_VPGRP(bdp);
	
	DSPGRP_RETARGET_CHECK(dsp);

	PGRP_TRACE2("I_dspgrp_clearbatch", vpg->vpg_pgid);

	(void) ppgrp_clearbatch(BHV_NEXT(bdp));

}

/*
 * Server-side operations:
 */
void
dspgrp_getattr(
	bhv_desc_t	*bdp,
	pid_t		*sidp,
	int		*is_orphanedp,
	int		*is_batchp)
{
	dspgrp_t	*dsp = BHV_TO_DSPGRP(bdp);
	tks_iter_t	result;
	pid_t		sid;
	int		is_orphaned;
	int 		is_batch;
	/* REFERENCED */
	vpgrp_t	*vpg = BHV_TO_VPGRP(bdp);

	/*
	 * First, see if the local pgrp is non-orphaned - if so,
	 * this is the global state also. Otherwise, we have to
	 * determine whether any (one) cell holds a non-orphan token.
	 * We iterate until we get a positive response.
	 * Note that if we haven't been asked for the orphan state
	 * we skip it.
	 */
	ppgrp_getattr(BHV_NEXT(bdp), &sid, &is_orphaned, &is_batch);
	if ((is_orphanedp != NULL) && is_orphaned) {
		int	error;

		/*
		 * Ensure we can safely iterate.
		 */
		error = tkc_acquire1(dsp->dspg_tclient,
				VPGRP_NO_MEMBER_RELOCATING_TOKEN);
		ASSERT(!error);

		result = tks_iterate(dsp->dspg_tserver,
				     VPGRP_NONORPHAN_TOKENSET,
				     TKS_STABLE,
				     nonorphan_iterator);
		ASSERT(result == TKS_STOP || result == TKS_CONTINUE);
		is_orphaned = (result == TKS_CONTINUE);
		tkc_release1(dsp->dspg_tclient, VPGRP_NO_MEMBER_RELOCATING_TOKEN);
	}
	if (sidp)
		*sidp = sid;
	if (is_orphanedp)
		*is_orphanedp = is_orphaned;
	if (is_batchp)
		*is_batchp = is_batch;

	PGRP_TRACE8("dspgrp_getattr", vpg->vpg_pgid, "*sidp", sidp ? sid : -1,
		"*is_orphanedp", is_orphanedp ? is_orphaned : -1,
		"*is_batchp", is_batchp ? is_batch : -1);
}

int
dspgrp_join_begin(
	bhv_desc_t	*bdp)
{
	dspgrp_t	*dsp = BHV_TO_DSPGRP(bdp);
	/* REFERENCED */
	vpgrp_t	*vpg = BHV_TO_VPGRP(bdp);
	/* REFERENCED */
	int		error;

	PGRP_TRACE2("dspgrp_join_begin", vpg->vpg_pgid);

	/*
	 * Serialize member token acquisition.
	 */
	DSPGRP_MEM_LOCK(dsp);
	if (VPGRP_IS_VALID(vpg)) {
		tkc_acquire1(dsp->dspg_tclient, VPGRP_MEMBER_TOKEN);
		DSPGRP_MEM_UNLOCK(dsp);
		error = ppgrp_join_begin(BHV_NEXT(bdp));
		ASSERT(!error);
		return 0;
	} else {
		DSPGRP_MEM_UNLOCK(dsp);
		return 1;
	}
}

void
dspgrp_join_end(
	bhv_desc_t	*bdp,
	proc_t		*p,
	int		attach)
{
	/* REFERENCED */
	vpgrp_t	*vpg = BHV_TO_VPGRP(bdp);

	PGRP_TRACE6("dspgrp_join_end", vpg->vpg_pgid, "pid", p->p_pid,
		    "attach", attach);

	ppgrp_join_end(BHV_NEXT(bdp), p, attach);
}


void
dspgrp_leave(
	bhv_desc_t	*bdp,
	proc_t		*p,
	int		exitting)
{
	dspgrp_t	*dsp = BHV_TO_DSPGRP(bdp);
	tk_set_t	to_return;
	tk_disp_t	why;
	/* REFERENCED */
	vpgrp_t	*vpg = BHV_TO_VPGRP(bdp);

	PGRP_TRACE6("dspgrp_leave", vpg->vpg_pgid, "pid", p->p_pid, "exiting",
		exitting);
	/*
	 * Here if local process p is leaving a pgrp served
	 * remotely. There will be a (non-empty) ppgrp
	 * behavior beneath.
	 */
	ppgrp_leave(BHV_NEXT(bdp), p, exitting);

	/*
	 * Release hold of member token and attempt to return it.
	 * If there are no remaining member/hold we will be allowed
	 * to return it.
	 */
	tkc_release1(dsp->dspg_tclient, VPGRP_MEMBER_TOKEN);
	tkc_returning(dsp->dspg_tclient, VPGRP_MEMBER_TOKENSET,
		      &to_return, &why, 0);
	if (TK_IS_IN_SET(VPGRP_MEMBER_TOKENSET, to_return)) {
		DSPGRP_MEM_LOCK(dsp);
		tks_return(dsp->dspg_tserver, cellid(),
			   to_return, TK_NULLSET, TK_NULLSET, why);
		tkc_returned(dsp->dspg_tclient, to_return, TK_NULLSET);
		DSPGRP_MEM_UNLOCK(dsp);
		/*
		 * If the above return was the last member to leave,
		 * the idle callout will have been made and the vpgrp
		 * will have been invalidated (removed from the lookup list)
		 * and all existence tokens recalled. The vpgrp will
		 * be on the verge of destruction, disapperaing when
		 * the ref count gets to 0.
		 */
	}
}

void
dspgrp_detach(
	bhv_desc_t	*bdp,
	struct proc	*p)
{
	/* REFERENCED */
	vpgrp_t	*vpg = BHV_TO_VPGRP(bdp);

	PGRP_TRACE4("dspgrp_detach", vpg->vpg_pgid, "pid", p->p_pid);

	/*
	 * Here if local process p is leaving a pgrp served
	 * locally. There must be a (non-empty) ppgrp
	 * behavior beneath. This is a simple pass-through.
	 * Note: interesting pgrp orphaning is the subject
	 * of virtual operations called below this.
	 */
	ASSERT(p->p_vpgrp == vpg);
	ASSERT(BHV_NEXT(bdp));
	ppgrp_detach(BHV_NEXT(bdp), p);
}

void
dspgrp_linkage(
	bhv_desc_t	*bdp,
	proc_t		*p,
	pid_t		parent_pgid,
	pid_t		parent_sid)
{
	/* REFERENCED */
	vpgrp_t		*vpg = BHV_TO_VPGRP(bdp);

	PGRP_TRACE8("dspgrp_linkage", vpg->vpg_pgid, "pid", p->p_pid,
		    "parent_pgid", parent_pgid, "parent_sid", parent_sid);

	/*
	 * Here if parent of local process p has changed pgid or sid. 
	 * There must be a (non-empty) ppgrp
	 * behavior beneath. This is a simple pass-through.
	 * Note: interesting pgrp orphaning is the subject
	 * of virtual operations called below this.
	 */
	ASSERT(vpg == p->p_vpgrp);
	ASSERT(BHV_NEXT(bdp));
	ppgrp_linkage(BHV_NEXT(bdp), p, parent_pgid, parent_sid);
}

int
dspgrp_sendsig(
	bhv_desc_t	*bdp,
	int		sig,
	int		options,
	pid_t		sid,
	cred_t		*credp,
	k_siginfo_t	*infop)
{
	dspgrp_t	*dsp = BHV_TO_DSPGRP(bdp);
	/* REFERENCED */
	tks_iter_t	result;
	/* REFERENCED */
	vpgrp_t	*vpg = BHV_TO_VPGRP(bdp);
	int		error;
	sig_arg_t	arg;

	PGRP_TRACE6("dspgrp_sendsig", vpg->vpg_pgid, "sig", sig, "options",
		options);

	/* Initialize argument block */
	arg.sig   = sig;
	arg.opts  = options;
	arg.sid   = sid;
	arg.credp = credp;
	arg.infop = infop;
	arg.error = 0;

	/*
	 * Here to send a signal to all pgrp members. However, the
	 * orphan pgrp handling option delivers SIGCONT in addition
	 * to SIGHUP signals.
	 */

	/*
	 * Ensure we can safely iterate.
	 */
	error = tkc_acquire1(dsp->dspg_tclient,
			VPGRP_NO_MEMBER_RELOCATING_TOKEN);
	ASSERT(!error);

	result = tks_iterate(dsp->dspg_tserver,
			     VPGRP_MEMBER_TOKENSET,
			     TKS_STABLE,
			     sendsig_iterator,
			     cellid(),
			     &arg);
	ASSERT(result == TKS_CONTINUE);
	error = ppgrp_sendsig(BHV_NEXT(bdp), sig,
			      options, sid, credp, infop);
	tkc_release1(dsp->dspg_tclient, VPGRP_NO_MEMBER_RELOCATING_TOKEN);
	return (error ? error : arg.error);
}

sequence_num_t
dspgrp_sigseq(
	bhv_desc_t	*bdp)
{
	return ppgrp_sigseq(BHV_NEXT(bdp));
}


int
dspgrp_sig_wait(
	bhv_desc_t	*bdp,
	sequence_num_t	seq)
{
	/* REFERENCED */
	vpgrp_t		*vpg = BHV_TO_VPGRP(bdp);

	PGRP_TRACE4("dspgrp_sig_wait", vpg->vpg_pgid, "sequence", seq);

	ASSERT(BHV_NEXT(bdp));
	return ppgrp_sig_wait(BHV_NEXT(bdp), seq);
}

void
dspgrp_orphan(
	bhv_desc_t	*bdp,
	int		is_orphaned,
	int		is_exit)
{
	dspgrp_t	*dsp = BHV_TO_DSPGRP(bdp);
	/* REFERENCED */
	vpgrp_t	*vpg = BHV_TO_VPGRP(bdp);

	/*
	 * There has been a local change. Depending on the local
	 * state, we get the NONORPHAN token if the pgrp is now not
	 * orphaned, or we return it if the local state is now orphaned.
	 * In the latter case, the server tells us what the
	 * global state becomes.
	 */
	if (!is_orphaned) {
		tkc_acquire1(dsp->dspg_tclient, VPGRP_NONORPHAN_TOKEN);
		PGRP_TRACE2("dspgrp_orphan - NONORPHAN token taken",
			vpg->vpg_pgid);
	} else {
		tks_iter_t	result;
		int		error;

		tkc_release1(dsp->dspg_tclient, VPGRP_NONORPHAN_TOKEN);
		tkc_recall(dsp->dspg_tclient,
			   VPGRP_NONORPHAN_TOKENSET, TK_DISP_CLIENT_ALL);
		PGRP_TRACE2("dspgrp_orphan - NONORPHAN token returned",
			vpg->vpg_pgid);

		/*
		 * See whether any other cell is preventing the pgrp
		 * from being orphaned.
		 */

		/*
		 * Ensure we can safely iterate.
		 */
		error = tkc_acquire1(dsp->dspg_tclient,
				VPGRP_NO_MEMBER_RELOCATING_TOKEN);
		ASSERT(!error);

		result = tks_iterate(dsp->dspg_tserver,
				     VPGRP_NONORPHAN_TOKENSET,
				     TKS_STABLE,
				     nonorphan_iterator);
		ASSERT(result == TKS_STOP || result == TKS_CONTINUE);
		if (result == TKS_CONTINUE) {
			/*
			 * The pgrp is trully orphaned.
			 */
			PGRP_TRACE4("dspgrp_orphan - orphaned", vpg->vpg_pgid,
				    "is_exit", is_exit);
			ppgrp_orphan(BHV_NEXT(bdp), 1, is_exit);
		} else {
			PGRP_TRACE2("dspgrp_orphan - not orphaned",
				    vpg->vpg_pgid);
		}
		tkc_release1(dsp->dspg_tclient,
			VPGRP_NO_MEMBER_RELOCATING_TOKEN);

	}
	return;
}

void
dspgrp_anystop(
	bhv_desc_t	*bdp,
	pid_t		pid,
	int		*is_stopped)
{
	dspgrp_t	*dsp = BHV_TO_DSPGRP(bdp);
	tks_iter_t	result;
	int		error;
	/* REFERENCED */
	vpgrp_t		*vpg = BHV_TO_VPGRP(bdp);

	/*
	 * Here to determine whether any process group member is stopped.
	 *
	 * This operation is called from VPGRP_LEAVE and VPGRP_DETACH.
	 * Pgrp membership must be locked while this is performed.
	 * The server will hold the READ level of the MEMBER_LIST token.
	 */

	/*
	 * Perform the operation first locally. If one is found,
	 * there's no need to go elsewhere. Otherwise, we must
	 * iterate over other member cells.
	 */
	ppgrp_anystop(BHV_NEXT(bdp), pid, is_stopped);
	if (*is_stopped)
		return;

	/*
	 * Ensure we can safely iterate.
	 */
	error = tkc_acquire1(dsp->dspg_tclient,
			VPGRP_NO_MEMBER_RELOCATING_TOKEN);
	ASSERT(!error);

	result = tks_iterate(dsp->dspg_tserver,
			     VPGRP_MEMBER_TOKENSET,
			     TKS_STABLE,
			     anystop_iterator,
			     cellid(),
			     pid);
	*is_stopped = (result == TKS_STOP);
	tkc_release1(dsp->dspg_tclient, VPGRP_NO_MEMBER_RELOCATING_TOKEN);
	PGRP_TRACE6("dspgrp_anystop", vpg->vpg_pgid, "exclude", pid,
		"is_stopped", *is_stopped);
	return;
}

/* ARGSUSED */
void
dspgrp_membership(
	bhv_desc_t	*bdp,
	int		count,
	pid_t		sid)
{
	dspgrp_t	*dsp = BHV_TO_DSPGRP(bdp);
	/* REFERENCED */
	vpgrp_t		*vpg = BHV_TO_VPGRP(bdp);

	PGRP_TRACE4("dspgrp_membership", vpg->vpg_pgid,
		    "count", count);

	if (count == 0) {
		/*
		 * No local members. When member tokens idle, we'll invalidate
		 * the vpgrp and recall existence tokens. But there's
		 * nothing to be done at this point.
		 */
		return;
	}

	if (!dsp->dspg_notify_idle) {
		/*
		 * Need callout when no member remains.
		 */
		tks_notify_idle(dsp->dspg_tserver, VPGRP_MEMBER_TOKENSET);

		dsp->dspg_notify_idle = 1;
	}
}

void
dspgrp_destroy(
	bhv_desc_t	*bdp)
{
	dspgrp_t	*dsp = BHV_TO_DSPGRP(bdp);
	vpgrp_t		*vpg = BHV_TO_VPGRP(bdp);
	vsession_t	*vsp;

	ASSERT(!VPGRP_IS_VALID(vpg));

	PGRP_TRACE2("dspgrp_destroy", vpg->vpg_pgid);

	vsp = VSESSION_LOOKUP(vpg->vpg_sid);
	ASSERT(vsp);
	VSESSION_LEAVE(vsp, vpg);
	VSESSION_RELE(vsp);
	VSESSION_RELE(vsp);

	ppgrp_destroy(BHV_NEXT(bdp));

	/*
	 * No tokens can be outstanding.
	 */
	tkc_destroy_local(dsp->dspg_tclient);
	tks_destroy(dsp->dspg_tserver);
	OBJ_STATE_DESTROY(&dsp->dspg_obj_state);
	DSPGRP_MEM_LOCK_DESTROY(dsp);

	bhv_remove(&vpg->vpg_bhvh, &dsp->dspg_bhv);
	kern_free(dsp);
}

int
dspgrp_setbatch(
	bhv_desc_t 	*bdp)
{
	int		error;
	/* REFERENCED */
	vpgrp_t		*vpg = BHV_TO_VPGRP(bdp);

	error = ppgrp_setbatch(BHV_NEXT(bdp));

	PGRP_TRACE4("dspgrp_setbatch", vpg->vpg_pgid, "error", error);
	return error;
}

int
dspgrp_clearbatch(
	bhv_desc_t	*bdp)
{
	int		error;
	/* REFERENCED */
	vpgrp_t		*vpg = BHV_TO_VPGRP(bdp);

	error = ppgrp_setbatch(BHV_NEXT(bdp));

	PGRP_TRACE4("dspgrp_clearbatch", vpg->vpg_pgid, "error", error);
	return error;
}

int
dspgrp_nice(
	bhv_desc_t	*bdp,
	int		flags,
	int		*nice,
	int		*count,
	cred_t		*scred)
{
	dspgrp_t	*dsp = BHV_TO_DSPGRP(bdp);
	/* REFERENCED */
	tks_iter_t	result;
	/* REFERENCED */
	vpgrp_t		*vpg = BHV_TO_VPGRP(bdp);
	int		lcnt, rcnt;
	int		lpri, rpri;
	int		error;

	/* BSD setpriority and getpriority system calls.
	 * 'flag' tells which we are doing.
	 * For setpriority, set all members of the pgroups priority
	 * to *nice.
	 * For getpriority, return 'best' priority of all pgroup
	 * members in *nice.
	 *
	 * Return EPERM for permission violations. Set *count to number
	 * of processes acted upon so caller can determine if ESRCH.
	 */

	/* Perform the operation first locally. errors
	 * terminate the operation.
	 */

	if (flags & VPG_GET_NICE)
		rpri = lpri = 0;
	else
		rpri = lpri = *nice;

	/*
	 * Ensure we can safely iterate.
	 */
	error = tkc_acquire1(dsp->dspg_tclient,
			VPGRP_NO_MEMBER_RELOCATING_TOKEN);
	ASSERT(!error);

	error = ppgrp_nice(BHV_NEXT(bdp), flags, &lpri, &lcnt, scred);

	if (error) {
		tkc_release1(dsp->dspg_tclient,
			VPGRP_NO_MEMBER_RELOCATING_TOKEN);
		PGRP_TRACE8("dspgrp_nice (local)", vpg->vpg_pgid,
			"flags", flags, "nice", *nice, "err", error);
		return error;
	}

	result = tks_iterate(dsp->dspg_tserver,
			     VPGRP_MEMBER_TOKENSET,
			     TKS_STABLE,
			     nice_iterator,
			     cellid(),
			     flags,
			     &rpri,
			     &rcnt,
			     scred,
			     &error);

	ASSERT(result == TKS_CONTINUE || error != 0);

	tkc_release1(dsp->dspg_tclient, VPGRP_NO_MEMBER_RELOCATING_TOKEN);

	PGRP_TRACE10("dspgrp_nice", vpg->vpg_pgid, "flags", flags, "nice",
			*nice, "err", error, "cnt", lcnt + rcnt);

	if (error)
		return error;

	if (flags & VPG_GET_NICE)
		*nice = MIN(lpri, rpri);

	*count = lcnt + rcnt;

	return 0;
}
