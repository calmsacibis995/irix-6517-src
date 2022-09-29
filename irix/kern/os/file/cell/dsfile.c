/**************************************************************************
 *									  *
 *	 	Copyright (C) 1986-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded	instructions, statements, and computer programs	 contain  *
 *  unpublished	 proprietary  information of Silicon Graphics, Inc., and  *
 *  are	protected by Federal copyright law.  They  may	not be disclosed  *
 *  to	third  parties	or copied or duplicated	in any form, in	whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident	"$Id: dsfile.c,v 1.34 1997/11/10 23:42:16 ethan Exp $"
#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/sema.h>
#include <sys/systm.h>
#include <sys/errno.h>

#include <stdarg.h>

#include <ksys/vfile.h>
#include <ksys/vfile.h>
#include "dfile_private.h"
#include "pfile_private.h"
#include "I_dsfile_stubs.h"
#include "invk_dcfile_stubs.h"
#include <ksys/cell.h>
#include <sys/vnode.h>
#include <sys/atomic_ops.h>
#include <fs/cfs/cfs.h>
#include <sys/vsocket.h>
#include <ksys/cell/relocation.h>

extern void dsfile_setflags (bhv_desc_t * bdp, int am, int om);
extern void dsfile_do_setflags (dsfile_t * dsfp, int am, int om);
extern void dsfile_getoffset (bhv_desc_t * bdp, off_t *offp);
extern void dsfile_setoffset (bhv_desc_t * bdp, off_t off);
extern void dsfile_getoffset_locked (bhv_desc_t * bdp, int lock, off_t *offp);
extern void dsfile_setoffset_locked (bhv_desc_t * bdp, int lock, off_t off);
extern void dsfile_teardown (bhv_desc_t * bdp);
extern int dsfile_close (bhv_desc_t * bdp, int);
extern void dsfile_destroy (dsfile_t *dsfp, vfile_t *vfp);
extern int dsfile_lookup_handle (obj_handle_t *, dsfile_t **);
extern void dsfile_qlookup(dsfiletab_t *, obj_handle_t *, dsfile_t **dsfilep);

vfileops_t  dsfile_ops = {
	BHV_IDENTITY_INIT_POSITION(VFILE_POSITION_DS),
	dsfile_setflags,
	dsfile_getoffset,
	dsfile_setoffset,
	dsfile_getoffset_locked,
	dsfile_setoffset_locked,
	dsfile_teardown,
	dsfile_close
};

static void dsfile_tsif_recall(void *, tks_ch_t, tk_set_t, tk_disp_t);
static void dsfile_tsif_panic();

tks_ifstate_t dsfile_tserver_iface = {
	dsfile_tsif_recall,
	dsfile_tsif_panic,
	NULL
};

struct zone     *dsfile_zone;
int 		dsfiletabsz;		/* size of hash table */
dsfiletab_t 	*dsfiletab;             /* The lookup hash table */
service_t	dsfile_service;		/* DS service */

void 
dsfile_tsif_panic()
{
	panic("dsfile_tsif_panic");
}

#if DEBUG
/* Leak checker */
long dsfile_outstanding, dsfile_setflag_count, dsfile_created;
#endif

/*
 * Broadcast the change to all clients
 */
void 
dsfile_setflags (
	bhv_desc_t * bdp, 
	int am, 
	int om)
{

	dsfile_do_setflags (BHV_TO_DSFILE(bdp), am, om);

}

void 
dsfile_getoffset (
	bhv_desc_t * bdp, 
	off_t *offp)
{
	dsfile_t *dsfp = BHV_TO_DSFILE(bdp);

	/* Token required to get current value */
	tkc_acquire1 (dsfp->dsf_tclient, DFILE_OFFSET_TOKEN);

	pfile_getoffset (BHV_NEXT(bdp), offp);

	tkc_release1 (dsfp->dsf_tclient, DFILE_OFFSET_TOKEN);
}

void 
dsfile_setoffset (
	bhv_desc_t * bdp, 
	off_t off)
{
	dsfile_t *dsfp = BHV_TO_DSFILE(bdp);

	/* Token required to set current value */
	tkc_acquire1 (dsfp->dsf_tclient, DFILE_OFFSET_TOKEN);

	pfile_setoffset (BHV_NEXT(bdp), off);

	tkc_release1 (dsfp->dsf_tclient, DFILE_OFFSET_TOKEN);
}

void 
dsfile_getoffset_locked (
	bhv_desc_t * bdp, 
	int lock, 
	off_t *offp)
{
	dsfile_t *dsfp = BHV_TO_DSFILE(bdp);

	tkc_acquire1 (dsfp->dsf_tclient, DFILE_OFFSET_TOKEN);

	pfile_getoffset_locked (BHV_NEXT(bdp), lock, offp);
}

void 
dsfile_setoffset_locked (
	bhv_desc_t * bdp, 
	int lock, 
	off_t off)
{
	dsfile_t *dsfp = BHV_TO_DSFILE(bdp);

	pfile_setoffset_locked (BHV_NEXT(bdp), lock, off);

	tkc_release1 (dsfp->dsf_tclient, DFILE_OFFSET_TOKEN);
}

/*
 * tks outcall to DS send recall message to DC 
 * called from I_dsfile_obtain, via tks_obtain
 */
static void
dsfile_tsif_recall(
	void            *obj,
	tks_ch_t        client,
	tk_set_t        to_be_recalled,
	tk_disp_t       why)
{
	dsfile_t *dsfp = (dsfile_t *)obj;
	int	error;

	if (client == cellid()) {
		tkc_recall(dsfp->dsf_tclient, to_be_recalled, why);
	} else {
		/* REFERENCED */
		int		msgerr;
		service_t       svc;

		SERVICE_MAKE (svc, client, SVC_VFILE);
		msgerr = invk_dcfile_recall(svc, &dsfp->dsf_handle,
				    to_be_recalled, why, &error);
		ASSERT(!msgerr);
		if (error) {
			/* client not found */
			tks_return(dsfp->dsf_tserver, client, 
				   TK_NULLSET, TK_NULLSET, to_be_recalled, why);
		}
	}
}

