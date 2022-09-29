/**************************************************************************
 *									  *
 *		 Copyright (C) 1997 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Id: cxfs_vfs.c,v 1.3 1997/09/15 15:17:43 henseler Exp $"

#include <sys/sysmacros.h>
#include <sys/kmem.h>
#include <sys/file.h>
#include <sys/cred.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <ksys/hwg.h>
#include <fs/specfs/spec_lsnode.h>
#include "cxfs.h"
#include "dcxvfs.h"

extern	dcvfs_t	*dcvfs_vfs_to_dcvfs(vfs_t *);
extern	int	xfs_mountfs_int(vfs_t *, xfs_mount_t *, dev_t, int, int);
extern cred_t *sys_cred;

typedef struct {
        uint    x_flags;
	dev_t	x_dev;
	dev_t	x_logdev;
	dev_t	x_rtdev;
        int     x_swidth;
        int     x_dalign;
} xfs_expinfo_t;


extern  int	printf(char *fmt, ...);

zone_t  *dcxvfs_zone;

void	dcxvfs_init()
{
	dcxvfs_zone = kmem_zone_init(sizeof(dcxvfs_t), "dcxvfs");
}

dcxvfs_t *
cxfs_dcvfs_make(
	vfs_t *vfsp,
	dcvfs_t *dcvfs)
{
	dcxvfs_t *dcxvfs = kmem_zone_alloc(dcxvfs_zone, KM_SLEEP);

	ASSERT(vfsp->vfs_eisize == sizeof(xfs_expinfo_t));

	dcxvfs->dcxv_vfs = vfsp;
	dcxvfs->dcxv_dcvfs = dcvfs;
	dcxvfs->dcxv_mp = (xfs_mount_t *)vfsp->vfs_expinfo;

	return(dcxvfs);
}

/* This is really our unmount call - do the right thing here,
 * flush buffers etc. For not just close the spec devices.
 */
void	cxfs_dcvfs_destroy(
	dcxvfs_t *dcxvfs)
{
	xfs_mount_t	*mp = dcxvfs->dcxv_mp;
	/* REFERENCED */
	int		noerr;
	buf_t		*sbp;

	if (mp) {
		int	vfs_flags;

		vfs_flags = (dcxvfs->dcxv_vfs->vfs_flag & VFS_RDONLY) ? FREAD :
			     FREAD|FWRITE;

		if (mp->m_ddevp) {
			VOP_CLOSE(mp->m_ddevp, vfs_flags, L_TRUE, sys_cred,
				  noerr);
			VN_RELE(mp->m_ddevp);
		}
		if (mp->m_rtdevp) {
			VOP_CLOSE(mp->m_rtdevp, vfs_flags, L_TRUE, sys_cred,
				 noerr);
			VN_RELE(mp->m_rtdevp);
		}

		if (mp->m_logdevp && mp->m_logdevp != mp->m_ddevp) {
			VOP_CLOSE(mp->m_logdevp, vfs_flags, L_TRUE, sys_cred,
				  noerr);
			VN_RELE(mp->m_logdevp);
		}
		sbp = xfs_getsb(mp, 0);
		nfreerbuf(sbp);
		
		kmem_free(mp, sizeof(xfs_mount_t));
	}
	kmem_zone_free(dcxvfs_zone, dcxvfs);
}

int	cxfs_import(vfs_t *vfsp)
{
	dcvfs_t		*dcvfs;
	xfs_mount_t	*mp;
	xfs_expinfo_t	*xfs_expinfo;
	vnode_t		*ddevvp, *rdevvp;
	int		vfs_flags;
	int		error = 0;
	/* REFERENCED */
	int		noerr;

	dcvfs = dcvfs_vfs_to_dcvfs(vfsp);
	ASSERT(dcvfs != NULL);

	/* Using info from the export structure, fill in the
	 * local copy of the mount structure.
	 */

	xfs_expinfo = (xfs_expinfo_t *) vfsp->vfs_expinfo;
	ASSERT(vfsp->vfs_eisize == sizeof (xfs_expinfo_t));

	mp = xfs_mount_init();
	mp->m_flags	= xfs_expinfo->x_flags;
	mp->m_swidth	= xfs_expinfo->x_swidth;
	mp->m_dalign	= xfs_expinfo->x_dalign;

	mp->m_dev	= xfs_expinfo->x_dev;
	mp->m_logdev	= xfs_expinfo->x_logdev;
	mp->m_rtdev	= xfs_expinfo->x_rtdev;

	/* Open the block device for the filesystem so we can do
	 * reads/writes from this cell.
	 */

	vfs_flags = (vfsp->vfs_flag & VFS_RDONLY) ? FREAD : FREAD|FWRITE;
	if (mp->m_dev != NODEV) {
		vnode_t *openvp = NULL;

		ddevvp = openvp = make_specvp(mp->m_dev, VBLK);

		VOP_OPEN(openvp, &ddevvp, vfs_flags, sys_cred, error);
		if (error) {
			VN_RELE(ddevvp);
			goto error0;
		}
		mp->m_ddevp = ddevvp;
		mp->m_ddev_targp = &mp->m_ddev_targ;
	} else {
		ddevvp = NULL;
	}
	ASSERT(ddevvp != NULL);
        if (mp->m_rtdev != NODEV) {
                vnode_t *openvp;

		rdevvp = openvp = make_specvp(mp->m_rtdev, VBLK);

                VOP_OPEN(openvp, &rdevvp, vfs_flags, sys_cred, error);
                if (error) {
                        VN_RELE(rdevvp);
                        goto error1;
                }
                mp->m_rtdevp = rdevvp;
        } else {
                rdevvp = NULL;
        }

#if XFS_BIG_FILESYSTEMS
        mp->m_inoadd = 0;
#endif

#if NOTDEF
	error = xfs_mountfs_int(vfsp, mp, mp->m_dev, 0, 1);
#endif

	if (!error) {
		vfsp->vfs_expinfo = (void *)mp;
		return(0);
	}

        if (rdevvp) {
                VOP_CLOSE(rdevvp, vfs_flags, L_TRUE, sys_cred, noerr);
                binval(mp->m_rtdev);
                VN_RELE(rdevvp);
        }
 error1:
        if (ddevvp) {
                VOP_CLOSE(ddevvp, vfs_flags, L_TRUE, sys_cred, noerr);
                binval(mp->m_dev);
                VN_RELE(ddevvp);
        }

 error0:
        kmem_free(mp, sizeof(xfs_mount_t));
        return error;
}

void	cxfs_export(
	vfs_t		*vfsp,
	xfs_mount_t	*mp)
{
	xfs_expinfo_t	*xfs_expinfo = (xfs_expinfo_t *)mp->m_export;

	ASSERT(sizeof(xfs_expinfo_t) < VFS_EILIMIT);

	xfs_expinfo->x_flags = mp->m_flags;
	xfs_expinfo->x_swidth = mp->m_swidth;
	xfs_expinfo->x_dalign = mp->m_dalign;
	xfs_expinfo->x_dev	= mp->m_dev;
	xfs_expinfo->x_logdev	= mp->m_logdev;
	xfs_expinfo->x_rtdev	= mp->m_rtdev;

	VFS_EXPINFO(vfsp, xfs_expinfo, sizeof(xfs_expinfo_t));
}
