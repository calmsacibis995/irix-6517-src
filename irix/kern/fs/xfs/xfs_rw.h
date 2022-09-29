#ifndef	_XFS_RW_H
#define	_XFS_RW_H

#ident "$Revision: 1.43 $"

struct bhv_desc;
struct bdevsw;
struct bmapval;
struct buf;
struct cred;
struct flid;
struct uio;
struct vnode;
struct xfs_inode;
struct xfs_mount;
struct xfs_trans;
struct pm;

/*
 * This structure is used to communicate which extents of a file
 * were holes when a write started from xfs_write_file() to
 * xfs_strat_read().  This is necessary so that we can know which
 * blocks need to be zeroed when they are read in in xfs_strat_read()
 * if they weren\'t allocated when the buffer given to xfs_strat_read()
 * was mapped.
 *
 * We keep a list of these attached to the inode.  The list is
 * protected by the inode lock and the fact that the io lock is
 * held exclusively by writers.
 */
typedef struct xfs_gap {
	struct xfs_gap	*xg_next;
	xfs_fileoff_t	xg_offset_fsb;
	xfs_extlen_t	xg_count_fsb;
} xfs_gap_t;

/*
 * This structure is used to hold common pieces of the buffer
 * and file for xfs_dio_write and xfs_dio_read.
 */
typedef	struct xfs_dio {
	buf_t		*xd_bp;
	struct dio_s	*xd_dp;
	struct vnode	*xd_vp;
	struct xfs_inode *xd_ip;
	xfs_mount_t	*xd_mp;
	int		xd_blkalgn;
} xfs_dio_t;

/*
 * used for mmap i/o page lockdown code
 */
typedef struct xfs_uaccmap {
	uvaddr_t		xfs_uacstart;
	__psunsigned_t		xfs_uaclen;
} xfs_uaccmap_t;

/*
 * Maximum count of bmaps used by read and write paths.
 */
#define	XFS_MAX_RW_NBMAPS	4

/*
 * Counts of readahead buffers to use based on physical memory size.
 * None of these should be more than XFS_MAX_RW_NBMAPS.
 */
#define	XFS_RW_NREADAHEAD_16MB	2
#define	XFS_RW_NREADAHEAD_32MB	3
#define	XFS_RW_NREADAHEAD_K32	4
#define	XFS_RW_NREADAHEAD_K64	4

/*
 * Maximum size of a buffer that we\'ll map.  Making this
 * too big will degrade performance due to the number of
 * pages which need to be gathered.  Making it too small
 * will prevent us from doing large I/O\'s to hardware that
 * needs it.
 *
 * This is currently set to 512 KB.
 */
#define	XFS_MAX_BMAP_LEN_BB	1024
#define	XFS_MAX_BMAP_LEN_BYTES	524288

/*
 * Convert the given file system block to a disk block.
 * We have to treat it differently based on whether the
 * file is a real time file or not, because the bmap code
 * does.
 */
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_FSB_TO_DB)
daddr_t xfs_fsb_to_db(struct xfs_inode *ip, xfs_fsblock_t fsb);
#define	XFS_FSB_TO_DB(ip,fsb)	xfs_fsb_to_db(ip,fsb)
#else
#define	XFS_FSB_TO_DB(ip,fsb) \
		(((ip)->i_d.di_flags & XFS_DIFLAG_REALTIME) ? \
		 (daddr_t)XFS_FSB_TO_BB((ip)->i_mount, (fsb)) : \
		 XFS_FSB_TO_DADDR((ip)->i_mount, (fsb)))
#endif

#define	xfs_bdwrite(mp, bp) \
          { ((bp)->b_vp == NULL) ? (bp)->b_bdstrat = xfs_bdstrat_cb: 0; \
		    (bp)->b_fsprivate3 = (mp); bdwrite(bp);}
#define	xfs_bawrite(mp, bp) \
	  { ((bp)->b_vp == NULL) ? (bp)->b_bdstrat = xfs_bdstrat_cb: 0; \
		    (bp)->b_fsprivate3 = (mp); bawrite(bp);}
/*
 * Defines for the trace mechanisms in xfs_rw.c.
 */
#define	XFS_RW_KTRACE_SIZE	64
#define	XFS_STRAT_KTRACE_SIZE	64
#define	XFS_STRAT_GTRACE_SIZE	512