#if DEBUG
long vfsob1, vfsob2, vfsob3, vfsob4, vfsob5;
#endif
/*
 * Called via message from dcfile_tcif_obtain to get
 * the offset token (getoffset)
 * handle is the server, which must be looked up when obtaining
 * the existence token (it is guaranteed in all other cases)
 */
void
I_dsfile_obtain(
	cell_t          sender,
	tk_set_t        to_be_obtained,
	tk_set_t        to_be_returned,
	tk_disp_t       dofret,
	tk_set_t        *already_obtained,
	tk_set_t        *granted,
	tk_set_t        *refused,
	obj_handle_t	*handle,
	off_t		*offset,
	int		*flags,
	int		*error)
{
	dsfile_t *dsfp;
	vfile_t	 *vfp;
	bhv_desc_t *bdp;
	dcfile_t *dcfp;

	if (TK_IS_IN_SET(DFILE_EXISTENCE_TOKENSET, to_be_obtained)) {
		/* Must search for the DS handle */
		if (dsfile_lookup_handle (handle, &dsfp) == 0) {
			VFILE_DBG(vfsob1);
			/* DS found - reference taken */
			bdp = DSFILE_TO_BHV(dsfp);
			vfp = BHV_TO_VFILE(bdp);
			BHV_READ_LOCK(&vfp->vf_bh);
			/*
			 * Check if DS migrated
			 */
			if(BHV_OPS(BHV_HEAD_FIRST(&vfp->vf_bh)) == 
			   &dcfile_ops) {
				VFILE_DBG(vfsob2);
				dcfp = BHV_TO_DCFILE(
					BHV_HEAD_FIRST(&vfp->vf_bh));
				*handle = dcfp->dcf_handle;
				BHV_READ_UNLOCK(&vfp->vf_bh);
				VFILE_REF_RELEASE(vfp);
				*error = EMIGRATED;
				return;
			}
		} else if (dcfile_lookup_handle(handle, &dcfp) == 0) {
			VFILE_DBG(vfsob3);
			/* Found DC - return handle */
			*handle = dcfp->dcf_handle;
			vfp = DCFILE_TO_VFILE(dcfp);
			VFILE_REF_RELEASE(vfp);
			*error = EMIGRATED;
			return;
		} else {
			VFILE_DBG(vfsob4);
			/* not found at all */
			*error = ENOENT;
			return;
		}
	} else {
		VFILE_DBG(vfsob5);
		dsfp = (dsfile_t *)(HANDLE_TO_OBJID(*handle));
		bdp = DSFILE_TO_BHV(dsfp);
	}

	OBJ_SVR_RETARGET_CHECK (&dsfp->dsf_obj_state);

	vfp = BHV_TO_VFILE(bdp);
	ASSERT(BHV_OPS(BHV_HEAD_FIRST(&vfp->vf_bh)) == &dsfile_ops);
	ASSERT(BHV_PDATA(BHV_HEAD_FIRST(&vfp->vf_bh)) == dsfp);

	if (to_be_returned != TK_NULLSET)
		tks_return(dsfp->dsf_tserver, sender, to_be_returned,
		    TK_NULLSET, TK_NULLSET, dofret);

	tks_obtain(dsfp->dsf_tserver, sender, to_be_obtained, granted,
	    refused, already_obtained);

	if (TK_IS_IN_SET(DFILE_OFFSET_TOKENSET, *granted)) {
		dsfp->dsf_offset_fetches++;
		pfile_getoffset (BHV_NEXT(bdp), offset);
	}

	if (TK_IS_IN_SET(DFILE_EXISTENCE_TOKENSET, to_be_obtained)) {
		if (TK_IS_IN_SET(DFILE_EXISTENCE_TOKENSET, *granted)) {
			vfp  = DSFILE_TO_VFILE(dsfp);
			*flags = vfp->vf_flag;
			atomicAddInt (&dsfp->dsf_remote_refs, 1);
		} else {
			VFILE_REF_RELEASE(vfp);
		}
		BHV_READ_UNLOCK(&vfp->vf_bh);
	}

	*error = 0;
	return;

}

/*
 * Find new handle from V
 */
void
I_dsfile_update_handle (
	void		*obj,
	obj_handle_t	*handle)
{
	dsfile_t *dsfp;
	vfile_t	 *vfp = obj;
	bhv_desc_t *bdp;
	dcfile_t *dcfp;

	BHV_READ_LOCK(&vfp->vf_bh);
	bdp = BHV_HEAD_FIRST(&vfp->vf_bh);
	if (BHV_OPS(bdp) == &dsfile_ops) {
		dsfp = (dsfile_t *)BHV_PDATA(bdp);
		*handle = dsfp->dsf_handle;
	} else {
		ASSERT(BHV_OPS(bdp) == &dcfile_ops);
		dcfp = (dcfile_t *)BHV_PDATA(bdp);
		*handle = dcfp->dcf_handle;
	}
	BHV_READ_UNLOCK(&vfp->vf_bh);
}

/*
 * Server routine called via message from client return callout
 * (dcfile_tcif_return).  Updates offset and returns token.
 */
void
I_dsfile_return(
	cell_t          sender,
	objid_t 	objid,
	off_t		offset,
	tk_set_t        to_be_returned,
	tk_set_t        refused,
	tk_set_t        unknown,
	tk_disp_t       why)
{
	dsfile_t *dsfp = (dsfile_t *)objid;
	bhv_desc_t *bdp = DSFILE_TO_BHV(dsfp);
	vfile_t *vfp = DSFILE_TO_VFILE(dsfp);

	OBJ_SVR_RETARGET_CHECK (&dsfp->dsf_obj_state);

	if (TK_IS_IN_SET(DFILE_OFFSET_TOKENSET, to_be_returned)) {
		pfile_setoffset (BHV_NEXT(bdp), offset);
	}

	tks_return(dsfp->dsf_tserver, sender, to_be_returned,
	    refused, unknown, why);

	if (TK_IS_IN_SET(DFILE_EXISTENCE_TOKENSET, to_be_returned)) {
		ASSERT(vfp->vf_count);
		atomicAddInt (&dsfp->dsf_remote_refs, -1);
		VFILE_REF_RELEASE(vfp);
	}

	return;
}

