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
#ident	"$Id: dcfile.c,v 1.33 1997/11/10 23:41:35 ethan Exp $"
#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/sema.h>
#include <sys/systm.h>

#include <ksys/cell/tkm.h>
#include <ksys/vfile.h>
#include <ksys/vfile.h>
#include "dfile_private.h"
#include "I_dcfile_stubs.h"
#include "invk_dsfile_stubs.h"
#include <sys/vnode.h>
#include <sys/atomic_ops.h>
#include <fs/cfs/dv.h>
#include <fs/cfs/cfs.h>
#include <sys/vsocket.h>
#include <sys/major.h>
#include <fs/specfs/spec_lsnode.h>
#include <ksys/cell/mesg.h>
#include <ksys/cell/subsysid.h>
#include "I_dsfile_stubs.h"
/*
 * The routines in this file implement the DC file (dcfile) object.
 *
 * The DC caches the file offset (dcf_offset) under control of the
 * DFILE_OFFSET_TOKEN.
 *
 */

extern void dcfile_setflags (bhv_desc_t * bdp, int am, int om);
extern void dcfile_getoffset (bhv_desc_t * bdp, off_t *offp);
extern void dcfile_setoffset (bhv_desc_t * bdp, off_t off);
extern void dcfile_getoffset_locked (bhv_desc_t * bdp, int lock, off_t *offp);
extern void dcfile_setoffset_locked (bhv_desc_t * bdp, int lock, off_t off);
extern void dcfile_teardown (bhv_desc_t * bdp);
extern void dcfile_qlookup( dcfiletab_t	*, obj_handle_t	*, dcfile_t **);
extern int dcfile_close (bhv_desc_t *bdp, int);

vfileops_t  dcfile_ops = {
	BHV_IDENTITY_INIT_POSITION(VFILE_POSITION_DC),
	dcfile_setflags,
	dcfile_getoffset,
	dcfile_setoffset,
	dcfile_getoffset_locked,
	dcfile_setoffset_locked,
	dcfile_teardown,
	dcfile_close
};

#if DEBUG
long dcfile_outstanding, dcfile_setflag_count, dcfile_created;
long dcfile_found, dsfile_found, dcfile_obtained;
#endif

static void dcfile_tcif_obtain(void *, tk_set_t, tk_set_t, tk_disp_t,
tk_set_t *);
static void dcfile_tcif_return(tkc_state_t, void *, tk_set_t, tk_set_t,
tk_disp_t);
tkc_ifstate_t dcfile_tclient_iface = {
	dcfile_tcif_obtain,
	dcfile_tcif_return
};

int 		dcfiletabsz;		/* size of hash table */
dcfiletab_t 	*dcfiletab;             /* The lookup hash table */
struct zone     *dcfile_zone;
extern service_t dsfile_service;        /* DS service */

/*
 * Send flags change to server, which will serialize and broadcast 
 * it to all clients.
 */
void 
dcfile_setflags (
	bhv_desc_t * bdp, 
	int am, 
	int om)
{
	dcfile_t *dcfp = BHV_TO_DCFILE(bdp);
	/* REFERENCED */
	int	msgerr;

	VFILE_DBG(dcfile_setflag_count);
	msgerr = invk_dsfile_setflags (DCFILE_TO_SERVICE(dcfp),
	    DCFILE_TO_OBJID(dcfp), am, om);
	ASSERT(!msgerr);
}

void 
dcfile_getoffset (
	bhv_desc_t * bdp, 
	off_t *offp)
{
	dcfile_t *dcfp = BHV_TO_DCFILE(bdp);

	/* Token required to get current value */
	tkc_acquire1 (dcfp->dcf_tclient, DFILE_OFFSET_TOKEN);

	ASSERT(dcfp->dcf_offset_valid); /* DEBUG */
	*offp = dcfp->dcf_offset;

	tkc_release1 (dcfp->dcf_tclient, DFILE_OFFSET_TOKEN);
}

