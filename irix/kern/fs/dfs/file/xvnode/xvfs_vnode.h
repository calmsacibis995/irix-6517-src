/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: xvfs_vnode.h,v $
 * Revision 65.10  1999/07/21 19:01:14  gwehrman
 * Defined SGIMIPS for this development header.  Fix for bug 657352.
 *
 * Revision 65.9  1998/10/06 19:06:20  lmc
 * Add missing ")" to VOPX_LINK.  Mistakenly checked in the
 * wrong file.  This is for PV 637459.
 *
 * Revision 65.8  1998/09/30 20:21:24  lmc
 * A lookup is done to find the DFS behavior.  This behavior is now used by
 * VOPX_LINK so that the correct vnodeops are being used.  When imon_link
 * was stacked above DFS, we were using the imon vnodeops to try to find
 * cm_link instead of our vnodeops.  This is a fix for PV 637459.
 *
 * Revision 65.7  1998/09/23  14:46:49  bdr
 * Changed ifdef usage of AFS_IRIX_ENV to use the standard SGIMIPS define
 * instead.  Changed the map function args in the xvfs_xops (VOPX calls)
 * struct.  Changed the args that the VOPX_MAP macro passes to reflect
 * the args that are needed by the new sgi cm_map.
 *
 * Revision 65.6  1998/07/06  20:50:47  bdr
 * Change DFS_BLKSIZE to be a 64 bit type.  Also Change SZTODFSBLKS macro
 * so that we get a full 64 bits even on 32 bit clients.
 *
 * Revision 65.5  1998/04/02  19:44:39  bdr
 * Changed function args expected in xvfs_xops for vn_getlength to match
 * the new types.
 *
 * Revision 65.4  1998/04/01  14:16:45  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Changed the prototype for vn_fid to match the xfs implementation.  The
 * 	second parameter in now a struct fid ** instead of a fid *.  Added
 * 	extern declarations for xvn_getacl() and xvn_setacl().
 *
 * Revision 65.3  1998/02/02 21:45:41  lmc
 * Changes for prototypes, explicit casts added after checking
 * that they are okay.
 *
 * Revision 65.1  1997/10/20  19:20:24  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.939.1  1996/10/02  19:04:14  damon
 * 	New DFS from Transarc
 * 	[1996/10/01  18:59:43  damon]
 *
 * $EndLog$
 */
/* Copyright (C) 1996, 1989 Transarc Corporation - All rights reserved */
/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/xvnode/RCS/xvfs_vnode.h,v 65.10 1999/07/21 19:01:14 gwehrman Exp $ */

#ifndef	TRANSARC_XVNODE_H
#define	TRANSARC_XVNODE_H
#ifndef SGIMIPS
#define SGIMIPS
#endif /* !SGIMIPS */
#include <dcedfs/volume.h>
#include <dcedfs/aggr.h>
#include <dcedfs/common_data.h>
#include <dcedfs/osi.h>
#include <dcedfs/osi_cred.h>
#include <dcedfs/tkm_tokens.h>		/* tkm_localCellID */
#include <dcedfs/osi_buf.h>
#include <dcedfs/aclint.h>

#ifdef	AFS_AIX_ENV
#include <dcedfs/xvfs_aixvnode.h>
#endif	/* AFS_AIX_ENV */

#ifdef	AFS_OSF_ENV
#include <dcedfs/xvfs_osfvnode.h>
#endif	/*AFS_OSF_ENV */

#ifdef	AFS_HPUX_ENV
#include <dcedfs/xvfs_osvnode.h>
#endif	/* AFS_HPUX_ENV */

#ifdef	SGIMIPS
#include <dcedfs/xvfs_osvnode.h>
#endif	/* AFS_HPUX_ENV */

#ifdef AFS_SUNOS5_ENV
#include <dcedfs/xvfs_sun5vnode.h>
#endif /* AFS_SUNOS5_ENV */

#ifdef	AFS_DEFAULT_ENV
#error  "xvfs_vnode.h: needs op definitions"
#endif	/* AFS_DEFAULT_ENV */

#include <dcedfs/xvfs_xattr.h>

/* Exported structures and functions are preceded by module name xvfs_ */

/* 
 * Define constant used in v_flag field to indicate that the vnode has already
 * been converted.
 */
#ifdef SGIMIPS
#define V_CONVERTED		VDFSCONVERTED	/* looks out of the way for now */
#else
#ifndef AFS_OSF_ENV
#define V_CONVERTED		0x1000	/* looks out of the way for now */
#endif
#endif /* SGIMIPS */

#ifdef	AFS_AIX31_ENV
#define IS_CONVERTED(vp)	((vp)->v_gnode->gn_flags & V_CONVERTED)
#define SET_CONVERTED(vp)	(vp)->v_gnode->gn_flags |= V_CONVERTED
#define SET_UNCONVERTED(vp)	(vp)->v_gnode->gn_flags &= ~V_CONVERTED
#else
#define IS_CONVERTED(vp)	((vp)->v_flag & V_CONVERTED)
#define SET_CONVERTED(vp)	(vp)->v_flag |= V_CONVERTED
#define SET_UNCONVERTED(vp)	(vp)->v_flag &= ~V_CONVERTED
#endif
#define XVFS_CONVERT(vp)	(IS_CONVERTED(vp) ? 0 : xvfs_convert(vp))
#define	xvfs_PutVolume(volp)	if (volp) VOL_RELE(volp)