/* ARGSUSED */
static tks_iter_t
setflags_iterator(
	void            *obj,
	tks_ch_t        client,
	tk_set_t        tokens_owned,
	va_list         args)
{
	int		newflags;
	dsfile_t *dsfp = (dsfile_t *)obj;
	service_t       svc;
	int		error;
	/* REFERENCED */
	int		msgerr;

	newflags = va_arg(args, int);

	if (client == cellid()) {
		/* update done by I_dsfile_setflags */
		return TKS_CONTINUE;
	}

	SERVICE_MAKE (svc, client, SVC_VFILE);
	msgerr = invk_dcfile_setflags(svc, newflags, &dsfp->dsf_handle, &error);
	ASSERT(!msgerr);

	if (error) {
		return TKS_RETRY;
	} else {
		return TKS_CONTINUE;
	}

}

void 
I_dsfile_setflags (
	objid_t objid, 
	int	am, 
	int	om)
{
	OBJ_SVR_RETARGET_CHECK (&((dsfile_t *)objid)->dsf_obj_state);
	dsfile_do_setflags ((dsfile_t *)objid, am, om);
}

/*
 * VFILE_SETFLAGS has requested a flags update.  Serialize with
 * other flags updates, then broadcast the change to all 
 * clients.  Called via message from dcfile_setflags, and
 * directly from dsfile_setflags.  May be invoked from client
 * via message, or from server via VFILE_SETFLAGS.
 */
void 
dsfile_do_setflags (
	dsfile_t * dsfp, 
	int am, 
	int om)
{
	/* REFERENCED */
	tks_iter_t result;
	vfile_t  *vfp  = DSFILE_TO_VFILE(dsfp);
	int newflags;
	int s;

	/*
	 * Serialize all flag changes
	 */
	mutex_lock(&dsfp->dsf_flaglock, PRIBIO);
	VFILE_DBG(dsfile_setflag_count);

	/*
	 * Get the current value of the flags, and apply the changes
	 */
	s = VFLOCK(vfp);
	vfp->vf_flag &= am;
	vfp->vf_flag |= om;
	newflags = vfp->vf_flag;
	VFUNLOCK(vfp, s);

	/*
	 * Send update (broadcast) to all clients
	 */
	result = tks_iterate (dsfp->dsf_tserver,
	    DFILE_EXISTENCE_TOKENSET,
	    0,
	    setflags_iterator,
	    newflags);

	/* result is the last value returned from setflags_iterator */
	ASSERT(result == TKS_CONTINUE);

	mutex_unlock(&dsfp->dsf_flaglock);

}

/*
 * Called from VFILE_TEARDOWN when server reference goes away.
 */

void 
dsfile_teardown (
	bhv_desc_t * bdp)
{
	dsfile_t *dsfp = BHV_TO_DSFILE(bdp);
	vfile_t  *vfp  = BHV_TO_VFILE(bdp);

	ASSERT(vfp->vf_count == 0);

	/* return the existence token held by the DS */
	tkc_release1(dsfp->dsf_tclient, DFILE_EXISTENCE_TOKEN);

	/* return (recall) any cached tokens */
	tkc_recall(dsfp->dsf_tclient, 
		TK_ADD_SET (DFILE_EXISTENCE_TOKENSET, DFILE_OFFSET_TOKENSET),
		TK_DISP_CLIENT_ALL);

	dsfile_destroy (dsfp, vfp);
}

/*
 * Called from vfile_close when file is closed
 */
int
dsfile_close (
	bhv_desc_t * bdp,
	int	flag)
{
	dsfile_t *dsfp = BHV_TO_DSFILE(bdp);
	vfile_t  *vfp  = BHV_TO_VFILE(bdp);
	dsfiletab_t	*vq;
	int		hq = DSFILE_HASH(dsfp->dsf_handle);
	int		rval;
	int		s;

	/*
	 * If this is the last close for this DS, then atomically
	 * remove it from the hash list (so an obtain won't find it)
	 */

	vq = &dsfiletab[hq];
	DSFILETAB_LOCK(vq, MR_UPDATE);
	s = VFLOCK(vfp);
	ASSERT(vfp->vf_count > 0);

	ASSERT(!kqueue_isnull(&dsfp->dsf_kqueue));
	if (vfp->vf_count == 1) {
		kqueue_remove(&dsfp->dsf_kqueue);
		kqueue_null(&dsfp->dsf_kqueue);
		vfp->vf_count--;
		rval = VFILE_ISLASTCLOSE;
	} else if (flag == VFILE_SECONDCLOSE) {
		vfp->vf_count--;
		rval = 0;
	} else {
		rval = 0;
	}

	VFUNLOCK(vfp, s);
	DSFILETAB_UNLOCK(vq);

	return(rval);
}

/*
 * Called when last reference goes away - either from client via
 * message, or from server via VFILE_TEARDOWN.
 */
void 
dsfile_destroy (
	dsfile_t *dsfp, 
	vfile_t *vfp)
{
	ASSERT(dsfp->dsf_remote_refs == 0);
	mutex_destroy(&dsfp->dsf_flaglock);
	if (vfp) {
		bhv_remove(&vfp->vf_bh, &dsfp->dsf_bd);
	}
	OBJ_STATE_DESTROY(&dsfp->dsf_obj_state);
	if (vfp) {
		tkc_destroy_local(dsfp->dsf_tclient);
		tks_destroy(dsfp->dsf_tserver);
	}

	pfile_teardown (BHV_NEXT(DSFILE_TO_BHV(dsfp)));

	kmem_zone_free (dsfile_zone, dsfp);

#if DEBUG
	atomicAddLong(&dsfile_outstanding, -1);
#endif
}

