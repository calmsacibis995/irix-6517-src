/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Id: pgrp_migrate.c,v 1.16 1997/10/14 14:32:34 cp Exp $"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/ksignal.h>
#include <sys/sema.h>
#include <sys/var.h>
#include <sys/debug.h>
#include <sys/ktrace.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <ksys/pid.h>

#include <ksys/cell/tkm.h>
#include <ksys/vproc.h>
#include <ksys/vpgrp.h>
#include "vpgrp_private.h"
#include "pgrp_migrate.h"
#include "dpgrp.h"
#include "pproc.h"
#include "I_dspgrp_stubs.h"
#include "invk_dspgrp_stubs.h"
#include "invk_dcpgrp_stubs.h"

extern tkc_ifstate_t dcpgrp_tclient_iface;
extern tks_ifstate_t dspgrp_tserver_iface;

/*
 * Manifest information passed source to target at prepare time.
 */
typedef struct {
	pid_t		pgid;
	pid_t		sid;
	int		is_batch;
	service_t	svc;
	objid_t		objid;
	int		is_pgrp_server;
	tk_set_t	tokens;
} vpgrp_src_mft_t;

#define OBJ_TAG_SRC_PREP	OBJ_SVC_TAG(SVC_PGRP,1)

/*
 * Token info passed source to target at bag time.
 */
#define OBJ_TAG_TOKENS		OBJ_SVC_TAG(SVC_PGRP,2)

static pgrp_t *
vpgrp_to_pgrp(
	vpgrp_t	*vpg)
{
	/* REFERENCED */
	bhv_head_t	*bhp = VPGRP_BHV_HEAD_PTR(vpg);
	bhv_desc_t	*bdp = VPGRP_TO_FIRST_BHV(vpg);
	pgrp_t		*pg;

	ASSERT(BHV_IS_READ_LOCKED(bhp));

	switch (BHV_POSITION(bdp)) {
	case VPGRP_POSITION_PP:
		pg = BHV_TO_PPGRP(bdp);
		break;
	case VPGRP_POSITION_DC:
		bdp = &BHV_TO_DCPGRP(bdp)->dcpg_bhv;
		bdp = BHV_NEXT(bdp);
		pg  = BHV_TO_PPGRP(bdp);
		break;
	case VPGRP_POSITION_DS:
		bdp = &BHV_TO_DSPGRP(bdp)->dspg_bhv;
		bdp = BHV_NEXT(bdp);
		pg  = BHV_TO_PPGRP(bdp);
	}
	return pg;
}

/*
 * Prepare local pgrp for the migration of a member process to
 * another cell.
 */
int
vpgrp_proc_emigrate_start(
	vpgrp_t		*vpg,		/* pgrp */
	cell_t		target_cell,	/* where it's going */
	obj_manifest_t	*mftp)		/* object manifest being constructed */
{
	bhv_head_t	*bhp;
	bhv_desc_t	*bdp;
	dspgrp_t	*dsp;
	tk_set_t	granted,
			refused,
			held;
	vpgrp_src_mft_t	src_info;
	/* REFERENCED */
	int		error;
	/* REFERENCED */
	int		bhvpos;

	bhp = VPGRP_BHV_HEAD_PTR(vpg);
	BHV_READ_LOCK(bhp);
	bdp = VPGRP_TO_FIRST_BHV(vpg);

	switch (bhvpos = BHV_POSITION(bdp)) {
	case VPGRP_POSITION_DC:
		/* Nothing much to do */
		src_info.tokens = TK_NULLSET;
		src_info.is_pgrp_server = 0;
		break;

	case VPGRP_POSITION_PP:
		/*
		 * Interpose ds.
		 */
		BHV_READ_UNLOCK(bhp);
		BHV_WRITE_LOCK(bhp);
		bdp = VPGRP_TO_FIRST_BHV(vpg);
		if (BHV_POSITION(bdp) == VPGRP_POSITION_PP) {
			vpgrp_interpose(vpg);
			bdp = VPGRP_TO_FIRST_BHV(vpg);
		}
		BHV_WRITE_TO_READ(bhp);

		/* FALLTHROUGH */

	case VPGRP_POSITION_DS:
		/*
		 * Need to ensure that the target cell has an existence tk.
		 */
		dsp = BHV_TO_DSPGRP(bdp);
		tks_obtain(dsp->dspg_tserver, target_cell,
			   VPGRP_EXISTENCE_TOKENSET,
			   &granted, &refused, &held);
		ASSERT(refused == TK_NULLSET);
		src_info.is_pgrp_server = 1;
		src_info.tokens = granted;
		SERVICE_MAKE(src_info.svc, cellid(), SVC_PGRP);
		src_info.objid	= BHV_TO_OBJID(bdp);
		ppgrp_getattr(BHV_NEXT(bdp), &src_info.sid, NULL,
				&src_info.is_batch);

		/*
		 * Prevent iterations over member cells while
		 * there's a member in transit.
		 * Note: if we're not on the server, we'll leave this
		 * to the target to do. If either we or the target is
		 * the serve, we'll save an rpc.
		 */
		error = tkc_acquire1(dsp->dspg_tclient, VPGRP_MEMBER_RELOCATING_TOKEN);
		ASSERT(!error);
	}
	/*
	 * Construct the prepare info and put it into the manifest.
	 */
	src_info.pgid  = vpg->vpg_pgid;
	error = obj_bag_put(obj_mft_src_info(mftp), OBJ_TAG_SRC_PREP,
			     &src_info, sizeof(src_info));
	ASSERT(!error);

	PGRP_TRACE8("vpgrp_proc_emigrate_start", src_info.pgid,
		    "cell", cellid(),
		    "bhvpos", bhvpos, "tokens", src_info.tokens);

	/* vpgrp referenced and the behavior chain left read-locked */
	VPGRP_HOLD(vpg);

	return 0;
}

/*
 * Prepare vpgrp for migration of process to the local cell. 
 * Details are contained in an object manifest.
 */