#ifdef SGIMIPS
#define DFS_BLKSIZE		65536LL
#define SZTODFSBLKS(x)		((x+DFS_BLKSIZE-1) & ~(DFS_BLKSIZE-1LL))
#endif /* SGIMIPS */
/* extended VOPX operations; note that all attr-taking dudes
 * take xvattrs, not vattrs.
 */
struct xvfs_xops {
    int	(*vn_open)(struct vnode **, long, osi_cred_t *);
    int	(*vn_close)(struct vnode *, long, osi_cred_t *);
    int	(*vn_rdwr)(struct vnode *, struct uio *, enum uio_rw, int, osi_cred_t *);
#ifdef SGIMIPS
    int	(*vn_ioctl)(struct vnode *, int, void *);
#else
    int	(*vn_ioctl)(struct vnode *, int, int);
#endif
    int	(*vn_select)(struct vnode *, int, osi_cred_t *);
    int	(*vn_getattr)(struct vnode *, struct xvfs_attr *, int, osi_cred_t *);
    int	(*vn_setattr)(struct vnode *, struct xvfs_attr *, int, osi_cred_t *);
    int	(*vn_access)(struct vnode *, int, osi_cred_t *);
    int	(*vn_lookup)(struct vnode *, char *, struct vnode **, osi_cred_t *);
    int	(*vn_create)(struct vnode *, char *, struct vattr *, int, int,
		     struct vnode **, osi_cred_t *);
    int	(*vn_remove)(struct vnode *, char *, osi_cred_t *);
    int	(*vn_link)(struct vnode *, struct vnode *, char *, osi_cred_t *);
    int	(*vn_rename)(struct vnode *, char *, struct vnode *, char *,
		     osi_cred_t *);
    int	(*vn_mkdir)(struct vnode *, char *, struct vattr *, struct vnode **,
		    osi_cred_t *);
    int	(*vn_rmdir)(struct vnode *, char *, struct vnode *, osi_cred_t *);
    int	(*vn_readdir)(struct vnode *, struct uio *, osi_cred_t *, int *, int);
    int	(*vn_symlink)(struct vnode *, char *, struct vattr *, char *,
		      osi_cred_t *);
    int	(*vn_readlink)(struct vnode *, struct uio *, osi_cred_t *);
#ifdef SGIMIPS
    int	(*vn_fsync)(struct vnode *, int, osi_cred_t *, off_t, off_t);
#else
    int	(*vn_fsync)(struct vnode *, osi_cred_t *, off_t, off_t);
#endif
    int	(*vn_inactive)(struct vnode *, osi_cred_t *);
    int	(*vn_bmap)(struct vnode *, long, struct vnode **, long *);
#ifdef AFS_SUNOS5_ENV
    int	(*vn_strategy)(struct buf *, osi_cred_t *);
    int	(*vn_ustrategy)(struct buf *, osi_cred_t *); /* assuming stuff already mapped in */
#else /* AFS_SUNOS5_ENV */
    int	(*vn_strategy)(struct buf *);
    int	(*vn_ustrategy)(struct buf *);	/* assuming stuff already mapped in */
#endif /* AFS_SUNOS5_ENV */
    int	(*vn_bread)(struct vnode *, daddr_t, struct buf **);
    int	(*vn_brelse)(struct vnode *, struct buf *);
#ifdef AFS_SUNOS5_ENV
    int	(*vn_lockctl)(struct vnode *, int, struct flock *, int, offset_t,
		      osi_cred_t *);
#else /* !AFS_SUNOS5_ENV */
    int	(*vn_lockctl)(struct vnode *, struct flock *, int, osi_cred_t *, int,
		      long);
#endif /* AFS_SUNOS5_ENV */
	/* op for old style fid op: */
#if defined(AFS_SUNOS54_ENV)
    int (*vn_fid)(struct vnode *, struct fid *);
#else
    int (*vn_fid)(struct vnode *, struct fid **);
#endif
    int (*vn_hold)(struct vnode *);	/* maybe don't need; revisit later */
    int (*vn_rele)(struct vnode *);
    /*
     * new ones for us to provide, rather than just existing to
     * make writing the O functions easier (i.e. porting).
     */
    int (*vn_setacl)(struct vnode *, struct dfs_acl *, struct vnode *, int, int,
		     osi_cred_t *);
    int (*vn_getacl)(struct vnode *, struct dfs_acl *, int, osi_cred_t *);
    int	(*vn_afsfid)(struct vnode *, struct afsFid *, int);
    int (*vn_getvolume)(struct vnode *, struct volume **);
#ifdef SGIMIPS
    int (*vn_getlength)(struct vnode *, afs_hyper_t *, osi_cred_t *);
#else
    int (*vn_getlength)(struct vnode *, long *, osi_cred_t *);
#endif
    /*
     * Some new ops for AIX 3 and SunOS 5
     */
#ifndef AFS_SUNOS5_ENV
#ifdef SGIMIPS
    int (*vn_map)(struct vnode *, u_int);
#else  /* SGIMIPS */
    int (*vn_map)(struct vnode *, caddr_t, u_int, u_int, u_int);
#endif /* SGIMIPS */
#else /* AFS_SUNOS5_ENV */
    int (*vn_map)(struct vnode *, offset_t, struct as *, caddr_t *, u_int,
		  u_char, u_char, u_int, osi_cred_t *);
#endif /* AFS_SUNOS5_ENV */
    int (*vn_unmap)(struct vnode *, int);
    /*
     * A new op for OSF/1
     */
    int (*vn_reclaim)(struct vnode *);
    /*
     * Some new ops for SunOS 5 
     */
    int (*vn_read)(struct vnode *, struct uio *, int, osi_cred_t *);
    int (*vn_write)(struct vnode *, struct uio *, int, osi_cred_t *);
    int (*vn_realvp)(struct vnode *, struct vnode **);
    void (*vn_rwlock)(struct vnode *, int);
    void (*vn_rwunlock)(struct vnode *, int);
#if defined(AFS_SUNOS5_ENV) && defined(KERNEL)
    int (*vn_seek)(struct vnode *, offset_t, offset_t *);
    int (*vn_space)(struct vnode *, int, struct flock *, int, offset_t,
		    osi_cred_t *);
    int (*vn_getpage)(struct vnode *, offset_t, u_int, u_int *, page_t *[],
		      u_int, struct seg *, caddr_t, enum seg_rw, osi_cred_t *);
    int (*vn_putpage)(struct vnode *, offset_t, u_int, int, osi_cred_t *);
    int (*vn_addmap)(struct vnode *, offset_t, struct as *, caddr_t, u_int,
		     u_char, u_char, u_int, osi_cred_t *);
    int (*vn_delmap)(struct vnode *, offset_t, struct as *, caddr_t, u_int,
		     u_int, u_int, u_int, osi_cred_t *);
    int (*vn_pageio)(struct vnode *, page_t *, u_int, u_int, int, osi_cred_t *);
#else /* AFS_SUNOS5_ENV && KERNEL */
    int (*vn_seek)();
    int (*vn_space)();
    int (*vn_getpage)();
    int (*vn_putpage)();
    int (*vn_addmap)();
    int (*vn_delmap)();
    int (*vn_pageio)();
#endif /* AFS_SUNOS5_ENV && KERNEL */
#define   vn_frlock  vn_lockctl		/* overlay equivalent ops */
    /*
     * Ops for HP/UX
     */
    int (*vn_pagein)();
    int (*vn_pageout)();
    /*
     * Another new op for SunOS 5
     */
    int (*vn_setfl)(struct vnode *, int, int, osi_cred_t *);
    /*
     * More new ops for SunOS 5 (5.4)
     */
#if defined(AFS_SUNOS54_ENV) && defined(KERNEL)
    void (*vn_dispose)(struct vnode *, page_t *, int, int, osi_cred_t *);
    int (*vn_setsecattr)(struct vnode *, vsecattr_t *, int, osi_cred_t *);
    int (*vn_getsecattr)(struct vnode *, vsecattr_t *, int, osi_cred_t *);
#else /* AFS_SUNOS54_ENV && KERNEL */
    void (*vn_dispose)();
    int (*vn_setsecattr)();
    int (*vn_getsecattr)();
#endif /* AFS_SUNOS54_ENV && KERNEL */
};