void 
dcfile_setoffset (
	bhv_desc_t * bdp, 
	off_t off)
{
	dcfile_t *dcfp = BHV_TO_DCFILE(bdp);

	/* Token required to set current value */
	tkc_acquire1 (dcfp->dcf_tclient, DFILE_OFFSET_TOKEN);

	ASSERT(dcfp->dcf_offset_valid); /* DEBUG */
	if (off != VFILE_NO_OFFSET)
		dcfp->dcf_offset = off;

	tkc_release1 (dcfp->dcf_tclient, DFILE_OFFSET_TOKEN);
}

void 
dcfile_getoffset_locked (
	bhv_desc_t * bdp, 
	int lock, 
	off_t *offp)
{
	dcfile_t *dcfp = BHV_TO_DCFILE(bdp);

	tkc_acquire1 (dcfp->dcf_tclient, DFILE_OFFSET_TOKEN);
	ASSERT(dcfp->dcf_offset_valid); /* DEBUG */
	if (lock) {
		mutex_lock(&dcfp->dcf_offlock, PRIBIO);
	}
	*offp = dcfp->dcf_offset;
}

void 
dcfile_setoffset_locked (
	bhv_desc_t * bdp, 
	int lock, 
	off_t off)
{
	dcfile_t *dcfp = BHV_TO_DCFILE(bdp);

	ASSERT(dcfp->dcf_offset_valid); /* DEBUG */
	if (off != VFILE_NO_OFFSET)
		dcfp->dcf_offset = off;

	if (lock) {
		mutex_unlock(&dcfp->dcf_offlock);
	}
	tkc_release1 (dcfp->dcf_tclient, DFILE_OFFSET_TOKEN);
}

/*
 * called by getoffset via tkc_acquire callout to get the offset token
 */
void
dcfile_tcif_obtain(
	void            *obj,
	tk_set_t        to_be_obtained,
	tk_set_t        to_be_returned,
	tk_disp_t       dofret,
	tk_set_t        *refused)
{
	dcfile_t         *dcfp = (dcfile_t *)obj;
	tk_set_t        granted, already;
	cell_t          mycell = cellid();
	off_t		offset;
	int		flags;
	int		error;
	/* REFERENCED */
	int		msgerr;

	/*
	 * Get token (and possibly offset) from server
	 * this is a synchronous ipc (with reply)
         */

	msgerr = invk_dsfile_obtain(DCFILE_TO_SERVICE(dcfp),
	    mycell, 
	    to_be_obtained,
	    to_be_returned, 
	    dofret,
	    &already, 
	    &granted, 
	    refused,
	    &dcfp->dcf_handle,
	    &offset,
	    &flags,
	    &error);
	ASSERT(!msgerr);

	ASSERT(error == 0);
	ASSERT(*refused == TK_NULLSET);

	if (TK_IS_IN_SET(DFILE_OFFSET_TOKENSET, to_be_obtained)) {
		if (TK_IS_IN_SET(DFILE_OFFSET_TOKENSET, granted)) {
			/*
	 		 * Update offset
	 		 */
			dcfp->dcf_offset = offset;
		} else {
			ASSERT(TK_IS_IN_SET(DFILE_OFFSET_TOKENSET, already));
		}
		dcfp->dcf_offset_valid = 1; /* DEBUG */
	}
}

/*
 * tkc callout to return token (and offset) to server
 */
static void
dcfile_tcif_return(
	tkc_state_t     tclient,
	void            *obj,
	tk_set_t        to_be_returned,
	tk_set_t        unknown,
	tk_disp_t       why)
{
	/* REFERENCED */
	int		msgerr;
	dcfile_t         *dcfp = (dcfile_t *)obj;

	ASSERT(tclient == dcfp->dcf_tclient);

	msgerr = invk_dsfile_return(DCFILE_TO_SERVICE(dcfp),
	    cellid(),
	    DCFILE_TO_OBJID(dcfp),
	    dcfp->dcf_offset,
	    to_be_returned, 
	    TK_NULLSET, 
	    unknown,
	    why);
	ASSERT(!msgerr);

	if (TK_IS_IN_SET(DFILE_OFFSET_TOKENSET, to_be_returned)) {
		dcfp->dcf_offset_valid = 0; /* DEBUG */
	}

	tkc_returned(tclient, to_be_returned, TK_NULLSET);
}

