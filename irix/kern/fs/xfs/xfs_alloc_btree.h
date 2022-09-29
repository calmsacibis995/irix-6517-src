#ifndef _FS_XFS_ALLOC_BTREE_H
#define	_FS_XFS_ALLOC_BTREE_H

#ident	"$Revision: 1.15 $"

/*
 * Freespace on-disk structures
 */

struct buf;
struct xfs_btree_cur;
struct xfs_btree_sblock;
struct xfs_mount;

/*
 * There are two on-disk btrees, one sorted by blockno and one sorted
 * by blockcount and blockno.  All blocks look the same to make the code
 * simpler; if we have time later, we'll make the optimizations.
 */
#define	XFS_ABTB_MAGIC	0x41425442	/* 'ABTB' for bno tree */
#define	XFS_ABTC_MAGIC	0x41425443	/* 'ABTC' for cnt tree */

/*
 * Data record/key structure
 */
typedef struct xfs_alloc_rec
{
	xfs_agblock_t	ar_startblock;	/* starting block number */
	xfs_extlen_t	ar_blockcount;	/* count of free blocks */
} xfs_alloc_rec_t, xfs_alloc_key_t;

typedef xfs_agblock_t xfs_alloc_ptr_t;	/* btree pointer type */
					/* btree block header type */
typedef	struct xfs_btree_sblock xfs_alloc_block_t;

#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_BUF_TO_ALLOC_BLOCK)
xfs_alloc_block_t *xfs_buf_to_alloc_block(struct buf *bp);
#define	XFS_BUF_TO_ALLOC_BLOCK(bp)	xfs_buf_to_alloc_block(bp)
#else
#define	XFS_BUF_TO_ALLOC_BLOCK(bp) ((xfs_alloc_block_t *)((bp)->b_un.b_addr))
#endif

/*
 * Real block structures have a size equal to the disk block size.
 */

#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_ALLOC_BLOCK_SIZE)
int xfs_alloc_block_size(int lev, struct xfs_btree_cur *cur);
#define	XFS_ALLOC_BLOCK_SIZE(lev,cur)	xfs_alloc_block_size(lev,cur)
#else
#define	XFS_ALLOC_BLOCK_SIZE(lev,cur)	(1 << (cur)->bc_blocklog)
#endif

#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_ALLOC_BLOCK_MAXRECS)
int xfs_alloc_block_maxrecs(int lev, struct xfs_btree_cur *cur);
#define	XFS_ALLOC_BLOCK_MAXRECS(lev,cur)	xfs_alloc_block_maxrecs(lev,cur)
#else
#define	XFS_ALLOC_BLOCK_MAXRECS(lev,cur)	\
	((cur)->bc_mp->m_alloc_mxr[lev != 0])
#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_ALLOC_BLOCK_MINRECS)
int xfs_alloc_block_minrecs(int lev, struct xfs_btree_cur *cur);
#define	XFS_ALLOC_BLOCK_MINRECS(lev,cur)	xfs_alloc_block_minrecs(lev,cur)
#else
#define	XFS_ALLOC_BLOCK_MINRECS(lev,cur)	\
	((cur)->bc_mp->m_alloc_mnr[lev != 0])
#endif

/*
 * Minimum and maximum blocksize.
 * The blocksize upper limit is pretty much arbitrary.
 */
#define	XFS_MIN_BLOCKSIZE_LOG	9	/* i.e. 512 bytes */
#define	XFS_MAX_BLOCKSIZE_LOG	16	/* i.e. 65536 bytes */
#define	XFS_MIN_BLOCKSIZE	(1 << XFS_MIN_BLOCKSIZE_LOG)
#define	XFS_MAX_BLOCKSIZE	(1 << XFS_MAX_BLOCKSIZE_LOG)

/*
 * block numbers in the AG; SB is BB 0, AGF is BB 1, AGI is BB 2, AGFL is BB 3
 */
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_BNO_BLOCK)
xfs_agblock_t xfs_bno_block(struct xfs_mount *mp);
#define	XFS_BNO_BLOCK(mp)	xfs_bno_block(mp)
#else
#define	XFS_BNO_BLOCK(mp)	((xfs_agblock_t)(XFS_AGFL_BLOCK(mp) + 1))
#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_CNT_BLOCK)
xfs_agblock_t xfs_cnt_block(struct xfs_mount *mp);
#define	XFS_CNT_BLOCK(mp)	xfs_cnt_block(mp)
#else
#define	XFS_CNT_BLOCK(mp)	((xfs_agblock_t)(XFS_BNO_BLOCK(mp) + 1))
#endif

/*
 * Record, key, and pointer address macros for btree blocks.
 */
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_ALLOC_REC_ADDR)
xfs_alloc_rec_t *xfs_alloc_rec_addr(xfs_alloc_block_t *bb, int i,
				    struct xfs_btree_cur *cur);
