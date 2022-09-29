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
#ident "$Id: dcpgrp.c,v 1.37 1998/01/10 02:40:00 ack Exp $"

/*
 * client-side object management for Process Group Management
 */

#include <sys/types.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/sema.h>
#include <sys/systm.h>
#include <sys/ksignal.h>
#include <sys/space.h>
#include <values.h>

#include <ksys/behavior.h>
#include <ksys/cell/tkm.h>
#include <ksys/vproc.h>
#include <ksys/vpgrp.h>
#include <ksys/vsession.h>
#include <ksys/cred.h>
#include <ksys/cell/wp.h>

#include "pproc_private.h"

#include "dpgrp.h"
#include "invk_dspgrp_stubs.h"
#include "I_dcpgrp_stubs.h"
#include "vpgrp_private.h"
#include "pproc_private.h"

#include <sys/ktrace.h>

/*
 * Protos for client-side distributed layer.
 */
static void dcpgrp_getattr(bhv_desc_t *, pid_t *, int *, int *);
static int  dcpgrp_join_begin(bhv_desc_t *);
static void dcpgrp_join_end(bhv_desc_t *, struct proc *, int);
static void dcpgrp_leave(bhv_desc_t *, struct proc *, int);
static void dcpgrp_detach(bhv_desc_t *, struct proc *);
static void dcpgrp_linkage(bhv_desc_t *, struct proc *, pid_t, pid_t);
static void dcpgrp_orphan(bhv_desc_t *, int, int);
static int  dcpgrp_sendsig(bhv_desc_t *, int, int,
			   pid_t, cred_t *, k_siginfo_t *);
static sequence_num_t dcpgrp_sigseq(bhv_desc_t *);
static int  dcpgrp_sig_wait(bhv_desc_t *, sequence_num_t);
static void dcpgrp_anystop(bhv_desc_t *, int, int *);
static void dcpgrp_membership(bhv_desc_t *, int, pid_t);
static void dcpgrp_destroy(bhv_desc_t *);
static int  dcpgrp_nice(bhv_desc_t *, int, int *, int *, cred_t *);
static int  dcpgrp_setbatch(bhv_desc_t *);
static int  dcpgrp_clearbatch(bhv_desc_t *);	

pgrp_ops_t dcpgrp_ops = {
	BHV_IDENTITY_INIT_POSITION(VPGRP_POSITION_DC),
	dcpgrp_getattr,
	dcpgrp_join_begin,
	dcpgrp_join_end,
	dcpgrp_leave,
	dcpgrp_detach,
	dcpgrp_linkage,
	dcpgrp_sendsig,
	dcpgrp_sigseq,
	dcpgrp_sig_wait,
	dcpgrp_setbatch,
	dcpgrp_clearbatch,
	dcpgrp_nice,
	dcpgrp_orphan,
	dcpgrp_anystop,
	dcpgrp_membership,
	dcpgrp_destroy,
};

static void dcpgrp_tcif_obtain(void *, tk_set_t, tk_set_t, tk_disp_t,
				tk_set_t *);
static void dcpgrp_tcif_return(tkc_state_t, void *, tk_set_t, tk_set_t,
				tk_disp_t);
/* Made extern for vpgrp migration */
extern tkc_ifstate_t dcpgrp_tclient_iface = {
		dcpgrp_tcif_obtain,
		dcpgrp_tcif_return
};

