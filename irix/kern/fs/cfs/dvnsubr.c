/**************************************************************************
 *									  *
 *		 Copyright (C) 1995-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident	"$Id: dvnsubr.c,v 1.89 1997/08/30 17:00:00 mostek Exp $"

/*
 * Vnode-related subroutines for the cell file system.
 */
#include "dvn.h"
#include "dvfs.h"
#include "cfs_relocation.h"
#include "invk_dsvn_stubs.h"
#include <sys/pvnode.h>
#include <sys/atomic_ops.h>
#include <sys/fs_subr.h>
#ifdef	DEBUG
#include <sys/idbgentry.h>
#endif	/* DEBUG */

#include <ksys/cell/mesg.h>
#include <ksys/cell/subsysid.h>
#include "I_dsvn_stubs.h"
#include "I_dcvn_stubs.h"
#include <fs/cell/cxfs_intfc.h>

#include <fs/specfs/spec_lsnode.h>

/*
 * This file is split into three sections based on type of functionality:
 * - applies to both client and server sides
 * - applies to client-side only
 * - applies to server-side only
 */

#if DVN_TRACING
ktrace_t	*dvn_trace_buf;	/* tracing dc and ds activity on all cells */
int		dvn_trace_mask = DVN_TRACE_ALL;
#endif

/*
 ********************* Client AND Server functionality **********************  
 */

void
dvn_init(void)
{
	handle_hashtab_t	*hp;
	int			i;

	/*
	 * Initialize hash tables and zones.
	 */
	for (i = 0; i < DCVN_NHASH; i++) {
		hp = &dcvn_hashtab[i];
		kqueue_init(&hp->hh_kqueue);
		spinlock_init(&hp->hh_lock, "dcvn");
	}
	for (i = 0; i < DSVN_NHASH; i++) {
		hp = &dsvn_hashtab[i];
		kqueue_init(&hp->hh_kqueue);
		spinlock_init(&hp->hh_lock, "dsvn");
	}
	dcvn_zone = kmem_zone_init(sizeof(dcvn_t), "dcvn");
	dsvn_zone = kmem_zone_init(sizeof(dsvn_t), "dsvn");
	dsvn_gen_num = 0;

	/*
	 * Initialize the dsvn ops vector.  Mostly passthru's.
	 */
	bcopy((caddr_t)vn_passthrup, (caddr_t)&dsvn_ops, 
	      sizeof(vnodeops_t));
	dsvn_ops.vn_position.bi_id = VN_BHV_CFS;
	dsvn_ops.vn_position.bi_position = VNODE_POSITION_CFS_DS;
	dsvn_ops.vop_read = dsvn_read;
	dsvn_ops.vop_write = dsvn_write;
	dsvn_ops.vop_getattr = dsvn_getattr;
	dsvn_ops.vop_setattr = dsvn_setattr;
	dsvn_ops.vop_create = dsvn_create;
	dsvn_ops.vop_remove = dsvn_remove;
	dsvn_ops.vop_link = dsvn_link;
	dsvn_ops.vop_rename = dsvn_rename;
	dsvn_ops.vop_mkdir = dsvn_mkdir;
	dsvn_ops.vop_rmdir = dsvn_rmdir;
	dsvn_ops.vop_readdir = dsvn_readdir;
	dsvn_ops.vop_symlink = dsvn_symlink;
	dsvn_ops.vop_readlink = dsvn_readlink;
	dsvn_ops.vop_inactive = dsvn_inactive;
	dsvn_ops.vop_allocstore = dsvn_allocstore;
	dsvn_ops.vop_fcntl = dsvn_fcntl;
	dsvn_ops.vop_reclaim = dsvn_reclaim;
	dsvn_ops.vop_link_removed = dsvn_link_removed;
	dsvn_ops.vop_vnode_change = dsvn_vnode_change;
	dsvn_ops.vop_tosspages = dsvn_tosspages;
	dsvn_ops.vop_flushinval_pages = dsvn_flushinval_pages;
	dsvn_ops.vop_flush_pages = dsvn_flush_pages;
	dsvn_ops.vop_invalfree_pages = dsvn_invalfree_pages;
	dsvn_ops.vop_pages_sethole = dsvn_pages_sethole;


#if DVN_TRACING
	dvn_trace_buf = ktrace_alloc(DVN_TRACE_SIZE, 0);
#endif	

	mesg_handler_register(dcvn_msg_dispatcher, DCVN_SUBSYSID);
	mesg_handler_register(dsvn_msg_dispatcher, DSVN_SUBSYSID);
}

/*
 * Map from a vnode to a cfs handle.  The returned handle does not
 * have an associated existence token, so the caller of cfs_vnimport must
 * be prepared to handle the case that the object has migrated, in which 
 * case the handle doesn't map to anything.
 */
void
cfs_vnexport(
        vnode_t		*vp,
        cfs_handle_t	*handlep)	/* out */
{
	dsvn_t		*dsp;
        bhv_desc_t      *bdp;

	DCSTAT(cfs_vnexport);
	DSSTAT(cfs_vnexport);

	/*
	 * If it's a client-side CFS vnode, then just get the handle from
	 * the dcvn.  Otherwise, must get the handle associated with a dsvn, 
	 * creating one if necessary.
	 *
	 * Take behavior chain read lock to prevent it from changing out 
	 * from under us.  Note that dsvn_vnode_to_dsvn_nolock returns
	 * with the chain unlocked.
	 */
	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp));
        bdp = VNODE_TO_FIRST_BHV(vp);
	DVN_KTRACE4(DVN_TRACE_MISC, "cfs_vnexport", vp, "handle", handlep);
	if (BHV_IS_DCVN(bdp)) {	
		*handlep = DCVN_TO_HANDLE(BHV_TO_DCVN(bdp));
		VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));
	} else if (BHV_IS_DSVN(bdp)) {
		dsp = BHV_TO_DSVN(bdp);
		VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));
		DSVN_HANDLE_MAKE(*handlep, dsp);
	} else {
                bdp = vn_bhv_lookup(VN_BHV_HEAD(vp), &cfs_vnodeops);
		if (bdp == NULL) {
			dsp = dsvn_vnode_to_dsvn_nolock(vp);
			DSVN_HANDLE_MAKE(*handlep, dsp);
		} else {
		        *handlep = DCVN_TO_HANDLE(BHV_TO_DCVN(bdp));
			VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));
		}
	}
}

