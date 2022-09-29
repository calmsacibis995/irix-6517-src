#ifndef	_XFS_INODE_H
#define	_XFS_INODE_H

#ident "$Revision: 1.125 $"

struct bhv_desc;
struct buf;
struct cred;
struct grio_ticket;
struct ktrace;
struct vnode;
struct xfs_bmbt_block;
struct xfs_ext_attr;
struct xfs_gap;
struct xfs_inode;
struct xfs_inode_log_item;
struct xfs_mount;
struct xfs_trans;
struct zone;
struct xfs_dquot;

/*
 * This is the type used in the xfs inode hash table.
 * An array of these is allocated for each mounted
 * file system to hash the inodes for that file system.
 */
typedef struct xfs_ihash {
	struct xfs_inode	*ih_next;	
	mrlock_t		ih_lock;
	uint			ih_version;
} xfs_ihash_t;
#if defined(MP)
#pragma set type attribute xfs_ihash align=128
#endif

/*
 * This is the xfs inode cluster hash.  This hash is used by xfs_iflush to
 * find inodes that share a cluster and can be flushed to disk at the same
 * time.
 */

typedef struct xfs_chashlist {
	struct xfs_chashlist	*chl_next;
	struct xfs_inode	*chl_ip;
	daddr_t			chl_blkno;	/* starting block number of 
						 * the cluster */
#ifdef DEBUG
	struct buf		*chl_buf;	/* debug: the inode buffer */
#endif
} xfs_chashlist_t;

typedef struct xfs_chash {
	xfs_chashlist_t		*ch_list;
	lock_t			ch_lock;
} xfs_chash_t;


#ifdef NOTYET
/*
 * The range structure is used to describe a locked range
 * within a file.  It is used in conjunction with the
 * xfs_range_lock_t defined below.
 */
typedef struct xfs_range {
	struct xfs_range	*r_forw;	/* range list pointer */
	struct xfs_range	*r_back;	/* range list pointer */
	xfs_fileoff_t		r_off;		/* start of range */
	xfs_fileoff_t		r_len;		/* length of range */
	int			r_sleepers;	/* count of sleepers */
} xfs_range_t;

/*
 * This is a structure embedded in the incore inode for
 * tracking range locks over the file.  The semaphore is
 * dynamically allocated to reduce our memory consumption.
 * XXX Semaphores are now only 32-bits -- should allocate in-line. --yohn
 */
typedef struct xfs_range_lock {
	mutex_t		r_spinlock;	/* lock to make sleeps atomic */
	sema_t		*r_sleep;	/* semaphore for sleeping on */
	xfs_range_t	*r_range_list;	/* list of locked ranges */
} xfs_range_lock_t;
#endif /* NOTYET */

/*
 * File incore extent information, present for each of data & attr forks.
 */
#define	XFS_INLINE_EXTS	2
#define	XFS_INLINE_DATA	32
typedef struct xfs_ifork {
	int			if_bytes; 	/* bytes in if_u1 */
	int			if_real_bytes;	/* bytes allocated in if_u1 */
	xfs_bmbt_block_t	*if_broot;	/* file's incore btree root */
	short			if_broot_bytes;	/* bytes allocated for root */
	unsigned char		if_flags;	/* per-fork flags */
	unsigned char		if_ext_max;	/* max # of extent records */
	xfs_extnum_t		if_lastex;	/* last if_extents used */
	union {
		xfs_bmbt_rec_t	*if_extents;	/* linear map file exts */
		char		*if_data;	/* inline file data */
	} if_u1;
	union {
		xfs_bmbt_rec_t	if_inline_ext[XFS_INLINE_EXTS];
						/* very small file extents */
		char		if_inline_data[XFS_INLINE_DATA];
						/* very small file data */
		dev_t		if_rdev;	/* dev number if special */
		uuid_t		if_uuid;	/* mount point value */
	} if_u2;
} xfs_ifork_t;