/* ARGSUSED */
static int
dcpgrp_alloc(
	int		pgid,
	service_t	svc,
	dcpgrp_t 	**dcpp,
	pid_t		*sidp,
	int		*is_batchp)
{
	dcpgrp_t	*dcp;
	tk_set_t	already, refused;
	/* REFERENCED */
	tk_set_t	 granted;
	objid_t	 objid;
	/* REFERENCED */
	int		error;
	/* REFERENCED */
	int		msgerr;
	cell_t		relocated_to;
	sequence_num_t	sigseq;			/* not returned */

	/*
	 * Try for EXISTENCE token from server
	 * Call remote cell, looping if necessary to cope with redirection
	 * to a new cell if the ppgrp has migrated from its origin - strictly
	 * its name service manager.
	 */
	for (;;) {
		msgerr = invk_dspgrp_obtain(svc, &error,
				   cellid(), pgid, VPGRP_EXISTENCE_TOKENSET,
				   TK_NULLSET, TK_NULLSET,
				   &already, &granted, &refused,
				   &objid, sidp, is_batchp, &sigseq,
				   &relocated_to);
		ASSERT(!msgerr);
		if (error != EMIGRATED)
			break;
		SERVICE_TO_CELL(svc) = relocated_to;
	}
	ASSERT(error == 0 || refused == VPGRP_EXISTENCE_TOKENSET);
	ASSERT(error != 0 || (granted|already|refused) == VPGRP_EXISTENCE_TOKENSET);

	if (refused == VPGRP_EXISTENCE_TOKENSET) {
		/* server didn't know about id */
		return ESRCH;
	} else if (already != TK_NULLSET) {
		/* server already gave another thread the token */
		return EAGAIN;
	}

	dcp = kern_malloc(sizeof(dcpgrp_t));
	ASSERT(dcp);

	HANDLE_MAKE(dcp->dcpg_handle, svc, objid);
	tkc_create("dpgrp", dcp->dcpg_tclient, dcp, &dcpgrp_tclient_iface,
			VPGRP_NTOKENS, granted, granted,
			(void *)(__psint_t)pgid);

	*dcpp = dcp;
	return 0;
}

/*
 * Create a distributed client for pgpr pgid. This function can return
 * an error if the server either:
 * 1) doesn't know about pgid
 * 2) thinks this client already knows about pgid
 */
int
dcpgrp_create(int pgid, service_t svc, vpgrp_t **vpgrpp)
{
	vpgrp_t		*vpgrp;
	dcpgrp_t	*dcp;
	pid_t		sid;
	int		is_batch;
	int		error;

	/*
	 * Create the distributed object
	 * This fills in the key from the server
	 */
	error = dcpgrp_alloc(pgid, svc, &dcp, &sid, &is_batch);
	if (error)
		return(error);

	/*
	 * If we get to this point, the server has blessed us with
	 * the existence token so we are the only thread able to create
	 * the v/dc chain.
	 */

	/*
	 * Allocate the virtual object for the process group
	 * for this cell.
	 */
	vpgrp = vpgrp_alloc(pgid, sid);

	/* Initialize and insert behavior descriptor. */
	bhv_desc_init(&dcp->dcpg_bhv, dcp, vpgrp, &dcpgrp_ops);
	BHV_WRITE_LOCK(&vpgrp->vpg_bhvh);
	bhv_insert(&vpgrp->vpg_bhvh, &dcp->dcpg_bhv);
	BHV_WRITE_UNLOCK(&vpgrp->vpg_bhvh);

	ppgrp_setattr(BHV_NEXT(&dcp->dcpg_bhv), &is_batch);

	/*
	 * All set up - bump reference and insert it into the hash table.
	 */
	VPGRP_HOLD(vpgrp);
	vpgrp_enter(vpgrp);

	/*
	 * Return success and a pointer to the new virtual pgrp object.
	 */
	PGRP_TRACE4("dcpgrp_create", vpgrp->vpg_pgid, "vpgrp", vpgrp);
	*vpgrpp = vpgrp;
	return(0);
}

/*
 * client token callout to obtain a token.
 * Note that this is never called on the server since that will
 * always go through the local_client interface
 */