/*
 * Map from a cfs handle to a referenced vnode, allocating one if
 * necessary.  If the returned vnode is a client-side representation
 * for a remote file, then upon completion of this routine there will 
 * be a existence token held for the file. 
 *
 * The export/import protocol is such that the provider of the handle
 * (typically cfs_vnexport) doesn't guarantee that the server-side vnode 
 * won't be torn down or migrate prior to cfs_vnimport being called.  
 * If the returned vp is NULL then the object has been torn down or migrated.  
 * Otherwise, the vp has a new reference and the caller is responsible for 
 * releasing it.
 */
void
cfs_vnimport(
        cfs_handle_t	*handlep,
        vnode_t		**vpp)		/* out */
{
	dcvn_t		*dcp;
	dsvn_t		*dsp;


	DCSTAT(cfs_vnimport);
	DSSTAT(cfs_vnimport);
	/*
	 * Look in the handle to see if we're the server managing it.
	 * If so, then the vnode will exist locally - unless it's been
	 * migrated, in which case dsvn_handle_lookup returns NULL.
	 */
	if (SERVICE_EQUAL(CFS_HANDLE_TO_SERVICE(*handlep), cfs_service_id)) {

		if ((dsp = dsvn_handle_lookup(handlep)) != NULL) {

						/* lookup took a ref */
			*vpp = DSVN_TO_VNODE(dsp);

		} else {
			*vpp = NULL;			/* migrated */
		}

		return;
	} 

	/* 
	 * We're not the server, so must be a client.  Check in 
	 * the client-side hash table to see if the vnode already
	 * exists.
	 */
	dcp = dcvn_handle_lookup(handlep);

	if (dcp == NULL)
		dcp = dcvn_import_force(handlep);

	if (dcp) {
		vnode_t	*nvp;
		vnode_t	*vp;


		*vpp = vp = DCVN_TO_VNODE(dcp);

		/*
		 * If the vnode is a special device give
		 * specfs a chance to insert its behavior
		 */
#if CELL_IRIX
		if (vp->v_type == VBLK || vp->v_type == VCHR) {

#ifdef  SPECFS_DEBUG
			printf(
			  "cfs_vnimport[%d]:%d vp/0x%x{%d}, rdev(%d/%d, %d)\n",
						cellid(), __LINE__,
						vp, vp->v_count,
						major(vp->v_rdev),
						emajor(vp->v_rdev),
						minor(vp->v_rdev));
#endif  /* SPECFS_DEBUG */

			nvp = spec_vnimport(vp, get_current_cred(), 1);

			ASSERT(nvp);
			ASSERT(nvp == vp);
			ASSERT(nvp->v_count > 1);

				/* Release xtra ref taken by spec */
			VN_RELE(nvp);

			*vpp = nvp;
		}
#endif /* CELL_IRIX */

	} else {
	        *vpp = NULL;
	}

	DVN_KTRACE4(DVN_TRACE_MISC, "cfs_import", dcp, "handle", handlep);
}

/*
 * Given a vnode return the cell id of the vnode's server.
 *
 * If the vnode is managed locally then the id of the local cell 
 * is returned.
 */
cell_t
cfs_vnode_to_cell(
        vnode_t		*vp)
{
	dcvn_t		*dcp;
	cell_t		cell;

	dcp = VNODE_TO_DCVN(vp);
	if (dcp) 
		cell = SERVICE_TO_CELL(DCVN_TO_SERVICE(dcp));
	else
		cell = cellid();
	DVN_KTRACE4(DVN_TRACE_MISC, "cfs_vnode_to_cell", dcp, "cell", cell);
	return(cell);
}
		

/*
 ********************* Client-side functionality only ***********************  
 */

