/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/* Copyright (C) 1996, 1994 Transarc Corporation - All rights reserved. */
/*
 * $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/cm/RCS/cm_vnodeops.h,v 65.7 1998/09/23 14:39:47 bdr Exp $
 */

/*
 * Declarations for CM vnode operations
 */

#ifndef _TRANSARC_CM_VNODEOPS_H
#define _TRANSARC_CM_VNODEOPS_H

#include <dcedfs/osi.h>
#include <dcedfs/osi_kvnode.h>
#include <dcedfs/osi_vmm.h>
#include <dcedfs/osi_cred.h>
#include <dcedfs/afs4int.h>
#include <dcedfs/volume.h>

/*
 * The basic functions
 */
#ifdef SGIMIPS
extern int cm_noop(void);
extern int cm_invalop();
extern void cm_void_invalop(void);
extern void cm_badop(void);
#else  /* SGIMIPS */
extern int cm_noop();
extern int cm_invalop();
extern void cm_void_invalop();
extern void cm_badop();
#endif /* SGIMIPS */
extern int cm_open(struct vnode **vpp, long flags, osi_cred_t *credp);
extern int cm_close(struct vnode *vp, long flags, osi_cred_t *credp);
#ifdef SGIMIPS
extern int cm_ioctl(struct vnode *vp, int cmd, void *arg);
#else  /* SGIMIPS */
extern int cm_ioctl(struct vnode *vp, int cmd, int arg);
#endif /* SGIMIPS */

extern int cm_rdwr(
  struct vnode *vp,
  struct uio *uiop,
  enum uio_rw arw,
  int ioflag,
  osi_cred_t *credp);

extern int cm_nlrw(
  struct vnode *vp,
  struct uio *uiop,
  enum uio_rw rwflag,
  int ioflag,
  osi_cred_t *credp);

extern int cm_getattr(
  struct vnode *vp,
  struct xvfs_attr *attrsp,
  int flag,
  osi_cred_t *credp);

extern int cm_setattr(
  struct vnode *vp,
  struct xvfs_attr *attrsp,
  int flag,
  osi_cred_t *credp);

extern int cm_access(struct vnode *vp, int mode, osi_cred_t *credp);

extern int cm_lookup(
  struct vnode *dvp,
  char *name,
  struct vnode **vpp,
  osi_cred_t *credp);

extern int cm_create(
  struct vnode *dvp,
  char *name,
  struct vattr *attrsp,
  int aexcl,
  int mode,
  struct vnode **vpp,
  osi_cred_t *credp);

extern int cm_remove(struct vnode *dvp, char *name, osi_cred_t *credp);

extern int cm_link(
  struct vnode *vp,
  struct vnode *dvp,
  char *name,
  osi_cred_t *credp);

extern int cm_rename(
  struct vnode *ovp,
  char *oname,
  struct vnode *nvp,
  char *nname,
  osi_cred_t *credp);

extern int cm_mkdir(
  struct vnode *dvp,
  char *name,
  struct vattr *attrsp,
  struct vnode **vpp,
  osi_cred_t *credp);

extern int cm_rmdir(
  struct vnode *dvp,
  char *name,
  struct vnode *cdp,
  osi_cred_t *credp);

extern int cm_readdir(
  struct vnode *vp,
  struct uio *uiop,
  osi_cred_t *credp,
  int *eofp,
  int isPX	/* XXX */ );

extern int cm_symlink(
  struct vnode *dvp,
  char *name,
  struct vattr *attrsp,
  char *tname,
  osi_cred_t *credp);

extern int cm_readlink(struct vnode *vp, struct uio *uiop, osi_cred_t *credp);
#ifdef SGIMIPS
extern int cm_fsync(struct vnode *vp, int flag, osi_cred_t *credp, off_t,off_t);
#else  /* SGIMIPS */
extern int cm_fsync(struct vnode *vp, osi_cred_t *credp);
#endif /* SGIMIPS */
extern int cm_inactive(struct vnode *vp, osi_cred_t *credp);
extern int cm_cmp(struct vnode *vp1, struct vnode *vp2);
extern int cm_lock(struct vnode *vp);
extern int cm_unlock(struct vnode *vp);
#ifdef SGIMIPS
extern int cm_getlength(struct vnode *vp, afs_hyper_t *lenp, osi_cred_t *credp);
#else  /* SGIMIPS */
extern int cm_getlength(struct vnode *vp, long *lenp, osi_cred_t *credp);
#endif /* SGIMIPS */
#if defined(AFS_SUNOS5_ENV) || defined(SGIMIPS)
#ifdef AFS_SUNOS54_ENV
extern int cm_fid(struct vnode *vp, struct fid *fidp);
#else
extern int cm_fid(struct vnode *vp, struct fid **fidpP);
#endif
#else
extern int cm_fid(struct vnode *vp, struct fid *fidp);
#endif

#ifdef AFS_SUNOS5_ENV
extern int cm_strategy(struct buf *bp, osi_cred_t *credp);
extern int cm_ustrategy(struct buf *bp, osi_cred_t *credp);
#else
extern int cm_strategy(struct buf *bufp);
extern int cm_ustrategy(struct buf *bp);
#endif

/*
 * bmap, bread, brelse -- ancient (pre-VM) VFS functions
 */
extern int cm_bmap(
  struct vnode *vp,
  long abn,
  struct vnode **vpp,
  long *anbnp);

extern int cm_bread(struct vnode *vp, daddr_t lbn, struct buf **bufpp);
extern int cm_brelse(struct vnode *vp, struct buf *bufp);