static void
dcpgrp_tcif_obtain(
	void		*obj,
	tk_set_t	to_be_obtained,
	tk_set_t	to_be_returned,
	tk_disp_t	dofret,
	tk_set_t	*refused)
{
	vpgrp_t		*vpgrp;
	dcpgrp_t	*dcp = (dcpgrp_t *)obj;
	/* REFERENCED */
	tk_set_t	granted, already;
	cell_t		relocated_to;
	int		error;
	/* REFERENCED */
	int		msgerr;
	pid_t		sid;
	int		is_batch;
	sequence_num_t	sigseq;

	vpgrp = BHV_TO_VPGRP(&dcp->dcpg_bhv);

	PGRP_TRACE6("dcpgrp_tcif_obtain", vpgrp->vpg_pgid, "vpgrp", 
		vpgrp, "to_be_obtained", to_be_obtained);

	/*
	 * Call remote cell, redirection to a new cell is not expected
	 * since the object handle is guaranteed at this point.
	 */
	msgerr = invk_dspgrp_obtain(DCPGRP_TO_SERVICE(dcp),
			   &error,
			   cellid(), vpgrp->vpg_pgid, to_be_obtained,
			   to_be_returned, dofret,
			   &already, &granted, refused,
			   &dcp->dcpg_handle.h_objid,
			   &sid, &is_batch,
			   &sigseq,
			   &relocated_to);
	ASSERT(!msgerr);
	ASSERT(already == TK_NULLSET);
	ASSERT(error == 0 || *refused == to_be_obtained);
	ASSERT(error != 0 || granted == to_be_obtained);

	/*
	 * Initialize signal sequence count with value returned if
	 * MEMBER token has been obtained.
	 */
	if (TK_IS_IN_SET(VPGRP_MEMBER_TOKENSET, granted))
		ppgrp_set_sigseq(BHV_NEXT(&dcp->dcpg_bhv), sigseq);
	
	PGRP_TRACE8("dcpgrp_tcif_obtain", vpgrp->vpg_pgid, "vpgrp", 
		vpgrp, "seq", sigseq, "granted", granted);
}

/*
 * token client return callout
 */
/* ARGSUSED */
static void
dcpgrp_tcif_return(
	tkc_state_t	tclient,
	void		*obj,
	tk_set_t	to_be_returned,
	tk_set_t	unknown,
	tk_disp_t	why)
{
	/* REFERENCED */
	int		msgerr;
	dcpgrp_t	*dcp = (dcpgrp_t *)obj;

	ASSERT(tclient == dcp->dcpg_tclient);

	PGRP_TRACE6("dcpgrp_tcif_return",
		BHV_TO_VPGRP(&dcp->dcpg_bhv)->vpg_pgid,
		"obj", obj, "to_be_returned", to_be_returned);

	msgerr = invk_dspgrp_return(DCPGRP_TO_SERVICE(dcp),
			   cellid(), 
			   DCPGRP_TO_OBJID(dcp),
			   to_be_returned, unknown, why);
	ASSERT(!msgerr);
	tkc_returned(tclient, to_be_returned, TK_NULLSET);
}

/*
 * server initiated message to client to return a token - client side
 */
void
I_dcpgrp_recall(
	int		*error,
	int		pgid,
	tk_set_t	to_be_revoked,
	tk_disp_t	why)
{
	vpgrp_t		*vpgrp;
	bhv_desc_t	*bdp;
	dcpgrp_t	*dcp;

	/*
	 * it's possible that we are getting a revoke for something
	 * we haven't yet got in our tables - this can happen if the
	 * revoke and obtain pass in the night
	 */
	vpgrp = vpgrp_lookup_local(pgid);
	if (vpgrp == NULL) {
		*error = ENOENT;
		return;
	}

	bdp = VPGRP_TO_FIRST_BHV(vpgrp);
	dcp = BHV_TO_DCPGRP(bdp);

	tkc_recall(dcp->dcpg_tclient, to_be_revoked, why);
	if (to_be_revoked == VPGRP_EXISTENCE_TOKENSET) {
		vpgrp_remove(vpgrp);		/* no more lookups */
		tkc_release1(dcp->dcpg_tclient, VPGRP_EXISTENCE_TOKEN);
	}
	*error = 0;
	/* drop reference that lookup_id_local gave us */
	VPGRP_RELE(vpgrp);	/* This may cause the vgprp to vaporize */
	return;
}

/*
 * This is called from the server to verify that the local
 * client, holder of the NONORPHAN token, indeed still prevents
 * the pgrp being orphaned.
 */
void
I_dcpgrp_getattr(
	pid_t		pgid,
	pid_t		*sid,
	int		*is_orphaned,
	int		*is_batch)
{
	vpgrp_t		*vpg;
	bhv_desc_t	*bdp;

	vpg = VPGRP_LOOKUP(pgid);
	ASSERT(vpg);
	bdp = VPGRP_TO_FIRST_BHV(vpg);
	ASSERT(BHV_OPS(bdp) == &dcpgrp_ops);
	ppgrp_getattr(BHV_NEXT(bdp), sid, is_orphaned, is_batch);
	VPGRP_RELE(vpg);

	PGRP_TRACE8("I_dcpgrp_getattr", pgid, "sid", *sid,
		"is_orphaned", is_orphaned, "is_batch", is_batch);
}