/*
 * Client routine called in response to server recall callout
 */
void
I_dcfile_recall(
	obj_handle_t *handle,
	tk_set_t to_be_recalled,
	tk_disp_t why,
	int	*error)
{
	dcfile_t *dcfp;
	vfile_t  *vfp;

	if (dcfile_lookup_handle(handle, &dcfp) == 0) {
		vfp = DCFILE_TO_VFILE(dcfp);
		tkc_recall(dcfp->dcf_tclient, to_be_recalled, why);
		VFILE_REF_RELEASE(vfp);
		*error = 0;
	} else {
		*error = 1;
	}
}

/*
 * Called from server via message when the server needs to broadcast
 * a flags change.  The DC is passed from the server.
 *
 * The entire new flags value is passed from the server
 */

void
I_dcfile_setflags(
	int	newflags, 
	obj_handle_t *handle,
	int	*error)
{
	dcfile_t *dcfp;
	vfile_t  *vfp;
	int	s;

	if (dcfile_lookup_handle(handle, &dcfp) == 0) {
		vfp = DCFILE_TO_VFILE(dcfp);
		s = VFLOCK(vfp);
		vfp->vf_flag = newflags;
		VFUNLOCK(vfp, s);
		VFILE_REF_RELEASE(vfp);
		*error = 0;
	} else {
		*error = 1;
	}
}

/*
 * Called from vfile_close via VFILE_TEARDOWN when vfile vf_count goes to
 * zero (last close from THIS cell).  Need to return the existence 
 * and offset token iff currently held.
 */

void 
dcfile_teardown (
	bhv_desc_t * bdp)
{
	dcfile_t *dcfp = BHV_TO_DCFILE(bdp);
	vfile_t  *vfp  = BHV_TO_VFILE(bdp);
	tk_set_t        retset;
	tk_disp_t       why;
	/* REFERENCED */
	int		msgerr;

	/*
	 * already removed from hash list by dcfile_close
	 */

	ASSERT(vfp->vf_count == 0);
	tkc_release1 (dcfp->dcf_tclient, DFILE_EXISTENCE_TOKEN);
	tkc_returning(dcfp->dcf_tclient, 
		TK_ADD_SET(DFILE_EXISTENCE_TOKENSET, DFILE_OFFSET_TOKENSET), 
		&retset, &why, 1);
	ASSERT(retset & DFILE_EXISTENCE_TOKENSET);
	ASSERT(vfp->vf_count == 0);
	ASSERT(kqueue_isnull(&dcfp->dcf_kqueue));

	/*
         * Invoke the server and teardown token client.
         */
	msgerr = invk_dsfile_return(DCFILE_TO_SERVICE(dcfp),
	    cellid(),
	    DCFILE_TO_OBJID(dcfp),
	    dcfp->dcf_offset,
	    retset, 
	    TK_NULLSET, 
	    TK_NULLSET, 
	    why);
	ASSERT(!msgerr);
	tkc_returned(dcfp->dcf_tclient, retset, TK_NULLSET);
	tkc_destroy(dcfp->dcf_tclient);
	bhv_remove(&vfp->vf_bh, &dcfp->dcf_bd);
	mutex_destroy(&dcfp->dcf_offlock);
	kmem_zone_free (dcfile_zone, dcfp);

#if DEBUG
	atomicAddLong(&dcfile_outstanding, -1);
#endif

}

/*
 * Called from VFILE_CLOSE (called from vfile_close)
 */