/*
 * Export the vnode/vsock associated with this vfile
 */
static void
vfile_export_node(
	vfile_t	*vfp,
	obj_manifest_t *mftp)
{
	/* REFERENCED */
	int		error;

	if (VF_IS_VNODE(vfp)) {
		error = obj_mft_put (mftp, VF_TO_VNODE(vfp), SVC_CFS,
				OBJ_RMODE_REFERENCE);
	} else {
		ASSERT(VF_IS_VSOCK(vfp));
		error = obj_mft_put (mftp, VF_TO_VSOCK(vfp), SVC_VSOCK,
				OBJ_RMODE_REFERENCE);
	}
	ASSERT(error == 0);
}

dsfile_t *
dsfile_interpose(
	vfile_t	*vf,
	cell_t	source_cell)
{
	dsfile_t	*dsp;
	tk_set_t	wanted;
	tk_set_t	granted;
	tk_set_t	refused;
	tk_set_t	already_obtained;
	service_t	svc;
	/* REFERENCED */
	int		error;
	cell_t		local_cell = cellid();

	ASSERT(BHV_IS_WRITE_LOCKED(&vf->vf_bh));
	dsp = kmem_zone_alloc(dsfile_zone, KM_SLEEP);
	dsp->dsf_remote_refs = 0;

	tks_create("dsfile", dsp->dsf_tserver, dsp, &dsfile_tserver_iface,
	    DFILE_NTOKENS, (void *)NULL);

	/*
	 * Get existence token for the server
	 */
	wanted = DFILE_EXISTENCE_TOKENSET;
	tks_obtain(dsp->dsf_tserver, (tks_ch_t)cellid(),
	    wanted, &granted, &refused,
	    &already_obtained);
	ASSERT(granted == wanted);
	ASSERT(already_obtained == TK_NULLSET);
	ASSERT(refused == TK_NULLSET);
	tkc_create_local("dsfile", dsp->dsf_tclient, dsp->dsf_tserver,
	    DFILE_NTOKENS, granted, granted, (void *)NULL);

	if (source_cell != local_cell) {
		/*
		 * Get existence token for new client
		 */
		wanted = DFILE_EXISTENCE_TOKENSET;
		tks_obtain(dsp->dsf_tserver, (tks_ch_t)source_cell,
    			wanted, &granted, &refused,
    			&already_obtained);
		ASSERT(granted == wanted);
		ASSERT(refused == TK_NULLSET);
	}

	mutex_init(&dsp->dsf_flaglock, MUTEX_DEFAULT, "flaglock");

	bhv_desc_init(&dsp->dsf_bd, dsp, vf, &dsfile_ops);

	error = bhv_insert(&vf->vf_bh, &dsp->dsf_bd);
	ASSERT(!error);

	dsp->dsf_offset_fetches = 0;
#if DEBUG
	atomicAddLong(&dsfile_outstanding, 1);
#endif
	VFILE_DBG(dsfile_created);
	SERVICE_MAKE (svc, cellid(), SVC_VFILE);
	HANDLE_MAKE (dsp->dsf_handle, svc, dsp);
	OBJ_STATE_INIT(&dsp->dsf_obj_state, &vf->vf_bh);

	dsfile_insert_handle(&dsp->dsf_handle, dsp);
	if (source_cell != local_cell) {
		obj_SR_target_prepared(&dsp->dsf_obj_state);
	}

	return(dsp);
}

/*
 * Add a new DS to the hash list
 */
void
dsfile_insert_handle (
	obj_handle_t *handle, 
	dsfile_t *dsfp)
{
	dsfiletab_t	*vq;
	int		hq = DSFILE_HASH(*handle);

        vq = &dsfiletab[hq];
	DSFILETAB_LOCK(vq, MR_UPDATE);
#if DEBUG
	{
	dsfile_t	*vdsfp;
        dsfile_qlookup(vq, handle, &vdsfp);
	ASSERT(vdsfp == NULL);
	}
#endif
	kqueue_enter(&vq->dct_queue, &dsfp->dsf_kqueue);
	DSFILETAB_UNLOCK(vq);
}

/*
 * Find a dsfile structure with a given id.
 * Returns with extra reference taken
 */
int
dsfile_lookup_handle(
	obj_handle_t *handle, 
	dsfile_t **dsfilep)
{
	dsfiletab_t	*vq;
	dsfile_t	*dsfile;
	int		hq = DSFILE_HASH(*handle);

	*dsfilep = NULL;

	/*
	 * Look for a match in the appropriate hash queue.
	 * Returns with extra reference taken
	 */
	vq = &dsfiletab[hq];
	DSFILETAB_LOCK(vq, MR_ACCESS);
	dsfile_qlookup(vq, handle, &dsfile);
	DSFILETAB_UNLOCK(vq);

	if (dsfile == NULL)
		return(1);

	*dsfilep = dsfile;
	return(0);
}

/*
 * Called by the DC (import) when the DS and DC are on the same cell
 */
int
dsfile_reference_handle (
	obj_handle_t *handle,
	vfile_t **vfpp)
{
	dsfile_t *dsfp;
	vfile_t *vfp;

	if (dsfile_lookup_handle (handle, &dsfp)) {
		/* Not found! Return error */
		return(0);
	} else {
		vfp  = DSFILE_TO_VFILE(dsfp);
		*vfpp = vfp;
		return(1);
	}
}