dcvn_t *
dcvn_import_force(
        cfs_handle_t	*handlep)
{
	dcvn_t		*newdcp;
	tk_set_t	obtset;
	vfs_t		*vfsp;
	enum vtype	vtype;
	dev_t		rdev;
	cfs_handle_t	vfs_handle;
	int		try_cnt, flags, error;
	int		cxfs_flags;
	char 		*cxfs_buff;
	size_t		cxfs_count;

	DCSTAT(dcvn_import_force);
	/*
	 * Allocate a dcvn in advance.  When we begin caching attributes this
	 * will allow the create RPC to deposit attributes in their final
	 * destination.  The call to vn_alloc occurs in dcvn_newdc_setup.
	 */
	newdcp = kmem_zone_alloc(dcvn_zone, KM_SLEEP);
	newdcp->dcv_handle = *handlep;
	newdcp->dcv_dcxvn = NULL;
	spinlock_init(&newdcp->dcv_lock, "dcv_lock");

	/*
	 * Send an obtain_exist RPC to the server.
	 *
	 * Note that a thread doing a lookup, or a thread returning the
	 * existence token, racing against us obtaining, requires that this
	 * code loop, checking a hash table and contacting the server.  
	 * Backoff each time through the loop.
	 *
	 * Another race:  if the object has been torn down or is being
	 * migrated, then our attempt to obtain an existence token from 
	 * (what we think is) the server will return DVN_NOTFOUND.
	 * In that case, this routine returns a NULL vp and it's the
	 * caller's repsonsibility to handle the race.
	 */
	try_cnt = 0;
	do {
		dcvfs_t		*dcvfsp;
		/* REFERENCED */
		int		msgerr;

		DVN_KTRACE6(DVN_TRACE_MISC, "dcvn_import_force", handlep,
			"try_cnt",  try_cnt, "newdcp", newdcp);
		cxfs_count = 0; cxfs_buff = NULL; cxfs_flags = 0;
		/*
		 * Send the RPC.
		 */
		msgerr = invk_dsvn_obtain_exist(CFS_HANDLE_TO_SERVICE(*handlep),
				       handlep, cellid(), &obtset,
				       &vtype, &rdev, &flags, &newdcp->dcv_vattr,
				       &cxfs_flags, &cxfs_buff, &cxfs_count,
				       &vfs_handle, &error);
		ASSERT(!msgerr);

		/* JIMJIM what else if error, return tokens? */
		if (error)
			printf("dcvn_import_force: error %d from invk_dsvn_obtain_exist\n",
				error);

		if (flags & DVN_NOTFOUND || error) {
			/* object has been torn down, migrated, or corrupted */
			ASSERT(newdcp != NULL);
			spinlock_destroy((&newdcp->dcv_lock));
			kmem_zone_free(dcvn_zone, newdcp);
			return(NULL);
		}

		/* 
		 * Get the file system if we are going to have to install
		 * the vnode.  If we don't have the existence token, it's
		 * not our job to get the file system and dcvn_handle_to_dcvn
		 * will not need dcvfsp.
		 */
		if (obtset & DVN_EXIST_READ) {

	                bhv_desc_t      *bdp_dcvfs;

		        /*
		         * Get the vfs and dcvfs associated with this vnode. 
		         * Locking the behavior chain might be dispensed with 
		         * if we could be sure that a vfs chain with a dcvfs in 
			 * it could never change once it was set up.  I know of 
			 * no contrary case but do not think it is worth fore-
                         * closing it here.  Note that previously we assumed 
                         * (in VFS_TO_DCVFS) that the chain was static and
		         * consisted only of the dcvfs.
                         *
                         * There is special handling to deal with distributed 
                         * fs's such as procfs in which a vnode served on another
                         * cell is in a file system served on this one. 
		         */
		        if (SERVICE_EQUAL(CFS_HANDLE_TO_SERVICE(vfs_handle), 
					  cfs_service_id)) {
			       dsvfs_t	*dsvfs;

			       dsvfs = (dsvfs_t *) CFS_HANDLE_TO_OBJID(vfs_handle);
			       vfsp = DSVFS_TO_VFS(dsvfs);
			}
			else 
		        	vfsp = dcvfs_import_mounthierarchy(&vfs_handle);  
		        bdp_dcvfs = bhv_lookup_unlocked(VFS_BHVHEAD(vfsp), 
							&cfs_vfsops);
			if (bdp_dcvfs == NULL) {
#if CELL_IRIX 
			        ASSERT(vfsp == dvfs_dummytab[vfsp->vfs_fstype]);
				dcvfsp = dcvfs_dummytab[vfsp->vfs_fstype];
				ASSERT(dcvfsp);
#else /* CELL_IRIX */
				panic("missing dcvfs");
#endif /* CELL_IRIX */
			}
			else
		        	dcvfsp = BHV_TO_DCVFS(bdp_dcvfs);
		} else {
			dcvfsp = NULL;
		}

		/*
		 * Map from a handle to a new dcvn.  If ERESTART is
		 * returned then we're racing and must retry.
		 *
		 * Note: this call can change the value of newdcp.
		 */
		error = dcvn_handle_to_dcvn(handlep, obtset, 
					    vtype, rdev, dcvfsp,
					    flags, cxfs_flags, cxfs_buff,
					    cxfs_count, &newdcp);

		ASSERT(error == 0 || error == ERESTART);
		if (!error)
			break;

		if (cxfs_count) {
			ASSERT(cxfs_buff);
			kmem_free((caddr_t)cxfs_buff, cxfs_count);
		}

		/* backoff to give others a chance to run */
		
		cell_backoff(++try_cnt);

		obtset = TK_NULLSET;		/* reset */
	} while (1);

	ASSERT(newdcp != NULL);
	ASSERT(!error);
	DVN_KTRACE4(DVN_TRACE_MISC, "import_force done", handlep, "newdcp", newdcp);
	return(newdcp);
}


/*
 * Given a handle and possibly a set of tokens (along with other info
 * about the vnode), return a new dcvn.
 *
 * This code takes care of looking in the dcvn hash table.  It also allocates
 * a new dcvn if necessary.
 *
 * Upon return, error may be either:
 * 	0 		=> success
 * 	ERESTART	=> caller must loop retrying its operation.
 *
 * Note: this routine can change the value of *newdcpp.
 */