/*
 * Operations on the Enhanced vnodes.
 */
struct xvfs_vnodeops {
    struct osi_vnodeops oops;		/* Glued vendor vnodeops */
    struct xvfs_xops xops;	      	/* Enhanced (Decorum) vnodeops */ 
    struct osi_vnodeops nops;		/* Original vendor vnodeops */
};

/*
 * Macros for extended vnodeops
 */
/*
 * The VOPX call assumed the first behavior was a dfs behavior.
 * This was ok if there were no inserted behaviors.
 * Check the bhv chain attached to this vnode and execute the 
 * correct behavior.
 */
#ifdef SGIMIPS
/*  All of the macros reference xvfs_ops, so I added the extern
	to define it here.  */
extern struct xvfs_vnodeops *xvfs_ops;
#endif

#ifndef	EFS_SINGLE
#define VOPX_OPEN(VP,F,C,rv) \
{					\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(*(VP))->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(*(VP)),xvfs_ops); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_open) (VP,F,C); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_CLOSE(VP,F,C,rv) \
{ 			\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_close) (VP,F,C); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_RDWR(VP,UIOP,RW,F,C,rv) \
{				\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_rdwr) (VP,UIOP,RW,F,C); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_IOCTL(VP,C,D,rv) \
{				\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_ioctl) (VP,C,D); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_SELECT(VP,W,C,rv) \
{			\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_select) (VP,W,C); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_GETATTR(VP,VA,F,C,rv) \
{				\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_getattr) (VP,VA,F,C); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_SETATTR(VP,VA,F,C,rv) \
{				\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_setattr) (VP,VA,F,C); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_ACCESS(VP,M,C,rv) \
{				\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_access) (VP,M,C); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_LOOKUP(VP,NM,VPP,C,rv) \
{				\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_lookup) (VP,NM,VPP,C); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_CREATE(VP,NM,VA,E,M,VPP,C,rv) \
{					\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_create) (VP,NM,VA,E,M,VPP,C); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_REMOVE(VP,NM,C,rv) \
{				\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_remove) (VP,NM,C); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_LINK(VP,TDVP,TNM,C,rv) \
{				\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_link) (VP,TDVP,TNM,C); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_RENAME(VP,NM,TDVP,TNM,C,rv) \
{					\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_rename) (VP,NM,TDVP,TNM,C); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_MKDIR(VP,NM,VA,VPP,C,rv) \
{				\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_mkdir) (VP,NM,VA,VPP,C); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_RMDIR(VP,NM,CD,C,rv) \
{				\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_rmdir) (VP,NM,CD,C); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_READDIR(VP,UIOP,C,EP,X,rv) \
{					\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_readdir) (VP,UIOP,C,EP,X); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_SYMLINK(VP,LNM,VA,TNM,C,rv) \
{				\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_symlink) (VP,LNM,VA,TNM,C); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_READLINK(VP,UIOP,C,rv) \
{				\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_readlink) (VP,UIOP,C); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_FSYNC(VP,F,C,S,SP,rv) \
{			\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_fsync)  \
		(VP,F,C,S,SP); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_INACTIVE(VP,C,rv) \
{				\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_inactive) (VP,C); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_BMAP(VP,BN,VPP,BNP,rv) \
{				\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_bmap) (VP,BN,VPP,BNP); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_STRATEGY(VP,BP,rv) \
{				\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_strategy) (BP); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_USTRATEGY(VP,BP,rv) \
{				\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_ustrategy) (BP); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_BREAD(VP,BN,BPP,rv) \
{				\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_bread) (VP,BN,BPP); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_BRELSE(VP,BP,rv) \
{				\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_brelse) (VP,BP); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_LOCKCTL(VP,LD,CMD,C,ID,OFF,rv) \
{					\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_lockctl) (VP,LD,CMD,C,ID,OFF); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_FID(VP, FIDP,rv) \
{				\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)(BHV_OPS(bdp)))->xops.vn_fid)(VP, FIDP); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_HOLD(VP,rv) \
{			\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_hold) (VP); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_RELE(VP,rv) \
{				\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_rele) (VP); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define	VOPX_SETACL(VP,ACL,SVP,DW,SW,C,rv) \
{					\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_setacl) (VP,ACL,SVP,DW,SW,C); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define	VOPX_GETACL(VP,ACL,W,C,rv) \
{					\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_getacl) (VP,ACL,W,C); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_AFSFID(VP, FIDPP, VF, rv) \
{					\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_afsfid) (VP, FIDPP, VF); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_GETVOLUME(VP, VOLPP, rv) \
{					\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_getvolume) (VP, VOLPP); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_GETLENGTH(VP,LENGTH,C,rv) \
{					\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_getlength) (VP,LENGTH,C); \
    VN_BHV_READ_UNLOCK(bhvh); \
}

