/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1995 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ifndef _FS_HWGFS_HWGNODE_H
#define _FS_HWGFS_HWGNODE_H

#ident	"$Revision: 1.8 $"

#include <sys/graph.h>

#if _KERNEL
/*
** A hardware graph file system node.  This is a hwgfs-specific
** structure with one per active hwgfs file.  
**
** Nodes are linked on two independent doubly-linked lists (non-
** circular and without a dummy header).  The first list is for
** all hwgnodes hanging off a vertex.  There can be multiple hwgfs 
** nodes for multiple mounts of hwgfs, and also for implementation
** reasons (to simplify locking).  The second list is for all hwgnodes
** associated with a particular mount of hwgfs.  This is used when
** performing a "sync" operation in preparatino for unmounting hwgfs.
**
** The hwgnode also includes a pointer back to the vnode behavior
** descriptor associated with this node, a pointer to the hwgfs mount
** structure for the associated mounted instance of hwgfs, a copy
** of the vertex handle associated with this node, and a valid flag.
**
** hwgfs allows multiple mounts maintly so that special files can 
** easily be used in chroot'ed environments.  Not strictly necessary,
** but useful.
*/
typedef struct hwgnode {
	struct hwgnode	*hwg_next_on_vertex;	/* linked list of hwgnodes */
						/* hanging off a hwgraph vertex */
	struct hwgnode	*hwg_prev_on_vertex;	/* back ptr */



	struct hwgnode	*hwg_next_on_fs;	/* linked list of hwgnodes */
						/* associated with a file system */
	struct hwgnode	*hwg_prev_on_fs;	/* back ptr */

	bhv_desc_t	hwg_vnode_bhv;	/* vnode behavior descriptor */

	struct hwgmount	*hwg_mount;	/* which mount this node belongs to */

	vertex_hdl_t	hwg_vertex;	/* which vertex this node is for */

	int		hwg_valid;	/* For DEBUG */
} hwgnode_t;

/* Convert a vnode behavior descriptor to a hwgnode */
#define	VNODE_TO_HWG(bdp)	((struct hwgnode *)BHV_PDATA(bdp))

/* Convert a hwgnode to its vnode behavior descriptor */
#define	HWG_TO_VNODEBHV(hwgp)	(&((hwgp)->hwg_vnode_bhv))

/* Convert a hwgnode to a vnode */
#define	HWG_TO_VNODE(hwgp)	((struct vnode *)BHV_VOBJ(HWG_TO_VNODEBHV(hwgp)))


/*
** Structure for every mount of hwgfs.
*/
typedef struct hwgmount {
	int		hwgmnt_count;	/* Count of hwg nodes created by this file system */
	hwgnode_t	*hwgmnt_head;	/* list of hwgnodes owned by this file system */

	hwgnode_t	*hwgmnt_cursor;	/* ptr into per-fs list for hwgsync */

	bhv_desc_t	*hwgmnt_root;	/* root vnode bhv of file system */
	bhv_desc_t	hwgmnt_vfsbhv;	/* vfs behavior */
} hwgmount_t;

/* Convert a vfs behavior descriptor to a hwg mount structure */
#define	VFSBHV_TO_HWGMNT(bdp)	((hwgmount_t *)BHV_PDATA(bdp))

/* Convert a vfs behavior descriptor to a vfs structure */
#define	VFSBHV_TO_VFS(bdp)	((struct vfs *)BHV_VOBJ(bdp))

/* Given a hwgmount object, convert to the root vnode of the file system */
#define HWGMNT_TO_ROOTVP(hwgmount) ((struct vnode *)BHV_VOBJ(hwgmount->hwgmnt_root))


extern mrlock_t hwgfs_node_mrlock;
extern struct vfsops hwgvfsops;
extern struct vnodeops hwgvnodeops;
extern dev_t hwgdev;
extern int hwgfstype;

extern hwgnode_t * hwgfs_allocnode(vertex_hdl_t, vtype_t, hwgmount_t *); 

extern void hwgfs_freenode(bhv_desc_t *);

extern struct vnode * hwgfs_getnode(vertex_hdl_t, vtype_t, hwgmount_t *, struct cred *);

extern void hwgfs_get_permissions(vertex_hdl_t vhdl, enum vtype type, mode_t *modep, 
					uid_t *uidp, gid_t *gidp);

extern void hwgfs_set_permissions(vertex_hdl_t vhdl, enum vtype type, mode_t mode, 
					uid_t uid, gid_t gid);

#endif /* _KERNEL */
#endif /* _FS_HWGFS_HWGNODE_H */
