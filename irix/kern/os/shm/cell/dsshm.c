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
#ident "$Id: dsshm.c,v 1.38 1998/09/14 16:14:27 jh Exp $"

/*
 * Server side for the System V shared memory subsystem
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

#include "dshm.h"
#include "vshm_migrate.h"
#include "I_dsshm_stubs.h"
#include "invk_dsshm_stubs.h"
#include "invk_dcshm_stubs.h"
#include "vshm_mgr.h"
#include "vshm_private.h"

static void	dsshm_getmo(bhv_desc_t *, as_mohandle_t *);
static int	dsshm_keycheck(bhv_desc_t *, struct cred *, int, size_t);
static int	dsshm_attach(bhv_desc_t *, int, asid_t, pid_t,
				struct cred *, caddr_t, caddr_t *);
static void	dsshm_setdtime(bhv_desc_t *, int, pid_t);
static int	dsshm_rmid(bhv_desc_t *, struct cred *, int);
static int	dsshm_ipcset(bhv_desc_t *, struct ipc_perm *, struct cred *);
static int	dsshm_getstat(bhv_desc_t *, struct cred *,
				struct shmid_ds *, cell_t *);
static int	dsshm_mac_access(bhv_desc_t *, struct cred *);
static void	dsshm_destroy(bhv_desc_t *);

shmops_t dsshm_ops = {
	BHV_IDENTITY_INIT_POSITION(VSHM_POSITION_DS),
	dsshm_getmo,
	dsshm_keycheck,
	dsshm_attach,
	dsshm_setdtime,
	dsshm_rmid,
	dsshm_ipcset,
	dsshm_getstat,
	dsshm_mac_access,
	dsshm_destroy,
};

static void dsshm_tsif_recall(void *, tks_ch_t, tk_set_t, tk_disp_t);
static void dsshm_tsif_recalled(void *, tk_set_t, tk_set_t);

static tks_ifstate_t dsshm_tserver_iface = {
		dsshm_tsif_recall,
		dsshm_tsif_recalled,
		NULL,
};
extern int vshm_count;

/*
 * a remote client has requested access to a previously local-only vshm
 * set up and interpose the DS layer.
 */
void
vshm_interpose(vshm_t *vshm)
{
	dsshm_t		*dsp;
	tk_set_t	granted;
	tk_set_t	already_obtained;
	tk_set_t	refused;
	tk_set_t	wanted;
	/* REFERENCED */
	int error;

	VSHM_TRACE4("vshm_interpose", vshm->vsm_id, "vshm", vshm);
	dsp = kern_malloc(sizeof(*dsp));

	tks_create("dshm", dsp->dssm_tserver, dsp, &dsshm_tserver_iface,
		   VSHM_NTOKENS, (void *)(__psint_t)vshm->vsm_id);

	wanted = VSHM_EXISTENCE_TOKENSET;
	tks_obtain(dsp->dssm_tserver, (tks_ch_t)cellid(),
		   wanted, &granted, &refused,
		   &already_obtained);
	ASSERT(granted == wanted);
	ASSERT(already_obtained == TK_NULLSET);
	ASSERT(refused == TK_NULLSET);
	tkc_create_local("dshm", dsp->dssm_tclient, dsp->dssm_tserver,
		   VSHM_NTOKENS, granted,
		   granted, (void *)(__psint_t)vshm->vsm_id);
	bhv_desc_init(&dsp->dssm_bd, dsp, vshm, &dsshm_ops);
	error = bhv_insert(&vshm->vsm_bh, &dsp->dssm_bd);
	OBJ_STATE_INIT(&dsp->dssm_obj_state, &vshm->vsm_bh);
	ASSERT(!error);
}

/*
 * Server-side stubs (beginning with dshm_*)
 * Most just call through the dsshm layer, which are mostly inlined routines
 * that just call the pshm layer.
 */