/*
 * The following is called from the server to signal all pgrp
 * members on the called cell.
 */
void
I_dcpgrp_sendsig(
	pid_t		pgid,
	int		sig,
	int		options,
	pid_t		sid,
	credid_t	credid,
	k_siginfo_t	*infop,
	size_t		ninfo,
	int		*errorp)
{
	vpgrp_t		*vpg;
	bhv_desc_t	*bdp;
	cred_t		*credp = CREDID_GETCRED(credid);

	vpg = VPGRP_LOOKUP(pgid);
	ASSERT(vpg);
	bdp = VPGRP_TO_FIRST_BHV(vpg);
	*errorp = ppgrp_sendsig(BHV_NEXT(bdp), sig, options, sid, credp,
					ninfo ? infop : NULL);
	VPGRP_RELE(vpg);
	if (credp)
		crfree(credp);

	PGRP_TRACE6("I_dcpgrp_sendsig", pgid, "sig", sig, "options", options);
}

/*
 * This following is called from the server to discover whether any
 * pgrp member is stopped. This is used to determine whether to
 * SIGHUP/SIGCONT an orphaned pgrp.
 */
void
I_dcpgrp_anystop(
	pid_t		pgid,
	pid_t		excluded,
	int		*is_stopped)
{
	vpgrp_t		*vpg;
	bhv_desc_t	*bdp;

	vpg = VPGRP_LOOKUP(pgid);
	ASSERT(vpg);
	bdp = VPGRP_TO_FIRST_BHV(vpg);
	ASSERT(BHV_OPS(bdp) == &dcpgrp_ops);
	ppgrp_anystop(BHV_NEXT(bdp), excluded, is_stopped);
	VPGRP_RELE(vpg);

	PGRP_TRACE6("I_dcpgrp_anystop", pgid,
		"excluded",  excluded, "*is_stopped", *is_stopped);
}


/* BSD setpriority/getpriority */
void
I_dcpgrp_nice(
	pid_t		pgid,
	int		flags,
	int		*nice,
	int		*count,
	credid_t	scredid,
	int		*error)
{
	vpgrp_t		*vpg;
	bhv_desc_t	*bdp;
	cred_t		*scred = CREDID_GETCRED(scredid);

	vpg = VPGRP_LOOKUP(pgid);
	ASSERT(vpg);
	bdp = VPGRP_TO_FIRST_BHV(vpg);
	ASSERT(BHV_OPS(bdp) == &dcpgrp_ops);

	*error = ppgrp_nice(BHV_NEXT(bdp), flags, nice, count, scred);

	VPGRP_RELE(vpg);
	if (scred)
		crfree(scred);
	PGRP_TRACE8("I_dcpgrp_nice", pgid,
		"flags", flags, "nice", nice, "error", error);
}

/*
 * This message is used to create dc on the origin cell of a
 * pgrp created by a pgrp leader process away from its home.
 */
