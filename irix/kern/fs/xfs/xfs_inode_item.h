#ifndef	_XFS_INODE_ITEM_H
#define	_XFS_INODE_ITEM_H

#ident "$Revision: 1.31 $"

struct buf;
struct proc;
struct xfs_bmbt_rec_32;
struct xfs_inode;
struct xfs_mount;

/*
 * This is the structure used to lay out an inode log item in the
 * log.  The size of the inline data/extents/b-tree root to be logged
 * (if any) is indicated in the ilf_dsize field.  Changes to this structure
 * must be added on to the end.
 *
 * Convention for naming inode log item versions :  The current version
 * is always named XFS_LI_INODE.  When an inode log item gets superseded,
 * add the latest version of IRIX that will generate logs with that item
 * to the version name.
 *
 * -Version 1 of this structure (XFS_LI_5_3_INODE) included up to the first
 *	union (ilf_u) field.  This was released with IRIX 5.3-XFS.
 * -Version 2 of this structure (XFS_LI_6_1_INODE) is currently the entire
 *	structure.  This was released with IRIX 6.0.1-XFS and IRIX 6.1.
 * -Version 3 of this structure (XFS_LI_INODE) is the same as version 2
 *	so a new structure definition wasn't necessary.  However, we had
 *	to add a new type because the inode cluster size changed from 4K
 *	to 8K and the version number had to be rev'ved to keep older kernels
 *	from trying to recover logs with the 8K buffers in them.  The logging
 *	code can handle recovery on different-sized clusters now so hopefully
 *	this'll be the last time we need to change the inode log item just
 *	for a change in the inode cluster size.  This new version was
 *	released with IRIX 6.2.
 */
typedef struct xfs_inode_log_format {
	unsigned short		ilf_type;	/* inode log item type */
	unsigned short		ilf_size;	/* size of this item */
	uint			ilf_fields;	/* flags for fields logged */
	ushort			ilf_asize;	/* size of attr d/ext/root */
	ushort			ilf_dsize;	/* size of data/ext/root */
	xfs_ino_t		ilf_ino;	/* inode number */
	union {
		dev_t		ilfu_rdev;	/* rdev value for dev inode*/
		uuid_t		ilfu_uuid;	/* mount point value */
	} ilf_u;
	__int64_t		ilf_blkno;	/* blkno of inode buffer */
	int			ilf_len;	/* len of inode buffer */
	int			ilf_boffset;	/* off of inode in buffer */
} xfs_inode_log_format_t;

/* Initial version shipped with IRIX 5.3-XFS */
typedef struct xfs_inode_log_format_v1 {
	unsigned short		ilf_type;	/* inode log item type */
	unsigned short		ilf_size;	/* size of this item */
	uint			ilf_fields;	/* flags for fields logged */
	uint			ilf_dsize;	/* size of data/ext/root */
	xfs_ino_t		ilf_ino;	/* inode number */
	union {
		dev_t		ilfu_rdev;	/* rdev value for dev inode*/
		uuid_t		ilfu_uuid;	/* mount point value */
	} ilf_u;
} xfs_inode_log_format_t_v1;

typedef struct xfs_inode_log_item {
	xfs_log_item_t		ili_item;	   /* common portion */
	struct xfs_inode	*ili_inode;	   /* inode ptr */
	xfs_lsn_t		ili_flush_lsn;	   /* lsn at last flush */
	unsigned short		ili_ilock_recur;   /* lock recursion count */
	unsigned short		ili_iolock_recur;  /* lock recursion count */
	unsigned short		ili_flags;	   /* misc flags */
	unsigned short		ili_logged;	   /* flushed logged data */
	unsigned int		ili_last_fields;   /* fields when flushed */
	struct xfs_bmbt_rec_32	*ili_extents_buf;  /* array of logged exts */
	unsigned int            ili_pushbuf_flag;  /* one bit used in push_ail */

#ifdef DEBUG
	uint64_t                ili_push_owner;    /* one who sets pushbuf_flag
						      above gets to push the buf */
#endif
#ifdef XFS_TRANS_DEBUG
	int			ili_root_size;
	char			*ili_orig_root;
#endif
	xfs_inode_log_format_t	ili_format;	   /* logged structure */
} xfs_inode_log_item_t;