void
dsfile_qlookup(
	dsfiletab_t	*vq,
	obj_handle_t	*handle,
	dsfile_t	**dsfilep)
{
	dsfile_t *dsfile;
	kqueue_t *kq;
	vfile_t *vfp;

	/* Get the hash queue */
	kq = &vq->dct_queue;
	/*
	 * Step through the hash queue looking for the given id
	 */
	for (dsfile = (dsfile_t *)kqueue_first(kq);
	     dsfile != (dsfile_t *)kqueue_end(kq);
	     dsfile = (dsfile_t *)kqueue_next(&dsfile->dsf_kqueue)) {
		if (HANDLE_EQU(*handle, dsfile->dsf_handle)) {
			/*
			 * Got it. Take reference
			 */
			vfp  = DSFILE_TO_VFILE(dsfile);
			VFILE_REF_HOLD(vfp);
			*dsfilep = dsfile;
			return;
		}
	}
	/*
	 * Can't find it, return NULL
	 */
	*dsfilep = NULL;
}

void
dsfile_dequeue(dsfile_t *dsfp)
{
	dsfiletab_t	*vq;
	int		hq = DSFILE_HASH(dsfp->dsf_handle);

	vq = &dsfiletab[hq];
	DSFILETAB_LOCK(vq, MR_UPDATE);

	ASSERT(!kqueue_isnull(&dsfp->dsf_kqueue));
	kqueue_remove(&dsfp->dsf_kqueue);
	kqueue_null(&dsfp->dsf_kqueue);

	DSFILETAB_UNLOCK(vq);

	return;
}

#if DEBUG
long vfsp1, vfsp2, vfsp3, vfsp4, vfsp5, vfsp6, vfsp7, vfsp8, vfsp9, vfsp91, vfsp92, vfsp93;
#endif
/* ARGSUSED */
int
vfile_obj_source_prepare(
	obj_manifest_t	*mftp,		/* IN/OUT object manifest */
	void		*v)		/* IN vfile */
{
	service_t	svc;
	vfile_t		*vfp;
	dsfile_t 	*dsfp;
	tk_set_t        granted;
	tk_set_t        already_obtained;
	tk_set_t        refused;
	tk_set_t        wanted;
	bhv_desc_t 	*bdp;
	cell_t		mycell;
	vfile_ei_bag1_t	ei_bag;
	/* REFERENCED */
	cell_t 		old;
	obj_mft_info_t	minfo;
	cell_t		target_cell;
	int		error = 0;

	/* Get target cell. */
	obj_mft_info_get(mftp, &minfo);
	target_cell = minfo.target_cell;

	vfp = v;
	ASSERT(vfp);

/* Do this in vnode code ? */
	if (VF_IS_VNODE(vfp)) {
		VN_HOLD(VF_TO_VNODE(vfp));
	}

retry:
	BHV_READ_LOCK(&vfp->vf_bh);
	ASSERT(vfp->vf_count > 0);
	bdp = BHV_HEAD_FIRST(&vfp->vf_bh);

	mycell = cellid();
	ei_bag.existence_token_granted = 0;
	ei_bag.v = vfp;

	if (BHV_OPS(bdp) == &dcfile_ops) {
		/* Is DC */
		dcfile_t *dcfp = BHV_TO_DCFILE(bdp);
		VFILE_DBG(vfsp1);
		if (!(minfo.rmode & OBJ_RMODE_REFERENCE)) {
			VFILE_DBG(vfsp91);
			error = EMIGRATED;
			BHV_READ_UNLOCK(&vfp->vf_bh);
			goto done;
		}
		/* new client must obtain the existence token */
		ei_bag.server_handle = dcfp->dcf_handle;
		BHV_READ_UNLOCK(&vfp->vf_bh);
		goto done;
	}

	if (BHV_OPS(bdp) == &dsfile_ops) {
		/* Is DS */
		dsfp = BHV_TO_DSFILE(bdp);

		/*
		 * Relocate the server if there is only 1 local reference
		 */
/* Note: counts can change !! */
		if ((minfo.rmode == OBJ_RMODE_SERVER) || 
		    ((vfp->vf_count - dsfp->dsf_remote_refs) == 1)) {
			VFILE_DBG(vfsp2);
			if ((obj_SR_source_begin(&dsfp->dsf_obj_state))
				 == OBJ_SUCCESS) {
				VFILE_DBG(vfsp3);
				ASSERT(target_cell != mycell);
				minfo.rmode |= OBJ_RMODE_SERVER;
			} else if (minfo.rmode == OBJ_RMODE_SERVER) {
				VFILE_DBG(vfsp92);
				error = EMIGRATED;
				BHV_READ_UNLOCK(&vfp->vf_bh);
				goto done;
			}
		}

		ei_bag.server_handle = dsfp->dsf_handle;
		ei_bag.server_vfile = *vfp;
		VFILE_DBG(vfsp7);

		BHV_READ_UNLOCK(&vfp->vf_bh);
		goto done;
	}

	ASSERT (BHV_OPS(bdp) == &pfile_ops);
	BHV_READ_UNLOCK(&vfp->vf_bh);
	BHV_WRITE_LOCK(&vfp->vf_bh);
	if (BHV_OPS(bdp) != &pfile_ops) {
		BHV_WRITE_UNLOCK(&vfp->vf_bh);
		goto retry;
	}
	VFILE_DBG(vfsp4);
	ASSERT(target_cell != mycell);

	/*
	 * It's a pfile - interpose a DS
	 */
	dsfp = dsfile_interpose(vfp, cellid());

	if (target_cell != mycell) {
		/*
		 * Get existence token for the client
		 */
		wanted = DFILE_EXISTENCE_TOKENSET;
		tks_obtain(dsfp->dsf_tserver, (tks_ch_t)target_cell,
    			wanted, &granted, &refused,
    			&already_obtained);
		ASSERT((granted | already_obtained) == wanted);
		ASSERT(refused == TK_NULLSET);
		if (TK_IS_IN_SET(DFILE_EXISTENCE_TOKENSET, granted)) {
			VFILE_REF_HOLD(vfp);
			atomicAddInt (&dsfp->dsf_remote_refs, 1);
			ei_bag.existence_token_granted = 1;
		}
	}

	ei_bag.server_handle = dsfp->dsf_handle;
	ei_bag.existence_token_granted = 1;
	ei_bag.server_vfile = *vfp;

	/*
	 * Relocate the server if there is only 1 local reference
	 */
/* Note: counts can change !! */
	if ((minfo.rmode == OBJ_RMODE_SERVER) || 
	    ((vfp->vf_count - dsfp->dsf_remote_refs) == 1)) {
		VFILE_DBG(vfsp5);
		if ((obj_SR_source_begin(&dsfp->dsf_obj_state))
			 == OBJ_SUCCESS) {
			VFILE_DBG(vfsp6);
			ASSERT(target_cell != mycell);
			minfo.rmode |= OBJ_RMODE_SERVER;
		} else if (minfo.rmode == OBJ_RMODE_SERVER) {
			VFILE_DBG(vfsp93);
			error = EMIGRATED;
			BHV_READ_UNLOCK(&vfp->vf_bh);
			goto done;
		}
	}

	BHV_WRITE_UNLOCK(&vfp->vf_bh);

done:
	if (error) {
		if (VF_IS_VNODE(vfp)) {
			VN_RELE(VF_TO_VNODE(vfp));
		}
	} else {
		SERVICE_MAKE(svc, cellid(), SVC_VFILE);
		HANDLE_MAKE(minfo.source.hndl, svc, vfp);
		minfo.source.tag = VFILE_BAG_TAG;
		minfo.source.infop = &ei_bag;
		minfo.source.info_size =  sizeof(ei_bag);
		obj_mft_info_put(mftp, &minfo);
		vfile_export_node (vfp, mftp);
	}
	return (error);
}

