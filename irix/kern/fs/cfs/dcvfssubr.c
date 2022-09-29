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
#ident  "$Revision: 1.25 $"

#include "dv.h"
#include "dvn.h"
#include "dvfs.h"
#include <fs/cell/cfs_intfc.h>
#include <fs/cell/mount_import.h>
#include "invk_dsvfs_stubs.h"
#include <sys/dnlc.h>
#include <sys/imon.h>
#include <fs/cell/cxfs_intfc.h>

/************************************************************************
 * Routines applied to client side of DVFS				*
 ************************************************************************/
static void	dcvfs_handle_enter(dcvfs_t *);
static void     dcvfs_tcif_obtain(void *, tk_set_t, tk_set_t, tk_disp_t,
                                 tk_set_t *);
static void     dcvfs_tcif_return(tkc_state_t, void *, tk_set_t, tk_set_t,
                                 tk_disp_t);
static void	dcvfs_handle_remove(dcvfs_t *);

tkc_ifstate_t dcvfs_tcif = {
        dcvfs_tcif_obtain,
        dcvfs_tcif_return
};

/*
 * Token client callout for obtaining a token.
 */
/* ARGSUSED */
static void
dcvfs_tcif_obtain(
        void            *dsobj,
        tk_set_t        obtset,
        tk_set_t        retset,
        tk_disp_t       why,
        tk_set_t        *refset)        /* out */
{
	*refset = TK_NULLSET;
	return;
}

/*
 * Token client callout for returning a token.
 */
/* ARGSUSED */
static void
dcvfs_tcif_return(
        tkc_state_t     tclient,
        void            *dcobj,
        tk_set_t        retset,
        tk_set_t        unknownset,
        tk_disp_t       why)
{
	/* REFERENCED */
	int		msgerr;
	dcvfs_t		*dcp = (dcvfs_t *)dcobj;

	msgerr = invk_dsvfs_vfsreturn(DCVFS_TO_SERVICE(dcp),
			DCVFS_TO_OBJID(dcp), cellid(), retset, unknownset, why);
	ASSERT(!msgerr);
	tkc_returned(dcp->dcvfs_tclient, retset, TK_NULLSET);
	if (TK_IS_IN_SET(TK_MAKE(DVFS_EXIST, TK_READ), retset)) {
		/* free distributed vfs data */
		tkc_destroy(dcp->dcvfs_tclient);
		if (dcp->dcvfs_xvfs) {
			cxfs_dcvfs_destroy(dcp->dcvfs_xvfs);
		}
		kmem_zone_free(dcvfs_zone, dcp);
	}
}

/*
 * dcvfs_prepare_unmount()
 */
int
dcvfs_prepare_unmount(
        dcvfs_t 	*dcp,
        cell_t          unmounter)
{
	int		error = 0;
	vfs_t 		*vfsp = DCVFS_TO_VFS(dcp);

	/*
         * Wait for sync to finish, lock vfsp, and set VFS_OFFLINE flag.  
         * If we started the unmount, this has already been done in
	 * dcvfs_dounmount.
         */
	if (unmounter != cellid()) {
        	if (error = vfs_lock_offline(vfsp))
                	return error;
	}

	/*
         * Purge all dnlc entries for this vfs.
         */
        (void) dnlc_purge_vfsp(vfsp, 0);

	/* this needs to be distributed XXX
        nfs_purge_vfsp(vfsp); */

	/* Call down to do the checking via a special VFS_IMPORT call. */
	/* This will allow any interposers as well as dcvfs_unmount to check */
	/* skip the VFS_SYNC, the server will do it later */
	/* make sure the root vnode ref count is 1 */
	VFS_UNMOUNT(vfsp, UNMOUNT_CKONLY, NOCRED, error);

	/*
         * In the case of an error, give up any lock that we got.
	 */
	if (error && unmounter != cellid()) {
		vfs_unlock(vfsp);
		return(error);
	}

	return(0);
}