void
vshm_freeid_remote(service_t svc, int id)
{
	int	msgerr;

	msgerr = invk_dsshm_freeid(svc, id);
	ASSERT(!msgerr);
}

void
I_dsshm_freeid(int id)
{
	vshm_freeid_local(id);
}

/*
 * client request to server to obtain tokens.
 * This means we potentially need to change from a local vshm to
 * a distributed vshm
 */
void
I_dsshm_obtain(
	int		*error,
	cell_t		sender,
	int		vshmid,
	tk_set_t	to_be_obtained,
	tk_set_t	to_be_returned,
	tk_disp_t	dofret,
	tk_set_t	*already_obtained,
	tk_set_t	*granted,
	tk_set_t	*refused,
	objid_t		*objid,
	int		*key,
	cell_t		*redirect)
{
	vshm_t		*vshm;
	dsshm_t		*dsp;
	bhv_desc_t	*bdp;
	int		err;

	ASSERT(to_be_returned == TK_NULLSET);
	*error = 0;

	VSHM_TRACE8("dsshm_tserver_obtain", vshmid, "from", sender,
		    "obtain", to_be_obtained, "*objid", *objid);

	/*
	 * if we're going after the EXISTENCE token then we use the
	 * vshmid as a handle and lookup to see if we have this vshm
	 * If however we aren't looking for th EXISTENCE token then
	 * we use objid as our handle. This lets us (the server) stop
	 * responding to new lookups but still honor other requests
	 * (like locking).
	 */
	if (!TK_IS_IN_SET(VSHM_EXISTENCE_TOKENSET, to_be_obtained)) {
		/* this is simple */
		bhv_desc_t *bdp = OBJID_TO_BHV(*objid);
		dsp = BHV_TO_DSSHM(bdp);

		DSSHM_RETARGET_CHECK(dsp);

		if (to_be_returned != TK_NULLSET)
			tks_return(dsp->dssm_tserver, sender, to_be_returned, 
				   TK_NULLSET, TK_NULLSET, dofret);

		tks_obtain(dsp->dssm_tserver, sender, to_be_obtained, granted,
			   refused, already_obtained);
		return;
	}

	if ((err = vshm_lookup_id_local(vshmid, &vshm))) {
		*refused = to_be_obtained;
		*granted = TK_NULLSET;
		*already_obtained = TK_NULLSET;
		*error = err;
		return;
	}
	/* lookup_id_local gives us a ref */

	/*
	 * Need the read lock at this stage to be assured that
	 * no migration is in progress.
	 */
	BHV_READ_LOCK(VSHM_BHV_HEAD_PTR(vshm));
	bdp = VSHM_TO_FIRST_BHV(vshm);

	/*
	 * need to lock out other threads that might also be
	 * in here - only one can transition us from local to distributed
	 */
	if (BHV_OPS(bdp) == &pshm_ops) {
		/*
		 * We need to add a distributed layer
		 */
		BHV_READ_UNLOCK(VSHM_BHV_HEAD_PTR(vshm));
		BHV_WRITE_LOCK(VSHM_BHV_HEAD_PTR(vshm));
		/*
		 * re-set bdp to 'top' level (since another thread might
		 * have beat us to the lock and already done the interposition
		 * (and conceivably even migrated the thing elsewhere)
		 */
		bdp = VSHM_TO_FIRST_BHV(vshm);
		if (BHV_OPS(bdp) == &pshm_ops)
			vshm_interpose(vshm);
		BHV_WRITE_TO_READ(VSHM_BHV_HEAD_PTR(vshm));
		/* re-set bdp to 'top' level */
		bdp = VSHM_TO_FIRST_BHV(vshm);
		VSHM_TRACE6("dsshm_tserver_obtain after interpose",
			    vshm->vsm_id, "vshm", vshm, "bdp", bdp);
	}

	/*
	 * Could reach this point but find that the dsshm/pshm has migrated
	 * away from its origin cell (here). But as origin (or surrogate
	 * if the origin cell went down) we know where the pshm is. Our
	 * dcshm is guaranteed - so return redirection to new service (i.e.
	 * cell).
	 */
	if (BHV_OPS(bdp) == &dcshm_ops) {
		dcshm_t	*dcp = BHV_TO_DCSHM(bdp);
		*redirect = SERVICE_TO_CELL(dcp->dcsm_handle.h_service);
		BHV_READ_UNLOCK(VSHM_BHV_HEAD_PTR(vshm));
		vshm_rele(vshm);
		*error = EMIGRATED;
		return;
	}

	ASSERT(BHV_OPS(bdp) == &dsshm_ops);
	dsp = BHV_TO_DSSHM(bdp);

	/*
	 * since we only have 2 levels max - dsshm is always the head
	 * and we want to return the pointer to the behavior descriptor 
	 * that represents the dsshm layer.
	 */
	*objid = bdp;
	*key = vshm->vsm_key;

	if (to_be_returned != TK_NULLSET)
		tks_return(dsp->dssm_tserver, sender, to_be_returned, 
			   TK_NULLSET, TK_NULLSET, dofret);

	tks_obtain(dsp->dssm_tserver, sender,
		   to_be_obtained, granted, refused, already_obtained);

	/*
	 * can only release once we have an existence token - otherwise
	 * the id could go away
	 */
	BHV_READ_UNLOCK(VSHM_BHV_HEAD_PTR(vshm));
	vshm_rele(vshm);

	return;
}