#define	XFS_ILI_HOLD		0x1
#define	XFS_ILI_IOLOCKED_EXCL	0x2
#define	XFS_ILI_IOLOCKED_SHARED	0x4

#define	XFS_ILI_IOLOCKED_ANY   (XFS_ILI_IOLOCKED_EXCL | XFS_ILI_IOLOCKED_SHARED)

/*
 * Flags for xfs_trans_log_inode flags field.
 */
#define	XFS_ILOG_CORE	0x001	/* log standard inode fields */
#define	XFS_ILOG_DDATA	0x002	/* log i_df.if_data */
#define	XFS_ILOG_DEXT	0x004	/* log i_df.if_extents */
#define	XFS_ILOG_DBROOT	0x008	/* log i_df.i_broot */
#define	XFS_ILOG_DEV	0x010	/* log the dev field */
#define	XFS_ILOG_UUID	0x020	/* log the uuid field */
#define	XFS_ILOG_ADATA	0x040	/* log i_af.if_data */
#define	XFS_ILOG_AEXT	0x080	/* log i_af.if_extents */
#define	XFS_ILOG_ABROOT	0x100	/* log i_af.i_broot */

#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_ILOG_FDATA)
int xfs_ilog_fdata(int w);
#define	XFS_ILOG_FDATA(w)	xfs_ilog_fdata(w)
#else
#define	XFS_ILOG_FDATA(w)	\
	((w) == XFS_DATA_FORK ? XFS_ILOG_DDATA : XFS_ILOG_ADATA)
#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_ILOG_FBROOT)
int xfs_ilog_fbroot(int w);
#define	XFS_ILOG_FBROOT(w)	xfs_ilog_fbroot(w)
#else
#define	XFS_ILOG_FBROOT(w)	\
	((w) == XFS_DATA_FORK ? XFS_ILOG_DBROOT : XFS_ILOG_ABROOT)
#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_ILOG_FEXT)
int xfs_ilog_fext(int w);
#define	XFS_ILOG_FEXT(w)	xfs_ilog_fext(w)
#else
#define	XFS_ILOG_FEXT(w)	\
	((w) == XFS_DATA_FORK ? XFS_ILOG_DEXT : XFS_ILOG_AEXT)
#endif

#define	XFS_ILOG_NONCORE	(XFS_ILOG_DDATA | XFS_ILOG_DEXT | \
				 XFS_ILOG_DBROOT | XFS_ILOG_DEV | \
				 XFS_ILOG_UUID | XFS_ILOG_ADATA | \
				 XFS_ILOG_AEXT | XFS_ILOG_ABROOT)

#define	XFS_ILOG_DFORK		(XFS_ILOG_DDATA | XFS_ILOG_DEXT | \
				 XFS_ILOG_DBROOT)

#define	XFS_ILOG_AFORK		(XFS_ILOG_ADATA | XFS_ILOG_AEXT | \
				 XFS_ILOG_ABROOT)

#define	XFS_ILOG_ALL		(XFS_ILOG_CORE | XFS_ILOG_DDATA | \
				 XFS_ILOG_DEXT | XFS_ILOG_DBROOT | \
				 XFS_ILOG_DEV | XFS_ILOG_UUID | \
				 XFS_ILOG_ADATA | XFS_ILOG_AEXT | \
				 XFS_ILOG_ABROOT)

     
void	xfs_inode_item_init(struct xfs_inode *, struct xfs_mount *);
void	xfs_inode_item_destroy(struct xfs_inode *);
void	xfs_iflush_done(struct buf *, xfs_inode_log_item_t *);
void	xfs_iflush_abort(struct xfs_inode *);

#endif	/* _XFS_INODE_ITEM_H */
