/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef _SYS_PVFS_H
#define _SYS_PVFS_H

#ident	"$Revision: 1.4 $"

/*
 * Include file associated with vfs behaviors.
 */
#include <ksys/behavior.h>

/*
 * Macro to fetch the next behavior descriptor.
 */
#define PVFS_NEXT(cur, next)						\
{       								\
        next = BHV_NEXT(cur);  						\
	ASSERT(next);							\
}

/* 
 * PVFS's.  Operates on a behavior descriptor pointer.  
 * Unlike VOP's, no ops-in-progress synchronization is done.
 */
#define PVFS_MNTUPDATE(bdp, mvp, uap, cr, rv) \
{       \
        rv = (*((vfsops_t *)(bdp)->bd_ops)->vfs_mntupdate)(bdp, mvp, uap, cr);	\
}
#define PVFS_DOUNMOUNT(bdp, f, vp, cr, rv)    \
{       \
        rv = (*((vfsops_t *)(bdp)->bd_ops)->vfs_dounmount)(bdp, f, vp, cr); \
}
#define PVFS_UNMOUNT(bdp,f,cr, rv)      \
{       \
        rv = (*((vfsops_t *)(bdp)->bd_ops)->vfs_unmount)(bdp, f, cr);   \
}
#define PVFS_ROOT(bdp, vpp, rv)         \
{       \
        rv = (*((vfsops_t *)(bdp)->bd_ops)->vfs_root)(bdp, vpp);        \
}
#define PVFS_STATVFS(bdp, sp, vp, rv)   \
{       \
        rv = (*((vfsops_t *)(bdp)->bd_ops)->vfs_statvfs)(bdp, sp, vp);  \
}
#define PVFS_SYNC(bdp, flag, cr, rv) \
{       \
        rv = (*((vfsops_t *)(bdp)->bd_ops)->vfs_sync)(bdp, flag, cr);   \
}
#define PVFS_VGET(bdp, vpp, fidp, rv) \
{       \
        rv = (*((vfsops_t *)(bdp)->bd_ops)->vfs_vget)(bdp, vpp, fidp); 	\
}
#define PVFS_MOUNTROOT(bdp, why, rv) \
{       \
        rv = (*((vfsops_t *)(bdp)->bd_ops)->vfs_mountroot)(bdp, why);	\
}
#define PVFS_FORCE_PINNED(bdp, rv) \
{       \
        rv = (*((vfsops_t *)(bdp)->bd_ops)->vfs_force_pinned)(bdp);   \
}


/*
 * Modules often will interpose on a few functions, and pass thru many others.
 */
extern int vfs_passthru_vfs_mntupdate(bhv_desc_t *, struct vnode *,  
				struct mounta *, struct cred *);
extern int vfs_passthru_vfs_dounmount(bhv_desc_t *, int, struct vnode *, 
				      struct cred *);
extern int vfs_passthru_vfs_unmount(bhv_desc_t *, int, struct cred *);
extern int vfs_passthru_vfs_root(bhv_desc_t *, struct vnode **);
extern int vfs_passthru_vfs_statvfs(bhv_desc_t *, struct statvfs *, 
				    struct vnode *);
extern int vfs_passthru_vfs_sync(bhv_desc_t *, int, struct cred *);
extern int vfs_passthru_vfs_vget(bhv_desc_t *, struct vnode **, struct fid *);
extern int vfs_passthru_vfs_mountroot(bhv_desc_t *, enum whymountroot);

#endif /* _SYS_PVFS_H */