int
dcvfs_check_unmount(
	dcvfs_t		*dcp)
{
        vfs_t 		*vfsp = DCVFS_TO_VFS(dcp);
	vnode_t		*rvp;
	vmap_t		vmap;

	if (vfsp->vfs_flag & VFS_CELLROOT)
	        return (0);
	rvp = dcp->dcvfs_rootvp;
	if (rvp->v_count > 1 || dcvfs_vnbusy(dcp)) 
	        return(EBUSY);
	/* lets free up the root vnode and force a reclaim */
	dcp->dcvfs_rootvp = NULL;
	if (rootdir == rvp)
	        rootdir = NULL;
	VN_RELE(rvp);
	VMAP(rvp, vmap);
	vn_purge(rvp, &vmap);
	/* if the server fails the VFS_UNMOUNT, the client cell will
	 * have to import a new instance of the root vnode */
	return (0);
}

/*
 * dcvfs_commit_unmount()
 * device server has successfully unmounted the file system and has instructed
 * client cells to tear down their local vfs copy.
 */


void
dcvfs_commit_unmount(
	dcvfs_t		*dcp,
        cell_t          unmounter)
{
	vnode_t		*cvp;
	vfs_t		*vfsp = DCVFS_TO_VFS(dcp);

	/*
	 * - remove vfs handle from list
	 * - release ref counts held on root vnode and cover vnode
	 * - return tokens and free root vnode (already done for vfs)
	 * - flush vnodes , right now there's no caching for distributed vnode
	 * so we don't do anything for now.
	 * - remove vfs from vfs list and tear down handles and data
	 */
	
	/* the client dcvfs struct is torn down in dcvfs_tcif_return() */
	dcvfs_handle_remove(dcp);
	spinlock_destroy(&dcp->dcvfs_lock);

	cvp = vfsp->vfs_vnodecovered;		/* vfs still locked */
	--cvp->v_vfsp->vfs_nsubmounts;	/* xxx how to distribute this */
	ASSERT(vfsp->vfs_nsubmounts == 0);
	/* remove this mount point from the vfs list */
	vfs_remove(vfsp);
	VFS_REMOVEBHV(vfsp, DCVFS_TO_BHV(dcp));

	/* 
         * Deallocate the vfs.  If we were the initiator of the unmount,
         * the caller of VFS_DOUNMOUNT is responsible for doing this.
	 */
	if (unmounter != cellid())
		vfs_deallocate(vfsp);

/*
        IMON_EVENT(cvp, cr, IMON_CONTENT);
*/
        VN_RELE(cvp);


}
	
	

/*
 * Given a vnode handle and a vfs handle, ensure that all the mount points 
 * leading to this vnode are brought in.  The vfs handle is used as an
 * optimization to look in the dcvfs hash table before sending an RPC.
 *
 * Return a local vfs of the file system that the input vp resides on.
 */