int
vpgrp_proc_immigrate_start(
	obj_manifest_t	*mftp,
	void 		**v)
{
	vpgrp_src_mft_t	src_info;
	vpgrp_t		*vpg;
	dcpgrp_t	*dcp;
	pgrp_t		*pg;
	int		error;
	
        /*
         * Extract object info from the manifest.
         */
	obj_bag_get_here(obj_mft_src_info(mftp), OBJ_TAG_SRC_PREP,
			 &src_info, sizeof(src_info), error);
	if (error)
		return error;

	/*
	 * Create or lookup the vpgrp.
	 */
	if (TK_IS_IN_SET(src_info.tokens, VPGRP_EXISTENCE_TOKENSET)) {
		ASSERT(src_info.is_pgrp_server);
		/*
		 * Sent an existence token, we have to create the
		 * pgrp locally.
		 */
		vpg = vpgrp_alloc(src_info.pgid, src_info.sid);

		/*
		 * Create dc and initialize token state.
		 */
		dcp = kern_malloc(sizeof(dcpgrp_t));
		ASSERT(dcp);
		HANDLE_MAKE(dcp->dcpg_handle, src_info.svc, src_info.objid);
		tkc_create("dpgrp", dcp->dcpg_tclient,
			   dcp, &dcpgrp_tclient_iface,
			   VPGRP_NTOKENS,
			   src_info.tokens,		/* cached */
			   VPGRP_EXISTENCE_TOKENSET,	/* held */
			   (void *)(__psint_t)src_info.pgid);

		/* Initialize and insert behavior descriptor. */
		bhv_desc_init(&dcp->dcpg_bhv, dcp, vpg, &dcpgrp_ops);
		BHV_WRITE_LOCK(&vpg->vpg_bhvh);
		bhv_insert(&vpg->vpg_bhvh, &dcp->dcpg_bhv);
		BHV_WRITE_UNLOCK(&vpg->vpg_bhvh);

		ppgrp_setattr(BHV_NEXT(&dcp->dcpg_bhv), &src_info.is_batch);

		/*
		 * All set up - bump ref and insert it into the hash table.
		 */
		VPGRP_HOLD(vpg);
		vpgrp_enter(vpg);

       		PGRP_TRACE6("ppgrp_proc_immigrate_start", vpg->vpg_pgid,
			    "vpgrp", vpg, "dc created", dcp);

	} else {
		vpg = VPGRP_LOOKUP(src_info.pgid);	
	  	ASSERT(vpg);
       		PGRP_TRACE4("ppgrp_proc_immigrate_start", vpg->vpg_pgid,
			    "vpgrp lookup", vpg);
	}

	/*
	 * If the process is migrating from the pgrp server.
	 * to avoid another round trip to fetch a member
	 * token, see whether we can piggy-back it
	 * on the bag steps. The following tkc_obtaining
	 * will synchronize this with other threads.
	 */
	if (src_info.is_pgrp_server) {
		tk_set_t	to_obtain;
		tk_set_t	to_return;
		tk_set_t	get_later;
		tk_set_t	refused;
		tk_disp_t	why;
		dcp = BHV_TO_DCPGRP(VPGRP_TO_FIRST_BHV(vpg));		
		tkc_obtaining(dcp->dcpg_tclient, VPGRP_MEMBER_TOKENSET,
			      &to_obtain, &to_return, &why, &refused,
			      &get_later);
		ASSERT(to_obtain == VPGRP_MEMBER_TOKENSET ||
		       to_obtain == TK_NULLSET);
		ASSERT(to_return == TK_NULLSET);
		ASSERT(refused == TK_NULLSET);
		ASSERT(get_later == TK_NULLSET);
	}

	/*
	 * Descend the behavior chain to the pgrp itself.
	 */
	BHV_READ_LOCK(&vpg->vpg_bhvh);
	pg = vpgrp_to_pgrp(vpg);


	if (!src_info.is_pgrp_server) {
		bhv_desc_t	*bdp = VPGRP_TO_FIRST_BHV(vpg);
		tkc_state_t	*tclient;

		/*
		 * Prevent iterations over member cells while
		 * there's a member in transit.
		 * Note: if the source was the server, it does this itself
		 * (to avoid unnecessary messaging).
		 */
		if (BHV_POSITION(bdp) == VPGRP_POSITION_DS) {
			tclient = BHV_TO_DSPGRP(bdp)->dspg_tclient;
		} else {
			tclient = BHV_TO_DCPGRP(bdp)->dcpg_tclient;
		}

		error = tkc_acquire1(tclient, VPGRP_MEMBER_RELOCATING_TOKEN);
		ASSERT(!error);
	}

	/*
	 * Grab the right to update the pgrp list.
	 */
	PGRP_LOCK(pg);
	PGRPLIST_WRITELOCK(pg);
	PGRP_UNLOCK(pg);

	*v = (void *) vpg;

	/* vpgrp left referenced */
	BHV_READ_UNLOCK(&vpg->vpg_bhvh);
	return 0;
}

/*
 * Remove process member from local pgrp list.
 * [Potentially, the pgrp could relocate at this time.]
 */
/* ARGSUSED */
int
vpgrp_proc_emigrate(
	vpgrp_t		*vpg,
	cell_t		target_cell,	/* where we're destined */
	proc_t		*p,		/* emigrating member process */
	obj_bag_t	bag)		/* object state bag */
{
	bhv_head_t	*bhp = VPGRP_BHV_HEAD_PTR(vpg);
	bhv_desc_t	*bdp = VPGRP_TO_FIRST_BHV(vpg);
	dspgrp_t	*dsp;
	tk_set_t	granted;
	tk_set_t	refused;
	tk_set_t	held;
	pgrp_t		*pg;
	/* REFERENCED */
	int		error;

	ASSERT(BHV_IS_READ_LOCKED(bhp));

	if (BHV_POSITION(bdp) == VPGRP_POSITION_DS) {
		/*
		 * Need to ensure that the target cell has a member tk.
		 * This is done here (and not at source_prepare time)
		 * because to avoid token push/return races the target
		 * must be obtaining.
		 */
		dsp = BHV_TO_DSPGRP(bdp);
		tks_obtain(dsp->dspg_tserver, target_cell,
			   VPGRP_MEMBER_TOKENSET,
			   &granted, &refused, &held);
		ASSERT(refused == TK_NULLSET);
		/*
		 * Put a tagged item (possibly empty) into the bag. 
		 */
		error = obj_bag_put(bag, OBJ_TAG_TOKENS,
				    &granted, sizeof(tk_set_t));
		ASSERT(!error);
	}

	pg = vpgrp_to_pgrp(vpg);

	/* mslock for update */
	PGRP_LOCK(pg);
	PGRPLIST_WRITELOCK(pg);
	PGRP_UNLOCK(pg);

	/*
	 * But we don't leave the local pgrp yet - not until the
	 * migratee has joined the remote pgrp.
	 */

	return 0;
}

