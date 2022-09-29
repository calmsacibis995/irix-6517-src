/*
 * Copyright (c) 1989, 1990, 1991 by Sun Microsystems, Inc.
 */

/*
 * Loopback mount info - one per mount
 */

#ifndef _SYS_FS_LOFS_INFO_H
#define	_SYS_FS_LOFS_INFO_H

#ifdef	__cplusplus
extern "C" {
#endif

struct loinfo {
	bhv_desc_t	li_bhv;		/* vfs lofs behavior */
	struct vfs	*li_realvfs;	/* real vfs of mount */
	struct vfs	*li_mountvfs;	/* loopback vfs */
	struct vnode	*li_rootvp;	/* root vnode of this vfs */
	int		 li_mflag;	/* mount flags to inherit */
	int		 li_refct;	/* # outstanding vnodes */
	dev_t		li_rdev;	/* distinguish lofs mounts */
	struct lfsnode	*li_lfs;	/* list of other vfss */
};

/* inheritable mount flags - propagated from real vfs to loopback */
#define	INHERIT_VFS_FLAG	(VFS_RDONLY|VFS_NOSUID)

/*
 * lfsnodes are allocated as new real vfs's are encountered
 * when looking up things in a loopback name space
 * It contains a new vfs which is paired with the real vfs
 * so that vfs ops (fsstat) can get to the correct real vfs
 * given just a loopback vfs
 */
struct lfsnode {
	struct lfsnode *lfs_next;	/* next in loinfo list */
	int		lfs_refct;	/* # outstanding vnodes */
	struct vfs	*lfs_realvfs;	/* real vfs */
	bhv_desc_t	lfs_bhv;	/* vfs lofs behavior */
	struct vfs	lfs_vfs;	/* new loopback vfs */
};

#define vfs_bhvtoli(bdp)	((struct loinfo *)BHV_PDATA(bdp))

extern struct vfs *lofs_realvfs(struct vfs *, struct loinfo *);

extern struct vfsops lofs_vfsops;

extern int lofsfstype;

extern void lofs_dprint(int var, int level, char *str, ...);


#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FS_LOFS_INFO_H */
