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
#ident  "$Revision: 1.21 $"

#include "dv.h"
#include "dvn.h"
#include "dvfs.h"
#include <fs/cell/cfs_intfc.h>
#include <sys/pvnode.h>		/* for PV_NEXT */
#include <sys/pvfs.h>		/* for PVFS_NEXT */
#include <sys/fs_subr.h>	/* for fs_cover */
#include <sys/vnode_private.h>	/* for NESTED_VN_UNLOCK */
#include <sys/uthread.h>
#include <ksys/cred.h>
#include "I_dsvfs_stubs.h"

/*
 * VFS server implementation of remote function calls
 */
	

/************************************************************************
 * These are vfsops being function shipped from client to server cell	*
 * Most of these functions bypass the VFS_XXX macros to avoid getting	*
 * the bhv chain lock. This is OK for now since there's only one VFS 	*
 * interposer and the interposer is removed only when the FS is torn	*
 * down.								*
 * Another assumption is that for some VOPS the VFS ds interposer ops   *
 * are pass through ops. In those cases, it's OK to skip the ds 	*
 * interposer.								*
 ************************************************************************/

#ifdef NOTDEF
void
I_dsvfs_vfsrefuse(
	objid_t		vfs_id,
	cell_t		client,
	tk_set_t	refset,
	tk_disp_t	why)
{
	bhv_desc_t      *bdp = OBJID_TO_BHV(vfs_id);

        tks_return(BHV_TO_DSVFS(bdp)->dsvfs_tserver, client, TK_NULLSET,
                   refset, TK_NULLSET, why);
}
#endif

/*
 * I_dsvfs_vfsreturn()
 */
void
I_dsvfs_vfsreturn(
	objid_t		vfs_id,
	cell_t		client,
	tk_set_t	tset_return,
	tk_set_t	tset_unknown,
	tk_disp_t	why)
{
	dsvfs_t		*dsp;

	dsp = BHV_TO_DSVFS(OBJID_TO_BHV(vfs_id));
	tks_return(dsp->dsvfs_tserver, client, tset_return,
			TK_NULLSET, tset_unknown, why);
}
	
/*
 * I_dsvfs_dounmount() simply calls ds vfs_dounmount
 */
void
I_dsvfs_dounmount(
	objid_t		vfs_id,
	int		flags,
	credid_t	credid,
	cell_t          issuer, 
	int		*errorp)
{
	bhv_desc_t	*bdp = OBJID_TO_BHV(vfs_id);
	struct cred	*cr = CREDID_GETCRED(credid);

	*errorp = dsvfs_dounmount_subr(bdp, flags, NULL, cr, issuer);

	if (cr)
		crfree(cr);
}
	
	
/*
 * I_dsvfs_sync()
 */
void
I_dsvfs_sync(
	objid_t		vfs_id,
	int		flags,
	credid_t	credid,
	int		*error)
{
	bhv_desc_t	*nbdp;
	bhv_desc_t	*bdp = OBJID_TO_BHV(vfs_id);
	struct	cred	*cred = CREDID_GETCRED(credid);

	/* skip the vfs ds behavior since it implements pass-through op */
	PVFS_NEXT(bdp, nbdp);		
	PVFS_SYNC(nbdp, flags, cred, *error);

	if (cred)
		crfree(cred);
}

 
	
/*
 * I_dsvfs_mount()
 * We're on the device server. The dir server has identified this cell
 * as the device server. Now we need to carry out the mount request.
 * A new server vfs will be created on this cell. The dir cell will make
 * requests to instantiate a client vfs after this call succeeds.
 * All he needs from us is the vfs handle of the new vfs.
 */