/*
 * Add process member into local pgrp list.
 */
/* ARGSUSED */
int
vpgrp_proc_immigrate(
	vpgrp_t		*vpg,
	proc_t		*p,		/* immigrating member process */
	obj_bag_t	bag)		/* object state bag */
{
	bhv_head_t	*bhp;
	bhv_desc_t	*bdp;
	tk_set_t	tokens;
	tkc_state_t	*tclient;
	int		source_is_server = 0;
	dspgrp_t	*dsp = NULL;
	dcpgrp_t	*dcp = NULL;
	pgrp_t		*pg;
	int		error;

	BHV_READ_LOCK(&vpg->vpg_bhvh);
	bhp = VPGRP_BHV_HEAD_PTR(vpg);
	bdp = VPGRP_TO_FIRST_BHV(vpg);

	if (BHV_POSITION(bdp) == VPGRP_POSITION_DS) {
		dsp = BHV_TO_DSPGRP(bdp);
		tclient = dsp->dspg_tclient;
	} else {
		dcp = BHV_TO_DCPGRP(bdp);
		tclient = dcp->dcpg_tclient;
	}

        /*
         * Extract any token info from the bag.
         */
	obj_bag_get_here(bag, OBJ_TAG_TOKENS, &tokens, sizeof(tk_set_t), error);
	if (!error) {
		/*
		 * If we find the right tag, we expect that we're a client
		 * receiving the (needed) member token from the server.
		 */
		source_is_server = 1;
		if (tokens == VPGRP_MEMBER_TOKENSET)
			tkc_obtained(tclient, tokens, TK_NULLSET, 0);
	}

	pg = vpgrp_to_pgrp(vpg);

	/*
	 * Sign on to local pgrp.
	 */
	mrlock(&p->p_who, MR_UPDATE, PZERO);

	VPGRP_JOIN(vpg, p, 0);
	p->p_vpgrp = vpg;
	p->p_sid = vpg->vpg_sid;

	PGRP_LOCK(pg);
	if (p->p_flag & SPGJCL) {	/* Assumes p->p_flag correct here */
		if (++pg->pg_jclcnt == 1) {
			PGRP_UNLOCK(pg);
			VPGRP_ORPHAN(vpg, 0, 0);
			PGRP_LOCK(pg);
		}
	}
	PGRPLIST_UNLOCK(pg);		/* Release update lock */
	PGRP_UNLOCK(pg);

	mrunlock(&p->p_who);

	if (source_is_server) {
		/*
		 * This release matches the hold provided by the tkc_obtaining
		 * performed in the prepare step. This release must be
		 * done after the join so that the member token is not returned
		 * prematurely. Note that tkc_obtaining may have told us that
		 * the member token was already held and didn't need obtaining,
		 * but a hold would have been taken nonetheless.
		 */
		tkc_release1(tclient, VPGRP_MEMBER_TOKEN);
	} else {
		tk_set_t	to_return;
		tk_disp_t	why;
		int		msgerr;

		/*
		 * Either this cell or a cell other than the source is the
		 * server - either way, we release and return the 
		 * iteration inhibiting token.
		 */
		tkc_release1(tclient, VPGRP_MEMBER_RELOCATING_TOKEN);
		tkc_returning(tclient, VPGRP_MEMBER_RELOCATING_TOKEN,
				&to_return, &why, 0);
		if (TK_IS_IN_SET(VPGRP_MEMBER_RELOCATING_TOKENSET, to_return)) {
			if (dsp) {
				tks_return(dsp->dspg_tserver, cellid(),
					to_return, TK_NULLSET, TK_NULLSET, why);
			} else {
				msgerr = invk_dspgrp_return(
					DCPGRP_TO_SERVICE(dcp),
					cellid(),
					DCPGRP_TO_OBJID(dcp),
					VPGRP_MEMBER_RELOCATING_TOKENSET,
					TK_NULLSET,
					why);
				ASSERT(!msgerr);
			}
			tkc_returned(tclient, to_return, TK_NULLSET);
		}
	}

	BHV_READ_UNLOCK(bhp);
	VPGRP_RELE(vpg);
	return 0;
}

/*
 * We wave a fond farewell to our plucky proc and wish it bon voyage
 * and good fortune on the other side.
 */
int
vpgrp_proc_emigrate_end(
	vpgrp_t		*vpg,
	proc_t		*p)		/* immigrating member process */
{
	bhv_head_t	*bhp = VPGRP_BHV_HEAD_PTR(vpg);
	bhv_desc_t	*bdp = VPGRP_TO_FIRST_BHV(vpg);
	pgrp_t		*pg;

	ASSERT(BHV_IS_READ_LOCKED(bhp));

	/*
	 * Let iterations resume.
	 */
	if (BHV_POSITION(bdp) == VPGRP_POSITION_DS) {
		tkc_release1(BHV_TO_DSPGRP(bdp)->dspg_tclient,
			VPGRP_MEMBER_RELOCATING_TOKEN);
	}

	/*
	 * Leave and dereference the pgrp.
	 */
	mrlock(&p->p_who, MR_UPDATE, PZERO);
	VPGRP_LEAVE(vpg, p, 0);
	mrunlock(&p->p_who);

	/* msunlock */
	pg = vpgrp_to_pgrp(vpg);
	PGRP_LOCK(pg);
	PGRPLIST_UNLOCK(pg);
	PGRP_UNLOCK(pg);

	BHV_READ_UNLOCK(bhp);
	VPGRP_RELE(vpg);
	return 0;
}