int
dcfile_close (
	bhv_desc_t *bdp,
	int	flag)
{
	dcfile_t *dcfp = BHV_TO_DCFILE(bdp);
	vfile_t  *vfp  = BHV_TO_VFILE(bdp);
	dcfiletab_t	*vq;
	int		hq = DCFILE_HASH(dcfp->dcf_handle);
	int		rval;
	int		s;

	/*
	 * If this is the last close for this DC, then atomically
	 * remove it from the hash list (so an import won't find
	 * it) and return "1" to the caller indicating that the
	 * Vxxx close can be skipped (because it will be done by
	 * the DS when the existence token is returned).
	 */

	vq = &dcfiletab[hq];
	DCFILETAB_LOCK(vq, MR_UPDATE);
	s = VFLOCK(vfp);
	ASSERT(vfp->vf_count > 0);

	ASSERT(!kqueue_isnull(&dcfp->dcf_kqueue));
	if (vfp->vf_count == 1) {
		kqueue_remove(&dcfp->dcf_kqueue);
		kqueue_null(&dcfp->dcf_kqueue);
		vfp->vf_count--;

		/*
		 * If this file is one the the specfs devices
		 * that "runs locally", we need to call the
		 * local close routine, even if we're a dc.
		 */
		if (VF_IS_VNODE(vfp)) {
			vnode_t		*vp;

			vp = VF_TO_VNODE(vfp);

			if (SPECFS_IS_LOCAL_DEVICE(vp))
				rval = VFILE_ISLASTCLOSE;
			else
				rval = VFILE_SKIPCLOSE | VFILE_ISLASTCLOSE;

		} else {
			rval = VFILE_SKIPCLOSE | VFILE_ISLASTCLOSE;
		}
	} else if (flag == VFILE_SECONDCLOSE) {
		vfp->vf_count--;
		rval = 0;
	} else {
		rval = 0;
	}

	VFUNLOCK(vfp, s);
	DCFILETAB_UNLOCK(vq);

	return(rval);
}

/*
 * Import the vnode/vsock associated with this vfile
 */
void
vfile_import_node(
	vfile_t	*vfp,
	obj_manifest_t *mftp)
{
	/* REFERENCED */
	int		error;
	vnode_t		*vp;
	/* REFERENCED */
	vsock_t		*vsop;

	if (VF_IS_VNODE(vfp)) {
		error = obj_mft_get (mftp, (void **)&vp);
		ASSERT(error == 0);
		ASSERT(vp);
		if (VF_TO_VNODE(vfp) == NULL) {
			VF_SET_DATA (vfp, vp);
		} else {
			ASSERT(VF_TO_VNODE(vfp) == vp);
			/*
			 * obj_mft_get caused a new hold on vp
			 * for an existing vfile - which is not needed
			 * because the vfile count has also been incremented
			 */
			VN_RELE(vp);
		}
	} else {
		ASSERT(VF_IS_VSOCK(vfp));
		error = obj_mft_get (mftp, (void **)&vsop);
		ASSERT(error == 0);
		ASSERT(vsop);
		if (VF_TO_VSOCK(vfp) == NULL) {
			VF_SET_DATA (vfp, vsop);
		} else {
			ASSERT(VF_TO_VSOCK(vfp) == vsop);
			/*
			 * obj_mft_get caused a new hold on vsop
			 * for an existing vfile - which is not needed
			 * because the vfile count has also been incremented
			 */
			(void)vsock_drop_ref(vsop);
		}
	}
}

void
dcfile_create (
	dcfile_t **dcfile,
	vfile_t	 *vfp,
	obj_handle_t *handle)
{
	dcfile_t *dcfp;

	dcfp = kmem_zone_alloc (dcfile_zone, KM_SLEEP);
	ASSERT(dcfp != NULL);

	if (vfp == NULL) {
		vfp = vfile_create(0);
		bhv_desc_init(&dcfp->dcf_bd, dcfp, vfp, &dcfile_ops);
		bhv_insert_initial(&vfp->vf_bh, &dcfp->dcf_bd);
	} else {
		bhv_desc_init(&dcfp->dcf_bd, dcfp, vfp, &dcfile_ops);
		bhv_insert(&vfp->vf_bh, &dcfp->dcf_bd);
	}

	mutex_init(&dcfp->dcf_offlock, MUTEX_DEFAULT, "flaglock");
	dcfp->dcf_offset_valid = 0;
#if DEBUG
	atomicAddLong(&dcfile_outstanding, 1);
#endif
	
	/*
	 * create client with DFILE_EXISTENCE_TOKENSET held
	 */
	tkc_create("dcfile", dcfp->dcf_tclient, dcfp, 
		&dcfile_tclient_iface,
	  	DFILE_NTOKENS, DFILE_EXISTENCE_TOKENSET, 
		DFILE_EXISTENCE_TOKENSET, (void *)handle->h_objid);

	dcfp->dcf_handle = *handle;
	dcfile_insert_handle(&dcfp->dcf_handle, dcfp);

	*dcfile = dcfp;
	VFILE_DBG(dcfile_created);
}