/*
 * This is the xfs in-core inode structure.
 * Most of the on-disk inode is embedded in the i_d field.
 *
 * The extent pointers/inline file space, however, are managed
 * separately.  The memory for this information is pointed to by
 * the if_u1 unions depending on the type of the data.
 * This is used to linearize the array of extents for fast in-core
 * access.  This is used until the file's number of extents
 * surpasses XFS_MAX_INCORE_EXTENTS, at which point all extent pointers
 * are accessed through the buffer cache.
 *
 * Other state kept in the in-core inode is used for identification,
 * locking, transactional updating, etc of the inode.
 *
 * Generally, we do not want to hold the i_rlock while holding the 
 * i_ilock. Hierarchy is i_iolock followed by i_rlock. 
 *
 * xfs_iptr_t contains all the inode fields upto and including the
 * i_mnext and i_mprev fields, it is used as a marker in the inode
 * chain off the mount structure by xfs_sync calls.
 */

typedef struct {
	struct xfs_ihash	*ip_hash;	/* pointer to hash header */
	struct xfs_inode	*ip_next;	/* inode hash link forw */
	struct xfs_inode	*ip_mnext;	/* next inode in mount list */
	struct xfs_inode	*ip_mprev;	/* ptr to prev inode */
	struct xfs_inode	**ip_prevp;	/* ptr to prev i_next */
	struct xfs_mount	*ip_mount;	/* fs mount struct ptr */
} xfs_iptr_t;

typedef struct xfs_inode {
	/* Inode linking and identification information. */
	struct xfs_ihash	*i_hash;	/* pointer to hash header */
	struct xfs_inode	*i_next;	/* inode hash link forw */
	struct xfs_inode	*i_mnext;	/* next inode in mount list */
	struct xfs_inode	*i_mprev;	/* ptr to prev inode */
	struct xfs_inode	**i_prevp;	/* ptr to prev i_next */
	struct xfs_mount	*i_mount;	/* fs mount struct ptr */
	struct bhv_desc		i_bhv_desc;	/* inode behavior descriptor*/
	struct xfs_dquot	*i_udquot;	/* user dquot */
	struct xfs_dquot	*i_pdquot;	/* project dquot */

	/* Inode location stuff */
	xfs_ino_t		i_ino;		/* inode number (agno/agino)*/
	daddr_t			i_blkno;	/* blkno of inode buffer */
	dev_t			i_dev;		/* dev for this inode */
	ushort			i_len;		/* len of inode buffer */
	ushort			i_boffset;	/* off of inode in buffer */

	/* Extent information. */
	xfs_ifork_t		*i_afp;		/* attribute fork pointer */
	xfs_ifork_t		i_df;		/* data fork */

	/* Transaction and locking information. */
	struct xfs_trans	*i_transp;	/* ptr to owning transaction*/
	struct xfs_inode_log_item *i_itemp;	/* logging information */
	mrlock_t		i_lock;		/* inode lock */
	mrlock_t		i_iolock;	/* inode IO lock */
	sema_t			i_flock;	/* inode flush lock */
	unsigned int		i_pincount;	/* inode pin count */
	sv_t			i_pinsema;	/* inode pin sema */
	lock_t			i_ipinlock;	/* inode pinning mutex */
	mutex_t			i_rlock;	/* inode readahead mutex */
#ifdef NOTYET
	xfs_range_lock_t	i_range_lock;	/* range lock base */
#endif /* NOTYET */
	struct xfs_inode	**i_refcache;	/* ptr to entry in ref cache */
	struct xfs_inode	*i_release;	/* inode to unref */

	/* I/O state */
	off_t			i_io_offset;	/* last buf offset */
	off_t			i_next_offset;	/* seq read detector */
	unsigned int		i_last_req_sz;	/* last read size */
	unsigned int		i_io_size;	/* file io buffer len */
	xfs_fsize_t		i_new_size;	/* sz when write completes */
	off_t			i_write_offset;	/* start off of curr write */
	xfs_fileoff_t		i_reada_blkno;	/* next blk to start ra */
	struct xfs_gap		*i_gap_list;	/* hole list in write range */
	unsigned int		i_readio_blocks;	/* read buffer size */
	unsigned int		i_writeio_blocks;	/* write buffer size */
	uchar_t			i_readio_log;	/* log2 of read buffer size */
	uchar_t			i_writeio_log;	/* log2 of write buffer size */
	uchar_t			i_max_io_log;	/* max r/w io value */

	/* Miscellaneous state. */
	unsigned short		i_flags;	/* see defined flags below */
	unsigned short		i_update_core;	/* timestamps/size is dirty */
	unsigned short		i_update_size;	/* di_size field is dirty */
	int			i_queued_bufs;	/* count of xfsd queued bufs*/
	unsigned int		i_gen;		/* generation count */
	unsigned int		i_delayed_blks;	/* count of delay alloc blks */
	struct xfs_ext_attr	*i_ext_attr;	/* Critical ext attributes */
	void			*i_ilock_ra;	/* current ilock ret addr */

	xfs_dinode_core_t	i_d;		/* most of ondisk inode */
	xfs_chashlist_t		*i_chash;	/* cluster hash list header */
	struct xfs_inode	*i_cnext;	/* cluster hash link forward */
	struct xfs_inode	*i_cprev;	/* cluster hash link backward */

#ifdef DEBUG
	/* Trace buffers per inode. */
	struct ktrace		*i_xtrace;	/* inode extent list trace */
	struct ktrace		*i_btrace;	/* inode bmap btree trace */
	struct ktrace		*i_rwtrace;	/* inode read/write trace */
	struct ktrace		*i_strat_trace;	/* inode strat_write trace */
	struct ktrace		*i_lock_trace;	/* inode lock/unlock trace */
	struct ktrace		*i_dir_trace;	/* inode directory trace */
#endif /* DEBUG */
} xfs_inode_t;