void
I_dcpgrp_create_origin(
	cell_t		server,
	objid_t		objid,
	pid_t		pgid,
	pid_t		sid,
	tk_set_t	granted,
	int		*error)
{
	dcpgrp_t	*dcp;
	service_t	svc;
	vpgrp_t		*vpgrp;
	vpgrptab_t	*vq;

	ASSERT(pgid_is_local(pgid));

	/*
	 * Allocate a vprgp.
	 */
	vpgrp = vpgrp_alloc(pgid, sid);

	vq = VPGRP_ENTRY(pgid);
	VPGRPTAB_LOCK(vq, MR_UPDATE);
	if (vpgrp_qlookup_locked(&vq->vpgt_queue, pgid)) {
		/*
		 * Oops, someone beat us to it. Undo what we have done
		 * and return an error.
		 */
		VPGRPTAB_UNLOCK(vq);
		vpgrp_destroy(vpgrp);

		*error = EAGAIN;
		PGRP_TRACE4("I_dcpgrp_create_origin", pgid, "error", *error);
		return;
	}

	/* Create dc and initialize client token state. */
	dcp = kern_malloc(sizeof(dcpgrp_t));
	ASSERT(dcp);
	SERVICE_MAKE(svc, server, SVC_PGRP);
	HANDLE_MAKE(dcp->dcpg_handle, svc, objid);
	tkc_create("dpgrp", dcp->dcpg_tclient, dcp, &dcpgrp_tclient_iface,
			VPGRP_NTOKENS, granted, granted,
			(void *)(__psint_t)pgid);

	/* Initialize and insert behavior descriptor. */
	bhv_desc_init(&dcp->dcpg_bhv, dcp, vpgrp, &dcpgrp_ops);
	BHV_WRITE_LOCK(&vpgrp->vpg_bhvh);
	bhv_insert(&vpgrp->vpg_bhvh, &dcp->dcpg_bhv);
	BHV_WRITE_UNLOCK(&vpgrp->vpg_bhvh);

	kqueue_enter(&vq->vpgt_queue, &vpgrp->vpg_queue);
	PID_PGRP_JOIN(pgid);

	VPGRPTAB_UNLOCK(vq);

	/*
	 * Return success and a pointer to the new virtual pgrp object.
	 */
	PGRP_TRACE4("I_dcpgrp_create_origin", vpgrp->vpg_pgid, "vpgrp", vpgrp);
	*error = 0;
}

/*
 * Client-side distributed operations follow:
 */
void
dcpgrp_getattr(
	bhv_desc_t	*bdp,
	pid_t		*sidp,
	int		*is_orphanedp,
	int		*is_batchp)
{
	dcpgrp_t	*dcp = BHV_TO_DCPGRP(bdp);
	vpgrp_t		*vpg = BHV_TO_VPGRP(bdp);
	pid_t		sid;
	int		is_orphaned;
	int		is_batch;
	/* REFERENCED */
	int		msgerr;

	/*
	 * If there's a pgrp locally, see what is says.
	 * If the local group isn't orphaned, that's the global
	 * state too.
	 * Otherwise, we need to consult the server.
	 */
	ppgrp_getattr(BHV_NEXT(bdp), &sid, &is_orphaned, &is_batch);
	if (is_batchp == NULL && (is_orphanedp == NULL || !is_orphaned))
		goto out;

	msgerr = invk_dspgrp_getattr(DCPGRP_TO_SERVICE(dcp),
			    DCPGRP_TO_OBJID(dcp), vpg->vpg_pgid,
			    &sid, &is_orphaned, &is_batch);
	ASSERT(!msgerr);

out:
	if (sidp)
		*sidp = sid;
	if (is_orphanedp)
		*is_orphanedp = is_orphaned;
	if (is_batchp)
		*is_batchp = is_batch; 
	PGRP_TRACE8("dcpgrp_getattr", vpg->vpg_pgid, "*sidp", sidp ? sid : -1,
		"*is_orphanedp", is_orphanedp ? is_orphaned : -1,
		"*is_batchp", is_batchp ? is_batch : -1);
}

int
dcpgrp_join_begin(
	bhv_desc_t	*bdp)
{
	dcpgrp_t	*dcp = BHV_TO_DCPGRP(bdp);
	/* REFERENCED */
	vpgrp_t		*vpg = BHV_TO_VPGRP(bdp);
	int		error;

	PGRP_TRACE2("dcpgrp_join_begin", vpg->vpg_pgid);

	/*
	 * Get member token - may be refused if the pgrp is being torn-down.
	 */
	error = tkc_acquire1(dcp->dcpg_tclient, VPGRP_MEMBER_TOKEN);
	if (!error) {
		error = ppgrp_join_begin(BHV_NEXT(bdp));
		ASSERT(!error);
	}
	return error;
}