/*
 * Insert DC into list
 */
void
dcfile_insert_handle (
	obj_handle_t *handle, 
	dcfile_t *dcfp)
{
	dcfiletab_t	*vq;
	int		hq = DCFILE_HASH(*handle);

        vq = &dcfiletab[hq];
	DCFILETAB_LOCK(vq, MR_UPDATE);
#if DEBUG
	{
	dcfile_t	*vdcfp;
        dcfile_qlookup(vq, handle, &vdcfp);
	ASSERT(vdcfp == NULL);
	}
#endif
	kqueue_enter(&vq->dct_queue, &dcfp->dcf_kqueue);
	DCFILETAB_UNLOCK(vq);
}

/*
 * Find a dcfile structure with a given id.
 * Returns with the vfile VFLOCK held
 */
int
dcfile_lookup_handle(
	obj_handle_t *handle, 
	dcfile_t **dcfilep)
{
	dcfiletab_t	*vq;
	dcfile_t	*dcfile;
	int		hq = DCFILE_HASH(*handle);

	*dcfilep = NULL;

	/*
	 * Look for a match in the appropriate hash queue.
	 * Returns with the vfile VFLOCK held
	 */
	vq = &dcfiletab[hq];
	DCFILETAB_LOCK(vq, MR_ACCESS);
	dcfile_qlookup(vq, handle, &dcfile);
	DCFILETAB_UNLOCK(vq);

	if (dcfile == NULL)
		return(1);

	*dcfilep = dcfile;
	return(0);
}

void
dcfile_qlookup(
	dcfiletab_t	*vq,
	obj_handle_t	*handle,
	dcfile_t	**dcfilep)
{
	dcfile_t *dcfile;
	kqueue_t *kq;
	vfile_t	 *vfp;

	/* Get the hash queue */
	kq = &vq->dct_queue;
	/*
	 * Step through the hash queue looking for the given id
	 */
	for (dcfile = (dcfile_t *)kqueue_first(kq);
	     dcfile != (dcfile_t *)kqueue_end(kq);
	     dcfile = (dcfile_t *)kqueue_next(&dcfile->dcf_kqueue)) {
		if (HANDLE_EQU(*handle, dcfile->dcf_handle)) {
			/*
			 * Got it.  Take a reference.
			 */
			vfp = DCFILE_TO_VFILE(dcfile);
			VFILE_REF_HOLD(vfp);
			*dcfilep = dcfile;
			return;
		}
	}
	/*
	 * Can't find it, return NULL
	 */
	*dcfilep = NULL;
}

void
dcfile_cell_init()
{
        int             i;
        dcfiletab_t       *dcq;

	dcfile_zone = kmem_zone_init(sizeof(struct dcfile), "dcfile table");
        dcfiletabsz = 128;
        dcfiletab = (dcfiletab_t *)kern_malloc(sizeof(dcfiletab_t) * dcfiletabsz);
        ASSERT(dcfiletab != 0);
        for (i = 0; i < dcfiletabsz; i++) {
                dcq = &dcfiletab[i];
                kqueue_init(&dcq->dct_queue);
                mrinit (&dcq->dct_lock, "dct");
        }
}

void
vfile_cell_init()
{
	dcfile_cell_init();
	dsfile_cell_init();

	mesg_handler_register(dcfile_msg_dispatcher, DCFILE_SUBSYSID);
	mesg_handler_register(dsfile_msg_dispatcher, DSFILE_SUBSYSID);
}