struct vfs *
dcvfs_import_mounthierarchy(
        cfs_handle_t 	*vfs_hdlp)	/* handle for initial vfs */
{
	typedef struct mntpt {
                kqueue_t        mpt_kq;
                cfs_handle_t    mpt_vfsh;
                cfs_handle_t    mpt_cvph;
        } mntpt_t;
#define MNTPT_MALLOC(e)	(e = (struct mntpt *)kern_malloc(sizeof(struct mntpt)))
#define MNTPT_FREE(e)	kern_free(e)


	cfs_handle_t	curvfs_hdl = *vfs_hdlp;
	cfs_handle_t	cvp_hdl; 
        cfs_handle_t    newvfs_hdl;
	vfs_t		*vfsp;
	dcvfs_t		*dcvfs;
	mntpt_t         *mntpt;
        kqueue_t        mntpt_stack;


	ASSERT(vfs_hdlp);
	if (dcvfs = dcvfs_handle_lookup(vfs_hdlp)) {
		vfsp = DCVFS_TO_VFS(dcvfs);
		return(vfsp);
	}
	kqueue_init(&mntpt_stack);
	  
	while (1) {
		/* REFERENCED */
		int	msgerr;

		/*
		 * Bring back the chain of file systems within which our original
                 * fs is mounted, back to the root or one we already have.  
		 */
		msgerr = invk_dsvfs_getmounthdls(
					CFS_HANDLE_TO_SERVICE(curvfs_hdl), 
					&curvfs_hdl, &cvp_hdl, &newvfs_hdl);
		ASSERT(!msgerr);
 
		/*
		 * we're at the root file system but we couldn't find
		 * the root vfs in the hash. 
		 */
		if (CFS_HANDLE_IS_NULL(newvfs_hdl)) {
                        int error;

		        ASSERT(CFS_HANDLE_IS_NULL(cvp_hdl));
		        /* this also brings in the root vnode */
		        VFS_INIT(rootvfs);
			rootvfs->vfs_flag |= VFS_CELLULAR;
			VFSOPS_ROOTINIT(&cfs_vfsops, rootvfs, error);
			if (error)
			        panic("dcvfs_import_mounthierarchy");
			vfsp = rootdir->v_vfsp;
			break;
		}
			
		/*
		 * otherwise just put this mount point on the stack 
		 */
		MNTPT_MALLOC(mntpt);
		mntpt->mpt_vfsh = curvfs_hdl;
		mntpt->mpt_cvph = cvp_hdl;
		kqueue_enter(&mntpt_stack, &mntpt->mpt_kq);
		curvfs_hdl = newvfs_hdl;

		/*
		 * If new vfs already exists locally (either as client or server),
                 * there's no need to go further up the tree.
		 */
		if (SERVICE_TO_CELL(CFS_HANDLE_TO_SERVICE(newvfs_hdl)) == cellid() ||
		    dcvfs_handle_lookup(&newvfs_hdl))
			break;
	}
	/*
	 * start importing the mount points top down according to what
	 * was put on the stack
	 */
	while (!kqueue_isempty(&mntpt_stack)) {
		vnode_t *vp;
		dcvn_t  *dcp;

		mntpt = (mntpt_t *)kqueue_first(&mntpt_stack);

		dcp = dcvn_import_force(&mntpt->mpt_cvph);
		ASSERT(dcp != NULL);
		vp = DCVN_TO_VNODE(dcp);
		vfsp = vp->v_vfsmountedhere;
		VN_RELE(vp);
		kqueue_remove(&mntpt->mpt_kq);
		MNTPT_FREE(mntpt);
	}
	/* 
	 * return the last vfsp which corresponds with the vfs of the input 
	 * handle.
	 */
	return(vfsp);
                
}

			


/*
 * dcvfs_delayed_mount()
 * dcp is the DC for the client covered vnode.
 * Imports the mount point covering this vnode.
 * This means the mountedhere vfs and its root vnode.
 * Assume that caller holds an existence token on the covered vnode.
 */ 
dcvfs_delayed_mount(dcvn_t *cvp_dcp)
{
	int		error;
	cfs_handle_t	vfs_hdl, cvp_hdl;
	vfs_t		*vfsp;
	int		s;
	vnode_t		*cvp;
	/* REFERENCED */
	int		msgerr;

	/* get the vfs handle for the file system mounted on this cvp */
	msgerr = invk_dsvfs_vfsmountedhere(DCVN_TO_SERVICE(cvp_dcp), 
			DCVN_TO_OBJID(cvp_dcp), &vfs_hdl, &error);
	ASSERT(!msgerr);
	if (error) 
		return(error);

	/* allocate a client vfs */
	vfsp = vfs_allocate();
	vfsp->vfs_flag |= VFS_CELLULAR;

	/* 
	 * brings in the root vnode and other attributes of this vfs
	 * Has no reference or any kind of tokens on this FS.
	 */
	cvp_hdl = cvp_dcp->dcv_handle;
	error = dcvfs_import_mountpoint(&cvp_hdl, vfsp, &vfs_hdl);
	if (error) {
	        vfs_deallocate(vfsp);
		return(error);	
	}
	
	cvp = DCVN_TO_VNODE(cvp_dcp);
	VN_HOLD(cvp);		/* local ref count */
	vfs_add(cvp, vfsp, VFS_FLAGS_PRESET);
	s = VN_LOCK(cvp);
        cvp->v_vfsmountedhere = vfsp;
	/* XXX R2 purge this cvp from cache */
	VN_UNLOCK(cvp, s);
	return(0);
}

/*
 * dcvfs_import_mountpoint()
 * From the cvp handle lets bring in tokens and attributes for the mountedhere
 * vfs and root vnode. After that create local copies of those objects.
 * Have existence token for cvp.
 */
