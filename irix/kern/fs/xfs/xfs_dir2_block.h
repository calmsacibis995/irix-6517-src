#ifndef _FS_XFS_DIR2_BLOCK_H
#define	_FS_XFS_DIR2_BLOCK_H

#ident "$Revision: 1.1 $"

/*
 * xfs_dir2_block.h
 * Directory version 2, single block format structures
 */

struct dirent;
struct uio;
struct xfs_dabuf;
struct xfs_da_args;
struct xfs_dir2_data_hdr;
struct xfs_dir2_leaf_entry;
struct xfs_inode;
struct xfs_mount;
struct xfs_trans;

/*
 * The single block format is as follows:
 * xfs_dir2_data_hdr_t structure
 * xfs_dir2_data_entry_t and xfs_dir2_data_unused_t structures
 * xfs_dir2_leaf_entry_t structures
 * xfs_dir2_block_tail_t structure
 */

#define	XFS_DIR2_BLOCK_MAGIC	0x58443242	/* XD2B: for one block dirs */

typedef struct xfs_dir2_block_tail {
	__uint32_t	count;			/* count of leaf entries */
	__uint32_t	stale;			/* count of stale lf entries */
} xfs_dir2_block_tail_t;

/*
 * Generic single-block structure, for xfs_db.
 */
typedef struct xfs_dir2_block {
	xfs_dir2_data_hdr_t	hdr;		/* magic XFS_DIR2_BLOCK_MAGIC */
	xfs_dir2_data_union_t	u[1];
	xfs_dir2_leaf_entry_t	leaf[1];
	xfs_dir2_block_tail_t	tail;
} xfs_dir2_block_t;

/*
 * Pointer to the leaf header embedded in a data block (1-block format)
 */
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_DIR2_BLOCK_TAIL_P)
xfs_dir2_block_tail_t *
xfs_dir2_block_tail_p(struct xfs_mount *mp, xfs_dir2_block_t *block);
#define	XFS_DIR2_BLOCK_TAIL_P(mp,block)	xfs_dir2_block_tail_p(mp,block) 
#else
#define	XFS_DIR2_BLOCK_TAIL_P(mp,block)	\
	(((xfs_dir2_block_tail_t *)((char *)(block) + (mp)->m_dirblksize)) - 1)
#endif

/*
 * Pointer to the leaf entries embedded in a data block (1-block format)
 */
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_DIR2_BLOCK_LEAF_P)
struct xfs_dir2_leaf_entry *xfs_dir2_block_leaf_p(xfs_dir2_block_tail_t *btp);
#define	XFS_DIR2_BLOCK_LEAF_P(btp)	xfs_dir2_block_leaf_p(btp)
#else
#define	XFS_DIR2_BLOCK_LEAF_P(btp)	\
	(((struct xfs_dir2_leaf_entry *)(btp)) - (btp)->count)
#endif

/*
 * Function declarations.
 */

extern int
	xfs_dir2_block_addname(struct xfs_da_args *args);

#ifndef SIM
extern int
	xfs_dir2_block_getdents(struct xfs_trans *tp, struct xfs_inode *dp,
				struct uio *uio, int *eofp, struct dirent *dbp,
				xfs_dir2_put_t put);
#endif

extern int
	xfs_dir2_block_lookup(struct xfs_da_args *args);

#if defined(XFS_REPAIR_SIM) || !defined(SIM)
extern int
	xfs_dir2_block_removename(struct xfs_da_args *args);

extern int
	xfs_dir2_block_replace(struct xfs_da_args *args);
#endif /* XFS_REPAIR_SIM || !SIM */

#if defined(XFS_REPAIR_SIM) || !defined(SIM)
extern int
	xfs_dir2_leaf_to_block(struct xfs_da_args *args, struct xfs_dabuf *lbp,
			       struct xfs_dabuf *dbp);
#endif /* XFS_REPAIR_SIM || !SIM */

extern int
	xfs_dir2_sf_to_block(struct xfs_da_args *args);

#endif	/* !_FS_XFS_DIR2_BLOCK_H */