/*
 * Fork handling.
 */
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_IFORK_PTR)
xfs_ifork_t *xfs_ifork_ptr(xfs_inode_t *ip, int w);
#define	XFS_IFORK_PTR(ip,w)   		xfs_ifork_ptr(ip,w)
#else
#define	XFS_IFORK_PTR(ip,w)   ((w) == XFS_DATA_FORK ? &(ip)->i_df : (ip)->i_afp)
#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_IFORK_Q)
int xfs_ifork_q(xfs_inode_t *ip);
#define	XFS_IFORK_Q(ip)			xfs_ifork_q(ip)
#else
#define	XFS_IFORK_Q(ip)			XFS_CFORK_Q(&(ip)->i_d)
#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_IFORK_DSIZE)
int xfs_ifork_dsize(xfs_inode_t *ip);
#define	XFS_IFORK_DSIZE(ip)		xfs_ifork_dsize(ip)
#else
#define	XFS_IFORK_DSIZE(ip)		XFS_CFORK_DSIZE(&ip->i_d, ip->i_mount)
#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_IFORK_ASIZE)
int xfs_ifork_asize(xfs_inode_t *ip);
#define	XFS_IFORK_ASIZE(ip)		xfs_ifork_asize(ip)
#else
#define	XFS_IFORK_ASIZE(ip)		XFS_CFORK_ASIZE(&ip->i_d, ip->i_mount)
#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_IFORK_SIZE)
int xfs_ifork_size(xfs_inode_t *ip, int w);
#define	XFS_IFORK_SIZE(ip,w)		xfs_ifork_size(ip,w)
#else
#define	XFS_IFORK_SIZE(ip,w)		XFS_CFORK_SIZE(&ip->i_d, ip->i_mount, w)
#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_IFORK_FORMAT)
int xfs_ifork_format(xfs_inode_t *ip, int w);
#define	XFS_IFORK_FORMAT(ip,w)		xfs_ifork_format(ip,w)
#else
#define	XFS_IFORK_FORMAT(ip,w)		XFS_CFORK_FORMAT(&ip->i_d, w)
#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_IFORK_FMT_SET)
void xfs_ifork_fmt_set(xfs_inode_t *ip, int w, int n);
#define	XFS_IFORK_FMT_SET(ip,w,n)	xfs_ifork_fmt_set(ip,w,n)
#else
#define	XFS_IFORK_FMT_SET(ip,w,n)	XFS_CFORK_FMT_SET(&ip->i_d, w, n)
#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_IFORK_NEXTENTS)
int xfs_ifork_nextents(xfs_inode_t *ip, int w);
#define	XFS_IFORK_NEXTENTS(ip,w)	xfs_ifork_nextents(ip,w)
#else
#define	XFS_IFORK_NEXTENTS(ip,w)	XFS_CFORK_NEXTENTS(&ip->i_d, w)
#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_IFORK_NEXT_SET)
void xfs_ifork_next_set(xfs_inode_t *ip, int w, int n);
#define	XFS_IFORK_NEXT_SET(ip,w,n)	xfs_ifork_next_set(ip,w,n)
#else
#define	XFS_IFORK_NEXT_SET(ip,w,n)	XFS_CFORK_NEXT_SET(&ip->i_d, w, n)
#endif