int		
dcvfs_import_mountpoint(
	cfs_handle_t	*cvp_hdlp,	/* cover vnode */
	vfs_t		*vfsp,		/* client vfs to be setup */
	cfs_handle_t	*vfs_hdlp)	/* where device cell is at */
{
	mount_import_t	mnti;
	int		error;
	/* REFERENCED */
	int		msgerr;

	/*
	 * obtain tokens for vfs, root vnode
	 */
	msgerr = invk_dsvfs_getmountpoint(CFS_HANDLE_TO_SERVICE(*vfs_hdlp), 
		cvp_hdlp, cellid(), &mnti, &error);
	ASSERT(!msgerr);

	/*
	 * Handle error cases:	
	 * - fs can be unmounted -> error = ENOENT
	 * This func is only called by dcvfs_delayed_mount() via 
	 * dcvn_newdc_setup() when a  vnode being imported
	 * happens to be a covered vnode. Because cfs_vnimport() single threads
	 * importing of the same vnode we don't have to worry about other
	 * processes on this cell racing with us to import this mountpoint.
	 * 
	 * This is also called by dcvfs_mount() after the dir cell has done 
	 * a remote mount.  So there is a race between importing a vnode
	 * and a mount in progress. This is single threaded on the server
	 * by the invk_dsvfs_getmountpoint above. The token can only
	 * be granted once to a client cell. The loser will return an error 
	 * below.
	 */
	if (error)
		return(error);
	ASSERT ((mnti.vfs_tkset & DVFS_EXIST_TRAVERSE_RD) != TK_NULLSET);
	cfs_mountpoint_setup(vfsp, NULL, &mnti, NULL);
	return(0);
}

/*
 * set up a client mountpoint
 * vfsp points to the client vfs.
 * mntip points to the mount infor needed to set up the mount point
 */ 
void
cfs_mountpoint_setup(
	vfs_t 		*vfsp, 
	vnode_t 	**rdirp, 
	mount_import_t  *mntip,
	cxfs_sinfo_t    *cxsp)
{
	dcvn_t		*rvp_dcp = NULL;
	dcvfs_t		*vfs_dcp;
        bhv_desc_t      *bdp_vfs;
	/* REFERENCED */
	int		error;

	/*
	 * Setup the root vnode.
	 * Got tokens for both vfs and root vnode.
	 */
	if (!(mntip->vfs_flags & VFS_CELLROOT)) {
         	ASSERT ((mntip->rvp_tkset & DVN_EXIST_READ) != TK_NULLSET) ;
		rvp_dcp = kmem_zone_alloc(dcvn_zone, KM_SLEEP);
		rvp_dcp->dcv_handle = mntip->rvp_hdl;
		spinlock_init(&rvp_dcp->dcv_lock, "dcv_lock");

		/* root vnode can't be covered */
		ASSERT(!(mntip->rvp_flags & DVN_MOUNTPOINT));

		ASSERT((mntip->rvp_vtype != VCHR) && (mntip->rvp_vtype != VBLK));
		error = dcvn_newdc_setup(rvp_dcp, vfsp, mntip->rvp_vtype, 
					 mntip->rvp_rdev, mntip->rvp_flags);

		/* error only possible when the new vnode is covered */
		ASSERT(!error);
		
		if (rdirp)
		        *rdirp = DCVN_TO_VNODE(rvp_dcp);
		VN_FLAGSET(DCVN_TO_VNODE(rvp_dcp), VROOT);
	}

	vfsp->vfs_flag = mntip->vfs_flags;
	vfsp->vfs_dev = mntip->vfs_dev;
	vfsp->vfs_bsize = mntip->vfs_bsize;
	vfsp->vfs_fstype = cfs_fstype;
	vfsp->vfs_fsid = mntip->vfs_fsid;
	vfsp->vfs_altfsid = mntip->vfs_altfsid;
	vfsp->vfs_cell = mntip->vfs_cell;

	bdp_vfs = cfs_dcvfs_behavior(vfsp, mntip->vfs_fstype);
	vfs_dcp = BHV_TO_DCVFS(bdp_vfs);
	vfs_dcp->dcvfs_handle = mntip->vfs_hdl;
	vfs_dcp->dcvfs_sinfo = cxsp;
	tkc_create("dcvfs", vfs_dcp->dcvfs_tclient, vfs_dcp, &dcvfs_tcif,
			DVFS_NTOKENS, mntip->vfs_tkset,
			mntip->vfs_tkset, (void *)vfs_dcp);
        bhv_insert_initial(VFS_BHVHEAD(vfsp), bdp_vfs);

	/*
         * Call the VFSOPS_IMPORT routine to let some special local behavior 
         * be added.
	 */
	if (mntip->vfs_eisize > VFS_EILIMIT)
	        panic("vfs_eilimit");
	vfsp->vfs_eisize = mntip->vfs_eisize;
	vfsp->vfs_expinfo = mntip->vfs_expinfo;
	VFSOPS_IMPORT(vfssw[mntip->vfs_fstype].vsw_vfsops, vfsp, error);
	if ((vfs_dcp->dcvfs_fstype == xfs_fstype) && vfsp->vfs_eisize) {
		vfs_dcp->dcvfs_xvfs = cxfs_dcvfs_make(vfsp, vfs_dcp);
	}

	/*
         * Install the new dcvn.  Prior to this, install in dnlc
         * (when we have name caching).  If the VFS_ROOTCELL was set,
         * a local file system interposer is responsible for the root.
         */
        if (rvp_dcp) {
	        vfs_dcp->dcvfs_rootvp = DCVN_TO_VNODE(rvp_dcp);
		rvp_dcp->dcv_dcxvn = NULL;
		dcvn_newdc_install(rvp_dcp, mntip->rvp_tkset, vfs_dcp);
	}

	/*
	 * Install the new dcvfs.
	 */
	dcvfs_handle_enter(vfs_dcp);
}

