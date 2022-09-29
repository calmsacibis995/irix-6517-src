#ifndef _FS_XFS_ATTR_H
#define	_FS_XFS_ATTR_H

#ident	"$Revision: 1.11 $"

/*
 * xfs_attr.h
 *
 * Large attribute lists are structured around Btrees where all the data
 * elements are in the leaf nodes.  Attribute names are hashed into an int,
 * then that int is used as the index into the Btree.  Since the hashval
 * of an attribute name may not be unique, we may have duplicate keys.
 * The internal links in the Btree are logical block offsets into the file.
 *
 * Small attribute lists use a different format and are packed as tightly
 * as possible so as to fit into the literal area of the inode.
 */

#ifdef XFS_ALL_TRACE
#define	XFS_ATTR_TRACE
#endif

#if !defined(DEBUG) || defined(SIM)
#undef XFS_ATTR_TRACE
#endif

/*========================================================================
 * Function prototypes for the kernel.
 *========================================================================*/

struct cred;
struct vnode;
struct xfs_inode;
struct attrlist_cursor_kern;
struct xfs_ext_attr;
struct xfs_da_args;

/*
 * Overall external interface routines.
 */
int xfs_attr_get(bhv_desc_t *, char *, char *, int *, int, struct cred *);
int xfs_attr_set(bhv_desc_t *, char *, char *, int, int, struct cred *);
int xfs_attr_remove(bhv_desc_t *, char *, int, struct cred *);
int xfs_attr_list(bhv_desc_t *, char *, int, int,
			 struct attrlist_cursor_kern *, struct cred *);
int xfs_attr_inactive(struct xfs_inode *dp);

int xfs_attr_node_get(struct xfs_da_args *);
int xfs_attr_leaf_get(struct xfs_da_args *);
int xfs_attr_shortform_getvalue(struct xfs_da_args *);
int xfs_attr_fetch(struct xfs_inode *, char *, char *, int);

#endif	/* !_FS_XFS_ATTR_H */