/*
 * In-core inode flags.
 */
#define XFS_IGRIO	0x0001  /* inode will be used for guaranteed rate i/o */
#define XFS_IUIOSZ	0x0002  /* inode i/o sizes have been explicitly set */

/*
 * Per-fork incore inode flags.
 */
#define	XFS_IFINLINE	0x0001	/* Inline data is read in */
#define	XFS_IFEXTENTS	0x0002	/* All extent pointers are read in */
#define	XFS_IFBROOT	0x0004	/* i_broot points to the bmap b-tree root */

/*
 * Flags for inode locking.
 */
#define	XFS_IOLOCK_EXCL		0x01
#define	XFS_IOLOCK_SHARED	0x02
#define	XFS_ILOCK_EXCL		0x04
#define	XFS_ILOCK_SHARED	0x08
#define	XFS_IUNLOCK_NONOTIFY	0x10
#define XFS_IOLOCK_NESTED	0x20

/*
 * Flags for xfs_iflush()
 */
#define	XFS_IFLUSH_DELWRI_ELSE_SYNC	1
#define	XFS_IFLUSH_DELWRI_ELSE_ASYNC	2
#define	XFS_IFLUSH_SYNC			3
#define	XFS_IFLUSH_ASYNC		4
#define	XFS_IFLUSH_DELWRI		5

/*
 * Flags for xfs_iflush_all.
 */
#define	XFS_FLUSH_ALL		0x1

/*
 * Flags for xfs_itruncate_start().
 */
#define	XFS_ITRUNC_DEFINITE	0x1
#define	XFS_ITRUNC_MAYBE	0x2

/*
 * Flags for xfs_ichgtime().
 */
#define	XFS_ICHGTIME_MOD	0x1	/* data fork modification timestamp */
#define	XFS_ICHGTIME_ACC	0x2	/* data fork access timestamp */
#define	XFS_ICHGTIME_CHG	0x4	/* inode field change timestamp */

/*
 * Flags for xfs_imap() and xfs_dilocate().
 */
#define	XFS_IMAP_LOOKUP		0x1

/*
 * Maximum number of extent pointers in if_u1.if_extents.
 */
#define	XFS_MAX_INCORE_EXTENTS	32768

/*
 * Maximum file size.
 * if XFS_BIG_FILES 2^63 - 1 (largest positive value of xfs_fsize_t)
 * else 2^40 - 1 (40=31+9) (might be an int holding a block #)
 * Note, we allow seeks to this offset, although you can't read or write.
 * For the not XFS_BIG_FILES case, the value could be 1 higher but we don't
 * do that, for symmetry.
 */
#if XFS_BIG_FILES
#define XFS_MAX_FILE_OFFSET	((long long)((1ULL<<63)-1ULL))
#else
#define	XFS_MAX_FILE_OFFSET	((1LL<<40)-1LL)
#endif