/*
 * File/record locking
 */
#ifdef AFS_SUNOS5_ENV
extern int cm_lockctl(
    struct vnode *vp,
    int acmd,
    struct flock *flockp,
    int flag,
    offset_t offset,
    osi_cred_t *credp);
#else
extern int cm_lockctl(
    struct vnode *vp,
    struct flock *flockp,
    int acmd,
    osi_cred_t *credp,
    int id,
    long offset);
#endif

/*
 * ACL's
 */
extern int cm_getacl(
  struct vnode *vp,
  struct dfs_acl *aclp,
  int aclType,
  osi_cred_t *credp);

extern int cm_setacl(
  struct vnode *vp,
  struct dfs_acl *aclp,
  struct vnode *cvp,
  int dw,
  int sw,
  osi_cred_t *credp);

/*
 * Functions that are X-ops, but couldn't be N-ops.
 */
extern int cm_getxattr();
extern int cm_setxattr();
extern int cm_afsfid(struct vnode *vp, afsFid *afsfidp, int flag);
extern int cm_getvolume(struct vnode *realvp, struct volume **volpp);

/*
 * AIX 3
 */
#ifdef AFS_AIX_ENV
extern int cm_hold(struct vnode *vp);
extern int cm_rele(struct vnode *vp);
#endif

/*
 * XXX Since struct xvfs_xops doesn't specify prototypes and since
 * cm_addmap and cm_map have parameters of type u_char, we must _not_
 * have a prototype in scope in order to get correct behavior.  We
 * should probably change the u_char's to u_int's (and add prototypes
 * to xvfs_xops).
 */
#ifdef AFS_SUNOS5_ENV
extern int cm_map(
  struct vnode *vp,
  offset_t off,
  struct as *as,
  caddr_t *addrp,
  u_int len,
  u_char prot,
  u_char maxprot,
  u_int flags,
  osi_cred_t *credp
);
#elif SGIMIPS
extern int cm_map(
  struct vnode *vp,
  u_int flags);
#else  /* SGIMIPS */
extern int cm_map(
  struct vnode *vp,
  caddr_t addr,
  u_int length,
  u_int offset,
  u_int flags);
#endif

extern int cm_unmap(struct vnode *vp, int flag);

/*
 * OSF/1
 */
#ifdef AFS_OSF_ENV
extern int cm_reclaim(struct vnode *vp);
#endif

/*
 * SunOS 5
 */
#ifdef AFS_SUNOS5_ENV
extern int cm_vmread(
    struct vnode *vp,
    struct uio *uiop,
    int ioflag,
    osi_cred_t *credp);
extern int cm_vmwrite(
    struct vnode *vp,
    struct uio *uiop,
    int ioflag,
    osi_cred_t *credp);

extern void cm_rwlock(struct vnode *vp, int write_lock);
extern void cm_rwunlock(struct vnode *vp, int write_lock);
extern int cm_realvp(struct vnode *vp, struct vnode **rvpp);
extern int cm_seek(struct vnode *vp, offset_t ooff, offset_t *noffp);

extern int cm_space(
    struct vnode *vp,
    int cmd,
    struct flock *flockp,
    int flag,
    offset_t offset,
    osi_cred_t *credp);

extern int cm_getpage(
    struct vnode *vp,
    offset_t off,
    u_int len,
    u_int *protp,
    page_t *pl[],
    u_int plsz,
    struct seg *seg,
    caddr_t addr,
    enum seg_rw rw,
    osi_cred_t *credp);

extern int cm_putpage(
    struct vnode *vp,
    offset_t off,
    u_int len,
    int flags,
    osi_cred_t *credp);

#ifdef SGIMIPS
extern int cm_addmap(
    vnode_t *vp,
    vaddmap_t map_type,
    struct __vhandl_s *vt,
    pgno_t *pgno_type,
    off_t offset,
    size_t len,
    mprot_t prot,
    struct cred *cred);

extern int cm_delmap(
    vnode_t *vp,
    off_t off,
    struct pregion *as,
    addr_t addr,
    size_t len,
    u_int prot,
    u_int maxprot,
    u_int flags,
    struct cred *cr);
#else  /* SGIMIPS */

extern int cm_addmap(
    struct vnode *vp,
    offset_t off,
    struct as *as,
    caddr_t addr,
    u_int len,
    u_char prot,
    u_char maxprot,
    u_int flags,
    osi_cred_t *credp);

extern int cm_delmap(
    struct vnode *vp,
    offset_t off,
    struct as *as,
    caddr_t addr,
    u_int len,
    u_int prot,
    u_int maxprot,
    u_int flags,
    osi_cred_t *credp);
#endif /* SGIMIPS */

extern int cm_pageio(
    struct vnode *vp,
    page_t *pp,
    u_int off,
    u_int len,
    int flags,
    osi_cred_t *credp);

extern int cm_setfl(
  struct vnode *vp,
  int oflags,
  int nflags,
  osi_cred_t *credp);

extern int cm_getsecattr(
  struct vnode *vp,
  vsecattr_t *vsecattr,
  int flag,
  osi_cred_t *credp);
#endif /* AFS_SUNOS5_ENV */

#ifdef SGIMIPS
extern void cm_rwlock _TAKES(( struct vnode *vp, vrwlock_t locktype ));

extern void cm_rwunlock _TAKES(( struct vnode *vp, vrwlock_t locktype) );
#endif /* SGIMIPS */

#endif /* !_TRANSARC_CM_VNODEOPS_H */