#if DEBUG
long vfsrt1, vfsrt2;
#endif
/* ARGSUSED */
static tks_iter_t
dsfile_retarget_iter(
	void            *obj,
	tks_ch_t        client,
	tk_set_t        tokens_owned,
	va_list         args)
{
	dsfile_t *dsfp = (dsfile_t *)obj;
	service_t       svc;
	cell_t		target_cell	= va_arg(args, cell_t);
	obj_handle_t	*target_handle	= va_arg(args, obj_handle_t *);
	obj_retarget_t	*rt_syncp	= va_arg(args, void *);
	/* REFERENCED */
	int		msgerr;

	if ((client == cellid()) || (client == target_cell)) {
		return TKS_CONTINUE;
	}

	if (!TK_IS_IN_SET(DFILE_EXISTENCE_TOKENSET, tokens_owned)) {
		VFILE_DBG(vfsrt2);
		return TKS_CONTINUE;
	}

	SERVICE_MAKE (svc, client, SVC_VFILE);
	VFILE_DBG(vfsrt1);
	msgerr = invk_dcfile_retarget(svc, target_handle,
				&dsfp->dsf_handle, rt_syncp, dsfp);
	ASSERT(!msgerr);

	obj_retarget_register(rt_syncp, client);

	return TKS_CONTINUE;

}

void
dsfile_client_retarget(
	dsfile_t *dsfp,
	cell_t target_cell, 
	obj_handle_t *target_handle)
{
	obj_retarget_t	rt_sync;

	/*
	 * Send retarget message to all clients
	 */

	obj_retarget_begin(&rt_sync);

	(void) tks_iterate(dsfp->dsf_tserver,
		   	DFILE_EXISTENCE_TOKENSET,
		   	TKS_STABLE,
		   	dsfile_retarget_iter,
		   	target_cell, target_handle, 
			PHYS_TO_K0(kvtophys(&rt_sync)));

	obj_retarget_wait(&rt_sync);
	obj_retarget_end(&rt_sync);
}

#if DEBUG
long vfsb1, vfsb2;
#endif
/* ARGSUSED */
int
vfile_obj_source_bag(
	obj_manifest_t	*mftp,			/* IN object manifest */
	obj_bag_t	source_bag)		/* IN/OUT object state */
{
	obj_handle_t	target_handle;
	vfile_t		*vfp;
	dsfile_t	*dsfp;
	dcfile_t 	*dcfp;
	vfile_ei_bag2_t	ei_bag;
	/* REFERENCED */
	int		error;
	int		s;
	obj_mft_info_t	minfo;

	minfo.source.tag = OBJ_TAG_NONE;
	minfo.target.tag = VFILE_BAG_TAG;
	minfo.target.infop = &target_handle;
	minfo.target.info_size = sizeof(target_handle);
	obj_mft_info_get(mftp, &minfo);
	vfp = (vfile_t *)HANDLE_TO_OBJID(minfo.source.hndl);

	if (minfo.rmode == OBJ_RMODE_REFERENCE) {
		/*
		 * Nothing to do
		 */
		VFILE_DBG(vfsb1);
		return(0);
	}
	VFILE_DBG(vfsb2);
	dsfp = VFILE_TO_DSFILE(vfp);

	BHV_WRITE_LOCK (&vfp->vf_bh);
	dsfile_client_retarget(dsfp, minfo.target_cell, &target_handle);

	/*
	 * Create new DC on this cell
	 * Link vfp to the new DC 
	 * (v->dc->ds->p)
	 * Add DC to hash list
	 */
	dcfile_create (&dcfp, vfp, &target_handle);

/* Both DS and DC exist together right now */
/* What about other refs to dsfile?  Like from lookups? */

	/*
	 * Remove old DS from hash list (cannot be found by lookup)
	 */
	dsfile_dequeue(dsfp);

	/*
	 * return the offset token if cached
	 * this allows the correct offset to be passed to the new DS
	 */
	tkc_recall(dsfp->dsf_tclient, 
		DFILE_OFFSET_TOKENSET, TK_DISP_CLIENT_ALL);

	/*
	 * Bag all state for new server
	 */
	tks_bag(dsfp->dsf_tserver, source_bag);
	pfile_getoffset (BHV_NEXT(DSFILE_TO_BHV(dsfp)), &ei_bag.offset);

	/*
	 * Only the local references remain on this cell
	 */
	s = VFLOCK(vfp);
	ei_bag.server_vfile = *vfp;
	vfp->vf_count -= dsfp->dsf_remote_refs;
	ASSERT(vfp->vf_count > 0);
	ei_bag.remote_ref_count = dsfp->dsf_remote_refs;
	dsfp->dsf_remote_refs = 0;
	VFUNLOCK(vfp, s);

	/*
	 * New DC - copy any client tokens to the new DC
	 */
	tkc_copy (dsfp->dsf_tclient, dcfp->dcf_tclient);

	error = obj_bag_put(source_bag, VFILE_BAG_TAG, &ei_bag, 
		sizeof(ei_bag));
	ASSERT(error == 0);

	/*
	 * At this point the old DS has been unlinked, and the
	 * new DC linked.
	 */
	BHV_WRITE_UNLOCK (&vfp->vf_bh);
	return(0);
}