int
dcvn_handle_to_dcvn(
	cfs_handle_t	*handlep,
	tk_set_t	obtset,
	enum vtype	vtype,
	dev_t		rdev,
	dcvfs_t		*dcvfsp,
	int		flags, 
	int		cxfs_flags, 
	char *		cxfs_buff, 
	size_t		cxfs_count, 
	dcvn_t		**newdcpp)	/* in/out */
{
	int		error;
	dcvn_t		*hashdcp;
	dcvn_t		*newdcp;
	vnode_t		*vp;

	ASSERT(newdcpp != NULL);
	newdcp = *newdcpp;

	vp = NULL;

	DVN_KTRACE8(DVN_TRACE_MISC, "dcvn_handle_to_dcvn", handlep, "obtset", obtset,
		"newdcp", newdcp, "dcvfsp", dcvfsp);
	/*
	 * If we got the existence token, then setup/install the dc.
	 * Otherwise, check the hash table.  If don't find it there, 
	 * then we're racing with another thread obtaining or returning, 
	 * so retry.
	 */
	dcvn_count_obt_opti(obtset);
	if (obtset & DVN_EXIST_READ) {
		if (*newdcpp == NULL) {
			*newdcpp = kmem_zone_alloc(dcvn_zone, KM_SLEEP);
			(*newdcpp)->dcv_handle = *handlep;
			(*newdcpp)->dcv_dcxvn  = NULL;
			newdcp = *newdcpp;
			spinlock_init(&newdcp->dcv_lock, "dcv_lock");
		}
				
		/*
		 * Setup first, including dealing with mount points.
		 *
		 * If this returns ERESTART, then there was a race and
		 * we must retry.
		 */
		error = dcvn_newdc_setup(newdcp, DCVFS_TO_VFS(dcvfsp),
					 vtype, rdev, flags);
		if (!error) {
			/*
			 * If this is XFS, create the new dcxvn stuff.
			 */
			if ((vtype == VREG) && DC_FS_IS_XFS(dcvfsp)) {
				newdcp->dcv_dcxvn = 
					cxfs_dcxvn_make((void *)newdcp,
						dcvfsp->dcvfs_xvfs,
						&newdcp->dcv_vattr,
						newdcp->dcv_tclient,
						cxfs_flags, cxfs_buff,
						cxfs_count);
			} else {
				ASSERT(!cxfs_count && !cxfs_flags);
				newdcp->dcv_dcxvn = NULL;
			}

			/*
			 * Install the new dc.
			 * Prior to this, install in dnlc (when we
			 * have name caching).
			 */

			dcvn_newdc_install(newdcp, obtset, dcvfsp);

			vp = DCVN_TO_VNODE(newdcp);
			DCSTAT(dcvn_handle_to_dcvn_NEW);

		} else {
			/* REFERENCED */
			int	msgerr;
			ds_times_t	times;

			/*
			 * Must return the token(s) on error.
			 */
			times.ds_flags = 0; /* No times possibly updated yet. */
			msgerr = invk_dsvn_return(
					CFS_HANDLE_TO_SERVICE(*handlep),
					CFS_HANDLE_TO_OBJID(*handlep),
					cellid(), obtset, TK_NULLSET,
					TK_DISP_CLIENT_ALL, &times);
			ASSERT(!msgerr);
		}
	} else {
		/*
		 * We didn't get the existence token, but a handle
		 * was returned as a hint.  See if we find it in the 
		 * hash table.
		 */
		ASSERT(obtset == TK_NULLSET);
		hashdcp = dcvn_handle_lookup(handlep);
		if (hashdcp == NULL) {
			/*
			 * Race: this means the vnode was recycled in
			 * the meantime, or the caller's RPC reply raced
			 * ahead of another reply, so we need to try again.
			 */
			error = ERESTART;
		} else {
			/*
			 * Found the vnode/dcvn in the hash table, and the
			 * lookup returned us a vref to it.
			 * Do the following:
			 * - release the previously allocated dcvn.
			 * - check if the vnode is newly covered
			 */
			if (*newdcpp != NULL) {
				spinlock_destroy(&((*newdcpp)->dcv_lock));
				kmem_zone_free(dcvn_zone, *newdcpp);
			}
			*newdcpp = hashdcp;

			vp = DCVN_TO_VNODE(hashdcp);

			error = 0;
			DCSTAT(dcvn_handle_to_dcvn_hsh);
			if ((flags & DVN_MOUNTPOINT) &&
			    !(hashdcp->dcv_flags & DVN_MOUNTPOINT)) {
				dvn_flags_t fsave;
				int s;

			        s = VN_LOCK(vp);
				fsave = hashdcp->dcv_flags;
				hashdcp->dcv_flags |= DVN_MOUNTPOINT;
				VN_UNLOCK(vp, s);
				if (vp->v_vfsmountedhere)
					goto out;
				if (fsave & DVN_MOUNTPOINT) {
				        error = ERESTART;
				        goto out;
				}
				dcvfs_delayed_mount(hashdcp);
			}

		}
	}

	DVN_KTRACE6(DVN_TRACE_MISC, "dcvn_handle_to_dcvn", handlep, 
		"error", error, "newdcp", *newdcpp);
out:
	if (error) {
		DCSTAT(dcvn_handle_to_dcvn_err);
		return error;
	}

	dcvn_checkattr(*newdcpp);

	return 0;
}

/*
 * Initialize a new vnode/dcvn, including allocating the vnode.
 * 
 * Also deal with the cases of the vnode being covered as well as
 * inserting any necessary interposers.
 * 
 * Upon return, error may be either:
 * 	0 		=> success
 * 	ERESTART	=> caller must loop retrying its operation.
 */
/*ARGSUSED*/
int
dcvn_newdc_setup(
        dcvn_t		*newdcp,
	vfs_t		*vfsp,	   /* imported vnode resides on this vfs */
        enum vtype	vtype,
	dev_t		rdev,
	int		flags)
{
	vnode_t		*newvp;

	/*
	 * Allocate a vnode and connect it to the dcvn.
	 */
	DCSTAT(dcvn_newdc_setup);
	newvp = vn_alloc(vfsp, vtype, rdev);
	bhv_desc_init(DCVN_TO_BHV(newdcp), newdcp, newvp, &cfs_vnodeops);
	vn_bhv_insert_initial(VN_BHV_HEAD(newvp), DCVN_TO_BHV(newdcp));

	newdcp->dcv_flags = flags;
	newdcp->dcv_vrgen = 0;
	newdcp->dcv_empty_pcache_cnt = 0;
	newdcp->dcv_error = 0;

	DVN_KTRACE8(DVN_TRACE_MISC, "dcvn_newdc_setup", newdcp, "newvp", newvp,
		"flags", flags, "vtype", vtype);

	/*
	 * If the server's vnode is torn down at inactive time and/or
	 * the link count on the file is 0, then cause this vnode to
	 * be torn down at inactive time.
	 */
	if (flags & (DVN_LINKZERO | DVN_INACTIVE_TEARDOWN))
		newvp->v_flag |= VINACTIVE_TEARDOWN;
	if (flags & DVN_NOSWAP)
		newvp->v_flag |= VNOSWAP;
	if (flags & DVN_PRIVATE)
		newvp->v_flag &= ~VSHARE;
	if (flags & DVN_DOCMP)
		newvp->v_flag |= VDOCMP;
	if (flags & DVN_FRLOCKS)
		newvp->v_flag |= VFRLOCKS;
	if (flags & DVN_ENF_LOCKING)
		newvp->v_flag |= VENF_LOCKING;

	/*
	 * If the vnode's a covered vnode, initialize a mount structure
	 * and new root vnode.  If the setup of the mount point fails
         * due to a race with an unmount or another attempt to set up the
         * mount point we indicate to the caller that he must retry.
	 */
	 if (flags & DVN_MOUNTPOINT) {
		 if (dcvfs_delayed_mount(newdcp)) {
			 vn_bhv_remove(VN_BHV_HEAD(newvp), 
				       DCVN_TO_BHV(newdcp));
			 vn_free(newvp);
			 return ERESTART;
		 }
	 }


	/*
	 * XXX If keeping a list of all vnodes for this dcvfs, attach it now.
	 */

	return 0;
}