#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_ITOV)
struct vnode *xfs_itov(xfs_inode_t *ip);
#define	XFS_ITOV(ip)		xfs_itov(ip)
#else
#define	XFS_ITOV(ip)		BHV_TO_VNODE(XFS_ITOBHV(ip))
#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_ITOBHV)
struct bhv_desc *xfs_itobhv(xfs_inode_t *ip);
#define	XFS_ITOBHV(ip)		xfs_itobhv(ip)
#else
#define	XFS_ITOBHV(ip)		((struct bhv_desc *)(&((ip)->i_bhv_desc)))
#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_BHVTOI)
xfs_inode_t *xfs_bhvtoi(struct bhv_desc *bhvp);
#define	XFS_BHVTOI(bhvp)	xfs_bhvtoi(bhvp)
#else
#define	XFS_BHVTOI(bhvp)	\
	((xfs_inode_t *)((char *)(bhvp) - \
			 (char *)&(((xfs_inode_t *)0)->i_bhv_desc)))
#endif

#define BHV_IS_XFS(bdp)		(BHV_OPS(bdp) == &xfs_vnodeops)

/*
 * Clear out the read-ahead state in the in-core inode.
 * We actually only need to clear i_next_offset and
 * i_last_req_sz to get the effect of making all the
 * read ahead state unusable.
 */
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_INODE_CLEAR_READ_AHEAD)
void xfs_inode_clear_read_ahead(xfs_inode_t *ip);
#define	XFS_INODE_CLEAR_READ_AHEAD(ip)		xfs_inode_clear_read_ahead(ip)
#else
#define	XFS_INODE_CLEAR_READ_AHEAD(ip)	{	\
		mutex_lock(&((ip)->i_rlock), PINOD);	\
		ip->i_next_offset = 0;		\
		ip->i_last_req_sz = 0; 		\
		mutex_unlock(&((ip)->i_rlock)); }
#endif

/*
 * XFS file identifiers.  The xfs_fid_ino_t type is because NFS 2 won't
 * take more than 10 bytes of stuff past fid_len.  xfs_fid2_t is solely
 * to get a full 64 bit inode number.
 */
typedef __uint32_t	xfs_fid_ino_t;
typedef struct xfs_fid {
	u_short		fid_len;       /* length of remainder (ten!) */
        u_short		fid_pad;       /* middle 16 bits inode number */
	__uint32_t	fid_gen;       /* generation number */
        xfs_fid_ino_t	fid_ino;       /* lo 32 bits inode number */
} xfs_fid_t;

typedef struct xfs_fid2 {
	u_short		fid_len;	/* length of remainder */
	u_short		fid_pad;	/* padding, must be zero */
	__uint32_t	fid_gen;	/* generation number */
	xfs_ino_t	fid_ino;	/* inode number */
} xfs_fid2_t;

/*
 * Pick the inode cluster hash bucket
 * (m_chash is the same size as m_ihash)
 */
#define XFS_CHASH(mp,blk) ((mp)->m_chash + (blk % (mp)->m_chsize))


/*
 * xfs_iget.c prototypes.
 */
void		xfs_ihash_init(struct xfs_mount *);
void		xfs_ihash_free(struct xfs_mount *);
void		xfs_chash_init(struct xfs_mount *);
void		xfs_chash_free(struct xfs_mount *);
xfs_inode_t	*xfs_inode_incore(struct xfs_mount *, xfs_ino_t,
				  struct xfs_trans *);
int		xfs_iget(struct xfs_mount *, struct xfs_trans *, xfs_ino_t,
			 uint, xfs_inode_t **, daddr_t);
void		xfs_iput(xfs_inode_t *, uint);
void		xfs_ilock(xfs_inode_t *, uint);
int		xfs_ilock_nowait(xfs_inode_t *, uint);
void		xfs_iunlock(xfs_inode_t *, uint);
void		xfs_ilock_demote(xfs_inode_t *, uint);
void		xfs_iflock(xfs_inode_t *);
int		xfs_iflock_nowait(xfs_inode_t *);
uint		xfs_ilock_map_shared(xfs_inode_t *);
void		xfs_iunlock_map_shared(xfs_inode_t *, uint);
void		xfs_ifunlock(xfs_inode_t *);
void		xfs_ireclaim(xfs_inode_t *);