void
dcfile_migrate_destroy(dcfile_t *dcfp)
{
	dcfiletab_t	*vq;
	int		hq = DCFILE_HASH(dcfp->dcf_handle);

	vq = &dcfiletab[hq];
	DCFILETAB_LOCK(vq, MR_UPDATE);
	ASSERT(!kqueue_isnull(&dcfp->dcf_kqueue));
	kqueue_remove(&dcfp->dcf_kqueue);
	kqueue_null(&dcfp->dcf_kqueue);
	DCFILETAB_UNLOCK(vq);

	tkc_free(dcfp->dcf_tclient);
	mutex_destroy(&dcfp->dcf_offlock);
	kmem_zone_free (dcfile_zone, dcfp);
#if DEBUG
	atomicAddLong(&dcfile_outstanding, -1);
#endif

	return;
}

#if DEBUG
long vtp1, vtp2, vtp3, vtp4, vtp5, vtp6, vtp7, vtp8, vtp9, vtp91;
#endif
/* ARGSUSED */
int
vfile_obj_target_prepare(
	obj_manifest_t	*mftp,			/* IN object manifest */
	void		**v)			/* OUT virtual object */
{
	int		error = 0;
	vfile_t		*vfp;
	service_t	svc;
	/* REFERENCED */
	dcfile_t 	*dcfp;
	dsfile_t 	*dsfp = NULL;
	tk_set_t        granted, already;
	cell_t          mycell = cellid();
	off_t		offset;
	int		flags;
	tk_set_t        refused;
	int		try_cnt = 0;
	vfile_ei_bag1_t	ei_bag;
	obj_mft_info_t	minfo;
	obj_handle_t	server_handle;
	cell_t		source_cell;
	/* REFERENCED */
	int		msgerr;

	minfo.source.tag = VFILE_BAG_TAG;
	minfo.target.tag = OBJ_TAG_NONE;
	minfo.source.infop = &ei_bag;
	minfo.source.info_size =  sizeof(ei_bag);
	obj_mft_info_get(mftp, &minfo);
	source_cell = minfo.source_cell;

	while (1) {

		if (SERVICE_EQUAL(
		    HANDLE_TO_SERVICE(ei_bag.server_handle), dsfile_service)) {
			/*
			 * Search for handle in server
			 */
			if (dsfile_reference_handle (&ei_bag.server_handle, 
			    &vfp)) {
				VFILE_DBG(vtp1);
				ASSERT(minfo.rmode == OBJ_RMODE_REFERENCE);
				VFILE_DBG(dsfile_found);
				goto done;
			}
		}

		if (ei_bag.existence_token_granted) {
	
			if (minfo.rmode & OBJ_RMODE_SERVER) {
				/* 
				 * The DS gave us the existence token - 
				 * create a new DS
				 */
				VFILE_DBG(vtp2);
				vfp = vfile_create (0);
				BHV_WRITE_LOCK(&vfp->vf_bh);
				dsfp = dsfile_interpose(vfp, source_cell);
			} else {
				/* 
				 * The DS gave us the existence token -
				 * create a new DC
				 */
				VFILE_DBG(vtp9);
	
				dcfile_create(&dcfp, NULL, 
					&ei_bag.server_handle);
				vfp = DCFILE_TO_VFILE(dcfp);
				vfp->vf_flag = ei_bag.server_vfile.vf_flag;

			}

			break;
		} else if (dcfile_lookup_handle(&ei_bag.server_handle, &dcfp) == 0) {
			/* already on this site */
			vfp = DCFILE_TO_VFILE(dcfp);
			VFILE_DBG(vtp3);
			if (minfo.rmode & OBJ_RMODE_SERVER) {
				VFILE_DBG(vtp4);
				/*
				 * Create new server
				 * Old client gets removed later
				 */
				BHV_WRITE_LOCK(&vfp->vf_bh);
				dsfp = dsfile_interpose(vfp, source_cell);

			}
	
			break;
		} else {
			/*
			 * get the token from the DS
			 */
			if (!SERVICE_EQUAL(
			    HANDLE_TO_SERVICE(ei_bag.server_handle), 
			    dsfile_service)) {
				msgerr = invk_dsfile_obtain(
				  HANDLE_TO_SERVICE(ei_bag.server_handle),
				  mycell, 
				  DFILE_EXISTENCE_TOKENSET,
				  TK_NULLSET, 
				  0,
				  &already, 
				  &granted, 
				  &refused,
				  &ei_bag.server_handle,
				  &offset,
				  &flags,
				  &error);
				ASSERT(!msgerr);
			} else {
				/* Force update */
				error = ENOENT;
			}

			if (error == EMIGRATED) {
				VFILE_DBG(vtp5);
				error = 0;
				continue;
			} else if (error == ENOENT) {
				/*
				 * ask source cell for an updated handle
				 */
				VFILE_DBG(vtp8);
				SERVICE_MAKE (svc, source_cell, SVC_VFILE);
				msgerr = invk_dsfile_update_handle (svc,
					ei_bag.v,
					&ei_bag.server_handle);
				ASSERT(!msgerr);
				error = 0;
				continue;
			} else if (error) {
				VFILE_DBG(vtp7);
				goto done;
			}

			if (TK_IS_IN_SET(DFILE_EXISTENCE_TOKENSET, granted)) {
				/*
				 * This import got the existence token
				 * create the DC using the code above
				 */
				VFILE_DBG(dcfile_obtained);
				ei_bag.existence_token_granted = 1;
				ei_bag.server_vfile.vf_flag = flags;
				continue;
			}

			/*
			 * Backoff and retry everything again
			 */
			VFILE_DBG(vtp91);
			cell_backoff(++try_cnt);
		}
	}

done:

	if (error)
		return error;

	SERVICE_MAKE(svc, cellid(), SVC_VFILE);
	HANDLE_MAKE(minfo.target.hndl, svc, vfp);
	if (minfo.rmode & OBJ_RMODE_SERVER) {
		VFILE_DBG(vtp6);
		HANDLE_MAKE(server_handle, svc, dsfp);
	} else {
		HANDLE_MAKE_NULL(server_handle);
	}
	minfo.target.tag = VFILE_BAG_TAG;
	minfo.target.infop = &server_handle;
	minfo.target.info_size = sizeof(server_handle);
	obj_mft_info_put(mftp, &minfo);
	vfile_import_node(vfp, mftp);
	*v = vfp;
	return error;
}