bhv_desc_t *
cfs_dcvfs_behavior(
        vfs_t *vfsp,
	int   fstype)
{
	dcvfs_t		*vfs_dcp;

	vfs_dcp = kmem_zone_alloc(dcvfs_zone, KM_SLEEP);
	spinlock_init(&vfs_dcp->dcvfs_lock, "dcvf_lok");
	CFS_HANDLE_MAKE_NULL(vfs_dcp->dcvfs_handle);
	vfs_dcp->dcvfs_dcvlist = NULL;
	vfs_dcp->dcvfs_reclaims = 0;
	vfs_dcp->dcvfs_fstype = fstype;
	vfs_dcp->dcvfs_sinfo = NULL;
	vfs_dcp->dcvfs_xvfs = NULL;
	bhv_desc_init(DCVFS_TO_BHV(vfs_dcp), vfs_dcp, vfsp, &cfs_vfsops);
	return (DCVFS_TO_BHV(vfs_dcp));
}

void 
cfs_dcvfs_behavior_free(
        bhv_desc_t *bdp)
{
        dcvfs_t   	*vfs_dcp = BHV_TO_DCVFS(bdp);

	ASSERT(bdp->bd_ops == &cfs_vfsops);
	spinlock_destroy(&vfs_dcp->dcvfs_lock);
	ASSERT(vfs_dcp->dcvfs_dcvlist == NULL);
	kmem_zone_free(dcvfs_zone, vfs_dcp);
}

void
dcvfs_dcvn_insert(	
	dcvn_t		*vndcp,
	dcvfs_t		*vfsdcp)
{
        int             s;
	dcvn_t		*vndcp1;

	vndcp->dcv_dcvfs = vfsdcp;
	s = mutex_spinlock(&vfsdcp->dcvfs_lock);
	if (vndcp1 = vfsdcp->dcvfs_dcvlist) {
		vndcp->dcv_next = vndcp1;
		vndcp1->dcv_prev = vndcp;
		vfsdcp->dcvfs_dcvlist = vndcp;
	} else {
		vfsdcp->dcvfs_dcvlist = vndcp;
		vndcp->dcv_next = NULL;
	}
	vndcp->dcv_prev = NULL;
	mutex_spinunlock(&vfsdcp->dcvfs_lock, s);
}

void
dcvfs_dcvn_delete(	
	dcvn_t		*vndcp,
	dcvfs_t		*vfsdcp)
{
        int             s;

	s = mutex_spinlock(&vfsdcp->dcvfs_lock);
	vfsdcp->dcvfs_reclaims++;
	if (vndcp->dcv_prev) 
		vndcp->dcv_prev->dcv_next = vndcp->dcv_next;
	else
		vfsdcp->dcvfs_dcvlist = vndcp->dcv_next;
	if (vndcp->dcv_next)
		vndcp->dcv_next->dcv_prev = vndcp->dcv_prev;
		
	mutex_spinunlock(&vfsdcp->dcvfs_lock, s);
}