/*
 * server side of client returning tokens
 */
void
I_dsshm_return(
	cell_t		sender,
	objid_t		objid,
	tk_set_t	to_be_returned,
	tk_set_t	unknown,
	tk_disp_t	why)
{
	dsshm_t		*dsp = OBJID_TO_DSSHM(objid);

	tks_return(dsp->dssm_tserver, sender, to_be_returned,
			TK_NULLSET, unknown, why);
}

/*
 * server side of getstatus_remote
 */
void
I_dsshm_getstatus(int *error,
	int id,
	uint64_t loc,
	credid_t credid,
	struct shmstat *stat)
{
	cred_t	*credp;

	/* DSSHM_RETARGET_CHECK() not required because lookup done */
	stat->sh_id = id;
	stat->sh_location = loc;
	credp = CREDID_GETCRED(credid);
	*error = vshm_getstatus_local(stat, credp);
	if (credp)
		crfree(credp);
}

void
I_dsshm_getmo(objid_t objid, as_mohandle_t *mop)
{
	DSSHM_RETARGET_CHECK(OBJID_TO_DSSHM(objid));
	pshm_getmo(BHV_NEXT(OBJID_TO_BHV(objid)), mop);
}

void
I_dsshm_keycheck(
	int *error,
	objid_t objid,
	credid_t credid,
	int flag,
	size_t size)
{
	cred_t	*credp;
	DSSHM_RETARGET_CHECK(OBJID_TO_DSSHM(objid));
	credp = CREDID_GETCRED(credid);
	*error = pshm_keycheck(BHV_NEXT(OBJID_TO_BHV(objid)),
			       credp, flag, size);
	if (credp)
		crfree(credp);
}

void
I_dsshm_attach(
	int *error,
	objid_t objid,
	int flag,
	asid_t *asid,
	pid_t pid,
	credid_t credid,
	caddr_t addr,
	caddr_t *aaddr)
{
	cred_t	*credp;
	DSSHM_RETARGET_CHECK(OBJID_TO_DSSHM(objid));
	credp = CREDID_GETCRED(credid);
	*error = pshm_attach(BHV_NEXT(OBJID_TO_BHV(objid)),
					flag, *asid, pid, credp, addr, aaddr);
	if (credp)
		crfree(credp);
}