#if DEBUG
long vfse1, vfse2, vfse3;
#endif
/* ARGSUSED */
int
vfile_obj_source_end(
	obj_manifest_t	*mftp)		/* IN object manifest */
{
	obj_mft_info_t	minfo;
	vfile_t		*vfp;
	dsfile_t 	*dsfp;
	bhv_desc_t	*bdp;

	minfo.source.tag = OBJ_TAG_NONE;
	minfo.target.tag = OBJ_TAG_NONE;
	obj_mft_info_get(mftp, &minfo);
	vfp = (vfile_t *)HANDLE_TO_OBJID(minfo.source.hndl);

	if (minfo.rmode & OBJ_RMODE_SERVER) {
		VFILE_DBG(vfse1);

/* When/how to synch with lookups */

		/*
		 * Destroy the old server and pfile - they have been replaced by
		 * a new client.
		 */

		bdp = BHV_HEAD_FIRST(&vfp->vf_bh);
		ASSERT(BHV_POSITION(bdp) == VFILE_POSITION_DC);
		dsfp = BHV_TO_DSFILE(BHV_NEXT(bdp));
		bdp->bd_next = NULL;
		obj_SR_source_end(&dsfp->dsf_obj_state);
		tkc_free(dsfp->dsf_tclient);
		tks_free(dsfp->dsf_tserver);
		/* KLUDGE ! */
		dsfp->dsf_bd.bd_vobj = NULL;
		dsfp->dsf_bd.bd_next->bd_vobj = NULL;
		dsfile_destroy (dsfp, NULL);
	} else {
		VFILE_DBG(vfse3);
	}

	/*
	 * A vfile relocation is ALWAYS caused by a
	 * process migration.  That means that the vfile reference
	 * from the process on the source cell has been replaced
	 * by the reference from the same process on the target
	 * cell.  So we need to be careful how the old reference
	 * is released:  if the old reference is the last reference,
	 * do normal vfile_close processing; otherwise just
	 * drop the reference and do NOT call VOP_CLOSE (etc).
	 */
	ASSERT(vfp);
	ASSERT(vfp->vf_count >= 1);
	VFILE_REF_RELEASE(vfp);
	return (0);
}

/* ARGSUSED */
int
vfile_obj_source_abort(
	obj_manifest_t	*mftp)		/* IN object manifest */
{
	obj_mft_info_t	minfo;
	vfile_t	*vfp;

	minfo.source.tag = OBJ_TAG_NONE;
	minfo.target.tag = OBJ_TAG_NONE;
	obj_mft_info_get(mftp, &minfo);
	vfp = (vfile_t *)HANDLE_TO_OBJID(minfo.source.hndl);

	/*
	 * Release reference on vnode take by export
	 */
	if (VF_IS_VNODE(vfp)) {
		VN_RELE(VF_TO_VNODE(vfp));
	}
	return 0;
}

/* ARGSUSED */
int
vfile_obj_target_abort(
	obj_manifest_t	*mftp)		/* IN object manifest */
{
	obj_mft_info_t	minfo;
	vfile_t	*vfp;

	minfo.source.tag = OBJ_TAG_NONE;
	minfo.target.tag = OBJ_TAG_NONE;
	obj_mft_info_get(mftp, &minfo);
	vfp = (vfile_t *)HANDLE_TO_OBJID(minfo.target.hndl);

/* Need to re-incarnate old environment ??? */

	/*
	 * A vfile relocation is ALWAYS caused by a
	 * process migration.  That means that the vfile reference
	 * from the process on the source cell has been replaced
	 * by the reference from the same process on the target
	 * cell.  In this case the relocation has failed and
	 * the reference to this file on the target cell must
	 * be released.   Normal vfile_close processing is not
	 * appropriate because this is not a normal close - it is
	 * a failed relocation.  The process still exists on the
	 * source cell.
	 */
	ASSERT(vfp);
	ASSERT(vfp->vf_count >= 1);
	VFILE_REF_RELEASE(vfp);
	return (0);
}

/* ARGSUSED */
int
vfile_service_lookup (
	void 		*objid,
	service_t	*svc)
{
	printf("vfile_service_lookup called - noop!");
	return (EINVAL);
}