/*
 * Install a new dcvn, establishing tokens and inserting it in the
 * handle hash table.
 *
 * Note that the reason for separating the setup and install steps
 * is to prevent certain races with tokens being revoked.  For example,
 * consider setting up and installing a new dc along with a name token
 * that grants the right to enter the name in the dnlc.  The sequence
 * of operations is:
 * - dcvn_newdc_setup
 * - dnlc_enter
 * - dcvn_newdc_install (which inserts the dc in the hash table).
 *
 * The act of revoking the name token must lookup the dc in the hash
 * table and then call dnlc_purge_vp.  It can be seen that if the 
 * dnlc_enter above was done after inserting the dc in the hash table, 
 * then there's a race that could leave a stale name entry in the dnlc.
 */
void
dcvn_newdc_install(
        dcvn_t		*dcp,
	tk_set_t	obtset,
        dcvfs_t         *dcvfsp)
{
	DCSTAT(dcvn_newdc_install);
	DVN_KTRACE6(DVN_TRACE_MISC, "dcvn_newdc_install", dcp,
		"obtset", obtset, "tclient", dcp->dcv_tclient);
	tkc_create("dcvn", dcp->dcv_tclient, dcp, &dcvn_tclient_iface,
		   DVN_NTOKENS, obtset, TK_NULLSET, (void *)dcp);

	/*
	 * This makes the vnode/dcvn accessible from the 
	 * handle hash table.
	 */
	dcvfs_dcvn_insert(dcp, dcvfsp);
	dcvn_handle_enter(dcp);
}

/*
 * Lookup a handle-to-dcvn mapping. 
 * Return a dcvn pointer if found, NULL otherwise.
 *
 * For a successful match, the handle's generation number must match
 * the one stored in the hash queue entry.  The purpose of the generation
 * number is to guard against handles that are being supplied to this
 * routine without the protection of an existence token (implying that
 * the manager of the handle is free to recycle the handle).
 *
 * If the dcvn is found, then the corresponding vnode is returned
 * with an extra reference.  It is the caller's responsibility to vrele.
 */
/* ARGSUSED */
dcvn_t *	
dcvn_handle_lookup(
	cfs_handle_t 	*handlep)
{
	dcvn_t		*curdcp;
	vnode_t		*vp;
	vmap_t		vmap;
	kqueue_t	*kq;
	lock_t		*lp;
	int		index, s;

	DCSTAT(dcvn_handle_lookup);
	index = CFS_HASH_INDEX(CFS_HANDLE_TO_OBJID(*handlep), DCVN_NHASH);
	kq = &dcvn_hashtab[index].hh_kqueue;
	lp = &dcvn_hashtab[index].hh_lock;
 again:
	s = mutex_spinlock(lp);
	for (curdcp = (dcvn_t *)kqueue_first(kq);
	     curdcp != (dcvn_t *)kqueue_end(kq);
	     curdcp = (dcvn_t *)kqueue_next(&curdcp->dcv_kqueue)) {

		if (CFS_HANDLE_EQU(*handlep, curdcp->dcv_handle)) {
			vp = DCVN_TO_VNODE(curdcp);
			VMAP(vp, vmap);
			mutex_spinunlock(lp, s);
			if (!vn_get(vp, &vmap, 0))
				goto again;

			DVN_KTRACE6(DVN_TRACE_HASH, "dcvn_handle_lookup", 
				    curdcp, "index", index, "v_count",
				    DCVN_TO_VNODE(curdcp)->v_count);
			DVN_KTRACE8(DVN_TRACE_HASH, "", curdcp,
			      "h_cell", 
			      CFS_HANDLE_TO_SERVICE(curdcp->dcv_handle).s_cell,
			      "h_objid",
			      CFS_HANDLE_TO_OBJID(curdcp->dcv_handle),
			      "h_gen",
			      CFS_HANDLE_TO_GEN(curdcp->dcv_handle));
			return curdcp;		/* got it */
		}
			
	}
	mutex_spinunlock(lp, s);

	DVN_KTRACE8(DVN_TRACE_HASH, "dcvn_handle_lookup", 0, "h_cell", 
		     CFS_HANDLE_TO_SERVICE(*handlep).s_cell,
		     "h_objid", CFS_HANDLE_TO_OBJID(*handlep),
		     "h_gen", CFS_HANDLE_TO_GEN(*handlep));

	return NULL;
}

/*
 * Enter a handle-to-dcvn mapping.
 * Assert if a mapping already exists.
 */
/* ARGSUSED */
void
dcvn_handle_enter(
	dcvn_t		*dcp)
{
	kqueue_t	*kq;
	lock_t		*lp;
	int		index, s;

	DCSTAT(dcvn_handle_enter);

	index = CFS_HASH_INDEX(CFS_HANDLE_TO_OBJID(dcp->dcv_handle), 
			       DCVN_NHASH);
	kq = &dcvn_hashtab[index].hh_kqueue;
	lp = &dcvn_hashtab[index].hh_lock;

	DVN_KTRACE6(DVN_TRACE_HASH, "dcvn_handle_enter", dcp, "index", 
		    index, "v_count", DCVN_TO_VNODE(dcp)->v_count);
	DVN_KTRACE8(DVN_TRACE_HASH, "", dcp, 
		     "h_cell", 
		     CFS_HANDLE_TO_SERVICE(dcp->dcv_handle).s_cell,
		     "h_objid",
		     CFS_HANDLE_TO_OBJID(dcp->dcv_handle),
		     "h_gen",
		     CFS_HANDLE_TO_GEN(dcp->dcv_handle));

	s = mutex_spinlock(lp);
#ifdef	DEBUG
	{
		/* Verify that it's not already hashed. */
		dcvn_t		*curdcp;

		for (curdcp = (dcvn_t *)kqueue_first(kq);
		     curdcp != (dcvn_t *)kqueue_end(kq);
		     curdcp = (dcvn_t *)kqueue_next(&curdcp->dcv_kqueue)) {
			ASSERT(!CFS_HANDLE_EQU(dcp->dcv_handle, 
					       curdcp->dcv_handle));
		}
	}
#endif
	kqueue_enter(kq, &dcp->dcv_kqueue);   
	mutex_spinunlock(lp, s);
}