/* ARGSUSED */
int
vpgrp_proc_emigrate_abort(
	vpgrp_t		*vpg,
	proc_t		*p)		/* emigrating member process */
{
	bhv_head_t	*bhp = VPGRP_BHV_HEAD_PTR(vpg);
	bhv_desc_t	*bdp = VPGRP_TO_FIRST_BHV(vpg);
	pgrp_t		*pg;

	ASSERT(BHV_IS_READ_LOCKED(bhp));

	/*
	 * Let iterations resume.
	 */
	if (BHV_POSITION(bdp) == VPGRP_POSITION_DS) {
		tkc_release1(BHV_TO_DSPGRP(bdp)->dspg_tclient,
			VPGRP_MEMBER_RELOCATING_TOKEN);
	}

	/* msunlock */
	pg = vpgrp_to_pgrp(vpg);
	PGRP_LOCK(pg);
	PGRPLIST_UNLOCK(pg);
	PGRP_UNLOCK(pg);

	BHV_READ_UNLOCK(bhp);
	VPGRP_RELE(vpg);
	return 0;
}

/* ARGSUSED */
int
vpgrp_proc_immigrate_abort(
	vpgrp_t		*vpg,
	proc_t		*p)		/* immigrating member process */
{
	bhv_head_t	*bhp = VPGRP_BHV_HEAD_PTR(vpg);
	pgrp_t		*pg;

	ASSERT(BHV_IS_READ_LOCKED(bhp));

	/* msunlock */
	pg = vpgrp_to_pgrp(vpg);
	PGRP_LOCK(pg);
	PGRPLIST_UNLOCK(pg);
	PGRP_UNLOCK(pg);

	/*
	 * XXX Clean-up is incomplete. We could have a member token
	 * in the obtaining state and the no-iterate token held.
	 * Both conditions are awkward to infer from available state.
	 */

	BHV_READ_UNLOCK(bhp);
	VPGRP_RELE(vpg);
	return 0;
}


void
vpgrp_client_quiesce(
	vpgrp_t	*vpg)
{
	bhv_head_t	*bhp = VPGRP_BHV_HEAD_PTR(vpg);
	BHV_WRITE_LOCK(bhp);
}

/*
 * Unquiesce local client operations-in-progress.
 */
void
vpgrp_client_unquiesce(
	vpgrp_t	*vpg)
{
	bhv_head_t	*bhp = VPGRP_BHV_HEAD_PTR(vpg);
	BHV_WRITE_UNLOCK(bhp);
}

/*
 * Mobilize source.
 * Prepare vpgrp object for relocation if the server/physical object is local.
 * This is fraught with races: the vpgrp can may be coming or going
 * when we attempting to do this.
 */
int
vpgrp_obj_mobilize(
	vpgrp_t		*vpg)
{
	int		error = 0;
	bhv_head_t	*bhp = VPGRP_BHV_HEAD_PTR(vpg);
	bhv_desc_t	*bdp;
	dspgrp_t	*dsp;

	/*
	 * Standard sequence to check that a ds is present,
	 * or to install one if possible.
	 */
	BHV_READ_LOCK(bhp);
	bdp = VPGRP_TO_FIRST_BHV(vpg);
	switch (BHV_POSITION(bdp)) {
	case VPGRP_POSITION_DC:
		/*
		 * The object isn't here.
		 */
		error = EREMOTE;
		goto out;
	case VPGRP_POSITION_PP:
		/*
		 * The object is local and non-distributed - we need
		 * to interpose a ds.
		 */
		BHV_READ_UNLOCK(bhp);
		BHV_WRITE_LOCK(bhp);
		bdp = VPGRP_TO_FIRST_BHV(vpg);
		if (BHV_OPS(bdp) == &ppgrp_ops) {
			vpgrp_interpose(vpg);
			bdp = VPGRP_TO_FIRST_BHV(vpg);
		}
		BHV_WRITE_TO_READ(bhp);
	}

	/*
	 * Another thread may get here first. Check, then update
	 * the relocation state if we won.
	 */
	dsp = BHV_TO_DSPGRP(bdp);
	error = obj_SR_source_begin(&dsp->dspg_obj_state);
	if (error == OBJ_SUCCESS) {
		/*
		 * After all that - has the vpgrp been removed?
		 * If so, there's no point in relocating it.
		 */
		if (!VPGRP_IS_VALID(vpg)) {
			/* Reset the outbound state */
			obj_SR_source_abort(&dsp->dspg_obj_state);
			error = ENOENT; 
		} else {
			PGRP_TRACE4("vpgrp_obj_mobilize", vpg->vpg_pgid,
				    "on cell", cellid());
		}
	}
out: 
	BHV_READ_UNLOCK(bhp);
	return error;
}

/*
 * Relocate vpgrp object (server) from local source cell to target cell.
 */
int
vpgrp_obj_relocate(
	vpgrp_t		*vpg,
	cell_t		target_cell)
{
	int		error;
	obj_manifest_t	*mftp;

	if (target_cell == cellid())
		return 0;		/* That's easy */
	PGRP_TRACE6("vpgrp_obj_relocate", vpg->vpg_pgid,
		    "from", cellid(), "to", target_cell);
	
	/*
	 * Call the object relocator to transfer the object.
	 */
	mftp = obj_mft_create(target_cell);
	error = obj_mft_put(mftp, (void *) vpg, SVC_PGRP, OBJ_RMODE_SERVER);
	if (error == OBJ_SUCCESS)
		error = obj_mft_ship(mftp);
	obj_mft_destroy(mftp);

	return error;
}

#define VPGRP_TO_OBJID(vpg)	((objid_t) (vpg))
#define HANDLE_TO_VPGRP(handle)	((vpgrp_t *) HANDLE_TO_OBJID(handle))