int
dcvfs_vnbusy(dcvfs_t *vfsdcp)
{
	dcvn_t         	*vndcp;
	vnode_t		*vp;
	int		busy = 0;
        int             s;
	vmap_t 		vmap;

loop:
        s = mutex_spinlock(&vfsdcp->dcvfs_lock);
	for (vndcp = vfsdcp->dcvfs_dcvlist; vndcp != NULL; 
		vndcp = vndcp->dcv_next) {
		vp = DCVN_TO_VNODE(vndcp);
		if (vp->v_count != 0) {
			if (vp->v_count == 1 && vp == vfsdcp->dcvfs_rootvp)
				continue;
			busy = 1;
			break;
		}
		/* count == 0, try to get server to drop the count as well */
		VMAP(vp, vmap);
        	mutex_spinunlock(&vfsdcp->dcvfs_lock, s);
		/* this deletes the dcvn from the dcvfs */
		vn_purge(vp, &vmap);
		goto loop;
        }
        mutex_spinunlock(&vfsdcp->dcvfs_lock, s);
	return(busy);
}
	
	
static void
dcvfs_handle_enter(
        dcvfs_t          *dcp)
{
        kqueue_t        *kq;
        lock_t          *lp;
        int             s;

        kq = &dcvfs_head.hh_kqueue;
        lp = &dcvfs_head.hh_lock;

        s = mutex_spinlock(lp);
        kqueue_enter(kq, &dcp->dcvfs_kqueue);
        mutex_spinunlock(lp, s);
}

static void
dcvfs_handle_remove(
        dcvfs_t         *dcp)
{
        lock_t          *lp;
        int             s;

        lp = &dcvfs_head.hh_lock;

        s = mutex_spinlock(lp);
        kqueue_remove(&dcp->dcvfs_kqueue);
        mutex_spinunlock(lp, s);
}

dcvfs_t *
dcvfs_handle_lookup(
	cfs_handle_t	*vfs_hdlp)
{
	dcvfs_t         *curdcp;
        kqueue_t        *kq;
        lock_t          *lp;
        int             s;


        kq = &dcvfs_head.hh_kqueue;
        lp = &dcvfs_head.hh_lock;
        s = mutex_spinlock(lp);
        for (curdcp = (dcvfs_t *)kqueue_first(kq);
             curdcp != (dcvfs_t *)kqueue_end(kq);
             curdcp = (dcvfs_t *)kqueue_next(&curdcp->dcvfs_kqueue)) {
		if (CFS_HANDLE_EQU(*vfs_hdlp, curdcp->dcvfs_handle)) {
			mutex_spinunlock(lp, s);
                        return curdcp;          /* got it */
		}
        }
        mutex_spinunlock(lp, s);

        return NULL;
}

void
cfs_vfsimport(
	cfs_handle_t	*vfs_hdlp,
        vfs_t           **vfspp)
{
        vfs_t           *vfsp;

        vfsp = dcvfs_import_mounthierarchy(vfs_hdlp);
	*vfspp = vfsp;
}
	
dcvfs_t *
dcvfs_vfs_to_dcvfs(
	vfs_t	*vfsp)
{
        bhv_desc_t      *bdp_vfs;

	for (bdp_vfs = vfsp->vfs_fbhv;
	     bdp_vfs != NULL;
	     bdp_vfs = BHV_NEXT(bdp_vfs)) {
		if (VFSOPS_IS_DCVFS(BHV_OPS(bdp_vfs)))
			return BHV_TO_DCVFS(bdp_vfs);
	}
	return NULL;
}


#if CELL_IRIX
void
dcvfs_dummy(
	int             fstype,
        cfs_handle_t    *vfs_hdlp)
{
        vfs_t           *vfsp = dvfs_dummytab[fstype];
        bhv_desc_t      *bdp_vfs;
	dcvfs_t         *dcvfs;

	ASSERT(dcvfs_dummytab[fstype] == NULL);
	bdp_vfs = cfs_dcvfs_behavior(vfsp, fstype);
	dcvfs = BHV_TO_DCVFS(bdp_vfs);
	dcvfs_dummytab[fstype] = dcvfs;
	dcvfs->dcvfs_handle = *vfs_hdlp;
	dcvfs_handle_enter(dcvfs);
}
#endif /* CELL_IRIX */




