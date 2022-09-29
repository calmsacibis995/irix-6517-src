/*
 * Copyright (c) 1989, 1990, 1991 by Sun Microsystems, Inc.
 */

/*
 * Loop-back file information structure.
 */

#ifndef _SYS_FS_LOFS_NODE_H
#define	_SYS_FS_LOFS_NODE_H

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * The lnode is the "inode" for loop-back files.  It contains
 * all the information necessary to handle loop-back file on the
 * client side.
 */
typedef struct lnode {
	struct lnode	*lo_next;	/* link for hash chain */
	struct vnode	*lo_vp;		/* pointer to real vnode */
	uint_t          lo_looping;     /* detect looping */
	struct bhv_desc	lo_bhv_desc;	/* loopback behavior descriptor */
	struct vnode    *lo_crossedvp;  /* ptr to rootvp of crossed lofs */
	struct vnode	*lo_vnode;	/* place holder vnode for file */
} lnode_t;

/*
 * Convert between vnode and lnode
 */
#define	ltov(lp)	((lp)->lo_vnode)
#define	ltobhv(lp)	(&((lp)->lo_bhv_desc))
#define	bhvtol(bdp)	((struct lnode *)(BHV_PDATA((bdp))))
#define	realvp(bdp)	(bhvtol(bdp)->lo_vp)

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FS_LOFS_NODE_H */