static void
vpgrp_obj_select_local(
	vpgrp_t          *vpg,
	void		*arg)
{
	service_t	svc;
	obj_list_t	obj_list = (obj_list_t) arg;
	obj_handle_t	obj_handle;
	bhv_desc_t	*bdp = VPGRP_TO_FIRST_BHV(vpg);

	if (VPGRP_IS_VALID(vpg) && BHV_POSITION(bdp) == VPGRP_POSITION_DS) {
		vpgrp_ref(vpg);			/* Provide extra ref */
		SERVICE_MAKE(svc, cellid(), SVC_PGRP);
		HANDLE_MAKE(obj_handle, svc, VPGRP_TO_OBJID(vpg));
		obj_list_put(obj_list, &obj_handle);
		PGRP_TRACE4("vpgrp_obj_select", vpg->vpg_pgid, "cell", cellid());
	}
}


/*
 * Relocated all local objects to another cell.
 * This involves scanning the vpgrp hash table to build a list of local
 * objects suitable for relocation and invoking the Common Object
 * Relocation Engine to drive the relocation.
 */
int
vpgrp_obj_evict(
	cell_t		target_cell)
{
	int		error;
	int		nobj;
	obj_list_t	local_vpgrp_list;
	obj_manifest_t	*mftp;
	obj_handle_t	obj_handle;

	if (target_cell == cellid())
		return EINVAL;		/* That's odd */

	local_vpgrp_list = obj_list_create(100);
	mftp = obj_mft_create(target_cell);

	/*
	 * Iterate over all vpgrps in the local hash table
	 * building a list of all local vpgrps (excluding client
	 * vpgrps).
	 */
	vpgrp_iterate(vpgrp_obj_select_local, (void *) local_vpgrp_list);

	/*
	 * Further filter only those objects not already
	 * undergoing relocation - in or out.
	 * Note a ds may get interposed so that we must ensure
	 * we quote the correct object id.
	 */
	obj_list_rewind(local_vpgrp_list);
	nobj = 0;
	while (obj_list_get(local_vpgrp_list, &obj_handle) == OBJ_SUCCESS) {
		error = obj_mft_put(mftp, (void *) HANDLE_TO_VPGRP(obj_handle),
				    SVC_PGRP, OBJ_RMODE_SERVER);
		if (error == OBJ_SUCCESS)
			nobj++;
	}

	if (nobj > 0)
		error = obj_mft_ship(mftp);
	else
		error = ENOENT;

	/*
	 * Dereference all selected objects, each was given a
	 * protective ref when selected for relocation.
	 */
	obj_list_rewind(local_vpgrp_list);
	while (obj_list_get(local_vpgrp_list, &obj_handle) == OBJ_SUCCESS) {
		VPGRP_RELE(HANDLE_TO_VPGRP(obj_handle));
	}

	obj_list_destroy(local_vpgrp_list);
	obj_mft_destroy(mftp);

	return error;
}
	

/*
 * The following routines implement object relocation call-outs which
 * CORE makes into the vpgrp subsystem to perform the vpgrp relocation.
 */

int
vpgrp_obj_source_prepare(
	obj_manifest_t	*mftp,		/* IN/OUT object manifest */
	void		*v)		/* IN vpgrp pointer */
{
	vpgrp_t		*vpg = (vpgrp_t *) v;
	bhv_desc_t	*bdp;
	dspgrp_t	*dsp;
	tk_set_t	granted;
	tk_set_t	refused;
	tk_set_t	held;
	vpgrp_src_mft_t	src_info;
	obj_mft_info_t	minfo;
	int		error;

	error = vpgrp_obj_mobilize(vpg);
	if (error)
		return error;
	bdp = VPGRP_TO_FIRST_BHV(vpg);
	dsp = BHV_TO_DSPGRP(bdp);

	obj_mft_info_get(mftp, &minfo);		/* Fetch target cell */

	/*
	 * Here with local, distributed object marked for OUTBOUND relocation.
	 *
	 * The target is provided with an existence token for the
	 * object if necessary since the object may not exist there.
	 */
	tks_obtain(dsp->dspg_tserver, minfo.target_cell,
		   VPGRP_EXISTENCE_TOKENSET,
		   &granted, &refused, &held);
	ASSERT((granted == TK_NULLSET && held == VPGRP_EXISTENCE_TOKENSET) ||
	       (granted == VPGRP_EXISTENCE_TOKENSET && held == TK_NULLSET));

	/*
	 * Construct the prepare info and put it into the supplied bag.
	 */
	src_info.pgid  = vpg->vpg_pgid;
	src_info.tokens = granted;
	src_info.objid = BHV_TO_OBJID(bdp);
	ppgrp_getattr(BHV_NEXT(bdp), &src_info.sid, NULL, &src_info.is_batch);
	SERVICE_MAKE(src_info.svc, cellid(), SVC_PGRP);
	HANDLE_MAKE(minfo.source.hndl, src_info.svc, BHV_TO_OBJID(bdp));
	minfo.source.tag = OBJ_TAG_SRC_PREP;
	minfo.source.infop = &src_info;
	minfo.source.info_size = sizeof(src_info);
	obj_mft_info_put(mftp, &minfo);

	PGRP_TRACE4("vpgrp_obj_source_prepare", src_info.pgid, "cell", cellid());
	return 0;
}