void
dcpgrp_join_end(
	bhv_desc_t	*bdp,
	proc_t		*p,
	int		attach)
{
	/* REFERENCED */
	vpgrp_t		*vpg = BHV_TO_VPGRP(bdp);

	PGRP_TRACE6("dcpgrp_join_end", vpg->vpg_pgid, "pid", p->p_pid, "attach",
		attach);

	/*
	 * Here if local process p is joining a pgrp served
	 * remotely. There will be a (possibly empty) ppgrp
	 * behavior beneath.
	 */
	ppgrp_join_end(BHV_NEXT(bdp), p, attach);
}


void
dcpgrp_leave(
	bhv_desc_t	*bdp,
	proc_t		*p,
	int		exitting)
{
	dcpgrp_t	*dcp = BHV_TO_DCPGRP(bdp);
	vpgrp_t		*vpg = BHV_TO_VPGRP(bdp);
	tk_set_t	to_return;
	tk_disp_t	why;

	PGRP_TRACE6("dcpgrp_leave", vpg->vpg_pgid, "pid", p->p_pid, "exiting",
		exitting);
	/*
	 * Here if local process p is leaving a pgrp served
	 * remotely. There will be a (non-empty) ppgrp
	 * behavior beneath.
	 */
	ppgrp_leave(BHV_NEXT(bdp), p, exitting);
	tkc_release1(dcp->dcpg_tclient, VPGRP_MEMBER_TOKEN);

	/*
	 * Attempt to return the member token. The token module
	 * will allow this only if this token is not busy - i.e.
	 * there are no members remaining locally.
	 */
	tkc_returning(dcp->dcpg_tclient, VPGRP_MEMBER_TOKENSET,
		      &to_return, &why, 0);
	if (to_return == VPGRP_MEMBER_TOKENSET) {
		/* REFERENCED */
		int	msgerr;

		msgerr = invk_dspgrp_leave(DCPGRP_TO_SERVICE(dcp), cellid(),
				  DCPGRP_TO_OBJID(dcp), vpg->vpg_pgid);
		ASSERT(!msgerr);
		tkc_returned(dcp->dcpg_tclient,
			     VPGRP_MEMBER_TOKENSET, TK_NULLSET);
	}
}

void
dcpgrp_detach(
	bhv_desc_t	*bdp,
	proc_t		*p)
{
	/* REFERENCED */
	vpgrp_t		*vpg = BHV_TO_VPGRP(bdp);

	PGRP_TRACE4("dcpgrp_detach", vpg->vpg_pgid, "pid", p->p_pid);

	/*
	 * Here if parent of local process p is leaving a pgrp served
	 * remotely. There must be a (non-empty) ppgrp
	 * behavior beneath. This is a simple pass-through.
	 * Note: interesting pgrp orphaning is the subject
	 * of virtual operations called below this.
	 */
	ASSERT(vpg == p->p_vpgrp);
	ASSERT(BHV_NEXT(bdp));
	ppgrp_detach(BHV_NEXT(bdp), p);
}