void
I_dsvfs_vfsmount(
	cfs_handle_t	*cvp_hdlp,		/* in, cover vnode handle */
	struct mounta	*uap,			/* in, mount syscall args */
	char		*attrs,			/* attrs */
	credid_t	credid,			/* in, original process cred */
	cfs_handle_t	*rdir_hdlp,		/* in, original process rdir */
	cfs_handle_t	*cdir_hdlp,		/* in, original process cdir */
	cfs_handle_t	*vfs_hdlp,		/* out, server vfs handle */
	int		*error)			/* out, error return */
{
	vnode_t *cvp;		/* client cover vp on device cell */ 
	vnode_t *rdir, *cdir;
	vnode_t *rdir_save, *cdir_save;
	struct cred	*cred = CREDID_GETCRED(credid);

	/*
	 * set the rdir and cdir to that of the process that made
	 * the mount system call.
	 * XXX R2
	 */
	if (rdir_hdlp)
		cfs_vnimport(rdir_hdlp, &rdir);
	else
		rdir = NULL;
	rdir_save = curuthread->ut_rdir;
	curuthread->ut_rdir = rdir;	/* XXX R2 */
        cfs_vnimport(cdir_hdlp, &cdir);
	cdir_save = curuthread->ut_cdir;
	curuthread->ut_cdir = cdir;	/* XXX R2 */

	/*
	 * import the cover vnode of this new mount.
	 * This also causes the mount hierarchy of the file system
	 * where this cvp resides to be brought in.
	 */
	cfs_vnimport(cvp_hdlp, &cvp);

	/*
	 * skip the vnode dc behavior, needs dc bhv to get back to cvp
	 * in fs_cover(). This is a special case where we don't
	 * follow the behavior chain and call the VOPs sequentially.
	 */
	*error = fs_cover(VNODE_TO_FIRST_BHV(cvp), uap, attrs, cred);
	if (!*error) {
		/* 
		 * sucessful mount, now return vfs handle to the dir cell
		 */
		cfs_vfsexport(cvp->v_vfsmountedhere, vfs_hdlp);
	}
	if (rdir)
		VN_RELE(rdir);
	VN_RELE(cdir);

	if (cred)
		crfree(cred);

	curuthread->ut_rdir = rdir_save;	/* XXX R2 */
	curuthread->ut_cdir = cdir_save;	/* XXX R2 */
}

void
I_dsvfs_vget(
	objid_t		vfs_id,
	cfs_handle_t	*vp_hdlp,	/* out */
	struct	fid	*fidp,
	int		*error)		/* out */
{
	bhv_desc_t	*nvfs_bdp;
	vnode_t		*vp;

	PVFS_NEXT(OBJID_TO_BHV(vfs_id), nvfs_bdp);
	PVFS_VGET(nvfs_bdp, &vp, fidp, *error);
	if (!*error)
		cfs_vnexport(vp, vp_hdlp);
}
	
	
void
I_dsvfs_statvfs(
	objid_t		vfs_id,		/* objid for the vfs */
	struct statvfs	*statp,		/* out, statvfs struct */
	cfs_handle_t	*vp_hdlp,
	int		*error)		/* out */
{
	bhv_desc_t	*nvfs_bdp;
	vnode_t		*vp;

	if (vp_hdlp)
		cfs_vnimport(vp_hdlp, &vp);

	PVFS_NEXT(OBJID_TO_BHV(vfs_id), nvfs_bdp);
	PVFS_STATVFS(nvfs_bdp, statp, vp, *error);

	if (vp_hdlp)
		VN_RELE(vp);
}

/*
 * I_dsvfs_getrootmountpoint()
 * export info for root mount point.
 */
/*ARGSUSED*/
void
I_dsvfs_getrootmountpoint(
	cell_t			client,		/* client cell */
	mount_import_t		*mnti,		/* out, mount point data */
        int             	*errorp,      	/* out, rootfs not mounted */
	cfs_handle_t            **htab,         /* table of dummy vfs handles */
        size_t                  *htsize,        /* size of table */
        void                    **datadesc)     /* for done routine */

{

#if CELL_IRIX
	vnode_t			*rvp;

	*htsize = nfstype;
	*htab = (cfs_handle_t *) kmem_zalloc(sizeof(cfs_handle_t) * nfstype,
					     KM_SLEEP);

	/*
	 * XXX is there a race here with unmounting the root at shudown?
	 */
	if (!(rvp = rootdir)) {
		*errorp = ENOENT;
		return;
	} 

	/*
	 * Get a mount_import structure for the mount point for this client.
	 * If this fails, then there was a race; return ERESTART so the
	 * client can handle it.
	 */
	if (cfs_mountpoint_export(rvp->v_vfsp, rvp, client, mnti))
		*errorp = ERESTART;
	else {
		int 	i;

		for (i = 0; i < nfstype; i++) {
			if (dvfs_dummytab[i])
				cfs_vfsexport(dvfs_dummytab[i], (*htab) + i);
		}
		*errorp = 0;
	}
#else /* CELL_IRIX */
	panic("bad message type");
#endif /* CELL_IRIX */
}
	
/*ARGSUSED*/
void
I_dsvfs_getrootmountpoint_done(
	cfs_handle_t            *htab,         /* table of dummy vfs handles */
        size_t                  htsize,        /* size of table */
        void                    *datadesc)     /* for done routine */
{
        kmem_free(htab, htsize * sizeof(cfs_handle_t));
}

/*
 * I_dsvfs_getmountpoint()
 * Caller  has existence token on covered vnode.
 * Return handles for vfs and root vnode.
 * Also return existence, traverse tokens for vfs, exitence token for vnode.
 * Vfs_flags, vnode type and dev are also returned.
 * Upon return, the ref count on the file system root vnode is held.
 */