int
vpgrp_obj_target_prepare(
	obj_manifest_t	*mftp,			/* IN object manifest */
	void		**v)			/* OUT virtual object */
{
	vpgrp_t		*vpg;
	bhv_desc_t	*bdp;
	dcpgrp_t	*dcp;
	dspgrp_t	*dsp;
	vpgrp_src_mft_t	src_info;
	service_t	svc;
	obj_mft_info_t	minfo;

	/*
	 * Extract object info from the manifest.
	 */
	minfo.source.tag = OBJ_TAG_SRC_PREP;
	minfo.source.infop = &src_info;
	minfo.source.info_size = sizeof(src_info);
	obj_mft_info_get(mftp, &minfo);

	/*
	 * Lookup the object.
	 * If the incoming bag includes an existence token,
	 * then the object can't exist here and we must create it.
	 * Otherwise, it already exists or it is being
	 * created by another thread and will exist soon.
	 */
	if (src_info.tokens == TK_NULLSET) {
		/*
		 * Object is or will be found locally.
		 */
		int	retries = 0;
		while ((vpg = vpgrp_lookup_local(src_info.pgid)) == NULL)
			cell_backoff(++retries);
		vpgrp_client_quiesce(vpg);

	} else {
		/*
		 * Allocate the virtual object for the shared memory segment
		 * for this cell
		 */
		vpg = vpgrp_alloc(src_info.pgid, src_info.sid);

		/*
		 * Create dc and initialize token state.
		 */
		dcp = kern_malloc(sizeof(dcpgrp_t));
		ASSERT(dcp);
		HANDLE_MAKE(dcp->dcpg_handle, src_info.svc, src_info.objid);
		tkc_create("dpgrp", dcp->dcpg_tclient,
			   dcp, &dcpgrp_tclient_iface,
			   VPGRP_NTOKENS,
			   src_info.tokens, src_info.tokens,
			   (void *)(__psint_t)src_info.pgid);

		/* Initialize and insert behavior descriptor. */
		bhv_desc_init(&dcp->dcpg_bhv, dcp, vpg, &dcpgrp_ops);
		BHV_WRITE_LOCK(&vpg->vpg_bhvh);
		bhv_insert(&vpg->vpg_bhvh, &dcp->dcpg_bhv);
		BHV_WRITE_UNLOCK(&vpg->vpg_bhvh);

		ppgrp_setattr(BHV_NEXT(&dcp->dcpg_bhv), &src_info.is_batch);

		/* Write-lock the chain so that no operation can commence. */
		BHV_WRITE_LOCK(VPGRP_BHV_HEAD_PTR(vpg));

		/* Add id into hash table */
		VPGRP_HOLD(vpg);
		vpgrp_enter(vpg);
	}

	/*
	 * Create a ds behavior but don't insert into the chain yet.
	 * No token state is set up because this will be sent from the source.
	 */
	dsp = kern_malloc(sizeof(*dsp));

	tks_create("dpgrp", dsp->dspg_tserver, dsp, &dspgrp_tserver_iface,
		   VPGRP_NTOKENS, (void *)(__psint_t)vpg->vpg_pgid);

	tkc_create_local("dpgrp", dsp->dspg_tclient, dsp->dspg_tserver,
			 VPGRP_NTOKENS, TK_NULLSET,  TK_NULLSET,
			 (void *)(__psint_t)vpg->vpg_pgid);

	DSPGRP_MEM_LOCK_INIT(dsp);
	dsp->dspg_notify_idle = 0;
	OBJ_STATE_INIT(&dsp->dspg_obj_state, &vpg->vpg_bhvh);

	bhv_desc_init(&dsp->dspg_bhv, dsp, vpg, &dspgrp_ops);

	bdp = &dsp->dspg_bhv;
	obj_SR_target_prepared(&dsp->dspg_obj_state);

	/*
	 * Return the object id of the embrionic ds and
	 * we leave the behavior chain write-lock.
	 * Returned token info would also be loaded into the reply bag -
	 * though vpgrp has none..
	 */
	SERVICE_MAKE(svc, cellid(), SVC_PGRP);
	HANDLE_MAKE(minfo.target.hndl, svc, BHV_TO_OBJID(bdp));
	minfo.target.tag = OBJ_TAG_NONE;
	obj_mft_info_put(mftp, &minfo);

	*v = (void *) vpg;
	VPGRP_RELE(vpg);

	PGRP_TRACE6("vpgrp_obj_target_prepare", src_info.pgid, "cell", cellid(),
		    "embrionic_objid", BHV_TO_OBJID(bdp));
	return 0;
}

int
vpgrp_obj_source_retarget(
	obj_manifest_t	*mftp,			/* IN object manifest */
	tks_state_t	**tserverp)		/* OUT token server state */
{
	bhv_desc_t	*bdp;
	vpgrp_t		*vpg;
	dspgrp_t	*dsp;
	dcpgrp_t	*dcp;
	obj_mft_info_t	 minfo;

	/*
	 * Get source and target info from manifest.
	 */
	minfo.source.tag = OBJ_TAG_NONE;
	minfo.target.tag = OBJ_TAG_NONE;
	obj_mft_info_get(mftp, &minfo);

	bdp = OBJID_TO_BHV(HANDLE_TO_OBJID(minfo.source.hndl));
	vpg = BHV_TO_VPGRP(bdp);
	dsp = BHV_TO_DSPGRP(bdp);

	/*
	 * Need the write-lock now. But there should be no need
	 * to check that the chain is as we left it.
	 * At this point, new clients seeking an existence token
	 * will be blocked until a dcpgrp is installed and this will
	 * cause them to be redirected to the new cell.
	 */
	vpgrp_client_quiesce(vpg);

	/*
	 * If the object is being removed we'll forget the relocation.
	 */
	if (!VPGRP_IS_VALID(vpg)) {
		obj_SR_source_abort(&dsp->dspg_obj_state);
		vpgrp_client_unquiesce(vpg);
		PGRP_TRACE4("vpgrp_obj_source_retarget abort", vpg->vpg_pgid,
			    "cell", cellid());
		return ESRCH;
	}

	/*
	 * Now interpose the dc to form a transient v/dc/ds/p chain.
	 */
	dcp = (dcpgrp_t *) kern_malloc(sizeof(dcpgrp_t));
	ASSERT(dcp);
	tkc_create("dpgrp", dcp->dcpg_tclient, dcp, &dcpgrp_tclient_iface,
		   VPGRP_NTOKENS, TK_NULLSET, TK_NULLSET,
		   (void *)(__psint_t)vpg->vpg_pgid);
	dcp->dcpg_handle = minfo.target.hndl;
	bhv_desc_init(&dcp->dcpg_bhv, dcp, vpg, &dcpgrp_ops);
	bhv_insert(&vpg->vpg_bhvh, &dcp->dcpg_bhv);

	/* Copy the client state */
	tkc_copy(dsp->dspg_tclient, dcp->dcpg_tclient);

	/*
	 * Return the token server state. This is required to about
	 * the current clients from grant map of the existence token.
	 * The existence token is stable since the behavior chain
	 * is update locked and this prevents new existence tokens
	 * being granted (and existence tokens are recalled from the
	 * server and not returned spontaneously by clients).
	 */
	*tserverp = dsp->dspg_tserver;
	PGRP_TRACE6("vpgrp_obj_source_retarget", vpg->vpg_pgid,
		    "cell", cellid(), "tserver", dsp->dspg_tserver);
	return 0;
}