#ifdef	AFS_AIX31_ENV

#define VOPX_MAP(VP,ADDR,LEN,OFF,FL) \
    (*((struct xvfs_vnodeops *)(VP)->v_op)->xops.vn_map) (VP,ADDR,LEN,OFF,FL)
#define VOPX_UNMAP(VP,FL) \
    (*((struct xvfs_vnodeops *)(VP)->v_op)->xops.vn_unmap) (VP,FL)

#endif

#ifdef	AFS_OSF_ENV

#define VOPX_RECLAIM(VP,rv) \
{				\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)((VP)->v_fops))->xops.vn_reclaim)(VP); \ 
    VN_BHV_READ_UNLOCK(bhvh); \
}
#endif

#ifdef	AFS_SUNOS5_ENV

#define VOPX_MAP(VP,O,AS,AP,L,P,MP,F,C) \
    (*((struct xvfs_vnodeops *)((VP)->v_op))->xops.vn_map) \
	(VP, O, AS, AP, L, P, MP, F, C)
#define VOPX_READ(VP,UIOP,F,C) \
    (*((struct xvfs_vnodeops *)((VP)->v_op))->xops.vn_read) (VP, UIOP, F, C)
#define VOPX_WRITE(VP,UIOP,F,C) \
    (*((struct xvfs_vnodeops *)((VP)->v_op))->xops.vn_write) (VP, UIOP, F, C)
#define VOPX_REALVP(VP1,VP2P) \
    (*((struct xvfs_vnodeops *)((VP1)->v_op))->xops.vn_realvp) (VP1, VP2P)
#define VOPX_RWLOCK(VP,W) \
    (*((struct xvfs_vnodeops *)((VP)->v_op))->xops.vn_rwlock) (VP, W)
#define VOPX_RWUNLOCK(VP,W) \
    (*((struct xvfs_vnodeops *)((VP)->v_op))->xops.vn_rwunlock) (VP, W)