#define	XFS_ALLOC_REC_ADDR(bb,i,cur)	xfs_alloc_rec_addr(bb,i,cur)
#else
#define	XFS_ALLOC_REC_ADDR(bb,i,cur)	\
	XFS_BTREE_REC_ADDR(XFS_ALLOC_BLOCK_SIZE(0,cur), xfs_alloc, bb, i, \
		XFS_ALLOC_BLOCK_MAXRECS(0, cur))
#endif

#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_ALLOC_KEY_ADDR)
xfs_alloc_key_t *xfs_alloc_key_addr(xfs_alloc_block_t *bb, int i,
				    struct xfs_btree_cur *cur);
#define	XFS_ALLOC_KEY_ADDR(bb,i,cur)	xfs_alloc_key_addr(bb,i,cur)
#else
#define	XFS_ALLOC_KEY_ADDR(bb,i,cur)	\
	XFS_BTREE_KEY_ADDR(XFS_ALLOC_BLOCK_SIZE(1,cur), xfs_alloc, bb, i, \
		XFS_ALLOC_BLOCK_MAXRECS(1, cur))
#endif

#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_ALLOC_PTR_ADDR)
xfs_alloc_ptr_t *xfs_alloc_ptr_addr(xfs_alloc_block_t *bb, int i,
				    struct xfs_btree_cur *cur);
#define	XFS_ALLOC_PTR_ADDR(bb,i,cur)	xfs_alloc_ptr_addr(bb,i,cur)
#else
#define	XFS_ALLOC_PTR_ADDR(bb,i,cur)	\
	XFS_BTREE_PTR_ADDR(XFS_ALLOC_BLOCK_SIZE(1,cur), xfs_alloc, bb, i, \
		XFS_ALLOC_BLOCK_MAXRECS(1, cur))
#endif

/*
 * Prototypes for externally visible routines.
 */

/*
 * Decrement cursor by one record at the level.
 * For nonzero levels the leaf-ward information is untouched.
 */
int					/* error */
xfs_alloc_decrement(
	struct xfs_btree_cur	*cur,	/* btree cursor */
	int			level,	/* level in btree, 0 is leaf */
	int			*stat);	/* success/failure */

/*
 * Delete the record pointed to by cur.
 * The cursor refers to the place where the record was (could be inserted)
 * when the operation returns.
 */
int					/* error */
xfs_alloc_delete(
	struct xfs_btree_cur	*cur,	/* btree cursor */
	int			*stat);	/* success/failure */

/*
 * Get the data from the pointed-to record.
 */
int					/* error */
xfs_alloc_get_rec(
	struct xfs_btree_cur	*cur,	/* btree cursor */
	xfs_agblock_t		*bno,	/* output: starting block of extent */
	xfs_extlen_t		*len,	/* output: length of extent */
	int			*stat);	/* output: success/failure */

/*
 * Increment cursor by one record at the level.
 * For nonzero levels the leaf-ward information is untouched.
 */
int					/* error */
xfs_alloc_increment(
	struct xfs_btree_cur	*cur,	/* btree cursor */
	int			level,	/* level in btree, 0 is leaf */
	int			*stat);	/* success/failure */

/*
 * Insert the current record at the point referenced by cur.
 * The cursor may be inconsistent on return if splits have been done.
 */
int					/* error */
xfs_alloc_insert(
	struct xfs_btree_cur	*cur,	/* btree cursor */
	int			*stat);	/* success/failure */

/*
 * Lookup the record equal to [bno, len] in the btree given by cur.
 */
int					/* error */
xfs_alloc_lookup_eq(
	struct xfs_btree_cur	*cur,	/* btree cursor */
	xfs_agblock_t		bno,	/* starting block of extent */
	xfs_extlen_t		len,	/* length of extent */
	int			*stat);	/* success/failure */

/*
 * Lookup the first record greater than or equal to [bno, len]
 * in the btree given by cur.
 */
int					/* error */
xfs_alloc_lookup_ge(
	struct xfs_btree_cur	*cur,	/* btree cursor */
	xfs_agblock_t		bno,	/* starting block of extent */
	xfs_extlen_t		len,	/* length of extent */
	int			*stat);	/* success/failure */

/*
 * Lookup the first record less than or equal to [bno, len]
 * in the btree given by cur.
 */
int					/* error */
xfs_alloc_lookup_le(
	struct xfs_btree_cur	*cur,	/* btree cursor */
	xfs_agblock_t		bno,	/* starting block of extent */
	xfs_extlen_t		len,	/* length of extent */
	int			*stat);	/* success/failure */
 
/*
 * Update the record referred to by cur, to the value given by [bno, len].
 * This either works (return 0) or gets an EFSCORRUPTED error.
 */
int					/* error */
xfs_alloc_update(
	struct xfs_btree_cur	*cur,	/* btree cursor */
	xfs_agblock_t		bno,	/* starting block of extent */
	xfs_extlen_t		len);	/* length of extent */

#endif	/* !_FS_XFS_ALLOC_BTREE_H */