int
vpgrp_obj_client_retarget(
	obj_manifest_t	*mftp)			/* IN object manifest */
{
	obj_mft_info_t	minfo;
	vpgrp_src_mft_t	src_info;
	int		lookup_retry = 0;
	vpgrp_t		*vpg;
	bhv_desc_t	*bdp;
	dcpgrp_t	*dcp;

	/*
	 * Extract object info from the manifest.
	 */
	minfo.source.tag = OBJ_TAG_SRC_PREP;
	minfo.source.infop = &src_info;
	minfo.source.info_size = sizeof(src_info);
	minfo.target.tag = OBJ_TAG_NONE;
	obj_mft_info_get(mftp, &minfo);

	/*
	 * Lookup the object by id. There's a race possible if
	 * we're releasing a recently created object which has
	 * not yet been registered.
	 */
	while ((vpg = vpgrp_lookup_local(src_info.pgid)) == NULL) {
		lookup_retry++;
		cell_backoff(lookup_retry);
	}
	bdp = VPGRP_TO_FIRST_BHV(vpg);
	dcp = BHV_TO_DCPGRP(bdp);

	/*
	 * Quiesce the local client.
	 * Update the object handle to point to the new server (and objid).
	 * Unquiesce - allow OIP
	 */
	vpgrp_client_quiesce(vpg);
	dcp->dcpg_handle = minfo.target.hndl;
	vpgrp_client_unquiesce(vpg);

	VPGRP_RELE(vpg);

	PGRP_TRACE4("vpgrp_obj_client_retarget", vpg->vpg_pgid,
		    "cell", cellid());
	return 0;
}

int
vpgrp_obj_source_bag(
	obj_manifest_t	*mftp,			/* IN object manifest */
	obj_bag_t	source_bag)		/* IN/OUT object state */
{
	bhv_desc_t	*bdp;
	dspgrp_t	*dsp;
	/* REFERENCED */
	vpgrp_t		*vpg;
	obj_mft_info_t	minfo;

	/*
	 * Get source and target hanldes from manifest.
	 */
	minfo.source.tag = OBJ_TAG_NONE;
	minfo.target.tag = OBJ_TAG_NONE;
	obj_mft_info_get(mftp, &minfo);

	bdp = OBJID_TO_BHV(HANDLE_TO_OBJID(minfo.source.hndl));
	dsp = BHV_TO_DSPGRP(bdp);
	vpg = BHV_TO_VPGRP(bdp);

	/*
	 * Bag the object state.
	 *
	 * Collect the token state - that's all.
	 */
	tks_bag(dsp->dspg_tserver, source_bag);

	PGRP_TRACE4("vpgrp_obj_source_bag", vpg->vpg_pgid, "cell", cellid());
	return 0;
}

int
vpgrp_obj_target_unbag(
	obj_manifest_t	*mftp,			/* IN object manifest */
	obj_bag_t	bag)
{
	obj_mft_info_t	minfo;
	bhv_desc_t	*bdp;
	dspgrp_t	*dsp;
	vpgrp_t		*vpg;
	dcpgrp_t	*dcp;
	bhv_head_t	*bhp;
	int		error;

	minfo.source.tag = OBJ_TAG_NONE;
	minfo.target.tag = OBJ_TAG_NONE;
	obj_mft_info_get(mftp, &minfo);

	bdp = OBJID_TO_BHV(HANDLE_TO_OBJID(minfo.target.hndl));
	dsp = BHV_TO_DSPGRP(bdp);
	vpg = BHV_TO_VPGRP(bdp);
	dcp = BHV_TO_DCPGRP(VPGRP_TO_FIRST_BHV(vpg));
	bhp = VPGRP_BHV_HEAD_PTR(vpg);

	ASSERT(BHV_IS_WRITE_LOCKED(bhp));
	ASSERT(obj_SR_target_is_inbound(&dsp->dspg_obj_state));

	/*
	 * Transfer the token client state from the dc to the new ds.
	 * At this point the behavior chain is v/dc/ds/p.
	 */
	tkc_copy(dcp->dcpg_tclient, dsp->dspg_tclient);

	/*
	 * Remove and destroy the old dc and insert the new ds.
	 */
	bhv_remove(bhp, VPGRP_TO_FIRST_BHV(vpg));
	tkc_free(dcp->dcpg_tclient);
	kern_free(dcp);
	error = bhv_insert(bhp, &dsp->dspg_bhv);
	ASSERT(!error);

	/*
	 * Transfer the server token state from bag.
	 */
	error = tks_unbag(dsp->dspg_tserver, bag);
	if (error)
		return error;

	/*
	 * Request callout when no member remains.
	 */
	tks_notify_idle(dsp->dspg_tserver, VPGRP_MEMBER_TOKENSET);


	/*
	 * There's no pgrp state since pgrp's are cell-local.
	 */
	vpgrp_client_unquiesce(vpg);
	obj_SR_target_end(&dsp->dspg_obj_state);
	PGRP_TRACE4("vpgrp_obj_target_unbag", vpg->vpg_pgid, "cell", cellid());

	return 0;
}