#define VOPX_SEEK(VP,OOFF,NOFFP) \
    (*((struct xvfs_vnodeops *)((VP)->v_op))->xops.vn_seek) (VP, OOFF, NOFFP)
#define VOPX_SPACE(VP,C,FLP,F,O,CR) \
    (*((struct xvfs_vnodeops *)((VP)->v_op))->xops.vn_space) \
	(VP, C, FLP, F, O, CR)
#define VOPX_GETPAGE(VP,O,L,PP,PL,PLSZ,S,A,RW,C) \
    (*((struct xvfs_vnodeops *)((VP)->v_op))->xops.vn_getpage) \
	(VP, O, L, PP, PL, PLSZ, S, A, RW, C)
#define VOPX_PUTPAGE(VP,O,L,F,C) \
    (*((struct xvfs_vnodeops *)((VP)->v_op))->xops.vn_putpage) (VP, O, L, F, C)
#define VOPX_ADDMAP(VP,O,AS,A,L,P,MP,F,C) \
    (*((struct xvfs_vnodeops *)((VP)->v_op))->xops.vn_addmap) \
	(VP, O, AS, A, L, P, MP, F, C)
#define VOPX_DELMAP(VP,O,AS,A,L,P,MP,F,C) \
    (*((struct xvfs_vnodeops *)((VP)->v_op))->xops.vn_delmap) \
	(VP, O, AS, A, L, P, MP, F, C)
#define VOPX_PAGEIO(VP, PP, LEN, OFF, FLAGS, CR) \
    (*((struct xvfs_vnodeops *)((VP)->v_op))->xops.vn_pageio) \
	(VP, PP, LEN, OFF, FLAGS, CR)
#define VOPX_FRLOCK(VP,C,FLP,F,O,CR) \
    (*((struct xvfs_vnodeops *)((VP)->v_op))->xops.vn_frlock) \
	(VP, C, FLP, F, O, CR)
#define VOPX_SETFL(VP,OF,NF,CR) \
    (*((struct xvfs_vnodeops *)((VP)->v_op))->xops.vn_setfl) (VP, OF, NF, CR)
#define VOPX_DISPOSE(VP,PP,F,DN,CR) \
    (*((struct xvfs_vnodeops *)((VP)->v_op))->xops.vn_dispose) \
	(VP, PP, F, DN, CR)
#define VOPX_SETSECATTR(VP,VSAP,F,CR) \
    (*((struct xvfs_vnodeops *)((VP)->v_op))->xops.vn_setsecattr) \
	(VP, VSAP, F, CR)
#define VOPX_GETSECATTR(VP,VSAP,F,CR) \
    (*((struct xvfs_vnodeops *)((VP)->v_op))->xops.vn_getsecattr) \
	(VP, VSAP, F, CR)

#endif

#ifdef	AFS_HPUX_ENV

#define VOPX_PAGEIN(VP,PRP,W,SP,VA,S) \
    (*((struct xvfs_vnodeops *)((VP)->v_op))->xops.vn_pagein) (PRP, W, SP, VA, S)
#define VOPX_PAGEOUT(VP,PRP,S,E,F) \
    (*((struct xvfs_vnodeops *)((VP)->v_op))->xops.vn_pageout) (PRP, S, E, F)

#endif

#ifdef SGIMIPS
/* Some of this stuff is similar to SUNOS5, but there are pieces which differ
 * and so instead of filling the place up with #ifdefs, we have this. - brat
 */
#define VOPX_MAP(VP,F,rv) \
{					\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_map) (VP, F); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_READ(VP,UIOP,F,C,rv) \
{						\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_read) (VP, UIOP, F, C); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_WRITE(VP,UIOP,F,C,rv) \
{						\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_write) (VP, UIOP, F, C); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_REALVP(VP1,VP2P,rv) \
{						\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP1)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)((VP1)->v_fops))->xops.vn_realvp) (VP1, VP2P); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_RWLOCK(VP,W,rv) \
{						\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_rwlock) (VP, W); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_RWUNLOCK(VP,W,rv) \
{						\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_rwunlock) (VP, W); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_SEEK(VP,OOFF,NOFFP,rv) \
{						\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_seek) (VP, OOFF, NOFFP); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_ADDMAP(VP,MT,VT,PT,O,L,P,C,rv) \
{						\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_addmap) \
		(VP, MT, VT, PT, O, L, P, C); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_DELMAP(VP,VT,S,C,rv) \
{						\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_delmap)     \
		(VP, VT, S, C); 				\
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_FRLOCK(VP,C,FLP,F,O,CR,rv) \
{					\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_frlock) (VP, C, FLP, F, O, CR); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#define VOPX_RECLAIM(VP,rv) \
{					\
    bhv_desc_t *bdp; \
    bhv_head_t *bhvh = &(VP)->v_bh;	\
    bdp = bhv_lookup_unlocked(VN_BHV_HEAD(VP),xvfs_ops); \
    ASSERT(bdp != NULL); \
    VN_BHV_READ_LOCK(bhvh); 	\
    rv = (*((struct xvfs_vnodeops *)BHV_OPS(bdp))->xops.vn_reclaim)(VP); \
    VN_BHV_READ_UNLOCK(bhvh); \
}
#endif /* SGIMIPS */
#else	/* EFS_SINGLE */

