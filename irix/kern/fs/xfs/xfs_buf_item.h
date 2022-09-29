#ifndef	_XFS_BUF_ITEM_H
#define	_XFS_BUF_ITEM_H

#ident "$Revision: 1.24 $"

struct buf;
struct ktrace;
struct xfs_mount;

/*
 * This is the structure used to lay out a buf log item in the
 * log.  The data map describes which 128 byte chunks of the buffer
 * have been logged.  This structure works only on buffers that
 * reside up to the first TB in the filesystem.  These buffers are
 * generated only by pre-6.2 systems and are known as XFS_LI_6_1_BUF.
 */
typedef struct xfs_buf_log_format_v1 {
	unsigned short	blf_type;	/* buf log item type indicator */
	unsigned short	blf_size;	/* size of this item */
	__int32_t	blf_blkno;	/* starting blkno of this buf */
	ushort		blf_flags;	/* misc state */
	ushort		blf_len;	/* number of blocks in this buf */
	unsigned int	blf_map_size;	/* size of data bitmap in words */
	unsigned int	blf_data_map[1];/* variable size bitmap of */
					/*   regions of buffer in this item */
} xfs_buf_log_format_v1_t;

/*
 * This is a form of the above structure with a 64 bit blkno field.
 * For 6.2 and beyond, this is XFS_LI_BUF.  We use this to log everything.
 */
typedef struct xfs_buf_log_format_t {
	unsigned short	blf_type;	/* buf log item type indicator */
	unsigned short	blf_size;	/* size of this item */
	ushort		blf_flags;	/* misc state */
	ushort		blf_len;	/* number of blocks in this buf */
	__int64_t	blf_blkno;	/* starting blkno of this buf */
	unsigned int	blf_map_size;	/* size of data bitmap in words */
	unsigned int	blf_data_map[1];/* variable size bitmap of */
					/*   regions of buffer in this item */
} xfs_buf_log_format_t;

/*
 * This flag indicates that the buffer contains on disk inodes
 * and requires special recovery handling.
 */
#define	XFS_BLI_INODE_BUF	0x1
/*
 * This flag indicates that the buffer should not be replayed
 * during recovery because its blocks are being freed.
 */
#define	XFS_BLI_CANCEL		0x2
/*
 * This flag indicates that the buffer contains on disk 
 * user or projectdquots and may require special recovery handling.   
 */
#define	XFS_BLI_UDQUOT_BUF	0x4
#define	XFS_BLI_PDQUOT_BUF	0x8

#define	XFS_BLI_CHUNK		128
#define	XFS_BLI_SHIFT		7
#define	BIT_TO_WORD_SHIFT	5
#define	NBWORD			(NBBY * sizeof(unsigned int))

/*
 * This is the in core log item structure used to track information
 * needed to log buffers.  It tracks how many times the lock has been
 * locked, and which 128 byte chunks of the buffer are dirty.
 */
typedef struct xfs_buf_log_item {
	xfs_log_item_t		bli_item;	/* common item structure */
	struct buf		*bli_buf;	/* real buffer pointer */
	unsigned int		bli_flags;	/* misc flags */
	unsigned int		bli_recur;	/* lock recursion count */
	int			bli_refcount;	/* cnt of tp refs */
#ifdef DEBUG
	struct ktrace		*bli_trace;	/* event trace buf */
#endif
#ifdef XFS_TRANS_DEBUG
	char			*bli_orig;	/* original buffer copy */
	char			*bli_logged;	/* bytes logged (bitmap) */
#endif
	xfs_buf_log_format_t	bli_format;	/* in-log header */
} xfs_buf_log_item_t;

/*
 * buf log item flags
 */
#define	XFS_BLI_HOLD		0x01
#define	XFS_BLI_DIRTY		0x02
#define	XFS_BLI_STALE		0x04
#define	XFS_BLI_LOGGED		0x08
#define	XFS_BLI_INODE_ALLOC_BUF	0x10

/*
 * This structure is used during recovery to record the buf log
 * items which have been canceled and should not be replayed.
 */
typedef struct xfs_buf_cancel {
	daddr_t			bc_blkno;
	uint			bc_len;
	int			bc_refcount;
	struct xfs_buf_cancel	*bc_next;
} xfs_buf_cancel_t;

#define	XFS_BLI_TRACE_SIZE	32


#if defined(XFS_ALL_TRACE)
#define	XFS_BLI_TRACE
#endif

#if !defined(DEBUG) || defined(SIM)
#undef XFS_BLI_TRACE
#endif

#if defined(XFS_BLI_TRACE)
void	xfs_buf_item_trace(char *, xfs_buf_log_item_t *);
#else
#define	xfs_buf_item_trace(id, bip)
#endif

void	xfs_buf_item_init(struct buf *, struct xfs_mount *);
void	xfs_buf_item_relse(struct buf *);
void	xfs_buf_item_log(xfs_buf_log_item_t *, uint, uint);
uint	xfs_buf_item_dirty(xfs_buf_log_item_t *);
int	xfs_buf_item_bits(uint *, uint, uint);
int	xfs_buf_item_contig_bits(uint *, uint, uint);
int	xfs_buf_item_next_bit(uint *, uint, uint);
void	xfs_buf_attach_iodone(struct buf *,
			      void(*)(struct buf *, xfs_log_item_t *),
			      xfs_log_item_t *);
void	xfs_buf_iodone_callbacks(struct buf *);
void	xfs_buf_iodone(struct buf *, xfs_buf_log_item_t *);

#ifdef XFS_TRANS_DEBUG
void
xfs_buf_item_flush_log_debug(
	struct buf *bp,			     
	uint	first,
	uint	last);
#else
#define	xfs_buf_item_flush_log_debug(bp, first, last)
#endif

#endif	/* _XFS_BUF_ITEM_H */
