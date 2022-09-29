/**************************************************************************
 *									  *
 *		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident	"Revision: $"

#include "dv.h"
#include "dvfs.h"
#include <ksys/cell/mesg.h>
#include <ksys/cell/subsysid.h>
#include "I_dsvfs_stubs.h"
#include "I_dcvfs_stubs.h"

/************************************************************************
 * Static data for VFS                                                  *
 ************************************************************************/
cfs_gen_t       dsvfs_gen_num;          /* gen. # for vfs handles */
zone_t          *dcvfs_zone;            /* dcvfs zone */
zone_t          *dsvfs_zone;            /* dsvfs zone */
handle_hashtab_t dcvfs_head;            /* dcvfs active list */

#ifdef	CELL_IRIX
cell_t          rootfs_cell;            /* cell serving root file system */
vfs_t           **dvfs_dummytab;        /* Table of dummy vfs's */
dsvfs_t         **dsvfs_dummytab;       /* Table of dummy dsvfs's */
dcvfs_t         **dcvfs_dummytab;       /* Table of dummy dcvfs's */
#endif	/* CELL_IRIX */



/************************************************************************
 * General dvfs routines						*
 ************************************************************************/

/*
 * dvfs_init()
 */
void
dvfs_init()
{
	handle_hashtab_t        *hp;

	hp = &dcvfs_head;
	kqueue_init(&hp->hh_kqueue);
	spinlock_init(&hp->hh_lock, "dcvfs");
	dcvfs_zone = kmem_zone_init(sizeof(dcvfs_t), "dcvfs");
	dsvfs_zone = kmem_zone_init(sizeof(dsvfs_t), "dsvfs");
	dsvfs_gen_num = 0;
#if CELL_IRIX
	rootfs_cell = golden_cell;
#endif

	mesg_handler_register(dcvfs_msg_dispatcher, DCVFS_SUBSYSID);
	mesg_handler_register(dsvfs_msg_dispatcher, DSVFS_SUBSYSID);
}


/*
 * cfs_vfsexport()
 *
 * 3 cases
 * - A client cell exports handle.
 * - A server with existing clients exports a handle.
 * - A server with no existing clients exports an object for the first time
 * to a prospective client cell.
 * 
 * Caller responsible for keeping the vfs around for the duration of the call.
 * Since the vfs doesn't go away and we know that dc,ds layers get torn down 
 * only during unmount, therefore there's no need to hold the read lock
 * on the chain. The write lock is needed to install the ds layer.
 *
 */
void
cfs_vfsexport(
	vfs_t 		*vfsp, 			/* vfs to be exported */
	cfs_handle_t 	*vfs_hdlp)		/* export handle */
{
	bhv_head_t	*vfs_bhvhead = VFS_BHVHEAD(vfsp);
	dsvfs_t		*dsp;
	bhv_desc_t	*newbdp;

	if (VFSOPS_IS_DCVFS(VFS_FOPS(vfsp))) {
		dcvfs_t		*dcp;

		/* client trying to export handle, simple case */
		dcp = BHV_TO_DCVFS(vfsp->vfs_fbhv);
		*vfs_hdlp = dcp->dcvfs_handle;
		return;
	}

	if (VFSOPS_IS_DSVFS(VFS_FOPS(vfsp))) {
		/* server with existing clients exports a handle */
		dsp = BHV_TO_DSVFS(vfsp->vfs_fbhv);
		DSVFS_MKHANDLE(*vfs_hdlp, dsp);
		return;
	}

	/*
         * Special handling for local sypport client interposers such as are
         * used by procfs.
	 */
	if (VFS_FOPS(vfsp)->vf_position.bi_position == VFS_POSITION_DIST_LC) {
	  	bhv_desc_t      *bdp = vfsp->vfs_fbhv->bd_next;
		dcvfs_t		*dcp;

		dcp = BHV_TO_DCVFS(bdp);
		*vfs_hdlp = dcp->dcvfs_handle;
		return;
	}		

	/*
	 * Special handling for dummy vfs's
	 */
#if CELL_IRIX 
	if (dcvfs_dummytab[vfsp->vfs_fstype] != NULL) {
		dcvfs_t 	*dcp = dcvfs_dummytab[vfsp->vfs_fstype];

		*vfs_hdlp = dcp->dcvfs_handle;
		return;
	}		
#endif /* CELL_IRIX */


	/* create the ds vfs behavior for this vfs */
	dsp = dsvfs_allocate(vfsp);
	newbdp = &dsp->dsvfs_bhv;
	tks_create("dsvfs", dsp->dsvfs_tserver, dsp, &dsvfs_tsif,
			DVFS_NTOKENS, (void *)dsp);
	tkc_create_local("dsvfs", dsp->dsvfs_tclient, dsp->dsvfs_tserver,
			DVFS_NTOKENS, TK_NULLSET,
			TK_NULLSET, (void *)dsp); 
	
	bhv_desc_init(newbdp, dsp, vfsp, &cfs_dsvfsops);

	
	BHV_WRITE_LOCK(vfs_bhvhead);
	/* check again in case there's a race */
	if (VFSOPS_IS_DSVFS(VFS_FOPS(vfsp))) {
		BHV_WRITE_UNLOCK(vfs_bhvhead);
		/* clean up all data structures  since we lost the race */
		tks_destroy(dsp->dsvfs_tserver);
		tkc_destroy_local(dsp->dsvfs_tclient);
		kmem_zone_free(dsvfs_zone, dsp);

                /* some other cpu already installed the ds behavior */
                dsp = BHV_TO_DSVFS(vfsp->vfs_fbhv);
                DSVFS_MKHANDLE(*vfs_hdlp, dsp);
                return;
        }

	/* server exporting a vfs handle for the 1st time */	
	/* install the new ds vfs behavior */
	if (!bhv_insert(vfs_bhvhead, newbdp)) {
		BHV_WRITE_UNLOCK(vfs_bhvhead);
		dsp->dsvfs_gen = atomicAddUint(&dsvfs_gen_num, 1);
                DSVFS_MKHANDLE(*vfs_hdlp, dsp);
	}
	else {
		BHV_WRITE_UNLOCK(vfs_bhvhead);
		panic("dvfs_exporthdl");
	}
}

#if CELL_IRIX

/*
 * Take note of dummy vfs.
 * 
 * We need to take note of dummy vfs's constructed during initialization.
 * When basic initialization is completed, we need to construct a dcvfs
 * possibly a dsvfs for each of these dummy vfs's.
 */
void 
cfs_dummy_vfs(
	vfs_t 		*vfsp)
{
  	if (dvfs_dummytab == NULL) {
	        ASSERT(dcvfs_dummytab == NULL);
	        ASSERT(dsvfs_dummytab == NULL);
		dvfs_dummytab = kmem_zalloc(sizeof(vfs_t *) * nfstype, 
					    KM_NOSLEEP);
		dcvfs_dummytab = kmem_zalloc(sizeof(dcvfs_t *) * nfstype, 
					    KM_NOSLEEP);
		dsvfs_dummytab = kmem_zalloc(sizeof(dsvfs_t *) * nfstype, 
					    KM_NOSLEEP);
		if (dvfs_dummytab == NULL ||
		    dcvfs_dummytab == NULL ||
		    dsvfs_dummytab == NULL)
			panic("could not allocate dummy vfs table");
	}
	ASSERT(vfsp->vfs_fstype < nfstype);
	ASSERT(dvfs_dummytab[vfsp->vfs_fstype] == NULL);
	dvfs_dummytab[vfsp->vfs_fstype] = vfsp;
}

#endif /* CELL_IRIX */