#define	VOPX_OPEN	efs_null
#define	VOPX_CLOSE	efs_null
#define	VOPX_RDWR	efs_rdwr
#define	VOPX_IOCTL	efs_invalid
#define	VOPX_SELECT	efs_invalid
#define	VOPX_GETATTR	efs_getxattr
#define	VOPX_SETATTR	efs_setxattr
#define	VOPX_ACCESS	efs_access
#define	VOPX_LOOKUP	efs_lookup
#define	VOPX_CREATE	efs_create
#define	VOPX_REMOVE	efs_remove
#define	VOPX_LINK	efs_link
#define	VOPX_RENAME	efs_rename
#define	VOPX_MKDIR	efs_mkdir
#define	VOPX_RMDIR	efs_rmdir
#define	VOPX_READDIR	efs_readdir
#define	VOPX_SYMLINK	efs_symlink
#define	VOPX_READLINK	efs_readlink
#define	VOPX_FSYNC	efs_fsync
#define	VOPX_INACTIVE	efs_inactive
#define	VOPX_BMAP	efs_bmap
#if defined(AFS_AIX31_VM)
#define VOPX_STRATEGY(VP,BP) efs_strategy (BP)
#else
#define VOPX_STRATEGY	efs_null
#endif
#define	VOPX_USTRATEGY	efs_panic
#define	VOPX_BREAD	efs_panic
#define	VOPX_BRELSE	efs_panic
#define	VOPX_LOCKCTL	efs_lockctl
#define	VOPX_FID	efs_fid
#define	VOPX_HOLD	efs_hold
#define	VOPX_RELE	efs_rele
#define	VOPX_GETACL 	efsx_getacl
#define	VOPX_SETACL	efsx_setacl
#define	VOPX_AFSFID	efs_afsfid
#define	VOPX_GETVOLUME	efs_getvolume
#define	VOPX_GETLENGTH	efs_getlength
#ifdef	AFS_AIX31_ENV
#define VOPX_MAP	efs_map
#define VOPX_UNMAP	efs_unmap
#endif
#ifdef	AFS_OSF_ENV
#define VOPX_RECLAIM	efs_reclaim
#endif
#ifdef	AFS_SUNOS5_ENV
#define VOPX_READ	efs_vmread
#define VOPX_WRITE	efs_vmwrite
#define VOPX_REALVP	efs_realvp
#define VOPX_RWLOCK	efs_rwlock
#define VOPX_RWUNLOCK	efs_rwunlock
#define VOPX_SEEK	efs_seek
#define VOPX_SPACE	efs_space
#define VOPX_GETPAGE	efs_getpage
#define VOPX_PUTPAGE	efs_putpage
#define VOPX_ADDMAP	efs_addmap
#define VOPX_DELMAP	efs_delmap
#define VOPX_PAGEIO	efs_pageio
#define VOPX_FRLOCK	efs_frlock
#define VOPX_SETFL	fs_setfl
#define VOPX_DISPOSE	fs_dispose
#define VOPX_SETSECATTR	fs_nosys
#define VOPX_GETSECATTR fs_fab_acl
#endif
#ifdef	AFS_HPUX_ENV
#define VOPX_PAGEIN	efs_pagein
#define VOPX_PAGEOUT	vfs_pageout
#endif

#endif /* EFS_SINGLE */

/*
 * Flags argument for VOPX_SETATTR
 */

#define XVN_EXTENDED		1	/* extended attribute structure */
#define XVN_SET_TIMES_NOW	2	/* System V style utime */
#define XVN_VOL_SETATTR         4	/* VOL_SETATTR path */

extern void xvfs_NullTxvattr(struct Txvattr *);

extern int xvfs_IsAdminGroup(osi_cred_t *);
extern void xvfs_SetAdminGroupID(long);

extern struct osi_vnodeops xglue_ops;
extern struct xvfs_xops xufs_ops;

/*
 * The extended VFS op vector
 */
struct xvfs_vfsops {
    struct vfsops xvfsops;
    struct vfsops vfsops;
    int (*vfsgetvolume)();
};

#ifdef SGIMIPS
extern struct xvfs_vfsops *GetNewVfsOpsFromOld(
    struct osi_vfsops *,
    int (*)());
extern int xvn_getacl(
    struct vnode *vp,
    struct dfs_acl *aclp,
    int w,
    osi_cred_t *credp);
extern int xvn_setacl(
    struct vnode *vp,
    struct dfs_acl *aclp,
    struct vnode *svp,
    long dw,
    long sw,
    osi_cred_t *credp);
#endif /* SGIMIPS */

/*
 * Macros for extended VFS ops
 */
#ifndef EFS_SINGLE
#ifdef	AFS_AIX31_ENV
#define VFSX_UNMOUNT(VFSP, F) \
    (*((struct xvfs_vfsops *)(VFSP)->osi_vfs_op)->vfsops.vfs_unmount) (VFSP, F)
