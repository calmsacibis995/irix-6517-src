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
#ident "$Id: dcshm.c,v 1.37 1998/09/14 16:14:27 jh Exp $"

/*
 * client-side object management for the System V shared memory subsystem
 */

#include <sys/types.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/ipc.h>
#include <sys/kmem.h>
#include <sys/sema.h>
#include <sys/shm.h>
#include <sys/systm.h>
#include <sys/idbgentry.h>

#include <ksys/cell/tkm.h>
#include <ksys/vshm.h>
#include <ksys/cred.h>
#include <ksys/cell/wp.h>

#include "dshm.h"
#include "vshm_migrate.h"
#include "invk_dsshm_stubs.h"
#include "I_dcshm_stubs.h"
#include "vshm_mgr.h"
#include "vshm_private.h"

static void	dcshm_getmo(bhv_desc_t *, as_mohandle_t *);
static int	dcshm_keycheck(bhv_desc_t *, struct cred *, int, size_t);
static int	dcshm_attach(bhv_desc_t *, int, asid_t, pid_t,
			struct cred *, caddr_t, caddr_t *);
static void	dcshm_setdtime(bhv_desc_t *, int, pid_t);
static int	dcshm_rmid(bhv_desc_t *, struct cred *, int);
static int	dcshm_ipcset(bhv_desc_t *, struct ipc_perm *, struct cred *);
static int	dcshm_getstat(bhv_desc_t *, struct cred *,
			struct shmid_ds *, cell_t *);
static int	dcshm_mac_access(bhv_desc_t *, struct cred *);
static void	dcshm_destroy(bhv_desc_t *);

shmops_t dcshm_ops = {
	BHV_IDENTITY_INIT_POSITION(VSHM_POSITION_DC),
	dcshm_getmo,
	dcshm_keycheck,
	dcshm_attach,
	dcshm_setdtime,
	dcshm_rmid,
	dcshm_ipcset,
	dcshm_getstat,
	dcshm_mac_access,
	dcshm_destroy,
};

static void dcshm_tcif_obtain(void *, tk_set_t, tk_set_t, tk_disp_t,
				tk_set_t *);
static void dcshm_tcif_return(tkc_state_t, void *, tk_set_t, tk_set_t,
				tk_disp_t);
/* Made extern for vshm migration */
extern tkc_ifstate_t dcshm_tclient_iface = {
		dcshm_tcif_obtain,
		dcshm_tcif_return
};
extern int vshm_count;

int
vshm_getstatus_remote(
	service_t	service,
	struct cred	*cred,
	struct shmstat	*stat)
{
	int	error;
	/* REFERENCED */
	int	msgerr;

	msgerr = invk_dsshm_getstatus(service, &error, stat->sh_id,
			     stat->sh_location, CRED_GETID(cred), stat);
	ASSERT(!msgerr);
	return(error);
}

/* ARGSUSED */
static int
dcshm_alloc(
	key_t		*key,
	int		id,
	service_t	svc,
	dcshm_t 	**dcpp)
{
	dcshm_t	*dcp;
	tk_set_t already, refused;
	/* REFERENCED */
	tk_set_t granted;
	objid_t objid;
	/* REFERENCED */
	int error;
	/* REFERENCED */
	cell_t	relocated_to;
	/* REFERENCED */
	int	msgerr;

	/*
	 * try for EXISTENCE token from serv
	 * Call remote cell, looping if necessary to cope with redirection
	 * to a new cell if the pshm has migrated from its origin - strictly
	 * its name service manager.
	 */
	for (;;) {
		msgerr = invk_dsshm_obtain(svc, &error,
				  cellid(), id, VSHM_EXISTENCE_TOKENSET,
				  TK_NULLSET, TK_NULLSET,
				  &already, &granted, &refused,
				  &objid, key, &relocated_to);
		ASSERT(!msgerr);
		if (error != EMIGRATED)
			break;
		if (relocated_to == cellid())
			return EAGAIN;
		SERVICE_TO_CELL(svc) = relocated_to;
	}
	ASSERT(error == 0 || refused == VSHM_EXISTENCE_TOKENSET);
	ASSERT(error != 0 || (granted|already|refused) == VSHM_EXISTENCE_TOKENSET);

	if (refused != TK_NULLSET) {
		/* server didn't know about id */
		return EINVAL;
	} else if (already != TK_NULLSET) {
		/* server already gave some other thread token */
		return EAGAIN;
	}

	dcp = kern_malloc(sizeof(dcshm_t));
	ASSERT(dcp);

	HANDLE_MAKE(dcp->dcsm_handle, svc, objid);
	tkc_create("dshm", dcp->dcsm_tclient, dcp, &dcshm_tclient_iface,
			VSHM_NTOKENS, granted, granted,
			(void *)(__psint_t)id);

	*dcpp = dcp;
	return 0;
}

/*
 * create a distributed client for 'id'. This function can return an error
 * if the server either:
 * 1) doesn't know about 'id'
 * 2) thinks this client already knows about 'id'
 */