int
vpgrp_obj_source_end(
	obj_manifest_t	*mftp)		/* IN object manifest */
{
	bhv_desc_t	*bdp;
	dspgrp_t	*dsp;
	vpgrp_t		*vpg;
	bhv_head_t	*bhp;
	obj_mft_info_t	minfo;

	minfo.source.tag = OBJ_TAG_NONE;
	minfo.target.tag = OBJ_TAG_NONE;
	obj_mft_info_get(mftp, &minfo);

	bdp = OBJID_TO_BHV(HANDLE_TO_OBJID(minfo.source.hndl));
	dsp = BHV_TO_DSPGRP(bdp);
	vpg = BHV_TO_VPGRP(bdp);
	bhp = VPGRP_BHV_HEAD_PTR(vpg);

	/*
	 * Teardown.
	 *
	 * All we need do is remove and tear down
	 * the old dspgrp after marking the end of token
	 * migration and destroying the token server state.
	 */
	bhv_remove(bhp, bdp);
	obj_SR_source_end(&dsp->dspg_obj_state);
	OBJ_STATE_DESTROY(&dsp->dspg_obj_state);
	DSPGRP_MEM_LOCK_DESTROY(dsp);
	tks_free(dsp->dspg_tserver);
	tkc_free(dsp->dspg_tclient);
	kern_free(dsp);

	vpgrp_client_unquiesce(vpg);

	PGRP_TRACE6("vpgrp_obj_source_end", vpg->vpg_pgid,
		    "from", cellid(), "to", minfo.target_cell);
	return 0;
}

int
vpgrp_obj_source_abort(
	obj_manifest_t	*mftp)		/* IN object manifest */
{
	bhv_desc_t	*bdp;
	/* REFERENCED */
	vpgrp_t		*vpg;
	obj_mft_info_t	minfo;

	minfo.source.tag = OBJ_TAG_NONE;
	minfo.target.tag = OBJ_TAG_NONE;
	obj_mft_info_get(mftp, &minfo);

	bdp = OBJID_TO_BHV(HANDLE_TO_OBJID(minfo.source.hndl));
	vpg = BHV_TO_VPGRP(bdp);

	/* Nothing to do. */

	PGRP_TRACE2("vpgrp_obj_source_abort", vpg->vpg_pgid);
	return 0;
}

int
vpgrp_obj_target_abort(
	obj_manifest_t	*mftp)		/* IN object manifest */
{
	bhv_desc_t	*bdp;
	dspgrp_t	*dsp;
	vpgrp_t		*vpg;
	bhv_head_t	*bhp;
	obj_mft_info_t	minfo;

	minfo.source.tag = OBJ_TAG_NONE;
	minfo.target.tag = OBJ_TAG_NONE;
	obj_mft_info_get(mftp, &minfo);

	bdp = OBJID_TO_BHV(HANDLE_TO_OBJID(minfo.target.hndl));
	dsp = BHV_TO_DSPGRP(bdp);
	vpg = BHV_TO_VPGRP(bdp);
	bhp = VPGRP_BHV_HEAD_PTR(vpg);

	ASSERT(BHV_IS_WRITE_LOCKED(bhp));
	ASSERT(obj_SR_target_is_inbound(&dsp->dspg_obj_state));

	/* Out with ds */
	bhv_remove(bhp, bdp);
	OBJ_STATE_DESTROY(&dsp->dspg_obj_state);
	DSPGRP_MEM_LOCK_DESTROY(dsp);
	tks_free(dsp->dspg_tserver);
	tkc_free(dsp->dspg_tclient);

	vpgrp_client_unquiesce(vpg);

	PGRP_TRACE2("vpgrp_obj_target_abort", vpg->vpg_pgid);
	return 0;
}

int
vpgrp_svr_lookup(
	void		*id,
	service_t	*svc)
{
	vpgrp_t		*vpg;
	bhv_desc_t	*bdp;
	bhv_head_t	*bhp;
	dcpgrp_t	*dcp;
	__psint_t	pgid = (__psint_t) id;

	vpg = VPGRP_LOOKUP((pid_t) pgid);
	if (!vpg)
		return ESRCH;

	bhp = VPGRP_BHV_HEAD_PTR(vpg);
	BHV_READ_LOCK(bhp);
	bdp = VPGRP_TO_FIRST_BHV(vpg);
	switch (BHV_POSITION(bdp)) {
	case VPGRP_POSITION_PP:
	case VPGRP_POSITION_DS:
		/*
		 * Here be the server.
		 */
		SERVICE_MAKE(*svc, cellid(), SVC_PGRP);
		break;
	case VPGRP_POSITION_DC:
		dcp = BHV_TO_DCPGRP(bdp);
		*svc = HANDLE_TO_SERVICE(dcp->dcpg_handle);
		break;
	}
	
	BHV_READ_UNLOCK(bhp);
	VPGRP_RELE(vpg);
	return 0;
}

int
vpgrp_svr_evict(
	void		*id,
	cell_t		target)
{
	vpgrp_t		*vpg;
	int		error;
	__psint_t	pgid = (__psint_t) id;

	if (pgid != 0) {
		vpg = VPGRP_LOOKUP((pid_t) pgid);
		if (!vpg)
			return ESRCH;
		error = vpgrp_obj_relocate(vpg, target);
		VPGRP_RELE(vpg);
	} else {
		error = vpgrp_obj_evict(target);
	}
	return error;
}

obj_relocation_if_t vpgrp_obj_iface = {
	vpgrp_obj_source_prepare,
	vpgrp_obj_target_prepare,
	vpgrp_obj_source_retarget,
	vpgrp_obj_client_retarget,
	vpgrp_obj_source_bag,
	vpgrp_obj_target_unbag,
	vpgrp_obj_source_end,
	vpgrp_obj_source_abort,
	vpgrp_obj_target_abort
};

obj_service_if_t vpgrp_obj_svr_iface = {
	vpgrp_svr_lookup,
	vpgrp_svr_evict,
	NULL,
};

void
vpgrp_obj_init(void)
{
	service_t	svc;

	/* 
	 * Register our callout vector with the central object management.
	 */
	SERVICE_MAKE(svc, cellid(), SVC_PGRP);
	obj_service_register(svc, &vpgrp_obj_svr_iface, &vpgrp_obj_iface); 
}
