#ifndef _FS_XFS_DIR2_NODE_H
#define	_FS_XFS_DIR2_NODE_H

#ident "$Revision: 1.1 $"

/*
 * xfs_dir2_node.h
 * Directory version 2, btree node format structures
 */

struct dirent;
struct uio;
struct xfs_dabuf;
struct xfs_da_args;
struct xfs_da_state;
struct xfs_da_state_blk;
struct xfs_inode;
struct xfs_trans;

/*
 * Constants.
 */

/*
 * Offset of the freespace index.
 */
#define	XFS_DIR2_FREE_SPACE	2
#define	XFS_DIR2_FREE_OFFSET	(XFS_DIR2_FREE_SPACE * XFS_DIR2_SPACE_SIZE)
#define	XFS_DIR2_FREE_FIRSTDB(mp)	\
	XFS_DIR2_BYTE_TO_DB(mp, XFS_DIR2_FREE_OFFSET)

#define	XFS_DIR2_FREE_MAGIC	0x58443246	/* XD2F */

/*
 * Structures.
 */
typedef	struct xfs_dir2_free_hdr {
	__uint32_t		magic;		/* XFS_DIR2_FREE_MAGIC */
	__int32_t		firstdb;	/* db of first entry */
	__int32_t		nvalid;		/* count of valid entries */
	__int32_t		nused;		/* count of used entries */
} xfs_dir2_free_hdr_t;

typedef struct xfs_dir2_free {
	xfs_dir2_free_hdr_t	hdr;		/* block header */
	xfs_dir2_data_off_t	bests[1];	/* best free counts */
						/* unused entries are -1 */
} xfs_dir2_free_t;
#define	XFS_DIR2_MAX_FREE_BESTS(mp)	\
	(((mp)->m_dirblksize - (uint)sizeof(xfs_dir2_free_hdr_t)) / \
	 (uint)sizeof(xfs_dir2_data_off_t))

/*
 * Macros.
 */

/*
 * Convert data space db to the corresponding free db.
 */
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_DIR2_DB_TO_FDB)
xfs_dir2_db_t
xfs_dir2_db_to_fdb(struct xfs_mount *mp, xfs_dir2_db_t db);
#define	XFS_DIR2_DB_TO_FDB(mp,db)	xfs_dir2_db_to_fdb(mp, db)
#else
#define	XFS_DIR2_DB_TO_FDB(mp,db)	\
	(XFS_DIR2_FREE_FIRSTDB(mp) + (db) / XFS_DIR2_MAX_FREE_BESTS(mp))
#endif

/*
 * Convert data space db to the corresponding index in a free db.
 */
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_DIR2_DB_TO_FDINDEX)
int
xfs_dir2_db_to_fdindex(struct xfs_mount *mp, xfs_dir2_db_t db);
#define	XFS_DIR2_DB_TO_FDINDEX(mp,db)	xfs_dir2_db_to_fdindex(mp, db)
#else
#define	XFS_DIR2_DB_TO_FDINDEX(mp,db)	((db) % XFS_DIR2_MAX_FREE_BESTS(mp))
#endif

/* 
 * Functions.
 */

extern void
	xfs_dir2_free_log_bests(struct xfs_trans *tp, struct xfs_dabuf *bp,
				int first, int last);

extern int
	xfs_dir2_leaf_to_node(struct xfs_da_args *args, struct xfs_dabuf *lbp);

extern xfs_dahash_t
	xfs_dir2_leafn_lasthash(struct xfs_dabuf *bp, int *count);

extern int
	xfs_dir2_leafn_lookup_int(struct xfs_dabuf *bp,
				  struct xfs_da_args *args, int *indexp,
				  struct xfs_da_state *state);

extern int
	xfs_dir2_leafn_order(struct xfs_dabuf *leaf1_bp,
			     struct xfs_dabuf *leaf2_bp);

extern int
	xfs_dir2_leafn_split(struct xfs_da_state *state,
			     struct xfs_da_state_blk *oldblk,
			     struct xfs_da_state_blk *newblk);

#if defined(XFS_REPAIR_SIM) || !defined(SIM)
extern int
	xfs_dir2_leafn_toosmall(struct xfs_da_state *state, int *action);

extern void
	xfs_dir2_leafn_unbalance(struct xfs_da_state *state,
				 struct xfs_da_state_blk *drop_blk,
				 struct xfs_da_state_blk *save_blk);
#endif /* XFS_REPAIR_SIM || !SIM */

extern int
	xfs_dir2_node_addname(struct xfs_da_args *args);

extern int
	xfs_dir2_node_lookup(struct xfs_da_args *args);

#if defined(XFS_REPAIR_SIM) || !defined(SIM)
extern int
	xfs_dir2_node_removename(struct xfs_da_args *args);

extern int
	xfs_dir2_node_replace(struct xfs_da_args *args);

extern int
	xfs_dir2_node_trim_free(struct xfs_da_args *args, xfs_fileoff_t fo,
				int *rvalp);
#endif /* XFS_REPAIR_SIM || !SIM */

#endif	/* !_FS_XFS_DIR2_NODE_H */