/*
 * xfs_inode.c prototypes.
 */
int		xfs_inotobp(struct xfs_mount *, struct xfs_trans *, xfs_ino_t,
			    xfs_dinode_t **, struct buf **);
int		xfs_itobp(struct xfs_mount *, struct xfs_trans *,
			  xfs_inode_t *, xfs_dinode_t **, struct buf **,
			  daddr_t);
int		xfs_iread(struct xfs_mount *, struct xfs_trans *, xfs_ino_t,
			  xfs_inode_t **, daddr_t);
int		xfs_iread_extents(struct xfs_trans *, xfs_inode_t *, int);
int		xfs_ialloc(struct xfs_trans *, xfs_inode_t *, mode_t, nlink_t,
		           dev_t, struct cred *, xfs_prid_t, int,
			   struct buf **, boolean_t *, xfs_inode_t **);
#ifndef SIM
int		xfs_ifree(struct xfs_trans *, xfs_inode_t *);
int		xfs_atruncate_start(xfs_inode_t *);
void		xfs_itruncate_start(xfs_inode_t *, uint, xfs_fsize_t);
int		xfs_itruncate_finish(struct xfs_trans **, xfs_inode_t *,
				     xfs_fsize_t, int, int);
int		xfs_iunlink(struct xfs_trans *, xfs_inode_t *);
#endif	/* !SIM */
int		xfs_igrow_start(xfs_inode_t *, xfs_fsize_t, struct cred *);
void		xfs_igrow_finish(struct xfs_trans *, xfs_inode_t *,
				 xfs_fsize_t, int);

void		xfs_idestroy_fork(xfs_inode_t *, int);
void		xfs_idestroy(xfs_inode_t *);
void		xfs_idata_realloc(xfs_inode_t *, int, int);
void		xfs_iext_realloc(xfs_inode_t *, int, int);
void		xfs_iroot_realloc(xfs_inode_t *, int, int);
void		xfs_ipin(xfs_inode_t *);
void		xfs_iunpin(xfs_inode_t *);
unsigned int	xfs_ipincount(xfs_inode_t *);
int		xfs_iextents_copy(xfs_inode_t *, xfs_bmbt_rec_32_t *, int);
int		xfs_iflush(xfs_inode_t *, uint);
int		xfs_iflush_all(struct xfs_mount *, int);
#ifdef SIM
void		xfs_iprint(xfs_inode_t *);
#endif
int		xfs_iaccess(xfs_inode_t *, mode_t, struct cred *);
uint		xfs_iroundup(uint);
void		xfs_ichgtime(xfs_inode_t *, int);
xfs_fsize_t	xfs_file_last_byte(xfs_inode_t *);

#ifdef DEBUG
void		xfs_isize_check(struct xfs_mount *, xfs_inode_t *, xfs_fsize_t);
#else	/* DEBUG */
#define xfs_isize_check(mp, ip, isize)
#endif	/* DEBUG */

#if defined(DEBUG) && !defined(XFS_REPAIR_SIM)
void		xfs_inobp_check(struct xfs_mount *, struct buf *);
#else
#define	xfs_inobp_check(mp, bp)
#endif /* DEBUG && !XFS_REPAIR_SIM */

/*
 * xfs_vnodeops.c prototypes.
 */
int		xfs_fast_fid(struct bhv_desc *, xfs_fid_t *);

extern struct zone	*xfs_ifork_zone;
extern struct zone	*xfs_inode_zone;
extern struct zone	*xfs_ili_zone;
extern struct vnodeops	xfs_vnodeops;

#ifdef XFS_ILOCK_TRACE
#define XFS_ILOCK_KTRACE_SIZE	32
void	xfs_ilock_trace(xfs_inode_t *ip, int lock, unsigned int lockflags, 
			inst_t *ra);
#endif

#endif	/* _XFS_INODE_H */