void
I_dsshm_setdtime(objid_t objid, int dtime, pid_t pid)
{
	DSSHM_RETARGET_CHECK(OBJID_TO_DSSHM(objid));
	pshm_setdtime(BHV_NEXT(OBJID_TO_BHV(objid)), dtime, pid);
}

void
I_dsshm_rmid(int *error, objid_t objid, credid_t credid, int flags)
{
	cred_t	*credp;
	DSSHM_RETARGET_CHECK(OBJID_TO_DSSHM(objid));
	credp = CREDID_GETCRED(credid);
	*error = pshm_rmid(BHV_NEXT(OBJID_TO_BHV(objid)), credp, flags);
	if (credp)
		crfree(credp);
}

void
I_dsshm_ipcset(
	int *error,
	objid_t objid,
	struct ipc_perm *perm,
	credid_t credid)
{
	cred_t	*credp;
	DSSHM_RETARGET_CHECK(OBJID_TO_DSSHM(objid));
	credp = CREDID_GETCRED(credid);
	*error = pshm_ipcset(BHV_NEXT(OBJID_TO_BHV(objid)), perm, credp);
	if (credp)
		crfree(credp);
}

void
I_dsshm_getstat(
	int		*error,
	objid_t		objid,
	credid_t	credid,
	struct shmid_ds	*shmds,
	cell_t		*cell)
{
	cred_t	*credp;
	DSSHM_RETARGET_CHECK(OBJID_TO_DSSHM(objid));
	credp = CREDID_GETCRED(credid);
	*error = pshm_getstat(BHV_NEXT(OBJID_TO_BHV(objid)),
			      credp, shmds, cell);
	if (credp)
		crfree(credp);
}

void
I_dsshm_mac_access(int *error, objid_t objid, credid_t credid)
{
	cred_t	*credp;
	DSSHM_RETARGET_CHECK(OBJID_TO_DSSHM(objid));
	credp = CREDID_GETCRED(credid);
	*error = pshm_mac_access(BHV_NEXT(OBJID_TO_BHV(objid)), credp);
	if (credp)
		crfree(credp);
}

/*
 * server side interposition layer.
 * Once there is a remote access to a shm id all client-at-server and
 * some client
 * access goes through the dsshm_ops
 */
static void
dsshm_getmo(bhv_desc_t *bdp, as_mohandle_t *mop)
{
	pshm_getmo(BHV_NEXT(bdp), mop);
}

static int
dsshm_keycheck(bhv_desc_t *bdp, struct cred *cred, int flag, size_t size)
{
	return pshm_keycheck(BHV_NEXT(bdp), cred, flag, size);
}

static int
dsshm_attach(bhv_desc_t *bdp,
	int flag,
	asid_t asid,
	pid_t pid,
	struct cred *cred,
	caddr_t addr,
	caddr_t *aaddr)
{
	return pshm_attach(BHV_NEXT(bdp), flag, asid, pid, cred, addr, aaddr);
}

static void
dsshm_setdtime(bhv_desc_t *bdp, int dtime, pid_t pid)
{
	pshm_setdtime(BHV_NEXT(bdp), dtime, pid);
}

/*
 * we are called with a reference but unlocked
 */
static int
dsshm_rmid(bhv_desc_t *bdp, struct cred *cred, int flags)
{
	return pshm_rmid(BHV_NEXT(bdp), cred, flags);
}

static int
dsshm_ipcset(bhv_desc_t *bdp, struct ipc_perm *perm, struct cred *cred)
{
	return pshm_ipcset(BHV_NEXT(bdp), perm, cred);
}

static int
dsshm_getstat(
	bhv_desc_t	*bdp,
	struct cred	*cred,
	struct shmid_ds	*shmds,
	cell_t		*cell)
{
	return pshm_getstat(BHV_NEXT(bdp), cred, shmds, cell);
}

static int
dsshm_mac_access(bhv_desc_t *bdp, struct cred *cred)
{
	return pshm_mac_access(BHV_NEXT(bdp), cred);
}