#if DEBUG
long vtu1, vtu2, vtu3, vtu4;
#endif
/* ARGSUSED */
int
vfile_obj_target_unbag(
	obj_manifest_t	*mftp,			/* IN object manifest */
	obj_bag_t	bag)
{
	vfile_t		*vfp;
	dsfile_t	*dsfp;
	dcfile_t	*dcfp = NULL;
	vfile_ei_bag2_t	ei_bag;
	/* REFERENCED */
	int		error;
	int		s;
	obj_mft_info_t	minfo;
	bhv_desc_t	*bdp;

	minfo.source.tag = OBJ_TAG_NONE;
	minfo.target.tag = OBJ_TAG_NONE;
	obj_mft_info_get(mftp, &minfo);
	vfp = (vfile_t *)HANDLE_TO_OBJID(minfo.target.hndl);

	if (minfo.rmode == OBJ_RMODE_REFERENCE) {
		VFILE_DBG(vtu1);
		return(0);
	}
	VFILE_DBG(vtu2);
	bdp = BHV_HEAD_FIRST(&vfp->vf_bh);
	if (BHV_POSITION(bdp) == VFILE_POSITION_DC) {
		/* Is DC now */
		dcfp = (dcfile_t *)BHV_PDATA(bdp);
		bdp = BHV_NEXT(bdp);
		ASSERT (BHV_POSITION(bdp) == VFILE_POSITION_DS);
		dsfp = (dsfile_t *)BHV_PDATA(bdp);
	} else {
		dsfp = VFILE_TO_DSFILE(vfp);
	}

	/*
	 * unbag the token state into the new server
	 */
	tks_unbag(dsfp->dsf_tserver, bag);

	/*
	 * unbag the rest of the object state and install correctly
	 */
	obj_bag_get_here(bag, VFILE_BAG_TAG,
			 &ei_bag, sizeof(ei_bag), error);
	ASSERT(error == 0);
	
	/*
	 * Create pfile, insert into V chain as first entry
	 *
	 * If this DC currently has the offset token, update the pfile
	 * offset.
	 */

	if (dcfp) {
		ASSERT(!mutex_owned(&dcfp->dcf_offlock));
		if (dcfp->dcf_offset_valid) {
			ei_bag.offset = dcfp->dcf_offset;
		}
	}
	pfile_target_migrate (vfp, ei_bag.offset);
	vfp->vf_flag = ei_bag.server_vfile.vf_flag;

	/*
	 * This object owns all the old DS references
	 * plus any existing local references
	 *
	 * A server relocate does not move a local reference
	 * (a reference relocate does)
	 */
	s = VFLOCK(vfp);
	vfp->vf_count += ei_bag.remote_ref_count;
	if (!(minfo.rmode & OBJ_RMODE_REFERENCE)) {
		vfp->vf_count--;
	}
	dsfp->dsf_remote_refs = ei_bag.remote_ref_count;
	VFUNLOCK(vfp, s);

/* What about other refs via lookup ? */

	if (dcfp) {
		VFILE_DBG(vtu3);
		/*
		 * old object was a DC
		 * copy client tokens, remove DC, then destroy
		 */
		tkc_copy (dcfp->dcf_tclient, dsfp->dsf_tclient);
		bhv_remove(&vfp->vf_bh, &dcfp->dcf_bd);
		dcfile_migrate_destroy (dcfp);
	} else {
		VFILE_DBG(vtu4);
	}

	obj_SR_target_end(&dsfp->dsf_obj_state);
	BHV_WRITE_UNLOCK (&vfp->vf_bh);

	return(0);
}