/* ARGSUSED */
int
vfile_service_evict(
	void 	*objid,
	cell_t	target_cell)
{
	dsfiletab_t	*vq;
	dsfile_t	*dsfile;
	int		hq;
	kqueue_t 	*kq;
	int		done = 0;
	obj_manifest_t	*mftp;
	vfile_t 	*vfp;
	int		error;

	while (!done) {
		done = 1;
		for (hq = 0; hq < dsfiletabsz; hq++) {
			vq = &dsfiletab[hq];
			kq = &vq->dct_queue;
			DSFILETAB_LOCK(vq, MR_ACCESS);
			dsfile = (dsfile_t *)kqueue_first(kq);
			while (dsfile != (dsfile_t *)kqueue_end(kq)) {
				/*
				 * Take a new reference for the relocation
				 */
				vfp  = DSFILE_TO_VFILE(dsfile);
#if HENSELER
/* KLUDGE! */
				if ((vfp->vf_count - dsfile->dsf_remote_refs) 
					== 0) {
#endif
					VFILE_REF_HOLD(vfp);
					DSFILETAB_UNLOCK(vq);
#if HENSELER
				} else {
					dsfile = (dsfile_t *)
					   kqueue_next(&dsfile->dsf_kqueue);
					continue;
				}
#endif

				printf ("Relocating vfile %lx %lx from %d to cell %d \n", 
				  vfp, dsfile, cellid(), target_cell);
				/*
				 * Use CORE to relocate the server
				 */
				mftp = obj_mft_create(target_cell);
				ASSERT(mftp);
				done = 0;
				error = obj_mft_put (mftp, (void *)vfp,
					SVC_VFILE, OBJ_RMODE_SERVER);
				if (error) {
					printf ("vfile relocate failed %d\n",
						error);
					ASSERT(error == EMIGRATED);
					VFILE_REF_RELEASE(vfp);
				} else {
					error = obj_mft_ship(mftp);
					ASSERT(error == 0);
printf("relocated %lx \n", vfp);
				}
				obj_mft_destroy(mftp);

				DSFILETAB_LOCK(vq, MR_ACCESS);
				dsfile = (dsfile_t *)kqueue_first(kq);
			}
			DSFILETAB_UNLOCK(vq);
		}
	}
	return(0);
}

/* ARGSUSED */
int
vfile_service_shutdown(
	cell_t	surrogate_cell)
{
	printf("vfile_service_shutdown called - noop!");
	return(EINVAL);
}

obj_relocation_if_t vfile_obj_iface = {
	vfile_obj_source_prepare,
	vfile_obj_target_prepare,
	NULL,
	NULL,
	vfile_obj_source_bag,
	vfile_obj_target_unbag,
	vfile_obj_source_end,
	vfile_obj_source_abort,
	vfile_obj_target_abort
};

obj_service_if_t vfile_service_iface = {
	vfile_service_lookup,
	vfile_service_evict,
	vfile_service_shutdown,
};

void
dsfile_cell_init()
{
        int             i;
        dsfiletab_t       *dsq;

	dsfile_zone = kmem_zone_init(sizeof(struct dsfile), "dsfile table");

        dsfiletabsz = 128;
        dsfiletab = (dsfiletab_t *)kern_malloc(sizeof(dsfiletab_t) * dsfiletabsz);
	SERVICE_MAKE (dsfile_service, cellid(), SVC_VFILE);
        ASSERT(dsfiletab != 0);
        for (i = 0; i < dsfiletabsz; i++) {
                dsq = &dsfiletab[i];
                kqueue_init(&dsq->dct_queue);
                mrinit (&dsq->dct_lock, "dst");
        }

	obj_service_register(dsfile_service, &vfile_service_iface, 
		&vfile_obj_iface); 
}

#if DEBUG
long vfsrte1, vfsrte2, vfsrte3, vfsrte4;
#endif
/*
 * This routine is called by a client cell when it has completed
 * its retargetting to the target server cell.
 */
/* ARGSUSED */
void
I_dsfile_obj_retargetted(
	cell_t		client,
	void		*rt_sync,
	obj_handle_t	*new_handle,
	void		*error)
{
	/*
	 * If error is not NULL, then the client could not complete the
	 * retarget as requested.  Check the token state and try again.
	 */
	if (error) {
		dsfile_t 	*dsfp = error;
		service_t	svc;
		tk_set_t        already_obtained;
		tk_set_t        granted;
		tk_set_t        refused;

		VFILE_DBG(vfsrte1);
		/*
		 * Attempt to get the existence token
		 */
		tks_obtain(dsfp->dsf_tserver, client, DFILE_EXISTENCE_TOKENSET, 
			&granted, &refused, &already_obtained);

		/*
		 * If the token was obtained, give it back and complete the
		 * retarget.  If the token was not obtained, reissue the
		 * retarget.
		 */
		if (TK_IS_IN_SET(DFILE_EXISTENCE_TOKENSET, already_obtained)) {
			/* REFERENCED */
			int	msgerr;

			VFILE_DBG(vfsrte2);
			SERVICE_MAKE (svc, client, SVC_VFILE);
			msgerr = invk_dcfile_retarget(svc,
				new_handle,
				&dsfp->dsf_handle, rt_sync, dsfp);
			ASSERT(!msgerr);
		} else {
			VFILE_DBG(vfsrte3);
			tks_return(dsfp->dsf_tserver, client, 
				DFILE_EXISTENCE_TOKENSET, TK_NULLSET, 
				TK_NULLSET, TK_DISP_CLIENT_ALL);
			obj_retarget_deregister(rt_sync, client);
		}
	} else {
		VFILE_DBG(vfsrte4);
		obj_retarget_deregister(rt_sync, client);
	}
}

dsfile_t *
vfile_to_dsfile(
	vfile_t	*vf)
{
	bhv_desc_t	*bdp;

	bdp = BHV_HEAD_FIRST(&vf->vf_bh);
	if (BHV_OPS(bdp) == &pfile_ops) {
		BHV_READ_UNLOCK(&vf->vf_bh);
		BHV_WRITE_LOCK(&vf->vf_bh);
		bdp = BHV_HEAD_FIRST(&vf->vf_bh);
		if (BHV_OPS(bdp) == &pfile_ops) {
			(void)dsfile_interpose(vf, cellid());
			bdp = BHV_HEAD_FIRST(&vf->vf_bh);
		}
		BHV_WRITE_TO_READ(&vf->vf_bh);
	}
	if (BHV_OPS(bdp) == &dsfile_ops)
		return(BHV_TO_DSFILE(bdp));
	ASSERT(BHV_OPS(bdp) == &dcfile_ops);
	return(NULL);
}