#else
#ifdef AFS_SUNOS5_ENV
#define VFSX_UNMOUNT(VFSP,CR) \
    (*((struct xvfs_vfsops *)(VFSP)->osi_vfs_op)->vfsops.vfs_unmount) (VFSP,CR)
#else /* not SUNOS5 */
#ifdef SGIMIPS
#define VFSX_UNMOUNT(VFSP,FLAG,CR) \
    (*((struct xvfs_vfsops *)(VFSP)->osi_vfs_op)->vfsops.vfs_unmount)    \
		(VFSP->vfs_fbhv ,FLAG,CR)
#else
#define VFSX_UNMOUNT(VFSP) \
    (*((struct xvfs_vfsops *)(VFSP)->osi_vfs_op)->vfsops.vfs_unmount) (VFSP)
#endif /* SGIMIPS */
#endif  /* AFS_SUNOS5_ENV */
#endif	/* AFS_AIX31_ENV */

#ifdef	AFS_HPUX_ENV
#define VFSX_ROOT(VFSP,VPP,NM) \
    (*((struct xvfs_vfsops *)(VFSP)->osi_vfs_op)->vfsops.vfs_root) (VFSP,VPP,NM)
#else
#ifdef SGIMIPS
#define VFSX_ROOT(VFSP,VPP) \
    (*((struct xvfs_vfsops *)(VFSP)->osi_vfs_op)->vfsops.vfs_root) \
	(VFSP->vfs_fbhv,VPP)
#else
#define VFSX_ROOT(VFSP,VPP) \
    (*((struct xvfs_vfsops *)(VFSP)->osi_vfs_op)->vfsops.vfs_root) (VFSP,VPP)
#endif /* SGIMIPS */
#endif	/* AFS_HPUX_ENV */
#ifdef AFS_OSF_ENV
#define VFSX_FHTOVP(VFSP,FIDP,VPP) \
    (*((struct xvfs_vfsops *)(VFSP)->osi_vfs_op)->vfsops.vfs_fhtovp) (VFSP,FIDP,VPP)
#else
#ifdef SGIMIPS
#define VFSX_VGET(VFSP,VPP,FIDP) \
    (*((struct xvfs_vfsops *)(VFSP)->osi_vfs_op)->vfsops.vfs_vget)  \
	(VFSP->vfs_fbhv,VPP,FIDP)
#else
#define VFSX_VGET(VFSP,VPP,FIDP) \
    (*((struct xvfs_vfsops *)(VFSP)->osi_vfs_op)->vfsops.vfs_vget) (VFSP,VPP,FIDP)
#endif  /* SGIMIPS */
#endif
#define VFSX_VFSGETVOLUME(VFSP,VOLPP) \
    (*((struct xvfs_vfsops *)(VFSP)->osi_vfs_op)->vfsgetvolume) (VFSP, VOLPP)
#else	/* EFS_SINGLE */
#define VFSX_UNMOUNT	efs_unmount
#define VFSX_ROOT	efs_root
#define VFSX_VGET	efs_vget
#define VFSX_VFSGETVOLUME efs_vfsgetvolume
#endif	/* EFS_SINGLE */

#ifdef SGIMIPS
extern int xvfs_GetVolume(struct vnode *, struct volume **);
extern int xvfs_VfsGetVolume(struct osi_vfs *, struct volume **);
#else
extern long xvfs_GetVolume(struct vnode *, struct volume **);
extern long xvfs_VfsGetVolume(struct osi_vfs *, struct volume **);
#endif
extern int xvfs_NeedTokens(struct volume *);

/*
 * glue functions backing VFS macros
 */
#ifdef	AFS_AIX31_ENV
extern int xglue_unmount(struct osi_vfs *, int);
#else
#ifdef AFS_SUNOS5_ENV
extern int xglue_unmount(struct osi_vfs *, osi_cred_t *);
#else /* not SUNOS5 */
#ifdef SGIMIPS
extern int xglue_unmount(bhv_desc_t *,int, osi_cred_t *);
#else
extern int xglue_unmount(struct osi_vfs *);
#endif  /* SGIMIPS */
#endif  /* AFS_SUNOS5_ENV */
#endif	/* AFS_AIX31_ENV */
#ifdef SGIMIPS
extern int xglue_root(bhv_desc_t *, struct vnode **);
extern int xglue_vget(bhv_desc_t *, struct vnode **, struct fid *);
#else
extern int xglue_root(struct osi_vfs *, struct vnode **);
extern int xglue_vget(struct osi_vfs *, struct vnode **, struct fid *);
#endif
#ifdef AFS_OSF_ENV
int xglue_fhtovp(struct mount *, struct fid *, struct vnode **);
#endif /* AFS_OSF_ENV */

/*
 * glue function backing extended VFS function
 */
int xufs_vfsgetvolume(struct osi_vfs *, struct volume **);