int
dcshm_create(int id, service_t svc, vshm_t **vshmp)
{
	vshm_t		*vshm;
	dcshm_t		*dcp;
	int		error;
	key_t		key;

	/* this should only be called for id's that we don't control */
	ASSERT(vshmid_to_server_cell(id) != cellid());

	/*
	 * Create the distributed object
	 * This fills in the key from the server
	 */
	error = dcshm_alloc(&key, id, svc, &dcp);
	if (error)
		return(error);

	/*
	 * Allocate the virtual object for the shared memory segment
	 * for this cell
	 */
	vshm = kmem_zone_alloc(vshm_zone, KM_SLEEP);
	ADDVSHM();
	vshm->vsm_id = id;
	vshm->vsm_key = key;
	vshm->vsm_refcnt = 1;
	bhv_head_init(&vshm->vsm_bh, "vshm");

	/* Initialize and insert behavior descriptor. */
	bhv_desc_init(&dcp->dcsm_bd, dcp, vshm, &dcshm_ops);
	bhv_insert_initial(&vshm->vsm_bh, &dcp->dcsm_bd);

	/*
	 * return success and a pointer to the new virtual shared memory
	 * object
	 */
	*vshmp = vshm;
	return(0);
}

/*
 * Client side of access to a remote object
 */
static void
dcshm_getmo(bhv_desc_t *bdp, as_mohandle_t *mop)
{
	/* REFERENCED */
	int		msgerr;
	dcshm_t		*dcp = BHV_TO_DCSHM(bdp);

	msgerr = invk_dsshm_getmo(DCSHM_TO_SERVICE(dcp), DCSHM_TO_OBJID(dcp),
			     mop);
	ASSERT(!msgerr);
}

static int
dcshm_keycheck(bhv_desc_t *bdp, struct cred *cred, int flag, size_t size)
{
	dcshm_t		*dcp = BHV_TO_DCSHM(bdp);
	int		error;
	/* REFERENCED */
	int		msgerr;

	msgerr = invk_dsshm_keycheck(DCSHM_TO_SERVICE(dcp), &error,
			    DCSHM_TO_OBJID(dcp), CRED_GETID(cred),
			    flag, size);
	ASSERT(!msgerr);
	return error;
}

static int
dcshm_attach(bhv_desc_t *bdp,
	int flag,
	asid_t asid,
	pid_t pid,
	struct cred *cred,
	caddr_t addr,
	caddr_t *aaddr)
{
	dcshm_t		*dcp = BHV_TO_DCSHM(bdp);
	int		error;
	/* REFERENCED */
	int		msgerr;

	msgerr = invk_dsshm_attach(DCSHM_TO_SERVICE(dcp), &error,
			  DCSHM_TO_OBJID(dcp),
			  flag, &asid, pid, CRED_GETID(cred), addr, aaddr);
	ASSERT(!msgerr);
	return error;
}

static void
dcshm_setdtime(bhv_desc_t *bdp, int dtime, pid_t pid)
{
	/* REFERENCED */
	int		msgerr;
	dcshm_t		*dcp = BHV_TO_DCSHM(bdp);

	msgerr = invk_dsshm_setdtime(DCSHM_TO_SERVICE(dcp), DCSHM_TO_OBJID(dcp),
			    dtime, pid);
	ASSERT(!msgerr);
}

static int
dcshm_rmid(bhv_desc_t *bdp, struct cred *cred, int flags)
{
	dcshm_t		*dcp = BHV_TO_DCSHM(bdp);
	int		error;
	/* REFERENCED */
	int		msgerr;

	msgerr = invk_dsshm_rmid(DCSHM_TO_SERVICE(dcp), &error,
			DCSHM_TO_OBJID(dcp), CRED_GETID(cred), flags);
	ASSERT(!msgerr);
	return error;
}

static int
dcshm_ipcset(bhv_desc_t *bdp, struct ipc_perm *perm, struct cred *cred)
{
	dcshm_t		*dcp = BHV_TO_DCSHM(bdp);
	int		error;
	/* REFERENCED */
	int		msgerr;

	msgerr = invk_dsshm_ipcset(DCSHM_TO_SERVICE(dcp), &error,
			  DCSHM_TO_OBJID(dcp), perm, CRED_GETID(cred));
	ASSERT(!msgerr);
	return error ;
}

static int
dcshm_getstat(
	bhv_desc_t	*bdp,
	struct cred	*cred,
	struct shmid_ds	*shmds,
	cell_t		*cell)
{
	dcshm_t		*dcp = BHV_TO_DCSHM(bdp);
	int		error;
	/* REFERENCED */
	int		msgerr;

	msgerr = invk_dsshm_getstat(DCSHM_TO_SERVICE(dcp), &error,
			   DCSHM_TO_OBJID(dcp), CRED_GETID(cred),
			   shmds, cell);
	ASSERT(!msgerr);
	return error;
}

static int
dcshm_mac_access(bhv_desc_t *bdp, struct cred *cred)
{
	dcshm_t		*dcp = BHV_TO_DCSHM(bdp);
	int		error;
	/* REFERENCED */
	int		msgerr;

	msgerr = invk_dsshm_mac_access(DCSHM_TO_SERVICE(dcp), &error,
			      DCSHM_TO_OBJID(dcp), CRED_GETID(cred));
	ASSERT(!msgerr);
	return error;
}