/*
 * Find the client matching old_handle, and change it to use
 * the new handle.
 */
#if DEBUG
long dcfret1, dcfret2;
#endif
void
I_dcfile_retarget(
	obj_handle_t *new_handle,
	obj_handle_t *old_handle,
	void	*rt_sync,
	void	*error)
{
	dcfile_t *dcfp;
	vfile_t  *vfp;
	dcfiletab_t	*vq;
	int	hq;
	/* REFERENCED */
	int	err;
	int	s;
	/* REFERENCED */
	int	msgerr;

	if (dcfile_lookup_handle(old_handle, &dcfp) == 0) {
		vfp = DCFILE_TO_VFILE(dcfp);
		BHV_WRITE_LOCK (&vfp->vf_bh);

		/*
		 * Remove from old queue
		 */
		hq = DCFILE_HASH(dcfp->dcf_handle);
		vq = &dcfiletab[hq];
		DCFILETAB_LOCK(vq, MR_UPDATE);
		s = VFLOCK(vfp);
		ASSERT(!kqueue_isnull(&dcfp->dcf_kqueue));
		kqueue_remove(&dcfp->dcf_kqueue);
		kqueue_null(&dcfp->dcf_kqueue);
		VFUNLOCK(vfp, s);
		DCFILETAB_UNLOCK(vq);

		/*
		 * set new handle, add to new queue
		 */
		dcfp->dcf_handle = *new_handle;
		dcfile_insert_handle(&dcfp->dcf_handle, dcfp);

		BHV_WRITE_UNLOCK (&vfp->vf_bh);
		VFILE_REF_RELEASE(vfp);
		VFILE_DBG(dcfret1);
		msgerr = invk_dsfile_obj_retargetted (
				HANDLE_TO_SERVICE(*old_handle), cellid(), 
				rt_sync, new_handle, NULL);
		ASSERT(!msgerr);
	} else {
		VFILE_DBG(dcfret2);
		/*
		 * Cause DS to re-try the retarget
		 */
		msgerr = invk_dsfile_obj_retargetted (
				HANDLE_TO_SERVICE(*old_handle), cellid(), 
				rt_sync, new_handle, error);
		ASSERT(!msgerr);
	}
}
