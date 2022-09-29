#ifndef _FS_XFS_IALLOC_H
#define	_FS_XFS_IALLOC_H

#ident	"$Revision: 1.32 $"

struct buf;
struct xfs_dinode;
struct xfs_mount;
struct xfs_trans;

/*
 * Allocation parameters for inode allocation.
 */
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_IALLOC_INODES)
int xfs_ialloc_inodes(struct xfs_mount *mp);
#define	XFS_IALLOC_INODES(mp)	xfs_ialloc_inodes(mp)
#else
#define	XFS_IALLOC_INODES(mp)	((mp)->m_ialloc_inos)
#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_IALLOC_BLOCKS)
xfs_extlen_t xfs_ialloc_blocks(struct xfs_mount *mp);
#define	XFS_IALLOC_BLOCKS(mp)	xfs_ialloc_blocks(mp)
#else
#define	XFS_IALLOC_BLOCKS(mp)	((mp)->m_ialloc_blks)
#endif

/*
 * For small block file systems, move inodes in clusters of this size.
 * When we don't have a lot of memory, however, we go a bit smaller
 * to reduce the number of AGI and ialloc btree blocks we need to keep
 * around for xfs_dilocate().  We choose which one to use in
 * xfs_mount_int().
 */
#define	XFS_INODE_BIG_CLUSTER_SIZE	8192
#define	XFS_INODE_SMALL_CLUSTER_SIZE	4096
#define	XFS_INODE_CLUSTER_SIZE(mp)	(mp)->m_inode_cluster_size

/*
 * Make an inode pointer out of the buffer/offset.
 */
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_MAKE_IPTR)
struct xfs_dinode *xfs_make_iptr(struct xfs_mount *mp, struct buf *b, int o);
#define	XFS_MAKE_IPTR(mp,b,o) 		xfs_make_iptr(mp,b,o)
#else
#define	XFS_MAKE_IPTR(mp,b,o) \
	((xfs_dinode_t *)((b)->b_un.b_addr + ((o) << (mp)->m_sb.sb_inodelog)))
#endif

/*
 * Find a free (set) bit in the inode bitmask.
 */
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_IALLOC_FIND_FREE)
int xfs_ialloc_find_free(xfs_inofree_t *fp);
#define	XFS_IALLOC_FIND_FREE(fp)	xfs_ialloc_find_free(fp)
#else
#define	XFS_IALLOC_FIND_FREE(fp)	xfs_lowbit64(*(fp))
#endif

/*
 * Prototypes for visible xfs_ialloc.c routines.
 */

/*
 * Allocate an inode on disk.
 * Mode is used to tell whether the new inode will need space, and whether
 * it is a directory.
 *
 * To work within the constraint of one allocation per transaction,
 * xfs_dialloc() is designed to be called twice if it has to do an
 * allocation to make more free inodes.  If an inode is 
 * available without an allocation, agbp would be set to the current
 * agbp and alloc_done set to false.
 * If an allocation needed to be done, agbp would be set to the
 * inode header of the allocation group and alloc_done set to true.
 * The caller should then commit the current transaction and allocate a new
 * transaction.  xfs_dialloc() should then be called again with
 * the agbp value returned from the previous call.
 *
 * Once we successfully pick an inode its number is returned and the
 * on-disk data structures are updated.  The inode itself is not read
 * in, since doing so would break ordering constraints with xfs_reclaim.
 *
 * *agbp should be set to NULL on the first call, *alloc_done set to FALSE.
 */
int					/* error */
xfs_dialloc(
	struct xfs_trans *tp,		/* transaction pointer */
	xfs_ino_t	parent,		/* parent inode (directory) */
	mode_t		mode,		/* mode bits for new inode */
	int		okalloc,	/* ok to allocate more space */
	struct buf	**agbp,		/* buf for a.g. inode header */
	boolean_t	*alloc_done,	/* an allocation was done to replenish
					   the free inodes */
	xfs_ino_t	*inop);		/* inode number allocated */
#ifndef SIM
/*
 * Free disk inode.  Carefully avoids touching the incore inode, all
 * manipulations incore are the caller's responsibility.
 * The on-disk inode is not changed by this operation, only the
 * btree (free inode mask) is changed.
 */
int					/* error */
xfs_difree(
	struct xfs_trans *tp,		/* transaction pointer */
	xfs_ino_t	inode);		/* inode to be freed */
#endif	/* !SIM */

/*
 * Return the location of the inode in bno/len/off,
 * for mapping it into a buffer.
 */
int
xfs_dilocate(
	struct xfs_mount *mp,		/* file system mount structure */
	struct xfs_trans *tp,		/* transaction pointer */
	xfs_ino_t	ino,		/* inode to locate */
	xfs_fsblock_t	*bno,		/* output: block containing inode */
	int		*len,		/* output: num blocks in cluster*/
	int		*off,		/* output: index in block of inode */
	uint		flags);		/* flags for inode btree lookup */

/*
 * Compute and fill in value of m_in_maxlevels.
 */
void
xfs_ialloc_compute_maxlevels(
	struct xfs_mount *mp);		/* file system mount structure */

/*
 * Log specified fields for the ag hdr (inode section)
 */
void
xfs_ialloc_log_agi(
	struct xfs_trans *tp,		/* transaction pointer */
	struct buf	*bp,		/* allocation group header buffer */
	int		fields);	/* bitmask of fields to log */

/*
 * Read in the allocation group header (inode allocation section)
 */
int					/* error */
xfs_ialloc_read_agi(
	struct xfs_mount *mp,		/* file system mount structure */
	struct xfs_trans *tp,		/* transaction pointer */
	xfs_agnumber_t	agno,		/* allocation group number */
	struct buf	**bpp);		/* allocation group hdr buf */

#endif	/* !_FS_XFS_IALLOC_H */
