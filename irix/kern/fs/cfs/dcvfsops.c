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
#ident  "$Revision: 1.26 $"

#include "dv.h"
#include "dvn.h"
#include "dvfs.h"
#include "invk_dsvfs_stubs.h"
#include <fs/cell/cfs_intfc.h>
#include <sys/debug.h>
#include <sys/dnlc.h>
#include <sys/vfs.h>
#include <sys/fs_subr.h>
#include <sys/uthread.h>

/*
 * Client-side vfs ops for the Cell File System.
 */

/*
 * dcvfs_sync
 */
/*ARGSUSED*/
static int
dcvfs_sync(
	bhv_desc_t	*bdp,
	int		flags,
	cred_t		*cred)
{
	int		s, error=0;
	dcvfs_t		*vfsdcp = BHV_TO_DCVFS(bdp);
	dcvn_t		*vndcp;
	vnode_t		*vp;
	uint		reclaims;
	vmap_t		vmap;
	

	/*
	 * For now, the only flag we handle  is the PDFLUSH flag.
	 */
	if ( !(flags&SYNC_PDFLUSH))
		return(0);
loop:
        s = mutex_spinlock(&vfsdcp->dcvfs_lock);
        for (vndcp = vfsdcp->dcvfs_dcvlist; 
			vndcp != NULL;
                	vndcp = vndcp->dcv_next) {
                vp = DCVN_TO_VNODE(vndcp);

                /*
                 * If this is just vfs_sync() or pflushd() calling
                 * then we can skip vnodes for which it looks like
                 * there is nothing to do.  Since we don't have the
                 * vnode locked this is racey, but these are periodic
                 * calls so it doesn't matter. 
                 */

                if (flags & SYNC_PDFLUSH) {
                        if (vp->v_dpages == NULL) 
                                continue;
		} else {
			continue;
		}

                /*
                 * We don't mess with swap files from here since it is
                 * too easy to deadlock on memory.
                 */
                if (vp->v_flag & VISSWAP) {
                        continue;
                }

                VMAP(vp, vmap);
		reclaims = vfsdcp->dcvfs_reclaims;
                mutex_spinunlock(&vfsdcp->dcvfs_lock, s);

		if (!vn_get(vp, &vmap, 0)) {
			goto loop;
		}

                if (flags & SYNC_PDFLUSH) {
                        if (vp->v_dpages)  {
				pdflush(vp, B_DELWRI);
			}
		}

		vn_rele(vp);

        	s = mutex_spinlock(&vfsdcp->dcvfs_lock);
		if (reclaims != vfsdcp->dcvfs_reclaims) {
                	mutex_spinunlock(&vfsdcp->dcvfs_lock, s);
			goto loop;
		}
		
        }
        mutex_spinunlock(&vfsdcp->dcvfs_lock, s);


#ifdef NOTYET
	if ( !(flags & SYNC_PDFLUSH)) {
		msgerr = invk_dsvfs_sync(DCVFS_TO_SERVICE(vfsdcp),
				DCVFS_TO_OBJID(vfsdcp),
				flags, CRED_GETID(cred), &error);
	}
#endif

	return(error);
}


/*ARGSUSED*/
static int
dcvfs_mntupdate(
	bhv_desc_t *bdp,
        struct vnode *mvp,
        struct mounta *uap,
        struct cred *cr)
{
	/* XXX later */
	panic("dcvfs_mntupdate: not yet implemented");
	return(0);
}

#if CELL_IRIX
static int
dcvfs_dounmount(
	bhv_desc_t	*bdp,
	int		flags,
        vnode_t         *rootvp,
	cred_t		*cr)
{
	int 	error;
	/* REFERENCED */
	int	msgerr;
	dcvfs_t	*dcp = BHV_TO_DCVFS(bdp);
        vfs_t *vfsp = bhvtovfs(bdp);

	error = vfs_lock_offline(vfsp);
	if (rootvp)
		VN_RELE(rootvp);
	if (error)
		return (error);
	msgerr = invk_dsvfs_dounmount(DCVFS_TO_SERVICE(dcp),
				DCVFS_TO_OBJID(dcp),
				flags, CRED_GETID(cr), cellid(), &error);
	ASSERT(!msgerr);
	if (error)
		vfs_unlock(vfsp);
	return(error);
}

#endif /* CELL_IRIX */

/*
 * dcvfs_unmount is used for special VFS_UNMOUNT calls with the UNMOUNT_CKONLY
 * or UNMOUNT_FINAL flags from dcvfs_{prepare,commit}_unmount.  It should not
 * be called in the normal unmount case and will panic if so called.
 */
