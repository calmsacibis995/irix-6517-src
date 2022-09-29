#ifndef _FS_XFS_DIR2_H
#define	_FS_XFS_DIR2_H

#ident 	"$Revision: 1.1 $"

/*
 * xfs_dir2.h
 */

struct dirent;
struct uio;
struct xfs_dabuf;
struct xfs_da_args;
struct xfs_dir2_put_args;
struct xfs_inode;
struct xfs_trans;

/*
 * Directory version 2.
 * There are 4 possible formats:
 *	shortform
 *	single block - data with embedded leaf at the end
 *	multiple data blocks, single leaf+freeindex block
 *	data blocks, node&leaf blocks (btree), freeindex blocks
 *
 *	The shortform format is in xfs_dir2_sf.h.
 *	The single block format is in xfs_dir2_block.h.
 *	The data block format is in xfs_dir2_data.h.
 *	The leaf and freeindex block formats are in xfs_dir2_leaf.h.
 *	Node blocks are the same as the other version, in xfs_da_btree.h.
 */

/*
 * Byte offset in data block and shortform entry.
 */
typedef	__uint16_t	xfs_dir2_data_off_t;
#define	NULLDATAOFF	0xffffU
typedef uint		xfs_dir2_data_aoff_t;	/* argument form */

/*
 * Directory block number (logical dirblk in file)
 */
typedef	__uint32_t	xfs_dir2_db_t;

/*
 * Byte offset in a directory.
 */
typedef	off64_t		xfs_dir2_off_t;

/*
 * For getdents, argument struct for put routines.
 */
typedef int (*xfs_dir2_put_t)(struct xfs_dir2_put_args *pa);
typedef struct xfs_dir2_put_args {
	off_t		cook;		/* cookie of (next) entry */
	xfs_intino_t	ino;		/* inode number */
	struct dirent	*dbp;		/* buffer pointer */
	char		*name;		/* directory entry name */
	int		namelen;	/* length of name */
	int		done;		/* output: set if value was stored */
	xfs_dir2_put_t	put;		/* put function ptr (i/o) */
	struct uio	*uio;		/* uio control structure */
} xfs_dir2_put_args_t;

#define	XFS_DIR_IS_V2(mp)	((mp)->m_dirversion == 2)
extern xfs_dirops_t	xfsv2_dirops;

/*
 * Other interfaces used by the rest of the dir v2 code.
 */
extern int
	xfs_dir2_grow_inode(struct xfs_da_args *args, int space,
			    xfs_dir2_db_t *dbp);

extern int
	xfs_dir2_isblock(struct xfs_trans *tp, struct xfs_inode *dp, int *vp);

extern int
	xfs_dir2_isleaf(struct xfs_trans *tp, struct xfs_inode *dp, int *vp);

#if defined(XFS_REPAIR_SIM) || !defined(SIM)
extern int
	xfs_dir2_shrink_inode(struct xfs_da_args *args, xfs_dir2_db_t db,
			      struct xfs_dabuf *bp);
#endif /* XFS_REPAIR_SIM || !SIM */

#endif	/* !_FS_XFS_DIR2_H */