/*
 * The next section requires a bit of explanation.  The glue layer holds vnode
 * reference counts high when tokens are granted as a caching mechanism.
 * However, each individual filesystem may also have processes which depend
 * on recycling.  For example, in the Episode code base we maintain our own
 * vnode pool.  This pool MUST be larger than the number of vnodes cached by
 * the tkc.  Otherwise, we will exhaust the Episode vnode pool before we
 * begin to release vnodes in the tkc.
 *
 * In reality, these numbers must be determined by the overall size of
 * the vnode pool and by the number of vnodes managed by each vfs.  The tkc
 * cannot start recycling after any vfs runs out of vnodes!
 * - wam 4/24/91
 */
#ifndef XVFS_MIN_VNODE_ALLOC_SIZE

/* this constant is the minimum # of vnodes created by any vfs */ 
#define XVFS_MIN_VNODE_ALLOC_SIZE 128

/* the following value MUST be at least 1 */
#define XVFS_DELTA_VNODE_RECYCLE_SIZE 32

/* this constant is the maximum # of vnodes the tkc, or similar processes,
 * should hold prior to recycling them.
 */
#define XVFS_MAX_VNODE_RECYCLE_SIZE \
	    (XVFS_MIN_VNODE_ALLOC_SIZE - XVFS_DELTA_VNODE_RECYCLE_SIZE)

#endif /* XVFS_MIN_VNODE_ALLOC_SIZE */

#ifndef RMTUSER_REQ		/* XXX Temporarily XXX */
#define	RMTUSER_REQ	0xabc
#endif

/* Macros to classify vnode ops into noop, read-only and read-write */

#define VNOP_LOCK	VNOP_TYPE_READWRITE
#define VNOP_LINK	VNOP_TYPE_READWRITE
#define VNOP_UNLINK	VNOP_TYPE_READWRITE
#define VNOP_MKDIR	VNOP_TYPE_READWRITE
#define VNOP_RMDIR	VNOP_TYPE_READWRITE
#define VNOP_RENAME	VNOP_TYPE_READWRITE
#define VNOP_SYNCGP	VNOP_TYPE_READWRITE
#define VNOP_TRUNC	VNOP_TYPE_READWRITE
#define VNOP_GETVAL	VNOP_TYPE_READONLY
#define VNOP_RWGP	VNOP_TYPE_READWRITE
#define VNOP_STAT	VNOP_TYPE_READWRITE
#define VNOP_UPDATE	VNOP_TYPE_READWRITE
#define VNOP_OPEN	VNOP_TYPE_READWRITE
#define VNOP_CLOSE	VNOP_TYPE_READWRITE
#define VNOP_READLINK	VNOP_TYPE_READWRITE
#define VNOP_SYMLINK	VNOP_TYPE_READWRITE
#define VNOP_BMAP	VNOP_TYPE_READWRITE
#define VNOP_NAMEI	VNOP_TYPE_READWRITE
#define VNOP_MKNOD	VNOP_TYPE_READWRITE
#define VNOP_REMOVE	VNOP_TYPE_READWRITE
#define VNOP_LOOKUP	VNOP_TYPE_READONLY

/* 
 * Change MAP and UNMAP to NOOP type vnodeops instead of READWRITE type
 * to prevent MAP and UNMAP vnodeops from blocking on busy filesets
 * as the AIX kernel global shared memory lock is held across these
 * calls. If these vnode ops block, any process trying to start or exit
 * will deadlock on the held shared memory lock
 */
#define VNOP_MAP	VNOP_TYPE_NOOP
#define VNOP_UNMAP	VNOP_TYPE_NOOP

#define VNOP_ACCESS	VNOP_TYPE_READONLY
#define VNOP_GETATTR	VNOP_TYPE_READONLY
#define VNOP_SETATTR	VNOP_TYPE_READWRITE
#define VNOP_FSYNC	VNOP_TYPE_READWRITE
#define VNOP_FTRUNC	VNOP_TYPE_READWRITE
#define VNOP_RDWR	VNOP_TYPE_READWRITE
#define VNOP_LOCKCTL	VNOP_TYPE_READWRITE
#define VNOP_READDIR	VNOP_TYPE_READWRITE
#define VNOP_GETACL	VNOP_TYPE_READONLY
#define VNOP_SETACL	VNOP_TYPE_READWRITE
#define VNOP_PGRD	VNOP_TYPE_READWRITE
#define VNOP_PGWR	VNOP_TYPE_READWRITE
#define VNOP_CREATE	VNOP_TYPE_READWRITE

/* SunOS relies on VFS_ROOT never blocking. If VFS_ROOT blocks, it holds
 * the vnode mutex which prevents anybody trying to obtain a reference
 * to the vnode to block that leads to a deadlock. 
 * HP, AIX and Solaris hold a reference to a filesystems's root vnode always
 * and hence there should not be any problem if VFS_ROOT is unglued.
 */
#ifndef AFS_OSF_ENV
#define VFSOP_ROOT	VNOP_TYPE_NOOP
#else
#define VFSOP_ROOT	VNOP_TYPE_READWRITE
#endif

#define VFSOP_UNMOUNT	VNOP_TYPE_READWRITE
#define VFSOP_FHTOVP	VNOP_TYPE_READWRITE
#define VFSOP_VGET	VNOP_TYPE_READWRITE

#endif	/* TRANSARC_XVNODE_H */
