#ifndef _FS_XFS_DIR2_DATA_H
#define	_FS_XFS_DIR2_DATA_H

#ident "$Revision: 1.1 $"

/*
 * xfs_dir2_data.h
 * Directory format 2, data block structures.
 */

struct xfs_dabuf;
struct xfs_da_args;
struct xfs_inode;
struct xfs_trans;

/*
 * Constants.
 */
#define	XFS_DIR2_DATA_MAGIC	0x58443244	/* XD2D: for multiblock dirs */
#define	XFS_DIR2_DATA_ALIGN_LOG	3		/* i.e., 8 bytes */
#define	XFS_DIR2_DATA_ALIGN	(1 << XFS_DIR2_DATA_ALIGN_LOG)
#define	XFS_DIR2_DATA_FREE_TAG	0xffff
#define	XFS_DIR2_DATA_FD_COUNT	3

/*
 * Directory address space divided into sections,
 * spaces separated by 32gb.
 */
#define	XFS_DIR2_SPACE_SIZE	(1ULL << (32 + XFS_DIR2_DATA_ALIGN_LOG))
#define	XFS_DIR2_DATA_SPACE	0
#define	XFS_DIR2_DATA_OFFSET	(XFS_DIR2_DATA_SPACE * XFS_DIR2_SPACE_SIZE)
#define	XFS_DIR2_DATA_FIRSTDB(mp)	\
	XFS_DIR2_BYTE_TO_DB(mp, XFS_DIR2_DATA_OFFSET)

/*
 * Offsets of . and .. in data space (always block 0)
 */
#define	XFS_DIR2_DATA_DOT_OFFSET	\
	((xfs_dir2_data_aoff_t)sizeof(xfs_dir2_data_hdr_t))
#define	XFS_DIR2_DATA_DOTDOT_OFFSET	\
	(XFS_DIR2_DATA_DOT_OFFSET + XFS_DIR2_DATA_ENTSIZE(1))
#define	XFS_DIR2_DATA_FIRST_OFFSET		\
	(XFS_DIR2_DATA_DOTDOT_OFFSET + XFS_DIR2_DATA_ENTSIZE(2))

/*
 * Structures.
 */

/*
 * Describe a free area in the data block.
 * The freespace will be formatted as a xfs_dir2_data_unused_t.
 */
typedef struct xfs_dir2_data_free {
	xfs_dir2_data_off_t	offset;		/* start of freespace */
	xfs_dir2_data_off_t	length;		/* length of freespace */
} xfs_dir2_data_free_t;

/*
 * Header for the data blocks.
 * Always at the beginning of a directory-sized block.
 * The code knows that XFS_DIR2_DATA_FD_COUNT is 3.
 */
typedef struct xfs_dir2_data_hdr {
	__uint32_t		magic;		/* XFS_DIR2_DATA_MAGIC */
						/* or XFS_DIR2_BLOCK_MAGIC */
	xfs_dir2_data_free_t	bestfree[XFS_DIR2_DATA_FD_COUNT];
} xfs_dir2_data_hdr_t;

/*
 * Active entry in a data block.  Aligned to 8 bytes.
 * Tag appears as the last 2 bytes.
 */
typedef struct xfs_dir2_data_entry {
	xfs_ino_t		inumber;	/* inode number */
	__uint8_t		namelen;	/* name length */
	__uint8_t		name[1];	/* name bytes, no null */
						/* variable offset */
	xfs_dir2_data_off_t	tag;		/* starting offset of us */
} xfs_dir2_data_entry_t;

/*
 * Unused entry in a data block.  Aligned to 8 bytes.
 * Tag appears as the last 2 bytes.
 */
typedef struct xfs_dir2_data_unused {
	__uint16_t		freetag;	/* XFS_DIR2_DATA_FREE_TAG */
	xfs_dir2_data_off_t	length;		/* total free length */
						/* variable offset */
	xfs_dir2_data_off_t	tag;		/* starting offset of us */
} xfs_dir2_data_unused_t;

typedef union {
	xfs_dir2_data_entry_t	entry;
	xfs_dir2_data_unused_t	unused;
} xfs_dir2_data_union_t;

/*
 * Generic data block structure, for xfs_db.
 */