/*
 * Remove a handle-to-dcvn mapping.
 * Assert if a mapping doesn't exist.
 */
/* ARGSUSED */
void
dcvn_handle_remove(
	dcvn_t		*dcp)
{
	lock_t		*lp;
	int		index, s;

	DCSTAT(dcvn_handle_remove);
	index = CFS_HASH_INDEX(CFS_HANDLE_TO_OBJID(dcp->dcv_handle), 
			       DCVN_NHASH);
	lp = &dcvn_hashtab[index].hh_lock;
	s = mutex_spinlock(lp);
#ifdef	DEBUG
	{
		/* Verify that it is indeed hashed. */
		kqueue_t 	*kq = &dcvn_hashtab[index].hh_kqueue;
		dcvn_t		*curdcp;
		int		found = 0;

		for (curdcp = (dcvn_t *)kqueue_first(kq);
		     curdcp != (dcvn_t *)kqueue_end(kq);
		     curdcp = (dcvn_t *)kqueue_next(&curdcp->dcv_kqueue)) {
			if (CFS_HANDLE_EQU(dcp->dcv_handle, 
					   curdcp->dcv_handle)) {
				found = 1;
				break;
			}
		}
		ASSERT(found == 1);
	}
#endif

	DVN_KTRACE6(DVN_TRACE_HASH, "dcvn_handle_remove", dcp, "index", 
		    index, "v_count", DCVN_TO_VNODE(dcp)->v_count);
	DVN_KTRACE8(DVN_TRACE_HASH, "", dcp, 
		     "h_cell", 
		     CFS_HANDLE_TO_SERVICE(dcp->dcv_handle).s_cell,
		     "h_objid",
		     CFS_HANDLE_TO_OBJID(dcp->dcv_handle),
		     "h_gen",
		     CFS_HANDLE_TO_GEN(dcp->dcv_handle));

	kqueue_remove(&dcp->dcv_kqueue);   
	mutex_spinunlock(lp, s);
	kqueue_null(&dcp->dcv_kqueue);
}

/*
 * Finds the dcvn associated with a a given vnode.  This routine is now
 * necessary because the dcvn may no longer be the first behavior.
 *
 * It would be nice to have a way to do this without getting a lock in 
 * the simple case in which the first behavior is a dcvn.  This would
 * require an additional field in the vnode which counted the number of
 * time behaviors were removed from the chain.  This should be looked 
 * at after the kudzu/teak tree-split.
 *
 * If we have no dcvn behavior, NULL is returned.
 */
dcvn_t *
dcvn_from_vnode(
	vnode_t		*vp)
{
        bhv_desc_t      *bdp;

	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp));
	bdp = vn_bhv_lookup(VN_BHV_HEAD(vp), &cfs_vnodeops);
	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));

	DVN_KTRACE4(DVN_TRACE_MISC, "dcvn_from_vnode", vp, "bdp",
		bdp ? BHV_TO_DCVN(bdp) : NULL);
	if (bdp == NULL)
		return (NULL);
	else
	        return BHV_TO_DCVN(bdp);
} 

/*
 * Routine to free a dcvn structure.
 */
void
dcvn_destroy(
	dcvn_t		*dcp)
{
	/*
	 * Teardown token client and other data structures.
	 */
	DCSTAT(dcvn_destroy);
	DVN_KTRACE6(DVN_TRACE_MISC, "dcvn_destroy", dcp, "dcxvn", dcp->dcv_dcxvn,
		"tclient", dcp->dcv_tclient);
	ASSERT(DCVN_TO_VNODE(dcp)->v_pgcnt == 0);
	if (dcp->dcv_dcxvn)
		cxfs_dcxvn_destroy(dcp->dcv_dcxvn);
	tkc_destroy(dcp->dcv_tclient);
	spinlock_destroy(&dcp->dcv_lock);
	kmem_zone_free(dcvn_zone, dcp);
}

/*
 ********************* Server-side functionality only ***********************
 */


/*
 * Given a vnode, return its ds behavior, creating one if necessary.
 * Must NOT be called with lock held on vnode's behavior chain.
 */
dsvn_t *
dsvn_vnode_to_dsvn(
        vnode_t		*vp)
{
	dsvn_t		*dsp;

	VN_BHV_READ_LOCK(VN_BHV_HEAD(vp));
	dsp = dsvn_vnode_to_dsvn_nolock(vp);	/* unlocks chain as well */

	DVN_KTRACE4(DVN_TRACE_MISC, "dsvn_vnode_to_dsvn", vp, "dsp", dsp);
	return dsp;
}

/*
 * Allocate and initialize a ds behavior.
 */
dsvn_t *
dsvn_alloc(
        vnode_t		*vp)
{
	bhv_desc_t	*bdp;
	dsvn_t		*dsp;

	DSSTAT(dsvn_alloc);
	dsp = kmem_zone_alloc(dsvn_zone, KM_SLEEP);
	bdp = DSVN_TO_BHV(dsp);
	tks_create("dsvn", dsp->dsv_tserver, dsp, &dsvn_tserver_iface,
		   DVN_NTOKENS, (void *)dsp);
	tkc_create_local("dsvn", dsp->dsv_tclient, dsp->dsv_tserver, 
			 DVN_NTOKENS, TK_NULLSET,
			 TK_NULLSET, (void *)dsp);
	bhv_desc_init(bdp, dsp, vp, &dsvn_ops);
	mutex_init(&dsp->dsv_mutex, MUTEX_DEFAULT, "dsv_mutex");
	dsp->dsv_flags = 0;
	dsp->dsv_vrgen_flags = 0;
	dsp->dsv_dsxvn = NULL;

	/*
	 * Generation number for dsvn structures.  It's needed to protect
	 * clients from using a non-existence-token-protected handle (i.e.,
	 * a hint handle) to obtain access to a dcvn structure corresponding 
	 * to a dsvn that's been recycled.  Each allocation of a dsvn structure
	 * increments the generation number and stores it within the dsvn.
	 * The client-side uses it when using a hint handle to lookup in its
	 * hash table.  
	 *
	 * The generation number is also used on the server side to protect
	 * the import code from using a hint handle to locate a dsvn
	 * structure that has been recycled.
	 */
	dsp->dsv_gen = atomicAddUint(&dsvn_gen_num, 1);

	DVN_KTRACE6(DVN_TRACE_MISC, "dsvn_alloc", dsp, "vp", vp, "gen", dsp->dsv_gen);

	return dsp;
}