/*ARGSUSED*/
static int
dcvfs_unmount(
	bhv_desc_t	*bdp,
	int		flags,
	cred_t		*cr)
{
        int 		error = 0;
	dcvfs_t		*dcp = BHV_TO_DCVFS(bdp);
#if CELL_IRIX
	ASSERT(cr == NOCRED);
	switch (flags) {
        case UNMOUNT_CKONLY:
	        error = dcvfs_check_unmount(dcp);
		break;

        case UNMOUNT_FINAL:
	        break;

	default:
	        panic("dcvfs_unmount");
	}
#else /* CELL_IRIX */
        error = dcvfs_check_unmount(dcp);
	if (error == 0) {
		dcvfs_commit_unmount(dcp, cellid());
		tkc_release(dcp->dcvfs_tclient, DVFS_EXIST_TRAVERSE_RD); 
		tkc_recall(dcp->dcvfs_tclient, 
			   DVFS_EXIST_TRAVERSE_RD, 
			   TK_ADD_DISP(TK_MAKE_DISP(DVFS_EXIST, 
						    TK_CLIENT_INITIATED),
				       TK_MAKE_DISP(DVFS_TRAVERSE, 
						    TK_CLIENT_INITIATED)));
	}
		
	
#endif /* CELL_IRIX */
	return (error);
}
	
/*
 * dcvfs_mount
 * executes on dir cell.
 * function ship the mount to the device cell then instantiate a local mount 
 * point. vfsp points to the new vfs that was created on the dir cell.
 */
static int
dcvfs_mount(
	struct vfs *vfsp, 			/* local vfs on dir cell */
	struct vnode *cvp, 			/* server cvp on dir cell */
	struct mounta *uap,
	char *attrs,
	struct	cred *cred)
{
	cfs_handle_t    rdir_hdl, cdir_hdl;
	cfs_handle_t	vfs_hdl;
	cfs_handle_t	cvp_hdl;
	int		error;
	/* REFERENCED */
	int		msgerr;
	service_t	svc;
	

	/* 
	 * we need to pass along the handle for cvp to the device server
	 * Device cell will be client of this cover vnode.
	 */
	cfs_vnexport(cvp, &cvp_hdl);

	/* export rdir and cdir to device server */
	if (curuthread->ut_rdir)
        	cfs_vnexport(curuthread->ut_rdir, &rdir_hdl);
        cfs_vnexport(curuthread->ut_cdir, &cdir_hdl);


	/* fig out where to function ship the mount request to */
	SERVICE_MAKE(svc, vfsp->vfs_cell, SVC_CFS);

	/*
	 * function ship the mount request to the device cell
	 */
	msgerr = invk_dsvfs_vfsmount(svc, &cvp_hdl, uap, attrs,
			CRED_GETID(cred), 
			(curuthread->ut_rdir ? &rdir_hdl : NULL), 
			&cdir_hdl, &vfs_hdl, &error);
	ASSERT(!msgerr);

	if (error)
		return(error);
	/* 
	 * The remote mount succeeds. Lets setup the client vfs
	 * for the new mount on the dir cell.
	 */
	if (error = dcvfs_import_mountpoint(&cvp_hdl, vfsp, &vfs_hdl)) 
		return(error);
	
	return(0);			
}

/*
 * dcvfs_rootinit()
 */
#if CELL_IRIX
static
dcvfs_rootinit(vfs_t *vfsp)
{
	service_t	rootfs_svc;
	mount_import_t	mnti;
	int		iteration = 0;
	int		error = 1;
	cfs_handle_t	*htab;
	size_t          htsize;
	int             i;

	/* 
         * If we are on the rootfs cell, reject this so vfs_mountroot
         * will go on to try the other file systems.  We take this
         * opportunity to set up the dsvfs's for all of the dummy 
         * vfs structures. 
         */
	if (cellid() == rootfs_cell) {
                dsvfs_dummy_setup();
		return(EINVAL);
	}

	/*
         * To avoid perturbing those working on multikernel, we will,
         * for the moment, reject this call.  This is now necessary
         * because cfs has been placed first in the vfssw and so will 
         * get called first.  When multikernel is ready to rock and roll, 
         * the temporary code below can be removed.
	 */
	SERVICE_MAKE(rootfs_svc, rootfs_cell, SVC_CFS);	
	while (error) {
		/* REFERENCED */
        	int	msgerr;

	        htab = NULL;
                htsize = 0;
		msgerr = invk_dsvfs_getrootmountpoint(rootfs_svc, cellid(),
					&mnti, &error, &htab, &htsize);
		ASSERT(!msgerr);
		

		/*
                 * Delay and retry in the event of errors.  In the case of
                 * an unmounted root (ENOENT), delay more substantially
                 * so that we rootfs cell has time to finish the mount.
		 */
		iteration++;
		if (error) {
		        kmem_free(htab, sizeof(cfs_handle_t) * htsize);
			if (error == ENOENT)
				delay(iteration);
			else 
			        cell_backoff(iteration);
		}
		  
		/* 
		 * ERESTART can only mean another process from this cell races	
		 * ahead of us. Check in vfs handle hash list.
		 * If not there then try again.
		 */
		if ((error == ERESTART) && dcvfs_handle_lookup(&mnti.vfs_hdl))
			/* found the rootvfs dc in the hash list */
			return(0);
	}

	ASSERT ((mnti.vfs_tkset & DVFS_EXIST_TRAVERSE_RD) != TK_NULLSET);
        ASSERT ((mnti.rvp_tkset & DVN_EXIST_READ) != TK_NULLSET);
	for (i = 0; i < htsize; i++) {
	        if (dvfs_dummytab[i] != NULL) 
			dcvfs_dummy(i, &htab[i]);
	}
	cfs_mountpoint_setup(vfsp, &rootdir, &mnti, NULL);
	kmem_free(htab, sizeof(cfs_handle_t) * htsize);
        return(0);
}
#else /* CELL_IRIX */
/* ARGSUSED */
static
dcvfs_rootinit(vfs_t *vfsp)
{
        return (EINVAL);
}
#endif /* CELL_IRIX */