static void
dcshm_destroy(bhv_desc_t *bdp)
{
	dcshm_t *dcp = BHV_TO_DCSHM(bdp);
	vshm_t *vshm = BHV_TO_VSHM(bdp);
	/*
	 * We can only be here if a token revoke came in
	 * At this point a revoke is pending for the EXISTENCE
	 * token so when we drop the hold the callout will occur
	 * immediately. Once we return then we know the token
	 * module is done.
	 */
	tkc_release1(dcp->dcsm_tclient, VSHM_EXISTENCE_TOKEN);
	tkc_destroy(dcp->dcsm_tclient);
	bhv_remove(&vshm->vsm_bh, &dcp->dcsm_bd);
	kern_free(dcp);

	bhv_head_destroy(&vshm->vsm_bh);
	kmem_zone_free(vshm_zone, vshm);
	DELETEVSHM();
}

/*
 * client token callout to obtain a token.
 * Note that this is never called on the server since that will
 * always go through the local_client interface
 */
static void
dcshm_tcif_obtain(
	void		*obj,
	tk_set_t	to_be_obtained,
	tk_set_t	to_be_returned,
	tk_disp_t	dofret,
	tk_set_t	*refused)
{
	vshm_t		*vshm;
	dcshm_t		*dcp = (dcshm_t *)obj;
	/* REFERENCED */
	tk_set_t	granted, already;
	cell_t		mycell = cellid();
	cell_t		relocated_to;
	int		error;
	/* REFERENCED */
	int		msgerr;

	vshm = BHV_TO_VSHM(&dcp->dcsm_bd);

	VSHM_TRACE6("dcshm_tcif_obtain", vshm->vsm_id, "vshm", vshm,
		    "tokens", to_be_obtained);

	/*
	 * Call remote cell, redirection to a new cell is not expected
	 * since the object handle is guaranteed at this point.
	 */
	msgerr = invk_dsshm_obtain(dcp->dcsm_handle.h_service,
			  &error,
			  mycell, vshm->vsm_id, to_be_obtained,
			  to_be_returned, dofret,
			  &already, &granted, refused,
			  &dcp->dcsm_handle.h_objid,
			  &vshm->vsm_key,
			  &relocated_to);
	ASSERT(!msgerr);
	ASSERT(already == TK_NULLSET);
	ASSERT(error == 0 || *refused == to_be_obtained);
	ASSERT(error != 0 || granted == to_be_obtained);
}

/*
 * token client return callout
 */
/* ARGSUSED */
static void
dcshm_tcif_return(
	tkc_state_t	tclient,
	void		*obj,
	tk_set_t	to_be_returned,
	tk_set_t	unknown,
	tk_disp_t	why)
{
	/* REFERENCED */
	int		msgerr;
	dcshm_t		*dcp = (dcshm_t *)obj;

	ASSERT(tclient == dcp->dcsm_tclient);

	ASSERT(TK_IS_IN_SET(VSHM_EXISTENCE_TOKENSET, to_be_returned));

	msgerr = invk_dsshm_return(DCSHM_TO_SERVICE(dcp),
			  cellid(), 
			  DCSHM_TO_OBJID(dcp),
			  to_be_returned, unknown, why);
	ASSERT(!msgerr);
	tkc_returned(tclient, to_be_returned, TK_NULLSET);
}

/*
 * server initiated message to client to return a token - client side
 */
void
I_dcshm_recall(int *error,
	int id,
	tk_set_t to_be_revoked,
	tk_disp_t why)
{
	int err;
	auto vshm_t *vshm;
	bhv_desc_t *bdp;

	/*
	 * it's possible that we are getting a revoke for something
	 * we haven't yet got in our tables - this can happen if the
	 * revoke and obtain pass in the night
	 */
	err = vshm_lookup_id_local(id, &vshm);
	if (err == ENOENT) {
		/* tell server to -Go Fish- */
		*error = err;
		return;
	}

	ASSERT(err == 0);
	bdp = VSHM_TO_FIRST_BHV(vshm);
	tkc_recall(BHV_TO_DCSHM(bdp)->dcsm_tclient, to_be_revoked, why);
	if (TK_IS_IN_SET(VSHM_EXISTENCE_TOKENSET, to_be_revoked)) {
		/*
		 * remove vshm from lookup tables.
		 */
		vshm_remove(vshm, 0);
	}
	/* drop reference that lookup_id_local gave us */
	vshm_rele(vshm);
	*error = 0;
	return;
}

#if defined(DEBUG) || defined(SHMDEBUG)
void
idbg_dcshm_print(
	dcshm_t	*dcp)
{
	qprintf("dcshm 0x%x:\n", dcp);
	qprintf("    token state - client 0x%x\n", dcp->dcsm_tclient);
	qprintf("    handle (0x%x, 0x%x)\n",
		HANDLE_TO_SERVICE(dcp->dcsm_handle),
		HANDLE_TO_OBJID(dcp->dcsm_handle));
	qprintf("    bhv 0x%x\n", &dcp->dcsm_bd);
}
#endif