typedef struct xfs_dir2_data {
	xfs_dir2_data_hdr_t	hdr;		/* magic XFS_DIR2_DATA_MAGIC */
	xfs_dir2_data_union_t	u[1];
} xfs_dir2_data_t;

/*
 * Macros.
 */

/*
 * Size of a data entry.
 */
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_DIR2_DATA_ENTSIZE)
int xfs_dir2_data_entsize(int n);
#define XFS_DIR2_DATA_ENTSIZE(n)	xfs_dir2_data_entsize(n)
#else
#define	XFS_DIR2_DATA_ENTSIZE(n)	\
	((int)(roundup(offsetof(xfs_dir2_data_entry_t, name[0]) + (n) + \
		 (uint)sizeof(xfs_dir2_data_off_t), XFS_DIR2_DATA_ALIGN)))
#endif

/*
 * Pointer to an entry's tag word.
 */
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_DIR2_DATA_ENTRY_TAG_P)
xfs_dir2_data_off_t *xfs_dir2_data_entry_tag_p(xfs_dir2_data_entry_t *dep);
#define	XFS_DIR2_DATA_ENTRY_TAG_P(dep)	xfs_dir2_data_entry_tag_p(dep)
#else
#define	XFS_DIR2_DATA_ENTRY_TAG_P(dep)	\
	((xfs_dir2_data_off_t *)\
	 ((char *)(dep) + XFS_DIR2_DATA_ENTSIZE((dep)->namelen) - \
	  (uint)sizeof(xfs_dir2_data_off_t)))
#endif

/*
 * Pointer to a freespace's tag word.
 */
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_DIR2_DATA_UNUSED_TAG_P)
xfs_dir2_data_off_t *xfs_dir2_data_unused_tag_p(xfs_dir2_data_unused_t *dup);
#define	XFS_DIR2_DATA_UNUSED_TAG_P(dup)	xfs_dir2_data_unused_tag_p(dup)
#else
#define	XFS_DIR2_DATA_UNUSED_TAG_P(dup)	\
	((xfs_dir2_data_off_t *)\
	 ((char *)(dup) + (dup)->length - (uint)sizeof(xfs_dir2_data_off_t)))
#endif

/*
 * Function declarations.
 */

#ifdef DEBUG
extern void
	xfs_dir2_data_check(struct xfs_inode *dp, struct xfs_dabuf *bp);
#else
#define	xfs_dir2_data_check(dp,bp)
#endif

extern xfs_dir2_data_free_t *
	xfs_dir2_data_freefind(xfs_dir2_data_t *d,
			       xfs_dir2_data_unused_t *dup);

extern xfs_dir2_data_free_t *
	xfs_dir2_data_freeinsert(xfs_dir2_data_t *d,
				 xfs_dir2_data_unused_t *dup, int *loghead);

extern void
	xfs_dir2_data_freeremove(xfs_dir2_data_t *d,
				 xfs_dir2_data_free_t *dfp, int *loghead);

extern void
	xfs_dir2_data_freescan(struct xfs_mount *mp, xfs_dir2_data_t *d,
			       int *loghead, char *aendp);

extern int
	xfs_dir2_data_init(struct xfs_da_args *args, xfs_dir2_db_t blkno,
			   struct xfs_dabuf **bpp);

extern void
	xfs_dir2_data_log_entry(struct xfs_trans *tp, struct xfs_dabuf *bp,
				xfs_dir2_data_entry_t *dep);

extern void
	xfs_dir2_data_log_header(struct xfs_trans *tp, struct xfs_dabuf *bp);

extern void
	xfs_dir2_data_log_unused(struct xfs_trans *tp, struct xfs_dabuf *bp,
				 xfs_dir2_data_unused_t *dup);

extern void
	xfs_dir2_data_make_free(struct xfs_trans *tp, struct xfs_dabuf *bp,
				xfs_dir2_data_aoff_t offset,
				xfs_dir2_data_aoff_t len, int *needlogp,
				int *needscanp);

extern void
	xfs_dir2_data_use_free(struct xfs_trans *tp, struct xfs_dabuf *bp,
			       xfs_dir2_data_unused_t *dup,
			       xfs_dir2_data_aoff_t offset,
			       xfs_dir2_data_aoff_t len, int *needlogp,
			       int *needscanp);

#endif	/* !_FS_XFS_DIR2_DATA_H */