void
I_dsvfs_getmountpoint(
	cfs_handle_t		*cvp_hdlp,	/* cvp handle */
	cell_t			client,		/* client cell */
	mount_import_t		*mnti,		/* out, mount point data */
	int			*errorp)	/* out, error */
{
	vnode_t			*cvp, *rvp;
	vfs_t			*vfsp;
	int			s;
	/*REFERENCED*/
	int			unused;

	cfs_vnimport(cvp_hdlp, &cvp);

	s = VN_LOCK(cvp);
	while ((vfsp = cvp->v_vfsmountedhere) != NULL) {
	        if (vfs_busy_trywait_vnlock(vfsp, cvp, s, NULL))
			break;
		s = VN_LOCK(cvp);
	}

	/* file system has been unmounted */
	if (vfsp == NULL) {	
		*errorp = ENOENT;
		VN_UNLOCK(cvp, s);
		VN_RELE(cvp);	/* taken in cfs_vnimport */
		return;
	}

	VN_RELE(cvp);	/* taken in cfs_vnimport */
	VFS_ROOT(vfsp, &rvp, unused);
	ASSERT(unused == 0);		/* can't fail */
	vfs_unbusy(vfsp);
	
	/*
	 * Get a mount_import structure for the mount point for this client.
	 * If this fails, then there was a race; return ERESTART so the
	 * client can handle it.
	 */
	if (cfs_mountpoint_export(vfsp, rvp, client, mnti))
		*errorp = ERESTART;
	else
		*errorp = 0;
	VN_RELE(rvp);
}

/*
 * I_dsvfs_vfsmountedhere()
 *
 * Given a covered vnode, returns handle to the mountedhere vfs in 'vfs_hdl'.
 * If the vnode is no longer covered then return NULL vfs_hdl.
 * Assume that the client has the existence token on the input vnode.
 */
void
I_dsvfs_vfsmountedhere(
	objid_t			cvn_id, 	/* in, vnode obj id */
	cfs_handle_t		*vfs_hdlp,	/* out, vfs handle */
	int			*error)		/* out */
{
	vnode_t		*cvp;
	vfs_t		*vfsp;
	int		s;

	cvp = BHV_TO_VNODE(OBJID_TO_BHV(cvn_id));
	/* keep the vfs from being unmounted while we're exporting its handle */
	s = VN_LOCK(cvp);
	while ((vfsp = cvp->v_vfsmountedhere) != NULL) {
	        if (vfs_busy_trywait_vnlock(vfsp, cvp, s, NULL))
			break;
		s = VN_LOCK(cvp);
	}

	/* file system has been unmounted */
	if (vfsp == NULL) {	
		*error = 0;
		VN_UNLOCK(cvp, s);
		return;
	}

	/* we can now go ahead and export this vfs */
	ASSERT(vfsp->vfs_flag & VFS_CELLULAR);
	cfs_vfsexport(vfsp, vfs_hdlp);
	vfs_unbusy(vfsp);
	*error = 0;
}

	
	
/*
 * I_dsvfs_getmounthdls()
 *
 * Given an input vfs handle, returns handles for the covered vnode and for
 * the vfs in which the covered vnode is located.  We assume that the caller 
 * has made sure that the input vfs cannot be unmounted, although we check
 * the handle for validity to catch any mistakes.
 */
void
I_dsvfs_getmounthdls(
	cfs_handle_t	*in_vfsh,	/* input vfs handle */
	cfs_handle_t	*cov_vnh,	/* out, covered vnode for fs */
	cfs_handle_t	*cov_vfsh)	/* out, vfs containing covered vnode */
{
        dsvfs_t         *dsvfsp;
        vfs_t           *vfsp;
	vnode_t		*cvp;

	dsvfsp = BHV_TO_DSVFS(OBJID_TO_BHV(CFS_HANDLE_TO_OBJID(*in_vfsh)));
	ASSERT(DSVFS_TO_GEN(dsvfsp) == CFS_HANDLE_TO_GEN(*in_vfsh));
	vfsp = DSVFS_TO_VFS(dsvfsp);

	if (vfsp == rootvfs) {
		CFS_HANDLE_MAKE_NULL(*cov_vnh);
		CFS_HANDLE_MAKE_NULL(*cov_vfsh);
		return;
	}
	
        /* caller has a vnode reference preventing this vfs to be unmounted */
	cvp = vfsp->vfs_vnodecovered;
	cfs_vnexport(cvp, cov_vnh);
	cfs_vfsexport(cvp->v_vfsp, cov_vfsh);
}	