static void
dsshm_destroy(bhv_desc_t *bdp)
{
	dsshm_t *dsp = BHV_TO_DSSHM(bdp);

	/*
	 * We must make sure no other cells have this vshm -
	 * so we revoke the EXISTENCE token and wait for things
	 * to go idle.
	 * Note that we can only recall the EXISTENCE token
	 * not the LOCK token since clients may still be
	 * looking up and 'using' the vshm on their cell.
	 * (and its illegal to call tks_obtain on a token that
	 * is in the process of being recalled).
	 * XXX this wouldn't be necessary if the local-client
	 * option automatically made sure that tokens
	 * weren't cached .... as OSF does
	 * Note that calling revoke in the local client
	 * case will doo all we want w/ no callouts.
	 */
	tkc_release1(dsp->dssm_tclient, VSHM_EXISTENCE_TOKEN);
	tkc_recall(dsp->dssm_tclient, VSHM_EXISTENCE_TOKENSET, TK_DISP_CLIENT_ALL);
	tks_recall(dsp->dssm_tserver, VSHM_EXISTENCE_TOKENSET, NULL);
	/*
	 * NOTE - by the time we return ALL the vshm, dsp, etc
	 * could have been free'd - keep hands OFF
	 */
}

static void
dsshm_tsif_recall(
	void		*obj,
	tks_ch_t	client,
	tk_set_t	to_be_recalled,
	tk_disp_t	why)
{
	dsshm_t	*dsp = (dsshm_t *)obj;

	if (client == cellid()) {
		tkc_recall(dsp->dssm_tclient, to_be_recalled, why);
	} else {
		service_t	svc;
		int		error;
		int		msgerr;
		bhv_desc_t	*bdp = BHV_NEXT(&dsp->dssm_bd);

		SERVICE_MAKE(svc, (cell_t)client, SVC_SVIPC);
		msgerr = invk_dcshm_recall(svc, &error,
					BHV_TO_VSHM(bdp)->vsm_id,
					to_be_recalled, why);
		ASSERT(!msgerr);
		if (error == ENOENT) {
			/* client didn't know what we were talking about! */
			tks_return(dsp->dssm_tserver, client, TK_NULLSET,
				TK_NULLSET, to_be_recalled, why);
		}
	}
}

/* ARGSUSED */
static void
dsshm_tsif_recalled(void *obj, tk_set_t done, tk_set_t ndone)
{
	dsshm_t	*dsp = (dsshm_t *)obj;
	bhv_desc_t *bdp = &dsp->dssm_bd;
	vshm_t *vshm = BHV_TO_VSHM(bdp);

	ASSERT(ndone == TK_NULLSET);
	/* note - recalled always returns TK_READ set */
	ASSERT(done == VSHM_EXISTENCE_TOKENSET);
	
	/* free DS struct */
	OBJ_STATE_DESTROY(&dsp->dssm_obj_state);
	tkc_destroy_local(dsp->dssm_tclient);
	tks_destroy(dsp->dssm_tserver);
	bdp = BHV_NEXT(bdp);
	bhv_remove(&vshm->vsm_bh, &dsp->dssm_bd);
	kern_free(dsp);

	/* free pshm and vshm */
	pshm_destroy(bdp);
}

#if defined(DEBUG) || defined(SHMDEBUG)
void
idbg_dsshm_print(
	dsshm_t	*dsp)
{
	qprintf("dsshm 0x%x:\n", dsp);
	qprintf("    token state - client 0x%x server 0x%x\n",
		dsp->dssm_tclient,  dsp->dssm_tserver);
	qprintf("    object state 0x%x\n", &dsp->dssm_obj_state);
	qprintf("    bhv 0x%x\n", &dsp->dssm_bd);
	idbg_pshm_print(BHV_PDATA(BHV_NEXT(&dsp->dssm_bd)));
}
#endif