bhv_desc_t *
dsvn_vnode_to_dsbhv(
        vnode_t		*vp)
{
	bhv_desc_t	*bdp;

	/*
	 * First a quick optimization:  check if the first behavior
	 * is already a dsvn.  This is done because when a dsvn exists
         * is most frequently is the first behavior.
	 */
        bdp = VNODE_TO_FIRST_BHV(vp);
	if (BHV_IS_DSVN(bdp)) {
		goto out;
	}

	/*
         * Now check for a dsvn anywhere in the chain.  Because we
         * have to give up the read behavior lock to get the write 
         * version, we will still have to check later.  It make sense,
         * however, to check ehere for a non-first dsvn behavior before
         * we start to do a lot of extra work.
	 */
	bdp = vn_bhv_lookup(VN_BHV_HEAD(vp), &dsvn_ops);

 out:
 	
	DVN_KTRACE4(DVN_TRACE_MISC, "dsvn_vnode_to_dsbhv", vp, "dsp",
			 bdp ? BHV_IS_DSVN(bdp) : 0);
	return bdp;
}

/*
 * Given a vnode, return its ds behavior, creating one if necessary.
 *
 * Must be called with read lock held on vnode's behavior chain.
 * NOTE: upon return, the chain is left UNlocked.
 */
dsvn_t *
dsvn_vnode_to_dsvn_nolock(
        vnode_t		*vp)
{
	bhv_desc_t	*bdp, *nbdp;
	dsvn_t		*dsp;
	vattr_t		vattr;
	int		error;

	ASSERT(VN_BHV_IS_READ_LOCKED(VN_BHV_HEAD(vp)));

	DSSTAT(dsvn_vnode_to_dsvn_nl);
	bdp = dsvn_vnode_to_dsbhv(vp);
	if (bdp != NULL) {
		dsp = BHV_TO_DSVN(bdp);
		VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));
		DVN_KTRACE4(DVN_TRACE_MISC, "dsvn_vn_to_dsvn_nl-1", vp, "dsp", dsp);
		return dsp;
	}
	
	/*
	 * Allocate/initialize a ds and insert it.
	 */
	dsp = dsvn_alloc(vp);
	bdp = DSVN_TO_BHV(dsp);

	/*
	 * Must have the write lock when updating the behavior chain.
	 */
	VN_BHV_READ_UNLOCK(VN_BHV_HEAD(vp));
	VN_BHV_WRITE_LOCK(VN_BHV_HEAD(vp));
	if (!vn_bhv_insert(VN_BHV_HEAD(vp), bdp)) {
		/*
		 * Success inserting.  Enter it into the hash table 
		 * prior to unlocking. 
		 *
		 * First need to set the dsvn's linkzero flag based 
		 * on the underlying file's link count.  Also set the dsvn's
		 * inactive_teardown and private flag as necessary.  If any
		  * of these flags are set, then clients won't cache their 
		 * vnodes once they go inactive.
		 */
		dsp->dsv_dsxvn = NULL;
		if (vp->v_type == VREG) {
			vattr.va_mask = AT_NLINK;
			PV_NEXT(bdp, nbdp, vop_getattr);
			PVOP_GETATTR(nbdp, &vattr, 0, get_current_cred(), 
				     error);
			/*
			 * If getattr returned an error (should never happen
			 * with xfs or efs), then be conservative and act as 
			 * though the link count is zero.
			 */
			if (vattr.va_nlink <= 0 || error)
				/* lock not needed */
				dsp->dsv_flags |= DVN_LINKZERO;	
			if (DS_FS_IS_XFS(vp))
				dsp->dsv_dsxvn = cxfs_dsxvn_make((void *)dsp,
					bdp, dsp->dsv_tclient, dsp->dsv_tserver);
		}

		if (vp->v_flag & VINACTIVE_TEARDOWN)
			dsp->dsv_flags |= DVN_INACTIVE_TEARDOWN;
		if (!(vp->v_flag & VSHARE))
			dsp->dsv_flags |= DVN_PRIVATE;
		if (vp->v_flag & VNOSWAP)
			dsp->dsv_flags |= DVN_NOSWAP;
		if (vp->v_flag & VDOCMP)
			dsp->dsv_flags |= DVN_DOCMP;
		if (vp->v_flag & VFRLOCKS)
			dsp->dsv_flags |= DVN_FRLOCKS;
		if (vp->v_flag & VENF_LOCKING)
			dsp->dsv_flags |= DVN_ENF_LOCKING;

		/*
		 * Record the ds in the server-side hash table.
		 */

		dsvn_handle_enter(dsp);
	} else {
		/* 
		 * Duplicate => must have raced when we were unlocked.
		 * Free up the data structures and return the one
		 * already in the chain.
		 */
		tkc_destroy_local(dsp->dsv_tclient);
		tks_destroy(dsp->dsv_tserver);
		mutex_destroy(&dsp->dsv_mutex);
		kmem_zone_free(dsvn_zone, dsp);
		
		bdp = vn_bhv_lookup(VN_BHV_HEAD(vp), &dsvn_ops);
		ASSERT(bdp != NULL);
		dsp = BHV_TO_DSVN(bdp);
	} 
	VN_BHV_WRITE_UNLOCK(VN_BHV_HEAD(vp));

	DVN_KTRACE4(DVN_TRACE_MISC, "dsvn_vn_to_dsvn_nl-2", vp, "dsp", dsp);
	return dsp;
}