#define	XFS_READ_ENTER		1
#define	XFS_WRITE_ENTER		2
#define XFS_IOMAP_READ_ENTER	3
#define	XFS_IOMAP_WRITE_ENTER	4
#define	XFS_IOMAP_READ_MAP	5
#define	XFS_IOMAP_WRITE_MAP	6
#define	XFS_IOMAP_WRITE_NOSPACE	7
#define	XFS_ITRUNC_START	8
#define	XFS_ITRUNC_FINISH1	9
#define	XFS_ITRUNC_FINISH2	10
#define	XFS_CTRUNC1		11
#define	XFS_CTRUNC2		12
#define	XFS_CTRUNC3		13
#define	XFS_CTRUNC4		14
#define	XFS_CTRUNC5		15
#define	XFS_CTRUNC6		16     
#define	XFS_BUNMAPI		17
#define	XFS_INVAL_CACHED	18
#define	XFS_DIORD_ENTER		19
#define	XFS_DIOWR_ENTER		20

#define	XFS_STRAT_ENTER		1
#define	XFS_STRAT_FAST		2
#define	XFS_STRAT_SUB		3
#define	XFS_STRAT_UNINT		4
#define	XFS_STRAT_UNINT_DONE	5
#define	XFS_STRAT_UNINT_CMPL	6

#if defined(XFS_ALL_TRACE)
#define	XFS_RW_TRACE
#define	XFS_STRAT_TRACE
#endif

#if !defined(DEBUG) || defined(SIM)
#undef XFS_RW_TRACE
#undef XFS_STRAT_TRACE
#endif

/*
 * Prototypes for functions in xfs_rw.c.
 */
int
xfs_read(struct bhv_desc	*bdp,
	 struct uio		*uiop,
	 int			ioflag,
	 struct cred		*credp,
	 struct flid		*fl);

int
xfs_vop_readbuf(bhv_desc_t 	*bdp,
		off_t		offset,
		ssize_t		len,
		int		ioflags,
		struct cred	*creds,
		struct flid	*fl,
		struct buf	**rbuf,
		int		*pboff,
		int		*pbsize);

int
xfs_write_clear_setuid(
	struct xfs_inode	*ip);

int
xfs_write(struct bhv_desc	*bdp,
	  struct uio		*uiop,
	  int			ioflag,
	  struct cred		*credp,
	  struct flid		*fl);

int 
xfs_bwrite(
	struct xfs_mount 	*mp,
	struct buf		*bp);
int
xfsbdstrat(
	struct xfs_mount 	*mp,
	struct buf		*bp);

void
xfs_strategy(struct bhv_desc	*bdp,
	     struct buf		*bp);

int
xfs_bmap(struct bhv_desc	*bdp,
	 off_t			offset,
	 ssize_t		count,
	 int			flags,
	 struct cred		*credp,
	 struct bmapval		*bmapp,
	 int			*nbmaps);

int
xfs_zero_eof(struct xfs_inode	*ip,
	     off_t		offset,
	     xfs_fsize_t	isize,
	     struct cred	*credp,
	     struct pm		*pmp);

void
xfs_inval_cached_pages(
	struct xfs_inode	*ip,
	off_t			offset,
	off_t			len,
	void			*dio);

void
xfs_refcache_insert(
	struct xfs_inode	*ip);

void
xfs_refcache_purge_ip(
	struct xfs_inode	*ip);

void
xfs_refcache_purge_mp(
	struct xfs_mount	*mp);

void
xfs_refcache_purge_some(void);

int
xfs_bioerror(struct buf *b);

void
xfs_xfsd_list_evict(bhv_desc_t *bdp);

/*
 * Needed by xfs_rw.c
 */
void
xfs_rwlock(
	bhv_desc_t	*bdp,
	vrwlock_t	write_lock);

void
xfs_rwlockf(
	bhv_desc_t	*bdp,
	vrwlock_t	write_lock,
	int		flags);

void
xfs_rwunlock(
	bhv_desc_t	*bdp,
	vrwlock_t	write_lock);

void
xfs_rwunlockf(
	bhv_desc_t	*bdp,
	vrwlock_t	write_lock,
	int		flags);

int
xfs_read_buf(
	struct xfs_mount *mp,
	buftarg_t	 *target,
        daddr_t 	 blkno,
        int              len,
        uint             flags,
	struct buf	 **bpp);

int
xfs_bdstrat_cb(struct buf *bp);

void
xfs_ioerror_alert(
	char 			*func,
	struct xfs_mount	*mp,
	dev_t			dev,
	daddr_t			blkno);
	  
#endif /* _XFS_RW_H */