void
dcpgrp_linkage(
	bhv_desc_t	*bdp,
	proc_t		*p,
	pid_t		parent_pgid,
	pid_t		parent_sid)
{
	/* REFERENCED */
	vpgrp_t		*vpg = BHV_TO_VPGRP(bdp);

	PGRP_TRACE8("dcpgrp_linkage", vpg->vpg_pgid, "pid", p->p_pid,
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
dcpgrp_sendsig(
	bhv_desc_t	*bdp,
	int		sig,
	int		options,
	pid_t		sid,
	cred_t		*credp,
	k_siginfo_t	*infop)
{
	dcpgrp_t	*dcp = BHV_TO_DCPGRP(bdp);
	vpgrp_t		*vpg = BHV_TO_VPGRP(bdp);
	int		local_err;
	int		remote_err;
	/* REFERENCED */
	int		msgerr;

	PGRP_TRACE6("dcpgrp_sendsig", vpg->vpg_pgid, "sig", sig, "options",
		options);

	/*
	 * Here to send a signal to all pgrp members. However, the
	 * orphan pgrp handling option delivers SIGCONT in addition
	 * to SIGHUP signals.
	 */
	msgerr = invk_dspgrp_sendsig(DCPGRP_TO_SERVICE(dcp), cellid(),
			    DCPGRP_TO_OBJID(dcp), vpg->vpg_pgid,
			    sig,
			    options,
			    sid,
			    CRED_GETID(credp),
			    infop, infop ? 1 : 0,
			    &remote_err);
	ASSERT(!msgerr);
	local_err = ppgrp_sendsig(BHV_NEXT(bdp), sig, options,
				  sid, credp, infop);
	/* EPERM is possible */
	return (local_err ? local_err : remote_err);
}

sequence_num_t
dcpgrp_sigseq(
	bhv_desc_t	*bdp)
{
	return ppgrp_sigseq(BHV_NEXT(bdp));
}

int
dcpgrp_sig_wait(
	bhv_desc_t	*bdp,
	sequence_num_t	seq)
{
	/* REFERENCED */
	vpgrp_t		*vpg = BHV_TO_VPGRP(bdp);

	PGRP_TRACE4("dcpgrp_sig_wait", vpg->vpg_pgid, "sequence", seq);

	/*
	 * Wait for no signaling locally and for the expected
	 * sequence count.
	 */
	return ppgrp_sig_wait(BHV_NEXT(bdp), seq);
}

void
dcpgrp_orphan(
	bhv_desc_t	*bdp,
	int		is_orphaned,
	int		is_exit)
{
	dcpgrp_t	*dcp = BHV_TO_DCPGRP(bdp);
	vpgrp_t		*vpg = BHV_TO_VPGRP(bdp);

	/*
	 * This operation is called from VPGRP_JOIN, VPGRP_LEAVE
	 * and VPGRP_DETACH.
	 */
	ASSERT(BHV_NEXT(bdp));

	/*
	 * There has been a local change. Depending on the local
	 * state, we get the NONORPHAN token if the pgrp is now not
	 * orphaned, or we return it if the local state is now orphaned.
	 * In the latter case, the server tells us what the
	 * global state becomes.
	 */
	if (!is_orphaned) {
		tkc_acquire1(dcp->dcpg_tclient, VPGRP_NONORPHAN_TOKEN);
		PGRP_TRACE2("dcpgrp_orphan - not orphaned", vpg->vpg_pgid);
	} else {
		tk_set_t	to_return;
		tk_disp_t	why;
		/* REFERENCED */
		int		msgerr;

		tkc_release(dcp->dcpg_tclient, VPGRP_NONORPHAN_TOKENSET);
		tkc_returning(dcp->dcpg_tclient, VPGRP_NONORPHAN_TOKENSET,
			      &to_return, &why, 0);
		/*
		 * Rely on physical locking (holding) to prevent other
		 * members joining - this ensures that
		 * the return will not be refused.
		 */
		ASSERT(to_return == VPGRP_NONORPHAN_TOKENSET);
		msgerr = invk_dspgrp_orphan(DCPGRP_TO_SERVICE(dcp), cellid(),
				   DCPGRP_TO_OBJID(dcp),
				   vpg->vpg_pgid,
				   is_exit);
		ASSERT(!msgerr);
		tkc_returned(dcp->dcpg_tclient, to_return, TK_NULLSET);
		PGRP_TRACE2("dcpgrp_orphan - orphaned", vpg->vpg_pgid);
	}
	return;
}

void
dcpgrp_anystop(
	bhv_desc_t	*bdp,
	pid_t		exclude,
	int		*is_stopped)
{
	dcpgrp_t	*dcp = BHV_TO_DCPGRP(bdp);
	vpgrp_t		*vpg = BHV_TO_VPGRP(bdp);
	/* REFERENCED */
	int		msgerr;

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
	 * call the server to iterate over other member cells.
	 */
	ppgrp_anystop(BHV_NEXT(bdp), exclude, is_stopped);
	if (is_stopped) {
		PGRP_TRACE6("dcpgrp_anystop", vpg->vpg_pgid, "pid", exclude,
			"is_stopped here", *is_stopped);
		return;
	}

	msgerr = invk_dspgrp_anystop(DCPGRP_TO_SERVICE(dcp), cellid(),
			    DCPGRP_TO_OBJID(dcp), vpg->vpg_pgid,
			    exclude,
			    is_stopped);
	ASSERT(!msgerr);

	PGRP_TRACE6("dcpgrp_anystop", vpg->vpg_pgid, "pid", exclude,
			"is_stopped", *is_stopped);
	return;
}

/* ARGSUSED */
void
dcpgrp_membership(
	bhv_desc_t	*bdp,
	int		count,
	pid_t		sid)
{
	/* REFERENCED */
	vpgrp_t		*vpg = BHV_TO_VPGRP(bdp);

	PGRP_TRACE4("dcpgrp_membership", vpg->vpg_pgid,
		    "count", count);

	/*
	 * Local pgrp has no members.
	 * We do nothing so that the pgrp remains in the lookup table
	 * and cached. The eventual recall of the existence token will
	 * trigger its final destruction.
	 */ 
	return;
}

void
dcpgrp_destroy(
	bhv_desc_t	*bdp)
{
	dcpgrp_t	*dcp = BHV_TO_DCPGRP(bdp);
	vpgrp_t		*vpg = BHV_TO_VPGRP(bdp);

	PGRP_TRACE2("dcpgrp_destroy", vpg->vpg_pgid);

	/* 
	 * Must have been removed from the lookup lists
	 * to be destroyed.
	 */
	ASSERT(!VPGRP_IS_VALID(vpg));

	ppgrp_destroy(BHV_NEXT(bdp));

	tkc_destroy(dcp->dcpg_tclient);

	bhv_remove(&vpg->vpg_bhvh, &dcp->dcpg_bhv);
	kern_free(dcp);
}

int
dcpgrp_nice(
	bhv_desc_t	*bdp,
	int		flags,
	int		*nice,
	int		*cnt,
	cred_t		*scred)
{
	dcpgrp_t	*dcp = BHV_TO_DCPGRP(bdp);
	vpgrp_t		*vpg = BHV_TO_VPGRP(bdp);
	int		lcnt, rcnt;
	int		lpri, rpri;
	int		error;
	/* REFERENCED */
	int		msgerr;

	/* BSD setpriority and getpriority system calls.
	 * 'flag' tells which we are doing.
	 * For setpriority, set all members of the pgroups priority
	 * to *nice.
	 * For getpriority, return 'best' priority of all pgroup
	 * members in *nice.
	 *
	 * Return EPERM for permission violations. Set *cnt to number
	 * of processes acted upon so caller can determine if ESRCH.
	 */

	/* Perform the operation first locally. errors
	 * terminate the operation.
	 */
	if (flags & VPG_GET_NICE)
		rpri = lpri = 0; 
	else
		rpri = lpri = *nice;

	error = ppgrp_nice(BHV_NEXT(bdp), flags, &lpri, &lcnt, scred);

	if (error) {
		PGRP_TRACE8("dcpgrp_nice (local)", vpg->vpg_pgid, "flags",
			flags, "nice", nice, "err", error);
		return error;
	}

	msgerr = invk_dspgrp_nice(DCPGRP_TO_SERVICE(dcp), cellid(),
			    DCPGRP_TO_OBJID(dcp), vpg->vpg_pgid,
			    flags, &rpri, &rcnt,
			    CRED_GETID(scred), &error);
	ASSERT(!msgerr);

	PGRP_TRACE10("dcpgrp_nice", vpg->vpg_pgid, "flags", flags, "nice", nice,
			"err", error,  "cnt", lcnt + rcnt);

	if (error)
		return error;

	if (flags & VPG_GET_NICE)
		*nice = MAX(lpri, rpri);

	*cnt = lcnt + rcnt;

	return 0;
}

/* ARGSUSED */
int
dcpgrp_setbatch(	
	bhv_desc_t 	*bdp)
{
	panic("dcpgrp_setbatch: setbatch expected from server");
	return 0;
}
	
/* ARGSUSED */
int
dcpgrp_clearbatch( 
	bhv_desc_t	*bdp)
{
	dcpgrp_t	*dcp = BHV_TO_DCPGRP(bdp);
	vpgrp_t		*vpg = BHV_TO_VPGRP(bdp);
	/* REFERENCED */
	int		msgerr;

	msgerr = invk_dspgrp_clearbatch(DCPGRP_TO_SERVICE(dcp), cellid(),
					DCPGRP_TO_OBJID(dcp), vpg->vpg_pgid);
	ASSERT(!msgerr);

	return 0;
}