/*
 * Given a handle, return a dsvn along with a reference to its
 * corresponding vnode.  Return NULL if the handle doesn't map
 * to an existing dsvn/vnode.
 *
 * If the dsvn is found, then the corresponding vnode is returned
 * with an extra reference.  It is the caller's responsibility to vrele.
 */
/* ARGSUSED */
dsvn_t *	
dsvn_handle_lookup(
	cfs_handle_t 	*handlep)
{
	dsvn_t		*curdsp;
	vnode_t		*vp;
	vmap_t		vmap;
	kqueue_t	*kq;
	lock_t		*lp;
	int		index, s;

	DSSTAT(dsvn_handle_lookup);

	ASSERT(SERVICE_EQUAL(CFS_HANDLE_TO_SERVICE(*handlep), cfs_service_id));

	index = CFS_HASH_INDEX(CFS_HANDLE_TO_OBJID(*handlep), DSVN_NHASH);
	kq = &dsvn_hashtab[index].hh_kqueue;
	lp = &dsvn_hashtab[index].hh_lock;
 again:
	s = mutex_spinlock(lp);
	for (curdsp = (dsvn_t *)kqueue_first(kq);
	     curdsp != (dsvn_t *)kqueue_end(kq);
	     curdsp = (dsvn_t *)kqueue_next(&curdsp->dsv_kqueue)) {
		/*
		 * Both object id and generation # must match.  Latter 
		 * needed to prevent hit on a dsvn that has been recycled.
		 */
		if (DSVN_TO_OBJID(curdsp) == (CFS_HANDLE_TO_OBJID(*handlep)) &&
		    DSVN_TO_GEN(curdsp) == (CFS_HANDLE_TO_GEN(*handlep))) {
			vp = DSVN_TO_VNODE(curdsp);
			VMAP(vp, vmap);
			mutex_spinunlock(lp, s);
			if (!vn_get(vp, &vmap, 0))
				goto again;
			DVN_KTRACE4(DVN_TRACE_MISC, "dsvn_handle_lookup-1",
				handlep, "dsp", curdsp);
			return curdsp;		/* got it */
		}
	}
	mutex_spinunlock(lp, s);

	DVN_KTRACE2(DVN_TRACE_MISC, "dsvn_handle_lookup-2",
				handlep);
	return NULL;
}

/*
 * Enter a handle-to-dsvn mapping.
 * Assert if a mapping already exists.
 */
/* ARGSUSED */
void
dsvn_handle_enter(
	dsvn_t		*dsp)
{
	kqueue_t	*kq;
	lock_t		*lp;
	int		index, s;

	DSSTAT(dsvn_handle_enter);

	index = CFS_HASH_INDEX(DSVN_TO_OBJID(dsp), DSVN_NHASH);
	kq = &dsvn_hashtab[index].hh_kqueue;
	lp = &dsvn_hashtab[index].hh_lock;

	DVN_KTRACE6(DVN_TRACE_HASH, "dsvn_handle_enter", dsp, "index", 
		    index, "v_count", DSVN_TO_VNODE(dsp)->v_count);

	s = mutex_spinlock(lp);
#ifdef	DEBUG
	{
		/* Verify that it's not already hashed. */
		dsvn_t		*curdsp;

		for (curdsp = (dsvn_t *)kqueue_first(kq);
		     curdsp != (dsvn_t *)kqueue_end(kq);
		     curdsp = (dsvn_t *)kqueue_next(&curdsp->dsv_kqueue)) {
			ASSERT(DSVN_TO_OBJID(dsp) != DSVN_TO_OBJID(curdsp));
		}
	}
#endif
	kqueue_enter(kq, &dsp->dsv_kqueue);   
	mutex_spinunlock(lp, s);
}

/*
 * Remove a handle-to-dsvn mapping.
 * Assert if a mapping doesn't exist.
 */
/* ARGSUSED */
void
dsvn_handle_remove(
	dsvn_t		*dsp)
{
	lock_t		*lp;
	int		index, s;

	DSSTAT(dsvn_handle_remove);
	index = CFS_HASH_INDEX(DSVN_TO_OBJID(dsp), DSVN_NHASH);
	lp = &dsvn_hashtab[index].hh_lock;
	s = mutex_spinlock(lp);
#ifdef	DEBUG
	{
		/* Verify that it is indeed hashed. */
		kqueue_t 	*kq = &dsvn_hashtab[index].hh_kqueue;
		dsvn_t		*curdsp;
		int		found = 0;

		for (curdsp = (dsvn_t *)kqueue_first(kq);
		     curdsp != (dsvn_t *)kqueue_end(kq);
		     curdsp = (dsvn_t *)kqueue_next(&curdsp->dsv_kqueue)) {
			if (DSVN_TO_OBJID(dsp) == DSVN_TO_OBJID(curdsp)) {
				found = 1;
				break;
			}
		}
		ASSERT(found == 1);
	}
#endif
	kqueue_remove(&dsp->dsv_kqueue);   
	mutex_spinunlock(lp, s);
	kqueue_null(&dsp->dsv_kqueue);
	DVN_KTRACE4(DVN_TRACE_MISC, "dsvn_handle_remove", dsp, "index", index);
}

/*
 * Free a dsvn structure.
 */
void
dsvn_destroy(
        dsvn_t		*dsp)
{
	DSSTAT(dsvn_destroy);
	DVN_KTRACE4(DVN_TRACE_MISC, "dsvn_destroy", dsp,
			"dsxvn", dsp->dsv_dsxvn);
	if (dsp->dsv_dsxvn) {
		cxfs_dsxvn_destroy(dsp->dsv_dsxvn);
	}
	tkc_destroy_local(dsp->dsv_tclient);
	tks_destroy(dsp->dsv_tserver);
	mutex_destroy(&dsp->dsv_mutex);
	kmem_zone_free(dsvn_zone, dsp);
}

/*
 * Return 1 if the vnode supports cells.
 */
int
cfs_is_cell_supported(vnode_t *vp)
{
        return ((vp->v_vfsp->vfs_flag & VFS_CELLULAR) != 0);
}