/*
 * dcvfs_root
 */
static int
dcvfs_root(
	bhv_desc_t		*vfs_bdp,
	vnode_t			**rootvp)
{
	dcvfs_t	*vfs_dcp = BHV_TO_DCVFS(vfs_bdp);
	
	ASSERT(vfs_dcp->dcvfs_rootvp != NULL);
	ASSERT(vfs_dcp->dcvfs_rootvp->v_count > 0);

	VN_HOLD(vfs_dcp->dcvfs_rootvp);
	*rootvp = vfs_dcp->dcvfs_rootvp;
	return(0);
}

/*
 * dcvfs_statvfs()
 */
static
dcvfs_statvfs(
	bhv_desc_t		*bdp,		/* vfs dc behavior */
	struct statvfs		*statp,
	vnode_t			*vp)		/* used by nfs to get fhandle */
{
	dcvfs_t			*dcp = BHV_TO_DCVFS(bdp);
	cfs_handle_t		vp_hdl;
	int			error;
	/* REFERENCED */
	int			msgerr;

	/*
	 * typically vp is the root vnode of this FS so we should have
	 * a hard ref on this at the server, but with nfs it looks like this
	 * is not always the case. So in the following invk, just pass the 
	 * vnode handle instead of objid.
	 */
	if (vp)
		cfs_vnexport(vp, &vp_hdl);
	msgerr = invk_dsvfs_statvfs(DCVFS_TO_SERVICE(dcp), DCVFS_TO_OBJID(dcp),
				statp, (vp) ? &vp_hdl : NULL, &error);
	ASSERT(!msgerr);
	return(error);
	
}


/*
 * dcvfs_vget()
 */
static
dcvfs_vget(
	bhv_desc_t		*vfs_bdp,
	vnode_t			**vpp,
	struct fid		*fidp)
{
	dcvfs_t		*dcp = BHV_TO_DCVFS(vfs_bdp);
	cfs_handle_t	vp_hdl;
	int		error;
	/* REFERENCED */
	int		msgerr;

	msgerr = invk_dsvfs_vget(DCVFS_TO_SERVICE(dcp), DCVFS_TO_OBJID(dcp),
				&vp_hdl, fidp, &error);
	ASSERT(!msgerr);
	if (error)
		return(error);
	cfs_vnimport(&vp_hdl, vpp);
	return(0);
}

/*ARGSUSED*/
int
dcvfs_force_pinned (bhv_desc_t      *bdp)
{
	panic("dcvfs_force_pinned: not yet implemented");
	return(0);
}
 
vfsops_t cfs_vfsops = {
	BHV_IDENTITY_INIT_POSITION(VFS_POSITION_CFS_DC),
	dcvfs_mount,		/* mount */
	dcvfs_rootinit,		/* rootinit */
	dcvfs_mntupdate,	/* mntupdate */
#if CELL_IRIX
	dcvfs_dounmount,	/* dounmount */
#else /* CELL_IRIX */
	fs_dounmount,		/* dounmount */
#endif /* CELL_IRIX */
	dcvfs_unmount,		/* unmount */
	dcvfs_root,		/* root */
	dcvfs_statvfs,		/* statvfs */
	dcvfs_sync,		/* sync */
	dcvfs_vget,		/* vget */
	fs_nosys,		/* mountroot */
	fs_nosys,		/* realvfsops */
	fs_nosys,		/* import */
	fs_quotactl,		/* quotactl */
	dcvfs_force_pinned	/* force_pinned */
};
	
	

